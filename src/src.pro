TEMPLATE = subdirs

SUBDIRS += \
    glslang \
    SPIRV-Cross \
    shadertools \
    rhi

shadertools.depends = glslang SPIRV-Cross
rhi.depends = shadertools
