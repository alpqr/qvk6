TARGET = QtRhi

QT += shadertools gui-private

DEFINES += QT_BUILD_RHI_LIB

HEADERS += \
    qtrhiglobal.h \
    qtrhiglobal_p.h \
    qrhi.h \
    qrhi_p.h \
    qrhiprofiler.h \
    qrhiprofiler_p.h

SOURCES += \
    qrhi.cpp \
    qrhiprofiler.cpp

qtConfig(opengl) {
    HEADERS += \
        qrhigles2.h \
        qrhigles2_p.h
    SOURCES += \
        qrhigles2.cpp
}

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

    LIBS += -ld3d11 -ldxgi -ldxguid -ld3dcompiler
}

mac {
    HEADERS += \
        qrhimetal.h \
        qrhimetal_p.h
    SOURCES += \
        qrhimetal.mm

    LIBS += -framework AppKit -framework Metal
}

include($$PWD/../3rdparty/VulkanMemoryAllocator.pri)

load(qt_module)
