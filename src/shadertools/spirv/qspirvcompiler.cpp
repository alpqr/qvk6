/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Shader Tools module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qspirvcompiler.h"
#include "qshaderbatchablerewriter_p.h"
#include <QFile>
#include <QFileInfo>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

QT_BEGIN_NAMESPACE

static const TBuiltInResource resourceLimits = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }
};

struct QSpirvCompilerPrivate
{
    bool readFile(const QString &fn);
    bool compile();

    QString sourceFileName;
    QByteArray source;
    QByteArray batchableSource;
    EShLanguage stage = EShLangVertex;
    QSpirvCompiler::Flags flags = 0;
    QByteArray spirv;
    QString log;
};

bool QSpirvCompilerPrivate::readFile(const QString &fn)
{
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("QSpirvCompiler: Failed to open %s", qPrintable(fn));
        return false;
    }
    source = f.readAll();
    batchableSource.clear();
    sourceFileName = fn;
    f.close();
    return true;
}

class Includer : public glslang::TShader::Includer
{
public:
    IncludeResult *includeLocal(const char *headerName,
                                const char *includerName,
                                size_t inclusionDepth) override
    {
        Q_UNUSED(inclusionDepth);
        return readFile(headerName, includerName);
    }

    IncludeResult *includeSystem(const char *headerName,
                                 const char *includerName,
                                 size_t inclusionDepth) override
    {
        Q_UNUSED(inclusionDepth);
        return readFile(headerName, includerName);
    }

    void releaseInclude(IncludeResult *result) override
    {
        if (result) {
            delete static_cast<QByteArray *>(result->userData);
            delete result;
        }
    }

private:
    IncludeResult *readFile(const char *headerName, const char *includerName);
};

glslang::TShader::Includer::IncludeResult *Includer::readFile(const char *headerName, const char *includerName)
{
    // Just treat the included name as relative to the includer:
    //   Take the path from the includer, append the included name, remove redundancies.
    // This should work also for qrc (source filenames with qrc:/ or :/ prefix).

    QString includer = QString::fromUtf8(includerName);
    if (includer.isEmpty())
        includer = QLatin1String(".");
    QString included = QFileInfo(includer).canonicalPath() + QLatin1Char('/') + QString::fromUtf8(headerName);
    included = QFileInfo(included).canonicalFilePath();
    if (included.isEmpty()) {
        qWarning("QSpirvCompiler: Failed to find include file %s", headerName);
        return nullptr;
    }
    QFile f(included);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("QSpirvCompiler: Failed to read include file %s", qPrintable(included));
        return nullptr;
    }

    QByteArray *data = new QByteArray;
    *data = f.readAll();
    return new IncludeResult(included.toStdString(), data->constData(), data->size(), data);
}

class GlobalInit
{
public:
    GlobalInit() { glslang::InitializeProcess(); }
    ~GlobalInit() { glslang::FinalizeProcess(); }
};

bool QSpirvCompilerPrivate::compile()
{
    log.clear();

    const bool useBatchable = (stage == EShLangVertex && flags.testFlag(QSpirvCompiler::RewriteToMakeBatchableForSG));
    const QByteArray *actualSource = useBatchable ? &batchableSource : &source;
    if (actualSource->isEmpty())
        return false;

    static GlobalInit globalInit;

    glslang::TShader shader(stage);
    const QByteArray fn = sourceFileName.toUtf8();
    const char *fnStr = fn.constData();
    const char *srcStr = actualSource->constData();
    const int size = actualSource->size();
    shader.setStringsWithLengthsAndNames(&srcStr, &size, &fnStr, 1);

    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_0);

    Includer includer;
    if (!shader.parse(&resourceLimits, 100, false, EShMsgDefault, includer)) {
        qWarning("QSpirvCompiler: Failed to parse shader");
        log = QString::fromUtf8(shader.getInfoLog()).trimmed();
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        qWarning("QSpirvCompiler: Link failed");
        log = QString::fromUtf8(shader.getInfoLog()).trimmed();
        return false;
    }

    std::vector<unsigned int> spv;
    glslang::GlslangToSpv(*program.getIntermediate(stage), spv);
    if (!spv.size()) {
        qWarning("Failed to generate SPIR-V");
        return false;
    }

    spirv.resize(int(spv.size() * 4));
    memcpy(spirv.data(), spv.data(), spirv.size());

    return true;
}

QSpirvCompiler::QSpirvCompiler()
    : d(new QSpirvCompilerPrivate)
{
}

QSpirvCompiler::~QSpirvCompiler()
{
    delete d;
}

void QSpirvCompiler::setSourceFileName(const QString &fileName)
{
    if (!d->readFile(fileName))
        return;

    const QString suffix = QFileInfo(fileName).suffix();
    if (suffix == QStringLiteral("vert")) {
        d->stage = EShLangVertex;
    } else if (suffix == QStringLiteral("frag")) {
        d->stage = EShLangFragment;
    } else if (suffix == QStringLiteral("tesc")) {
        d->stage = EShLangTessControl;
    } else if (suffix == QStringLiteral("tese")) {
        d->stage = EShLangTessEvaluation;
    } else if (suffix == QStringLiteral("geom")) {
        d->stage = EShLangGeometry;
    } else if (suffix == QStringLiteral("comp")) {
        d->stage = EShLangCompute;
    } else {
        qWarning("QSpirvCompiler: Unknown shader stage, defaulting to vertex");
        d->stage = EShLangVertex;
    }
}

static inline EShLanguage mapShaderStage(QBakedShader::ShaderStage stage)
{
    switch (stage) {
    case QBakedShader::VertexStage:
        return EShLangVertex;
    case QBakedShader::TessControlStage:
        return EShLangTessControl;
    case QBakedShader::TessEvaluationStage:
        return EShLangTessEvaluation;
    case QBakedShader::GeometryStage:
        return EShLangGeometry;
    case QBakedShader::FragmentStage:
        return EShLangFragment;
    case QBakedShader::ComputeStage:
        return EShLangCompute;
    default:
        return EShLangVertex;
    }
}

void QSpirvCompiler::setSourceFileName(const QString &fileName, QBakedShader::ShaderStage stage)
{
    if (!d->readFile(fileName))
        return;

    d->stage = mapShaderStage(stage);
}

void QSpirvCompiler::setSourceDevice(QIODevice *device, QBakedShader::ShaderStage stage, const QString &fileName)
{
    setSourceString(device->readAll(), stage, fileName);
}

void QSpirvCompiler::setSourceString(const QByteArray &sourceString, QBakedShader::ShaderStage stage, const QString &fileName)
{
    d->sourceFileName = fileName; // for error messages, include handling, etc.
    d->source = sourceString;
    d->batchableSource.clear();
    d->stage = mapShaderStage(stage);
}

void QSpirvCompiler::setFlags(Flags flags)
{
    d->flags = flags;
}

QByteArray QSpirvCompiler::compileToSpirv()
{
    if (d->stage == EShLangVertex && d->flags.testFlag(RewriteToMakeBatchableForSG) && d->batchableSource.isEmpty())
        d->batchableSource = QShaderBatchableRewriter::addZAdjustment(d->source);

    return d->compile() ? d->spirv : QByteArray();
}

QString QSpirvCompiler::errorMessage() const
{
    return d->log;
}

QT_END_NAMESPACE
