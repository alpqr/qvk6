TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    cubemap.cpp

RESOURCES = cubemap.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/cubemap
INSTALLS += target
