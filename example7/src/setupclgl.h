#ifndef SETUPCLGL_H
#define SETUPCLGL_H

#include <CL/cl.h>

// Header de interoperabilidad CL/GL
#include <CL/cl_gl.h>
// Header de funciones de OpenGL especificas para X11
#include <GL/glx.h>

#include "clutils.h"

// Funcion similar a setupOpenCL de clutils, pero que crea un contexto
// OpenCL con interoperabilidad OpenGL
//
// El contexto OpenGL ya debe estar creado cuando se llama a esta funcion
//
// Esta funcion esta implementada solo para Linux/X11

bool setupOpenCLGL(cl_context& context, cl_command_queue& queue, cl_device_id& device);

#endif // SETUPCLGL_H
