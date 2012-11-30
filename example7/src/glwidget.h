#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QGLWidget>
#include <QtOpenGL>
#include <QMatrix4x4>
#include <sphericalcoord.h>
#include <particle.h>

#include <CL/cl.h>

class GLWidget : public QGLWidget
{
    Q_OBJECT

public:
    GLWidget(QWidget *parent = 0, int numberOfParticles = 32768);
    ~GLWidget();

    QSize minimumSizeHint() const { return QSize(400, 400); }
    QSize sizeHint() const { return QSize(800, 800); }

protected:
    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);

    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *);
    void setFullScreen();
    
private:

    void initMembers();
    void initializeCL();
    
    void updateMatrices();
    
    void runKernel();
    
    void drawCube();
    void drawAxes();
    void drawPoints();

    void initVBOs();
    void initParticles();
    void initShaders();

    void loadShaders(QGLShaderProgram* program, QString vShaderFileName, QString fShaderFileName);

    QGLShaderProgram* particleShaderProgram;
    QGLShaderProgram* passShaderProgram;

    int particleVertexLocation;
    int particleColorLocation;
    int particleMatrixLocation;
    int particleSamplerPSLocation;
    int particleSamplerVelLocation;
    
    int axesVertexLocation;
    int axesColorLocation;
    int axesMatrixLocation;
    int axesAlphaLocation;

    int cubeVertexLocation;
    int cubeColorLocation;
    int cubeMatrixLocation;
    int cubeAlphaLocation;

    QMatrix4x4 mMatrix;
    QMatrix4x4 vMatrix;
    QMatrix4x4 pMatrix;

    GLint psTexture;
    GLint velTexture;
    
    Particle* particles;
    int vertexNumber;
    int vboSize;
    float timestep;

    QGLBuffer* particlesVBO;

    QGLBuffer* cubePositionsVbo;
    QGLBuffer* cubeColorsVbo;

    QGLBuffer* axesPositionsVbo;
    QGLBuffer* axesColorsVbo;

    QGLBuffer* cubeLinesPositionsVbo;

    QRect viewport;

    QImage pointSpriteImage;
    QImage paletteImage;

    QPoint lastPos;
    bool fullScreen;
    
    size_t localWorkSize;
    size_t globalWorkSize;
    
    const QVector3D cubeLimits;
    
    // camera vars
    SphericalCoord cameraPosition;
    QVector3D cameraLookAt;
    QVector3D cameraUp;
    
    // Variables OpenCL
    cl_context clContext;
    cl_command_queue clQueue;
    cl_device_id clDevice;
    
    cl_kernel clKernel;
    
    cl_mem clvbo;

};

#endif
