TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    main.cpp \
    ../shared/trianglerenderer.cpp \
    ../shared/texturedcuberenderer.cpp \
    ../shared/triangleoncuberenderer.cpp

HEADERS = \
    ../shared/trianglerenderer.h \
    ../shared/texturedcuberenderer.h \
    ../shared/triangleoncuberenderer.h

INCLUDEPATH += ../shared

RESOURCES = plainqwindow.qrc
