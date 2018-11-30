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
#include "../shared/cube.h"
#include <QPainter>

struct {
    QRhiBuffer *vbuf = nullptr;
    QRhiBuffer *ubuf = nullptr;
    QRhiTexture *tex = nullptr;
    QRhiSampler *sampler = nullptr;
    QRhiShaderResourceBindings *srb = nullptr;
    QRhiGraphicsPipeline *ps = nullptr;

    float rotation = 0;
    QRhiResourceUpdateBatch *initialUpdates = nullptr;
    int frameCount = 0;
    QImage customImage;
} d;

void Window::customInit()
{
    d.vbuf = m_r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(cube));
    d.vbuf->build();

    d.ubuf = m_r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    d.ubuf->build();

    QImage baseImage(QLatin1String(":/qt256.png"));
    d.tex = m_r->newTexture(QRhiTexture::RGBA8, baseImage.size());
    d.tex->build();

    // As an alternative to what some of the other examples do, prepare an
    // update batch right here instead of relying on vbufReady and similar flags.
    d.initialUpdates = m_r->nextResourceUpdateBatch();
    d.initialUpdates->uploadStaticBuffer(d.vbuf, cube);
    qint32 flip = 0;
    d.initialUpdates->updateDynamicBuffer(d.ubuf, 64, 4, &flip);
    d.initialUpdates->uploadTexture(d.tex, baseImage);

    d.sampler = m_r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    d.sampler->build();

    d.srb = m_r->newShaderResourceBindings();
    d.srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d.ubuf),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, d.tex, d.sampler)
    });
    d.srb->build();

    d.ps = m_r->newGraphicsPipeline();

    d.ps->setDepthTest(true);
    d.ps->setDepthWrite(true);
    d.ps->setDepthOp(QRhiGraphicsPipeline::Less);

    d.ps->setCullMode(QRhiGraphicsPipeline::Back);
    d.ps->setFrontFace(QRhiGraphicsPipeline::CCW);

    const QBakedShader vs = getShader(QLatin1String(":/texture.vert.qsb"));
    if (!vs.isValid())
        qFatal("Failed to load shader pack (vertex)");
    const QBakedShader fs = getShader(QLatin1String(":/texture.frag.qsb"));
    if (!fs.isValid())
        qFatal("Failed to load shader pack (fragment)");

    d.ps->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, vs },
        { QRhiGraphicsShaderStage::Fragment, fs }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.bindings = {
        { 3 * sizeof(float) },
        { 2 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float3, 0 },
        { 1, 1, QRhiVertexInputLayout::Attribute::Float2, 0 }
    };

    d.ps->setVertexInputLayout(inputLayout);
    d.ps->setShaderResourceBindings(d.srb);
    d.ps->setRenderPassDescriptor(m_rp);

    d.ps->build();

    d.customImage = QImage(128, 64, QImage::Format_RGBA8888);
    d.customImage.fill(Qt::red);
    QPainter painter(&d.customImage);
    painter.drawText(5, 25, "Hello world");
    painter.end();
}

void Window::customRelease()
{
    if (d.ps) {
        d.ps->releaseAndDestroy();
        d.ps = nullptr;
    }

    if (d.srb) {
        d.srb->releaseAndDestroy();
        d.srb = nullptr;
    }

    if (d.ubuf) {
        d.ubuf->releaseAndDestroy();
        d.ubuf = nullptr;
    }

    if (d.vbuf) {
        d.vbuf->releaseAndDestroy();
        d.vbuf = nullptr;
    }

    if (d.sampler) {
        d.sampler->releaseAndDestroy();
        d.sampler = nullptr;
    }

    if (d.tex) {
        d.tex->releaseAndDestroy();
        d.tex = nullptr;
    }
}

void Window::customRender()
{
    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();

    // take the initial set of updates, if this is the first frame
    if (d.initialUpdates) {
        u->merge(d.initialUpdates);
        d.initialUpdates->release();
        d.initialUpdates = nullptr;
    }

    d.rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.scale(0.5f);
    mvp.rotate(d.rotation, 0, 1, 0);
    u->updateDynamicBuffer(d.ubuf, 0, 64, mvp.constData());

    // After some time, partially change the texture.
    if (d.frameCount > 100) {
        QRhiTextureUploadDescription desc;
        QRhiTextureUploadDescription::Layer layer;
        QRhiTextureUploadDescription::Layer::MipLevel mipDesc;

        mipDesc.image = d.customImage;
        // The image here is smaller than the original. Use a non-zero position
        // to make it more interesting.
        mipDesc.destinationTopLeft = QPointF(100, 20);

        layer.mipImages.append(mipDesc);
        desc.layers.append(layer);
        u->uploadTexture(d.tex, desc);
    }

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    const QSize outputSizeInPixels = m_sc->effectivePixelSize();

    m_r->beginPass(m_sc->currentFrameRenderTarget(), cb, { 0.4f, 0.7f, 0.0f, 1.0f }, { 1.0f, 0 }, u);

    m_r->setGraphicsPipeline(cb, d.ps);
    m_r->setViewport(cb, { 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    m_r->setVertexInput(cb, 0, { { d.vbuf, 0 }, { d.vbuf, 36 * 3 * sizeof(float) } });
    m_r->draw(cb, 36);

    m_r->endPass(cb);

    d.frameCount += 1;
}
