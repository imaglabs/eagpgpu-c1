#include <QtGui>
#include <QtOpenGL>

#include <fdmheat.h>
#include <fdmheatwidget.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    FDMHeatWidget widget;
    widget.setWindowTitle("FDM Heat");
    widget.resize(800, 800);
    widget.show();
    // El widget inicializa OpenCL, asi comparte el contexto
    // con OpenGL. Esperamos que termine de configurar OpenCL.
    widget.waitCLConfig();

    FDMHeat heat(widget.getCLContext(), widget.getCLQueue(), widget.getCLDevice());
    if(!heat.loadFromImage("input.png")) {
        qDebug() << "Error al configurar FDMHeat.";
        return EXIT_FAILURE;
    }
    widget.setSystem(&heat);

    heat.start();
    
    app.setQuitOnLastWindowClosed(true);
    
    return app.exec();
}
