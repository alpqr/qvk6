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
    sharedresource \
    offscreen

qtConfig(vulkan) {
    SUBDIRS += \
        vulkanwindow
}
