TARGET = QtRhi

load(qt_module)

QT += shadertools

!qtConfig(vulkan): error(This module requires Vulkan support)

DEFINES += QT_BUILD_RHI_LIB

HEADERS += \
    qtrhiglobal.h \
    qtrhiglobal_p.h \
    qrhi.h \
    qrhivulkan.h \
    qrhivulkan_p.h

SOURCES += \
    qrhi.cpp \
    qrhivulkan.cpp

include($$PWD/../3rdparty/VulkanMemoryAllocator.pri)
