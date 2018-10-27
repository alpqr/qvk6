TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    main.cpp \
    renderer.cpp \
    ../shared/trianglerenderer.cpp

HEADERS = \
    renderer.h \
    ../shared/trianglerenderer.h

INCLUDEPATH += ../shared

RESOURCES = vulkanwindow.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/vulkanwindow
INSTALLS += target
