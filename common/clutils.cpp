#include "clutils.h"

#include <iostream>
#include <fstream>

using namespace std;

bool setupOpenCL(cl_context& context, cl_command_queue& queue, cl_device_id& device)
{
    cl_int clError;

    // Obtener informacion de la plataforma
    cl_platform_id platform;
    clError= clGetPlatformIDs(1, &platform, NULL);
    if(checkError(clError, "setupOpenCL: clGetPlatformIDs"))
        return false;

    // Seleccionar la GPU por defecto
    clError= clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if(checkError(clError, "setupOpenCL: clGetDeviceIDs"))
        return false;

    context= clCreateContext(0, 1, &device, NULL, NULL, &clError);
    if(checkError(clError, "setupOpenCL: clCreateContext"))
        return false;

    // Crear una cola de comandos la GPU seleccionada
    queue= clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &clError);
    if(checkError(clError, "setupOpenCL: clCreateCommandQueue"))
        return false;

    return true;
}

string clErrorToString(cl_int err)
{
    switch (err) {
        case CL_SUCCESS:                            return "Success!";
        case CL_DEVICE_NOT_FOUND:                   return "Device not found.";
        case CL_DEVICE_NOT_AVAILABLE:               return "Device not available";
        case CL_COMPILER_NOT_AVAILABLE:             return "Compiler not available";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:      return "Memory object allocation failure";
        case CL_OUT_OF_RESOURCES:                   return "Out of resources";
        case CL_OUT_OF_HOST_MEMORY:                 return "Out of host memory";
        case CL_PROFILING_INFO_NOT_AVAILABLE:       return "Profiling information not available";
        case CL_MEM_COPY_OVERLAP:                   return "Memory copy overlap";
        case CL_IMAGE_FORMAT_MISMATCH:              return "Image format mismatch";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:         return "Image format not supported";
        case CL_BUILD_PROGRAM_FAILURE:              return "Program build failure";
        case CL_MAP_FAILURE:                        return "Map failure";
        case CL_INVALID_VALUE:                      return "Invalid value";
        case CL_INVALID_DEVICE_TYPE:                return "Invalid device type";
        case CL_INVALID_PLATFORM:                   return "Invalid platform";
        case CL_INVALID_DEVICE:                     return "Invalid device";
        case CL_INVALID_CONTEXT:                    return "Invalid context";
        case CL_INVALID_QUEUE_PROPERTIES:           return "Invalid queue properties";
        case CL_INVALID_COMMAND_QUEUE:              return "Invalid command queue";
        case CL_INVALID_HOST_PTR:                   return "Invalid host pointer";
        case CL_INVALID_MEM_OBJECT:                 return "Invalid memory object";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:    return "Invalid image format descriptor";
        case CL_INVALID_IMAGE_SIZE:                 return "Invalid image size";
        case CL_INVALID_SAMPLER:                    return "Invalid sampler";
        case CL_INVALID_BINARY:                     return "Invalid binary";
        case CL_INVALID_BUILD_OPTIONS:              return "Invalid build options";
        case CL_INVALID_PROGRAM:                    return "Invalid program";
        case CL_INVALID_PROGRAM_EXECUTABLE:         return "Invalid program executable";
        case CL_INVALID_KERNEL_NAME:                return "Invalid kernel name";
        case CL_INVALID_KERNEL_DEFINITION:          return "Invalid kernel definition";
        case CL_INVALID_KERNEL:                     return "Invalid kernel";
        case CL_INVALID_ARG_INDEX:                  return "Invalid argument index";
        case CL_INVALID_ARG_VALUE:                  return "Invalid argument value";
        case CL_INVALID_ARG_SIZE:                   return "Invalid argument size";
        case CL_INVALID_KERNEL_ARGS:                return "Invalid kernel arguments";
        case CL_INVALID_WORK_DIMENSION:             return "Invalid work dimension";
        case CL_INVALID_WORK_GROUP_SIZE:            return "Invalid work group size";
        case CL_INVALID_WORK_ITEM_SIZE:             return "Invalid work item size";
        case CL_INVALID_GLOBAL_OFFSET:              return "Invalid global offset";
        case CL_INVALID_EVENT_WAIT_LIST:            return "Invalid event wait list";
        case CL_INVALID_EVENT:                      return "Invalid event";
        case CL_INVALID_OPERATION:                  return "Invalid operation";
        case CL_INVALID_GL_OBJECT:                  return "Invalid OpenGL object";
        case CL_INVALID_BUFFER_SIZE:                return "Invalid buffer size";
        case CL_INVALID_MIP_LEVEL:                  return "Invalid mip-map level";
        default: return "Unknown";
    }
}

bool checkError(cl_int error, const char* msg)
{
    if(error == CL_SUCCESS)
        return false;

    cerr << "** OpenCL Error '" << clErrorToString(error);
    if(msg)
        cerr << "': " << msg << "." << endl;
    else
        cerr << "'." << endl;
    return true;
}

int roundUp(int count, int multiple)
{
    int r = count % multiple;
    if(!r)
        return count;
    else
        return count + multiple - r;
}

float eventElapsed(cl_event event)
{
    cl_int error;

    cl_ulong start_time, end_time;
    error = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
    error |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, NULL);
    checkError(error, "eventElapsed: clGetEventProfilingInfo");

    return float(end_time - start_time) * 1.0e-6f; // in ms.
}

bool loadKernel(cl_context context, cl_kernel* kernel, cl_device_id device, const char* path, const char* kernelName)
{
    // Cargar texto de programa a un string
    char* programText;
    size_t programLength;
    if(!loadProgramText(path, &programText, &programLength)) {
        cerr << "Error al cargar archivo de programa." << endl;
        return false;
    }
    // Crear programa
    cl_int error;
    cl_program program;
    program= clCreateProgramWithSource(context, 1, (const char **)&programText, (const size_t *)&programLength, &error);
    free(programText);
    if(checkError(error, "loadKernel: clCreateProgramWithSource"))
        return false;
    // Compilar programa para todos los dispositivos del contexto
    error= clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if(checkError(error, "loadKernel: clBuildProgram")) {
        checkProgramBuild(program, device);
        return false;
    }

    // Crear kernel a partir del programa (un programa puede tener varios kernels)
    *kernel= clCreateKernel(program, kernelName, &error);
    if(checkError(error, "loadKernel: clCreateKernel"))
        return false;

    clReleaseProgram(program);

    return true;
}


bool loadProgramText(const char* path, char** text, size_t* length)
{
    ifstream is(path);
    if(!is.is_open()) {
        cerr << "Error al cargar archivo '" << path << "'." << endl;
        return false;
    }

    is.seekg(0, ios::end);
    *length= is.tellg();
    is.seekg(0, ios::beg);

    *text= (char*)malloc(*length + 1);

    is.read(*text, *length);
    is.close();

    // Finalizar string
    (*text)[*length]= 0;

    return true;
}

void checkProgramBuild(cl_program program, cl_device_id device)
{
    cl_int clError;
    cl_build_status build_status;
    clError = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_STATUS,
                    sizeof(cl_build_status), &build_status, NULL);
    checkError(clError, "checkProgramBuild: clGetProgramBuildInfo");


    char *build_log;
    size_t ret_val_size;
    clError = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                    0, NULL, &ret_val_size);
    checkError(clError, "checkProgramBuild: clGetProgramBuildInfo");

    build_log = new char[ret_val_size+1];
    clError = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                    ret_val_size, build_log, NULL);
    checkError(clError, "checkProgramBuild: clGetProgramBuildInfo");

    // end of string character
    build_log[ret_val_size] = '\0';

    if (build_status != CL_BUILD_SUCCESS) {
        cerr << "Error building program:" << endl;
        cerr << build_log << endl;
    }

}

