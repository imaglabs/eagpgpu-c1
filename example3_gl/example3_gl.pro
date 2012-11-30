TEMPLATE = app

CONFIG += warn_on
QT += opengl

DESTDIR = bin
OBJECTS_DIR = obj
MOC_DIR = obj

INCLUDEPATH += ./src/ ../common/ /usr/local/cuda/include /opt/AMDAPP/include

LIBS += -lOpenCL -lGLU

QMAKE_CXXFLAGS_RELEASE = -march=native -O3 -fPIC

SOURCES += \
    src/main.cpp \
    ../common/clutils.cpp \
    src/fdmheat.cpp \
    src/fdmheatwidget.cpp \
    src/setupclgl.cpp

HEADERS += \
    ../common/clutils.h \
    src/fdmheat.h \
    src/fdmheatwidget.h \
    src/setupclgl.h

OTHER_FILES += \
    src/fdmHeat.cl \
    src/systemToImage.cl \
    src/heatBrush.cl
