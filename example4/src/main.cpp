#include <iostream>

#include <cstring>
// Header de OpenCL
#include <CL/cl.h>
// Utilidades propias para OpenCL
#include "clutils.h"

using namespace std;

int main(int argc, char *argv[])
{
    
    /// Argumentos de entrada al programa
    if(argc < 2) {
      cerr << "usage: ./atomics {globalcounter|localcounter} [numberOfElements]" << endl;
      return EXIT_FAILURE;      
    }
    
    const char* memoryParam = argv[1];
    if(strcmp(memoryParam, "globalcounter") != 0 and strcmp(memoryParam, "localcounter") != 0) {
      cerr << "usage: ./atomics {globalcounter|localcounter} [numberOfElements]" << endl;
      cerr << "Parametro incorrecto: " << memoryParam << endl;
      return EXIT_FAILURE;      
    }
    
    const bool withSharedMemory = (strcmp(memoryParam, "localcounter")==0) ? true : false;
    
    cerr << "Configurando OpenCL." << endl;
    // Crear contexto y cola de comandos de OpenCL
    cl_context clContext;
    cl_command_queue clQueue;
    cl_device_id clDevice;
    
    if(!setupOpenCL(clContext, clQueue, clDevice))
        return EXIT_FAILURE;

    // Cantidad de elementos en el arreglo de datos
    const int n= (argc==3) ? atoi(argv[2]) : 100000000;
    // Tamanio en bytes del arreglo de datos
    const int hABytes= n * sizeof(int);
    // porcentaje de ocurrencia del valor a contar
    float occurrFactor = 0.4f;
	
    //
    // Cargar el programa
    //

    // Cargar texto de programa a un string
    cerr << "Cargando programa." << endl;
    char* programText;
    size_t programLength;
    if(!loadProgramText("../src/atomics.cl", &programText, &programLength)) {
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

    // Crear kernel a partir del programa (un programa puede tener varios kernels)
    cl_kernel kernel;
    const char* kernelName = (withSharedMemory) ? "shMemCounter" : "globCounter";
    kernel= clCreateKernel(program, kernelName, &error);
    if(checkError(error, "clCreateKernel"))
        return EXIT_FAILURE;

    //
    // Reserva de memoria
    //

    cerr << "Reservando memoria." << endl;

    int* hA= (int*)malloc(hABytes);
    
    cl_int error1, error2;
    // Reservar el arreglo en el dispositivo
    cl_mem dA = clCreateBuffer(clContext, CL_MEM_READ_ONLY, hABytes, NULL, &error1);
    // Creacion e inicializacion de contador en dispositivo
    cl_mem dCounter= clCreateBuffer(clContext, CL_MEM_READ_WRITE, sizeof(cl_uint), NULL, &error2);

    //  Verificar que se pudo reservar toda memoria
    if(!hA or checkError(error1, "clCreateBuffer") or checkError(error2, "clCreateBuffer")) {
        cerr << "Error al reservar memoria." << endl;
        return EXIT_FAILURE;
    }

    //
    // Inicializacion de los datos
    //

    // 1. Lleno el arreglo de entrada hA (de CPU) con datos enteros aleatorios 
    cerr << "Inicializando datos." << endl;
    srand(31);
    int countingValue = rand();
    for(int i=0; i<n; ++i) {
	hA[i] = (float(rand())/RAND_MAX < occurrFactor) ? countingValue : rand();
    }
    
    // count the number of ocurrences (referencia)
    uint referenceCounter = 0;
    for(int i=0; i < n; ++i) {
	if (hA[i] == countingValue)
	    referenceCounter++;
    }
    
    // 2. Subir los datos de entrada a la GPU (de hA a dA)
    // Esta operacion se realiza de forma asincronica: se encola en el queue y el programa continua
    cerr << "Subiendo datos de entrada." << endl;
    error= clEnqueueWriteBuffer(clQueue, dA, CL_FALSE, 0, hABytes, hA, 0, NULL, NULL);
    if(checkError(error, "clEnqueueWriteBuffer"))
        return EXIT_FAILURE;

    uint zero= 0;
    error= clEnqueueWriteBuffer(clQueue, dCounter, CL_FALSE, 0, sizeof(cl_uint), &zero, 0, NULL, NULL);
    if(checkError(error, "clEnqueueWriteBuffer"))
        return EXIT_FAILURE;

    // 3. Ejecutar el kernel
    // Primero se determina el tamanio de work-group y la cantidad total de threads en el NDRange
    const size_t workGroupSize= 1024;
    // Para cada dimension se redondea hacia arriba la cantidad de threads total para que sea
    // multiplo del tamanio del work-group
    const size_t ndRangeSize= roundUp(n, workGroupSize);
    
    cl_event kernelExecEvent;
    
    // Setean los parametros del kernel, y luego se encola su ejecucion
    cerr << "Ejecutando kernel." << endl;
    error  = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&dA);
    error |= clSetKernelArg(kernel, 1, sizeof(cl_uint), (void*)&n);
    error |= clSetKernelArg(kernel, 2, sizeof(cl_int), (void*)&countingValue);
    if (withSharedMemory) {
      error |= clSetKernelArg(kernel, 3, sizeof(cl_uint), (void*)NULL);
      error |= clSetKernelArg(kernel, 4, sizeof(cl_mem), (void*)&dCounter);
    }
    else {
      error |= clSetKernelArg(kernel, 3, sizeof(cl_mem), (void*)&dCounter);
    }
    error |= clEnqueueNDRangeKernel(clQueue, kernel, 1, NULL, &ndRangeSize, &workGroupSize, 0, NULL, &kernelExecEvent);
    if(checkError(error, "clEnqueueNDRangeKernel"))
        return EXIT_FAILURE;
    
    // 4. Bajar resultados
    // Esta vez indicamos que la operacion sea sincronica, por lo que el CPU va a quedar esperando
    // a que se ejecuten las tareas anteriores de la cola (paso 2 y 3) asi como esta tarea
    uint dResult=0;
    error= clEnqueueReadBuffer(clQueue, dCounter, CL_TRUE, 0, sizeof(cl_uint), &dResult, 0, NULL, NULL);
    if(checkError(error, "clEnqueueReadBuffer")) {
        return EXIT_FAILURE;
    }

    cerr << "Number of elements\t" << n << " (" << hABytes/1024.0/1024.0 << " MiB)" << endl;
    cerr << "Occurrence factor\t" << occurrFactor*100 << "%" << endl;
    cerr << "Work-group size\t\t" << workGroupSize<< endl;
    cerr << "ND-Range size\t\t" << ndRangeSize << endl;
    cerr << "Tiempo de ejecucion\t" << eventElapsed(kernelExecEvent) << " ms." << endl;
    
    //
    // Verificacion
    //
    
    if (referenceCounter == dResult) {
	cerr << "Operacion de conteo exitosa, se encontraron " << dResult << " elementos." << endl;
    }
    else {
	cerr << "Error en el conteo de ocurrencias, se contaron " << dResult << " elementos, siendo el valor de referencia =" << referenceCounter << "." << endl;
    }
    
    // Liber de mem
    //
    // Libero memoria reservada
    //
    // libero memoria de Host
    cerr << "Liberacion de Memoria reservada." << endl;
    free(hA);
    // libero memoria de Dispositivo
    clReleaseMemObject(dA);
    clReleaseMemObject(dCounter);
    // libero objetos de OpenCL
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(clQueue);
    clReleaseContext(clContext);
    
    cerr << "Fin." << endl;
    
    return EXIT_SUCCESS;
}
