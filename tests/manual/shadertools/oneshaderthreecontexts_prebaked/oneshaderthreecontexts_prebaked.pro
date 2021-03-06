TEMPLATE = app

QT += shadertools

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

target.path = $$[QT_INSTALL_EXAMPLES]/shadertools/oneshaderthreecontexts_prebaked
INSTALLS += target
