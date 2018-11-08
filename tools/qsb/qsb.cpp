/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcommandlineparser.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtShaderTools/qshaderbaker.h>

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

static QByteArray readFile(const QString &filename, bool text = false)
{
    QFile f(filename);
    QIODevice::OpenMode flags = QIODevice::ReadOnly;
    if (text)
        flags |= QIODevice::Text;
    if (!f.open(flags)) {
        qWarning("Failed to open %s", qPrintable(filename));
        return QByteArray();
    }
    return f.readAll();
}

static QString stageStr(QBakedShader::ShaderStage stage)
{
    switch (stage) {
    case QBakedShader::VertexStage:
        return QStringLiteral("Vertex");
    case QBakedShader::TessControlStage:
        return QStringLiteral("TessControl");
    case QBakedShader::TessEvaluationStage:
        return QStringLiteral("TessEval");
    case QBakedShader::GeometryStage:
        return QStringLiteral("Geometry");
    case QBakedShader::FragmentStage:
        return QStringLiteral("Fragment");
    case QBakedShader::ComputeStage:
        return QStringLiteral("Compute");
    default:
        Q_UNREACHABLE();
    }
}

static QString sourceStr(QBakedShader::ShaderSource source)
{
    switch (source) {
    case QBakedShader::SpirvShader:
        return QStringLiteral("SPIR-V");
    case QBakedShader::GlslShader:
        return QStringLiteral("GLSL");
    case QBakedShader::HlslShader:
        return QStringLiteral("HLSL");
    case QBakedShader::DxShader:
        return QStringLiteral("DXBC/DXIL");
    case QBakedShader::MslShader:
        return QStringLiteral("MSL");
    default:
        Q_UNREACHABLE();
    }
}

static QString sourceVersionStr(const QBakedShader::ShaderSourceVersion &v)
{
    QString s = v.version ? QString::number(v.version) : QString();
    if (v.flags.testFlag(QBakedShader::ShaderSourceVersion::GlslEs))
        s += QLatin1String(" es");

    return s;
}

static QString sourceVariantStr(const QBakedShader::ShaderVariant &v)
{
    switch (v) {
    case QBakedShader::NormalShader:
        return QLatin1String("Normal");
    case QBakedShader::BatchableVertexShader:
        return QLatin1String("Batchable");
    default:
        Q_UNREACHABLE();
    }
}

static void dump(const QBakedShader &bs)
{
    QTextStream ts(stdout);
    ts << "Stage: " << stageStr(bs.stage()) << "\n\n";
    QList<QBakedShader::ShaderKey> s = bs.availableShaders();
    ts << "Has " << s.count() << " shaders: (unordered list)\n";
    for (int i = 0; i < s.count(); ++i) {
        ts << "  Shader " << i << ": " << sourceStr(s[i].source)
            << " " << sourceVersionStr(s[i].sourceVersion)
            << " [" << sourceVariantStr(s[i].variant) << "]\n";
    }
    ts << "\n";
    ts << "Reflection info: " << bs.description().toJson() << "\n\n";
    for (int i = 0; i < s.count(); ++i) {
        ts << "Shader " << i << ": " << sourceStr(s[i].source)
            << " " << sourceVersionStr(s[i].sourceVersion)
            << " [" << sourceVariantStr(s[i].variant) << "]\n";
        QBakedShader::Shader shader = bs.shader(s[i]);
        if (!shader.entryPoint.isEmpty())
            ts << "Entry point: " << shader.entryPoint << "\n";
        ts << "Contents:\n";
        switch (s[i].source) {
        case QBakedShader::SpirvShader:
        case QBakedShader::DxShader:
            ts << "Binary of " << shader.shader.size() << " bytes\n\n";
            break;
        default:
            ts << shader.shader << "\n";
            break;
        }
        ts << "\n************************************\n\n";
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QCommandLineParser cmdLineParser;
    cmdLineParser.setApplicationDescription(QObject::tr("Qt Shader Baker"));
    cmdLineParser.addHelpOption();
    cmdLineParser.addPositionalArgument(QLatin1String("file"), QObject::tr("Vulkan GLSL source file to compile"), QObject::tr("file"));
    QCommandLineOption batchableOption({ "b", "batchable" }, QObject::tr("Also generates rewritten vertex shader for Qt Quick scene graph batching."));
    cmdLineParser.addOption(batchableOption);
    QCommandLineOption glslOption({ "g", "glsl" },
                                  QObject::tr("Comma separated list of GLSL versions to generate. (for example, \"100 es,120,330\")"),
                                  QObject::tr("glsl"));
    cmdLineParser.addOption(glslOption);
    QCommandLineOption hlslOption({ "l", "hlsl" },
                                  QObject::tr("Comma separated list of HLSL (Shader Model) versions to generate. F.ex. 50 is 5.0, 51 is 5.1."),
                                  QObject::tr("hlsl"));
    cmdLineParser.addOption(hlslOption);
    QCommandLineOption mslOption({ "m", "msl" },
                                  QObject::tr("Comma separated list of Metal Shading Language versions to generate. F.ex. 12 is 1.2, 20 is 2.0."),
                                  QObject::tr("msl"));
    cmdLineParser.addOption(mslOption);
    QCommandLineOption outputOption({ "o", "output" },
                                     QObject::tr("Output file for the baked shader pack."),
                                     QObject::tr("output"));
    cmdLineParser.addOption(outputOption);
    QCommandLineOption dumpOption({ "d", "dump" }, QObject::tr("Switches to dump mode. Input file is expected to be a baked shader pack."));
    cmdLineParser.addOption(dumpOption);

    cmdLineParser.process(app);

    if (cmdLineParser.positionalArguments().isEmpty()) {
        cmdLineParser.showHelp();
        return 0;
    }

    QShaderBaker baker;
    for (const QString &fn : cmdLineParser.positionalArguments()) {
        if (cmdLineParser.isSet(dumpOption)) {
            QByteArray buf = readFile(fn);
            if (!buf.isEmpty()) {
                QBakedShader bs = QBakedShader::fromSerialized(buf);
                if (bs.isValid())
                    dump(bs);
                else
                    qWarning("Failed to deserialize %s", qPrintable(fn));
            }
            continue;
        }

        baker.setSourceFileName(fn);

        QVector<QBakedShader::ShaderVariant> variants;
        variants << QBakedShader::NormalShader;
        if (cmdLineParser.isSet(batchableOption))
            variants << QBakedShader::BatchableVertexShader;
        baker.setGeneratedShaderVariants(variants);

        QVector<QShaderBaker::GeneratedShader> genShaders;
        genShaders << qMakePair(QBakedShader::SpirvShader, QBakedShader::ShaderSourceVersion(100));
        if (cmdLineParser.isSet(glslOption)) {
            const QStringList versions = cmdLineParser.value(glslOption).trimmed().split(',');
            for (QString version : versions) {
                QBakedShader::ShaderSourceVersion::Flags flags = 0;
                if (version.endsWith(QStringLiteral(" es"))) {
                    version = version.left(version.count() - 3);
                    flags |= QBakedShader::ShaderSourceVersion::GlslEs;
                } else if (version.endsWith(QStringLiteral("es"))) {
                    version = version.left(version.count() - 2);
                    flags |= QBakedShader::ShaderSourceVersion::GlslEs;
                }
                bool ok = false;
                int v = version.toInt(&ok);
                if (ok)
                    genShaders << qMakePair(QBakedShader::GlslShader, QBakedShader::ShaderSourceVersion(v, flags));
                else
                    qWarning("Ignoring invalid GLSL version %s", qPrintable(version));
            }
        }
        if (cmdLineParser.isSet(hlslOption)) {
            const QStringList versions = cmdLineParser.value(hlslOption).trimmed().split(',');
            for (QString version : versions) {
                bool ok = false;
                int v = version.toInt(&ok);
                if (ok)
                    genShaders << qMakePair(QBakedShader::HlslShader, QBakedShader::ShaderSourceVersion(v));
                else
                    qWarning("Ignoring invalid HLSL (Shader Model) version %s", qPrintable(version));
            }
        }
        if (cmdLineParser.isSet(mslOption)) {
            const QStringList versions = cmdLineParser.value(mslOption).trimmed().split(',');
            for (QString version : versions) {
                bool ok = false;
                int v = version.toInt(&ok);
                if (ok)
                    genShaders << qMakePair(QBakedShader::MslShader, QBakedShader::ShaderSourceVersion(v));
                else
                    qWarning("Ignoring invalid MSL version %s", qPrintable(version));
            }
        }
        baker.setGeneratedShaders(genShaders);

        QBakedShader bs = baker.bake();
        if (!bs.isValid()) {
            qWarning("%s", qPrintable(baker.errorMessage()));
            return 1;
        }

        if (cmdLineParser.isSet(outputOption))
            writeToFile(bs.serialized(), cmdLineParser.value(outputOption));
    }

    return 0;
}
