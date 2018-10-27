TEMPLATE = app

QT += shadertools

SOURCES = \
    main.cpp \
    renderwindow.cpp

HEADERS = \
    renderwindow.h

qtConfig(vulkan) {
    SOURCES += trianglerenderer.cpp
    HEADERS += trianglerenderer.h
}

RESOURCES = tri.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/shadertools/oneshaderthreecontexts_runtime
INSTALLS += target
