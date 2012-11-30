#include "fdmheat.h"

FDMHeat::FDMHeat(cl_context context, cl_command_queue queue, cl_device_id device) :
    QThread()
{
    clContext= context;
    clQueue= queue;
    clDevice= device;

    firstRun= true;
    suspended= false;
}

bool FDMHeat::loadFromImage(QString path)
{
    // En la primera llamada a loadFromImage cargamos el kernel
    if(firstRun) {
        if(!loadKernel(clContext, &kernel, clDevice, "../src/fdmHeat.cl", "fdmHeat"))
            return false;
        if(!loadKernel(clContext, &brushKernel, clDevice, "../src/heatBrush.cl", "heatBrush"))
            return false;
    }

    QImage image(path);
    if(image.isNull()) {
        qDebug() << "FDMHeat::loadFromImage: Could not load" << path;
        return false;
    }
    width= image.width();
    height= image.height();
    bytes= width * height * sizeof(float);

    // Si no es la primera llamada liberamos los buffers anteriores
    if(!firstRun) {
        free(hData);
        clReleaseMemObject(dData1);
        clReleaseMemObject(dData2);
    }

    // Reservamos buffers
    hData= (float*)malloc(bytes);

    cl_image_format format;
    format.image_channel_data_type= CL_FLOAT;
    format.image_channel_order= CL_INTENSITY;
    cl_int error1, error2;
    dData1= clCreateImage2D(clContext, CL_MEM_READ_WRITE, &format, width, height, 0, NULL, &error1);
    dData2= clCreateImage2D(clContext, CL_MEM_READ_WRITE, &format, width, height, 0, NULL, &error2);

    if(!hData or checkError(error1, "clCreateImage2D") or checkError(error2, "clCreateImage2D")) {
        qDebug() << "FDMHeat::loadFromImage: Error al reservar memoria.";
        return false;
    }

    // Convertir los pixels de image a los floats de hData
    for(int y=0; y<height; y++) {
        for(int x=0; x<width; x++) {
            const int val= 255 - qRed(image.pixel(x, y));
            hData[x + y * width]= val / 255.0f;
        }
    }

    // Subir los datos de hData a dData1
    cl_int error;
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {width, height, 1};
    error= clEnqueueWriteImage(clQueue, dData1, CL_FALSE, origin, region, 0, 0, hData, 0, NULL, NULL);
    if(checkError(error, "clEnqueueWriteImage"))
        return false;

    firstRun= false;
    return true;
}

// Codigo del nuevo hilo
void FDMHeat::run()
{
    // Work group y NDRange del kernel
    size_t workGroupSize[2] = { 16, 16 };
    size_t ndRangeSize[2];
    ndRangeSize[0]= roundUp(width, workGroupSize[0]);
    ndRangeSize[1]= roundUp(height, workGroupSize[1]);

    bool even= true;
    iteration= 0;

    // Iterar hasta que se setee el semaforo finish (con stop())
    while(!finish.tryAcquire()) {
        dataLock.lock();
        // Seteamos las referencias a los ping pong buffers segun si iteration es par o no
        dataInput= even ? dData1 : dData2;
        dataOutput= even ? dData2 : dData1;
        dataLock.unlock();

        // Ejecutamos el kernel, sin descargar los resultados
        cl_int error;
        error  = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&dataInput);
        error |= clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&dataOutput);

        error |= clEnqueueNDRangeKernel(clQueue, kernel, 2, NULL, ndRangeSize, workGroupSize, 0, NULL, NULL);
        checkError(error, "FDMHeat::run: clEnqueueNDRangeKernel");

        // Esperar a que termine de ejecutarse el kernel
        // Este es un thread separado, asi que podemos "trabarlo"
        clFinish(clQueue);

        iteration.ref();
        even= !even; // true cuando iteration es par
    }

    qDebug() << "FDMHeat::run: Terminando thread.";
}

void FDMHeat::suspend()
{
    if(suspended)
        return;
    dataLock.lock();
    suspended= true;
}

void FDMHeat::resume()
{
    if(!suspended)
        return;
    dataLock.unlock();
    suspended= false;
}

void FDMHeat::drawHeatQuad(QPoint center, int size, bool hot)
{
    // Work group y NDRange del kernel
    size_t workGroupSize[2] = { 16, 16 };
    size_t ndRangeSize[2];
    ndRangeSize[0]= roundUp(width, workGroupSize[0]);
    ndRangeSize[1]= roundUp(height, workGroupSize[1]);

    if(!suspended)
        dataLock.lock();

    int bx= center.x();
    int by= center.y();
    float value= hot ? 1.0f : 0.0f;

    cl_int error;
    error  = clSetKernelArg(brushKernel, 0, sizeof(cl_mem), (void*)&dataOutput);
    error |= clSetKernelArg(brushKernel, 1, sizeof(int), (void*)&bx);
    error |= clSetKernelArg(brushKernel, 2, sizeof(int), (void*)&by);
    error |= clSetKernelArg(brushKernel, 3, sizeof(int), (void*)&size);
    error |= clSetKernelArg(brushKernel, 4, sizeof(float), (void*)&value);

    error |= clEnqueueNDRangeKernel(clQueue, brushKernel, 2, NULL, ndRangeSize, workGroupSize, 0, NULL, NULL);
    checkError(error, "FDMHeat::drawHeatQuad: clEnqueueNDRangeKernel");

    if(!suspended)
        dataLock.unlock();
}
