TEMPLATE = app

CONFIG += warn_on

DESTDIR = bin
OBJECTS_DIR = obj
MOC_DIR = obj

INCLUDEPATH += ./src/ ../common/ /usr/local/cuda/include /opt/AMDAPP/include

LIBS += -lOpenCL

QMAKE_CXXFLAGS_RELEASE = -march=native -O3 -fPIC

SOURCES += \
    src/main.cpp \
    ../common/clutils.cpp

HEADERS += \
    ../common/clutils.h

OTHER_FILES += \
    src/fdmHeat.cl
