TEMPLATE = subdirs

SUBDIRS += \
    hellominimalcrossgfxtriangle \
    compressedtexture_bc1 \
    compressedtexture_bc1_subupload \
    texuploads \
    msaatexture \
    msaarenderbuffer \
    cubemap \
    multiwindow \
    multiwindow_threaded \
    imguidemo \
    triquadcube \
    offscreen_gles2

qtConfig(vulkan) {
    SUBDIRS += \
        vulkanwindow \
        offscreen_vulkan
}

win32 {
    SUBDIRS += \
        offscreen_d3d11
}

mac {
    SUBDIRS += \
        offscreen_metal
}
