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
#include <QVkRender>

Renderer::Renderer(QVulkanWindow *w)
    : m_window(w)
{
}

void Renderer::initResources()
{
    QVkRender::InitParams params;
    params.inst = m_window->vulkanInstance();
    params.physDev = m_window->physicalDevice();
    params.dev = m_window->device();
    m_r = new QVkRender(params);
}

void Renderer::initSwapChainResources()
{
}

void Renderer::releaseSwapChainResources()
{
}

void Renderer::releaseResources()
{
    delete m_r;
    m_r = nullptr;
}

void Renderer::startNextFrame()
{
    // ### to be removed
#if 1
    VkDevice dev = m_window->device();
    QVulkanDeviceFunctions *devFuncs = m_window->vulkanInstance()->deviceFunctions(dev);
    const QSize sz = m_window->swapChainImageSize();

    VkClearColorValue clearColor = { 0, 0, 0, 1 };
    VkClearDepthStencilValue clearDS = { 1, 0 };
    VkClearValue clearValues[3];
    memset(clearValues, 0, sizeof(clearValues));
    clearValues[0].color = clearValues[2].color = clearColor;
    clearValues[1].depthStencil = clearDS;

    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = m_window->defaultRenderPass();
    rpBeginInfo.framebuffer = m_window->currentFramebuffer();
    rpBeginInfo.renderArea.extent.width = sz.width();
    rpBeginInfo.renderArea.extent.height = sz.height();
    rpBeginInfo.clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
    rpBeginInfo.pClearValues = clearValues;
    VkCommandBuffer cmdBuf = m_window->currentCommandBuffer();
    devFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    devFuncs->vkCmdEndRenderPass(cmdBuf);
#endif

    m_window->frameReady();
    m_window->requestUpdate(); // render continuously, throttled by the presentation rate
}
