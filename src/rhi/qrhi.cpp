/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt RHI module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qrhi.h"
#include "qrhivulkan_p.h"
#include "qrhigles2_p.h"

QT_BEGIN_NAMESPACE

QRhiResource::QRhiResource(QRhiImplementation *rhi_)
    : rhi(rhi_)
{
}

QRhiResource::~QRhiResource()
{
}

void QRhiResource::releaseAndDestroy()
{
    release();
    delete this;
}

QRhiBuffer::QRhiBuffer(QRhiImplementation *rhi, Type type_, UsageFlags usage_, int size_)
    : QRhiResource(rhi),
      type(type_), usage(usage_), size(size_)
{
}

QRhiRenderBuffer::QRhiRenderBuffer(QRhiImplementation *rhi, Type type_, const QSize &pixelSize_,
                                   int sampleCount_, Hints hints_)
    : QRhiResource(rhi),
      type(type_), pixelSize(pixelSize_), sampleCount(sampleCount_), hints(hints_)
{
}

QRhiTexture::QRhiTexture(QRhiImplementation *rhi, Format format_, const QSize &pixelSize_, Flags flags_)
    : QRhiResource(rhi),
      format(format_), pixelSize(pixelSize_), flags(flags_)
{
}

QRhiSampler::QRhiSampler(QRhiImplementation *rhi,
                         Filter magFilter_, Filter minFilter_, Filter mipmapMode_, AddressMode u_, AddressMode v_)
    : QRhiResource(rhi),
      magFilter(magFilter_), minFilter(minFilter_), mipmapMode(mipmapMode_),
      addressU(u_), addressV(v_)
{
}

QRhiRenderPass::QRhiRenderPass(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiRenderTarget::QRhiRenderTarget(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiReferenceRenderTarget::QRhiReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiRenderTarget(rhi)
{
}

QRhiTextureRenderTarget::QRhiTextureRenderTarget(QRhiImplementation *rhi,
                                                 QRhiTexture *texture_, Flags flags_)
    : QRhiRenderTarget(rhi),
      texture(texture_), depthTexture(nullptr), depthStencilBuffer(nullptr), flags(flags_)
{
}

QRhiTextureRenderTarget::QRhiTextureRenderTarget(QRhiImplementation *rhi,
                                                 QRhiTexture *texture_, QRhiRenderBuffer *depthStencilBuffer_, Flags flags_)
    : QRhiRenderTarget(rhi),
      texture(texture_), depthTexture(nullptr), depthStencilBuffer(depthStencilBuffer_), flags(flags_)
{
}

QRhiTextureRenderTarget::QRhiTextureRenderTarget(QRhiImplementation *rhi,
                                                 QRhiTexture *texture_, QRhiTexture *depthTexture_, Flags flags_)
    : QRhiRenderTarget(rhi),
      texture(texture_), depthTexture(depthTexture_), depthStencilBuffer(nullptr), flags(flags_)
{
}

QRhiShaderResourceBindings::QRhiShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiShaderResourceBindings::Binding QRhiShaderResourceBindings::Binding::uniformBuffer(
        int binding_, StageFlags stage_, QRhiBuffer *buf_, int offset_, int size_)
{
    Binding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = UniformBuffer;
    b.ubuf.buf = buf_;
    b.ubuf.offset = offset_;
    b.ubuf.size = size_;
    return b;
}

QRhiShaderResourceBindings::Binding QRhiShaderResourceBindings::Binding::sampledTexture(
        int binding_, StageFlags stage_, QRhiTexture *tex_, QRhiSampler *sampler_)
{
    Binding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = SampledTexture;
    b.stex.tex = tex_;
    b.stex.sampler = sampler_;
    return b;
}

QRhiGraphicsPipeline::QRhiGraphicsPipeline(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiSwapChain::QRhiSwapChain(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiCommandBuffer::QRhiCommandBuffer(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiImplementation::~QRhiImplementation()
{
}

QRhi::QRhi()
{
}

QRhi::~QRhi()
{
    delete d;
}

QRhi *QRhi::create(Implementation impl, QRhiInitParams *params)
{
    switch (impl) {
    case Vulkan:
    {
        QRhi *r = new QRhi;
        r->d = new QRhiVulkan(params);
        return r;
    }
    case OpenGLES2:
    {
        QRhi *r = new QRhi;
        r->d = new QRhiGles2(params);
        return r;
    }
    default:
        break;
    }
    return nullptr;
}

QRhi::PassUpdates &QRhi::PassUpdates::operator+=(const QRhi::PassUpdates &u)
{
    dynamicBufferUpdates += u.dynamicBufferUpdates;
    staticBufferUploads += u.staticBufferUploads;
    textureUploads += u.textureUploads;
    return *this;
}

int QRhi::ubufAligned(int v) const
{
    const int byteAlign = ubufAlignment();
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

QMatrix4x4 QRhi::openGLVertexCorrectionMatrix() const
{
    return d->openGLVertexCorrectionMatrix();
}

bool QRhi::isYUpInFramebuffer() const
{
    return d->isYUpInFramebuffer();
}

QRhiGraphicsPipeline *QRhi::createGraphicsPipeline()
{
    return d->createGraphicsPipeline();
}

QRhiShaderResourceBindings *QRhi::createShaderResourceBindings()
{
    return d->createShaderResourceBindings();
}

QRhiBuffer *QRhi::createBuffer(QRhiBuffer::Type type,
                               QRhiBuffer::UsageFlags usage,
                               int size)
{
    return d->createBuffer(type, usage, size);
}

QRhiRenderBuffer *QRhi::createRenderBuffer(QRhiRenderBuffer::Type type,
                                           const QSize &pixelSize,
                                           int sampleCount,
                                           QRhiRenderBuffer::Hints hints)
{
    return d->createRenderBuffer(type, pixelSize, sampleCount, hints);
}

QRhiTexture *QRhi::createTexture(QRhiTexture::Format format,
                                 const QSize &pixelSize,
                                 QRhiTexture::Flags flags)
{
    return d->createTexture(format, pixelSize, flags);
}

QRhiSampler *QRhi::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                 QRhiSampler::Filter mipmapMode,
                                 QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v)
{
    return d->createSampler(magFilter, minFilter, mipmapMode, u, v);
}

QRhiTextureRenderTarget *QRhi::createTextureRenderTarget(QRhiTexture *texture,
                                                         QRhiTextureRenderTarget::Flags flags)
{
    return d->createTextureRenderTarget(texture, flags);
}

QRhiTextureRenderTarget *QRhi::createTextureRenderTarget(QRhiTexture *texture,
                                                         QRhiRenderBuffer *depthStencilBuffer,
                                                         QRhiTextureRenderTarget::Flags flags)
{
    return d->createTextureRenderTarget(texture, depthStencilBuffer, flags);
}

QRhiTextureRenderTarget *QRhi::createTextureRenderTarget(QRhiTexture *texture,
                                                         QRhiTexture *depthTexture,
                                                         QRhiTextureRenderTarget::Flags flags)
{
    return d->createTextureRenderTarget(texture, depthTexture, flags);
}

QRhiSwapChain *QRhi::createSwapChain()
{
    return d->createSwapChain();
}

QRhi::FrameOpResult QRhi::beginFrame(QRhiSwapChain *swapChain)
{
    return d->beginFrame(swapChain);
}

QRhi::FrameOpResult QRhi::endFrame(QRhiSwapChain *swapChain)
{
    return d->endFrame(swapChain);
}

void QRhi::beginPass(QRhiRenderTarget *rt,
                     QRhiCommandBuffer *cb,
                     const QRhiClearValue *clearValues,
                     const QRhi::PassUpdates &updates)
{
    d->beginPass(rt, cb, clearValues, updates);
}

void QRhi::endPass(QRhiCommandBuffer *cb)
{
    d->endPass(cb);
}

void QRhi::setGraphicsPipeline(QRhiCommandBuffer *cb,
                               QRhiGraphicsPipeline *ps,
                               QRhiShaderResourceBindings *srb)
{
    d->setGraphicsPipeline(cb, ps, srb);
}

void QRhi::setVertexInput(QRhiCommandBuffer *cb,
                          int startBinding, const QVector<VertexInput> &bindings,
                          QRhiBuffer *indexBuf, quint32 indexOffset,
                          IndexFormat indexFormat)
{
    d->setVertexInput(cb, startBinding, bindings, indexBuf, indexOffset, indexFormat);
}

void QRhi::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
    d->setViewport(cb, viewport);
}

void QRhi::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
    d->setScissor(cb, scissor);
}

void QRhi::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
    d->setBlendConstants(cb, c);
}

void QRhi::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
    d->setStencilRef(cb, refValue);
}

void QRhi::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    d->draw(cb, vertexCount, instanceCount, firstVertex, firstInstance);
}

void QRhi::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                       quint32 instanceCount, quint32 firstIndex,
                       qint32 vertexOffset, quint32 firstInstance)
{
    d->drawIndexed(cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

QVector<int> QRhi::supportedSampleCounts() const
{
    return d->supportedSampleCounts();
}

int QRhi::ubufAlignment() const
{
    return d->ubufAlignment();
}

QT_END_NAMESPACE
