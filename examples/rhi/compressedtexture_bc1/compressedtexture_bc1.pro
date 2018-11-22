TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    compressedtexture_bc1.cpp

RESOURCES = compressedtexture_bc1.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/compressedtexture_bc1
INSTALLS += target
