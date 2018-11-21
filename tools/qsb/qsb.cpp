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
#include <QtCore/qdir.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qprocess.h>
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

static bool runProcess(const QString &cmd, QByteArray *output, QByteArray *errorOutput)
{
    QProcess p;
    p.start(cmd);
    if (!p.waitForFinished()) {
        qWarning("Failed to run %s", qPrintable(cmd));
        return false;
    }

    if (p.exitStatus() == QProcess::CrashExit) {
        qWarning("%s crashed", qPrintable(cmd));
        return false;
    }

    *output = p.readAllStandardOutput();
    *errorOutput = p.readAllStandardError();

    if (p.exitCode() != 0) {
        qWarning("%s returned non-zero error code %d", qPrintable(cmd), p.exitCode());
        return false;
    }

    return true;
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
    case QBakedShader::DxbcShader:
        return QStringLiteral("DXBC");
    case QBakedShader::MslShader:
        return QStringLiteral("MSL");
    case QBakedShader::DxilShader:
        return QStringLiteral("DXIL");
    case QBakedShader::MetalLibShader:
        return QStringLiteral("metallib");
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
            Q_FALLTHROUGH();
        case QBakedShader::DxbcShader:
            Q_FALLTHROUGH();
        case QBakedShader::DxilShader:
            Q_FALLTHROUGH();
        case QBakedShader::MetalLibShader:
            ts << "Binary of " << shader.shader.size() << " bytes\n\n";
            break;
        default:
            ts << shader.shader << "\n";
            break;
        }
        ts << "\n************************************\n\n";
    }
}

static QByteArray fxcProfile(const QBakedShader &bs, const QBakedShader::ShaderKey &k)
{
    QByteArray t;

    switch (bs.stage()) {
    case QBakedShader::VertexStage:
        t += QByteArrayLiteral("vs_");
        break;
    case QBakedShader::TessControlStage:
        t += QByteArrayLiteral("hs_");
        break;
    case QBakedShader::TessEvaluationStage:
        t += QByteArrayLiteral("ds_");
        break;
    case QBakedShader::GeometryStage:
        t += QByteArrayLiteral("gs_");
        break;
    case QBakedShader::FragmentStage:
        t += QByteArrayLiteral("ps_");
        break;
    case QBakedShader::ComputeStage:
        t += QByteArrayLiteral("cs_");
        break;
    default:
        break;
    }

    const int major = k.sourceVersion.version / 10;
    const int minor = k.sourceVersion.version % 10;
    t += QByteArray::number(major);
    t += '_';
    t += QByteArray::number(minor);

    return t;
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
    QCommandLineOption fxcOption({ "c", "fxc" }, QObject::tr("In combination with --hlsl invokes fxc to store DXBC instead of HLSL."));
    cmdLineParser.addOption(fxcOption);
    QCommandLineOption mtllibOption({ "t", "mtllib" },
                                    QObject::tr("In combination with --msl builds a Metal library with xcrun metal(lib) and stores that instead of the source."));
    cmdLineParser.addOption(mtllibOption);
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
                if (version.endsWith(QLatin1String(" es"))) {
                    version = version.left(version.count() - 3);
                    flags |= QBakedShader::ShaderSourceVersion::GlslEs;
                } else if (version.endsWith(QLatin1String("es"))) {
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
            qWarning("Shader baking failed: %s", qPrintable(baker.errorMessage()));
            return 1;
        }

        if (cmdLineParser.isSet(fxcOption)) {
            QTemporaryDir tempDir;
            if (!tempDir.isValid()) {
                qWarning("Failed to create temporary directory");
                return 1;
            }
            auto skeys = bs.availableShaders();
            for (QBakedShader::ShaderKey &k : skeys) {
                if (k.source == QBakedShader::HlslShader) {
                    QBakedShader::Shader s = bs.shader(k);

                    const QString tmpIn = tempDir.path() + QLatin1String("/qsb_hlsl_temp");
                    const QString tmpOut = tempDir.path() + QLatin1String("/qsb_hlsl_temp_out");
                    QFile f(tmpIn);
                    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        qWarning("Failed to create temporary file");
                        return 1;
                    }
                    f.write(s.shader);
                    f.close();

                    const QByteArray tempOutFileName = QDir::toNativeSeparators(tmpOut).toUtf8();
                    const QByteArray inFileName = QDir::toNativeSeparators(tmpIn).toUtf8();
                    const QByteArray typeArg = fxcProfile(bs, k);
                    const QString cmd = QString::asprintf("fxc /nologo /E %s /T %s /Fo %s %s",
                                                          s.entryPoint.constData(),
                                                          typeArg.constData(),
                                                          tempOutFileName.constData(),
                                                          inFileName.constData());
                    QByteArray output;
                    QByteArray errorOutput;
                    bool success = runProcess(cmd, &output, &errorOutput);
                    if (!success) {
                        if (!output.isEmpty() || !errorOutput.isEmpty()) {
                            qDebug("%s\n%s",
                                   qPrintable(output.constData()),
                                   qPrintable(errorOutput.constData()));
                        }
                        return 1;
                    }
                    f.setFileName(tmpOut);
                    if (!f.open(QIODevice::ReadOnly)) {
                        qWarning("Failed to open fxc output %s", qPrintable(tmpOut));
                        return 1;
                    }
                    const QByteArray bytecode = f.readAll();
                    f.close();

                    QBakedShader::ShaderKey dxbcKey = k;
                    dxbcKey.source = QBakedShader::DxbcShader;
                    QBakedShader::Shader dxbcShader(bytecode, s.entryPoint);
                    bs.setShader(dxbcKey, dxbcShader);
                    bs.removeShader(k);
                }
            }
        }

        if (cmdLineParser.isSet(mtllibOption)) {
            QTemporaryDir tempDir;
            if (!tempDir.isValid()) {
                qWarning("Failed to create temporary directory");
                return 1;
            }
            auto skeys = bs.availableShaders();
            for (QBakedShader::ShaderKey &k : skeys) {
                if (k.source == QBakedShader::MslShader) {
                    QBakedShader::Shader s = bs.shader(k);

                    const QString tmpIn = tempDir.path() + QLatin1String("/qsb_msl_temp");
                    const QString tmpInterm = tempDir.path() + QLatin1String("/qsb_msl_temp_air");
                    const QString tmpOut = tempDir.path() + QLatin1String("/qsb_msl_temp_out");
                    QFile f(tmpIn);
                    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        qWarning("Failed to create temporary file");
                        return 1;
                    }
                    f.write(s.shader);
                    f.close();

                    const QByteArray inFileName = QDir::toNativeSeparators(tmpIn).toUtf8();
                    const QByteArray tempIntermediateFileName = QDir::toNativeSeparators(tmpInterm).toUtf8();
                    QString cmd = QString::asprintf("xcrun -sdk macosx metal %s -o %s",
                                                    inFileName.constData(),
                                                    tempIntermediateFileName.constData());
                    QByteArray output;
                    QByteArray errorOutput;
                    bool success = runProcess(cmd, &output, &errorOutput);
                    if (!success) {
                        if (!output.isEmpty() || !errorOutput.isEmpty()) {
                            qDebug("%s\n%s",
                                   qPrintable(output.constData()),
                                   qPrintable(errorOutput.constData()));
                        }
                        return 1;
                    }
                    f.remove();

                    const QByteArray tempOutFileName = QDir::toNativeSeparators(tmpOut).toUtf8();
                    cmd = QString::asprintf("xcrun -sdk macosx metallib %s -o %s",
                                            tempIntermediateFileName.constData(),
                                            tempOutFileName.constData());
                    output.clear();
                    errorOutput.clear();
                    success = runProcess(cmd, &output, &errorOutput);
                    if (!success) {
                        if (!output.isEmpty() || !errorOutput.isEmpty()) {
                            qDebug("%s\n%s",
                                   qPrintable(output.constData()),
                                   qPrintable(errorOutput.constData()));
                        }
                        return 1;
                    }

                    f.setFileName(tmpOut);
                    if (!f.open(QIODevice::ReadOnly)) {
                        qWarning("Failed to open xcrun metallib output %s", qPrintable(tmpOut));
                        return 1;
                    }
                    const QByteArray bytecode = f.readAll();
                    f.close();
                    f.remove();

                    QBakedShader::ShaderKey mtlKey = k;
                    mtlKey.source = QBakedShader::MetalLibShader;
                    QBakedShader::Shader mtlShader(bytecode, s.entryPoint);
                    bs.setShader(mtlKey, mtlShader);
                    bs.removeShader(k);
                }
            }
        }

        if (cmdLineParser.isSet(outputOption))
            writeToFile(bs.serialized(), cmdLineParser.value(outputOption));
    }

    return 0;
}
