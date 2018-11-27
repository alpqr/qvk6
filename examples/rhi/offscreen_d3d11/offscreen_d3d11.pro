TEMPLATE = app
CONFIG += console

QT += shadertools rhi

SOURCES = \
    main.cpp

RESOURCES = offscreen_d3d11.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/offscreen_d3d11
INSTALLS += target
