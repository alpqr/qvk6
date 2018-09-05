TEMPLATE = app

QT += shadertools vkrender

SOURCES = \
    main.cpp \
    renderer.cpp \
    ../shared/trianglerenderer.cpp

HEADERS = \
    renderer.h \
    ../shared/trianglerenderer.h

INCLUDEPATH += ../shared

RESOURCES = vulkanwindow.qrc
