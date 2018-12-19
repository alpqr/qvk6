TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    main.cpp \
    ../shared/examplewindow.cpp \
    ../shared/trianglerenderer.cpp \
    ../shared/quadrenderer.cpp \
    ../shared/texturedcuberenderer.cpp \
    ../shared/triangleoncuberenderer.cpp

HEADERS = \
    ../shared/examplewindow.h \
    ../shared/trianglerenderer.h \
    ../shared/quadrenderer.h \
    ../shared/texturedcuberenderer.h \
    ../shared/triangleoncuberenderer.h

INCLUDEPATH += ../shared

RESOURCES = plainqwindow_metal.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/plainqwindow_metal
INSTALLS += target
