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

#include "qrhimetal_p.h"
#include <QGuiApplication>
#include <QWindow>
#include <QElapsedTimer>
#include <qmath.h>
#include <QBakedShader>
#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

QT_BEGIN_NAMESPACE

/*
    Metal backend. Double buffers and throttles to vsync. "Dynamic" buffers are
    Shared (host visible) and duplicated (due to 2 frames in flight), "static"
    are Managed on macOS and Shared on iOS/tvOS, and still duplicated.
    "Immutable" is like "static" but with only one native buffer underneath.
    Textures are Private (device local) and a host visible staging buffer is
    used to upload data to them.
*/

#if __has_feature(objc_arc)
#error ARC not supported
#endif

// Note: we expect everything here pass the Metal API validation when running
// in Debug mode in XCode. Some of the issues that break validation are not
// obvious and not visible when running outside XCode.

struct QRhiMetalData
{
    id<MTLDevice> dev;
    id<MTLCommandQueue> cmdQueue;

    MTLRenderPassDescriptor *createDefaultRenderPass(bool hasDepthStencil,
                                                     const QRhiColorClearValue &colorClearValue,
                                                     const QRhiDepthStencilClearValue &depthStencilClearValue);
    id<MTLLibrary> createMetalLib(const QBakedShader &shader, QString *error, QByteArray *entryPoint);
    id<MTLFunction> createMSLShaderFunction(id<MTLLibrary> lib, const QByteArray &entryPoint);

    struct DeferredReleaseEntry {
        enum Type {
            Buffer,
            RenderBuffer,
            Texture,
            Sampler,
            StagingBuffer
        };
        Type type;
        int lastActiveFrameSlot; // -1 if not used otherwise 0..FRAMES_IN_FLIGHT-1
        union {
            struct {
                id<MTLBuffer> buffers[QMTL_FRAMES_IN_FLIGHT];
            } buffer;
            struct {
                id<MTLTexture> texture;
            } renderbuffer;
            struct {
                id<MTLTexture> texture;
                id<MTLBuffer> stagingBuffers[QMTL_FRAMES_IN_FLIGHT];
            } texture;
            struct {
                id<MTLSamplerState> samplerState;
            } sampler;
            struct {
                id<MTLBuffer> buffer;
            } stagingBuffer;
        };
    };
    QVector<DeferredReleaseEntry> releaseQueue;
};

struct QMetalBufferData
{
    bool managed;
    id<MTLBuffer> buf[QMTL_FRAMES_IN_FLIGHT];
    QVector<QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate> pendingUpdates[QMTL_FRAMES_IN_FLIGHT];
};

struct QMetalRenderBufferData
{
    MTLPixelFormat format;
    id<MTLTexture> tex = nil;
};

struct QMetalTextureData
{
    MTLPixelFormat format;
    id<MTLTexture> tex = nil;
    id<MTLBuffer> stagingBuf[QMTL_FRAMES_IN_FLIGHT];
};

struct QMetalSamplerData
{
    id<MTLSamplerState> samplerState = nil;
};

struct QMetalCommandBufferData
{
    id<MTLCommandBuffer> cb;
    id<MTLRenderCommandEncoder> currentPassEncoder;
    MTLRenderPassDescriptor *currentPassRpDesc;
};

struct QMetalRenderTargetData
{
    QSize pixelSize;
    int colorAttCount = 0;
    int dsAttCount = 0;
    struct {
        id<MTLTexture> colorTex[QMetalRenderPassDescriptor::MAX_COLOR_ATTACHMENTS];
        id<MTLTexture> dsTex = nil;
        id<MTLTexture> resolveTex = nil;
        bool hasStencil = false;
    } fb;
};

struct QMetalGraphicsPipelineData
{
    id<MTLRenderPipelineState> ps = nil;
    id<MTLDepthStencilState> ds = nil;
    MTLPrimitiveType primitiveType;
    MTLWinding winding;
    MTLCullMode cullMode;
    id<MTLLibrary> vsLib = nil;
    id<MTLFunction> vsFunc = nil;
    id<MTLLibrary> fsLib = nil;
    id<MTLFunction> fsFunc = nil;
};

struct QMetalSwapChainData
{
    CAMetalLayer *layer = nullptr;
    id<CAMetalDrawable> curDrawable;
    dispatch_semaphore_t sem = nullptr;
    id<MTLCommandBuffer> cb[QMTL_FRAMES_IN_FLIGHT];
    MTLRenderPassDescriptor *rp = nullptr;
    id<MTLTexture> msaaTex[QMTL_FRAMES_IN_FLIGHT];
    MTLPixelFormat colorFormat;
};

QRhiMetal::QRhiMetal(QRhiInitParams *params)
{
    d = new QRhiMetalData;

    QRhiMetalInitParams *metalparams = static_cast<QRhiMetalInitParams *>(params);
    importedDevice = metalparams->importExistingDevice;
    if (importedDevice) {
        d->dev = (id<MTLDevice>) metalparams->dev;
        [d->dev retain];
    }

    create();
}

QRhiMetal::~QRhiMetal()
{
    destroy();
    delete d;
}

static inline uint aligned(uint v, uint byteAlign)
{
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

void QRhiMetal::create()
{
    if (!importedDevice)
        d->dev = MTLCreateSystemDefaultDevice();

    qDebug("Metal device: %s", qPrintable(QString::fromNSString([d->dev name])));

    d->cmdQueue = [d->dev newCommandQueue];
}

void QRhiMetal::destroy()
{
    executeDeferredReleases(true);

    if (d->cmdQueue) {
        [d->cmdQueue release];
        d->cmdQueue = nil;
    }

    if (d->dev) {
        [d->dev release];
        d->dev = nil;
    }
}

QVector<int> QRhiMetal::supportedSampleCounts() const
{
    return { 1, 2, 4, 8 };
}

int QRhiMetal::effectiveSampleCount(int sampleCount) const
{
    // Stay compatible with QSurfaceFormat and friends where samples == 0 means the same as 1.
    const int s = qBound(1, sampleCount, 64);
    if (!supportedSampleCounts().contains(s)) {
        qWarning("Attempted to set unsupported sample count %d", sampleCount);
        return 1;
    }
    return s;
}

QRhiSwapChain *QRhiMetal::createSwapChain()
{
    return new QMetalSwapChain(this);
}

QRhiBuffer *QRhiMetal::createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, int size)
{
    return new QMetalBuffer(this, type, usage, size);
}

int QRhiMetal::ubufAlignment() const
{
    return 256;
}

bool QRhiMetal::isYUpInFramebuffer() const
{
    return false;
}

QMatrix4x4 QRhiMetal::clipSpaceCorrMatrix() const
{
    // ### ?

    return QMatrix4x4(); // identity
}

bool QRhiMetal::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const
{
    Q_UNUSED(flags);

    if (format >= QRhiTexture::BC1 && format <= QRhiTexture::BC7)
        return false;

    return true;
}

bool QRhiMetal::isFeatureSupported(QRhi::Feature feature) const
{
    switch (feature) {
    case QRhi::MultisampleTexture:
        Q_FALLTHROUGH();
    case QRhi::MultisampleRenderBuffer:
        return true;
    default:
        Q_UNREACHABLE();
        return false;
    }
}

QRhiRenderBuffer *QRhiMetal::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize,
                                                int sampleCount, QRhiRenderBuffer::Flags flags)
{
    return new QMetalRenderBuffer(this, type, pixelSize, sampleCount, flags);
}

QRhiTexture *QRhiMetal::createTexture(QRhiTexture::Format format, const QSize &pixelSize,
                                      int sampleCount, QRhiTexture::Flags flags)
{
    return new QMetalTexture(this, format, pixelSize, sampleCount, flags);
}

QRhiSampler *QRhiMetal::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                      QRhiSampler::Filter mipmapMode,
                                      QRhiSampler::AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w)
{
    return new QMetalSampler(this, magFilter, minFilter, mipmapMode, u, v, w);
}

QRhiTextureRenderTarget *QRhiMetal::createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QMetalTextureRenderTarget(this, desc, flags);
}

QRhiGraphicsPipeline *QRhiMetal::createGraphicsPipeline()
{
    return new QMetalGraphicsPipeline(this);
}

QRhiShaderResourceBindings *QRhiMetal::createShaderResourceBindings()
{
    return new QMetalShaderResourceBindings(this);
}

void QRhiMetal::setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb)
{
    Q_ASSERT(inPass);

    QMetalGraphicsPipeline *psD = QRHI_RES(QMetalGraphicsPipeline, ps);
    if (!srb)
        srb = psD->m_shaderResourceBindings;

    QMetalShaderResourceBindings *srbD = QRHI_RES(QMetalShaderResourceBindings, srb);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);

    for (const QRhiShaderResourceBinding &b : qAsConst(srbD->sortedBindings)) {
        switch (b.type) {
        case QRhiShaderResourceBinding::UniformBuffer:
        {
            QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, b.ubuf.buf);
            Q_ASSERT(bufD->m_usage.testFlag(QRhiBuffer::UniformBuffer));
            bufD->lastActiveFrameSlot = currentFrameSlot;
            executeBufferHostWritesForCurrentFrame(bufD);
            id<MTLBuffer> mtlbuf = bufD->d->buf[bufD->m_type == QRhiBuffer::Immutable ? 0 : currentFrameSlot];
            if (b.stage.testFlag(QRhiShaderResourceBinding::VertexStage))
                [cbD->d->currentPassEncoder setVertexBuffer: mtlbuf offset: b.ubuf.offset atIndex: b.binding];
            if (b.stage.testFlag(QRhiShaderResourceBinding::FragmentStage))
                [cbD->d->currentPassEncoder setFragmentBuffer: mtlbuf offset: b.ubuf.offset atIndex: b.binding];
        }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
        {
            QMetalTexture *tex = QRHI_RES(QMetalTexture, b.stex.tex);
            tex->lastActiveFrameSlot = currentFrameSlot;
            QMetalSampler *samp = QRHI_RES(QMetalSampler, b.stex.sampler);
            samp->lastActiveFrameSlot = currentFrameSlot;
            if (b.stage.testFlag(QRhiShaderResourceBinding::VertexStage)) {
                [cbD->d->currentPassEncoder setVertexTexture: tex->d->tex atIndex: b.binding];
                [cbD->d->currentPassEncoder setVertexSamplerState: samp->d->samplerState atIndex: b.binding];
            }
            if (b.stage.testFlag(QRhiShaderResourceBinding::FragmentStage)) {
                [cbD->d->currentPassEncoder setFragmentTexture: tex->d->tex atIndex: b.binding];
                [cbD->d->currentPassEncoder setFragmentSamplerState: samp->d->samplerState atIndex: b.binding];
            }
        }
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    if (cbD->currentPipeline != ps || cbD->currentPipelineGeneration != psD->generation
            || cbD->currentSrb != srb || cbD->currentSrbGeneration != srbD->generation)
    {
        cbD->currentPipeline = ps;
        cbD->currentPipelineGeneration = psD->generation;
        cbD->currentSrb = srb;
        cbD->currentSrbGeneration = srbD->generation;
        [cbD->d->currentPassEncoder setRenderPipelineState: psD->d->ps];
        [cbD->d->currentPassEncoder setDepthStencilState: psD->d->ds];
        [cbD->d->currentPassEncoder setCullMode: psD->d->cullMode];
        [cbD->d->currentPassEncoder setFrontFacingWinding: psD->d->winding];
    }
    psD->lastActiveFrameSlot = currentFrameSlot;
}

void QRhiMetal::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<QRhiCommandBuffer::VertexInput> &bindings,
                               QRhiBuffer *indexBuf, quint32 indexOffset, QRhiCommandBuffer::IndexFormat indexFormat)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);

    QRhiBatchedBindings<id<MTLBuffer> > buffers;
    QRhiBatchedBindings<NSUInteger> offsets;
    for (int i = 0; i < bindings.count(); ++i) {
        QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, bindings[i].first);
        executeBufferHostWritesForCurrentFrame(bufD);
        bufD->lastActiveFrameSlot = currentFrameSlot;
        id<MTLBuffer> mtlbuf = bufD->d->buf[bufD->m_type == QRhiBuffer::Immutable ? 0 : currentFrameSlot];
        buffers.feed(startBinding + i, mtlbuf);
        offsets.feed(startBinding + i, bindings[i].second);
    }
    buffers.finish();
    offsets.finish();

    // same binding space for vertex and constant buffers - work it around
    const int firstVertexBinding = QRHI_RES(QMetalShaderResourceBindings, cbD->currentSrb)->maxBinding + 1;

    for (int i = 0, ie = buffers.batches.count(); i != ie; ++i) {
        const auto &bufferBatch(buffers.batches[i]);
        const auto &offsetBatch(offsets.batches[i]);
        [cbD->d->currentPassEncoder setVertexBuffers:
            bufferBatch.resources.constData()
          offsets: offsetBatch.resources.constData()
          withRange: NSMakeRange(firstVertexBinding + bufferBatch.startBinding, bufferBatch.resources.count())];
    }

    if (indexBuf) {
        QMetalBuffer *ibufD = QRHI_RES(QMetalBuffer, indexBuf);
        executeBufferHostWritesForCurrentFrame(ibufD);
        ibufD->lastActiveFrameSlot = currentFrameSlot;
        cbD->currentIndexBuffer = indexBuf;
        cbD->currentIndexOffset = indexOffset;
        cbD->currentIndexFormat = indexFormat;
    } else {
        cbD->currentIndexBuffer = nullptr;
    }
}

static inline MTLViewport toMetalViewport(const QRhiViewport &viewport, const QSize &outputSize)
{
    // x,y is top-left in MTLViewport but bottom-left in QRhiViewport
    MTLViewport vp;
    vp.originX = viewport.r.x();
    vp.originY = outputSize.height() - (viewport.r.y() + viewport.r.w() - 1);
    vp.width = viewport.r.z();
    vp.height = viewport.r.w();
    vp.znear = viewport.minDepth;
    vp.zfar = viewport.maxDepth;
    return vp;
}

void QRhiMetal::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    Q_ASSERT(cbD->currentPipeline && cbD->currentTarget);
    const QSize outputSize = cbD->currentTarget->sizeInPixels();
    const MTLViewport vp = toMetalViewport(viewport, outputSize);
    [cbD->d->currentPassEncoder setViewport: vp];
}

static inline MTLScissorRect toMetalScissor(const QRhiScissor &scissor, const QSize &outputSize)
{
    // x,y is top-left in MTLScissorRect but bottom-left in QRhiScissor
    MTLScissorRect s;
    s.x = scissor.r.x();
    s.y = outputSize.height() - (scissor.r.y() + scissor.r.w() - 1);
    s.width = scissor.r.z();
    s.height = scissor.r.w();
    return s;
}

void QRhiMetal::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    Q_ASSERT(cbD->currentPipeline && cbD->currentTarget);
    const QSize outputSize = cbD->currentTarget->sizeInPixels();
    const MTLScissorRect s = toMetalScissor(scissor, outputSize);
    [cbD->d->currentPassEncoder setScissorRect: s];
}

void QRhiMetal::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    [cbD->d->currentPassEncoder setBlendColorRed: c.x() green: c.y() blue: c.z() alpha: c.w()];
}

void QRhiMetal::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    [cbD->d->currentPassEncoder setStencilReferenceValue: refValue];
}

void QRhiMetal::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                     quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    [cbD->d->currentPassEncoder drawPrimitives:
        QRHI_RES(QMetalGraphicsPipeline, cbD->currentPipeline)->d->primitiveType
      vertexStart: firstVertex vertexCount: vertexCount instanceCount: instanceCount baseInstance: firstInstance];
}

void QRhiMetal::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                            quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    if (!cbD->currentIndexBuffer)
        return;

    const quint32 indexOffset = cbD->currentIndexOffset + firstIndex * (cbD->currentIndexFormat == QRhiCommandBuffer::IndexUInt16 ? 2 : 4);
    Q_ASSERT(indexOffset == aligned(indexOffset, 4));

    QMetalBuffer *ibufD = QRHI_RES(QMetalBuffer, cbD->currentIndexBuffer);
    id<MTLBuffer> mtlbuf = ibufD->d->buf[ibufD->m_type == QRhiBuffer::Immutable ? 0 : currentFrameSlot];

    [cbD->d->currentPassEncoder drawIndexedPrimitives: QRHI_RES(QMetalGraphicsPipeline, cbD->currentPipeline)->d->primitiveType
      indexCount: indexCount
      indexType: cbD->currentIndexFormat == QRhiCommandBuffer::IndexUInt16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
      indexBuffer: mtlbuf
      indexBufferOffset: indexOffset
      instanceCount: instanceCount
      baseVertex: vertexOffset
      baseInstance: firstInstance];
}

QRhi::FrameOpResult QRhiMetal::beginFrame(QRhiSwapChain *swapChain)
{
    Q_ASSERT(!inFrame);
    inFrame = true;

    QMetalSwapChain *swapChainD = QRHI_RES(QMetalSwapChain, swapChain);

//    static QElapsedTimer t;
//    qDebug() << t.restart();

    dispatch_semaphore_wait(swapChainD->d->sem, DISPATCH_TIME_FOREVER);

    swapChainD->d->curDrawable = [swapChainD->d->layer nextDrawable];
    if (!swapChainD->d->curDrawable) {
        qWarning("No drawable");
        return QRhi::FrameOpSwapChainOutOfDate;
    }

    currentFrameSlot = swapChainD->currentFrame;
    if (swapChainD->ds)
        swapChainD->ds->lastActiveFrameSlot = currentFrameSlot;

    // Do not let the command buffer mess with the refcount of objects. We do
    // have a proper render loop and will manage lifetimes similarly to other
    // backends (Vulkan).
    swapChainD->d->cb[currentFrameSlot] = [d->cmdQueue commandBufferWithUnretainedReferences];

    swapChainD->cbWrapper.d->cb = swapChainD->d->cb[currentFrameSlot];
    swapChainD->cbWrapper.resetState();

    id<MTLTexture> scTex = swapChainD->d->curDrawable.texture;
    id<MTLTexture> resolveTex = nil;
    if (swapChainD->samples > 1) {
        resolveTex = scTex;
        scTex = swapChainD->d->msaaTex[currentFrameSlot];
    }

    swapChainD->rtWrapper.d->fb.colorTex[0] = scTex;
    swapChainD->rtWrapper.d->fb.dsTex = swapChainD->ds ? swapChainD->ds->d->tex : nil;
    swapChainD->rtWrapper.d->fb.resolveTex = resolveTex;
    swapChainD->rtWrapper.d->fb.hasStencil = swapChainD->ds ? true : false;

    executeDeferredReleases();

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiMetal::endFrame(QRhiSwapChain *swapChain)
{
    Q_ASSERT(inFrame);
    inFrame = false;

    QMetalSwapChain *swapChainD = QRHI_RES(QMetalSwapChain, swapChain);

    [swapChainD->d->cb[currentFrameSlot] presentDrawable: swapChainD->d->curDrawable];

    __block dispatch_semaphore_t sem = swapChainD->d->sem;
    [swapChainD->d->cb[currentFrameSlot] addCompletedHandler: ^(id<MTLCommandBuffer>) {
        dispatch_semaphore_signal(sem);
    }];

    [swapChainD->d->cb[currentFrameSlot] commit];

    swapChainD->currentFrame = (swapChainD->currentFrame + 1) % QMTL_FRAMES_IN_FLIGHT;

    ++finishedFrameCount;
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiMetal::beginOffscreenFrame(QRhiCommandBuffer **cb)
{
    Q_UNUSED(cb);
    return QRhi::FrameOpError;
}

QRhi::FrameOpResult QRhiMetal::endOffscreenFrame()
{
    return QRhi::FrameOpError;
}

QRhi::FrameOpResult QRhiMetal::finish()
{
    Q_ASSERT(!inPass);

    return QRhi::FrameOpSuccess;
}

MTLRenderPassDescriptor *QRhiMetalData::createDefaultRenderPass(bool hasDepthStencil,
                                                                const QRhiColorClearValue &colorClearValue,
                                                                const QRhiDepthStencilClearValue &depthStencilClearValue)
{
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];

    rp.colorAttachments[0].loadAction = MTLLoadActionClear;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    MTLClearColor c = MTLClearColorMake(colorClearValue.rgba.x(),
                                        colorClearValue.rgba.y(),
                                        colorClearValue.rgba.z(),
                                        colorClearValue.rgba.w());
    rp.colorAttachments[0].clearColor = c;

    if (hasDepthStencil) {
        rp.depthAttachment.loadAction = MTLLoadActionClear;
        rp.depthAttachment.storeAction = MTLStoreActionDontCare;
        rp.stencilAttachment.loadAction = MTLLoadActionClear;
        rp.stencilAttachment.storeAction = MTLStoreActionDontCare;
        rp.depthAttachment.clearDepth = depthStencilClearValue.d;
        rp.stencilAttachment.clearStencil = depthStencilClearValue.s;
    }

    return rp;
}

void QRhiMetal::enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QRhiResourceUpdateBatchPrivate *ud = QRhiResourceUpdateBatchPrivate::get(resourceUpdates);

    for (const QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate &u : ud->dynamicBufferUpdates) {
        QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, u.buf);
        Q_ASSERT(bufD->m_type == QRhiBuffer::Dynamic);
        for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
            bufD->d->pendingUpdates[i].append(u);
    }

    for (const QRhiResourceUpdateBatchPrivate::StaticBufferUpload &u : ud->staticBufferUploads) {
        QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, u.buf);
        Q_ASSERT(bufD->m_type != QRhiBuffer::Dynamic);
        Q_ASSERT(u.offset + u.data.size() <= bufD->m_size);
        for (int i = 0, ie = bufD->m_type == QRhiBuffer::Immutable ? 1 : QMTL_FRAMES_IN_FLIGHT; i != ie; ++i)
            bufD->d->pendingUpdates[i].append({ u.buf, u.offset, u.data.size(), u.data.constData() });
    }

    id<MTLBlitCommandEncoder> blitEnc = nil;
    auto ensureBlit = [&blitEnc, cbD] {
        if (!blitEnc)
            blitEnc = [cbD->d->cb blitCommandEncoder];
    };

    for (const QRhiResourceUpdateBatchPrivate::TextureUpload &u : ud->textureUploads) {
        if (u.desc.layers.isEmpty() || u.desc.layers[0].mipImages.isEmpty())
            continue;

        QMetalTexture *utexD = QRHI_RES(QMetalTexture, u.tex);
        qsizetype stagingSize = 0;
        const int texbufAlign = 256; // probably not needed

        for (int layer = 0, layerCount = u.desc.layers.count(); layer != layerCount; ++layer) {
            const QRhiTextureUploadDescription::Layer &layerDesc(u.desc.layers[layer]);
            Q_ASSERT(layerDesc.mipImages.count() == 1 || utexD->m_flags.testFlag(QRhiTexture::MipMapped));
            for (int level = 0, levelCount = layerDesc.mipImages.count(); level != levelCount; ++level) {
                const QRhiTextureUploadDescription::Layer::MipLevel mipDesc(layerDesc.mipImages[level]);
                const qsizetype imageSizeBytes = mipDesc.image.sizeInBytes();
                if (imageSizeBytes > 0)
                    stagingSize += aligned(imageSizeBytes, texbufAlign);
            }
        }

        ensureBlit();
        if (!utexD->d->stagingBuf[currentFrameSlot])
            utexD->d->stagingBuf[currentFrameSlot] = [d->dev newBufferWithLength: stagingSize options: MTLResourceStorageModeShared];

        void *mp = [utexD->d->stagingBuf[currentFrameSlot] contents];
        qsizetype curOfs = 0;
        for (int layer = 0, layerCount = u.desc.layers.count(); layer != layerCount; ++layer) {
            const QRhiTextureUploadDescription::Layer &layerDesc(u.desc.layers[layer]);
            for (int level = 0, levelCount = layerDesc.mipImages.count(); level != levelCount; ++level) {
                const QRhiTextureUploadDescription::Layer::MipLevel mipDesc(layerDesc.mipImages[level]);
                const qsizetype fullImageSizeBytes = mipDesc.image.sizeInBytes();
                if (fullImageSizeBytes > 0) {
                    QImage img = mipDesc.image;
                    int w = img.width();
                    int h = img.height();
                    int bpl = img.bytesPerLine();
                    int srcOffset = 0;

                    if (!mipDesc.sourceSize.isEmpty() || !mipDesc.sourceTopLeft.isNull()) {
                        const int sx = mipDesc.sourceTopLeft.x();
                        const int sy = mipDesc.sourceTopLeft.y();
                        if (!mipDesc.sourceSize.isEmpty()) {
                            w = mipDesc.sourceSize.width();
                            h = mipDesc.sourceSize.height();
                        }
                        if (img.depth() == 32) {
                            memcpy(reinterpret_cast<char *>(mp) + curOfs, img.constBits(), fullImageSizeBytes);
                            srcOffset = sy * bpl + sx * 4;
                            // bpl remains set to the original image's row stride
                        } else {
                            img = img.copy(sx, sy, w, h);
                            bpl = img.bytesPerLine();
                            Q_ASSERT(img.sizeInBytes() <= fullImageSizeBytes);
                            memcpy(reinterpret_cast<char *>(mp) + curOfs, img.constBits(), img.sizeInBytes());
                        }
                    } else {
                        memcpy(reinterpret_cast<char *>(mp) + curOfs, img.constBits(), fullImageSizeBytes);
                    }

                    const int dx = mipDesc.destinationTopLeft.x();
                    const int dy = mipDesc.destinationTopLeft.y();
                    [blitEnc copyFromBuffer: utexD->d->stagingBuf[currentFrameSlot]
                                             sourceOffset: curOfs + srcOffset
                                             sourceBytesPerRow: bpl
                                             sourceBytesPerImage: 0
                                             sourceSize: MTLSizeMake(w, h, 1)
                                             toTexture: utexD->d->tex
                                             destinationSlice: layer
                                             destinationLevel: level
                                             destinationOrigin: MTLOriginMake(dx, dy, 0)
                                             options: MTLBlitOptionNone];
                    curOfs += aligned(fullImageSizeBytes, texbufAlign);
                }
            }
        }

        utexD->lastActiveFrameSlot = currentFrameSlot;

        if (!utexD->m_flags.testFlag(QRhiTexture::ChangesFrequently)) {
            QRhiMetalData::DeferredReleaseEntry e;
            e.type = QRhiMetalData::DeferredReleaseEntry::StagingBuffer;
            e.lastActiveFrameSlot = currentFrameSlot;
            e.stagingBuffer.buffer = utexD->d->stagingBuf[currentFrameSlot];
            utexD->d->stagingBuf[currentFrameSlot] = nil;
            d->releaseQueue.append(e);
        }
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureCopy &u : ud->textureCopies) {
        Q_ASSERT(u.src && u.dst);
        QMetalTexture *srcD = QRHI_RES(QMetalTexture, u.src);
        QMetalTexture *dstD = QRHI_RES(QMetalTexture, u.dst);
        const float dx = u.desc.destinationTopLeft.x();
        const float dy = u.desc.destinationTopLeft.y();
        const QSize size = u.desc.pixelSize.isEmpty() ? srcD->m_pixelSize : u.desc.pixelSize;
        const float sx = u.desc.sourceTopLeft.x();
        const float sy = u.desc.sourceTopLeft.y();

        ensureBlit();
        [blitEnc copyFromTexture: srcD->d->tex
                                  sourceSlice: u.desc.sourceLayer
                                  sourceLevel: u.desc.sourceLevel
                                  sourceOrigin: MTLOriginMake(sx, sy, 0)
                                  sourceSize: MTLSizeMake(size.width(), size.height(), 1)
                                  toTexture: dstD->d->tex
                                  destinationSlice: u.desc.destinationLayer
                                  destinationLevel: u.desc.destinationLevel
                                  destinationOrigin: MTLOriginMake(dx, dy, 0)];
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureMipGen &u : ud->textureMipGens) {
        ensureBlit();
        [blitEnc generateMipmapsForTexture: QRHI_RES(QMetalTexture, u.tex)->d->tex];
    }

    if (blitEnc)
        [blitEnc endEncoding];

    ud->free();
}

void QRhiMetal::executeBufferHostWritesForCurrentFrame(QMetalBuffer *bufD)
{
    const int idx = bufD->m_type == QRhiBuffer::Immutable ? 0 : currentFrameSlot;
    QVector<QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate> &updates(bufD->d->pendingUpdates[idx]);
    if (updates.isEmpty())
        return;

    void *p = [bufD->d->buf[idx] contents];
    int changeBegin = -1;
    int changeEnd = -1;
    for (const QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate &u : updates) {
        Q_ASSERT(bufD == QRHI_RES(QMetalBuffer, u.buf));
        memcpy(static_cast<char *>(p) + u.offset, u.data.constData(), u.data.size());
        if (changeBegin == -1 || u.offset < changeBegin)
            changeBegin = u.offset;
        if (changeEnd == -1 || u.offset + u.data.size() > changeEnd)
            changeEnd = u.offset + u.data.size();
    }
    if (changeBegin >= 0 && bufD->d->managed)
        [bufD->d->buf[idx] didModifyRange: NSMakeRange(changeBegin, changeEnd - changeBegin)];

    updates.clear();
}

void QRhiMetal::resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(inFrame && !inPass);

    enqueueResourceUpdates(cb, resourceUpdates);
}

void QRhiMetal::beginPass(QRhiCommandBuffer *cb,
                          QRhiRenderTarget *rt,
                          const QRhiColorClearValue &colorClearValue,
                          const QRhiDepthStencilClearValue &depthStencilClearValue,
                          QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(!inPass);

    if (resourceUpdates)
        enqueueResourceUpdates(cb, resourceUpdates);

    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);

    QMetalRenderTargetData *rtD = nullptr;
    switch (rt->type()) {
    case QRhiRenderTarget::RtRef:
        rtD = QRHI_RES(QMetalReferenceRenderTarget, rt)->d;
        cbD->d->currentPassRpDesc = d->createDefaultRenderPass(false, colorClearValue, depthStencilClearValue);
        break;
    case QRhiRenderTarget::RtTexture:
    {
        QMetalTextureRenderTarget *rtTex = QRHI_RES(QMetalTextureRenderTarget, rt);
        rtD = rtTex->d;
        cbD->d->currentPassRpDesc = d->createDefaultRenderPass(false, colorClearValue, depthStencilClearValue);
        if (rtTex->m_flags.testFlag(QRhiTextureRenderTarget::PreserveColorContents)) {
            for (int i = 0; i < rtD->colorAttCount; ++i)
                cbD->d->currentPassRpDesc.colorAttachments[i].loadAction = MTLLoadActionLoad;
        }
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    for (int i = 0; i < rtD->colorAttCount; ++i)
        cbD->d->currentPassRpDesc.colorAttachments[i].texture = rtD->fb.colorTex[i];

    // MSAA swapchains pass the multisample texture in colorTex and the
    // non-multisample texture (from the drawable) in resolveTex.
    if (rtD->fb.resolveTex) {
        cbD->d->currentPassRpDesc.colorAttachments[0].storeAction = MTLStoreActionMultisampleResolve;
        cbD->d->currentPassRpDesc.colorAttachments[0].resolveTexture = rtD->fb.resolveTex;
    }

    if (rtD->dsAttCount) {
        Q_ASSERT(rtD->fb.dsTex);
        cbD->d->currentPassRpDesc.depthAttachment.texture = rtD->fb.dsTex;
        cbD->d->currentPassRpDesc.stencilAttachment.texture = rtD->fb.hasStencil ? rtD->fb.dsTex : nil;
    }

    cbD->d->currentPassEncoder = [cbD->d->cb renderCommandEncoderWithDescriptor: cbD->d->currentPassRpDesc];

    cbD->currentTarget = rt;
    inPass = true;
}

void QRhiMetal::endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(inPass);
    inPass = false;

    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    [cbD->d->currentPassEncoder endEncoding];

    cbD->currentTarget = nullptr;

    if (resourceUpdates)
        enqueueResourceUpdates(cb, resourceUpdates);
}

void QRhiMetal::executeDeferredReleases(bool forced)
{
    for (int i = d->releaseQueue.count() - 1; i >= 0; --i) {
        const QRhiMetalData::DeferredReleaseEntry &e(d->releaseQueue[i]);
        if (forced || currentFrameSlot == e.lastActiveFrameSlot || e.lastActiveFrameSlot < 0) {
            switch (e.type) {
            case QRhiMetalData::DeferredReleaseEntry::Buffer:
                for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
                    [e.buffer.buffers[i] release];
                break;
            case QRhiMetalData::DeferredReleaseEntry::RenderBuffer:
                [e.renderbuffer.texture release];
                break;
            case QRhiMetalData::DeferredReleaseEntry::Texture:
                [e.texture.texture release];
                for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
                    [e.texture.stagingBuffers[i] release];
                break;
            case QRhiMetalData::DeferredReleaseEntry::Sampler:
                [e.sampler.samplerState release];
                break;
            case QRhiMetalData::DeferredReleaseEntry::StagingBuffer:
                [e.stagingBuffer.buffer release];
                break;
            default:
                break;
            }
            d->releaseQueue.removeAt(i);
        }
    }
}

QMetalBuffer::QMetalBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size)
    : QRhiBuffer(rhi, type, usage, size),
      d(new QMetalBufferData)
{
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
        d->buf[i] = nil;
}

QMetalBuffer::~QMetalBuffer()
{
    delete d;
}

void QMetalBuffer::release()
{
    if (!d->buf[0])
        return;

    QRhiMetalData::DeferredReleaseEntry e;
    e.type = QRhiMetalData::DeferredReleaseEntry::Buffer;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        e.buffer.buffers[i] = d->buf[i];
        d->buf[i] = nil;
        d->pendingUpdates[i].clear();
    }

    QRHI_RES_RHI(QRhiMetal);
    rhiD->d->releaseQueue.append(e);
}

bool QMetalBuffer::build()
{
    if (d->buf[0])
        release();

    const int roundedSize = m_usage.testFlag(QRhiBuffer::UniformBuffer) ? aligned(m_size, 256) : m_size;

    d->managed = false;
    MTLResourceOptions opts = MTLResourceStorageModeShared;
#ifdef Q_OS_MACOS
    if (m_type != Dynamic) {
        opts = MTLResourceStorageModeManaged;
        d->managed = true;
    }
#endif

    QRHI_RES_RHI(QRhiMetal);
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        // Immutable only has buf[0] and pendingUpdates[0] in use.
        // Static and Dynamic use all.
        if (i == 0 || m_type != Immutable) {
            d->buf[i] = [rhiD->d->dev newBufferWithLength: roundedSize options: opts];
            d->pendingUpdates[i].reserve(16);
        }
    }

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QMetalRenderBuffer::QMetalRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                                       int sampleCount, QRhiRenderBuffer::Flags flags)
    : QRhiRenderBuffer(rhi, type, pixelSize, sampleCount, flags),
      d(new QMetalRenderBufferData)
{
}

QMetalRenderBuffer::~QMetalRenderBuffer()
{
    delete d;
}

void QMetalRenderBuffer::release()
{
    if (!d->tex)
        return;

    QRhiMetalData::DeferredReleaseEntry e;
    e.type = QRhiMetalData::DeferredReleaseEntry::RenderBuffer;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.renderbuffer.texture = d->tex;
    d->tex = nil;

    QRHI_RES_RHI(QRhiMetal);
    rhiD->d->releaseQueue.append(e);
}

bool QMetalRenderBuffer::build()
{
    if (d->tex)
        release();

    if (m_type != DepthStencil)
        return false;

    QRHI_RES_RHI(QRhiMetal);
    d->format = rhiD->d->dev.depth24Stencil8PixelFormatSupported
            ? MTLPixelFormatDepth24Unorm_Stencil8 : MTLPixelFormatDepth32Float_Stencil8;

    samples = rhiD->effectiveSampleCount(m_sampleCount);

    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = samples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
    desc.pixelFormat = d->format;
    desc.width = m_pixelSize.width();
    desc.height = m_pixelSize.height();
    if (samples > 1)
        desc.sampleCount = samples;
    desc.resourceOptions = MTLResourceStorageModePrivate;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageRenderTarget;

    d->tex = [rhiD->d->dev newTextureWithDescriptor: desc];
    [desc release];

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QRhiTexture::Format QMetalRenderBuffer::backingFormat() const
{
    return m_type == Color ? QRhiTexture::RGBA8 : QRhiTexture::UnknownFormat;
}

QMetalTexture::QMetalTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                             int sampleCount, Flags flags)
    : QRhiTexture(rhi, format, pixelSize, sampleCount, flags),
      d(new QMetalTextureData)
{
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
        d->stagingBuf[i] = nil;
}

QMetalTexture::~QMetalTexture()
{
    delete d;
}

void QMetalTexture::release()
{
    if (!d->tex)
        return;

    QRhiMetalData::DeferredReleaseEntry e;
    e.type = QRhiMetalData::DeferredReleaseEntry::Texture;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.texture.texture = d->tex;
    d->tex = nil;

    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        e.texture.stagingBuffers[i] = d->stagingBuf[i];
        d->stagingBuf[i] = nil;
    }

    QRHI_RES_RHI(QRhiMetal);
    rhiD->d->releaseQueue.append(e);
}

static inline MTLPixelFormat toMetalTextureFormat(QRhiTexture::Format format)
{
    switch (format) {
    case QRhiTexture::RGBA8:
        return MTLPixelFormatRGBA8Unorm;
    case QRhiTexture::BGRA8:
        return MTLPixelFormatBGRA8Unorm;
    case QRhiTexture::R8:
        return MTLPixelFormatR8Unorm;
    case QRhiTexture::R16:
        return MTLPixelFormatR16Unorm;

    case QRhiTexture::D16:
        return MTLPixelFormatDepth16Unorm;
    case QRhiTexture::D32:
        return MTLPixelFormatDepth32Float;

    default:
        Q_UNREACHABLE();
        return MTLPixelFormatRGBA8Unorm;
    }
}

bool QMetalTexture::build()
{
    if (d->tex)
        release();

    const QSize size = m_pixelSize.isEmpty() ? QSize(16, 16) : m_pixelSize;
    const bool hasMipMaps = m_flags.testFlag(MipMapped);

    d->format = toMetalTextureFormat(m_format);
    mipLevelCount = hasMipMaps ? qCeil(log2(qMax(size.width(), size.height()))) + 1 : 1;

    QRHI_RES_RHI(QRhiMetal);
    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = MTLTextureType2D;
    desc.pixelFormat = d->format;
    desc.width = size.width();
    desc.height = size.height();
    desc.mipmapLevelCount = mipLevelCount;
    desc.resourceOptions = MTLResourceStorageModePrivate;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageShaderRead;
    if (m_flags.testFlag(RenderTarget))
        desc.usage |= MTLTextureUsageRenderTarget;

    d->tex = [rhiD->d->dev newTextureWithDescriptor: desc];
    [desc release];

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QMetalSampler::QMetalSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                             AddressMode u, AddressMode v, AddressMode w)
    : QRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v, w),
      d(new QMetalSamplerData)
{
}

QMetalSampler::~QMetalSampler()
{
    delete d;
}

void QMetalSampler::release()
{
    if (!d->samplerState)
        return;

    QRhiMetalData::DeferredReleaseEntry e;
    e.type = QRhiMetalData::DeferredReleaseEntry::Sampler;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.sampler.samplerState = d->samplerState;
    d->samplerState = nil;

    QRHI_RES_RHI(QRhiMetal);
    rhiD->d->releaseQueue.append(e);
}

static inline MTLSamplerMinMagFilter toMetalFilter(QRhiSampler::Filter f)
{
    switch (f) {
    case QRhiSampler::Nearest:
        return MTLSamplerMinMagFilterNearest;
    case QRhiSampler::Linear:
        return MTLSamplerMinMagFilterLinear;
    default:
        Q_UNREACHABLE();
        return MTLSamplerMinMagFilterNearest;
    }
}

static inline MTLSamplerMipFilter toMetalMipmapMode(QRhiSampler::Filter f)
{
    switch (f) {
    case QRhiSampler::None:
        return MTLSamplerMipFilterNotMipmapped;
    case QRhiSampler::Nearest:
        return MTLSamplerMipFilterNearest;
    case QRhiSampler::Linear:
        return MTLSamplerMipFilterLinear;
    default:
        Q_UNREACHABLE();
        return MTLSamplerMipFilterNotMipmapped;
    }
}

static inline MTLSamplerAddressMode toMetalAddressMode(QRhiSampler::AddressMode m)
{
    switch (m) {
    case QRhiSampler::Repeat:
        return MTLSamplerAddressModeRepeat;
    case QRhiSampler::ClampToEdge:
        return MTLSamplerAddressModeClampToEdge;
    case QRhiSampler::Border:
        return MTLSamplerAddressModeClampToBorderColor;
    case QRhiSampler::Mirror:
        return MTLSamplerAddressModeMirrorRepeat;
    case QRhiSampler::MirrorOnce:
        return MTLSamplerAddressModeMirrorClampToEdge;
    default:
        Q_UNREACHABLE();
        return MTLSamplerAddressModeClampToEdge;
    }
}

bool QMetalSampler::build()
{
    if (d->samplerState)
        release();

    MTLSamplerDescriptor *desc = [[MTLSamplerDescriptor alloc] init];
    desc.minFilter = toMetalFilter(m_minFilter);
    desc.magFilter = toMetalFilter(m_magFilter);
    desc.mipFilter = toMetalMipmapMode(m_mipmapMode);
    desc.sAddressMode = toMetalAddressMode(m_addressU);
    desc.tAddressMode = toMetalAddressMode(m_addressV);
    desc.rAddressMode = toMetalAddressMode(m_addressW);

    QRHI_RES_RHI(QRhiMetal);
    d->samplerState = [rhiD->d->dev newSamplerStateWithDescriptor: desc];
    [desc release];

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

// dummy, no Vulkan-style RenderPass+Framebuffer concept here.
// We do have MTLRenderPassDescriptor of course, but it will be created on the fly for each pass.
QMetalRenderPassDescriptor::QMetalRenderPassDescriptor(QRhiImplementation *rhi)
    : QRhiRenderPassDescriptor(rhi)
{
}

void QMetalRenderPassDescriptor::release()
{
    // nothing to do here
}

QMetalReferenceRenderTarget::QMetalReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiReferenceRenderTarget(rhi),
      d(new QMetalRenderTargetData)
{
}

QMetalReferenceRenderTarget::~QMetalReferenceRenderTarget()
{
    delete d;
}

void QMetalReferenceRenderTarget::release()
{
    // nothing to do here
}

QRhiRenderTarget::Type QMetalReferenceRenderTarget::type() const
{
    return RtRef;
}

QSize QMetalReferenceRenderTarget::sizeInPixels() const
{
    return d->pixelSize;
}

QMetalTextureRenderTarget::QMetalTextureRenderTarget(QRhiImplementation *rhi,
                                                     const QRhiTextureRenderTargetDescription &desc,
                                                     Flags flags)
    : QRhiTextureRenderTarget(rhi, desc, flags),
      d(new QMetalRenderTargetData)
{
}

QMetalTextureRenderTarget::~QMetalTextureRenderTarget()
{
    delete d;
}

void QMetalTextureRenderTarget::release()
{
    // nothing to do here
}

QRhiRenderPassDescriptor *QMetalTextureRenderTarget::newCompatibleRenderPassDescriptor()
{
    QMetalRenderPassDescriptor *rpD = new QMetalRenderPassDescriptor(rhi);
    rpD->colorAttachmentCount = m_desc.colorAttachments.count();
    rpD->hasDepthStencil = m_desc.depthStencilBuffer || m_desc.depthTexture;

    for (int i = 0, ie = m_desc.colorAttachments.count(); i != ie; ++i)
        rpD->colorFormat[i] = QRHI_RES(QMetalTexture, m_desc.colorAttachments[i].texture)->d->format;

    if (m_desc.depthTexture)
        rpD->dsFormat = QRHI_RES(QMetalTexture, m_desc.depthTexture)->d->format;
    else if (m_desc.depthStencilBuffer)
        rpD->dsFormat = QRHI_RES(QMetalRenderBuffer, m_desc.depthStencilBuffer)->d->format;

    return rpD;
}

bool QMetalTextureRenderTarget::build()
{
    Q_ASSERT(!m_desc.colorAttachments.isEmpty() || m_desc.depthTexture);
    Q_ASSERT(!m_desc.depthStencilBuffer || !m_desc.depthTexture);
    const bool hasDepthStencil = m_desc.depthStencilBuffer || m_desc.depthTexture;

    d->colorAttCount = m_desc.colorAttachments.count();
    for (int i = 0; i < d->colorAttCount; ++i) {
        QRhiTexture *texture = m_desc.colorAttachments[i].texture;
        Q_ASSERT(texture);
        QMetalTexture *texD = QRHI_RES(QMetalTexture, texture);
        d->fb.colorTex[i] = texD->d->tex;
        if (i == 0)
            d->pixelSize = texD->pixelSize();
    }

    if (hasDepthStencil) {
        if (m_desc.depthTexture) {
            d->fb.dsTex = QRHI_RES(QMetalTexture, m_desc.depthTexture)->d->tex;
            d->fb.hasStencil = false;
            if (d->colorAttCount == 0)
                d->pixelSize = m_desc.depthTexture->pixelSize();
        } else {
            d->fb.dsTex = QRHI_RES(QMetalRenderBuffer, m_desc.depthStencilBuffer)->d->tex;
            d->fb.hasStencil = true;
            if (d->colorAttCount == 0)
                d->pixelSize = m_desc.depthStencilBuffer->pixelSize();
        }
        d->dsAttCount = 1;
    } else {
        d->dsAttCount = 0;
    }

    return true;
}

QRhiRenderTarget::Type QMetalTextureRenderTarget::type() const
{
    return RtTexture;
}

QSize QMetalTextureRenderTarget::sizeInPixels() const
{
    return d->pixelSize;
}

QMetalShaderResourceBindings::QMetalShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiShaderResourceBindings(rhi)
{
}

void QMetalShaderResourceBindings::release()
{
    sortedBindings.clear();
    maxBinding = -1;
}

bool QMetalShaderResourceBindings::build()
{
    if (!sortedBindings.isEmpty())
        release();

    sortedBindings = m_bindings;
    std::sort(sortedBindings.begin(), sortedBindings.end(),
              [](const QRhiShaderResourceBinding &a, const QRhiShaderResourceBinding &b)
    {
        return a.binding < b.binding;
    });
    if (!sortedBindings.isEmpty())
        maxBinding = sortedBindings.last().binding;
    else
        maxBinding = -1;

    generation += 1;
    return true;
}

QMetalGraphicsPipeline::QMetalGraphicsPipeline(QRhiImplementation *rhi)
    : QRhiGraphicsPipeline(rhi),
      d(new QMetalGraphicsPipelineData)
{
}

QMetalGraphicsPipeline::~QMetalGraphicsPipeline()
{
    delete d;
}

void QMetalGraphicsPipeline::release()
{
    if (!d->ps)
        return;

    if (d->ps) {
        [d->ps release];
        d->ps = nil;
    }

    if (d->ds) {
        [d->ds release];
        d->ds = nil;
    }

    if (d->vsFunc) {
        [d->vsFunc release];
        d->vsFunc = nil;
    }
    if (d->vsLib) {
        [d->vsLib release];
        d->vsLib = nil;
    }

    if (d->fsFunc) {
        [d->fsFunc release];
        d->fsFunc = nil;
    }
    if (d->fsLib) {
        [d->fsLib release];
        d->fsLib = nil;
    }
}

static inline MTLVertexFormat toMetalAttributeFormat(QRhiVertexInputLayout::Attribute::Format format)
{
    switch (format) {
    case QRhiVertexInputLayout::Attribute::Float4:
        return MTLVertexFormatFloat4;
    case QRhiVertexInputLayout::Attribute::Float3:
        return MTLVertexFormatFloat3;
    case QRhiVertexInputLayout::Attribute::Float2:
        return MTLVertexFormatFloat2;
    case QRhiVertexInputLayout::Attribute::Float:
        return MTLVertexFormatFloat;
    case QRhiVertexInputLayout::Attribute::UNormByte4:
        return MTLVertexFormatUChar4;
    case QRhiVertexInputLayout::Attribute::UNormByte2:
        return MTLVertexFormatUChar2;
    case QRhiVertexInputLayout::Attribute::UNormByte:
        if (@available(macOS 10.13, iOS 11.0, *))
            return MTLVertexFormatUChar;
        else
            Q_UNREACHABLE();
    default:
        Q_UNREACHABLE();
        return MTLVertexFormatFloat4;
    }
}

static inline MTLBlendFactor toMetalBlendFactor(QRhiGraphicsPipeline::BlendFactor f)
{
    switch (f) {
    case QRhiGraphicsPipeline::Zero:
        return MTLBlendFactorZero;
    case QRhiGraphicsPipeline::One:
        return MTLBlendFactorOne;
    case QRhiGraphicsPipeline::SrcColor:
        return MTLBlendFactorSourceColor;
    case QRhiGraphicsPipeline::OneMinusSrcColor:
        return MTLBlendFactorOneMinusSourceColor;
    case QRhiGraphicsPipeline::DstColor:
        return MTLBlendFactorDestinationColor;
    case QRhiGraphicsPipeline::OneMinusDstColor:
        return MTLBlendFactorOneMinusDestinationColor;
    case QRhiGraphicsPipeline::SrcAlpha:
        return MTLBlendFactorSourceAlpha;
    case QRhiGraphicsPipeline::OneMinusSrcAlpha:
        return MTLBlendFactorOneMinusSourceAlpha;
    case QRhiGraphicsPipeline::DstAlpha:
        return MTLBlendFactorDestinationAlpha;
    case QRhiGraphicsPipeline::OneMinusDstAlpha:
        return MTLBlendFactorOneMinusDestinationAlpha;
    case QRhiGraphicsPipeline::ConstantColor:
        return MTLBlendFactorBlendColor;
    case QRhiGraphicsPipeline::ConstantAlpha:
        return MTLBlendFactorBlendAlpha;
    case QRhiGraphicsPipeline::OneMinusConstantColor:
        return MTLBlendFactorOneMinusBlendColor;
    case QRhiGraphicsPipeline::OneMinusConstantAlpha:
        return MTLBlendFactorOneMinusBlendAlpha;
    case QRhiGraphicsPipeline::SrcAlphaSaturate:
        return MTLBlendFactorSourceAlphaSaturated;
    case QRhiGraphicsPipeline::Src1Color:
        return MTLBlendFactorSource1Color;
    case QRhiGraphicsPipeline::OneMinusSrc1Color:
        return MTLBlendFactorOneMinusSource1Color;
    case QRhiGraphicsPipeline::Src1Alpha:
        return MTLBlendFactorSource1Alpha;
    case QRhiGraphicsPipeline::OneMinusSrc1Alpha:
        return MTLBlendFactorOneMinusSource1Alpha;
    default:
        Q_UNREACHABLE();
        return MTLBlendFactorZero;
    }
}

static inline MTLBlendOperation toMetalBlendOp(QRhiGraphicsPipeline::BlendOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::Add:
        return MTLBlendOperationAdd;
    case QRhiGraphicsPipeline::Subtract:
        return MTLBlendOperationSubtract;
    case QRhiGraphicsPipeline::ReverseSubtract:
        return MTLBlendOperationReverseSubtract;
    case QRhiGraphicsPipeline::Min:
        return MTLBlendOperationMin;
    case QRhiGraphicsPipeline::Max:
        return MTLBlendOperationMax;
    default:
        Q_UNREACHABLE();
        return MTLBlendOperationAdd;
    }
}

static inline uint toMetalColorWriteMask(QRhiGraphicsPipeline::ColorMask c)
{
    uint f = 0;
    if (c.testFlag(QRhiGraphicsPipeline::R))
        f |= MTLColorWriteMaskRed;
    if (c.testFlag(QRhiGraphicsPipeline::G))
        f |= MTLColorWriteMaskGreen;
    if (c.testFlag(QRhiGraphicsPipeline::B))
        f |= MTLColorWriteMaskBlue;
    if (c.testFlag(QRhiGraphicsPipeline::A))
        f |= MTLColorWriteMaskAlpha;
    return f;
}

static inline MTLCompareFunction toMetalCompareOp(QRhiGraphicsPipeline::CompareOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::Never:
        return MTLCompareFunctionNever;
    case QRhiGraphicsPipeline::Less:
        return MTLCompareFunctionLess;
    case QRhiGraphicsPipeline::Equal:
        return MTLCompareFunctionEqual;
    case QRhiGraphicsPipeline::LessOrEqual:
        return MTLCompareFunctionLessEqual;
    case QRhiGraphicsPipeline::Greater:
        return MTLCompareFunctionGreater;
    case QRhiGraphicsPipeline::NotEqual:
        return MTLCompareFunctionNotEqual;
    case QRhiGraphicsPipeline::GreaterOrEqual:
        return MTLCompareFunctionGreaterEqual;
    case QRhiGraphicsPipeline::Always:
        return MTLCompareFunctionAlways;
    default:
        Q_UNREACHABLE();
        return MTLCompareFunctionAlways;
    }
}

static inline MTLStencilOperation toMetalStencilOp(QRhiGraphicsPipeline::StencilOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::StencilZero:
        return MTLStencilOperationZero;
    case QRhiGraphicsPipeline::Keep:
        return MTLStencilOperationKeep;
    case QRhiGraphicsPipeline::Replace:
        return MTLStencilOperationReplace;
    case QRhiGraphicsPipeline::IncrementAndClamp:
        return MTLStencilOperationIncrementClamp;
    case QRhiGraphicsPipeline::DecrementAndClamp:
        return MTLStencilOperationDecrementClamp;
    case QRhiGraphicsPipeline::Invert:
        return MTLStencilOperationInvert;
    case QRhiGraphicsPipeline::IncrementAndWrap:
        return MTLStencilOperationIncrementWrap;
    case QRhiGraphicsPipeline::DecrementAndWrap:
        return MTLStencilOperationDecrementWrap;
    default:
        Q_UNREACHABLE();
        return MTLStencilOperationKeep;
    }
}

static inline MTLPrimitiveType toMetalPrimitiveType(QRhiGraphicsPipeline::Topology t)
{
    switch (t) {
    case QRhiGraphicsPipeline::Triangles:
        return MTLPrimitiveTypeTriangle;
    case QRhiGraphicsPipeline::TriangleStrip:
        return MTLPrimitiveTypeTriangleStrip;
    case QRhiGraphicsPipeline::Lines:
        return MTLPrimitiveTypeLine;
    case QRhiGraphicsPipeline::LineStrip:
        return MTLPrimitiveTypeLineStrip;
    case QRhiGraphicsPipeline::Points:
        return MTLPrimitiveTypePoint;
    default:
        Q_UNREACHABLE();
        return MTLPrimitiveTypeTriangle;
    }
}

static inline MTLCullMode toMetalCullMode(QRhiGraphicsPipeline::CullMode c)
{
    switch (c) {
    case QRhiGraphicsPipeline::None:
        return MTLCullModeNone;
    case QRhiGraphicsPipeline::Front:
        return MTLCullModeFront;
    case QRhiGraphicsPipeline::Back:
        return MTLCullModeBack;
    default:
        Q_UNREACHABLE();
        return MTLCullModeNone;
    }
}

id<MTLLibrary> QRhiMetalData::createMetalLib(const QBakedShader &shader, QString *error, QByteArray *entryPoint)
{
    QBakedShader::Shader mtllib = shader.shader({ QBakedShader::MetalLibShader, 12 });
    if (!mtllib.shader.isEmpty()) {
        dispatch_data_t data = dispatch_data_create(mtllib.shader.constData(),
                                                    mtllib.shader.size(),
                                                    dispatch_get_global_queue(0, 0),
                                                    DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        NSError *err = nil;
        id<MTLLibrary> lib = [dev newLibraryWithData: data error: &err];
        dispatch_release(data);
        if (!err) {
            *entryPoint = mtllib.entryPoint;
            return lib;
        } else {
            const QString msg = QString::fromNSString(err.localizedDescription);
            qWarning("Failed to load metallib from baked shader: %s", qPrintable(msg));
        }
    }

    QBakedShader::Shader mslSource = shader.shader({ QBakedShader::MslShader, 12 });
    if (mslSource.shader.isEmpty()) {
        qWarning() << "No MSL 1.2 code found in baked shader" << shader;
        return nil;
    }

    NSString *src = [NSString stringWithUTF8String: mslSource.shader.constData()];
    MTLCompileOptions *opts = [[MTLCompileOptions alloc] init];
    opts.languageVersion = MTLLanguageVersion1_2;
    NSError *err = nil;
    id<MTLLibrary> lib = [dev newLibraryWithSource: src options: opts error: &err];
    [opts release];
    // src is autoreleased

    if (err) {
        const QString msg = QString::fromNSString(err.localizedDescription);
        *error = msg;
        return nil;
    }

    *entryPoint = mslSource.entryPoint;
    return lib;
}

id<MTLFunction> QRhiMetalData::createMSLShaderFunction(id<MTLLibrary> lib, const QByteArray &entryPoint)
{
    NSString *name = [NSString stringWithUTF8String: entryPoint.constData()];
    id<MTLFunction> f = [lib newFunctionWithName: name];
    [name release];
    return f;
}

bool QMetalGraphicsPipeline::build()
{
    if (d->ps)
        release();

    QRHI_RES_RHI(QRhiMetal);

    // same binding space for vertex and constant buffers - work it around
    const int firstVertexBinding = QRHI_RES(QMetalShaderResourceBindings, m_shaderResourceBindings)->maxBinding + 1;

    MTLVertexDescriptor *inputLayout = [MTLVertexDescriptor vertexDescriptor];
    for (const QRhiVertexInputLayout::Attribute &attribute : m_vertexInputLayout.attributes) {
        inputLayout.attributes[attribute.location].format = toMetalAttributeFormat(attribute.format);
        inputLayout.attributes[attribute.location].offset = attribute.offset;
        inputLayout.attributes[attribute.location].bufferIndex = firstVertexBinding + attribute.binding;
    }
    for (int i = 0; i < m_vertexInputLayout.bindings.count(); ++i) {
        const QRhiVertexInputLayout::Binding &binding(m_vertexInputLayout.bindings[i]);
        const int layoutIdx = firstVertexBinding + i;
        inputLayout.layouts[layoutIdx].stepFunction =
                binding.classification == QRhiVertexInputLayout::Binding::PerInstance
                ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
        inputLayout.layouts[layoutIdx].stepRate = binding.instanceStepRate;
        inputLayout.layouts[layoutIdx].stride = binding.stride;
    }

    MTLRenderPipelineDescriptor *rpDesc = [[MTLRenderPipelineDescriptor alloc] init];

    rpDesc.vertexDescriptor = inputLayout;

    if (@available(macOS 10.13, iOS 11.0, *)) {
        // Everything is immutable because we can guarantee that "neither the
        // CPU nor the GPU will modify a buffer's contents between the time the
        // buffer is set in a function's argument table and the time its
        // associated command buffer completes execution" (as that's the point
        // of our Vulkan-style buffer juggling in the first place).
        const int vertexBufferCount = firstVertexBinding + m_vertexInputLayout.bindings.count(); // cbuf + vbuf
        const int fragmentBufferCount = firstVertexBinding; // cbuf
        for (int i = 0; i < vertexBufferCount; ++i)
            rpDesc.vertexBuffers[i].mutability = MTLMutabilityImmutable;
        for (int i = 0; i < fragmentBufferCount; ++i)
            rpDesc.fragmentBuffers[i].mutability = MTLMutabilityImmutable;
    }

    for (const QRhiGraphicsShaderStage &shaderStage : qAsConst(m_shaderStages)) {
        QString error;
        QByteArray entryPoint;
        id<MTLLibrary> lib = rhiD->d->createMetalLib(shaderStage.shader, &error, &entryPoint);
        if (!lib) {
            qWarning("MSL shader compilation failed: %s", qPrintable(error));
            return false;
        }
        id<MTLFunction> func = rhiD->d->createMSLShaderFunction(lib, entryPoint);
        if (!func) {
            qWarning("MSL function for entry point %s not found", entryPoint.constData());
            [lib release];
            return false;
        }
        switch (shaderStage.type) {
        case QRhiGraphicsShaderStage::Vertex:
            rpDesc.vertexFunction = func;
            d->vsLib = lib;
            d->vsFunc = func;
            break;
        case QRhiGraphicsShaderStage::Fragment:
            rpDesc.fragmentFunction = func;
            d->fsLib = lib;
            d->fsFunc = func;
            break;
        default:
            [func release];
            [lib release];
            break;
        }
    }

    QMetalRenderPassDescriptor *rpD = QRHI_RES(QMetalRenderPassDescriptor, m_renderPassDesc);

    if (rpD->colorAttachmentCount) {
        // defaults when no targetBlends are provided
        rpDesc.colorAttachments[0].pixelFormat = MTLPixelFormat(rpD->colorFormat[0]);
        rpDesc.colorAttachments[0].writeMask = MTLColorWriteMaskAll;
        rpDesc.colorAttachments[0].blendingEnabled = false;

        for (int i = 0, ie = m_targetBlends.count(); i != ie; ++i) {
            const QRhiGraphicsPipeline::TargetBlend &b(m_targetBlends[i]);
            rpDesc.colorAttachments[i].pixelFormat = MTLPixelFormat(rpD->colorFormat[i]);
            rpDesc.colorAttachments[i].blendingEnabled = b.enable;
            rpDesc.colorAttachments[i].sourceRGBBlendFactor = toMetalBlendFactor(b.srcColor);
            rpDesc.colorAttachments[i].destinationRGBBlendFactor = toMetalBlendFactor(b.dstColor);
            rpDesc.colorAttachments[i].rgbBlendOperation = toMetalBlendOp(b.opColor);
            rpDesc.colorAttachments[i].sourceAlphaBlendFactor = toMetalBlendFactor(b.srcAlpha);
            rpDesc.colorAttachments[i].destinationAlphaBlendFactor = toMetalBlendFactor(b.dstAlpha);
            rpDesc.colorAttachments[i].alphaBlendOperation = toMetalBlendOp(b.opAlpha);
            rpDesc.colorAttachments[i].writeMask = toMetalColorWriteMask(b.colorWrite);
        }
    }

    if (rpD->hasDepthStencil) {
        // Must only be set when a depth-stencil buffer will actually be bound,
        // validation blows up otherwise.
        MTLPixelFormat fmt = MTLPixelFormat(rpD->dsFormat);
        rpDesc.depthAttachmentPixelFormat = fmt;
        if (fmt != MTLPixelFormatDepth16Unorm && fmt != MTLPixelFormatDepth32Float)
            rpDesc.stencilAttachmentPixelFormat = fmt;
    }

    rpDesc.sampleCount = rhiD->effectiveSampleCount(m_sampleCount);

    NSError *err = nil;
    d->ps = [rhiD->d->dev newRenderPipelineStateWithDescriptor: rpDesc error: &err];
    if (!d->ps) {
        const QString msg = QString::fromNSString(err.localizedDescription);
        qWarning("Failed to create render pipeline state: %s", qPrintable(msg));
        [rpDesc release];
        return false;
    }
    [rpDesc release];

    MTLDepthStencilDescriptor *dsDesc = [[MTLDepthStencilDescriptor alloc] init];
    dsDesc.depthCompareFunction = m_depthTest ? toMetalCompareOp(m_depthOp) : MTLCompareFunctionAlways;
    dsDesc.depthWriteEnabled = m_depthWrite;
    if (m_stencilTest) {
        dsDesc.frontFaceStencil = [[MTLStencilDescriptor alloc] init];
        dsDesc.frontFaceStencil.stencilFailureOperation = toMetalStencilOp(m_stencilFront.failOp);
        dsDesc.frontFaceStencil.depthFailureOperation = toMetalStencilOp(m_stencilFront.depthFailOp);
        dsDesc.frontFaceStencil.depthStencilPassOperation = toMetalStencilOp(m_stencilFront.passOp);
        dsDesc.frontFaceStencil.stencilCompareFunction = toMetalCompareOp(m_stencilFront.compareOp);
        dsDesc.frontFaceStencil.readMask = m_stencilReadMask;
        dsDesc.frontFaceStencil.writeMask = m_stencilWriteMask;

        dsDesc.backFaceStencil = [[MTLStencilDescriptor alloc] init];
        dsDesc.backFaceStencil.stencilFailureOperation = toMetalStencilOp(m_stencilBack.failOp);
        dsDesc.backFaceStencil.depthFailureOperation = toMetalStencilOp(m_stencilBack.depthFailOp);
        dsDesc.backFaceStencil.depthStencilPassOperation = toMetalStencilOp(m_stencilBack.passOp);
        dsDesc.backFaceStencil.stencilCompareFunction = toMetalCompareOp(m_stencilBack.compareOp);
        dsDesc.backFaceStencil.readMask = m_stencilReadMask;
        dsDesc.backFaceStencil.writeMask = m_stencilWriteMask;
    }

    d->ds = [rhiD->d->dev newDepthStencilStateWithDescriptor: dsDesc];
    [dsDesc release];

    d->primitiveType = toMetalPrimitiveType(m_topology);
    d->winding = m_frontFace == CCW ? MTLWindingCounterClockwise : MTLWindingClockwise;
    d->cullMode = toMetalCullMode(m_cullMode);

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QMetalCommandBuffer::QMetalCommandBuffer(QRhiImplementation *rhi)
    : QRhiCommandBuffer(rhi),
      d(new QMetalCommandBufferData)
{
    resetState();
}

QMetalCommandBuffer::~QMetalCommandBuffer()
{
    delete d;
}

void QMetalCommandBuffer::release()
{
    Q_UNREACHABLE();
}

void QMetalCommandBuffer::resetState()
{
    currentTarget = nullptr;
    currentPipeline = nullptr;
    currentPipelineGeneration = 0;
    currentSrb = nullptr;
    currentSrbGeneration = 0;
    currentIndexBuffer = nullptr;
    d->currentPassEncoder = nil;
    d->currentPassRpDesc = nil;
}

QMetalSwapChain::QMetalSwapChain(QRhiImplementation *rhi)
    : QRhiSwapChain(rhi),
      rtWrapper(rhi),
      cbWrapper(rhi),
      d(new QMetalSwapChainData)
{
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
        d->msaaTex[i] = nil;
}

QMetalSwapChain::~QMetalSwapChain()
{
    delete d;
}

void QMetalSwapChain::release()
{
    d->layer = nullptr;
    if (d->sem) {
        dispatch_release(d->sem);
        d->sem = nullptr;
    }
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        [d->msaaTex[i] release];
        d->msaaTex[i] = nil;
    }
}

QRhiCommandBuffer *QMetalSwapChain::currentFrameCommandBuffer()
{
    return &cbWrapper;
}

QRhiRenderTarget *QMetalSwapChain::currentFrameRenderTarget()
{
    return &rtWrapper;
}

QSize QMetalSwapChain::surfacePixelSize()
{
    NSView *v = (NSView *) m_window->winId();
    if (v) {
        CAMetalLayer *layer = (CAMetalLayer *) [v layer];
        if (layer) {
            CGSize size = [layer drawableSize];
            return QSize(size.width, size.height);
        }
    }
    return QSize();
}

QRhiRenderPassDescriptor *QMetalSwapChain::newCompatibleRenderPassDescriptor()
{
    chooseFormats(); // ensure colorFormat and similar are filled out

    QRHI_RES_RHI(QRhiMetal);
    QMetalRenderPassDescriptor *rpD = new QMetalRenderPassDescriptor(rhi);
    rpD->colorAttachmentCount = 1;
    rpD->hasDepthStencil = m_depthStencil != nullptr;

    rpD->colorFormat[0] = d->colorFormat;

    // m_depthStencil may not be built yet so cannot rely on computed fields in it
    rpD->dsFormat = rhiD->d->dev.depth24Stencil8PixelFormatSupported
            ? MTLPixelFormatDepth24Unorm_Stencil8 : MTLPixelFormatDepth32Float_Stencil8;

    return rpD;
}

void QMetalSwapChain::chooseFormats()
{
    QRHI_RES_RHI(QRhiMetal);
    samples = rhiD->effectiveSampleCount(m_sampleCount);
    d->colorFormat = MTLPixelFormatBGRA8Unorm;
}

bool QMetalSwapChain::buildOrResize()
{
    // no release(), this is intentional

    Q_ASSERT(m_window);
    if (m_window->surfaceType() != QSurface::MetalSurface) {
        qWarning("QMetalSwapChain only supports MetalSurface windows");
        return false;
    }

    NSView *v = (NSView *) m_window->winId();
    d->layer = (CAMetalLayer *) [v layer];
    Q_ASSERT(d->layer);

    m_currentPixelSize = surfacePixelSize();
    pixelSize = m_currentPixelSize;

    QRHI_RES_RHI(QRhiMetal);
    [d->layer setDevice: rhiD->d->dev];

    if (!d->sem)
        d->sem = dispatch_semaphore_create(QMTL_FRAMES_IN_FLIGHT);

    currentFrame = 0;

    chooseFormats();

    ds = m_depthStencil ? QRHI_RES(QMetalRenderBuffer, m_depthStencil) : nullptr;
    if (m_depthStencil && m_depthStencil->sampleCount() != m_sampleCount) {
        qWarning("Depth-stencil buffer's sampleCount (%d) does not match color buffers' sample count (%d). Expect problems.",
                 m_depthStencil->sampleCount(), m_sampleCount);
    }
    if (m_depthStencil && m_depthStencil->pixelSize() != pixelSize) {
        qWarning("Depth-stencil buffer's size (%dx%d) does not match the layer size (%dx%d). Expect problems.",
                 m_depthStencil->pixelSize().width(), m_depthStencil->pixelSize().height(),
                 pixelSize.width(), pixelSize.height());
    }

    rtWrapper.d->pixelSize = pixelSize;
    rtWrapper.d->colorAttCount = 1;
    rtWrapper.d->dsAttCount = ds ? 1 : 0;

    qDebug("got CAMetalLayer, size %dx%d", pixelSize.width(), pixelSize.height());

    if (samples > 1) {
        MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
        desc.textureType = MTLTextureType2DMultisample;
        desc.pixelFormat = d->colorFormat;
        desc.width = pixelSize.width();
        desc.height = pixelSize.height();
        desc.sampleCount = samples;
        desc.resourceOptions = MTLResourceStorageModePrivate;
        desc.storageMode = MTLStorageModePrivate;
        desc.usage = MTLTextureUsageRenderTarget;
        for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
            [d->msaaTex[i] release];
            d->msaaTex[i] = [rhiD->d->dev newTextureWithDescriptor: desc];
        }
        [desc release];
    }

    return true;
}

QT_END_NAMESPACE
