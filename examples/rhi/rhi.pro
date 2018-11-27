TEMPLATE = subdirs

SUBDIRS += \
    hellominimalcrossgfxtriangle \
    compressedtexture_bc1 \
    plainqwindow_gles2

qtConfig(vulkan) {
    SUBDIRS += \
        vulkanwindow \
        plainqwindow_vulkan \
        offscreen_vulkan
}

win32 {
    SUBDIRS += \
        plainqwindow_d3d11 \
        offscreen_d3d11
}

mac {
    SUBDIRS += \
        plainqwindow_metal
}
