TEMPLATE = app

CONFIG += warn_on debug
QT += opengl

INCLUDEPATH += ./src/ ../common/ /usr/local/cuda/include/ /opt/AMDAPP/include

DESTDIR = bin
OBJECTS_DIR = obj
MOC_DIR = obj

LIBS += -lOpenCL
QMAKE_CXXFLAGS_RELEASE = -march=native -O3 -fPIC

SOURCES += \
	src/main.cpp \
	../common/clutils.cpp \
        src/glwidget.cpp \
        src/sphericalcoord.cpp \
	src/setupclgl.cpp

HEADERS += \
	../common/clutils.h \
        src/glwidget.h \
        src/sphericalcoord.h \
	src/setupclgl.h

OTHER_FILES += \
	src/vboproc.cl \
        src/partvshader.glsl \
        src/partfshader.glsl \
        src/passvshader.glsl \
        src/passfshader.glsl
