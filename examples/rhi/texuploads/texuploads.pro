TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    texuploads.cpp

RESOURCES = texuploads.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/texuploads
INSTALLS += target
