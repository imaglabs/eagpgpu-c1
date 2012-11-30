#include <QApplication>
#include <glwidget.h>

int main(int argc, char** argv) 
{
    QApplication app(argc, argv);
	
    GLWidget widget(NULL, 32768);
    widget.setWindowTitle("OpenGL/OpenCL Example");
    widget.show();	

    QTimer timer;
    timer.setInterval(30);
    timer.start();
    app.connect(&timer, SIGNAL(timeout()), &widget, SLOT(updateGL()));
	
    return app.exec();
}
