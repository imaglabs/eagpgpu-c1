#include "fdmheatwidget.h"

#include <GL/glu.h>

FDMHeatWidget::FDMHeatWidget(QSize maxSize) :
    QGLWidget()
{
    maxWidth= maxSize.width();
    maxHeight= maxSize.height();

    system= 0;
    lastIteration= 0;

    // Cada vez que displayTimer se dispare, actualizar el render
    connect(&displayTimer, SIGNAL(timeout()), this, SLOT(updateGL()));
    // Inicializar el timer de visualizacion
    setDisplayFramerate(30);

    drawing= false;
    drawingHot= false;
    setMouseTracking(true);
}


void FDMHeatWidget::initializeGL()
{
    qDebug() << "Initialize OpenGL";
    // Color de fondo gris
    qglClearColor(Qt::darkGray);
    // Activar texturas
    glEnable(GL_TEXTURE_2D);

    // Crear la textura OpenGL
    QImage tempImg(maxWidth, maxHeight, QImage::Format_RGB32);
    tempImg.fill(Qt::darkGray);
    texture= bindTexture(tempImg);

    // Inicializamos OpenCL despues de inicializar OpenGL
    // asi pueden compartir el contexto
    initializeCL();
}

void FDMHeatWidget::initializeCL()
{
    // Configurar OpenCL con soporte de OpenGL
    if(!setupOpenCLGL(clContext, clQueue, clDevice)) {
        qDebug() << "FDMHeatWidget::initializeCL: Error al configurar OpenCL.";
        return;
    }

    if(!loadKernel(clContext, &renderKernel, clDevice, "../src/systemToImage.cl", "systemToImage")) {
        qDebug() << "FDMHeatWidget::initializeCL: Error al cargar kernel.";
        return;
    }

    // Mapear la memoria la textura en OpenCL
    // Desde OpenCL solo vamos a escribir en la textura.
    cl_int error;
    textureMem= clCreateFromGLTexture2D(clContext, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture, &error);
    if(checkError(error, "clCreateFromGLTexture2D"))
        return;

    // Cargar paleta y subirla a la GPU
    QImage palette("palette.png");
    if(palette.isNull())
        return;
    paletteMem= clCreateBuffer(clContext, CL_MEM_READ_ONLY, palette.byteCount(), NULL, &error);
    error |= clEnqueueWriteBuffer(clQueue, paletteMem, CL_FALSE, 0, palette.byteCount(), palette.bits(), 0, NULL, NULL);
    if(checkError(error, "clCreateBuffer"))
        return;

    // Liberamos el semaforo
    clConfigReady.release();
}

void FDMHeatWidget::resizeGL(int width, int height)
{
    if(!system)
        return;

    // Aspect ratio del widget y del sistema
    const float widgetAR= (float)width / height;
    const float systemAR= (float)system->getWidth() / system->getHeight();

    borderWidth= 0;
    borderHeight= 0;

    if(widgetAR > systemAR)
        // Widget "mas fino" que el sistema
        borderWidth=  (width - height*systemAR) / 2;
    else
        // Sistema "mas fino" que el widget
        borderHeight= (height - width/systemAR) / 2;

    // Setear el viewport
    glViewport(borderWidth, borderHeight, width-borderWidth*2, height-borderHeight*2);

    // Setear proyeccion ortogonal 2D
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,1, 0,1);
}

void FDMHeatWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Si no se seteo el sistema con setSystem no mostramos nada
    if(!system)
        return;

    // Si se actualizo el sistema, actualizamos la textura
    int iteration= system->getIteration();
    if((iteration != lastIteration and !system->isSuspended()) or drawing) {
        updateSystemTexture();
        lastIteration= iteration;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Calcular que porcentaje de la textura reservada (de tamanio maxWidth*maxHeight) usamos
    float w= (float)system->getWidth() / maxWidth;
    float h= (float)system->getHeight() / maxHeight;

    // Dibujar un cuadrado del tamanio del viewport con la textura
    glBegin(GL_QUADS);
        glTexCoord2f(0, h); glVertex2f(0, 0);
        glTexCoord2f(0, 0); glVertex2f(0, 1);
        glTexCoord2f(w, 0); glVertex2f(1, 1);
        glTexCoord2f(w, h); glVertex2f(1, 0);
    glEnd();
}

void FDMHeatWidget::updateSystemTexture()
{
    cl_int error;

    error= clEnqueueAcquireGLObjects(clQueue, 1, &textureMem, 0, 0, 0);
    if(checkError(error, "clEnqueueAcquireGLObjects"))
        return;

    // Work group y NDRange de renderKernel
    size_t workGroupSize[2] = { 16, 16 };
    size_t ndRangeSize[2];
    ndRangeSize[0]= roundUp(system->getWidth(), workGroupSize[0]);
    ndRangeSize[1]= roundUp(system->getHeight(), workGroupSize[1]);

    bool suspended= system->isSuspended();
    if(!suspended)
        system->suspend();

    // Ejecutamos el kernel para renderizar el sistema en una imagen
    cl_mem systemData= system->getOutputData();
    error  = clSetKernelArg(renderKernel, 0, sizeof(cl_mem), (void*)&systemData);
    error |= clSetKernelArg(renderKernel, 1, sizeof(cl_mem), (void*)&textureMem);
    error |= clSetKernelArg(renderKernel, 2, sizeof(cl_mem), (void*)&paletteMem);

    error |= clEnqueueNDRangeKernel(clQueue, renderKernel, 2, NULL, ndRangeSize, workGroupSize, 0, NULL, NULL);
    checkError(error, "FDMHeatWidget::updateSystemTexture: clEnqueueNDRangeKernel");

    if(!suspended)
        system->resume();

    error= clEnqueueReleaseGLObjects(clQueue, 1, &textureMem, 0, 0, 0);
    if (checkError(error, "clEnqueueReleaseGLObjects"))
        return;
}

void FDMHeatWidget::setSystem(FDMHeat* sys)
{
    // Paramos el render
    displayTimer.stop();
    // Actualizamos los parametros
    system= sys;
    lastIteration= 0;
    // Actualizamos el tamanio del render
    resizeGL(width(), height());
    // Reanudamos el render
    displayTimer.start();
}

//
// Full screen
//

void FDMHeatWidget::setFullScreen(bool fullScreen)
{
    if(fullScreen) {
        setWindowFlags(Qt::Window);
        showFullScreen();
    } else {
        setWindowFlags(Qt::Widget);
        showNormal();
    }
}

//
// Eventos de mouse y teclado para manejar el full scren
//

void FDMHeatWidget::keyPressEvent(QKeyEvent* event)
{
    switch(event->key()) {
    case Qt::Key_F:
        setFullScreen(!isFullScreen());
        break;
    case Qt::Key_Space:
        if(system->isSuspended())
            system->resume();
        else
            system->suspend();
        break;
    default:
        break;
    }
}

void FDMHeatWidget::mousePressEvent(QMouseEvent* event)
{
    drawing= true;
    drawingHot= event->button() == Qt::RightButton;
}

void FDMHeatWidget::mouseReleaseEvent(QMouseEvent* event)
{
    drawing= false;
}

void FDMHeatWidget::mouseMoveEvent(QMouseEvent* event)
{
    if(!drawing)
        return;

    QPoint widgetPos= event->pos();

    // Pasar de cordenadas del widget a coordenadas del sistema
    float widthRatio= (float)system->getWidth() / (width()-2*borderWidth);
    float heightRatio= (float)system->getHeight() / (height()-2*borderHeight);

    QPoint systemPos;
    systemPos.rx()= (widgetPos.x()-borderWidth) * widthRatio;
    systemPos.ry()= (widgetPos.y()-borderHeight) * heightRatio;

    system->drawHeatQuad(systemPos, 25, drawingHot);
}
