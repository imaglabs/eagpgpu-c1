#include "setupclgl.h"

bool setupOpenCLGL(cl_context& context, cl_command_queue& queue, cl_device_id &device)
{
    cl_int clError;

    // Obtener informacion de la plataforma
    cl_platform_id platform;
    clError= clGetPlatformIDs(1, &platform, NULL);
    if(checkError(clError, "clGetPlatformIDs"))
        return false;

    // Seleccionar la GPU por defecto
    clError= clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if(checkError(clError, "clGetDeviceIDs"))
        return false;

    // Crear contexto global para la GPU seleccionada
    cl_context_properties props[] =  {
        CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
        CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
        0
    };
    context= clCreateContext(props, 1, &device, NULL, NULL, &clError);

    if(checkError(clError, "clCreateContext"))
        return false;

    // Crear una cola de comandos la GPU seleccionada
    queue= clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &clError);
    if(checkError(clError, "clCreateCommandQueue"))
        return false;

    return true;
}
