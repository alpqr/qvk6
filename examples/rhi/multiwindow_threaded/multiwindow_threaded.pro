TEMPLATE = app

QT += shadertools rhi widgets

SOURCES = \
    multiwindow_threaded.cpp \
    window.cpp

HEADERS = \
    window.h

RESOURCES = multiwindow_threaded.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/multiwindow_threaded
INSTALLS += target
