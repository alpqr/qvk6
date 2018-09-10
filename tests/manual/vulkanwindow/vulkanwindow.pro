TEMPLATE = app

QT += shadertools vkrender

SOURCES = \
    main.cpp \
    renderer.cpp \
    ../shared/trianglerenderer.cpp \
    ../shared/texturedcuberenderer.cpp \
    ../shared/triangleoncuberenderer.cpp

HEADERS = \
    renderer.h \
    ../shared/trianglerenderer.h \
    ../shared/texturedcuberenderer.h \
    ../shared/triangleoncuberenderer.h

INCLUDEPATH += ../shared

RESOURCES = vulkanwindow.qrc
