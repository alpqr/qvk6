TEMPLATE = subdirs

SUBDIRS += \
    glslang \
    SPIRV-Cross \
    imgui \
    shadertools \
    rhi

shadertools.depends = glslang SPIRV-Cross
rhi.depends = shadertools imgui
