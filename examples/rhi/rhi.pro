TEMPLATE = subdirs

SUBDIRS += \
    hellominimalcrossgfxtriangle \
    compressedtexture_bc1 \
    compressedtexture_bc1_subupload \
    texuploads \
    msaatexture \
    msaarenderbuffer \
    cubemap \
    plainqwindow_gles2 \
    offscreen_gles2

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
        plainqwindow_metal \
        offscreen_metal
}
