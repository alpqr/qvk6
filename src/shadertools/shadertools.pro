TARGET = QtShaderTools

load(qt_module)

DEFINES += QT_BUILD_SHADERTOOLS_LIB

HEADERS += \
    $$PWD/qtshadertoolsglobal.h \
    $$PWD/qshaderdescription.h \
    $$PWD/qshaderdescription_p.h \
    $$PWD/qbakedshader.h \
    $$PWD/qbakedshader_p.h \
    $$PWD/qshaderbaker.h \
    $$PWD/spirv/qspirvshader.h \
    $$PWD/spirv/qspirvcompiler.h \
    $$PWD/spirv/qshaderbatchablerewriter_p.h

SOURCES += \
    $$PWD/qshaderdescription.cpp \
    $$PWD/qbakedshader.cpp \
    $$PWD/qshaderbaker.cpp \
    $$PWD/spirv/qspirvshader.cpp \
    $$PWD/spirv/qspirvcompiler.cpp \
    $$PWD/spirv/qshaderbatchablerewriter.cpp

INCLUDEPATH += $$PWD $$PWD/../3rdparty/SPIRV-Cross $$PWD/../3rdparty/glslang

STATICLIBS = qtspirv-cross qtglslang-glslang qtglslang-spirv qtglslang-osdependent qtglslang-oglcompiler # qtglslang-hlsl
for(libname, STATICLIBS) {
    staticlib = $$[QT_HOST_LIBS]/$${QMAKE_PREFIX_STATICLIB}$$qtLibraryTarget($$libname).$${QMAKE_EXTENSION_STATICLIB}
    LIBS_PRIVATE += $$staticlib
    PRE_TARGETDEPS += $$staticlib
}
