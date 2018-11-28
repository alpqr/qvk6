TEMPLATE = app
CONFIG += console

QT += shadertools rhi

SOURCES = \
    main.cpp

RESOURCES = offscreen_gles2.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/offscreen_gles2
INSTALLS += target
