/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcommandlineparser.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdebug.h>
#include <QtShaderTools/qspirvshader.h>
#include <QtShaderTools/qspirvcompiler.h>

static bool writeToFile(const QByteArray &buf, const QString &filename, bool text = false)
{
    QFile f(filename);
    QIODevice::OpenMode flags = QIODevice::WriteOnly;
    if (text)
        flags |= QIODevice::Text;
    if (!f.open(flags)) {
        qWarning("Failed to open %s for writing", qPrintable(filename));
        return false;
    }
    f.write(buf);
    return true;
}

static QByteArray compile(const QString &fn, QString *outSpvName, QSpirvCompiler::Flags flags)
{
    QSpirvCompiler compiler;
    compiler.setSourceFileName(fn);
    compiler.setFlags(flags);
    const QByteArray spirv = compiler.compileToSpirv();
    if (spirv.isEmpty()) {
        qDebug("%s", qPrintable(compiler.errorMessage()));
        return QByteArray();
    }

    QFileInfo info(fn);
    *outSpvName = info.filePath() + QLatin1String(".spv");

    return spirv;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addPositionalArgument(QLatin1String("file"), QObject::tr("Shader to compile. Extension must be .vert, .frag, etc."), QObject::tr("files..."));
    QCommandLineOption versionOption({ "n", "versions" },
                                     QObject::tr("Comma-separated list of output GLSL versions (e.g. 100 es, 120, 300 es, 330, etc.). Defaults to \"100 es,120,330\". Set to \"\" to disable GLSL."),
                                     QObject::tr("version"));
    cmdLineParser.addOption(versionOption);
    QCommandLineOption clipSpaceOption({ "c", "fix-clipspace" }, QObject::tr("Fix up depth [0, w] -> [-w, w]"));
    cmdLineParser.addOption(clipSpaceOption);
    QCommandLineOption hlslOption({ "l", "hlsl" }, QObject::tr("Output HLSL as well (experimental)."));
    cmdLineParser.addOption(hlslOption);
    QCommandLineOption mslOption({ "m", "msl" }, QObject::tr("Output MSL as well (experimental)."));
    cmdLineParser.addOption(mslOption);
    QCommandLineOption stripOption({ "s", "strip" }, QObject::tr("Strip the output SPIR-V."));
    cmdLineParser.addOption(stripOption);
    QCommandLineOption batchableOption({ "b", "batchable" }, QObject::tr("Rewrite the vertex shader for Qt Quick scene graph batching."));
    cmdLineParser.addOption(batchableOption);

    cmdLineParser.process(app);

    if (cmdLineParser.positionalArguments().isEmpty()) {
        cmdLineParser.showHelp();
        return 0;
    }

    for (const QString &fn : cmdLineParser.positionalArguments()) {
        // Compile to SPIR-V.
        QString spvName;
        QSpirvCompiler::Flags flags = 0;
        if (cmdLineParser.isSet(batchableOption))
            flags |= QSpirvCompiler::RewriteToMakeBatchableForSG;
        QByteArray spirv = compile(fn, &spvName, flags);
        if (spirv.isEmpty())
            return 1;

        // Generate reflection information from the SPIR-V binary.
        QSpirvShader shader;
        shader.setSpirvBinary(spirv);
        QShaderDescription desc = shader.shaderDescription();
        if (!desc.isValid())
            return 1;

        // Strip the SPIR-V binary, if requested.
        if (cmdLineParser.isSet(stripOption)) {
            QString errMsg;
            const QByteArray strippedSpirv = shader.strippedSpirvBinary(0, &errMsg);
            if (strippedSpirv.isEmpty()) {
                qDebug("%s", qPrintable(errMsg));
                return 1;
            }
            spirv = strippedSpirv; // only used for the file write, QSpirvShader has the original still
        }

        // Write out to the .spv file
        if (!writeToFile(spirv, spvName))
            return 1;

        QFileInfo info(spvName);
        const QString outBaseName = info.canonicalPath() + QChar('/') + info.completeBaseName();

        // Write out reflection info.
        const QString binReflName = outBaseName + QLatin1String(".refl");
        if (!writeToFile(desc.toBinaryJson(), binReflName))
            return 1;
        const QString textReflName = outBaseName + QLatin1String(".refl.json");
        if (!writeToFile(desc.toJson(), textReflName, true))
            return 1;

        // GLSL.
        struct GLSLVersion {
            int version = 100;
            bool es = false;
        };
        QVector<GLSLVersion> versions;
        QString versionStr = QLatin1String("100 es,120,330");
        if (cmdLineParser.isSet(versionOption))
            versionStr = cmdLineParser.value(versionOption);

        for (QString v : versionStr.split(',')) {
            v = v.trimmed();
            if (v.isEmpty())
                continue;
            GLSLVersion ver;
            ver.es = v.endsWith(QStringLiteral(" es"));
            if (ver.es)
                v = v.mid(0, v.count() - 3);
            bool ok = false;
            int val = v.toInt(&ok);
            if (ok) {
                ver.version = val;
                versions.append(ver);
            } else {
                qWarning("Invalid version %s", qPrintable(versionStr));
            }
        }

        for (const GLSLVersion &ver : versions) {
            QSpirvShader::GlslFlags flags = 0;
            if (ver.es)
                flags |= QSpirvShader::GlslEs;
            if (cmdLineParser.isSet(clipSpaceOption))
                flags |= QSpirvShader::FixClipSpace;
            QString glslName = outBaseName + QLatin1String(".glsl") + QString::number(ver.version);
            if (ver.es)
                glslName += QLatin1String("es");
            if (!writeToFile(shader.translateToGLSL(ver.version, flags), glslName, true))
                return 1;
        }

        // HLSL.
        if (cmdLineParser.isSet(hlslOption)) {
            const QString hlslName = outBaseName + QLatin1String(".hlsl");
            if (!writeToFile(shader.translateToHLSL(), hlslName, true))
                return 1;
        }

        // Metal SL.
        if (cmdLineParser.isSet(mslOption)) {
            const QString mslName = outBaseName + QLatin1String(".msl");
            if (!writeToFile(shader.translateToMSL(), mslName, true))
                return 1;
        }
    }

    return 0;
}
