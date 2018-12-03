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

#include "../shared/examplefw.h"

// Renders into a non-multisample and then a multisample (4x) texture and then
// uses those textures to draw two quads.

static float vertexData[] =
{ // Y up, CCW
  -0.5f,   0.5f,   0.0f, 0.0f,
  -0.5f,  -0.5f,   0.0f, 1.0f,
  0.5f,   -0.5f,   1.0f, 1.0f,
  0.5f,   0.5f,    1.0f, 0.0f
};

static quint16 indexData[] =
{
    0, 1, 2, 0, 2, 3
};

static float triangleData[] =
{ // Y up, CCW
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f,
};

const int UBUFSZ = 68;

struct {
    QRhiBuffer *vbuf = nullptr;
    QRhiBuffer *ibuf = nullptr;
    QRhiBuffer *ubuf = nullptr;
    QRhiTexture *tex = nullptr;
    QRhiTexture *msaaTex = nullptr;
    QRhiSampler *sampler = nullptr;
    QRhiShaderResourceBindings *srbLeft = nullptr;
    QRhiShaderResourceBindings *srbRight = nullptr;
    QRhiGraphicsPipeline *psLeft = nullptr;
    QRhiGraphicsPipeline *psRight = nullptr;
    QRhiResourceUpdateBatch *initialUpdates = nullptr;
    int rightOfs;

    QRhiShaderResourceBindings *triSrb = nullptr;
    QRhiGraphicsPipeline *msaaTriPs = nullptr;
    QRhiGraphicsPipeline *triPs = nullptr;
    QRhiBuffer *triUbuf = nullptr;
    QRhiTextureRenderTarget *msaaRt = nullptr;
    QRhiRenderPassDescriptor *msaaRtRp = nullptr;
    QRhiTextureRenderTarget *rt = nullptr;
    QRhiRenderPassDescriptor *rtRp = nullptr;
} d;

void Window::customInit()
{
    d.vbuf = m_r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData) + sizeof(triangleData));
    d.vbuf->build();

    d.ibuf = m_r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, sizeof(indexData));
    d.ibuf->build();

    d.rightOfs = m_r->ubufAligned(UBUFSZ);
    d.ubuf = m_r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, d.rightOfs + UBUFSZ);
    d.ubuf->build();

    d.tex = m_r->newTexture(QRhiTexture::RGBA8, QSize(512, 512), 1, QRhiTexture::RenderTarget);
    d.tex->build();
    d.msaaTex = m_r->newTexture(QRhiTexture::RGBA8, QSize(512, 512), 4, QRhiTexture::RenderTarget);
    d.msaaTex->build();

    d.initialUpdates = m_r->nextResourceUpdateBatch();
    d.initialUpdates->uploadStaticBuffer(d.vbuf, 0, sizeof(vertexData), vertexData);
    d.initialUpdates->uploadStaticBuffer(d.vbuf, sizeof(vertexData), sizeof(triangleData), triangleData);
    d.initialUpdates->uploadStaticBuffer(d.ibuf, indexData);

    d.sampler = m_r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    d.sampler->build();

    d.srbLeft = m_r->newShaderResourceBindings();
    d.srbLeft->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d.ubuf, 0, UBUFSZ),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, d.tex, d.sampler)
    });
    d.srbLeft->build();

    d.srbRight = m_r->newShaderResourceBindings();
    d.srbRight->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d.ubuf, d.rightOfs, UBUFSZ),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, d.msaaTex, d.sampler)
    });
    d.srbRight->build();

    d.psLeft = m_r->newGraphicsPipeline();
    d.psLeft->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, getShader(QLatin1String(":/texture.vert.qsb")) },
        { QRhiGraphicsShaderStage::Fragment, getShader(QLatin1String(":/texture.frag.qsb")) }
    });
    QRhiVertexInputLayout inputLayout;
    inputLayout.bindings = {
        { 4 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float2, 0 },
        { 0, 1, QRhiVertexInputLayout::Attribute::Float2, 2 * sizeof(float) }
    };
    d.psLeft->setVertexInputLayout(inputLayout);
    d.psLeft->setShaderResourceBindings(d.srbLeft);
    d.psLeft->setRenderPassDescriptor(m_rp);
    d.psLeft->build();

    d.psRight = m_r->newGraphicsPipeline();
    d.psRight->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, getShader(QLatin1String(":/texture.vert.qsb")) },
        { QRhiGraphicsShaderStage::Fragment, getShader(QLatin1String(":/texture_ms4.frag.qsb")) }
    });
    d.psRight->setVertexInputLayout(d.psLeft->vertexInputLayout());
    d.psRight->setShaderResourceBindings(d.srbRight);
    d.psRight->setRenderPassDescriptor(m_rp);
    d.psRight->build();

    // set up the offscreen triangle that goes into tex and msaaTex
    d.triUbuf = m_r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    d.triUbuf->build();
    d.triSrb = m_r->newShaderResourceBindings();
    d.triSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d.triUbuf)
    });
    d.triSrb->build();
    d.rt = m_r->newTextureRenderTarget({ d.tex });
    d.rtRp = d.rt->newCompatibleRenderPassDescriptor();
    d.rt->setRenderPassDescriptor(d.rtRp);
    d.rt->build();
    d.msaaRt = m_r->newTextureRenderTarget({ d.msaaTex });
    d.msaaRtRp = d.msaaRt->newCompatibleRenderPassDescriptor();
    d.msaaRt->setRenderPassDescriptor(d.msaaRtRp);
    d.msaaRt->build();
    d.triPs = m_r->newGraphicsPipeline();
    d.triPs->setSampleCount(1);
    d.triPs->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, getShader(QLatin1String(":/color.vert.qsb")) },
        { QRhiGraphicsShaderStage::Fragment, getShader(QLatin1String(":/color.frag.qsb")) }
    });
    inputLayout.bindings = {
        { 5 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float2, 0 },
        { 0, 1, QRhiVertexInputLayout::Attribute::Float3, 2 * sizeof(float) }
    };
    d.triPs->setVertexInputLayout(inputLayout);
    d.triPs->setShaderResourceBindings(d.triSrb);
    d.triPs->setRenderPassDescriptor(d.rtRp);
    d.triPs->build();
    d.msaaTriPs = m_r->newGraphicsPipeline();
    d.msaaTriPs->setSampleCount(4);
    d.msaaTriPs->setShaderStages(d.triPs->shaderStages());
    d.msaaTriPs->setVertexInputLayout(d.triPs->vertexInputLayout());
    d.msaaTriPs->setShaderResourceBindings(d.triSrb);
    d.msaaTriPs->setRenderPassDescriptor(d.msaaRtRp);
    d.msaaTriPs->build();
}

void Window::customRelease()
{
    if (d.psLeft) {
        d.psLeft->releaseAndDestroy();
        d.psLeft = nullptr;
    }

    if (d.psRight) {
        d.psRight->releaseAndDestroy();
        d.psRight = nullptr;
    }

    if (d.srbLeft) {
        d.srbLeft->releaseAndDestroy();
        d.srbLeft = nullptr;
    }

    if (d.srbRight) {
        d.srbRight->releaseAndDestroy();
        d.srbRight = nullptr;
    }

    if (d.triPs) {
        d.triPs->releaseAndDestroy();
        d.triPs = nullptr;
    }

    if (d.msaaTriPs) {
        d.msaaTriPs->releaseAndDestroy();
        d.msaaTriPs = nullptr;
    }

    if (d.triSrb) {
        d.triSrb->releaseAndDestroy();
        d.triSrb = nullptr;
    }

    if (d.triUbuf) {
        d.triUbuf->releaseAndDestroy();
        d.triUbuf = nullptr;
    }

    if (d.ubuf) {
        d.ubuf->releaseAndDestroy();
        d.ubuf = nullptr;
    }

    if (d.vbuf) {
        d.vbuf->releaseAndDestroy();
        d.vbuf = nullptr;
    }

    if (d.ibuf) {
        d.ibuf->releaseAndDestroy();
        d.ibuf = nullptr;
    }

    if (d.sampler) {
        d.sampler->releaseAndDestroy();
        d.sampler = nullptr;
    }

    if (d.rtRp) {
        d.rtRp->releaseAndDestroy();
        d.rtRp = nullptr;
    }

    if (d.rt) {
        d.rt->releaseAndDestroy();
        d.rt = nullptr;
    }

    if (d.msaaRtRp) {
        d.msaaRtRp->releaseAndDestroy();
        d.msaaRtRp = nullptr;
    }

    if (d.msaaRt) {
        d.msaaRt->releaseAndDestroy();
        d.msaaRt = nullptr;
    }

    if (d.msaaTex) {
        d.msaaTex->releaseAndDestroy();
        d.msaaTex = nullptr;
    }

    if (d.tex) {
        d.tex->releaseAndDestroy();
        d.tex = nullptr;
    }
}

void Window::customRender()
{
    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();
    if (d.initialUpdates) {
        u->merge(d.initialUpdates);
        d.initialUpdates->release();
        d.initialUpdates = nullptr;

        // onscreen ubuf
        QMatrix4x4 mvp = m_proj; // aspect ratio is then wrong when resizing but oh well
        mvp.scale(2);
        mvp.translate(-0.8f, 0, 0);
        u->updateDynamicBuffer(d.ubuf, 0, 64, mvp.constData());
        qint32 flip = 0;
        u->updateDynamicBuffer(d.ubuf, 64, 4, &flip);
        mvp.translate(1.6f, 0, 0);
        u->updateDynamicBuffer(d.ubuf, d.rightOfs, 64, mvp.constData());
        u->updateDynamicBuffer(d.ubuf, d.rightOfs + 64, 4, &flip);

        // offscreen ubuf
        mvp = m_r->clipSpaceCorrMatrix();
        mvp.perspective(45.0f, d.msaaTex->pixelSize().width() / float(d.msaaTex->pixelSize().height()), 0.01f, 1000.0f);
        mvp.translate(0, 0, -2);
        u->updateDynamicBuffer(d.triUbuf, 0, 64, mvp.constData());
        float opacity = 1.0f;
        u->updateDynamicBuffer(d.triUbuf, 64, 4, &opacity);
    }

    // offscreen
    cb->beginPass(d.rt, { 0.5f, 0.2f, 0, 1 }, { 1, 0 });
    cb->setGraphicsPipeline(d.triPs);
    cb->setViewport({ 0, 0, float(d.msaaTex->pixelSize().width()), float(d.msaaTex->pixelSize().height()) });
    cb->setVertexInput(0, { { d.vbuf, sizeof(vertexData) } });
    cb->draw(3);
    cb->endPass();

    // offscreen msaa
    cb->beginPass(d.msaaRt, { 0.5f, 0.2f, 0, 1 }, { 1, 0 }, u);
    cb->setGraphicsPipeline(d.msaaTriPs);
    cb->setViewport({ 0, 0, float(d.msaaTex->pixelSize().width()), float(d.msaaTex->pixelSize().height()) });
    cb->setVertexInput(0, { { d.vbuf, sizeof(vertexData) } });
    cb->draw(3);
    cb->endPass();

    // onscreen
    const QSize outputSizeInPixels = m_sc->effectivePixelSize();
    cb->beginPass(m_sc->currentFrameRenderTarget(), { 0.4f, 0.7f, 0.0f, 1.0f }, { 1.0f, 0 });
    cb->setGraphicsPipeline(d.psLeft); // showing the non-msaa version
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setVertexInput(0, { { d.vbuf, 0 } }, d.ibuf, 0, QRhiCommandBuffer::IndexUInt16);
    cb->drawIndexed(6);
    cb->setGraphicsPipeline(d.psRight); // showing the msaa version, resolved in the shader
    cb->drawIndexed(6);
    cb->endPass();
}
