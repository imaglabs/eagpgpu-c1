#include <math.h>
#include <glwidget.h>
#include <GL/glext.h>

#include "clutils.h"
#include <CL/cl_gl.h>
#include <GL/glx.h>

#include <setupclgl.h>

GLWidget::GLWidget(QWidget *parent, int numberOfParticles)
    : QGLWidget(parent), cubeLimits(2.0f, 2.0f, 2.0f)
{
    initMembers();
    vertexNumber = numberOfParticles;
}

void GLWidget::initMembers()
{
    particleShaderProgram = NULL;
    passShaderProgram = NULL;
    particlesVBO = NULL;
    cubePositionsVbo = NULL;
    cubeColorsVbo = NULL;
    axesPositionsVbo = NULL;
    axesColorsVbo = NULL;
    cubeLinesPositionsVbo = NULL;
    particles = NULL;

    pointSpriteImage = QImage("./particle.png");
    paletteImage = QImage("./palette.png");

    vboSize = 0;
    timestep = 0.005f;
    
    fullScreen = false;
    viewport.setX(0);
    viewport.setY(0);
    
    cameraPosition = SphericalCoord(4.0f, 45.0f, 45.0f);
    cameraLookAt = QVector3D(0.0f, 0.0f, 0.0f);
    cameraUp = QVector3D(0.0f, 1.0f, 0.0f);

    mMatrix.setToIdentity();
    vMatrix.setToIdentity();
    pMatrix.setToIdentity(); 
    
    setCursor(Qt::OpenHandCursor);
    
}

GLWidget::~GLWidget()
{
    // Elimino VBOs
    delete particlesVBO;
    delete cubePositionsVbo;
    delete cubeColorsVbo;
    delete axesPositionsVbo;
    delete axesColorsVbo;
    delete cubeLinesPositionsVbo;

    // Elimino shaders
    delete particleShaderProgram;
    delete passShaderProgram;

    // Elimino particulas
    delete [] particles;
    
    // Libero las variables OpenCL
    clReleaseCommandQueue(clQueue);
    clReleaseKernel(clKernel);
    clReleaseMemObject(clvbo);
    clReleaseContext(clContext);
    
}

void GLWidget::initializeGL()
{
    qDebug() << "Initializing OpenGL";

    qglClearColor(QColor(20, 20, 20));

    initShaders();
    initParticles();
    initVBOs();

    psTexture = bindTexture(pointSpriteImage, GL_TEXTURE_2D, GL_RGBA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    velTexture = bindTexture(paletteImage, GL_TEXTURE_2D, GL_RGBA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    qDebug() << "OpenGL initialized.";
    
    initializeCL();
    
}

void GLWidget::initializeCL() 
{
    qDebug() << "Initializing OpenCL";
    if (!setupOpenCLGL(clContext, clQueue, clDevice)) {
	qDebug() << "OpenCL initialization error";
	return;
    }    
    
    cl_int error;

    loadKernel(clContext, &clKernel, clDevice, "../src/vboproc.cl", "vboproc");
    
    // Creo OpenCL buffer a partir del OpenGL buffer
    qDebug() << "Creando OpenCL buffer.";
    clvbo = clCreateFromGLBuffer(clContext, CL_MEM_READ_WRITE, particlesVBO->bufferId(), &error);
    if (checkError(error, "clCreateFromGLBuffer")) {
	qDebug() << "OpenCL initialization error";
	return;
    }
    
    // Setean los parametros del kernel, y luego se encola su ejecucion
    qDebug() << "Seteo los parametros del kernel.";
    error  = clSetKernelArg(clKernel, 0, sizeof(cl_mem), (void*)&clvbo);
    error |= clSetKernelArg(clKernel, 1, sizeof(cl_int), (void*)&vertexNumber);
    const float cubeLims[]= {cubeLimits.x(), cubeLimits.y(), cubeLimits.z()};
    error |= clSetKernelArg(clKernel, 2, sizeof(cl_float3), (void*)&cubeLims);
    error |= clSetKernelArg(clKernel, 3, sizeof(cl_float), (void*)&timestep);
    if(checkError(error, "clSetKernelArg")) {
	qDebug() << "OpenCL initialization error";
        return;
    }
    
    // Una vez creado el kernel, decremento la referencia al programa creado
    qDebug() << "OpenCL initialized successfully";
    
}


void GLWidget::initShaders()
{

    passShaderProgram = new QGLShaderProgram;
    particleShaderProgram = new QGLShaderProgram;

    // initialize shaders
    qDebug() << "Loading Pass Shader";
    loadShaders(passShaderProgram, "../src/passvshader.glsl", "../src/passfshader.glsl");
    qDebug() << "Loading Particle Shader";
    loadShaders(particleShaderProgram, "../src/partvshader.glsl", "../src/partfshader.glsl");

    passShaderProgram->bind();
    axesVertexLocation = passShaderProgram->attributeLocation("vertex");
    axesColorLocation = passShaderProgram->attributeLocation("color");
    axesMatrixLocation = passShaderProgram->uniformLocation("matrix");
    axesAlphaLocation = passShaderProgram->uniformLocation("alpha");

    cubeVertexLocation = passShaderProgram->attributeLocation("vertex");
    cubeColorLocation = passShaderProgram->attributeLocation("color");
    cubeMatrixLocation = passShaderProgram->uniformLocation("matrix");
    cubeAlphaLocation = passShaderProgram->uniformLocation("alpha");

    particleShaderProgram->bind();
    particleVertexLocation = particleShaderProgram->attributeLocation("vertex");
    particleColorLocation = particleShaderProgram->attributeLocation("color");
    particleMatrixLocation = particleShaderProgram->uniformLocation("matrix");
    particleSamplerPSLocation = particleShaderProgram->uniformLocation("pointSpriteTex");
    particleSamplerVelLocation = particleShaderProgram->uniformLocation("velTex");
    
}

void GLWidget::initParticles()
{
    
    qDebug() << "Number of particles: " << vertexNumber;    
    particles = new Particle[vertexNumber];
    
    memset(particles, sizeof(Particle)*vertexNumber, 0); // for floats, 0 is equal to the float 0.0

    for(int i=0; i < vertexNumber; ++i) {
        // position data
        particles[i].px = ((double(qrand())/RAND_MAX)-0.5f)*cubeLimits.x()*2.0f;
        particles[i].py = ((double(qrand())/RAND_MAX)-0.5f)*cubeLimits.y()*2.0f;
        particles[i].pz = ((double(qrand())/RAND_MAX)-0.5f)*cubeLimits.z()*2.0f;
	// mass data
	particles[i].m = ((float(qrand())/RAND_MAX))*20.0f;
        // velocity data to 0	
    }

}

void GLWidget::drawAxes()
{
    glEnable(GL_DEPTH_TEST);

    passShaderProgram->bind();

    passShaderProgram->enableAttributeArray(axesVertexLocation);
    passShaderProgram->enableAttributeArray(axesColorLocation);

    axesPositionsVbo->bind();
    passShaderProgram->setAttributeBuffer(axesVertexLocation, GL_FLOAT, 0, 3);
    axesColorsVbo->bind();
    passShaderProgram->setAttributeBuffer(axesColorLocation, GL_FLOAT, 0, 3);

    passShaderProgram->setUniformValue(axesMatrixLocation, pMatrix * vMatrix);
    passShaderProgram->setUniformValue(axesAlphaLocation, 1.0f);

    glDrawArrays(GL_LINES, 0, 6);

    axesColorsVbo->release();
    axesPositionsVbo->release();

    passShaderProgram->disableAttributeArray(axesVertexLocation);
    passShaderProgram->disableAttributeArray(axesColorLocation);

    passShaderProgram->release();

}

void GLWidget::runKernel() 
{
    cl_int error;
    
    // block until all gl functions are completed
    glFinish();
    // Le doy a OpenCL el vbo que estaba usando OpenGL para renderizar
    error = clEnqueueAcquireGLObjects(clQueue, 1, &clvbo, 0, 0, 0);
    if (checkError(error, "clEnqueueAcquireGLObjects")) {
	return;
    }

    localWorkSize = 1024;
    globalWorkSize = roundUp(vertexNumber, localWorkSize);
    
    error = clEnqueueNDRangeKernel(clQueue, clKernel, 1, NULL, &globalWorkSize, &localWorkSize, 0, 0, 0);
    if (checkError(error, "clEnqueueNDRangeKernel")) {
	return;
    }

    // unmap buffer object
    error = clEnqueueReleaseGLObjects(clQueue, 1, &clvbo, 0, 0, 0);
    if (checkError(error, "clEnqueueReleaseGLObjects")) {
	return;
    }

    clFinish(clQueue);

}

void GLWidget::drawPoints()
{
    runKernel();
    
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    
    particleShaderProgram->bind();

    particleShaderProgram->enableAttributeArray(particleVertexLocation);
    particleShaderProgram->enableAttributeArray(particleColorLocation);

    particlesVBO->bind();
    int tupleSize = 4;
    particleShaderProgram->setAttributeBuffer(particleVertexLocation, GL_FLOAT, 0, tupleSize, sizeof(Particle));
    particleShaderProgram->setAttributeBuffer(particleColorLocation, GL_FLOAT, tupleSize*sizeof(float), tupleSize, sizeof(Particle));

    particleShaderProgram->setUniformValue(particleMatrixLocation, pMatrix * vMatrix);

    particleShaderProgram->setUniformValue(particleSamplerPSLocation, 0);
    particleShaderProgram->setUniformValue(particleSamplerVelLocation, 1);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, psTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, velTexture);

    glDrawArrays(GL_POINTS, 0, vertexNumber);

    particleShaderProgram->disableAttributeArray(particleVertexLocation);
    particleShaderProgram->disableAttributeArray(particleColorLocation);
    
    particlesVBO->release();

    particleShaderProgram->release();

    glDisable(GL_POINT_SPRITE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    
}

void GLWidget::updateMatrices()
{

    vMatrix.setToIdentity();
    vMatrix.lookAt(cameraPosition.pos(), cameraLookAt, cameraUp);
    pMatrix.setToIdentity();
    pMatrix.perspective(45.0, float(viewport.width())/viewport.height(), 0.1, 500.0);
}

void GLWidget::drawCube()
{

    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);

    passShaderProgram->bind();

    passShaderProgram->enableAttributeArray(cubeVertexLocation);
    passShaderProgram->enableAttributeArray(cubeColorLocation);

    cubePositionsVbo->bind();
    passShaderProgram->setAttributeBuffer(cubeVertexLocation, GL_FLOAT, 0, 3);
    cubeColorsVbo->bind();
    passShaderProgram->setAttributeBuffer(cubeColorLocation, GL_FLOAT, 0, 3);

    passShaderProgram->setUniformValue(cubeMatrixLocation, pMatrix * vMatrix);
    passShaderProgram->setUniformValue(cubeAlphaLocation, 0.05f);

    glDrawArrays(GL_QUADS, 0, 24);

    passShaderProgram->setUniformValue(cubeAlphaLocation, 1.0f);
    cubeLinesPositionsVbo->bind();
    passShaderProgram->setAttributeBuffer(cubeVertexLocation, GL_FLOAT, 0, 3);

    glDrawArrays(GL_LINES, 0, 48);

    cubeLinesPositionsVbo->release();
    cubePositionsVbo->release();
    cubeColorsVbo->release();

    passShaderProgram->disableAttributeArray(cubeVertexLocation);
    passShaderProgram->disableAttributeArray(cubeColorLocation);

    passShaderProgram->release();
}

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(viewport.x(), viewport.y(), viewport.width(), viewport.height());

    updateMatrices();

    drawAxes();
    drawCube();
    drawPoints();

}

void GLWidget::resizeGL(int width, int height)
{
    viewport.setWidth(width);
    viewport.setHeight(height);    
}

void GLWidget::loadShaders(QGLShaderProgram* program, QString vShaderFileName, QString fShaderFileName)
{
    QGLShader* vertexShader = NULL;
    QGLShader* fragmentShader = NULL;

    QFileInfo vsh(vShaderFileName);
    if(vsh.exists()) {
        vertexShader = new QGLShader(QGLShader::Vertex);
        if(vertexShader->compileSourceFile(vShaderFileName)) {
            program->addShader(vertexShader);
            qDebug() << "Vertex shader compiled successfully.";
        }
        else {
            qWarning() << "Vertex Shader Error" << vertexShader->log();
        }
    }
    else {
        qWarning() << "Vertex Shader source file " << fShaderFileName << " not found.";
    }

    QFileInfo fsh(fShaderFileName);
    if(fsh.exists()) {
        fragmentShader = new QGLShader(QGLShader::Fragment);
        if(fragmentShader->compileSourceFile(fShaderFileName)) {
            program->addShader(fragmentShader);
            qDebug() << "Fragment shader compiled successfully.";
        }
        else {
            qWarning() << "Fragment Shader Error" << fragmentShader->log();
        }
    }
    else {
        qWarning() << "Fragment Shader source file " << fShaderFileName << " not found.";
    }

    if(!program->link()){
        qWarning() << "Shader Program Linker Error" << program->log();
    }

}

void GLWidget::initVBOs()
{
    particlesVBO = new QGLBuffer(QGLBuffer::VertexBuffer);
    particlesVBO->setUsagePattern(QGLBuffer::DynamicDraw);
    if(!particlesVBO->create()) {
        qDebug() << "Error: particlesVBO creation";
    }

    particlesVBO->bind();
    vboSize = vertexNumber*sizeof(Particle);
    particlesVBO->allocate(particles, vboSize);
    // en este punto se podria liberar la memoria de particles de CPU (ya que los datos ya estan subidos)
    particlesVBO->release();
    
    float x = cubeLimits.x();
    float y = cubeLimits.y();
    float z = cubeLimits.z();

    // filled cube
    static GLfloat const cubeVertexs[] = {
        -x, y, z,
        x, y, z,
        x, -y, z,
        -x, -y, z,

        x, y, z,
        x, y, -z,
        x, -y, -z,
        x, -y, z,

        x, -y, -z,
        x, y, -z,
        -x, y, -z,
        -x, -y, -z,

        -x, y, z,
        -x, y, -z,
        -x, -y, -z,
        -x, -y, z,

        -x, -y, z,
        x, -y, z,
        x, -y, -z,
        -x, -y, -z,

        x, y, z,
        x, y, -z,
        -x, y, -z,
        -x, y, z
    };

    cubePositionsVbo = new QGLBuffer(QGLBuffer::VertexBuffer);
    if(!cubePositionsVbo->create()) {
        qDebug() << "Error: cubePositionsVbo creation";
    }
    cubePositionsVbo->bind();
    cubePositionsVbo->allocate(cubeVertexs, sizeof(cubeVertexs));
    cubePositionsVbo->release();

    static GLfloat const cubeColors[] = {
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f
    };

    cubeColorsVbo = new QGLBuffer(QGLBuffer::VertexBuffer);
    if(!cubeColorsVbo->create()) {
        qDebug() << "Error: cubeColorsVbo creation";
    }
    cubeColorsVbo->bind();
    cubeColorsVbo->allocate(cubeColors, sizeof(cubeColors));
    cubeColorsVbo->release();

    // axes
    static GLfloat const axesVertices[] = {
        -x, 0.0f, 0.0f,
        x, 0.0f, 0.0f,
        0.0f, -y, 0.0f,
        0.0f, y, 0.0f,
        0.0f, 0.0f, -z,
        0.0f, 0.0f, z
    };

    axesPositionsVbo = new QGLBuffer(QGLBuffer::VertexBuffer);
    if(!axesPositionsVbo->create()) {
        qDebug() << "Error: axesPositionsVbo creation";
    }
    axesPositionsVbo->bind();
    axesPositionsVbo->allocate(axesVertices, sizeof(axesVertices));
    axesPositionsVbo->release();

    static GLfloat const axesColors[] = {
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };

    axesColorsVbo = new QGLBuffer(QGLBuffer::VertexBuffer);
    if(!axesColorsVbo->create()) {
        qDebug() << "Error: axesColorsVbo creation";
    }
    axesColorsVbo->bind();
    axesColorsVbo->allocate(axesColors, sizeof(axesColors));
    axesColorsVbo->release();

    // cube lines
    static GLfloat const cubeVertexsLines[] = {
        -x, y, z,
        x, y, z,
        x, y, z,
        x, -y, z,
        x, -y, z,
        -x, -y, z,
        -x, -y, z,
        -x, y, z,

        x, y, z,
        x, y, -z,
        x, y, -z,
        x, -y, -z,
        x, -y, -z,
        x, -y, z,
        x, -y, z,
        x, y, z,

        x, -y, -z,
        x, -y, -z,
        x, y, -z,
        -x, y, -z,
        -x, y, -z,
        -x, -y, -z,
        -x, -y, -z,
        x, -y, -z,

        -x, y, z,
        -x, y, -z,
        -x, y, -z,
        -x, -y, -z,
        -x, -y, -z,
        -x, -y, z,
        -x, -y, z,
        -x, y, z,

        -x, -y, z,
        x, -y, z,
        x, -y, z,
        x, -y, -z,
        x, -y, -z,
        -x, -y, -z,
        -x, -y, -z,
        -x, -y, z,

        x, y, z,
        x, y, -z,
        x, y, -z,
        -x, y, -z,
        -x, y, -z,
        -x, y, z,
        -x, y, z,
        x, y, z
    };

    cubeLinesPositionsVbo = new QGLBuffer(QGLBuffer::VertexBuffer);
    if(!cubeLinesPositionsVbo->create()) {
        qDebug() << "Error: cubeLinesPositionsVbo creation";
    }
    cubeLinesPositionsVbo->bind();
    cubeLinesPositionsVbo->allocate(cubeVertexsLines, sizeof(cubeVertexsLines));
    cubeLinesPositionsVbo->release();

}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
    setCursor(Qt::ClosedHandCursor);
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - lastPos.x();
    int dy = event->y() - lastPos.y();

    if(event->buttons() & Qt::LeftButton) {
        cameraPosition.addPhi(-dx/2.0f);
        cameraPosition.addTheta(-dy/2.0f);
    }

    lastPos = event->pos();
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event) 
{
    setCursor(Qt::OpenHandCursor);
    event->accept();
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    int numDegrees = event->delta() / 8;
    int numSteps = numDegrees / 15;

    float wheelDelta = float(numSteps) * 0.1f;
    cameraPosition.addR(-wheelDelta);

    if (cameraPosition.r() < 0.2f)
        cameraPosition.setR(0.2f);
    else if (cameraPosition.r() > 10.0f)
        cameraPosition.setR(10.0f);

    event->accept();

}

void GLWidget::mouseDoubleClickEvent(QMouseEvent *event) 
{
    fullScreen = !fullScreen;
    setFullScreen();
    event->accept();
}
    

void GLWidget::setFullScreen()
{
    if(fullScreen) {
        setWindowFlags(Qt::Window);
        showFullScreen();
    } else {
        setWindowFlags(Qt::Widget);
        showNormal();
    }
}

