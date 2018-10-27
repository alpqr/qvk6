/****************************************************************************
 **
 ** Copyright (C) 2017 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the test suite of the Qt Toolkit.
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

// This application ships with Vulkan-style GLSL shaders.
//
// Let's run these through the shadertools infra to get:
//   - A SPIR-V binary suitable for Vulkan
//   - GLSL 330 sources suitable for a 3.3+ core profile context
//   - GLSL 120 or GLSL ES 100 for non-core context
//   - reflection info
//
// Then open two OpenGL windows and use the two GLSL shaders (taking the
// uniform buffer vs. struct differences into account), and a Vulkan window
// that uses the SPIR-V binary.

#include <QGuiApplication>
#include <QSpirvCompiler>
#include <QSpirvShader>
#include <QDebug>
#include <QOpenGLContext>
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

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QSpirvCompiler comp;
    comp.setSourceFileName(QLatin1String(":/color.vert"));
    QByteArray vertSpv = comp.compileToSpirv();
    if (vertSpv.isEmpty()) {
        qDebug() << comp.errorMessage();
        return 1;
    }
    comp.setSourceFileName(QLatin1String(":/color.frag"));
    QByteArray fragSpv = comp.compileToSpirv();
    if (fragSpv.isEmpty()) {
        qDebug() << comp.errorMessage();
        return 1;
    }

    QSpirvShader vs;
    vs.setSpirvBinary(vertSpv);
    qDebug() << "vertex shader reflection info:" << vs.shaderDescription();

    QSpirvShader fs;
    fs.setSpirvBinary(fragSpv);
    qDebug() << "fragment shader reflection info:" << fs.shaderDescription();

    // Now we have SPIR-V binaries that can be reflected and converted.
    // The rest of the magic is done in RenderWindow::init().

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
    VulkanWindow vkw(vertSpv, fragSpv);
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
