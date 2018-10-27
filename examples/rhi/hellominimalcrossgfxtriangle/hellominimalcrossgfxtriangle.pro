TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    hellominimalcrossgfxtriangle.cpp

RESOURCES = hellominimalcrossgfxtriangle.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/hellominimalcrossgfxtriangle
INSTALLS += target
