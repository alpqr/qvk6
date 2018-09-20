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
#include <QLoggingCategory>
#include <QVulkanInstance>
#include <QVulkanFunctions>
#include <QPlatformSurfaceEvent>

#include <QRhiVulkanInitParams>

#include "trianglerenderer.h"
#include "texturedcuberenderer.h"
#include "triangleoncuberenderer.h"

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
    VkPhysicalDevice m_vkPhysDev;
    VkPhysicalDeviceProperties m_physDevProps;
    VkDevice m_vkDev = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_devFuncs;
    VkQueue m_vkGfxQueue;
    VkQueue m_vkPresQueue;
    VkCommandPool m_vkCmdPool = VK_NULL_HANDLE;

    QRhi *m_r = nullptr;
    bool m_hasSwapChain = false;
    bool m_swapChainChanged = false;
    QRhiSwapChain *m_sc = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;

    TriangleRenderer m_triRenderer;
    TexturedCubeRenderer m_cubeRenderer;
    TriangleOnCubeRenderer m_liveTexCubeRenderer;
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
    QVulkanInstance *inst = vulkanInstance();
    QVulkanFunctions *f = inst->functions();
    uint32_t devCount = 0;
    f->vkEnumeratePhysicalDevices(inst->vkInstance(), &devCount, nullptr);
    qDebug("%d physical devices", devCount);
    if (!devCount)
        qFatal("No physical devices");

    // Just pick the first physical device for now.
    devCount = 1;
    VkResult err = f->vkEnumeratePhysicalDevices(inst->vkInstance(), &devCount, &m_vkPhysDev);
    if (err != VK_SUCCESS)
        qFatal("Failed to enumerate physical devices: %d", err);

    f->vkGetPhysicalDeviceProperties(m_vkPhysDev, &m_physDevProps);
    qDebug("Device name: %s Driver version: %d.%d.%d", m_physDevProps.deviceName,
           VK_VERSION_MAJOR(m_physDevProps.driverVersion), VK_VERSION_MINOR(m_physDevProps.driverVersion),
           VK_VERSION_PATCH(m_physDevProps.driverVersion));

    uint32_t queueCount = 0;
    f->vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysDev, &queueCount, nullptr);
    QVector<VkQueueFamilyProperties> queueFamilyProps(queueCount);
    f->vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysDev, &queueCount, queueFamilyProps.data());
    int gfxQueueFamilyIdx = -1;
    int presQueueFamilyIdx = -1;
    // First look for a queue that supports both.
    for (int i = 0; i < queueFamilyProps.count(); ++i) {
        qDebug("queue family %d: flags=0x%x count=%d", i, queueFamilyProps[i].queueFlags, queueFamilyProps[i].queueCount);
        if (gfxQueueFamilyIdx == -1
                && (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                && inst->supportsPresent(m_vkPhysDev, i, this))
            gfxQueueFamilyIdx = i;
    }
    if (gfxQueueFamilyIdx != -1) {
        presQueueFamilyIdx = gfxQueueFamilyIdx;
    } else {
        // Separate queues then.
        // ### not like the underlying stuff supports this yet but we can just pretend...
        qDebug("No queue with graphics+present; trying separate queues");
        for (int i = 0; i < queueFamilyProps.count(); ++i) {
            if (gfxQueueFamilyIdx == -1 && (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                gfxQueueFamilyIdx = i;
            if (presQueueFamilyIdx == -1 && inst->supportsPresent(m_vkPhysDev, i, this))
                presQueueFamilyIdx = i;
        }
    }
    if (gfxQueueFamilyIdx == -1)
        qFatal("No graphics queue family found");
    if (presQueueFamilyIdx == -1)
        qFatal("No present queue family found");

    VkDeviceQueueCreateInfo queueInfo[2];
    const float prio[] = { 0 };
    memset(queueInfo, 0, sizeof(queueInfo));
    queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo[0].queueFamilyIndex = gfxQueueFamilyIdx;
    queueInfo[0].queueCount = 1;
    queueInfo[0].pQueuePriorities = prio;
    if (gfxQueueFamilyIdx != presQueueFamilyIdx) {
        queueInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo[1].queueFamilyIndex = presQueueFamilyIdx;
        queueInfo[1].queueCount = 1;
        queueInfo[1].pQueuePriorities = prio;
    }

    QVector<const char *> devLayers;
    if (inst->layers().contains("VK_LAYER_LUNARG_standard_validation"))
        devLayers.append("VK_LAYER_LUNARG_standard_validation");

    QVector<const char *> devExts;
    devExts.append("VK_KHR_swapchain");

    VkDeviceCreateInfo devInfo;
    memset(&devInfo, 0, sizeof(devInfo));
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = gfxQueueFamilyIdx == presQueueFamilyIdx ? 1 : 2;
    devInfo.pQueueCreateInfos = queueInfo;
    devInfo.enabledLayerCount = devLayers.count();
    devInfo.ppEnabledLayerNames = devLayers.constData();
    devInfo.enabledExtensionCount = devExts.count();
    devInfo.ppEnabledExtensionNames = devExts.constData();

    err = f->vkCreateDevice(m_vkPhysDev, &devInfo, nullptr, &m_vkDev);
    if (err != VK_SUCCESS)
        qFatal("Failed to create device: %d", err);

    m_devFuncs = inst->deviceFunctions(m_vkDev);

    m_devFuncs->vkGetDeviceQueue(m_vkDev, gfxQueueFamilyIdx, 0, &m_vkGfxQueue);
    if (gfxQueueFamilyIdx == presQueueFamilyIdx)
        m_vkPresQueue = m_vkGfxQueue;
    else
        m_devFuncs->vkGetDeviceQueue(m_vkDev, presQueueFamilyIdx, 0, &m_vkPresQueue);

    VkCommandPoolCreateInfo poolInfo;
    memset(&poolInfo, 0, sizeof(poolInfo));
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = gfxQueueFamilyIdx;
    err = m_devFuncs->vkCreateCommandPool(m_vkDev, &poolInfo, nullptr, &m_vkCmdPool);
    if (err != VK_SUCCESS)
        qFatal("Failed to create command pool: %d", err);

    QRhiVulkanInitParams params;
    params.inst = vulkanInstance();
    params.physDev = m_vkPhysDev;
    params.dev = m_vkDev;
    params.cmdPool = m_vkCmdPool;
    params.gfxQueue = m_vkGfxQueue;
    m_r = QRhi::create(QRhi::Vulkan, &params);

    m_triRenderer.setRhi(m_r);
    m_triRenderer.initResources();
    m_triRenderer.setTranslation(QVector3D(0, 0.5f, 0));

    m_cubeRenderer.setRhi(m_r);
    m_cubeRenderer.initResources();
    m_cubeRenderer.setTranslation(QVector3D(0, -0.5f, 0));

    m_liveTexCubeRenderer.setRhi(m_r);
    m_liveTexCubeRenderer.initResources();
    m_liveTexCubeRenderer.setTranslation(QVector3D(-2.0f, 0, 0));

    m_sc = m_r->createSwapChain();
}

void VWindow::releaseResources()
{
    if (!m_vkDev)
        return;

    m_devFuncs->vkDeviceWaitIdle(m_vkDev);

    m_triRenderer.releaseOutputDependentResources();
    m_triRenderer.releaseResources();

    m_cubeRenderer.releaseOutputDependentResources();
    m_cubeRenderer.releaseResources();

    m_liveTexCubeRenderer.releaseOutputDependentResources();
    m_liveTexCubeRenderer.releaseResources();

    delete m_sc;
    m_sc = nullptr;

    delete m_r;
    m_r = nullptr;

    if (m_vkCmdPool) {
        m_devFuncs->vkDestroyCommandPool(m_vkDev, m_vkCmdPool, nullptr);
        m_vkCmdPool = VK_NULL_HANDLE;
    }

    if (m_vkDev) {
        m_devFuncs->vkDestroyDevice(m_vkDev, nullptr);
        // Play nice and notify QVulkanInstance that the QVulkanDeviceFunctions
        // for m_vkDev needs to be invalidated.
        vulkanInstance()->resetDeviceFunctions(m_vkDev);
        m_vkDev = VK_NULL_HANDLE;
    }
}

void VWindow::recreateSwapChain()
{
    const QSize outputSize = size() * devicePixelRatio();

    if (!m_ds) {
        m_ds = m_r->createRenderBuffer(QRhiRenderBuffer::DepthStencil, outputSize, TriangleRenderer::SAMPLES);
    } else {
        m_ds->release();
        m_ds->pixelSize = outputSize;
    }
    m_ds->build();

    m_hasSwapChain = m_sc->build(this, outputSize, QRhiSwapChain::UseDepthStencil, m_ds, TriangleRenderer::SAMPLES);
    m_swapChainChanged = true;
}

void VWindow::releaseSwapChain()
{
    if (m_hasSwapChain) {
        m_hasSwapChain = false;
        m_sc->release();
    }
    if (m_ds) {
        m_ds->release();
        delete m_ds;
        m_ds = nullptr;
    }
}

void VWindow::render()
{
    if (!m_hasSwapChain)
        return;

    if (m_sc->sizeInPixels() != size() * devicePixelRatio()) {
        recreateSwapChain();
        if (!m_hasSwapChain)
            return;
    }

    QRhi::FrameOpResult r = m_r->beginFrame(m_sc);
    if (r == QRhi::FrameOpSwapChainOutOfDate) {
        recreateSwapChain();
        if (!m_hasSwapChain)
            return;
        r = m_r->beginFrame(m_sc);
    }
    if (r != QRhi::FrameOpSuccess) {
        requestUpdate();
        return;
    }

    if (m_swapChainChanged) {
        m_swapChainChanged = false;
        m_triRenderer.releaseOutputDependentResources();
        m_cubeRenderer.releaseOutputDependentResources();
        m_liveTexCubeRenderer.releaseOutputDependentResources();
    }

    if (!m_triRenderer.isPipelineInitialized()) {
        const QRhiRenderPass *rp = m_sc->defaultRenderPass();
        m_triRenderer.initOutputDependentResources(rp, m_sc->sizeInPixels());
        m_cubeRenderer.initOutputDependentResources(rp, m_sc->sizeInPixels());
        m_liveTexCubeRenderer.initOutputDependentResources(rp, m_sc->sizeInPixels());
    }

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    m_liveTexCubeRenderer.queueOffscreenPass(cb);

    QRhi::PassUpdates u;
    u += m_triRenderer.update();
    u += m_cubeRenderer.update();
    u += m_liveTexCubeRenderer.update();

    const QVector4D clearColor(0.4f, 0.7f, 0.0f, 1.0f);
    const QRhiClearValue clearValues[] = {
        clearColor,
        QRhiClearValue(1.0f, 0), // depth, stencil
        clearColor // 3 attachments when using MSAA
    };
    m_r->beginPass(m_sc->currentFrameRenderTarget(), cb, clearValues, u);
    m_triRenderer.queueDraw(cb, m_sc->sizeInPixels());
    m_cubeRenderer.queueDraw(cb, m_sc->sizeInPixels());
    m_liveTexCubeRenderer.queueDraw(cb, m_sc->sizeInPixels());
    m_r->endPass(cb);

    m_r->endFrame(m_sc);

    requestUpdate(); // render continuously, throttled by the presentation rate
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
