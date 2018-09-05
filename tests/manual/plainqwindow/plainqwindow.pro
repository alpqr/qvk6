TEMPLATE = app

QT += shadertools vkrender

SOURCES = \
    main.cpp \
    ../shared/trianglerenderer.cpp

HEADERS = \
    ../shared/trianglerenderer.h

INCLUDEPATH += ../shared

RESOURCES = plainqwindow.qrc
