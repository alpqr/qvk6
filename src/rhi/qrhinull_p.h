/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt RHI module
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QRHINULL_P_H
#define QRHINULL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qrhinull.h"
#include "qrhi_p.h"

QT_BEGIN_NAMESPACE

struct QNullBuffer : public QRhiBuffer
{
    QNullBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    void release() override;
    bool build() override;
};

struct QNullRenderBuffer : public QRhiRenderBuffer
{
    QNullRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                       int sampleCount, QRhiRenderBuffer::Flags flags);
    void release() override;
    bool build() override;
    QRhiTexture::Format backingFormat() const override;
};

struct QNullTexture : public QRhiTexture
{
    QNullTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                  int sampleCount, Flags flags);
    void release() override;
    bool build() override;
    bool buildFrom(const QRhiNativeHandles *src) override;
    const QRhiNativeHandles *nativeHandles() override;

    QRhiNullTextureNativeHandles nativeHandlesStruct;
};

struct QNullSampler : public QRhiSampler
{
    QNullSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                  AddressMode u, AddressMode v, AddressMode w);
    void release() override;
    bool build() override;
};

struct QNullRenderPassDescriptor : public QRhiRenderPassDescriptor
{
    QNullRenderPassDescriptor(QRhiImplementation *rhi);
    void release() override;
};

struct QNullRenderTargetData
{
    QNullRenderTargetData(QRhiImplementation *) { }

    QNullRenderPassDescriptor *rp = nullptr;
    QSize pixelSize;
    float dpr = 1;
};

struct QNullReferenceRenderTarget : public QRhiRenderTarget
{
    QNullReferenceRenderTarget(QRhiImplementation *rhi);
    void release() override;
    Type type() const override;
    QSize sizeInPixels() const override;
    float devicePixelRatio() const override;

    QNullRenderTargetData d;
};

struct QNullTextureRenderTarget : public QRhiTextureRenderTarget
{
    QNullTextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags);
    void release() override;

    Type type() const override;
    QSize sizeInPixels() const override;
    float devicePixelRatio() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool build() override;

    QNullRenderTargetData d;
};

struct QNullShaderResourceBindings : public QRhiShaderResourceBindings
{
    QNullShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;
};

struct QNullGraphicsPipeline : public QRhiGraphicsPipeline
{
    QNullGraphicsPipeline(QRhiImplementation *rhi);
    void release() override;
    bool build() override;
};

struct QNullCommandBuffer : public QRhiCommandBuffer
{
    QNullCommandBuffer(QRhiImplementation *rhi);
    void release() override;
};

struct QNullSwapChain : public QRhiSwapChain
{
    QNullSwapChain(QRhiImplementation *rhi);
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;

    QSize surfacePixelSize() override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool buildOrResize() override;

    QNullReferenceRenderTarget rt;
    QNullCommandBuffer cb;
    int frameCount = 0;
};

class QRhiNull : public QRhiImplementation
{
public:
    QRhiNull(QRhiNullInitParams *params);

    bool create(QRhi::Flags flags) override;
    void destroy() override;

    QRhiGraphicsPipeline *createGraphicsPipeline() override;
    QRhiShaderResourceBindings *createShaderResourceBindings() override;
    QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                             QRhiBuffer::UsageFlags usage,
                             int size) override;
    QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                         const QSize &pixelSize,
                                         int sampleCount,
                                         QRhiRenderBuffer::Flags flags) override;
    QRhiTexture *createTexture(QRhiTexture::Format format,
                               const QSize &pixelSize,
                               int sampleCount,
                               QRhiTexture::Flags flags) override;
    QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                               QRhiSampler::Filter mipmapMode,
                               QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w) override;

    QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                       QRhiTextureRenderTarget::Flags flags) override;

    QRhiSwapChain *createSwapChain() override;
    QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain, QRhi::BeginFrameFlags flags) override;
    QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain, QRhi::EndFrameFlags flags) override;
    QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb) override;
    QRhi::FrameOpResult endOffscreenFrame() override;
    QRhi::FrameOpResult finish() override;

    void resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;

    void beginPass(QRhiCommandBuffer *cb,
                   QRhiRenderTarget *rt,
                   const QRhiColorClearValue &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates) override;
    void endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;

    void setGraphicsPipeline(QRhiCommandBuffer *cb,
                             QRhiGraphicsPipeline *ps) override;

    void setShaderResources(QRhiCommandBuffer *cb,
                            QRhiShaderResourceBindings *srb,
                            const QVector<QRhiCommandBuffer::DynamicOffset> &dynamicOffsets) override;

    void setVertexInput(QRhiCommandBuffer *cb,
                        int startBinding, const QVector<QRhiCommandBuffer::VertexInput> &bindings,
                        QRhiBuffer *indexBuf, quint32 indexOffset,
                        QRhiCommandBuffer::IndexFormat indexFormat) override;

    void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport) override;
    void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor) override;
    void setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c) override;
    void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue) override;

    void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
              quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) override;

    void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                     quint32 instanceCount, quint32 firstIndex,
                     qint32 vertexOffset, quint32 firstInstance) override;

    void debugMarkBegin(QRhiCommandBuffer *cb, const QByteArray &name) override;
    void debugMarkEnd(QRhiCommandBuffer *cb) override;
    void debugMarkMsg(QRhiCommandBuffer *cb, const QByteArray &msg) override;

    QVector<int> supportedSampleCounts() const override;
    int ubufAlignment() const override;
    bool isYUpInFramebuffer() const override;
    bool isYUpInNDC() const override;
    QMatrix4x4 clipSpaceCorrMatrix() const override;
    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const override;
    bool isFeatureSupported(QRhi::Feature feature) const override;
    int resourceSizeLimit(QRhi::ResourceSizeLimit limit) const override;
    const QRhiNativeHandles *nativeHandles() override;

    QRhiNullNativeHandles nativeHandlesStruct;
};

QT_END_NAMESPACE

#endif
