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

#ifndef QRHIMETAL_P_H
#define QRHIMETAL_P_H

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

#include "qrhimetal.h"
#include "qrhi_p.h"
#include <QShaderDescription>
#include <QWindow>

QT_BEGIN_NAMESPACE

static const int QMTL_FRAMES_IN_FLIGHT = 2;

// have to hide the ObjC stuff, this header cannot contain MTL* at all
struct QMetalBufferData;

struct QMetalBuffer : public QRhiBuffer
{
    QMetalBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    ~QMetalBuffer();
    bool isShareable() const override;
    void release() override;
    bool build() override;

    QMetalBufferData *d;
    uint generation = 0;
    int lastActiveFrameSlot = -1;
    friend class QRhiMetal;
    friend struct QMetalShaderResourceBindings;
};

struct QMetalRenderBufferData;

struct QMetalRenderBuffer : public QRhiRenderBuffer
{
    QMetalRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                       int sampleCount, QRhiRenderBuffer::Flags flags);
    ~QMetalRenderBuffer();
    bool isShareable() const override;
    void release() override;
    bool build() override;
    QRhiTexture::Format backingFormat() const override;

    QMetalRenderBufferData *d;
    int samples = 1;
    uint generation = 0;
    int lastActiveFrameSlot = -1;
    friend class QRhiMetal;
};

struct QMetalTextureData;

struct QMetalTexture : public QRhiTexture
{
    QMetalTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                  int sampleCount, Flags flags);
    ~QMetalTexture();
    bool isShareable() const override;
    void release() override;
    bool build() override;
    bool buildFrom(const QRhiNativeHandles *src) override;
    const QRhiNativeHandles *nativeHandles() override;

    bool prepareBuild(QSize *adjustedSize = nullptr);

    QMetalTextureData *d;
    QRhiMetalTextureNativeHandles nativeHandlesStruct;
    int mipLevelCount = 0;
    int samples = 1;
    uint generation = 0;
    int lastActiveFrameSlot = -1;
    friend class QRhiMetal;
    friend struct QMetalShaderResourceBindings;
};

struct QMetalSamplerData;

struct QMetalSampler : public QRhiSampler
{
    QMetalSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                  AddressMode u, AddressMode v, AddressMode w);
    ~QMetalSampler();
    bool isShareable() const override;
    void release() override;
    bool build() override;

    QMetalSamplerData *d;
    uint generation = 0;
    int lastActiveFrameSlot = -1;
    friend class QRhiMetal;
    friend struct QMetalShaderResourceBindings;
};

struct QMetalRenderPassDescriptor : public QRhiRenderPassDescriptor
{
    QMetalRenderPassDescriptor(QRhiImplementation *rhi);
    void release() override;

    // there is no MTLRenderPassDescriptor here as one will be created for each pass in beginPass()

    // but the things needed for the render pipeline descriptor have to be provided
    static const int MAX_COLOR_ATTACHMENTS = 8;
    int colorAttachmentCount = 0;
    bool hasDepthStencil = false;
    int colorFormat[MAX_COLOR_ATTACHMENTS];
    int dsFormat;
};

struct QMetalRenderTargetData;

struct QMetalReferenceRenderTarget : public QRhiRenderTarget
{
    QMetalReferenceRenderTarget(QRhiImplementation *rhi);
    ~QMetalReferenceRenderTarget();
    void release() override;

    Type type() const override;
    QSize sizeInPixels() const override;
    float devicePixelRatio() const override;

    QMetalRenderTargetData *d;
};

struct QMetalTextureRenderTarget : public QRhiTextureRenderTarget
{
    QMetalTextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags);
    ~QMetalTextureRenderTarget();
    void release() override;

    Type type() const override;
    QSize sizeInPixels() const override;
    float devicePixelRatio() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool build() override;

    QMetalRenderTargetData *d;
    friend class QRhiMetal;
};

struct QMetalShaderResourceBindings : public QRhiShaderResourceBindings
{
    QMetalShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    QVector<QRhiShaderResourceBinding> sortedBindings;
    int maxBinding = -1;

    struct BoundUniformBufferData {
        quint64 id;
        uint generation;
    };
    struct BoundSampledTextureData {
        quint64 texId;
        uint texGeneration;
        quint64 samplerId;
        uint samplerGeneration;
    };
    struct BoundResourceData {
        union {
            BoundUniformBufferData ubuf;
            BoundSampledTextureData stex;
        };
    };
    QVector<BoundResourceData> boundResourceData;

    uint generation = 0;
    friend class QRhiMetal;
};

struct QMetalGraphicsPipelineData;

struct QMetalGraphicsPipeline : public QRhiGraphicsPipeline
{
    QMetalGraphicsPipeline(QRhiImplementation *rhi);
    ~QMetalGraphicsPipeline();
    void release() override;
    bool build() override;

    QMetalGraphicsPipelineData *d;
    uint generation = 0;
    int lastActiveFrameSlot = -1;
    friend class QRhiMetal;
};

struct QMetalCommandBufferData;
struct QMetalSwapChain;

struct QMetalCommandBuffer : public QRhiCommandBuffer
{
    QMetalCommandBuffer(QRhiImplementation *rhi);
    ~QMetalCommandBuffer();
    void release() override;

    QMetalCommandBufferData *d = nullptr;

    QRhiRenderTarget *currentTarget;
    QRhiGraphicsPipeline *currentPipeline;
    uint currentPipelineGeneration;
    QRhiShaderResourceBindings *currentSrb;
    uint currentSrbGeneration;
    int currentResSlot;
    QRhiBuffer *currentIndexBuffer;
    quint32 currentIndexOffset;
    QRhiCommandBuffer::IndexFormat currentIndexFormat;

    void resetState();
    void resetPerPassState();
};

struct QMetalSwapChainData;

struct QMetalSwapChain : public QRhiSwapChain
{
    QMetalSwapChain(QRhiImplementation *rhi);
    ~QMetalSwapChain();
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;
    QSize surfacePixelSize() override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;

    bool buildOrResize() override;

    void chooseFormats();

    QWindow *window = nullptr;
    QSize pixelSize;
    int currentFrameSlot = 0; // 0..QMTL_FRAMES_IN_FLIGHT-1
    int frameCount = 0;
    int samples = 1;
    QMetalReferenceRenderTarget rtWrapper;
    QMetalCommandBuffer cbWrapper;
    QMetalRenderBuffer *ds = nullptr;
    QMetalSwapChainData *d = nullptr;
};

struct QRhiMetalData;

class QRhiMetal : public QRhiImplementation
{
public:
    QRhiMetal(QRhiMetalInitParams *params, QRhiMetalNativeHandles *importDevice = nullptr);
    ~QRhiMetal();

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

    void executeDeferredReleases(bool forced = false);
    void finishActiveReadbacks(bool forced = false);
    void enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates);
    void executeBufferHostWritesForCurrentFrame(QMetalBuffer *bufD);
    void enqueueShaderResourceBindings(QMetalShaderResourceBindings *srbD, QMetalCommandBuffer *cbD,
                                       const QVector<QRhiCommandBuffer::DynamicOffset> &dynamicOffsets,
                                       bool offsetOnlyChange);
    int effectiveSampleCount(int sampleCount) const;

    bool importedDevice = false;
    bool importedCmdQueue = false;
    bool inFrame = false;
    int currentFrameSlot = 0;
    bool inPass = false;
    QMetalSwapChain *currentSwapChain = nullptr;
    QSet<QMetalSwapChain *> swapchains;
    QRhiMetalNativeHandles nativeHandlesStruct;

    struct {
        int maxTextureSize = 4096;
    } caps;

    QRhiMetalData *d = nullptr;
};

QT_END_NAMESPACE

#endif
