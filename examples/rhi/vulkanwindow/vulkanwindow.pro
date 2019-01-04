TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    main.cpp \
    renderer.cpp \
    trianglerenderer.cpp

HEADERS = \
    renderer.h \
    trianglerenderer.h

RESOURCES = vulkanwindow.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/vulkanwindow
INSTALLS += target
