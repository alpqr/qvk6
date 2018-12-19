TEMPLATE = app
CONFIG += console

QT += shadertools rhi

SOURCES = \
    main.cpp

RESOURCES = offscreen_vulkan.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/offscreen_vulkan
INSTALLS += target
