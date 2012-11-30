#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QGLWidget>

#include "fdmheat.h"
#include "clutils.h"
#include "setupclgl.h"

class FDMHeatWidget : public QGLWidget
{
Q_OBJECT

public:
    FDMHeatWidget(QSize maxSize= QSize(512, 512));

    void setSystem(FDMHeat* sys);

    void setDisplayFramerate(float hz) { displayTimer.start(1000/hz); } // render period 33 ms. 

    cl_context getCLContext() { return clContext; }
    cl_command_queue getCLQueue() { return clQueue; }
    cl_device_id getCLDevice() { return clDevice; }

    void waitCLConfig() { clConfigReady.acquire(); }

protected:
    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);
    void keyPressEvent(QKeyEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent *event);

private:
    void initializeCL();
    void updateSystemTexture();
    void setFullScreen(bool fullScreen);

    int maxWidth;
    int maxHeight;

    QTimer displayTimer;

    // Referencia al sistema que visualizamos
    FDMHeat* system;
    int lastIteration;

    // OpenCL
    cl_context clContext;
    cl_command_queue clQueue;
    cl_device_id clDevice;

    cl_kernel renderKernel;
    cl_mem textureMem; // texture mapeada a OpenCL
    cl_mem paletteMem; // constant memory donde cargamos la paleta
    // OpenGL
    GLuint texture;

    QSemaphore clConfigReady;

    bool drawing;
    bool drawingHot;
    int borderWidth;
    int borderHeight;
};

#endif // GLWIDGET_H



