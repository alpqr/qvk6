TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    compressedtexture_bc1_subupload.cpp

RESOURCES = compressedtexture_bc1_subupload.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/compressedtexture_bc1_subupload
INSTALLS += target
