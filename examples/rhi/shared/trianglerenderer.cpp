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

#include "trianglerenderer.h"
#include <QFile>
#include <QBakedShader>

//#define VBUF_IS_DYNAMIC

static float vertexData[] = { // Y up (note m_proj), CCW
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f
};

static QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

void TriangleRenderer::initResources()
{
#ifdef VBUF_IS_DYNAMIC
    m_vbuf = m_r->createBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, sizeof(vertexData));
#else
    m_vbuf = m_r->createBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
#endif
    m_vbuf->build();
    m_vbufReady = false;

    m_ubuf = m_r->createBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    m_ubuf->build();

    m_srb = m_r->createShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_ubuf)
    });
    m_srb->build();
}

// ### the following limitation may be lifted later on, the ps won't need to be created here then
// the ps depends on the renderpass -> so it is tied to the swapchain.
// on the other hand, srb and buffers are referenced from the ps but can be reused.
void TriangleRenderer::initOutputDependentResources(const QRhiRenderPass *rp, const QSize &pixelSize)
{
    m_ps = m_r->createGraphicsPipeline();

    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend; // convenient defaults...
    premulAlphaBlend.enable = true;
    QVector<QRhiGraphicsPipeline::TargetBlend> rtblends;
    for (int i = 0; i < m_colorAttCount; ++i)
        rtblends << premulAlphaBlend;

    m_ps->setTargetBlends(rtblends);
    m_ps->setSampleCount(m_sampleCount);

    if (m_depthWrite) { // TriangleOnCube may want to exercise this
        m_ps->setDepthTest(true);
        m_ps->setDepthOp(QRhiGraphicsPipeline::Always);
        m_ps->setDepthWrite(true);
    }

    QBakedShader vs = getShader(QLatin1String(":/color.vert.qsb"));
    Q_ASSERT(vs.isValid());
    QBakedShader fs = getShader(QLatin1String(":/color.frag.qsb"));
    Q_ASSERT(fs.isValid());
    m_ps->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, vs },
        { QRhiGraphicsShaderStage::Fragment, fs }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.bindings = {
        { 7 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float2, 0 },
        { 0, 1, QRhiVertexInputLayout::Attribute::Float3, 2 * sizeof(float) }
    };

    m_ps->setVertexInputLayout(inputLayout);
    m_ps->setShaderResourceBindings(m_srb);
    m_ps->setRenderPass(rp);

    m_ps->build();

    m_proj = m_r->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, pixelSize.width() / (float) pixelSize.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);
}

void TriangleRenderer::releaseResources()
{
    if (m_srb) {
        m_srb->releaseAndDestroy();
        m_srb = nullptr;
    }

    if (m_ubuf) {
        m_ubuf->releaseAndDestroy();
        m_ubuf = nullptr;
    }

    if (m_vbuf) {
        m_vbuf->releaseAndDestroy();
        m_vbuf = nullptr;
    }
}

void TriangleRenderer::releaseOutputDependentResources()
{
    if (m_ps) {
        m_ps->releaseAndDestroy();
        m_ps = nullptr;
    }
}

void TriangleRenderer::queueResourceUpdates(QRhiResourceUpdateBatch *resourceUpdates)
{
#if 0
    static int messWithBufferTrigger = 0;
    // recreate the underlying VkBuffer every second frame
    // to exercise setGraphicsPipeline's built-in smartness
    if (!(messWithBufferTrigger & 1)) {
        m_ubuf->release();
        m_ubuf->build();
    }
    ++messWithBufferTrigger;
#endif

    if (!m_vbufReady) {
        m_vbufReady = true;
#ifdef VBUF_IS_DYNAMIC
        resourceUpdates->updateDynamicBuffer(m_vbuf, 0, m_vbuf->size, vertexData);
#else
        resourceUpdates->uploadStaticBuffer(m_vbuf, vertexData);
#endif
    }

    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.translate(m_translation);
    mvp.scale(m_scale);
    mvp.rotate(m_rotation, 0, 1, 0);
    resourceUpdates->updateDynamicBuffer(m_ubuf, 0, 64, mvp.constData());

    m_opacity += m_opacityDir * 0.005f;
    if (m_opacity < 0.0f || m_opacity > 1.0f) {
        m_opacityDir *= -1;
        m_opacity = qBound(0.0f, m_opacity, 1.0f);
    }
    resourceUpdates->updateDynamicBuffer(m_ubuf, 64, 4, &m_opacity);
}

void TriangleRenderer::queueDraw(QRhiCommandBuffer *cb, const QSize &outputSizeInPixels)
{
    m_r->setGraphicsPipeline(cb, m_ps);
    m_r->setViewport(cb, QRhiViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->setVertexInput(cb, 0, { { m_vbuf, 0 } });
    m_r->draw(cb, 3);
}
