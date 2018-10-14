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

struct QMetalBuffer : public QRhiBuffer
{
    QMetalBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    void release() override;
    bool build() override;
};

struct QMetalRenderBuffer : public QRhiRenderBuffer
{
    QMetalRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                       int sampleCount, QRhiRenderBuffer::Hints hints);
    void release() override;
    bool build() override;
};

struct QMetalTexture : public QRhiTexture
{
    QMetalTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize, Flags flags);
    void release() override;
    bool build() override;
};

struct QMetalSampler : public QRhiSampler
{
    QMetalSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode, AddressMode u, AddressMode v);
    void release() override;
    bool build() override;
};

struct QMetalRenderPassData;

struct QMetalRenderPass : public QRhiRenderPass
{
    QMetalRenderPass(QRhiImplementation *rhi);
    void release() override;

    // there is no MTLRenderPassDescriptor here as one will be created for each pass in beginPass()
};

struct QMetalBasicRenderTargetData
{
    QMetalBasicRenderTargetData(QRhiImplementation *rhi) : rp(rhi) { }

    QMetalRenderPass rp;
    QSize pixelSize;
    int attCount;
};

struct QMetalReferenceRenderTarget : public QRhiReferenceRenderTarget
{
    QMetalReferenceRenderTarget(QRhiImplementation *rhi);
    void release() override;
    Type type() const override;
    QSize sizeInPixels() const override;
    const QRhiRenderPass *renderPass() const override;

    QMetalBasicRenderTargetData d;
};

struct QMetalTextureRenderTarget : public QRhiTextureRenderTarget
{
    QMetalTextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, Flags flags);
    QMetalTextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiRenderBuffer *depthStencilBuffer, Flags flags);
    QMetalTextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiTexture *depthTexture, Flags flags);
    void release() override;
    Type type() const override;
    bool build() override;
    QSize sizeInPixels() const override;
    const QRhiRenderPass *renderPass() const override;

    QMetalBasicRenderTargetData d;
};

struct QMetalShaderResourceBindings : public QRhiShaderResourceBindings
{
    QMetalShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    uint generation = 0;
};

struct QMetalGraphicsPipeline : public QRhiGraphicsPipeline
{
    QMetalGraphicsPipeline(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    uint generation = 0;
};

struct QMetalCommandBufferData;

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

    void resetState() {
        currentTarget = nullptr;
        currentPipeline = nullptr;
        currentPipelineGeneration = 0;
        currentSrb = nullptr;
        currentSrbGeneration = 0;
    }
};

struct QMetalSwapChainData;

struct QMetalSwapChain : public QRhiSwapChain
{
    QMetalSwapChain(QRhiImplementation *rhi);
    ~QMetalSwapChain();
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;
    const QRhiRenderPass *defaultRenderPass() const override;
    QSize requestedSizeInPixels() const override;
    QSize effectiveSizeInPixels() const override;

    bool build(QWindow *window, const QSize &requestedPixelSize, SurfaceImportFlags flags,
               QRhiRenderBuffer *depthStencil, int sampleCount) override;

    bool build(QObject *target) override;

    QWindow *window = nullptr;
    QSize requestedPixelSize;
    QSize effectivePixelSize;

    int currentFrame = 0; // 0..QMTL_FRAMES_IN_FLIGHT-1
    QMetalReferenceRenderTarget rtWrapper;
    QMetalCommandBuffer cbWrapper;
    QMetalSwapChainData *d = nullptr;
};

struct QRhiMetalData;

class QRhiMetal : public QRhiImplementation
{
public:
    QRhiMetal(QRhiInitParams *params);
    ~QRhiMetal();

    QRhiGraphicsPipeline *createGraphicsPipeline() override;
    QRhiShaderResourceBindings *createShaderResourceBindings() override;
    QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                             QRhiBuffer::UsageFlags usage,
                             int size) override;
    QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                         const QSize &pixelSize,
                                         int sampleCount,
                                         QRhiRenderBuffer::Hints hints) override;
    QRhiTexture *createTexture(QRhiTexture::Format format,
                               const QSize &pixelSize,
                               QRhiTexture::Flags flags) override;
    QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                               QRhiSampler::Filter mipmapMode,
                               QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v) override;

    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiTextureRenderTarget::Flags flags) override;
    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiRenderBuffer *depthStencilBuffer,
                                                       QRhiTextureRenderTarget::Flags flags) override;
    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiTexture *depthTexture,
                                                       QRhiTextureRenderTarget::Flags flags) override;

    QRhiSwapChain *createSwapChain() override;
    QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain) override;
    QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain) override;

    void beginPass(QRhiRenderTarget *rt,
                   QRhiCommandBuffer *cb,
                   const QRhiClearValue *clearValues,
                   QRhiResourceUpdateBatch *resourceUpdates) override;
    void endPass(QRhiCommandBuffer *cb) override;

    void setGraphicsPipeline(QRhiCommandBuffer *cb,
                             QRhiGraphicsPipeline *ps,
                             QRhiShaderResourceBindings *srb) override;

    void setVertexInput(QRhiCommandBuffer *cb,
                        int startBinding, const QVector<QRhi::VertexInput> &bindings,
                        QRhiBuffer *indexBuf, quint32 indexOffset,
                        QRhi::IndexFormat indexFormat) override;

    void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport) override;
    void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor) override;
    void setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c) override;
    void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue) override;

    void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
              quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) override;

    void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                     quint32 instanceCount, quint32 firstIndex,
                     qint32 vertexOffset, quint32 firstInstance) override;

    QVector<int> supportedSampleCounts() const override;
    int ubufAlignment() const override;
    bool isYUpInFramebuffer() const override;
    QMatrix4x4 clipSpaceCorrMatrix() const override;

    void create();
    void destroy();

    bool importedDevice = false;
    bool inFrame = false;
    QMetalSwapChain *currentSwapChain = nullptr;
    int currentFrameSlot = 0;
    int finishedFrameCount = 0;
    bool inPass = false;

    QRhiMetalData *d = nullptr; // have to hide the ObjC stuff
};

QT_END_NAMESPACE

#endif
