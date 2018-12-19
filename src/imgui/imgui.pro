TARGET = qtrhi-imgui

CONFIG += \
    static \
    hide_symbols \
    warn_off

load(qt_helper_lib)

IMGUI_PATH=$$PWD/../3rdparty/imgui

SOURCES += \
    $$IMGUI_PATH/imgui.cpp \
    $$IMGUI_PATH/imgui_draw.cpp \
    $$IMGUI_PATH/imgui_widgets.cpp \
    $$IMGUI_PATH/imgui_demo.cpp
