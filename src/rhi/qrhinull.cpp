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

#include "qrhinull_p.h"
#include <qmath.h>

QT_BEGIN_NAMESPACE

/*!
    \class QRhiNullInitParams
    \inmodule QtRhi
    \brief Null backend specific initialization parameters.

    A Null QRhi needs no special parameters for initialization.

    \badcode
        QRhiNullInitParams params;
        rhi = QRhi::create(QRhi::Null, &params);
    \endcode

    The Null backend does not issue any graphics calls and creates no
    resources. All QRhi operations will succeed as normal so applications can
    still be run, albeit potentially at an unthrottled speed, depending on
    their frame rendering strategy. The backend reports resources to
    QRhiProfiler as usual.
 */

/*!
    \class QRhiNullNativeHandles
    \inmodule QtRhi
    \brief Empty.
 */

/*!
    \class QRhiNullTextureNativeHandles
    \inmodule QtRhi
    \brief Empty.
 */

QRhiNull::QRhiNull(QRhiInitParams *params)
{
    Q_UNUSED(params);
}

bool QRhiNull::create(QRhi::Flags flags)
{
    Q_UNUSED(flags);
    return true;
}

void QRhiNull::destroy()
{
}

QVector<int> QRhiNull::supportedSampleCounts() const
{
    return { 1 };
}

QRhiSwapChain *QRhiNull::createSwapChain()
{
    return new QNullSwapChain(this);
}

QRhiBuffer *QRhiNull::createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, int size)
{
    return new QNullBuffer(this, type, usage, size);
}

int QRhiNull::ubufAlignment() const
{
    return 256;
}

bool QRhiNull::isYUpInFramebuffer() const
{
    return true;
}

QMatrix4x4 QRhiNull::clipSpaceCorrMatrix() const
{
    return QMatrix4x4(); // identity
}

bool QRhiNull::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const
{
    Q_UNUSED(format);
    Q_UNUSED(flags);
    return true;
}

bool QRhiNull::isFeatureSupported(QRhi::Feature feature) const
{
    Q_UNUSED(feature);
    return true;
}

int QRhiNull::resourceSizeLimit(QRhi::ResourceSizeLimit limit) const
{
    switch (limit) {
    case QRhi::TextureSizeMin:
        return 1;
    case QRhi::TextureSizeMax:
        return 16384;
    default:
        Q_UNREACHABLE();
        return 0;
    }
}

const QRhiNativeHandles *QRhiNull::nativeHandles()
{
    return &nativeHandlesStruct;
}

QRhiRenderBuffer *QRhiNull::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize,
                                                int sampleCount, QRhiRenderBuffer::Flags flags)
{
    return new QNullRenderBuffer(this, type, pixelSize, sampleCount, flags);
}

QRhiTexture *QRhiNull::createTexture(QRhiTexture::Format format, const QSize &pixelSize,
                                      int sampleCount, QRhiTexture::Flags flags)
{
    return new QNullTexture(this, format, pixelSize, sampleCount, flags);
}

QRhiSampler *QRhiNull::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                      QRhiSampler::Filter mipmapMode,
                                      QRhiSampler::AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w)
{
    return new QNullSampler(this, magFilter, minFilter, mipmapMode, u, v, w);
}

QRhiTextureRenderTarget *QRhiNull::createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QNullTextureRenderTarget(this, desc, flags);
}

QRhiGraphicsPipeline *QRhiNull::createGraphicsPipeline()
{
    return new QNullGraphicsPipeline(this);
}

QRhiShaderResourceBindings *QRhiNull::createShaderResourceBindings()
{
    return new QNullShaderResourceBindings(this);
}

void QRhiNull::setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb)
{
    Q_UNUSED(cb);
    Q_UNUSED(ps);
    Q_UNUSED(srb);
}

void QRhiNull::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<QRhiCommandBuffer::VertexInput> &bindings,
                               QRhiBuffer *indexBuf, quint32 indexOffset, QRhiCommandBuffer::IndexFormat indexFormat)
{
    Q_UNUSED(cb);
    Q_UNUSED(startBinding);
    Q_UNUSED(bindings);
    Q_UNUSED(indexBuf);
    Q_UNUSED(indexOffset);
    Q_UNUSED(indexFormat);
}

void QRhiNull::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
    Q_UNUSED(cb);
    Q_UNUSED(viewport);
}

void QRhiNull::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
    Q_UNUSED(cb);
    Q_UNUSED(scissor);
}

void QRhiNull::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
    Q_UNUSED(cb);
    Q_UNUSED(c);
}

void QRhiNull::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
    Q_UNUSED(cb);
    Q_UNUSED(refValue);
}

void QRhiNull::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                    quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    Q_UNUSED(cb);
    Q_UNUSED(vertexCount);
    Q_UNUSED(instanceCount);
    Q_UNUSED(firstVertex);
    Q_UNUSED(firstInstance);
}

void QRhiNull::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                            quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
    Q_UNUSED(cb);
    Q_UNUSED(indexCount);
    Q_UNUSED(instanceCount);
    Q_UNUSED(firstIndex);
    Q_UNUSED(vertexOffset);
    Q_UNUSED(firstInstance);
}

void QRhiNull::debugMarkBegin(QRhiCommandBuffer *cb, const QByteArray &name)
{
    Q_UNUSED(cb);
    Q_UNUSED(name);
}

void QRhiNull::debugMarkEnd(QRhiCommandBuffer *cb)
{
    Q_UNUSED(cb);
}

void QRhiNull::debugMarkMsg(QRhiCommandBuffer *cb, const QByteArray &msg)
{
    Q_UNUSED(cb);
    Q_UNUSED(msg);
}

QRhi::FrameOpResult QRhiNull::beginFrame(QRhiSwapChain *swapChain)
{
    QRhiProfilerPrivate *rhiP = profilerPrivateOrNull();
    QRHI_PROF_F(beginSwapChainFrame(swapChain));
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiNull::endFrame(QRhiSwapChain *swapChain)
{
    QNullSwapChain *swapChainD = QRHI_RES(QNullSwapChain, swapChain);
    QRhiProfilerPrivate *rhiP = profilerPrivateOrNull();
    QRHI_PROF_F(endSwapChainFrame(swapChain, swapChainD->frameCount + 1));
    QRHI_PROF_F(swapChainFrameGpuTime(swapChain, 0.000666f));
    swapChainD->frameCount += 1;
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiNull::beginOffscreenFrame(QRhiCommandBuffer **cb)
{
    Q_UNUSED(cb);
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiNull::endOffscreenFrame()
{
    return QRhi::FrameOpSuccess;;
}

QRhi::FrameOpResult QRhiNull::finish()
{
    return QRhi::FrameOpSuccess;
}

void QRhiNull::resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_UNUSED(cb);
    QRhiResourceUpdateBatchPrivate *ud = QRhiResourceUpdateBatchPrivate::get(resourceUpdates);
    ud->free();
}

void QRhiNull::beginPass(QRhiCommandBuffer *cb,
                          QRhiRenderTarget *rt,
                          const QRhiColorClearValue &colorClearValue,
                          const QRhiDepthStencilClearValue &depthStencilClearValue,
                          QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_UNUSED(cb);
    Q_UNUSED(rt);
    Q_UNUSED(colorClearValue);
    Q_UNUSED(depthStencilClearValue);
    if (resourceUpdates) {
        QRhiResourceUpdateBatchPrivate *ud = QRhiResourceUpdateBatchPrivate::get(resourceUpdates);
        ud->free();
    }
}

void QRhiNull::endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_UNUSED(cb);
    if (resourceUpdates) {
        QRhiResourceUpdateBatchPrivate *ud = QRhiResourceUpdateBatchPrivate::get(resourceUpdates);
        ud->free();
    }
}

QNullBuffer::QNullBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size)
    : QRhiBuffer(rhi, type, usage, size)
{
}

void QNullBuffer::release()
{
    QRHI_PROF;
    QRHI_PROF_F(releaseBuffer(this));
}

bool QNullBuffer::build()
{
    QRHI_PROF;
    QRHI_PROF_F(newBuffer(this, m_size, 1, 0));
    return true;
}

QNullRenderBuffer::QNullRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                                       int sampleCount, QRhiRenderBuffer::Flags flags)
    : QRhiRenderBuffer(rhi, type, pixelSize, sampleCount, flags)
{
}

void QNullRenderBuffer::release()
{
    QRHI_PROF;
    QRHI_PROF_F(releaseRenderBuffer(this));
}

bool QNullRenderBuffer::build()
{
    QRHI_PROF;
    QRHI_PROF_F(newRenderBuffer(this, false, false, 1));
    return true;
}

QRhiTexture::Format QNullRenderBuffer::backingFormat() const
{
    return m_type == Color ? QRhiTexture::RGBA8 : QRhiTexture::UnknownFormat;
}

QNullTexture::QNullTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                             int sampleCount, Flags flags)
    : QRhiTexture(rhi, format, pixelSize, sampleCount, flags)
{
}

void QNullTexture::release()
{
    QRHI_PROF;
    QRHI_PROF_F(releaseTexture(this));
}

bool QNullTexture::build()
{
    const bool isCube = m_flags.testFlag(CubeMap);
    const bool hasMipMaps = m_flags.testFlag(MipMapped);
    QSize size = m_pixelSize.isEmpty() ? QSize(1, 1) : m_pixelSize;
    const int mipLevelCount = hasMipMaps ? qCeil(log2(qMax(size.width(), size.height()))) + 1 : 1;
    QRHI_PROF;
    QRHI_PROF_F(newTexture(this, true, mipLevelCount, isCube ? 6 : 1, 1));
    return true;
}

bool QNullTexture::buildFrom(const QRhiNativeHandles *src)
{
    Q_UNUSED(src);
    const bool isCube = m_flags.testFlag(CubeMap);
    const bool hasMipMaps = m_flags.testFlag(MipMapped);
    QSize size = m_pixelSize.isEmpty() ? QSize(1, 1) : m_pixelSize;
    const int mipLevelCount = hasMipMaps ? qCeil(log2(qMax(size.width(), size.height()))) + 1 : 1;
    QRHI_PROF;
    QRHI_PROF_F(newTexture(this, false, mipLevelCount, isCube ? 6 : 1, 1));
    return true;
}

const QRhiNativeHandles *QNullTexture::nativeHandles()
{
    return &nativeHandlesStruct;
}

QNullSampler::QNullSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                             AddressMode u, AddressMode v, AddressMode w)
    : QRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v, w)
{
}

void QNullSampler::release()
{
}

bool QNullSampler::build()
{
    return true;
}

QNullRenderPassDescriptor::QNullRenderPassDescriptor(QRhiImplementation *rhi)
    : QRhiRenderPassDescriptor(rhi)
{
}

void QNullRenderPassDescriptor::release()
{
}

QNullReferenceRenderTarget::QNullReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiReferenceRenderTarget(rhi),
      d(rhi)
{
}

void QNullReferenceRenderTarget::release()
{
}

QRhiRenderTarget::Type QNullReferenceRenderTarget::type() const
{
    return RtRef;
}

QSize QNullReferenceRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

float QNullReferenceRenderTarget::devicePixelRatio() const
{
    return d.dpr;
}

QNullTextureRenderTarget::QNullTextureRenderTarget(QRhiImplementation *rhi,
                                                     const QRhiTextureRenderTargetDescription &desc,
                                                     Flags flags)
    : QRhiTextureRenderTarget(rhi, desc, flags),
      d(rhi)
{
}

void QNullTextureRenderTarget::release()
{
}

QRhiRenderPassDescriptor *QNullTextureRenderTarget::newCompatibleRenderPassDescriptor()
{
    return new QNullRenderPassDescriptor(rhi);
}

bool QNullTextureRenderTarget::build()
{
    d.rp = QRHI_RES(QNullRenderPassDescriptor, m_renderPassDesc);
    const QVector<QRhiColorAttachment> colorAttachments = m_desc.colorAttachments();
    if (!colorAttachments.isEmpty()) {
        QRhiTexture *tex = colorAttachments.first().texture();
        QRhiRenderBuffer *rb = colorAttachments.first().renderBuffer();
        d.pixelSize = tex ? tex->pixelSize() : rb->pixelSize();
    } else if (m_desc.depthStencilBuffer()) {
        d.pixelSize = m_desc.depthStencilBuffer()->pixelSize();
    } else if (m_desc.depthTexture()) {
        d.pixelSize = m_desc.depthTexture()->pixelSize();
    }
    return true;
}

QRhiRenderTarget::Type QNullTextureRenderTarget::type() const
{
    return RtTexture;
}

QSize QNullTextureRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

float QNullTextureRenderTarget::devicePixelRatio() const
{
    return d.dpr;
}

QNullShaderResourceBindings::QNullShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiShaderResourceBindings(rhi)
{
}

void QNullShaderResourceBindings::release()
{
}

bool QNullShaderResourceBindings::build()
{
    return true;
}

QNullGraphicsPipeline::QNullGraphicsPipeline(QRhiImplementation *rhi)
    : QRhiGraphicsPipeline(rhi)
{
}

void QNullGraphicsPipeline::release()
{
}

bool QNullGraphicsPipeline::build()
{
    return true;
}

QNullCommandBuffer::QNullCommandBuffer(QRhiImplementation *rhi)
    : QRhiCommandBuffer(rhi)
{
}

void QNullCommandBuffer::release()
{
    Q_UNREACHABLE();
}

QNullSwapChain::QNullSwapChain(QRhiImplementation *rhi)
    : QRhiSwapChain(rhi),
      rt(rhi),
      cb(rhi)
{
}

void QNullSwapChain::release()
{
    QRHI_PROF;
    QRHI_PROF_F(releaseSwapChain(this));
}

QRhiCommandBuffer *QNullSwapChain::currentFrameCommandBuffer()
{
    return &cb;
}

QRhiRenderTarget *QNullSwapChain::currentFrameRenderTarget()
{
    return &rt;
}

QSize QNullSwapChain::surfacePixelSize()
{
    return QSize(1280, 720);
}

QRhiRenderPassDescriptor *QNullSwapChain::newCompatibleRenderPassDescriptor()
{
    return new QNullRenderPassDescriptor(rhi);
}

bool QNullSwapChain::buildOrResize()
{
    m_currentPixelSize = surfacePixelSize();
    rt.d.rp = QRHI_RES(QNullRenderPassDescriptor, m_renderPassDesc);
    rt.d.pixelSize = m_currentPixelSize;
    frameCount = 0;
    QRHI_PROF;
    QRHI_PROF_F(resizeSwapChain(this, 1, 0, 1));
    return true;
}

QT_END_NAMESPACE
