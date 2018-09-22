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

const int SAMPLES = 1;

void ExampleWindow::exposeEvent(QExposeEvent *)
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
    //
    // Then there's a similar problem with any OpenGL resource: trying to do
    // makeCurrent from a QWindow dtor (or afterwards) is futile: the
    // underlying native window is long gone by then. So act early and do also
    // releaseResources() from here.
    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            m_inited = false;
            releaseSwapChain();
            releaseResources();
        }
        break;

    default:
        break;
    }

    return QWindow::event(e);
}

void ExampleWindow::init()
{
    m_triRenderer.setRhi(m_r);
    m_triRenderer.setSampleCount(SAMPLES);
    m_triRenderer.initResources();
    m_triRenderer.setTranslation(QVector3D(0, 0.5f, 0));

    m_cubeRenderer.setRhi(m_r);
    m_cubeRenderer.setSampleCount(SAMPLES);
    m_cubeRenderer.initResources();
    m_cubeRenderer.setTranslation(QVector3D(0, -0.5f, 0));

    m_liveTexCubeRenderer.setRhi(m_r);
    m_liveTexCubeRenderer.setSampleCount(SAMPLES);
    m_liveTexCubeRenderer.initResources();
    m_liveTexCubeRenderer.setTranslation(QVector3D(-2.0f, 0, 0));

    m_sc = m_r->createSwapChain();
}

void ExampleWindow::releaseResources()
{
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
}

void ExampleWindow::recreateSwapChain()
{
    const QSize outputSize = size() * devicePixelRatio();

    if (!m_ds) {
        m_ds = m_r->createRenderBuffer(QRhiRenderBuffer::DepthStencil, outputSize, m_triRenderer.sampleCount());
    } else {
        m_ds->release();
        m_ds->pixelSize = outputSize;
    }
    m_ds->build();

    m_hasSwapChain = m_sc->build(this, outputSize, QRhiSwapChain::UseDepthStencil, m_ds, m_triRenderer.sampleCount());
    m_swapChainChanged = true;
}

void ExampleWindow::releaseSwapChain()
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

void ExampleWindow::render()
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
