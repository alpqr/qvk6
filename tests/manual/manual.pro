TEMPLATE = subdirs
SUBDIRS += \
    plainqwindow_gles2

qtConfig(vulkan) {
    SUBDIRS += \
        vulkanwindow \
        plainqwindow_vulkan
}
