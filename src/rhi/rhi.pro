TARGET = QtRhi

load(qt_module)

QT += shadertools

DEFINES += QT_BUILD_RHI_LIB

HEADERS += \
    qtrhiglobal.h \
    qtrhiglobal_p.h \
    qrhi.h \
    qrhigles2.h \
    qrhigles2_p.h

SOURCES += \
    qrhi.cpp \
    qrhigles2.cpp

qtConfig(vulkan) {
    HEADERS += \
        qrhivulkan.h \
        qrhivulkan_p.h
    SOURCES += \
        qrhivulkan.cpp
}

win32 {
    HEADERS += \
        qrhid3d11.h \
        qrhid3d11_p.h
    SOURCES += \
        qrhid3d11.cpp
}

include($$PWD/../3rdparty/VulkanMemoryAllocator.pri)
