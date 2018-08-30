/****************************************************************************
 **
 ** Copyright (C) 2018 The Qt Company Ltd.
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

#include <QGuiApplication>
#include <QWindow>
#include <QVulkanInstance>
#include <QLoggingCategory>
#include <QPlatformSurfaceEvent>

class VWindow : public QWindow
{
public:
    VWindow() { setSurfaceType(VulkanSurface); }
    ~VWindow() { releaseResources(); }

private:
    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

    void init();
    void releaseResources();
    void recreateSwapChain();
    void releaseSwapChain();
    void render();

    bool m_inited = false;
};

void VWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed() && !m_inited) {
        m_inited = true;
        init();
        recreateSwapChain();
        render();
    }

    // Release everything when unexposed - the meaning of which is platform specific.
    if (!isExposed() && m_inited) {
        m_inited = false;
        releaseSwapChain();
        releaseResources();
    }
}

void VWindow::init()
{
}

void VWindow::releaseResources()
{
}

void VWindow::recreateSwapChain()
{
}

void VWindow::releaseSwapChain()
{
}

void VWindow::render()
{
}

bool VWindow::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::UpdateRequest:
        render();
        break;

    // Now the fun part: the swapchain must be destroyed before the surface as per
    // spec. This is not ideal for us because the surface is managed by the
    // QPlatformWindow which may be gone already when the unexpose comes, making the
    // validation layer scream. The solution is to listen to the PlatformSurface events.
    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            releaseSwapChain();
        break;

    default:
        break;
    }

    return QWindow::event(e);
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

    QVulkanInstance inst;

#ifndef Q_OS_ANDROID
    inst.setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation");
#else
    inst.setLayers(QByteArrayList()
                   << "VK_LAYER_GOOGLE_threading"
                   << "VK_LAYER_LUNARG_parameter_validation"
                   << "VK_LAYER_LUNARG_object_tracker"
                   << "VK_LAYER_LUNARG_core_validation"
                   << "VK_LAYER_LUNARG_image"
                   << "VK_LAYER_LUNARG_swapchain"
                   << "VK_LAYER_GOOGLE_unique_objects");
#endif

    VWindow vkw;
    if (inst.create()) {
        vkw.setVulkanInstance(&inst);
        vkw.resize(1280, 720);
        vkw.setTitle(QLatin1String("Vulkan"));
        vkw.show();
    } else {
        qWarning("Vulkan not supported");
        return 1;
    }

    return app.exec();
}
