TEMPLATE = app
CONFIG += console

QT += shadertools rhi

SOURCES = \
    main.cpp

RESOURCES = offscreen_metal.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/offscreen_metal
INSTALLS += target
