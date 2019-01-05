TEMPLATE = app
CONFIG += console

QT += shadertools rhi

SOURCES = \
    offscreen.cpp

RESOURCES = offscreen.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/offscreen
INSTALLS += target
