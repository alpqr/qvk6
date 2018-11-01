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

#include "examplewindow.h"

void ExampleWindow::exposeEvent(QExposeEvent *)
{
    // You never know how Vulkan behaves today - at some point it started
    // requiring a swapchain recreate on unexpose-expose on Windows at least
    // (where unexpose comes when e.g. minimizing the window). Manage this.
    if (!isExposed() && m_running)
        m_notExposed = true;

    if (isExposed() && m_running && m_notExposed) {
        m_notExposed = false;
        m_newlyExposed = true;
        render();
    }

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
    m_sc = m_r->newSwapChain();
    if (!m_sc)
        return;

    // allow depth-stencil, although we do not actually enable depth test/write for the triangle
    m_ds = m_r->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                QSize(), // we don't know the size yet, this is fine
                                m_sampleCount,
                                QRhiRenderBuffer::ToBeUsedWithSwapChainOnly);
    m_sc->setWindow(this);
    m_sc->setDepthStencil(m_ds);
    m_sc->setSampleCount(m_sampleCount);
    m_scrp = m_sc->newCompatibleRenderPassDescriptor();
    m_sc->setRenderPassDescriptor(m_scrp);

    m_triRenderer.setRhi(m_r);
    m_triRenderer.setSampleCount(m_sampleCount);
    m_triRenderer.initResources(m_scrp);

    if (!m_triangleOnly) {
        m_triRenderer.setTranslation(QVector3D(0, 0.5f, 0));

        m_quadRenderer.setRhi(m_r);
        m_quadRenderer.setSampleCount(m_sampleCount);
        m_quadRenderer.setPipeline(m_triRenderer.pipeline());
        m_quadRenderer.initResources(m_scrp);
        m_quadRenderer.setTranslation(QVector3D(1.5f, -0.5f, 0));

        m_cubeRenderer.setRhi(m_r);
        m_cubeRenderer.setSampleCount(m_sampleCount);
        m_cubeRenderer.initResources(m_scrp);
        m_cubeRenderer.setTranslation(QVector3D(0, -0.5f, 0));
    }

    if (!m_onScreenOnly) {
        m_liveTexCubeRenderer.setRhi(m_r);
        m_liveTexCubeRenderer.setSampleCount(m_sampleCount);
        m_liveTexCubeRenderer.initResources(m_scrp);
        m_liveTexCubeRenderer.setTranslation(QVector3D(-2.0f, 0, 0));
    }
}

void ExampleWindow::releaseResources()
{
    m_triRenderer.releaseResources();

    if (!m_triangleOnly) {
        m_quadRenderer.releaseResources();
        m_cubeRenderer.releaseResources();
    }

    if (!m_onScreenOnly)
        m_liveTexCubeRenderer.releaseResources();

    if (m_scrp) {
        m_scrp->releaseAndDestroy();
        m_scrp = nullptr;
    }

    if (m_ds) {
        m_ds->releaseAndDestroy();
        m_ds = nullptr;
    }

    delete m_sc;
    m_sc = nullptr;

    delete m_r;
    m_r = nullptr;
}

void ExampleWindow::recreateSwapChain()
{
    const QSize outputSize = size() * devicePixelRatio();

    m_ds->setPixelSize(outputSize);
    m_ds->build(); // == m_ds->release(); m_ds->build();

    m_sc->setRequestedPixelSize(outputSize);
    m_hasSwapChain = m_sc->buildOrResize();
    m_resizedSwapChain = true;
}

void ExampleWindow::releaseSwapChain()
{
    if (m_hasSwapChain) {
        m_hasSwapChain = false;
        m_sc->release();
    }
}

void ExampleWindow::render()
{
    if (!m_hasSwapChain || m_notExposed)
        return;

    if (m_sc->requestedPixelSize() != size() * devicePixelRatio() || m_newlyExposed) {
        recreateSwapChain();
        if (!m_hasSwapChain)
            return;
        m_newlyExposed = false;
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

    const QSize outputSize = m_sc->effectivePixelSize();
    if (m_resizedSwapChain) {
        m_resizedSwapChain = false;
        m_triRenderer.resize(outputSize);
        if (!m_triangleOnly) {
            m_quadRenderer.resize(outputSize);
            m_cubeRenderer.resize(outputSize);
        }
        if (!m_onScreenOnly)
            m_liveTexCubeRenderer.resize(outputSize);
    }

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    if (!m_onScreenOnly)
        m_liveTexCubeRenderer.queueOffscreenPass(cb);

    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();
    m_triRenderer.queueResourceUpdates(u);
    if (!m_triangleOnly) {
        m_quadRenderer.queueResourceUpdates(u);
        m_cubeRenderer.queueResourceUpdates(u);
    }
    if (!m_onScreenOnly)
        m_liveTexCubeRenderer.queueResourceUpdates(u);

    m_r->beginPass(m_sc->currentFrameRenderTarget(), cb, { 0.4f, 0.7f, 0.0f, 1.0f }, { 1.0f, 0 }, u);
    m_triRenderer.queueDraw(cb, outputSize);
    if (!m_triangleOnly) {
        m_quadRenderer.queueDraw(cb, outputSize);
        m_cubeRenderer.queueDraw(cb, outputSize);
    }
    if (!m_onScreenOnly)
        m_liveTexCubeRenderer.queueDraw(cb, outputSize);
    m_r->endPass(cb);

    m_r->endFrame(m_sc);

    requestUpdate(); // render continuously, throttled by the presentation rate
}
