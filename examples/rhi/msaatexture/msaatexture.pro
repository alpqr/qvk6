TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    msaatexture.cpp

RESOURCES = msaatexture.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/msaatexture
INSTALLS += target
