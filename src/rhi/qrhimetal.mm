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
#include "qrhirsh_p.h"
#include <QGuiApplication>
#include <QWindow>
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
    used to upload data to them. Does not rely on strong objects refs from
    command buffers (hence uses commandBufferWithUnretainedReferences), but
    does rely on automatic dependency tracking between encoders (hence no
    MTLResourceHazardTrackingModeUntracked atm).
*/

#if __has_feature(objc_arc)
#error ARC not supported
#endif

// Note: we expect everything here pass the Metal API validation when running
// in Debug mode in XCode. Some of the issues that break validation are not
// obvious and not visible when running outside XCode.
//
// An exception is the nextDrawable Called Early blah blah warning, which is
// plain and simply false.

/*!
    \class QRhiMetalInitParams
    \inmodule QtRhi
    \brief Metal specific initialization parameters.

    A Metal-based QRhi needs no special parameters for initialization.

    \badcode
        QRhiMetalInitParams params;
        rhi = QRhi::create(QRhi::Metal, &params);
    \endcode

    \note Metal API validation cannot be enabled by the application. Instead,
    run the debug build of the application in XCode. Generating a
    \c{.xcodeproj} file via \c{qmake -spec macx-xcode} provides a convenient
    way to enable this.

    \note QRhiSwapChain can only target QWindow instances that have their
    surface type set to QSurface::MetalSurface.

    \section2 Working with existing Metal devices

    When interoperating with another graphics engine, it may be necessary to
    get a QRhi instance that uses the same Metal device. This can be achieved
    by passing a pointer to a QRhiMetalNativeHandles to QRhi::create(). The
    device must be set to a non-null value then. Optionally, a command queue
    object can be specified as well.

    The QRhi does not take ownership of any of the external objects.
 */

/*!
    \class QRhiMetalNativeHandles
    \inmodule QtRhi
    \brief Holds the Metal device used by the QRhi.

    \note The class uses \c{void *} as the type since including the Objective C
    headers is not acceptable here. The actual types are \c{id<MTLDevice>} and
    \c{id<MTLCommandQueue>}.
 */

/*!
    \class QRhiMetalTextureNativeHandles
    \inmodule QtRhi
    \brief Holds the Metal texture object that is backing a QRhiTexture instance.

    \note The class uses \c{void *} as the type since including the Objective C
    headers is not acceptable here. The actual type is \c{id<MTLTexture>}.
 */

struct QRhiMetalData
{
    QRhiMetalData(QRhiImplementation *rhi) : ofr(rhi) { }

    id<MTLDevice> dev = nil;
    id<MTLCommandQueue> cmdQueue = nil;

    MTLRenderPassDescriptor *createDefaultRenderPass(bool hasDepthStencil,
                                                     const QRhiColorClearValue &colorClearValue,
                                                     const QRhiDepthStencilClearValue &depthStencilClearValue);
    id<MTLLibrary> createMetalLib(const QBakedShader &shader, QBakedShaderKey::ShaderVariant shaderVariant,
                                  QString *error, QByteArray *entryPoint);
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

    static void executeDeferredReleasesOnRshNow(QVector<DeferredReleaseEntry> *rshRelQueue);

    struct OffscreenFrame {
        OffscreenFrame(QRhiImplementation *rhi) : cbWrapper(rhi) { }
        bool active = false;
        QMetalCommandBuffer cbWrapper;
    } ofr;

    struct ActiveReadback {
        int activeFrameSlot = -1;
        QRhiReadbackDescription desc;
        QRhiReadbackResult *result;
        id<MTLBuffer> buf;
        quint32 bufSize;
        QSize pixelSize;
        QRhiTexture::Format format;
    };
    QVector<ActiveReadback> activeReadbacks;

    API_AVAILABLE(macos(10.13), ios(11.0)) MTLCaptureManager *captureMgr;
    API_AVAILABLE(macos(10.13), ios(11.0)) id<MTLCaptureScope> captureScope = nil;
};

Q_DECLARE_TYPEINFO(QRhiMetalData::DeferredReleaseEntry, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiMetalData::ActiveReadback, Q_MOVABLE_TYPE);

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
    bool owns = true;
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
    bool shaderResourceBindingsValid;
    int currentFirstVertexBinding;
    QRhiBatchedBindings<id<MTLBuffer> > currentVertexInputsBuffers;
    QRhiBatchedBindings<NSUInteger> currentVertexInputOffsets;
};

struct QMetalRenderTargetData
{
    QSize pixelSize;
    float dpr = 1;
    int colorAttCount = 0;
    int dsAttCount = 0;

    struct ColorAtt {
        bool needsDrawableForTex = false;
        id<MTLTexture> tex = nil;
        int layer = 0;
        int level = 0;
        bool needsDrawableForResolveTex = false;
        id<MTLTexture> resolveTex = nil;
        int resolveLayer = 0;
        int resolveLevel = 0;
    };

    struct {
        ColorAtt colorAtt[QMetalRenderPassDescriptor::MAX_COLOR_ATTACHMENTS];
        id<MTLTexture> dsTex = nil;
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
    dispatch_semaphore_t sem[QMTL_FRAMES_IN_FLIGHT];
    MTLRenderPassDescriptor *rp = nullptr;
    id<MTLTexture> msaaTex[QMTL_FRAMES_IN_FLIGHT];
    QRhiTexture::Format rhiColorFormat;
    MTLPixelFormat colorFormat;
};

QRhiMetal::QRhiMetal(QRhiMetalInitParams *params, QRhiMetalNativeHandles *importDevice)
{
    d = new QRhiMetalData(this);

    if (params->resourceSharingHost)
        rsh = QRhiResourceSharingHostPrivate::get(params->resourceSharingHost);

    importedDevice = importDevice != nullptr;
    if (importedDevice) {
        if (d->dev) {
            d->dev = (id<MTLDevice>) importDevice->dev;
            importedCmdQueue = importDevice->cmdQueue != nullptr;
            if (importedCmdQueue)
                d->cmdQueue = (id<MTLCommandQueue>) importDevice->cmdQueue;
        } else {
            qWarning("No MTLDevice given, cannot import");
            importedDevice = false;
        }
    }
}

QRhiMetal::~QRhiMetal()
{
    delete d;
}

static inline uint aligned(uint v, uint byteAlign)
{
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

bool QRhiMetal::create(QRhi::Flags flags)
{
    Q_UNUSED(flags);

    QMutexLocker lock(rsh ? &rsh->mtx : nullptr);

    if (importedDevice) {
        [d->dev retain];
    } else {
        if (!rsh || !rsh->d_metal.dev) {
            d->dev = MTLCreateSystemDefaultDevice();
            if (rsh) {
                rsh->d_metal.dev = d->dev;
                [d->dev retain];
            }
        } else {
            d->dev = (id<MTLDevice>) rsh->d_metal.dev;
            [d->dev retain];
        }
    }

    qDebug("Metal device: %s", qPrintable(QString::fromNSString([d->dev name])));

    if (importedCmdQueue) {
        [d->cmdQueue retain];
    } else {
        // We could use the existing MTLCommandQueue when rsh is enabled. We
        // choose not to share the queue however, in order avoid performance
        // surprises with multiple windows and QRhis severly limiting each
        // other's rendering rate.
        d->cmdQueue = [d->dev newCommandQueue];
    }

    if (@available(macOS 10.13, iOS 11.0, *)) {
        d->captureMgr = [MTLCaptureManager sharedCaptureManager];
        // Have a custom capture scope as well which then shows up in XCode as
        // an option when capturing, and becomes especially useful when having
        // multiple windows with multiple QRhis.
        d->captureScope = [d->captureMgr newCaptureScopeWithCommandQueue: d->cmdQueue];
        const QString label = QString::asprintf("Qt capture scope for QRhi %p", this);
        d->captureScope.label = label.toNSString();
    }

#if defined(Q_OS_MACOS)
    caps.maxTextureSize = 16384;
#elif defined(Q_OS_TVOS)
    if ([d->dev supportsFeatureSet: MTLFeatureSet(30003)]) // MTLFeatureSet_tvOS_GPUFamily2_v1
        caps.maxTextureSize = 16384;
    else
        caps.maxTextureSize = 8192;
#elif defined(Q_OS_IOS)
    // welcome to feature set hell
    if ([d->dev supportsFeatureSet: MTLFeatureSet(16)] // MTLFeatureSet_iOS_GPUFamily5_v1
            || [d->dev supportsFeatureSet: MTLFeatureSet(11)] // MTLFeatureSet_iOS_GPUFamily4_v1
            || [d->dev supportsFeatureSet: MTLFeatureSet(4)]) // MTLFeatureSet_iOS_GPUFamily3_v1
    {
        caps.maxTextureSize = 16384;
    } else if ([d->dev supportsFeatureSet: MTLFeatureSet(3)] // MTLFeatureSet_iOS_GPUFamily2_v2
            || [d->dev supportsFeatureSet: MTLFeatureSet(2)]) // MTLFeatureSet_iOS_GPUFamily1_v2
    {
        caps.maxTextureSize = 8192;
    } else {
        caps.maxTextureSize = 4096;
    }
#endif

    nativeHandlesStruct.dev = d->dev;
    nativeHandlesStruct.cmdQueue = d->cmdQueue;

    if (rsh) {
        qDebug("Attached to QRhiResourceSharingHost %p, currently %d other QRhi instances on MTLDevice %p",
               rsh, rsh->rhiCount, (void *) d->dev);
        rsh->rhiCount += 1;
    }

    return true;
}

void QRhiMetal::destroy()
{
    executeDeferredReleases(true);
    finishActiveReadbacks(true);

    QMutexLocker lock(rsh ? &rsh->mtx : nullptr);

    if (@available(macOS 10.13, iOS 11.0, *)) {
        [d->captureScope release];
        d->captureScope = nil;
    }

    [d->cmdQueue release];
    if (!importedCmdQueue)
        d->cmdQueue = nil;

    [d->dev release];
    if (!importedDevice)
        d->dev = nil;

    if (rsh) {
        if (--rsh->rhiCount == 0) {
            if (rsh->d_metal.releaseQueue) {
                auto rshRelQueue = static_cast<QVector<QRhiMetalData::DeferredReleaseEntry> *>(rsh->d_metal.releaseQueue);
                QRhiMetalData::executeDeferredReleasesOnRshNow(rshRelQueue);
                delete rshRelQueue;
            }
            [(id<MTLDevice>) rsh->d_metal.dev release];
            rsh->d_metal.dev = nullptr;
            rsh->d_metal.releaseQueue = nullptr;
        }
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

bool QRhiMetal::isYUpInNDC() const
{
    return true;
}

QMatrix4x4 QRhiMetal::clipSpaceCorrMatrix() const
{
    // depth range 0..1
    static QMatrix4x4 m;
    if (m.isIdentity()) {
        // NB the ctor takes row-major
        m = QMatrix4x4(1.0f, 0.0f, 0.0f, 0.0f,
                       0.0f, 1.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 0.5f, 0.5f,
                       0.0f, 0.0f, 0.0f, 1.0f);
    }
    return m;
}

bool QRhiMetal::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const
{
    Q_UNUSED(flags);

#ifdef Q_OS_MACOS
    if (format >= QRhiTexture::ETC2_RGB8 && format <= QRhiTexture::ETC2_RGBA8)
        return false;
    if (format >= QRhiTexture::ASTC_4x4 && format <= QRhiTexture::ASTC_12x12)
        return false;
#else
    if (format >= QRhiTexture::BC1 && format <= QRhiTexture::BC7)
        return false;
#endif

    return true;
}

bool QRhiMetal::isFeatureSupported(QRhi::Feature feature) const
{
    switch (feature) {
    case QRhi::MultisampleTexture:
        return true;
    case QRhi::MultisampleRenderBuffer:
        return true;
    case QRhi::DebugMarkers:
        return true;
    case QRhi::Timestamps:
        return false;
    case QRhi::Instancing:
        return true;
    case QRhi::CustomInstanceStepRate:
        return true;
    case QRhi::PrimitiveRestart:
        return true;
    case QRhi::GeometryShaders:
        return false;
    case QRhi::TessellationShaders:
        return false; // for now
    default:
        Q_UNREACHABLE();
        return false;
    }
}

int QRhiMetal::resourceSizeLimit(QRhi::ResourceSizeLimit limit) const
{
    switch (limit) {
    case QRhi::TextureSizeMin:
        return 1;
    case QRhi::TextureSizeMax:
        return caps.maxTextureSize;
    default:
        Q_UNREACHABLE();
        return 0;
    }
}

const QRhiNativeHandles *QRhiMetal::nativeHandles()
{
    return &nativeHandlesStruct;
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

void QRhiMetal::enqueueShaderResourceBindings(QMetalShaderResourceBindings *srbD, QMetalCommandBuffer *cbD)
{
    static const int KNOWN_STAGES = 2;
    struct {
        QRhiBatchedBindings<id<MTLBuffer> > buffers;
        QRhiBatchedBindings<NSUInteger> bufferOffsets;
        QRhiBatchedBindings<id<MTLTexture> > textures;
        QRhiBatchedBindings<id<MTLSamplerState> > samplers;
    } res[KNOWN_STAGES];

    for (const QRhiShaderResourceBinding &binding : qAsConst(srbD->sortedBindings)) {
        const QRhiShaderResourceBindingPrivate *b = QRhiShaderResourceBindingPrivate::get(&binding);
        switch (b->type) {
        case QRhiShaderResourceBinding::UniformBuffer:
        {
            QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, b->u.ubuf.buf);
            id<MTLBuffer> mtlbuf = bufD->d->buf[bufD->m_type == QRhiBuffer::Immutable ? 0 : currentFrameSlot];
            if (b->stage.testFlag(QRhiShaderResourceBinding::VertexStage)) {
                res[0].buffers.feed(b->binding, mtlbuf);
                res[0].bufferOffsets.feed(b->binding, b->u.ubuf.offset);
            }
            if (b->stage.testFlag(QRhiShaderResourceBinding::FragmentStage)) {
                res[1].buffers.feed(b->binding, mtlbuf);
                res[1].bufferOffsets.feed(b->binding, b->u.ubuf.offset);
            }
        }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
        {
            QMetalTexture *texD = QRHI_RES(QMetalTexture, b->u.stex.tex);
            QMetalSampler *samplerD = QRHI_RES(QMetalSampler, b->u.stex.sampler);
            if (b->stage.testFlag(QRhiShaderResourceBinding::VertexStage)) {
                res[0].textures.feed(b->binding, texD->d->tex);
                res[0].samplers.feed(b->binding, samplerD->d->samplerState);
            }
            if (b->stage.testFlag(QRhiShaderResourceBinding::FragmentStage)) {
                res[1].textures.feed(b->binding, texD->d->tex);
                res[1].samplers.feed(b->binding, samplerD->d->samplerState);
            }
        }
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    for (int idx = 0; idx < KNOWN_STAGES; ++idx) {
        res[idx].buffers.finish();
        res[idx].bufferOffsets.finish();
        res[idx].textures.finish();
        res[idx].samplers.finish();
        for (int i = 0, ie = res[idx].buffers.batches.count(); i != ie; ++i) {
            const auto &bufferBatch(res[idx].buffers.batches[i]);
            const auto &offsetBatch(res[idx].bufferOffsets.batches[i]);
            switch (idx) {
            case 0:
                [cbD->d->currentPassEncoder setVertexBuffers: bufferBatch.resources.constData()
                  offsets: offsetBatch.resources.constData()
                  withRange: NSMakeRange(bufferBatch.startBinding, bufferBatch.resources.count())];
                break;
            case 1:
                [cbD->d->currentPassEncoder setFragmentBuffers: bufferBatch.resources.constData()
                  offsets: offsetBatch.resources.constData()
                  withRange: NSMakeRange(bufferBatch.startBinding, bufferBatch.resources.count())];
                break;
            default:
                Q_UNREACHABLE();
                break;
            }
        }
        for (int i = 0, ie = res[idx].textures.batches.count(); i != ie; ++i) {
            const auto &batch(res[idx].textures.batches[i]);
            switch (idx) {
            case 0:
                [cbD->d->currentPassEncoder setVertexTextures: batch.resources.constData()
                  withRange: NSMakeRange(batch.startBinding, batch.resources.count())];
                break;
            case 1:
                [cbD->d->currentPassEncoder setFragmentTextures: batch.resources.constData()
                  withRange: NSMakeRange(batch.startBinding, batch.resources.count())];
                break;
            default:
                Q_UNREACHABLE();
                break;
            }
        }
        for (int i = 0, ie = res[idx].samplers.batches.count(); i != ie; ++i) {
            const auto &batch(res[idx].samplers.batches[i]);
            switch (idx) {
            case 0:
                [cbD->d->currentPassEncoder setVertexSamplerStates: batch.resources.constData()
                  withRange: NSMakeRange(batch.startBinding, batch.resources.count())];
                break;
            case 1:
                [cbD->d->currentPassEncoder setFragmentSamplerStates: batch.resources.constData()
                  withRange: NSMakeRange(batch.startBinding, batch.resources.count())];
                break;
            default:
                Q_UNREACHABLE();
                break;
            }
        }
    }
}

void QRhiMetal::setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb)
{
    Q_ASSERT(inPass);

    QMetalGraphicsPipeline *psD = QRHI_RES(QMetalGraphicsPipeline, ps);
    if (!srb)
        srb = psD->m_shaderResourceBindings;

    QMetalShaderResourceBindings *srbD = QRHI_RES(QMetalShaderResourceBindings, srb);
    bool hasSlottedResourceInSrb = false;
    bool resNeedsRebind = false;

    // do buffer writes, figure out if we need to rebind, and mark as in-use
    for (int i = 0, ie = srbD->sortedBindings.count(); i != ie; ++i) {
        const QRhiShaderResourceBindingPrivate *b = QRhiShaderResourceBindingPrivate::get(&srbD->sortedBindings[i]);
        QMetalShaderResourceBindings::BoundResourceData &bd(srbD->boundResourceData[i]);
        switch (b->type) {
        case QRhiShaderResourceBinding::UniformBuffer:
        {
            QMetalBuffer *bufD = QRHI_RES(QMetalBuffer, b->u.ubuf.buf);
            Q_ASSERT(bufD->m_usage.testFlag(QRhiBuffer::UniformBuffer));
            executeBufferHostWritesForCurrentFrame(bufD);
            if (bufD->m_type != QRhiBuffer::Immutable)
                hasSlottedResourceInSrb = true;
            if (bufD->generation != bd.ubuf.generation) {
                resNeedsRebind = true;
                bd.ubuf.generation = bufD->generation;
            }
            bufD->lastActiveFrameSlot = currentFrameSlot;
        }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
        {
            QMetalTexture *texD = QRHI_RES(QMetalTexture, b->u.stex.tex);
            QMetalSampler *samplerD = QRHI_RES(QMetalSampler, b->u.stex.sampler);
            if (texD->generation != bd.stex.texGeneration
                    || samplerD->generation != bd.stex.samplerGeneration)
            {
                resNeedsRebind = true;
                bd.stex.texGeneration = texD->generation;
                bd.stex.samplerGeneration = samplerD->generation;
            }
            texD->lastActiveFrameSlot = currentFrameSlot;
            samplerD->lastActiveFrameSlot = currentFrameSlot;
        }
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    // make sure the resources for the correct slot get bound
    const int resSlot = hasSlottedResourceInSrb ? currentFrameSlot : 0;
    if (hasSlottedResourceInSrb && cbD->currentResSlot != resSlot)
        resNeedsRebind = true;

    if (cbD->currentPipeline != ps || cbD->currentPipelineGeneration != psD->generation) {
        cbD->currentPipeline = ps;
        cbD->currentPipelineGeneration = psD->generation;

        [cbD->d->currentPassEncoder setRenderPipelineState: psD->d->ps];
        [cbD->d->currentPassEncoder setDepthStencilState: psD->d->ds];
        [cbD->d->currentPassEncoder setCullMode: psD->d->cullMode];
        [cbD->d->currentPassEncoder setFrontFacingWinding: psD->d->winding];
    }

    if (!cbD->d->shaderResourceBindingsValid)
        resNeedsRebind = true;

    if (resNeedsRebind || cbD->currentSrb != srb || cbD->currentSrbGeneration != srbD->generation) {
        cbD->currentSrb = srb;
        cbD->currentSrbGeneration = srbD->generation;
        cbD->currentResSlot = resSlot;

        enqueueShaderResourceBindings(srbD, cbD);
        cbD->d->shaderResourceBindingsValid = true;
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

    if (firstVertexBinding != cbD->d->currentFirstVertexBinding
            || buffers != cbD->d->currentVertexInputsBuffers
            || offsets != cbD->d->currentVertexInputOffsets)
    {
        cbD->d->currentFirstVertexBinding = firstVertexBinding;
        cbD->d->currentVertexInputsBuffers = buffers;
        cbD->d->currentVertexInputOffsets = offsets;

        for (int i = 0, ie = buffers.batches.count(); i != ie; ++i) {
            const auto &bufferBatch(buffers.batches[i]);
            const auto &offsetBatch(offsets.batches[i]);
            [cbD->d->currentPassEncoder setVertexBuffers:
                bufferBatch.resources.constData()
              offsets: offsetBatch.resources.constData()
              withRange: NSMakeRange(firstVertexBinding + bufferBatch.startBinding, bufferBatch.resources.count())];
        }
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
    const QVector4D r = viewport.viewport();
    vp.originX = r.x();
    vp.originY = outputSize.height() - (r.y() + r.w());
    vp.width = r.z();
    vp.height = r.w();
    vp.znear = viewport.minDepth();
    vp.zfar = viewport.maxDepth();
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
    const QVector4D r = scissor.scissor();
    s.x = r.x();
    s.y = outputSize.height() - (r.y() + r.w());
    s.width = r.z();
    s.height = r.w();
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

void QRhiMetal::debugMarkBegin(QRhiCommandBuffer *cb, const QByteArray &name)
{
    if (!debugMarkers)
        return;

    NSString *str = [NSString stringWithUTF8String: name.constData()];
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    if (inPass) {
        [cbD->d->currentPassEncoder pushDebugGroup: str];
    } else {
        if (@available(macOS 10.13, iOS 11.0, *))
            [cbD->d->cb pushDebugGroup: str];
    }
}

void QRhiMetal::debugMarkEnd(QRhiCommandBuffer *cb)
{
    if (!debugMarkers)
        return;

    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    if (inPass) {
        [cbD->d->currentPassEncoder popDebugGroup];
    } else {
        if (@available(macOS 10.13, iOS 11.0, *))
            [cbD->d->cb popDebugGroup];
    }
}

void QRhiMetal::debugMarkMsg(QRhiCommandBuffer *cb, const QByteArray &msg)
{
    if (!debugMarkers)
        return;

    if (inPass) {
        QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
        [cbD->d->currentPassEncoder insertDebugSignpost: [NSString stringWithUTF8String: msg.constData()]];
    }
}

QRhi::FrameOpResult QRhiMetal::beginFrame(QRhiSwapChain *swapChain, QRhi::BeginFrameFlags flags)
{
    Q_UNUSED(flags);
    Q_ASSERT(!inFrame);
    inFrame = true;

    QMetalSwapChain *swapChainD = QRHI_RES(QMetalSwapChain, swapChain);

    // This is a bit messed up since for this swapchain we want to wait for the
    // commands+present to complete, while for others just for the commands
    // (for this same frame slot) but not sure how to do that in a sane way so
    // wait for full cb completion for now.
    for (QMetalSwapChain *sc : qAsConst(swapchains)) {
        dispatch_semaphore_t sem = sc->d->sem[swapChainD->currentFrameSlot];
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        if (sc != swapChainD)
            dispatch_semaphore_signal(sem);
    }

    currentSwapChain = swapChainD;
    currentFrameSlot = swapChainD->currentFrameSlot;
    if (swapChainD->ds)
        swapChainD->ds->lastActiveFrameSlot = currentFrameSlot;

    if (@available(macOS 10.13, iOS 11.0, *))
        [d->captureScope beginScope];

    // Do not let the command buffer mess with the refcount of objects. We do
    // have a proper render loop and will manage lifetimes similarly to other
    // backends (Vulkan).
    swapChainD->cbWrapper.d->cb = [d->cmdQueue commandBufferWithUnretainedReferences];

    QMetalRenderTargetData::ColorAtt colorAtt;
    if (swapChainD->samples > 1) {
        colorAtt.tex = swapChainD->d->msaaTex[currentFrameSlot];
        colorAtt.needsDrawableForResolveTex = true;
    } else {
        colorAtt.needsDrawableForTex = true;
    }

    swapChainD->rtWrapper.d->fb.colorAtt[0] = colorAtt;
    swapChainD->rtWrapper.d->fb.dsTex = swapChainD->ds ? swapChainD->ds->d->tex : nil;
    swapChainD->rtWrapper.d->fb.hasStencil = swapChainD->ds ? true : false;

    QRhiProfilerPrivate *rhiP = profilerPrivateOrNull();
    QRHI_PROF_F(beginSwapChainFrame(swapChain));

    executeDeferredReleases();
    swapChainD->cbWrapper.resetState();
    finishActiveReadbacks();

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiMetal::endFrame(QRhiSwapChain *swapChain, QRhi::EndFrameFlags flags)
{
    Q_ASSERT(inFrame);
    inFrame = false;

    QMetalSwapChain *swapChainD = QRHI_RES(QMetalSwapChain, swapChain);
    Q_ASSERT(currentSwapChain == swapChainD);

    const bool needsPresent = !flags.testFlag(QRhi::SkipPresent);
    if (needsPresent)
        [swapChainD->cbWrapper.d->cb presentDrawable: swapChainD->d->curDrawable];

    __block int thisFrameSlot = currentFrameSlot;
    [swapChainD->cbWrapper.d->cb addCompletedHandler: ^(id<MTLCommandBuffer>) {
        dispatch_semaphore_signal(swapChainD->d->sem[thisFrameSlot]);
    }];

    [swapChainD->cbWrapper.d->cb commit];

    QRhiProfilerPrivate *rhiP = profilerPrivateOrNull();
    QRHI_PROF_F(endSwapChainFrame(swapChain, swapChainD->frameCount + 1));

    if (@available(macOS 10.13, iOS 11.0, *))
        [d->captureScope endScope];

    if (needsPresent)
        swapChainD->currentFrameSlot = (swapChainD->currentFrameSlot + 1) % QMTL_FRAMES_IN_FLIGHT;

    swapChainD->frameCount += 1;
    currentSwapChain = nullptr;
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiMetal::beginOffscreenFrame(QRhiCommandBuffer **cb)
{
    Q_ASSERT(!inFrame);
    inFrame = true;

    currentFrameSlot = (currentFrameSlot + 1) % QMTL_FRAMES_IN_FLIGHT;
    if (swapchains.count() > 1) {
        for (QMetalSwapChain *sc : qAsConst(swapchains)) {
            dispatch_semaphore_t sem = sc->d->sem[currentFrameSlot];
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
            dispatch_semaphore_signal(sem);
        }
    }

    d->ofr.active = true;
    *cb = &d->ofr.cbWrapper;
    d->ofr.cbWrapper.d->cb = [d->cmdQueue commandBufferWithUnretainedReferences];

    executeDeferredReleases();
    d->ofr.cbWrapper.resetState();
    finishActiveReadbacks();

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiMetal::endOffscreenFrame()
{
    Q_ASSERT(d->ofr.active);
    d->ofr.active = false;
    Q_ASSERT(inFrame);
    inFrame = false;

    [d->ofr.cbWrapper.d->cb commit];

    // offscreen frames wait for completion, unlike swapchain ones
    [d->ofr.cbWrapper.d->cb waitUntilCompleted];

    finishActiveReadbacks(true);

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiMetal::finish()
{
    Q_ASSERT(!inPass);

    QMetalSwapChain *swapChainD = nullptr;
    if (inFrame) {
        id<MTLCommandBuffer> cb;
        if (d->ofr.active) {
            Q_ASSERT(!currentSwapChain);
            cb = d->ofr.cbWrapper.d->cb;
            [cb commit];
            [cb waitUntilCompleted];
        } else {
            Q_ASSERT(currentSwapChain);
            swapChainD = currentSwapChain;
            cb = swapChainD->cbWrapper.d->cb;
            [cb commit];
        }
    }

    for (QMetalSwapChain *sc : qAsConst(swapchains)) {
        for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
            dispatch_semaphore_t sem = sc->d->sem[i];
            // wait+signal is the general pattern to ensure the commands for a
            // given frame slot have completed (if sem is 1, we go 0 then 1; if
            // sem is 0 we go -1, block, completion increments to 0, then us to 1)
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
            dispatch_semaphore_signal(sem);
        }
    }

    if (inFrame) {
        if (d->ofr.active)
            d->ofr.cbWrapper.d->cb = [d->cmdQueue commandBufferWithUnretainedReferences];
        else
            swapChainD->cbWrapper.d->cb = [d->cmdQueue commandBufferWithUnretainedReferences];
    }

    executeDeferredReleases(true);

    finishActiveReadbacks(true);

    return QRhi::FrameOpSuccess;
}

MTLRenderPassDescriptor *QRhiMetalData::createDefaultRenderPass(bool hasDepthStencil,
                                                                const QRhiColorClearValue &colorClearValue,
                                                                const QRhiDepthStencilClearValue &depthStencilClearValue)
{
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];

    rp.colorAttachments[0].loadAction = MTLLoadActionClear;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    const QVector4D rgba = colorClearValue.rgba();
    MTLClearColor c = MTLClearColorMake(rgba.x(), rgba.y(), rgba.z(), rgba.w());
    rp.colorAttachments[0].clearColor = c;

    if (hasDepthStencil) {
        rp.depthAttachment.loadAction = MTLLoadActionClear;
        rp.depthAttachment.storeAction = MTLStoreActionDontCare;
        rp.stencilAttachment.loadAction = MTLLoadActionClear;
        rp.stencilAttachment.storeAction = MTLStoreActionDontCare;
        rp.depthAttachment.clearDepth = depthStencilClearValue.depthClearValue();
        rp.stencilAttachment.clearStencil = depthStencilClearValue.stencilClearValue();
    }

    return rp;
}

void QRhiMetal::enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    QMetalCommandBuffer *cbD = QRHI_RES(QMetalCommandBuffer, cb);
    QRhiResourceUpdateBatchPrivate *ud = QRhiResourceUpdateBatchPrivate::get(resourceUpdates);
    QRhiProfilerPrivate *rhiP = profilerPrivateOrNull();

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
    auto ensureBlit = [&blitEnc, cbD, this] {
        if (!blitEnc) {
            blitEnc = [cbD->d->cb blitCommandEncoder];
            if (debugMarkers)
                [blitEnc pushDebugGroup: @"Texture upload/copy"];
        }
    };

    for (const QRhiResourceUpdateBatchPrivate::TextureUpload &u : ud->textureUploads) {
        const QVector<QRhiTextureLayer> layers = u.desc.layers();
        if (layers.isEmpty() || layers[0].mipImages().isEmpty())
            continue;

        QMetalTexture *utexD = QRHI_RES(QMetalTexture, u.tex);
        qsizetype stagingSize = 0;
        const int texbufAlign = 256; // probably not needed

        for (int layer = 0, layerCount = layers.count(); layer != layerCount; ++layer) {
            const QRhiTextureLayer &layerDesc(layers[layer]);
            const QVector<QRhiTextureMipLevel> mipImages = layerDesc.mipImages();
            Q_ASSERT(mipImages.count() == 1 || utexD->m_flags.testFlag(QRhiTexture::MipMapped));
            for (int level = 0, levelCount = mipImages.count(); level != levelCount; ++level) {
                const QRhiTextureMipLevel &mipDesc(mipImages[level]);
                const qsizetype imageSizeBytes = mipDesc.image().isNull() ?
                            mipDesc.compressedData().size() : mipDesc.image().sizeInBytes();
                if (imageSizeBytes > 0)
                    stagingSize += aligned(imageSizeBytes, texbufAlign);
            }
        }

        ensureBlit();
        if (!utexD->d->stagingBuf[currentFrameSlot]) {
            utexD->d->stagingBuf[currentFrameSlot] = [d->dev newBufferWithLength: stagingSize options: MTLResourceStorageModeShared];
            QRHI_PROF_F(newTextureStagingArea(utexD, currentFrameSlot, stagingSize));
        }

        void *mp = [utexD->d->stagingBuf[currentFrameSlot] contents];
        qsizetype curOfs = 0;
        for (int layer = 0, layerCount = layers.count(); layer != layerCount; ++layer) {
            const QRhiTextureLayer &layerDesc(layers[layer]);
            const QVector<QRhiTextureMipLevel> mipImages = layerDesc.mipImages();
            for (int level = 0, levelCount = mipImages.count(); level != levelCount; ++level) {
                const QRhiTextureMipLevel &mipDesc(mipImages[level]);
                const QPoint dp = mipDesc.destinationTopLeft();
                const QByteArray compressedData = mipDesc.compressedData();
                QImage img = mipDesc.image();

                if (!img.isNull()) {
                    const qsizetype fullImageSizeBytes = img.sizeInBytes();
                    int w = img.width();
                    int h = img.height();
                    int bpl = img.bytesPerLine();
                    int srcOffset = 0;

                    if (!mipDesc.sourceSize().isEmpty() || !mipDesc.sourceTopLeft().isNull()) {
                        const int sx = mipDesc.sourceTopLeft().x();
                        const int sy = mipDesc.sourceTopLeft().y();
                        if (!mipDesc.sourceSize().isEmpty()) {
                            w = mipDesc.sourceSize().width();
                            h = mipDesc.sourceSize().height();
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

                    [blitEnc copyFromBuffer: utexD->d->stagingBuf[currentFrameSlot]
                                             sourceOffset: curOfs + srcOffset
                                             sourceBytesPerRow: bpl
                                             sourceBytesPerImage: 0
                                             sourceSize: MTLSizeMake(w, h, 1)
                      toTexture: utexD->d->tex
                      destinationSlice: layer
                      destinationLevel: level
                      destinationOrigin: MTLOriginMake(dp.x(), dp.y(), 0)
                      options: MTLBlitOptionNone];

                    curOfs += aligned(fullImageSizeBytes, texbufAlign);
                } else if (!compressedData.isEmpty() && isCompressedFormat(utexD->m_format)) {
                    const QSize subresSize = q->sizeForMipLevel(level, utexD->m_pixelSize);
                    const int subresw = subresSize.width();
                    const int subresh = subresSize.height();
                    int w, h;
                    if (mipDesc.sourceSize().isEmpty()) {
                        w = subresw;
                        h = subresh;
                    } else {
                        w = mipDesc.sourceSize().width();
                        h = mipDesc.sourceSize().height();
                    }

                    quint32 bpl = 0;
                    QSize blockDim;
                    compressedFormatInfo(utexD->m_format, QSize(w, h), &bpl, nullptr, &blockDim);

                    const int dx = aligned(dp.x(), blockDim.width());
                    const int dy = aligned(dp.y(), blockDim.height());
                    if (dx + w != subresw)
                        w = aligned(w, blockDim.width());
                    if (dy + h != subresh)
                        h = aligned(h, blockDim.height());

                    memcpy(reinterpret_cast<char *>(mp) + curOfs, compressedData.constData(), compressedData.size());

                    [blitEnc copyFromBuffer: utexD->d->stagingBuf[currentFrameSlot]
                                             sourceOffset: curOfs
                                             sourceBytesPerRow: bpl
                                             sourceBytesPerImage: 0
                                             sourceSize: MTLSizeMake(w, h, 1)
                      toTexture: utexD->d->tex
                      destinationSlice: layer
                      destinationLevel: level
                      destinationOrigin: MTLOriginMake(dx, dy, 0)
                      options: MTLBlitOptionNone];

                    curOfs += aligned(compressedData.size(), texbufAlign);
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
            QRHI_PROF_F(releaseTextureStagingArea(utexD, currentFrameSlot));
        }
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureCopy &u : ud->textureCopies) {
        Q_ASSERT(u.src && u.dst);
        QMetalTexture *srcD = QRHI_RES(QMetalTexture, u.src);
        QMetalTexture *dstD = QRHI_RES(QMetalTexture, u.dst);
        const QPoint dp = u.desc.destinationTopLeft();
        const QSize size = u.desc.pixelSize().isEmpty() ? srcD->m_pixelSize : u.desc.pixelSize();
        const QPoint sp = u.desc.sourceTopLeft();

        ensureBlit();
        [blitEnc copyFromTexture: srcD->d->tex
                                  sourceSlice: u.desc.sourceLayer()
                                  sourceLevel: u.desc.sourceLevel()
                                  sourceOrigin: MTLOriginMake(sp.x(), sp.y(), 0)
                                  sourceSize: MTLSizeMake(size.width(), size.height(), 1)
                                  toTexture: dstD->d->tex
                                  destinationSlice: u.desc.destinationLayer()
                                  destinationLevel: u.desc.destinationLevel()
                                  destinationOrigin: MTLOriginMake(dp.x(), dp.y(), 0)];
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureRead &u : ud->textureReadbacks) {
        QRhiMetalData::ActiveReadback aRb;
        aRb.activeFrameSlot = currentFrameSlot;
        aRb.desc = u.rb;
        aRb.result = u.result;

        QMetalTexture *texD = QRHI_RES(QMetalTexture, u.rb.texture());
        QMetalSwapChain *swapChainD = nullptr;
        id<MTLTexture> src;
        QSize srcSize;
        if (texD) {
            if (texD->samples > 1) {
                qWarning("Multisample texture cannot be read back");
                continue;
            }
            aRb.pixelSize = u.rb.level() > 0 ? q->sizeForMipLevel(u.rb.level(), texD->m_pixelSize)
                                             : texD->m_pixelSize;
            aRb.format = texD->m_format;
            src = texD->d->tex;
            srcSize = texD->m_pixelSize;
        } else {
            Q_ASSERT(currentSwapChain);
            swapChainD = QRHI_RES(QMetalSwapChain, currentSwapChain);
            aRb.pixelSize = swapChainD->pixelSize;
            aRb.format = swapChainD->d->rhiColorFormat;
            // Multisample swapchains need nothing special since resolving
            // happens when ending a renderpass.
            const QMetalRenderTargetData::ColorAtt &colorAtt(swapChainD->rtWrapper.d->fb.colorAtt[0]);
            src = colorAtt.resolveTex ? colorAtt.resolveTex : colorAtt.tex;
            srcSize = swapChainD->rtWrapper.d->pixelSize;
        }

        quint32 bpl = 0;
        textureFormatInfo(aRb.format, aRb.pixelSize, &bpl, &aRb.bufSize);
        aRb.buf = [d->dev newBufferWithLength: aRb.bufSize options: MTLResourceStorageModeShared];

        QRHI_PROF_F(newReadbackBuffer(quint64(quintptr(aRb.buf)),
                                      texD ? static_cast<QRhiResource *>(texD) : static_cast<QRhiResource *>(swapChainD),
                                      aRb.bufSize));

        ensureBlit();
        [blitEnc copyFromTexture: src
                                  sourceSlice: u.rb.layer()
                                  sourceLevel: u.rb.level()
                                  sourceOrigin: MTLOriginMake(0, 0, 0)
                                  sourceSize: MTLSizeMake(srcSize.width(), srcSize.height(), 1)
                                  toBuffer: aRb.buf
                                  destinationOffset: 0
                                  destinationBytesPerRow: bpl
                                  destinationBytesPerImage: 0
                                  options: MTLBlitOptionNone];

        d->activeReadbacks.append(aRb);
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureMipGen &u : ud->textureMipGens) {
        ensureBlit();
        [blitEnc generateMipmapsForTexture: QRHI_RES(QMetalTexture, u.tex)->d->tex];
    }

    if (blitEnc) {
        if (debugMarkers)
            [blitEnc popDebugGroup];
        [blitEnc endEncoding];
    }

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
        cbD->d->currentPassRpDesc = d->createDefaultRenderPass(rtD->dsAttCount, colorClearValue, depthStencilClearValue);
        if (rtD->colorAttCount) {
            QMetalRenderTargetData::ColorAtt &color0(rtD->fb.colorAtt[0]);
            if (color0.needsDrawableForTex || color0.needsDrawableForResolveTex) {
                Q_ASSERT(currentSwapChain);
                QMetalSwapChain *swapChainD = QRHI_RES(QMetalSwapChain, currentSwapChain);
                swapChainD->d->curDrawable = [swapChainD->d->layer nextDrawable];
                if (!swapChainD->d->curDrawable) {
                    qWarning("No drawable");
                    return;
                }
                id<MTLTexture> scTex = swapChainD->d->curDrawable.texture;
                if (color0.needsDrawableForTex) {
                    color0.tex = scTex;
                    color0.needsDrawableForTex = false;
                } else {
                    color0.resolveTex = scTex;
                    color0.needsDrawableForResolveTex = false;
                }
            }
        }
        break;
    case QRhiRenderTarget::RtTexture:
    {
        QMetalTextureRenderTarget *rtTex = QRHI_RES(QMetalTextureRenderTarget, rt);
        rtD = rtTex->d;
        cbD->d->currentPassRpDesc = d->createDefaultRenderPass(rtD->dsAttCount, colorClearValue, depthStencilClearValue);
        if (rtTex->m_flags.testFlag(QRhiTextureRenderTarget::PreserveColorContents)) {
            for (int i = 0; i < rtD->colorAttCount; ++i)
                cbD->d->currentPassRpDesc.colorAttachments[i].loadAction = MTLLoadActionLoad;
        }
        if (rtD->dsAttCount && rtTex->m_flags.testFlag(QRhiTextureRenderTarget::PreserveDepthStencilContents)) {
            cbD->d->currentPassRpDesc.depthAttachment.loadAction = MTLLoadActionLoad;
            cbD->d->currentPassRpDesc.stencilAttachment.loadAction = MTLLoadActionLoad;
        }
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    for (int i = 0; i < rtD->colorAttCount; ++i) {
        cbD->d->currentPassRpDesc.colorAttachments[i].texture = rtD->fb.colorAtt[i].tex;
        cbD->d->currentPassRpDesc.colorAttachments[i].slice = rtD->fb.colorAtt[i].layer;
        cbD->d->currentPassRpDesc.colorAttachments[i].level = rtD->fb.colorAtt[i].level;
        if (rtD->fb.colorAtt[i].resolveTex) {
            cbD->d->currentPassRpDesc.colorAttachments[i].storeAction = MTLStoreActionMultisampleResolve;
            cbD->d->currentPassRpDesc.colorAttachments[i].resolveTexture = rtD->fb.colorAtt[i].resolveTex;
            cbD->d->currentPassRpDesc.colorAttachments[i].resolveSlice = rtD->fb.colorAtt[i].resolveLayer;
            cbD->d->currentPassRpDesc.colorAttachments[i].resolveLevel = rtD->fb.colorAtt[i].resolveLevel;
        }
    }

    if (rtD->dsAttCount) {
        Q_ASSERT(rtD->fb.dsTex);
        cbD->d->currentPassRpDesc.depthAttachment.texture = rtD->fb.dsTex;
        cbD->d->currentPassRpDesc.stencilAttachment.texture = rtD->fb.hasStencil ? rtD->fb.dsTex : nil;
    }

    cbD->d->currentPassEncoder = [cbD->d->cb renderCommandEncoderWithDescriptor: cbD->d->currentPassRpDesc];

    cbD->resetPerPassState();

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

static void qrhimtl_releaseBuffer(const QRhiMetalData::DeferredReleaseEntry &e)
{
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
        [e.buffer.buffers[i] release];
}

static void qrhimtl_releaseRenderBuffer(const QRhiMetalData::DeferredReleaseEntry &e)
{
    [e.renderbuffer.texture release];
}

static void qrhimtl_releaseTexture(const QRhiMetalData::DeferredReleaseEntry &e)
{
    [e.texture.texture release];
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i)
        [e.texture.stagingBuffers[i] release];
}

static void qrhimtl_releaseSampler(const QRhiMetalData::DeferredReleaseEntry &e)
{
    [e.sampler.samplerState release];
}

void QRhiMetalData::executeDeferredReleasesOnRshNow(QVector<QRhiMetalData::DeferredReleaseEntry> *rshRelQueue)
{
    for (int i = rshRelQueue->count() - 1; i >= 0; --i) {
        const QRhiMetalData::DeferredReleaseEntry &e((*rshRelQueue)[i]);
        // only need to handle resources that report isShareable() == true
        switch (e.type) {
        case QRhiMetalData::DeferredReleaseEntry::Buffer:
            qrhimtl_releaseBuffer(e);
            break;
        case QRhiMetalData::DeferredReleaseEntry::RenderBuffer:
            qrhimtl_releaseRenderBuffer(e);
            break;
        case QRhiMetalData::DeferredReleaseEntry::Texture:
            qrhimtl_releaseTexture(e);
            break;
        case QRhiMetalData::DeferredReleaseEntry::Sampler:
            qrhimtl_releaseSampler(e);
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
        rshRelQueue->removeAt(i);
    }
}

void QRhiMetal::executeDeferredReleases(bool forced)
{
    for (int i = d->releaseQueue.count() - 1; i >= 0; --i) {
        const QRhiMetalData::DeferredReleaseEntry &e(d->releaseQueue[i]);
        if (forced || currentFrameSlot == e.lastActiveFrameSlot || e.lastActiveFrameSlot < 0) {
            switch (e.type) {
            case QRhiMetalData::DeferredReleaseEntry::Buffer:
                qrhimtl_releaseBuffer(e);
                break;
            case QRhiMetalData::DeferredReleaseEntry::RenderBuffer:
                qrhimtl_releaseRenderBuffer(e);
                break;
            case QRhiMetalData::DeferredReleaseEntry::Texture:
                qrhimtl_releaseTexture(e);
                break;
            case QRhiMetalData::DeferredReleaseEntry::Sampler:
                qrhimtl_releaseSampler(e);
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

void QRhiMetal::finishActiveReadbacks(bool forced)
{
    QVarLengthArray<std::function<void()>, 4> completedCallbacks;
    QRhiProfilerPrivate *rhiP = profilerPrivateOrNull();

    for (int i = d->activeReadbacks.count() - 1; i >= 0; --i) {
        const QRhiMetalData::ActiveReadback &aRb(d->activeReadbacks[i]);
        if (forced || currentFrameSlot == aRb.activeFrameSlot || aRb.activeFrameSlot < 0) {
            aRb.result->format = aRb.format;
            aRb.result->pixelSize = aRb.pixelSize;
            aRb.result->data.resize(aRb.bufSize);
            void *p = [aRb.buf contents];
            memcpy(aRb.result->data.data(), p, aRb.bufSize);
            [aRb.buf release];

            QRHI_PROF_F(releaseReadbackBuffer(quint64(quintptr(aRb.buf))));

            if (aRb.result->completed)
                completedCallbacks.append(aRb.result->completed);

            d->activeReadbacks.removeAt(i);
        }
    }

    for (auto f : completedCallbacks)
        f();
}

static void addToRshReleaseQueue(QRhiResourceSharingHostPrivate *rsh, const QRhiMetalData::DeferredReleaseEntry &e)
{
    QVector<QRhiMetalData::DeferredReleaseEntry> *rshRelQueue =
            static_cast<QVector<QRhiMetalData::DeferredReleaseEntry> *>(rsh->d_metal.releaseQueue);
    if (!rshRelQueue) {
        rshRelQueue = new QVector<QRhiMetalData::DeferredReleaseEntry>;
        rsh->d_metal.releaseQueue = rshRelQueue;
    }
    rshRelQueue->append(e);
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

bool QMetalBuffer::isShareable() const
{
    return true;
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

    if (!orphanedWithRsh) {
        QRHI_RES_RHI(QRhiMetal);
        rhiD->d->releaseQueue.append(e);
        QRHI_PROF;
        QRHI_PROF_F(releaseBuffer(this));
        rhiD->unregisterResource(this);
    } else {
        // associated rhi is already gone, queue the deferred release to the rsh instead
        addToRshReleaseQueue(orphanedWithRsh, e);
    }
}

bool QMetalBuffer::build()
{
    if (!QRhiImplementation::orphanCheck(this))
        return false;

    if (d->buf[0])
        release();

    const int nonZeroSize = m_size <= 0 ? 256 : m_size;
    const int roundedSize = m_usage.testFlag(QRhiBuffer::UniformBuffer) ? aligned(nonZeroSize, 256) : nonZeroSize;

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
            if (!objectName.isEmpty()) {
                if (m_type == Immutable) {
                    d->buf[i].label = [NSString stringWithUTF8String: objectName.constData()];
                } else {
                    const QByteArray name = objectName + '/' + QByteArray::number(i);
                    d->buf[i].label = [NSString stringWithUTF8String: name.constData()];
                }
            }
        }
    }

    QRHI_PROF;
    QRHI_PROF_F(newBuffer(this, roundedSize, m_type == Immutable ? 1 : QMTL_FRAMES_IN_FLIGHT, 0));

    lastActiveFrameSlot = -1;
    generation += 1;
    rhiD->registerResource(this);
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

bool QMetalRenderBuffer::isShareable() const
{
    return true;
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

    if (!orphanedWithRsh) {
        QRHI_RES_RHI(QRhiMetal);
        rhiD->d->releaseQueue.append(e);
        QRHI_PROF;
        QRHI_PROF_F(releaseRenderBuffer(this));
        rhiD->unregisterResource(this);
    } else {
        addToRshReleaseQueue(orphanedWithRsh, e);
    }
}

bool QMetalRenderBuffer::build()
{
    if (!QRhiImplementation::orphanCheck(this))
        return false;

    if (d->tex)
        release();

    if (m_pixelSize.isEmpty())
        return false;

    QRHI_RES_RHI(QRhiMetal);
    samples = rhiD->effectiveSampleCount(m_sampleCount);

    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = samples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
    desc.width = m_pixelSize.width();
    desc.height = m_pixelSize.height();
    if (samples > 1)
        desc.sampleCount = samples;
    desc.resourceOptions = MTLResourceStorageModePrivate;
    desc.usage = MTLTextureUsageRenderTarget;

    bool transientBacking = false;
    switch (m_type) {
    case DepthStencil:
#ifdef Q_OS_MACOS
        desc.storageMode = MTLStorageModePrivate;
#else
        desc.storageMode = MTLResourceStorageModeMemoryless;
        transientBacking = true;
#endif
        d->format = rhiD->d->dev.depth24Stencil8PixelFormatSupported
                ? MTLPixelFormatDepth24Unorm_Stencil8 : MTLPixelFormatDepth32Float_Stencil8;
        desc.pixelFormat = d->format;
        break;
    case Color:
        desc.storageMode = MTLStorageModePrivate;
        d->format = MTLPixelFormatRGBA8Unorm;
        desc.pixelFormat = d->format;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    d->tex = [rhiD->d->dev newTextureWithDescriptor: desc];
    [desc release];

    if (!objectName.isEmpty())
        d->tex.label = [NSString stringWithUTF8String: objectName.constData()];

    QRHI_PROF;
    QRHI_PROF_F(newRenderBuffer(this, transientBacking, false, samples));

    lastActiveFrameSlot = -1;
    generation += 1;
    rhiD->registerResource(this);
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

bool QMetalTexture::isShareable() const
{
    return true;
}

void QMetalTexture::release()
{
    if (!d->tex)
        return;

    QRhiMetalData::DeferredReleaseEntry e;
    e.type = QRhiMetalData::DeferredReleaseEntry::Texture;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.texture.texture = d->owns ? d->tex : nil;
    d->tex = nil;
    nativeHandlesStruct.texture = nullptr;

    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        e.texture.stagingBuffers[i] = d->stagingBuf[i];
        d->stagingBuf[i] = nil;
    }

    if (!orphanedWithRsh) {
        QRHI_RES_RHI(QRhiMetal);
        rhiD->d->releaseQueue.append(e);
        QRHI_PROF;
        QRHI_PROF_F(releaseTexture(this));
        rhiD->unregisterResource(this);
    } else {
        addToRshReleaseQueue(orphanedWithRsh, e);
    }
}

static inline MTLPixelFormat toMetalTextureFormat(QRhiTexture::Format format, QRhiTexture::Flags flags)
{
    const bool srgb = flags.testFlag(QRhiTexture::sRGB);
    switch (format) {
    case QRhiTexture::RGBA8:
        return srgb ? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm;
    case QRhiTexture::BGRA8:
        return srgb ? MTLPixelFormatBGRA8Unorm_sRGB : MTLPixelFormatBGRA8Unorm;
    case QRhiTexture::R8:
#ifdef Q_OS_MACOS
        return MTLPixelFormatR8Unorm;
#else
        return srgb ? MTLPixelFormatR8Unorm_sRGB : MTLPixelFormatR8Unorm;
#endif
    case QRhiTexture::R16:
        return MTLPixelFormatR16Unorm;

    case QRhiTexture::D16:
        return MTLPixelFormatDepth16Unorm;
    case QRhiTexture::D32:
        return MTLPixelFormatDepth32Float;

#ifdef Q_OS_MACOS
    case QRhiTexture::BC1:
        return srgb ? MTLPixelFormatBC1_RGBA_sRGB : MTLPixelFormatBC1_RGBA;
    case QRhiTexture::BC2:
        return srgb ? MTLPixelFormatBC2_RGBA_sRGB : MTLPixelFormatBC2_RGBA;
    case QRhiTexture::BC3:
        return srgb ? MTLPixelFormatBC3_RGBA_sRGB : MTLPixelFormatBC3_RGBA;
    case QRhiTexture::BC4:
        return MTLPixelFormatBC4_RUnorm;
    case QRhiTexture::BC5:
        qWarning("QRhiMetal does not support BC5");
        return MTLPixelFormatRGBA8Unorm;
    case QRhiTexture::BC6H:
        return MTLPixelFormatBC6H_RGBUfloat;
    case QRhiTexture::BC7:
        return srgb ? MTLPixelFormatBC7_RGBAUnorm_sRGB : MTLPixelFormatBC7_RGBAUnorm;
#else
    case QRhiTexture::BC1:
    case QRhiTexture::BC2:
    case QRhiTexture::BC3:
    case QRhiTexture::BC4:
    case QRhiTexture::BC5:
    case QRhiTexture::BC6H:
    case QRhiTexture::BC7:
        qWarning("QRhiMetal: BCx compression not supported on this platform");
        return MTLPixelFormatRGBA8Unorm;
#endif

#ifndef Q_OS_MACOS
    case QRhiTexture::ETC2_RGB8:
        return srgb ? MTLPixelFormatETC2_RGB8_sRGB : MTLPixelFormatETC2_RGB8;
    case QRhiTexture::ETC2_RGB8A1:
        return srgb ? MTLPixelFormatETC2_RGB8A1_sRGB : MTLPixelFormatETC2_RGB8A1;
    case QRhiTexture::ETC2_RGBA8:
        return srgb ? MTLPixelFormatEAC_RGBA8_sRGB : MTLPixelFormatEAC_RGBA8;

    case QRhiTexture::ASTC_4x4:
        return srgb ? MTLPixelFormatASTC_4x4_sRGB : MTLPixelFormatASTC_4x4_LDR;
    case QRhiTexture::ASTC_5x4:
        return srgb ? MTLPixelFormatASTC_5x4_sRGB : MTLPixelFormatASTC_5x4_LDR;
    case QRhiTexture::ASTC_5x5:
        return srgb ? MTLPixelFormatASTC_5x5_sRGB : MTLPixelFormatASTC_5x5_LDR;
    case QRhiTexture::ASTC_6x5:
        return srgb ? MTLPixelFormatASTC_6x5_sRGB : MTLPixelFormatASTC_6x5_LDR;
    case QRhiTexture::ASTC_6x6:
        return srgb ? MTLPixelFormatASTC_6x6_sRGB : MTLPixelFormatASTC_6x6_LDR;
    case QRhiTexture::ASTC_8x5:
        return srgb ? MTLPixelFormatASTC_8x5_sRGB : MTLPixelFormatASTC_8x5_LDR;
    case QRhiTexture::ASTC_8x6:
        return srgb ? MTLPixelFormatASTC_8x6_sRGB : MTLPixelFormatASTC_8x6_LDR;
    case QRhiTexture::ASTC_8x8:
        return srgb ? MTLPixelFormatASTC_8x8_sRGB : MTLPixelFormatASTC_8x8_LDR;
    case QRhiTexture::ASTC_10x5:
        return srgb ? MTLPixelFormatASTC_10x5_sRGB : MTLPixelFormatASTC_10x5_LDR;
    case QRhiTexture::ASTC_10x6:
        return srgb ? MTLPixelFormatASTC_10x6_sRGB : MTLPixelFormatASTC_10x6_LDR;
    case QRhiTexture::ASTC_10x8:
        return srgb ? MTLPixelFormatASTC_10x8_sRGB : MTLPixelFormatASTC_10x8_LDR;
    case QRhiTexture::ASTC_10x10:
        return srgb ? MTLPixelFormatASTC_10x10_sRGB : MTLPixelFormatASTC_10x10_LDR;
    case QRhiTexture::ASTC_12x10:
        return srgb ? MTLPixelFormatASTC_12x10_sRGB : MTLPixelFormatASTC_12x10_LDR;
    case QRhiTexture::ASTC_12x12:
        return srgb ? MTLPixelFormatASTC_12x12_sRGB : MTLPixelFormatASTC_12x12_LDR;
#else
    case QRhiTexture::ETC2_RGB8:
    case QRhiTexture::ETC2_RGB8A1:
    case QRhiTexture::ETC2_RGBA8:
        qWarning("QRhiMetal: ETC2 compression not supported on this platform");
        return MTLPixelFormatRGBA8Unorm;

    case QRhiTexture::ASTC_4x4:
    case QRhiTexture::ASTC_5x4:
    case QRhiTexture::ASTC_5x5:
    case QRhiTexture::ASTC_6x5:
    case QRhiTexture::ASTC_6x6:
    case QRhiTexture::ASTC_8x5:
    case QRhiTexture::ASTC_8x6:
    case QRhiTexture::ASTC_8x8:
    case QRhiTexture::ASTC_10x5:
    case QRhiTexture::ASTC_10x6:
    case QRhiTexture::ASTC_10x8:
    case QRhiTexture::ASTC_10x10:
    case QRhiTexture::ASTC_12x10:
    case QRhiTexture::ASTC_12x12:
        qWarning("QRhiMetal: ASTC compression not supported on this platform");
        return MTLPixelFormatRGBA8Unorm;
#endif

    default:
        Q_UNREACHABLE();
        return MTLPixelFormatRGBA8Unorm;
    }
}

bool QMetalTexture::prepareBuild(QSize *adjustedSize)
{
    if (!QRhiImplementation::orphanCheck(this))
        return false;

    if (d->tex)
        release();

    const QSize size = m_pixelSize.isEmpty() ? QSize(1, 1) : m_pixelSize;
    const bool isCube = m_flags.testFlag(CubeMap);
    const bool hasMipMaps = m_flags.testFlag(MipMapped);

    QRHI_RES_RHI(QRhiMetal);
    d->format = toMetalTextureFormat(m_format, m_flags);
    mipLevelCount = hasMipMaps ? rhiD->q->mipLevelsForSize(size) : 1;
    samples = rhiD->effectiveSampleCount(m_sampleCount);
    if (samples > 1) {
        if (isCube) {
            qWarning("Cubemap texture cannot be multisample");
            return false;
        }
        if (hasMipMaps) {
            qWarning("Multisample texture cannot have mipmaps");
            return false;
        }
    }

    if (adjustedSize)
        *adjustedSize = size;

    return true;
}

bool QMetalTexture::build()
{
    QSize size;
    if (!prepareBuild(&size))
        return false;

    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];

    const bool isCube = m_flags.testFlag(CubeMap);
    if (isCube)
        desc.textureType = MTLTextureTypeCube;
    else
        desc.textureType = samples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
    desc.pixelFormat = d->format;
    desc.width = size.width();
    desc.height = size.height();
    desc.mipmapLevelCount = mipLevelCount;
    if (samples > 1)
        desc.sampleCount = samples;
    desc.resourceOptions = MTLResourceStorageModePrivate;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageShaderRead;
    if (m_flags.testFlag(RenderTarget))
        desc.usage |= MTLTextureUsageRenderTarget;

    QRHI_RES_RHI(QRhiMetal);
    d->tex = [rhiD->d->dev newTextureWithDescriptor: desc];
    [desc release];

    if (!objectName.isEmpty())
        d->tex.label = [NSString stringWithUTF8String: objectName.constData()];

    d->owns = true;
    nativeHandlesStruct.texture = d->tex;

    QRHI_PROF;
    QRHI_PROF_F(newTexture(this, true, mipLevelCount, isCube ? 6 : 1, samples));

    lastActiveFrameSlot = -1;
    generation += 1;
    rhiD->registerResource(this);
    return true;
}

bool QMetalTexture::buildFrom(const QRhiNativeHandles *src)
{
    const QRhiMetalTextureNativeHandles *h = static_cast<const QRhiMetalTextureNativeHandles *>(src);
    if (!h || !h->texture)
        return false;

    if (!prepareBuild())
        return false;

    d->tex = (id<MTLTexture>) h->texture;

    d->owns = false;
    nativeHandlesStruct.texture = d->tex;

    QRHI_PROF;
    QRHI_PROF_F(newTexture(this, false, mipLevelCount, m_flags.testFlag(CubeMap) ? 6 : 1, samples));

    lastActiveFrameSlot = -1;
    generation += 1;
    QRHI_RES_RHI(QRhiMetal);
    rhiD->registerResource(this);
    return true;
}

const QRhiNativeHandles *QMetalTexture::nativeHandles()
{
    return &nativeHandlesStruct;
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

bool QMetalSampler::isShareable() const
{
    return true;
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

    if (!orphanedWithRsh) {
        QRHI_RES_RHI(QRhiMetal);
        rhiD->d->releaseQueue.append(e);
        rhiD->unregisterResource(this);
    } else {
        addToRshReleaseQueue(orphanedWithRsh, e);
    }
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
    if (!QRhiImplementation::orphanCheck(this))
        return false;

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
    rhiD->registerResource(this);
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

float QMetalReferenceRenderTarget::devicePixelRatio() const
{
    return d->dpr;
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
    const QVector<QRhiColorAttachment> colorAttachments = m_desc.colorAttachments();
    QMetalRenderPassDescriptor *rpD = new QMetalRenderPassDescriptor(rhi);
    rpD->colorAttachmentCount = colorAttachments.count();
    rpD->hasDepthStencil = m_desc.depthStencilBuffer() || m_desc.depthTexture();

    for (int i = 0, ie = colorAttachments.count(); i != ie; ++i) {
        QMetalTexture *texD = QRHI_RES(QMetalTexture, colorAttachments[i].texture());
        QMetalRenderBuffer *rbD = QRHI_RES(QMetalRenderBuffer, colorAttachments[i].renderBuffer());
        rpD->colorFormat[i] = texD ? texD->d->format : rbD->d->format;
    }

    if (m_desc.depthTexture())
        rpD->dsFormat = QRHI_RES(QMetalTexture, m_desc.depthTexture())->d->format;
    else if (m_desc.depthStencilBuffer())
        rpD->dsFormat = QRHI_RES(QMetalRenderBuffer, m_desc.depthStencilBuffer())->d->format;

    return rpD;
}

bool QMetalTextureRenderTarget::build()
{
    const QVector<QRhiColorAttachment> colorAttachments = m_desc.colorAttachments();
    Q_ASSERT(!colorAttachments.isEmpty() || m_desc.depthTexture());
    Q_ASSERT(!m_desc.depthStencilBuffer() || !m_desc.depthTexture());
    const bool hasDepthStencil = m_desc.depthStencilBuffer() || m_desc.depthTexture();

    d->colorAttCount = colorAttachments.count();
    for (int i = 0; i < d->colorAttCount; ++i) {
        QMetalTexture *texD = QRHI_RES(QMetalTexture, colorAttachments[i].texture());
        QMetalRenderBuffer *rbD = QRHI_RES(QMetalRenderBuffer, colorAttachments[i].renderBuffer());
        Q_ASSERT(texD || rbD);
        id<MTLTexture> dst;
        if (texD) {
            dst = texD->d->tex;
            if (i == 0)
                d->pixelSize = texD->pixelSize();
        } else {
            dst = rbD->d->tex;
            if (i == 0)
                d->pixelSize = rbD->pixelSize();
        }
        QMetalRenderTargetData::ColorAtt colorAtt;
        colorAtt.tex = dst;
        colorAtt.layer = colorAttachments[i].layer();
        colorAtt.level = colorAttachments[i].level();
        QMetalTexture *resTexD = QRHI_RES(QMetalTexture, colorAttachments[i].resolveTexture());
        colorAtt.resolveTex = resTexD ? resTexD->d->tex : nil;
        colorAtt.resolveLayer = colorAttachments[i].resolveLayer();
        colorAtt.resolveLevel = colorAttachments[i].resolveLevel();
        d->fb.colorAtt[i] = colorAtt;
    }
    d->dpr = 1;

    if (hasDepthStencil) {
        if (m_desc.depthTexture()) {
            d->fb.dsTex = QRHI_RES(QMetalTexture, m_desc.depthTexture())->d->tex;
            d->fb.hasStencil = false;
            if (d->colorAttCount == 0)
                d->pixelSize = m_desc.depthTexture()->pixelSize();
        } else {
            d->fb.dsTex = QRHI_RES(QMetalRenderBuffer, m_desc.depthStencilBuffer())->d->tex;
            d->fb.hasStencil = true;
            if (d->colorAttCount == 0)
                d->pixelSize = m_desc.depthStencilBuffer()->pixelSize();
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

float QMetalTextureRenderTarget::devicePixelRatio() const
{
    return d->dpr;
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
        return QRhiShaderResourceBindingPrivate::get(&a)->binding < QRhiShaderResourceBindingPrivate::get(&b)->binding;
    });
    if (!sortedBindings.isEmpty())
        maxBinding = QRhiShaderResourceBindingPrivate::get(&sortedBindings.last())->binding;
    else
        maxBinding = -1;

    boundResourceData.resize(sortedBindings.count());

    for (int i = 0, ie = sortedBindings.count(); i != ie; ++i) {
        const QRhiShaderResourceBindingPrivate *b = QRhiShaderResourceBindingPrivate::get(&sortedBindings[i]);
        QMetalShaderResourceBindings::BoundResourceData &bd(boundResourceData[i]);
        switch (b->type) {
        case QRhiShaderResourceBinding::UniformBuffer:
            bd.ubuf.generation = QRHI_RES(QMetalBuffer, b->u.ubuf.buf)->generation;
            break;
        case QRhiShaderResourceBinding::SampledTexture:
            bd.stex.texGeneration = QRHI_RES(QMetalTexture, b->u.stex.tex)->generation;
            bd.stex.samplerGeneration = QRHI_RES(QMetalSampler, b->u.stex.sampler)->generation;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

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
    QRHI_RES_RHI(QRhiMetal);

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

    rhiD->unregisterResource(this);
}

static inline MTLVertexFormat toMetalAttributeFormat(QRhiVertexInputAttribute::Format format)
{
    switch (format) {
    case QRhiVertexInputAttribute::Float4:
        return MTLVertexFormatFloat4;
    case QRhiVertexInputAttribute::Float3:
        return MTLVertexFormatFloat3;
    case QRhiVertexInputAttribute::Float2:
        return MTLVertexFormatFloat2;
    case QRhiVertexInputAttribute::Float:
        return MTLVertexFormatFloat;
    case QRhiVertexInputAttribute::UNormByte4:
        return MTLVertexFormatUChar4Normalized;
    case QRhiVertexInputAttribute::UNormByte2:
        return MTLVertexFormatUChar2Normalized;
    case QRhiVertexInputAttribute::UNormByte:
        if (@available(macOS 10.13, iOS 11.0, *))
            return MTLVertexFormatUCharNormalized;
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

id<MTLLibrary> QRhiMetalData::createMetalLib(const QBakedShader &shader, QBakedShaderKey::ShaderVariant shaderVariant,
                                             QString *error, QByteArray *entryPoint)
{
    QBakedShaderCode mtllib = shader.shader({ QBakedShaderKey::MetalLibShader, 12, shaderVariant });
    if (!mtllib.shader().isEmpty()) {
        dispatch_data_t data = dispatch_data_create(mtllib.shader().constData(),
                                                    mtllib.shader().size(),
                                                    dispatch_get_global_queue(0, 0),
                                                    DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        NSError *err = nil;
        id<MTLLibrary> lib = [dev newLibraryWithData: data error: &err];
        dispatch_release(data);
        if (!err) {
            *entryPoint = mtllib.entryPoint();
            return lib;
        } else {
            const QString msg = QString::fromNSString(err.localizedDescription);
            qWarning("Failed to load metallib from baked shader: %s", qPrintable(msg));
        }
    }

    QBakedShaderCode mslSource = shader.shader({ QBakedShaderKey::MslShader, 12, shaderVariant });
    if (mslSource.shader().isEmpty()) {
        qWarning() << "No MSL 1.2 code found in baked shader" << shader;
        return nil;
    }

    NSString *src = [NSString stringWithUTF8String: mslSource.shader().constData()];
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

    *entryPoint = mslSource.entryPoint();
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
    const QVector<QRhiVertexInputAttribute> attributes = m_vertexInputLayout.attributes();
    for (const QRhiVertexInputAttribute &attribute : attributes) {
        const int loc = attribute.location();
        inputLayout.attributes[loc].format = toMetalAttributeFormat(attribute.format());
        inputLayout.attributes[loc].offset = attribute.offset();
        inputLayout.attributes[loc].bufferIndex = firstVertexBinding + attribute.binding();
    }
    const QVector<QRhiVertexInputBinding> bindings = m_vertexInputLayout.bindings();
    for (int i = 0, ie = bindings.count(); i != ie; ++i) {
        const QRhiVertexInputBinding &binding(bindings[i]);
        const int layoutIdx = firstVertexBinding + i;
        inputLayout.layouts[layoutIdx].stepFunction =
                binding.classification() == QRhiVertexInputBinding::PerInstance
                ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
        inputLayout.layouts[layoutIdx].stepRate = binding.instanceStepRate();
        inputLayout.layouts[layoutIdx].stride = binding.stride();
    }

    MTLRenderPipelineDescriptor *rpDesc = [[MTLRenderPipelineDescriptor alloc] init];

    rpDesc.vertexDescriptor = inputLayout;

    if (@available(macOS 10.13, iOS 11.0, *)) {
        // Everything is immutable because we can guarantee that "neither the
        // CPU nor the GPU will modify a buffer's contents between the time the
        // buffer is set in a function's argument table and the time its
        // associated command buffer completes execution" (as that's the point
        // of our Vulkan-style buffer juggling in the first place).
        const int vertexBufferCount = firstVertexBinding + bindings.count(); // cbuf + vbuf
        const int fragmentBufferCount = firstVertexBinding; // cbuf
        for (int i = 0; i < vertexBufferCount; ++i)
            rpDesc.vertexBuffers[i].mutability = MTLMutabilityImmutable;
        for (int i = 0; i < fragmentBufferCount; ++i)
            rpDesc.fragmentBuffers[i].mutability = MTLMutabilityImmutable;
    }

    for (const QRhiGraphicsShaderStage &shaderStage : qAsConst(m_shaderStages)) {
        QString error;
        QByteArray entryPoint;
        id<MTLLibrary> lib = rhiD->d->createMetalLib(shaderStage.shader(), shaderStage.shaderVariant(), &error, &entryPoint);
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
        switch (shaderStage.type()) {
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
    rhiD->registerResource(this);
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
    currentResSlot = -1;
    currentIndexBuffer = nullptr;

    d->currentPassEncoder = nil;
    d->currentPassRpDesc = nil;

    resetPerPassState();
}

void QMetalCommandBuffer::resetPerPassState()
{
    d->shaderResourceBindingsValid = false;
    d->currentFirstVertexBinding = -1;
    d->currentVertexInputsBuffers.clear();
    d->currentVertexInputOffsets.clear();
}

QMetalSwapChain::QMetalSwapChain(QRhiImplementation *rhi)
    : QRhiSwapChain(rhi),
      rtWrapper(rhi),
      cbWrapper(rhi),
      d(new QMetalSwapChainData)
{
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        d->sem[i] = nullptr;
        d->msaaTex[i] = nil;
    }
}

QMetalSwapChain::~QMetalSwapChain()
{
    delete d;
}

void QMetalSwapChain::release()
{
    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        if (d->sem[i]) {
            // the semaphores cannot be released if they do not have the initial value
            dispatch_semaphore_wait(d->sem[i], DISPATCH_TIME_FOREVER);
            dispatch_semaphore_signal(d->sem[i]);

            dispatch_release(d->sem[i]);
            d->sem[i] = nullptr;
        }
    }

    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        [d->msaaTex[i] release];
        d->msaaTex[i] = nil;
    }

    d->layer = nullptr;

    QRHI_RES_RHI(QRhiMetal);
    rhiD->swapchains.remove(this);

    QRHI_PROF;
    QRHI_PROF_F(releaseSwapChain(this));

    rhiD->unregisterResource(this);
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
    // may be called before build, must not access other than m_*

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
    // pick a format that is allowed for CAMetalLayer.pixelFormat
    d->colorFormat = m_flags.testFlag(sRGB) ? MTLPixelFormatBGRA8Unorm_sRGB : MTLPixelFormatBGRA8Unorm;
    d->rhiColorFormat = QRhiTexture::BGRA8;
}

bool QMetalSwapChain::buildOrResize()
{
    Q_ASSERT(m_window);

    const bool needsRegistration = !window || window != m_window;

    if (window && window != m_window)
        release();
    // else no release(), this is intentional

    QRHI_RES_RHI(QRhiMetal);
    if (needsRegistration)
        rhiD->swapchains.insert(this);

    window = m_window;

    if (window->surfaceType() != QSurface::MetalSurface) {
        qWarning("QMetalSwapChain only supports MetalSurface windows");
        return false;
    }

    NSView *v = (NSView *) window->winId();
    d->layer = (CAMetalLayer *) [v layer];
    Q_ASSERT(d->layer);

    chooseFormats();
    if (d->colorFormat != d->layer.pixelFormat)
        d->layer.pixelFormat = d->colorFormat;

    if (m_flags.testFlag(UsedAsTransferSource))
        d->layer.framebufferOnly = NO;

#ifdef Q_OS_MAC
    if (m_flags.testFlag(NoVSync)) {
        if (@available(macOS 10.13, *))
            d->layer.displaySyncEnabled = NO;
    }
#endif

    m_currentPixelSize = surfacePixelSize();
    pixelSize = m_currentPixelSize;

    [d->layer setDevice: rhiD->d->dev];

    for (int i = 0; i < QMTL_FRAMES_IN_FLIGHT; ++i) {
        if (!d->sem[i])
            d->sem[i] = dispatch_semaphore_create(1);
    }

    currentFrameSlot = 0;
    frameCount = 0;

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
    rtWrapper.d->dpr = window->devicePixelRatio();
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

    QRHI_PROF;
    QRHI_PROF_F(resizeSwapChain(this, QMTL_FRAMES_IN_FLIGHT, samples > 1 ? QMTL_FRAMES_IN_FLIGHT : 0, samples));

    if (needsRegistration)
        rhiD->registerResource(this);

    return true;
}

QT_END_NAMESPACE
