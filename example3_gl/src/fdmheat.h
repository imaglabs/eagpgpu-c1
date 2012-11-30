#ifndef FDMHEAT_H
#define FDMHEAT_H

#include <QtGui>

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS

#include "clutils.h"

class FDMHeat : public QThread
{
Q_OBJECT

public:
    FDMHeat(cl_context context, cl_command_queue queue, cl_device_id device);

    bool loadFromImage(QString path);

    // void start() se hereda de QThread, y llama a run()
    void stop() { if(isRunning()) finish.release(); }
    // Suspender y resumir el computo
    void suspend();
    void resume();
    bool isSuspended() { return suspended; }

    // Puede llamarse en cualquier momento
    int getIteration() { return iteration; }

    void drawHeatQuad(QPoint center, int size, bool hot);

    // Puede llamarse solo cuando el sistema esta suspendido
    cl_mem getOutputData() { return dataOutput; }

    int getWidth() { return width; }
    int getHeight() { return height; }

protected:
    void run();

private:
    QAtomicInt iteration;
    bool firstRun;
    QSemaphore finish;
    bool suspended;

    int width;
    int height;
    int bytes;

    // Buffers en GPU y CPU
    float* hData;

    cl_mem dData1;
    cl_mem dData2;
    QMutex dataLock;
    cl_mem dataInput;  // Referencias para el ping pong buffer, siempre
    cl_mem dataOutput; // son iguales a dData1/dData2 o el inverso

    cl_context clContext;
    cl_command_queue clQueue;
    cl_device_id clDevice;
    
    cl_kernel kernel;
    cl_kernel brushKernel;
};

#endif // FDMHEAT_H
