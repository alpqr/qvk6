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

#include "qshaderbaker.h"
#include "qspirvcompiler.h"
#include "qspirvshader.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>

QT_BEGIN_NAMESPACE

struct QShaderBakerPrivate
{
    bool readFile(const QString &fn);

    QString sourceFileName;
    QByteArray source;
    QBakedShader::ShaderStage stage;
    QVector<QShaderBaker::GeneratedShader> reqVersions;
    QVector<QBakedShader::ShaderVariant> variants;
    QSpirvCompiler compiler;
};

bool QShaderBakerPrivate::readFile(const QString &fn)
{
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("QShaderBaker: Failed to open %s", qPrintable(fn));
        return false;
    }
    source = f.readAll();
    sourceFileName = fn;
    return true;
}

QShaderBaker::QShaderBaker()
    : d(new QShaderBakerPrivate)
{
}

QShaderBaker::~QShaderBaker()
{
    delete d;
}

void QShaderBaker::setSourceFileName(const QString &fileName)
{
    if (!d->readFile(fileName))
        return;

    const QString suffix = QFileInfo(fileName).suffix();
    if (suffix == QStringLiteral("vert")) {
        d->stage = QBakedShader::VertexStage;
    } else if (suffix == QStringLiteral("frag")) {
        d->stage = QBakedShader::FragmentStage;
    } else if (suffix == QStringLiteral("tesc")) {
        d->stage = QBakedShader::TessControlStage;
    } else if (suffix == QStringLiteral("tese")) {
        d->stage = QBakedShader::TessEvaluationStage;
    } else if (suffix == QStringLiteral("geom")) {
        d->stage = QBakedShader::GeometryStage;
    } else if (suffix == QStringLiteral("comp")) {
        d->stage = QBakedShader::ComputeStage;
    } else {
        qWarning("QShaderBaker: Unknown shader stage, defaulting to vertex");
        d->stage = QBakedShader::VertexStage;
    }
}

void QShaderBaker::setSourceFileName(const QString &fileName, QBakedShader::ShaderStage stage)
{
    if (d->readFile(fileName))
        d->stage = stage;
}

void QShaderBaker::setSourceDevice(QIODevice *device, QBakedShader::ShaderStage stage, const QString &fileName)
{
    setSourceString(device->readAll(), stage, fileName);
}

void QShaderBaker::setSourceString(const QByteArray &sourceString, QBakedShader::ShaderStage stage, const QString &fileName)
{
    d->sourceFileName = fileName; // for error messages, include handling, etc.
    d->source = sourceString;
    d->stage = stage;
}

void QShaderBaker::setGeneratedShaders(const QVector<GeneratedShader> &v)
{
    d->reqVersions = v;
}

void QShaderBaker::setGeneratedShaderVariants(const QVector<QBakedShader::ShaderVariant> &v)
{
    d->variants = v;
}

QBakedShader QShaderBaker::bake()
{
    if (d->source.isEmpty()) {
        qWarning("QShaderBaker: No source specified");
        return QBakedShader();
    }

    d->compiler.setSourceString(d->source, d->stage, d->sourceFileName);
    d->compiler.setFlags(0);
    QByteArray spirv = d->compiler.compileToSpirv();
    if (spirv.isEmpty())
        return QBakedShader();

    QByteArray batchableSpirv;
    if (d->stage == QBakedShader::VertexStage && d->variants.contains(QBakedShader::BatchableVertexShader)) {
        d->compiler.setFlags(QSpirvCompiler::RewriteToMakeBatchableForSG);
        batchableSpirv = d->compiler.compileToSpirv();
        if (batchableSpirv.isEmpty())
            return QBakedShader();
    }

    QBakedShader bs;
    bs.setStage(d->stage);

    QSpirvShader spirvShader;
    spirvShader.setSpirvBinary(spirv);
    bs.setDescription(spirvShader.shaderDescription());

    QSpirvShader batchableSpirvShader;
    if (!batchableSpirv.isEmpty())
        batchableSpirvShader.setSpirvBinary(batchableSpirv);

    for (const GeneratedShader &req: d->reqVersions) {
        for (const QBakedShader::ShaderVariant &v : d->variants) {
            QByteArray *currentSpirv = &spirv;
            QSpirvShader *currentSpirvShader = &spirvShader;
            if (v == QBakedShader::BatchableVertexShader) {
                if (!batchableSpirv.isEmpty()) {
                    currentSpirv = &batchableSpirv;
                    currentSpirvShader = &batchableSpirvShader;
                } else {
                    continue;
                }
            }
            const QBakedShader::ShaderKey key(req.first, req.second, v);
            QBakedShader::Shader shader;
            shader.entryPoint = QByteArrayLiteral("main");
            switch (req.first) {
            case QBakedShader::SpirvShader:
                shader.shader = *currentSpirv;
                break;
            case QBakedShader::GlslShader:
            {
                QSpirvShader::GlslFlags flags = 0; // FixClipSpace??
                if (req.second.flags.testFlag(QBakedShader::ShaderSourceVersion::GlslEs))
                    flags |= QSpirvShader::GlslEs;
                shader.shader = currentSpirvShader->translateToGLSL(req.second.version, flags);
            }
                break;
            case QBakedShader::HlslShader:
                shader.shader = currentSpirvShader->translateToHLSL(req.second.version);
                break;
            case QBakedShader::MslShader:
                shader.shader = currentSpirvShader->translateToMSL();
                shader.entryPoint = QByteArrayLiteral("main0");
                break;
            default:
                Q_UNREACHABLE();
            }
            bs.setShader(key, shader);
        }
    }

    return bs;
}

QString QShaderBaker::errorMessage() const
{
    return d->compiler.errorMessage();
}

QT_END_NAMESPACE
