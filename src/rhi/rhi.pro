TARGET = QtRhi

load(qt_module)

QT += shadertools

DEFINES += QT_BUILD_RHI_LIB

HEADERS += \
    qtrhiglobal.h \
    qtrhiglobal_p.h \
    qrhi.h \
    qrhivulkan.h \
    qrhivulkan_p.h \
    qrhigles2.h \
    qrhigles2_p.h

SOURCES += \
    qrhi.cpp \
    qrhivulkan.cpp \
    qrhigles2.cpp

include($$PWD/../3rdparty/VulkanMemoryAllocator.pri)
