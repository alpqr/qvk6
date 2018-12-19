TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    imguidemo.cpp

RESOURCES = \
    imguidemo.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/imguidemo
INSTALLS += target
