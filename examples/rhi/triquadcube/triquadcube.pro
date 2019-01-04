TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    triquadcube.cpp \
    trianglerenderer.cpp \
    quadrenderer.cpp \
    texturedcuberenderer.cpp \
    triangleoncuberenderer.cpp

HEADERS = \
    trianglerenderer.h \
    quadrenderer.h \
    texturedcuberenderer.h \
    triangleoncuberenderer.h

RESOURCES = triquadcube.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/triquadcube
INSTALLS += target
