#include <iostream>

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS

// Header de OpenCL
#include <CL/cl.h>
// Utilidades propias para OpenCL
#include "clutils.h"

// Utilizamos la clase QImage de Qt para cargar y escribir en imagenes .png
#include <QImage>

using namespace std;


int main(int argc, char *argv[])
{
    cl_context clContext;
    cl_command_queue clQueue;
    cl_device_id clDevice;
    cl_kernel kernel;

    cerr << "Configurando OpenCL." << endl;
    if(!setupOpenCL(clContext, clQueue, clDevice))
        return EXIT_FAILURE;

    cerr << "Cargando programa." << endl;
    if(!loadKernel(clContext, &kernel, clDevice, "../src/fdmHeat.cl", "fdmHeat"))
        return EXIT_FAILURE;

    /// Cargar estado inicial del sistema de una imagen
    // Cada pixel va a representar una celda de la simulacion
    cerr << "Cargando imagen de entrada." << endl;
    QImage inputImage= QImage("input.png").convertToFormat(QImage::Format_RGB888);
    if(inputImage.isNull()) {
        cerr << "Error al cargar imagen." << endl;
        return EXIT_FAILURE;
    }
    
    // Paramatros de la simulacion
    const int iterations= argc==2 ? atoi(argv[1]) : 1000;
    const int width= inputImage.width();
    const int height= inputImage.height();
    const int bytes= width * height * sizeof(float); // Tamanio en bytes del sistema

    /// Alocacion de memoria
    cerr << "Reservando memoria." << endl;

    // hInput: Entrada y salida en el host (usamos el mismo buffer)
    float* hData= (float*)malloc(bytes);

    // El sistema se almacena en la memoria del GPU como un "imagen de un canal" (matriz 2D)
    // con elementos de tipo float
    // Reservamos dos buffers para usar la tecnica de "ping pong"
    cl_image_format format;
    format.image_channel_data_type= CL_FLOAT;
    format.image_channel_order= CL_INTENSITY;
    cl_int error1, error2;
    cl_mem dData1= clCreateImage2D(clContext, CL_MEM_READ_WRITE, &format, width, height, 0, NULL, &error1);
    cl_mem dData2= clCreateImage2D(clContext, CL_MEM_READ_WRITE, &format, width, height, 0, NULL, &error2);

    //  Verificar que se pudo reservar toda memoria
    if(!hData or checkError(error1, "clCreateImage2D") or checkError(error2, "clCreateImage2D")) {
        cerr << "Error al reservar memoria." << endl;
        return EXIT_FAILURE;
    }

    /// Computo
    // Convertir los pixels de inputImage a los floats de hData
    cerr << "Inicializando datos." << endl;
    for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
            hData[x + y * width]= qRed(inputImage.pixel(x, y)) / 255.0f;

    // Subir los datos de hData a dData1
    cl_int error;
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {width, height, 1};
    error= clEnqueueWriteImage(clQueue, dData1, CL_FALSE, origin, region, 0, 0, hData, 0, NULL, NULL);
    if(checkError(error, "clEnqueueWriteImage"))
        return EXIT_FAILURE;

    // Ejecutar el kernel
    // Work group y NDRange
    size_t workGroupSize[2] = { 16, 16 };
    size_t ndRangeSize[2];
    ndRangeSize[0]= roundUp(width, workGroupSize[0]);
    ndRangeSize[1]= roundUp(height, workGroupSize[1]);

    // Setean los parametros del kernel, y luego se encola su ejecucion
    cerr << "Ejecutando kernel." << endl;
    bool even=false;
    for(int i=0; i<iterations; i++) {
        even= !(i % 2); // true cuando i es par

        // Cuando no es par, invertimos el orden de los parametros del kernel
        error |= clSetKernelArg(kernel, (even ? 0 : 1), sizeof(cl_mem), (void*)&dData1);
        error |= clSetKernelArg(kernel, (even ? 1 : 0), sizeof(cl_mem), (void*)&dData2);

        error |= clEnqueueNDRangeKernel(clQueue, kernel, 2, NULL, ndRangeSize, workGroupSize, 0, NULL, NULL);
        if(checkError(error, "clEnqueueNDRangeKernel"))
            return EXIT_FAILURE;
    }

    /// Bajar resultados
    // Dependiendo de si la ultima iteracion fue par o no, bajamos de dData2 o dData1
    error= clEnqueueReadImage(clQueue, (even ? dData1 : dData2), CL_TRUE, origin, region, 0, 0, hData, 0, NULL, NULL);
    if(checkError(error, "clEnqueueReadImage"))
        return EXIT_FAILURE;

    // Convertir los floats del sistema a pixels, utilizando una paleta de colores
    QImage palette("palette.png");
    if(palette.isNull()) {
        cerr << "Error al cargar paleta." << endl;
        return EXIT_FAILURE;
    }

    QImage outputImage(width, height, QImage::Format_RGB888);
    for(int y=0; y<height; y++) {
        for(int x=0; x<width; x++) {
            // Valor de la celda
            const float value= hData[x + y * width];
            // Convertir de 0..1 a 0..255, asegurandonos de no exceder los 255
            const uchar position= qMin(value * 255.0f, 255.0f);
            outputImage.setPixel(x, y, palette.pixel(position, 0));
        }
    }
    outputImage.save("output.png");

    cerr << "Iterations     : " << iterations << endl;
    cerr << "System size    : (" << width << ", " << height << ")" << endl;
    cerr << "System cells   : " << width * height << " -> ~" << bytes/1024 << " KiB" << endl;
    cerr << "Work-group size: (" << workGroupSize[0] << ", " << workGroupSize[1] << ")" << endl;
    cerr << "ND-Range size  : (" << ndRangeSize[0] << ", " << ndRangeSize[1] << ")" << endl;

    cerr << "Fin." << endl;
    
    return EXIT_SUCCESS;
}
