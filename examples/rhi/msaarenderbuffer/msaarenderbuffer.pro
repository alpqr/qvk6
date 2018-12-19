TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    msaarenderbuffer.cpp

RESOURCES = msaarenderbuffer.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/msaarenderbuffer
INSTALLS += target
