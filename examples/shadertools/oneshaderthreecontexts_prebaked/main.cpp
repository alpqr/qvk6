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

// This application ships with pre-generated shader variants + reflection info
// based on the original Vulkan-style GLSL shaders.
//
// It opens two OpenGL windows using the two GLSL shaders (taking the uniform
// buffer vs. struct differences into account), and a Vulkan window that uses
// the SPIR-V binary.

#include <QGuiApplication>
#include <QBakedShader>
#include <QDebug>
#include <QOpenGLContext>
#include <QFile>
#include "renderwindow.h"

#if QT_CONFIG(vulkan)
#include "trianglerenderer.h"

class VulkanWindow : public QVulkanWindow
{
public:
    VulkanWindow(const QByteArray &vs, const QByteArray &fs)
        : m_vs(vs),
          m_fs(fs)
    { }

    QVulkanWindowRenderer *createRenderer() override;

private:
    QByteArray m_vs;
    QByteArray m_fs;
};

QVulkanWindowRenderer *VulkanWindow::createRenderer()
{
    return new TriangleRenderer(this, m_vs, m_fs, true);
}
#endif // vulkan

static QByteArray readFile(const char *fn)
{
    QFile f(QString::fromUtf8(fn));
    if (f.open(QIODevice::ReadOnly))
        return f.readAll();

    return QByteArray();
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    // our pre-generated .qsb files contain the reflection data, GLSL 100 es,
    // 120, 330 sources, and the SPIR-V binary

    QBakedShader vs = QBakedShader::fromSerialized(readFile(":/color.vert.qsb"));
    QBakedShader fs = QBakedShader::fromSerialized(readFile(":/color.frag.qsb"));

    qDebug() << "vertex shader reflection info:" << vs.description();
    qDebug() << "fragment shader reflection info:" << fs.description();

    // GL 2.0-compatible context
    QSurfaceFormat fmt;
    RenderWindow w(vs, fs, fmt);
    w.resize(800, 600);
    w.setTitle(QLatin1String("GL 2"));
    w.show();

    // 3.3 core
    QScopedPointer<RenderWindow> cw;
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        QSurfaceFormat coreFmt;
        coreFmt.setVersion(3, 3);
        coreFmt.setProfile(QSurfaceFormat::CoreProfile);
        cw.reset(new RenderWindow(vs, fs, coreFmt));
        cw->resize(800, 600);
        cw->setTitle(QLatin1String("GL 3.3 core"));
        cw->show();
    }

    // Vulkan
#if QT_CONFIG(vulkan)
    QVulkanInstance inst;
    const QByteArray vsSpv = vs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader;
    const QByteArray fsSpv = fs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader;
    VulkanWindow vkw(vsSpv, fsSpv);
    if (inst.create()) {
        vkw.setVulkanInstance(&inst);
        vkw.resize(800, 600);
        vkw.setTitle(QLatin1String("Vulkan"));
        vkw.show();
    } else {
        qDebug("Vulkan not supported");
    }
#endif

    return app.exec();
}
