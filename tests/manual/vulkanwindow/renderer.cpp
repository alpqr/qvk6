/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "renderer.h"
#include <QVulkanFunctions>
#include <QRhiVulkanInitParams>

const int SAMPLES = 1;

Renderer::Renderer(QVulkanWindow *w)
    : m_window(w)
{
    m_window->setSampleCount(SAMPLES);
}

void Renderer::initResources()
{
    QRhiVulkanInitParams params;
    params.inst = m_window->vulkanInstance();
    params.importExistingDevice = true;
    params.physDev = m_window->physicalDevice();
    params.dev = m_window->device();
    params.cmdPool = m_window->graphicsCommandPool();
    params.gfxQueue = m_window->graphicsQueue();
    m_r = QRhi::create(QRhi::Vulkan, &params);

    m_triRenderer.setRhi(m_r);
    m_triRenderer.setSampleCount(SAMPLES);
    m_triRenderer.initResources();

    m_sc = m_r->createSwapChain();
}

void Renderer::initSwapChainResources()
{
    m_sc->build(m_window); // this just wraps the window's swapchain
    m_triRenderer.initOutputDependentResources(m_sc->defaultRenderPass(), m_sc->effectiveSizeInPixels());
}

void Renderer::releaseSwapChainResources()
{
    m_triRenderer.releaseOutputDependentResources();
    m_sc->release(); // no-op, the real work is done by QVulkanWindow
}

void Renderer::releaseResources()
{
    m_triRenderer.releaseResources();

    delete m_sc;
    m_sc = nullptr;

    delete m_r;
    m_r = nullptr;
}

void Renderer::startNextFrame()
{
    m_r->beginFrame(m_sc);
    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();

    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();
    m_triRenderer.queueResourceUpdates(u);

    QRhiClearValue colorClear(QVector4D(0.4f, 0.7f, 0.0f, 1.0f));
    QRhiClearValue dsClear(1.0f, 0);
    m_r->beginPass(m_sc->currentFrameRenderTarget(), cb, &colorClear, &dsClear, u);
    m_triRenderer.queueDraw(cb, m_sc->effectiveSizeInPixels());
    m_r->endPass(cb);

    m_r->endFrame(m_sc);

    m_window->frameReady();
    m_window->requestUpdate(); // render continuously, throttled by the presentation rate
}
