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
#include <QLoggingCategory>
#include <QRhiVulkanInitParams>
#include "examplewindow.h"

class VWindow : public ExampleWindow
{
public:
    VWindow() { setSurfaceType(VulkanSurface); }
    ~VWindow() { releaseResources(); }

private:
    void init() override;
};

void VWindow::init()
{
    QRhiVulkanInitParams params;
    params.inst = vulkanInstance();
    params.importExistingDevice = false;
    params.window = this;
    m_r = QRhi::create(QRhi::Vulkan, &params);

    //setSampleCount(4); // enable 4x MSAA (except for the render-to-texture pass)

    ExampleWindow::init();
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
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

    VWindow w;
    if (inst.create()) {
        w.setVulkanInstance(&inst);
        w.resize(1280, 720);
        w.setTitle(QLatin1String("Vulkan"));
        w.show();
    } else {
        qWarning("Vulkan not supported");
        return 1;
    }

    return app.exec();
}
