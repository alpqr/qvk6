TEMPLATE = app

QT += shadertools-private

SOURCES = \
    main.cpp \
    renderwindow.cpp

HEADERS = \
    renderwindow.h

qtConfig(vulkan) {
    SOURCES += ../shared/vulkantrianglerenderer.cpp
    HEADERS += ../shared/vulkantrianglerenderer.h
}

INCLUDEPATH += ../shared

RESOURCES = tri.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/shadertools/oneshaderthreecontexts_runtime
INSTALLS += target
