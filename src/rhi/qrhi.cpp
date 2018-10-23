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

#include "qrhi_p.h"
#include <qmath.h>

#include "qrhigles2_p.h"
#if QT_CONFIG(vulkan)
#include "qrhivulkan_p.h"
#endif
#ifdef Q_OS_WIN
#include "qrhid3d11_p.h"
#endif
#ifdef Q_OS_DARWIN
#include "qrhimetal_p.h"
#endif

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
                         Filter magFilter_, Filter minFilter_, Filter mipmapMode_,
                         AddressMode u_, AddressMode v_, AddressMode w_)
    : QRhiResource(rhi),
      magFilter(magFilter_), minFilter(minFilter_), mipmapMode(mipmapMode_),
      addressU(u_), addressV(v_), addressW(w_)
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
        int binding_, StageFlags stage_, QRhiBuffer *buf_)
{
    Binding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = UniformBuffer;
    b.ubuf.buf = buf_;
    b.ubuf.offset = 0;
    b.ubuf.maybeSize = 0; // entire buffer
    return b;
}

QRhiShaderResourceBindings::Binding QRhiShaderResourceBindings::Binding::uniformBuffer(
        int binding_, StageFlags stage_, QRhiBuffer *buf_, int offset_, int size_)
{
    Q_ASSERT(size_ > 0);
    Binding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = UniformBuffer;
    b.ubuf.buf = buf_;
    b.ubuf.offset = offset_;
    b.ubuf.maybeSize = size_;
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
    qDeleteAll(resUpdPool);
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
#if QT_CONFIG(vulkan)
        QRhi *r = new QRhi;
        r->d = new QRhiVulkan(params);
        return r;
#else
        qWarning("This build of Qt has no Vulkan support");
        break;
#endif
    }
    case OpenGLES2:
    {
        QRhi *r = new QRhi;
        r->d = new QRhiGles2(params);
        return r;
    }
    case D3D11:
    {
#ifdef Q_OS_WIN
        QRhi *r = new QRhi;
        r->d = new QRhiD3D11(params);
        return r;
#else
        qWarning("This platform has no Direct3D 11 support");
        break;
#endif
    }
    case Metal:
    {
#ifdef Q_OS_DARWIN
        QRhi *r = new QRhi;
        r->d = new QRhiMetal(params);
        return r;
#else
        qWarning("This platform has no Metal support");
        break;
#endif
    }
    default:
        break;
    }
    return nullptr;
}

QRhiResourceUpdateBatch::QRhiResourceUpdateBatch(QRhiImplementation *rhi)
    : d(new QRhiResourceUpdateBatchPrivate)
{
    d->q = this;
    d->rhi = rhi;
}

QRhiResourceUpdateBatch::~QRhiResourceUpdateBatch()
{
    delete d;
}

void QRhiResourceUpdateBatch::release()
{
    d->free();
}

void QRhiResourceUpdateBatch::updateDynamicBuffer(QRhiBuffer *buf, int offset, int size, const void *data)
{
    d->dynamicBufferUpdates.append({ buf, offset, size, data });
}

void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, const void *data)
{
    d->staticBufferUploads.append({ buf, data });
}

void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const TextureUploadDescription &desc)
{
    d->textureUploads.append({ tex, desc });
}

void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QImage &image)
{
    uploadTexture(tex, {{{{{ image }}}}});
}

QRhiResourceUpdateBatch *QRhi::nextResourceUpdateBatch()
{
    auto nextFreeBatch = [this]() -> QRhiResourceUpdateBatch * {
        for (int i = 0, ie = d->resUpdPoolMap.count(); i != ie; ++i) {
            if (!d->resUpdPoolMap.testBit(i)) {
                d->resUpdPoolMap.setBit(i);
                QRhiResourceUpdateBatch *u = d->resUpdPool[i];
                QRhiResourceUpdateBatchPrivate::get(u)->poolIndex = i;
                return u;
            }
        }
        return nullptr;
    };

    QRhiResourceUpdateBatch *u = nextFreeBatch();
    if (!u) {
        const int oldSize = d->resUpdPool.count();
        const int newSize = oldSize + 4;
        d->resUpdPool.resize(newSize);
        d->resUpdPoolMap.resize(newSize);
        for (int i = oldSize; i < newSize; ++i)
            d->resUpdPool[i] = new QRhiResourceUpdateBatch(d);
        u = nextFreeBatch();
        Q_ASSERT(u);
    }

    return u;
}

void QRhiResourceUpdateBatchPrivate::free()
{
    Q_ASSERT(poolIndex >= 0 && rhi->resUpdPool[poolIndex] == q);

    dynamicBufferUpdates.clear();
    staticBufferUploads.clear();
    textureUploads.clear();

    rhi->resUpdPoolMap.clearBit(poolIndex);
    poolIndex = -1;
}

int QRhi::ubufAligned(int v) const
{
    const int byteAlign = ubufAlignment();
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

int QRhi::mipLevelsForSize(const QSize &size) const
{
    return qCeil(std::log2(qMax(size.width(), size.height()))) + 1;
}

QSize QRhi::sizeForMipLevel(int mipLevel, const QSize &baseLevelSize) const
{
    const int w = qFloor(double(qMax(1, baseLevelSize.width() >> mipLevel)));
    const int h = qFloor(double(qMax(1, baseLevelSize.height() >> mipLevel)));
    return QSize(w, h);
}

bool QRhi::isYUpInFramebuffer() const
{
    return d->isYUpInFramebuffer();
}

QMatrix4x4 QRhi::clipSpaceCorrMatrix() const
{
    return d->clipSpaceCorrMatrix();
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
                                 QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w)
{
    return d->createSampler(magFilter, minFilter, mipmapMode, u, v, w);
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
                     const QRhiColorClearValue &colorClearValue,
                     const QRhiDepthStencilClearValue &depthStencilClearValue,
                     QRhiResourceUpdateBatch *resourceUpdates)
{
    d->beginPass(rt, cb, colorClearValue, depthStencilClearValue, resourceUpdates);
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
