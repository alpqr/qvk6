TARGET = QtShaderTools

DEFINES += QT_BUILD_SHADERTOOLS_LIB

HEADERS += \
    $$PWD/qtshadertoolsglobal.h \
    $$PWD/qshaderdescription.h \
    $$PWD/qshaderdescription_p.h \
    $$PWD/qbakedshader.h \
    $$PWD/qbakedshader_p.h \
    $$PWD/qshaderbaker.h \
    $$PWD/qspirvshader.h \
    $$PWD/qspirvcompiler.h \
    $$PWD/qshaderbatchablerewriter_p.h

SOURCES += \
    $$PWD/qshaderdescription.cpp \
    $$PWD/qbakedshader.cpp \
    $$PWD/qshaderbaker.cpp \
    $$PWD/qspirvshader.cpp \
    $$PWD/qspirvcompiler.cpp \
    $$PWD/qshaderbatchablerewriter.cpp

INCLUDEPATH += $$PWD/../3rdparty/SPIRV-Cross $$PWD/../3rdparty/glslang

# Exceptions must be enabled since that is the only sane way to get errors reported from SPIRV-Cross.
# They will not propagate outside of this module though so should be safe enough.
CONFIG += exceptions

STATICLIBS = qtspirv-cross qtglslang-glslang qtglslang-spirv qtglslang-osdependent qtglslang-oglcompiler # qtglslang-hlsl
for(libname, STATICLIBS) {
    staticlib = $$[QT_HOST_LIBS]/$${QMAKE_PREFIX_STATICLIB}$$qtLibraryTarget($$libname).$${QMAKE_EXTENSION_STATICLIB}
    LIBS_PRIVATE += $$staticlib
    PRE_TARGETDEPS += $$staticlib
}

include($$PWD/doc/doc.pri)

load(qt_module)
