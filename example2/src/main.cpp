#include <iostream>
#include <iomanip>

#include <cstring>
// Header de OpenCL
#include <CL/cl.h>
// Utilidades propias para OpenCL
#include "clutils.h"

#define BLOCKSIZE 16

using namespace std;

int main(int argc, char *argv[])
{
  
    /// Argumentos de entrada al programa
    if(argc < 2) {
      cerr << "usage: ./matrixtranspose {sharedmem|globalmem} [matrixSize]" << endl;
      return EXIT_FAILURE;      
    }
    
    const char* memoryParam = argv[1];
    if(strcmp(memoryParam, "sharedmem") != 0 and strcmp(memoryParam, "globalmem") != 0) {
      cerr << "usage: ./matrixtranspose {sharedmem|globalmem} [matrixSize]" << endl;
      cerr << "Parametro incorrecto: " << memoryParam << endl;
      return EXIT_FAILURE;      
    }
    
    const bool withSharedMemory = (strcmp(memoryParam, "sharedmem")==0) ? true : false;
    // Dimension de la matriz, por defecto 2048 x 2048
    const int n= (argc==3) ? atoi(argv[2]) : 2048;
    // Tamanio en bytes de la matriz (por defecto 2048*2048*4 = 16 MiB)
    const int matrixBytes= n * n * sizeof(float);
    
    /// Inicializacion
    cerr << "Configurando OpenCL." << endl;
    // Crear contexto y cola de comandos de OpenCL
    cl_context clContext;
    cl_command_queue clQueue;
    cl_device_id clDevice;
    if(!setupOpenCL(clContext, clQueue, clDevice))
        return EXIT_FAILURE;

    /// Cargar del programa a ejecutar en GPU
    // Cargar texto de programa a un string
    cerr << "Cargando programa." << endl;
    char* programText;
    size_t programLength;
    if(!loadProgramText("../src/matrixtranspose.cl", &programText, &programLength)) {
        cerr << "Error al cargar archivo de programa." << endl;
        return EXIT_FAILURE;
    }
    // Crear programa
    cl_int error;
    cl_program program;
    program= clCreateProgramWithSource(clContext, 1, (const char **)&programText, (const size_t *)&programLength, &error);
    if(checkError(error, "clCreateProgramWithSource"))
        return EXIT_FAILURE;
    // Compilar programa para todos los dispositivos del contexto
    error= clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if(checkError(error, "clBuildProgram")) {
	checkProgramBuild(program, clDevice);
        return EXIT_FAILURE;
    }
    
    const char* kernelName = (withSharedMemory) ? "transposeShMem" : "transpose";	
    // Crear kernel a partir del programa (un programa puede tener varios kernels)
    cl_kernel kernel;
    kernel= clCreateKernel(program, kernelName, &error);
    if(checkError(error, "clCreateKernel"))
        return EXIT_FAILURE;

    //
    // Reserva de memoria
    //

    //  - Matriz hA: Entrada en memoria de CPU (Host)
    //  - Matriz dA: Entrada en memoria de GPU (Device)
    //  - Matriz hB: Salida en memoria de CPU (Host). Memoria de solo lectura para el kernel.
    //  - Matriz dB: Salida en memoria de GPU (Device). Memoria de solo escritura para el kernel.
    // Se usara indexado row-major: http://en.wikipedia.org/wiki/Row-major
    cerr << "Reservando memoria." << endl;

    float* hA= (float*)malloc(matrixBytes);
    float* hB= (float*)malloc(matrixBytes);

    cl_int error1, error2;
    cl_mem dA= clCreateBuffer(clContext, CL_MEM_READ_ONLY, matrixBytes, NULL, &error1);
    cl_mem dB= clCreateBuffer(clContext, CL_MEM_WRITE_ONLY, matrixBytes, NULL, &error2);

    //  Verificar que se pudo reservar toda memoria
    if(!hA or !hB or checkError(error1, "clCreateBuffer") or checkError(error2, "clCreateBuffer")) {
        cerr << "Error al reservar memoria." << endl;
        return EXIT_FAILURE;
    }

    /// Inicializacion de los datos
    // 1. Llenar la matriz A de entrada en el CPU con datos aleatorios entre 0 y 1
    cerr << "Inicializando datos." << endl;
    srand(42);
    for(int i=0; i<n; i++)
        for(int j=0; j<n; j++) 
            hA[i * n + j]= (float)rand()/RAND_MAX;
    
    /// Subir los datos de entrada a la GPU (de hA a dA)
    // Esta operacion se realiza de forma asincronica: se encola en el queue y el programa continua
    cerr << "Subiendo datos de entrada." << endl;
    error= clEnqueueWriteBuffer(clQueue, dA, CL_FALSE, 0, matrixBytes, hA, 0, NULL, NULL);
    if(checkError(error, "clEnqueueWriteBuffer"))
        return EXIT_FAILURE;
    
    /// Ejecutar el kernel
    // Primero se determina el tamanio de work-group y la cantidad total de threads en el NDRange
    size_t workGroupSize[2] = { BLOCKSIZE, BLOCKSIZE };
    // Para cada dimension se redondea hacia arriba la cantidad de threads total para que sea
    // multiplo del tamanio del work-group
    size_t ndRangeSize[2];
    ndRangeSize[0]= roundUp(n, workGroupSize[0]);
    ndRangeSize[1]= roundUp(n, workGroupSize[1]);
    // Setean los parametros del kernel, y luego se encola su ejecucion
    cerr << "Ejecutando kernel \'" << kernelName << "\'." << endl;
    error  = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&dA);
    error |= clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&dB);
    error |= clSetKernelArg(kernel, 2, sizeof(cl_int), (void*)&n);
    if (withSharedMemory) { // en caso de querer usar memoria compartida, debo pasar como argumento su tamanio
	error |= clSetKernelArg(kernel, 3, BLOCKSIZE * BLOCKSIZE * sizeof(float), (void*) NULL);
    }
    cl_event kernelExecEvent;
    error |= clEnqueueNDRangeKernel(clQueue, kernel, 2, NULL, ndRangeSize, workGroupSize, 0, NULL, &kernelExecEvent);
    if(checkError(error, "clEnqueueNDRangeKernel"))
        return EXIT_FAILURE;
    
    /// Download de los resultados
    // Esta vez indicamos que la operacion sea sincronica, por lo que el CPU va a quedar esperando
    // a que se ejecuten las tareas anteriores de la cola (paso 2 y 3) asi como esta tarea
    error= clEnqueueReadBuffer(clQueue, dB, CL_TRUE, 0, matrixBytes, hB, 0, NULL, NULL);
    if(checkError(error, "clEnqueueReadBuffer"))
        return EXIT_FAILURE;

    // Obtengo tiempo de ejecucion
    float execTime = eventElapsed(kernelExecEvent);
    
    cerr << "Tamanio matriz:\t\t\t\t(" << n << ", " << n << "), " << n * n << " elementos," << " ~" << matrixBytes/1024/1024 << " MiB." << endl;
    cerr << "Cantidad de Work-groups en el Grid:\t(" << ndRangeSize[0]/workGroupSize[0] << ", " << ndRangeSize[1]/workGroupSize[1] << ")" << endl;
    cerr << "Tamanio de Work-groups:\t\t\t(" << workGroupSize[0] << ", " << workGroupSize[1] << ")" << endl;
    cerr << "Tamanio global de Grid:\t\t\t(" << ndRangeSize[0] << ", " << ndRangeSize[1] << ")" << endl;
    cerr << "Tiempo de ejecucion:\t\t\t" << setprecision(2) << execTime << " ms." << endl;

    /// Verificacion de la salida
    cerr << "Verificando salida." << endl;

    int errorCount= 0;
    for(int i=0; i<n; i++) {
        for(int j=0; j<n; j++) {
            const int index= i * n + j;
	    const int transposeIndex= j * n + i;
            // Comparamos bit-a-bit porque ambos procesadores deberian implementar el estandar
            // de floating point IEEE 754-2008
            if(hB[index] != hA[transposeIndex])
                errorCount++;
        }
    }
    if(!errorCount)
        cerr << "Salida OK :D" << endl;
    else
        cerr << errorCount << " elementos erroneos." << endl;
    
    /// Liberacion de memoria reservada
    // libero memoria de Host
    cerr << "Liberacion de Memoria reservada." << endl;
    free(hA);
    free(hB);
    // libero memoria de GPU (cl_mems)
    clReleaseMemObject(dA);
    clReleaseMemObject(dB);
    // libero objetos de OpenCL
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(clQueue);
    clReleaseContext(clContext);
    
    cerr << "Fin." << endl;

    return EXIT_SUCCESS;
}
