TEMPLATE = subdirs

SUBDIRS += \
    hellominimalcrossgfxtriangle \
    plainqwindow_gles2

qtConfig(vulkan) {
    SUBDIRS += \
        vulkanwindow \
        plainqwindow_vulkan
}

win32 {
    SUBDIRS += \
        plainqwindow_d3d11
}

mac {
    SUBDIRS += \
        plainqwindow_metal
}
