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

#include "examplewindow.h"

void ExampleWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed() && !m_running) {
        m_running = true;
        init();
        recreateSwapChain();
        render();
    }
}

bool ExampleWindow::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::UpdateRequest:
        render();
        break;

    // With Vulkan the swapchain must be destroyed before the surface as per
    // spec. This is not ideal for us because the surface is managed by the
    // QPlatformWindow which may be gone already when the unexpose comes,
    // making the validation layer scream. The solution is to listen to the
    // PlatformSurface events.
    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            releaseSwapChain();
        break;

    default:
        break;
    }

    return QWindow::event(e);
}

void ExampleWindow::init()
{
    m_sc = m_r->createSwapChain();
    if (!m_sc)
        return;

    m_triRenderer.setRhi(m_r);
    m_triRenderer.setSampleCount(m_sampleCount);
    m_triRenderer.initResources();

    if (!m_triangleOnly) {
        m_triRenderer.setTranslation(QVector3D(0, 0.5f, 0));

        m_quadRenderer.setRhi(m_r);
        m_quadRenderer.setSampleCount(m_sampleCount);
        m_quadRenderer.initResources();
        m_quadRenderer.setTranslation(QVector3D(1.5f, -0.5f, 0));

        m_cubeRenderer.setRhi(m_r);
        m_cubeRenderer.setSampleCount(m_sampleCount);
        m_cubeRenderer.initResources();
        m_cubeRenderer.setTranslation(QVector3D(0, -0.5f, 0));
    }

    if (!m_onScreenOnly) {
        m_liveTexCubeRenderer.setRhi(m_r);
        m_liveTexCubeRenderer.setSampleCount(m_sampleCount);
        m_liveTexCubeRenderer.initResources();
        m_liveTexCubeRenderer.setTranslation(QVector3D(-2.0f, 0, 0));
    }
}

void ExampleWindow::releaseResources()
{
    m_triRenderer.releaseOutputDependentResources();
    m_triRenderer.releaseResources();

    if (!m_triangleOnly) {
        m_quadRenderer.releaseResources();

        m_cubeRenderer.releaseOutputDependentResources();
        m_cubeRenderer.releaseResources();
    }

    if (!m_onScreenOnly) {
        m_liveTexCubeRenderer.releaseOutputDependentResources();
        m_liveTexCubeRenderer.releaseResources();
    }

    delete m_sc;
    m_sc = nullptr;

    delete m_r;
    m_r = nullptr;
}

void ExampleWindow::recreateSwapChain()
{
    if (!m_sc)
        return;

    const QSize outputSize = size() * devicePixelRatio();

    if (!m_ds) {
        m_ds = m_r->createRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                       outputSize,
                                       m_triRenderer.sampleCount(),
                                       QRhiRenderBuffer::ToBeUsedWithSwapChainOnly);
    } else {
        m_ds->release();
        m_ds->pixelSize = outputSize;
    }

    if (!m_ds)
        return;

    m_ds->build();

    m_hasSwapChain = m_sc->build(this, outputSize, 0, m_ds, m_triRenderer.sampleCount());
    m_swapChainChanged = true;
}

void ExampleWindow::releaseSwapChain()
{
    if (m_hasSwapChain) {
        m_hasSwapChain = false;
        m_sc->release();
    }
    if (m_ds) {
        m_ds->releaseAndDestroy();
        m_ds = nullptr;
    }
}

void ExampleWindow::render()
{
    if (!m_hasSwapChain)
        return;

    if (m_sc->requestedSizeInPixels() != size() * devicePixelRatio()) {
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
        if (!m_triangleOnly)
            m_cubeRenderer.releaseOutputDependentResources();
        if (!m_onScreenOnly)
            m_liveTexCubeRenderer.releaseOutputDependentResources();
    }

    if (!m_triRenderer.isPipelineInitialized()) {
        const QRhiRenderPass *rp = m_sc->defaultRenderPass();
        m_triRenderer.initOutputDependentResources(rp, m_sc->effectiveSizeInPixels());
        if (!m_triangleOnly) {
            m_quadRenderer.setPipeline(m_triRenderer.pipeline(), m_sc->effectiveSizeInPixels());
            m_cubeRenderer.initOutputDependentResources(rp, m_sc->effectiveSizeInPixels());
        }
        if (!m_onScreenOnly)
            m_liveTexCubeRenderer.initOutputDependentResources(rp, m_sc->effectiveSizeInPixels());
    }

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    if (!m_onScreenOnly)
        m_liveTexCubeRenderer.queueOffscreenPass(cb);

    QRhi::PassUpdates u;
    u += m_triRenderer.update();
    if (!m_triangleOnly) {
        u += m_quadRenderer.update();
        u += m_cubeRenderer.update();
    }
    if (!m_onScreenOnly)
        u += m_liveTexCubeRenderer.update();

    const QVector4D clearColor(0.4f, 0.7f, 0.0f, 1.0f);
    const QRhiClearValue clearValues[] = {
        clearColor,
        QRhiClearValue(1.0f, 0), // depth, stencil
        clearColor // 3 attachments when using MSAA
    };
    m_r->beginPass(m_sc->currentFrameRenderTarget(), cb, clearValues, u);
    m_triRenderer.queueDraw(cb, m_sc->effectiveSizeInPixels());
    if (!m_triangleOnly) {
        m_quadRenderer.queueDraw(cb, m_sc->effectiveSizeInPixels());
        m_cubeRenderer.queueDraw(cb, m_sc->effectiveSizeInPixels());
    }
    if (!m_onScreenOnly)
        m_liveTexCubeRenderer.queueDraw(cb, m_sc->effectiveSizeInPixels());
    m_r->endPass(cb);

    m_r->endFrame(m_sc);

    requestUpdate(); // render continuously, throttled by the presentation rate
}
