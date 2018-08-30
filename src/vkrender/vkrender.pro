TARGET = QtVkRender

load(qt_module)

!qtConfig(vulkan): error(This module requires Vulkan support)

DEFINES += QT_BUILD_VKR_LIB

HEADERS += \
    qtvkrglobal.h \
    qtvkrglobal_p.h \
    qvkrender.h

SOURCES += \
    qvkrender.cpp
