TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    sharedresource.cpp

RESOURCES = sharedresource.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/sharedresource
INSTALLS += target
