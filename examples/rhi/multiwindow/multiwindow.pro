TEMPLATE = app

QT += shadertools rhi widgets

SOURCES = \
    multiwindow.cpp

RESOURCES = multiwindow.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/multiwindow
INSTALLS += target
