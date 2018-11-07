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
#include <QBakedShader>
#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

QT_BEGIN_NAMESPACE

/*
    Metal backend. Double buffers and throttles to vsync. "Dynamic" buffers are
    Shared (host visible) and duplicated (due to 2 frames in flight), while
    "static" buffers are Managed on macOS and Shared on iOS/tvOS.
*/

#if __has_feature(objc_arc)
#error ARC not supported
#endif

struct QRhiMetalData
{
    id<MTLDevice> dev;
    id<MTLCommandQueue> cmdQueue;

    MTLRenderPassDescriptor *createDefaultRenderPass(bool hasDepthStencil,
                                                     const QRhiColorClearValue &colorClearValue,
                                                     const QRhiDepthStencilClearValue &depthStencilClearValue);
    id<MTLLibrary> compileMSLShaderSource(const QBakedShader &shader, QString *error, QByteArray *entryPoint);
    id<MTLFunction> createMSLShaderFunction(id<MTLLibrary> lib, const QByteArray &entryPoint);

    struct DeferredReleaseEntry {
        enum Type {
            Buffer,
        };
        Type type;
        int lastActiveFrameSlot; // -1 if not used otherwise 0..FRAMES_IN_FLIGHT-1
        union {
            struct {
                id<MTLBuffer> buffers[QMTL_FRAMES_IN_FLIGHT];
            } buffer;
        };
    };
    QVector<DeferredReleaseEntry> releaseQueue;
};

struct QMetalBufferData
{
    id<MTLBuffer> buf[QMTL_FRAMES_IN_FLIGHT];
    QVector<QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate> pendingSharedModeUpdates[QMTL_FRAMES_IN_FLIGHT];
};

struct QMetalCommandBufferData
{
    id<MTLCommandBuffer> cb;
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
    dispatch_semaphore_t sem;
    struct FrameData {
        id<MTLCommandBuffer> cb;
        id<MTLRenderCommandEncoder> currentPassEncoder;
        MTLRenderPassDescriptor *currentPassRpDesc;
    } frame[QMTL_FRAMES_IN_FLIGHT];
    MTLRenderPassDescriptor *rp = nullptr;
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
    return { 1 };
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
    return QMatrix4x4(); // identity
}

QRhiRenderBuffer *QRhiMetal::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize,
                                                int sampleCount, QRhiRenderBuffer::Hints hints)
{
    return new QMetalRenderBuffer(this, type, pixelSize, sampleCount, hints);
}

QRhiTexture *QRhiMetal::createTexture(QRhiTexture::Format format, const QSize &pixelSize, QRhiTexture::Flags flags)
{
    return new QMetalTexture(this, format, pixelSize, flags);
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
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);

    for (const QRhiShaderResourceBinding &b : qAsConst(srbD->sortedBindings)) {
        switch (b.type) {
        case QRhiShaderResourceBinding::UniformBuffer:
        {
            QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, b.ubuf.buf);
            Q_ASSERT(bufD->m_usage.testFlag(QRhiBuffer::UniformBuffer));
            bufD->lastActiveFrameSlot = currentFrameSlot;
            executeBufferHostWritesForCurrentFrame(bufD);
            id<MTLBuffer> mtlbuf = bufD->d->buf[currentFrameSlot]; // ###
            if (b.stage.testFlag(QRhiShaderResourceBinding::VertexStage))
                [frame.currentPassEncoder setVertexBuffer: mtlbuf offset: b.ubuf.offset atIndex: b.binding];
            if (b.stage.testFlag(QRhiShaderResourceBinding::FragmentStage))
                [frame.currentPassEncoder setFragmentBuffer: mtlbuf offset: b.ubuf.offset atIndex: b.binding];
        }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
            QRHI_RES(QMetalTexture, b.stex.tex)->lastActiveFrameSlot = currentFrameSlot;
            QRHI_RES(QMetalSampler, b.stex.sampler)->lastActiveFrameSlot = currentFrameSlot;
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
        [frame.currentPassEncoder setRenderPipelineState: psD->d->ps];
        [frame.currentPassEncoder setDepthStencilState: psD->d->ds];
        [frame.currentPassEncoder setCullMode: psD->d->cullMode];
        [frame.currentPassEncoder setFrontFacingWinding: psD->d->winding];
    }
    psD->lastActiveFrameSlot = currentFrameSlot;
}

void QRhiMetal::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<QRhi::VertexInput> &bindings,
                               QRhiBuffer *indexBuf, quint32 indexOffset, QRhi::IndexFormat indexFormat)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);
    // same binding space for vertex and constant buffers - work it around
    const int firstVertexBinding = QRHI_RES(QMetalShaderResourceBindings, cbD->currentSrb)->maxBinding + 1;
    // ### batch
    for (int i = 0; i < bindings.count(); ++i) {
        QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, bindings[i].first);
        executeBufferHostWritesForCurrentFrame(bufD);
        id<MTLBuffer> mtlbuf = bufD->d->buf[currentFrameSlot]; // ###
        [frame.currentPassEncoder setVertexBuffer: mtlbuf offset: bindings[i].second atIndex: firstVertexBinding + startBinding + i];
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
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);
    [frame.currentPassEncoder setViewport: vp];
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
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);
    [frame.currentPassEncoder setScissorRect: s];
}

void QRhiMetal::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);
    [frame.currentPassEncoder setBlendColorRed: c.x() green: c.y() blue: c.z() alpha: c.w()];
}

void QRhiMetal::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);
    [frame.currentPassEncoder setStencilReferenceValue: refValue];
}

void QRhiMetal::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                     quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);
    [frame.currentPassEncoder drawPrimitives:
        QRHI_RES(QMetalGraphicsPipeline, cbD->currentPipeline)->d->primitiveType
      vertexStart: firstVertex vertexCount: vertexCount instanceCount: instanceCount baseInstance: firstInstance];
}

void QRhiMetal::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                            quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
    Q_ASSERT(inPass);
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);
//    [frame.currentPassEncoder drawIndexedPrimitives:
//        QRHI_RES(QMetalGraphicsPipeline, cbD->currentPipeline)->d->primitiveType
//      indexCount: indexCount indexType: type indexBuffer: buf indexBufferOffset: vertexOffset
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

    currentFrameSwapChain = swapChainD;
    currentFrameSlot = swapChainD->currentFrame;

    QMetalSwapChainData::FrameData &frame(swapChainD->d->frame[currentFrameSlot]);
    frame.cb = [d->cmdQueue commandBufferWithUnretainedReferences];
    swapChainD->cbWrapper.d->cb = frame.cb;
    swapChainD->cbWrapper.resetState();

    executeDeferredReleases();

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiMetal::endFrame(QRhiSwapChain *swapChain)
{
    Q_ASSERT(inFrame);
    inFrame = false;

    QMetalSwapChain *swapChainD = QRHI_RES(QMetalSwapChain, swapChain);
    QMetalSwapChainData::FrameData &frame(swapChainD->d->frame[currentFrameSlot]);

    [frame.cb presentDrawable: swapChainD->d->curDrawable];

    __block dispatch_semaphore_t sem = swapChainD->d->sem;
    [frame.cb addCompletedHandler: ^(id<MTLCommandBuffer> cb) {
        dispatch_semaphore_signal(sem);
    }];

    [frame.cb commit];

    currentFrameSwapChain = nullptr;
    swapChainD->currentFrame = (swapChainD->currentFrame + 1) % QMTL_FRAMES_IN_FLIGHT;

    ++finishedFrameCount;
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

void QRhiMetal::commitResourceUpdates(QRhiResourceUpdateBatch *resourceUpdates)
{
    QRhiResourceUpdateBatchPrivate *ud = QRhiResourceUpdateBatchPrivate::get(resourceUpdates);

    for (const QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate &u : ud->dynamicBufferUpdates) {
        QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, u.buf);
        //Q_ASSERT(u.buf->type == QRhiBuffer::Dynamic);
        for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
            bufD->d->pendingSharedModeUpdates[i].append(u);
    }

    for (const QRhiResourceUpdateBatchPrivate::StaticBufferUpload &u : ud->staticBufferUploads) {
        QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, u.buf);
        //Q_ASSERT(u.buf->type != QRhiBuffer::Dynamic);
        Q_ASSERT(u.data.size() == bufD->m_size);
        for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
            bufD->d->pendingSharedModeUpdates[i].append({ u.buf, 0, u.data.size(), u.data.constData() });
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureUpload &u : ud->textureUploads) {
        // ###
    }

    ud->free();
}

void QRhiMetal::executeBufferHostWritesForCurrentFrame(QMetalBuffer *bufD)
{
    QVector<QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate> &updates(bufD->d->pendingSharedModeUpdates[currentFrameSlot]);
    if (updates.isEmpty())
        return;

    void *p = [bufD->d->buf[currentFrameSlot] contents];
    for (const QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate &u : updates) {
        Q_ASSERT(bufD == QRHI_RES(QMetalBuffer, u.buf));
        memcpy(static_cast<char *>(p) + u.offset, u.data.constData(), u.data.size());
    }

    updates.clear();
}

void QRhiMetal::beginPass(QRhiRenderTarget *rt,
                          QRhiCommandBuffer *cb,
                          const QRhiColorClearValue &colorClearValue,
                          const QRhiDepthStencilClearValue &depthStencilClearValue,
                          QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(!inPass);

    if (resourceUpdates)
        commitResourceUpdates(resourceUpdates);

    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QMetalSwapChainData::FrameData &frame(currentFrameSwapChain->d->frame[currentFrameSlot]);

    QMetalBasicRenderTargetData *rtD = nullptr;
    switch (rt->type()) {
    case QRhiRenderTarget::RtRef:
        rtD = &QRHI_RES(QMetalReferenceRenderTarget, rt)->d;
        frame.currentPassRpDesc = d->createDefaultRenderPass(false, colorClearValue, depthStencilClearValue);
        frame.currentPassRpDesc.colorAttachments[0].texture = currentFrameSwapChain->d->curDrawable.texture;
        break;
    case QRhiRenderTarget::RtTexture:
    {
        QMetalTextureRenderTarget *rtTex = QRHI_RES(QMetalTextureRenderTarget, rt);
        rtD = &rtTex->d;
        frame.currentPassRpDesc = d->createDefaultRenderPass(false, colorClearValue, depthStencilClearValue);
        if (rtTex->m_flags.testFlag(QRhiTextureRenderTarget::PreserveColorContents))
            frame.currentPassRpDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
        // ###
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    frame.currentPassEncoder = [frame.cb renderCommandEncoderWithDescriptor: frame.currentPassRpDesc];

    cbD->currentSwapChain = currentFrameSwapChain;
    cbD->currentTarget = rt;

    inPass = true;
}

void QRhiMetal::endPass(QRhiCommandBuffer *cb)
{
    Q_ASSERT(inPass);
    inPass = false;

    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QMetalSwapChainData::FrameData &frame(cbD->currentSwapChain->d->frame[currentFrameSlot]);
    [frame.currentPassEncoder endEncoding];

    cbD->currentSwapChain = nullptr;
    cbD->currentTarget = nullptr;
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
        d->pendingSharedModeUpdates[i].clear();
    }

    QRHI_RES_RHI(QRhiMetal);
    rhiD->d->releaseQueue.append(e);
}

bool QMetalBuffer::build()
{
    if (d->buf[0])
        release();

    const int roundedSize = m_usage.testFlag(QRhiBuffer::UniformBuffer) ? aligned(m_size, 256) : m_size;

    // ### for now everything host visible and double buffered
    // should instead use Managed on macOS for immutable/static
    MTLResourceOptions opts = MTLResourceStorageModeShared;

    QRHI_RES_RHI(QRhiMetal);
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        d->buf[i] = [rhiD->d->dev newBufferWithLength: roundedSize options: opts];
        d->pendingSharedModeUpdates[i].reserve(16);
    }

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QMetalRenderBuffer::QMetalRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                                       int sampleCount, QRhiRenderBuffer::Hints hints)
    : QRhiRenderBuffer(rhi, type, pixelSize, sampleCount, hints)
{
}

void QMetalRenderBuffer::release()
{
}

bool QMetalRenderBuffer::build()
{
    return true;
}

QMetalTexture::QMetalTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize, Flags flags)
    : QRhiTexture(rhi, format, pixelSize, flags)
{
}

void QMetalTexture::release()
{
}

bool QMetalTexture::build()
{
    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QMetalSampler::QMetalSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                             AddressMode u, AddressMode v, AddressMode w)
    : QRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v, w)
{
}

void QMetalSampler::release()
{
}

bool QMetalSampler::build()
{
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
      d(rhi)
{
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
    return d.pixelSize;
}

QMetalTextureRenderTarget::QMetalTextureRenderTarget(QRhiImplementation *rhi,
                                                     const QRhiTextureRenderTargetDescription &desc,
                                                     Flags flags)
    : QRhiTextureRenderTarget(rhi, desc, flags),
      d(rhi)
{
}

void QMetalTextureRenderTarget::release()
{
}

QRhiRenderPassDescriptor *QMetalTextureRenderTarget::newCompatibleRenderPassDescriptor()
{
    QMetalRenderPassDescriptor *rpD = new QMetalRenderPassDescriptor(rhi);
    rpD->hasDepthStencil = m_desc.depthStencilBuffer || m_desc.depthTexture;
    return rpD;
}

bool QMetalTextureRenderTarget::build()
{
    return true;
}

QRhiRenderTarget::Type QMetalTextureRenderTarget::type() const
{
    return RtTexture;
}

QSize QMetalTextureRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
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

id<MTLLibrary> QRhiMetalData::compileMSLShaderSource(const QBakedShader &shader, QString *error, QByteArray *entryPoint)
{
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
        inputLayout.layouts[layoutIdx].stepRate = 1;
        inputLayout.layouts[layoutIdx].stride = binding.stride;
    }

    MTLRenderPipelineDescriptor *rpDesc = [[MTLRenderPipelineDescriptor alloc] init];
    rpDesc.vertexDescriptor = inputLayout;

    for (const QRhiGraphicsShaderStage &shaderStage : qAsConst(m_shaderStages)) {
        QString error;
        QByteArray entryPoint;
        id<MTLLibrary> lib = rhiD->d->compileMSLShaderSource(shaderStage.shader, &error, &entryPoint);
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

    int vertexBufferCount = 0; // ###
    int fragmentBufferCount = 0;
    if (@available(macOS 10.13, iOS 11.0, *)) {
        for (int i = 0; i < vertexBufferCount; ++i)
            rpDesc.vertexBuffers[i].mutability = MTLMutabilityImmutable;
        for (int i = 0; i < fragmentBufferCount; ++i)
            rpDesc.fragmentBuffers[i].mutability = MTLMutabilityImmutable;
    }

    rpDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    rpDesc.colorAttachments[0].writeMask = MTLColorWriteMaskAll;
    rpDesc.colorAttachments[0].blendingEnabled = false;

    for (int i = 0, ie = m_targetBlends.count(); i != ie; ++i) {
        const QRhiGraphicsPipeline::TargetBlend &b(m_targetBlends[i]);
        rpDesc.colorAttachments[i].pixelFormat = MTLPixelFormatBGRA8Unorm;
        rpDesc.colorAttachments[i].blendingEnabled = b.enable;
        rpDesc.colorAttachments[i].sourceRGBBlendFactor = toMetalBlendFactor(b.srcColor);
        rpDesc.colorAttachments[i].destinationRGBBlendFactor = toMetalBlendFactor(b.dstColor);
        rpDesc.colorAttachments[i].rgbBlendOperation = toMetalBlendOp(b.opColor);
        rpDesc.colorAttachments[i].sourceAlphaBlendFactor = toMetalBlendFactor(b.srcAlpha);
        rpDesc.colorAttachments[i].destinationAlphaBlendFactor = toMetalBlendFactor(b.dstAlpha);
        rpDesc.colorAttachments[i].alphaBlendOperation = toMetalBlendOp(b.opAlpha);
        rpDesc.colorAttachments[i].writeMask = toMetalColorWriteMask(b.colorWrite);
    }

    QMetalRenderPassDescriptor *rpD = QRHI_RES(QMetalRenderPassDescriptor, m_renderPassDesc);
    if (rpD->hasDepthStencil) {
        // Must only be set when a depth-stencil buffer will actually be bound,
        // validation blows up otherwise.
        if (rhiD->d->dev.depth24Stencil8PixelFormatSupported) {
            rpDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth24Unorm_Stencil8;
            rpDesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth24Unorm_Stencil8;
        } else {
            rpDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
            rpDesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
        }
    }

    rpDesc.sampleCount = 1;

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

QMetalSwapChain::QMetalSwapChain(QRhiImplementation *rhi)
    : QRhiSwapChain(rhi),
      rtWrapper(rhi),
      cbWrapper(rhi),
      d(new QMetalSwapChainData)
{
}

QMetalSwapChain::~QMetalSwapChain()
{
    delete d;
}

void QMetalSwapChain::release()
{
    if (!d->layer)
        return;

    d->layer = nullptr;

    dispatch_release(d->sem);
}

QRhiCommandBuffer *QMetalSwapChain::currentFrameCommandBuffer()
{
    return &cbWrapper;
}

QRhiRenderTarget *QMetalSwapChain::currentFrameRenderTarget()
{
    return &rtWrapper;
}

QSize QMetalSwapChain::effectivePixelSize() const
{
    return pixelSize;
}

QRhiRenderPassDescriptor *QMetalSwapChain::newCompatibleRenderPassDescriptor()
{
    QMetalRenderPassDescriptor *rpD = new QMetalRenderPassDescriptor(rhi);
    rpD->hasDepthStencil = m_depthStencil != nullptr;
    return rpD;
}

bool QMetalSwapChain::buildOrResize()
{
    if (d->layer)
        release();

    Q_ASSERT(m_window);
    if (m_window->surfaceType() != QSurface::MetalSurface) {
        qWarning("QMetalSwapChain only supports MetalSurface windows");
        return false;
    }

    NSView *v = (NSView *) m_window->winId();
    d->layer = (CAMetalLayer *) [v layer];
    Q_ASSERT(d->layer);

    CGSize size = [d->layer drawableSize];
    pixelSize = QSize(size.width, size.height);

    QRHI_RES_RHI(QRhiMetal);
    [d->layer setDevice: rhiD->d->dev];

    d->sem = dispatch_semaphore_create(QMTL_FRAMES_IN_FLIGHT);
    currentFrame = 0;

    ds = m_depthStencil ? QRHI_RES(QMetalRenderBuffer, m_depthStencil) : nullptr;

    rtWrapper.d.pixelSize = pixelSize;
    rtWrapper.d.colorAttCount = 1;
    rtWrapper.d.dsAttCount = ds ? 1 : 0;

    qDebug("got CAMetalLayer, size %dx%d", pixelSize.width(), pixelSize.height());

    return true;
}

QT_END_NAMESPACE
