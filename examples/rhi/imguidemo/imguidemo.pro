TEMPLATE = app

QT += shadertools rhi

SOURCES = \
    imguidemo.cpp \
    qrhiimgui.cpp \
    imgui/imgui.cpp \
    imgui/imgui_draw.cpp \
    imgui/imgui_widgets.cpp \
    imgui/imgui_demo.cpp

HEADERS = \
    qrhiimgui.h \
    qrhiimgui_p.h

INCLUDEPATH += imgui

RESOURCES = \
    imguidemo.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/rhi/imguidemo
INSTALLS += target
