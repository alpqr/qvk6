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

#include "qrhinull_p.h"
#ifndef QT_NO_OPENGL
#include "qrhigles2_p.h"
#endif
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

/*!
    \class QRhi
    \inmodule QtRhi

    \brief Accelerated 2D/3D graphics API abstraction.
 */

/*!
    \enum QRhi::Implementation
    Describes which graphics API-specific backend gets used by a QRhi instance.

    \value Null
    \value Vulkan
    \value OpenGLES2
    \value D3D11
    \value Metal
 */

/*!
    \enum QRhi::Flag
    Describes what special features to enable.

    \value EnableProfiling Enables gathering timing (CPU, GPU) and resource
    (QRhiBuffer, QRhiTexture, etc.) information and additional metadata. See
    QRhiProfiler. Avoid enabling in production builds as it may involve a
    performance penalty.

    \value EnableDebugMarkers Enables debug marker groups. Without this frame
    debugging features like making debug groups and custom resource name
    visible in external GPU debugging tools will not be available and functions
    like QRhiCommandBuffer::debugMarkBegin() will become a no-op. Avoid
    enabling in production builds as it may involve a performance penalty.
 */

/*!
    \enum QRhi::FrameOpResult
    Describes the result of operations that can have a soft failure.

    \value FrameOpSuccess Success

    \value FrameOpError Unspecified error

    \value FrameOpSwapChainOutOfDate The swapchain is in an inconsistent state
    internally. This can be recoverable by attempting to repeat the operation
    (such as, beginFrame()) later.

    \value FrameOpDeviceLost The graphics device was lost. This can be
    recoverable by attempting to repeat the operation (such as, beginFrame())
    and releasing and reinitializing all objects backed by native graphics
    resources.
 */

/*!
    \enum QRhi::Feature
    Flag values to indicate what features are supported by the backend currently in use.

    \value MultisampleTexture Textures with sample count larger than 1 are supported.
    \value MultisampleRenderBuffer Renderbuffers with sample count larger than 1 are supported.
    \value DebugMarkers Debug marker groups (and so QRhiCommandBuffer::debugMarkBegin()) are supported.
    \value Timestamps Command buffer timestamps are supported. Relevant for QRhiProfiler::gpuFrameTimes().
    \value Instancing Instanced drawing is supported.
    \value CustomInstanceStepRate Instance step rate other than 1 is supported.
 */

/*!
    \enum QRhi::ResourceSizeLimit
    Describes the resource limit to query.

    \value TextureSizeMin Minimum texture width and height. This is typically
    1. The minimum texture size is handled gracefully, meaning attempting to
    create a texture with an empty size will instead create a texture with the
    minimum size.

    \value TextureSizeMax Maximum texture width and height. This depends on the
    graphics API and sometimes the platform or implementation as well.
    Typically the value is in the range 4096 - 16384. Attempting to create
    textures larger than this is expected to fail.
 */

/*!
    \class QRhiInitParams
    \inmodule QtRhi
    \brief Base class for backend-specific initialization parameters.
 */

/*!
    \class QRhiColorClearValue
    \inmodule QtRhi
    \brief Specifies a clear color for a color buffer.
 */

/*!
    \class QRhiDepthStencilClearValue
    \inmodule QtRhi
    \brief Specifies clear values for a depth or stencil buffer.
 */

/*!
    \class QRhiViewport
    \inmodule QtRhi
    \brief Specifies a viewport rectangle.
 */

/*!
    \class QRhiScissor
    \inmodule QtRhi
    \brief Specifies a scissor rectangle.
 */

/*!
    \class QRhiVertexInputLayout
    \inmodule QtRhi
    \brief Describes the layout of vertex inputs consumed by a vertex shader.
 */

/*!
    \class QRhiVertexInputLayout::Binding
    \inmodule QtRhi
    \brief Describes a vertex input binding.
 */

/*!
    \class QRhiVertexInputLayout::Attribute
    \inmodule QtRhi
    \brief Describes a single vertex input element.
 */

/*!
    \class QRhiGraphicsShaderStage
    \inmodule QtRhi
    \brief Specifies the type and the shader code for a shader stage in the graphics pipeline.
 */

/*!
    \class QRhiShaderResourceBinding
    \inmodule QtRhi
    \brief Specifies the shader resources that are made visible to one or more shader stages.
 */

/*!
    \class QRhiTextureRenderTargetDescription
    \inmodule QtRhi
    \brief Describes the color and depth or depth/stencil attachments of a render target.
 */

/*!
    \class QRhiTextureRenderTargetDescription::ColorAttachment
    \inmodule QtRhi
    \brief Describes the color attachments of a render target.
 */

/*!
    \class QRhiTextureUploadDescription
    \inmodule QtRhi
    \brief Describes a texture upload operation.
 */

/*!
    \class QRhiTextureUploadDescription::Layer
    \inmodule QtRhi
    \brief Describes one layer (face for cubemaps) in a texture upload operation.
 */

/*!
    \class QRhiTextureUploadDescription::Layer::MipLevel
    \inmodule QtRhi
    \brief Describes one mip level in a layer in a texture upload operation.
 */

/*!
    \class QRhiTextureCopyDescription
    \inmodule QtRhi
    \brief Describes a texture-to-texture copy operation.
 */

/*!
    \class QRhiReadbackDescription
    \inmodule QtRhi
    \brief Describes a readback (reading back texture contents from possibly GPU-only memory) operation.
 */

/*!
    \class QRhiReadbackResult
    \inmodule QtRhi
    \brief Describes the results of a potentially asynchronous readback operation.
 */

/*!
    \class QRhiNativeHandles
    \inmodule QtRhi
    \brief Base class for classes exposing backend-specific collections of native resource objects.
 */

/*!
    \class QRhiResource
    \inmodule QtRhi
    \brief Base class for classes encapsulating native resource objects.
 */

/*!
    \class QRhiBuffer
    \inmodule QtRhi
    \brief Vertex, index, or uniform (constant) buffer resource.
 */

/*!
    \class QRhiTexture
    \inmodule QtRhi
    \brief Texture resource.
 */

/*!
    \class QRhiSampler
    \inmodule QtRhi
    \brief Sampler resource.
 */

/*!
    \class QRhiRenderBuffer
    \inmodule QtRhi
    \brief Renderbuffer resource.
 */

/*!
    \class QRhiRenderPassDescriptor
    \inmodule QtRhi
    \brief Render pass resource.
 */

/*!
    \class QRhiRenderTarget
    \inmodule QtRhi
    \brief Represents an onscreen (swapchain) or offscreen (texture) render target.
 */

/*!
    \class QRhiTextureRenderTarget
    \inmodule QtRhi
    \brief Texture render target resource.
 */

/*!
    \class QRhiShaderResourceBindings
    \inmodule QtRhi
    \brief Encapsulates resources for making buffer, texture, sampler resources visible to shaders.
 */

/*!
    \class QRhiGraphicsPipeline
    \inmodule QtRhi
    \brief Graphics pipeline state resource.
 */

/*!
    \class QRhiGraphicsPipeline::TargetBlend
    \inmodule QtRhi
    \brief Describes the blend state for one color attachment.
 */

/*!
    \class QRhiGraphicsPipeline::StencilOpState
    \inmodule QtRhi
    \brief Describes the stencil operation state.
 */

/*!
    \class QRhiSwapChain
    \inmodule QtRhi
    \brief Swapchain resource.
 */

/*!
    \class QRhiCommandBuffer
    \inmodule QtRhi
    \brief Command buffer resource.

    Not creatable by applications at the moment. The only ways to obtain a
    valid QRhiCommandBuffer are to get it from the targeted swapchain via
    QRhiSwapChain::currentFrameCommandBuffer(), or, in case of rendering
    compeletely offscreen, initializing one via beginOffscreenFrame().
 */

/*!
    \class QRhiResourceUpdateBatch
    \inmodule QtRhi
    \brief Records upload and copy type of operations.
 */

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

QByteArray QRhiResource::name() const
{
    return objectName;
}

void QRhiResource::setName(const QByteArray &name)
{
    objectName = name;
}

QRhiBuffer::QRhiBuffer(QRhiImplementation *rhi, Type type_, UsageFlags usage_, int size_)
    : QRhiResource(rhi),
      m_type(type_), m_usage(usage_), m_size(size_)
{
}

QRhiRenderBuffer::QRhiRenderBuffer(QRhiImplementation *rhi, Type type_, const QSize &pixelSize_,
                                   int sampleCount_, Flags flags_)
    : QRhiResource(rhi),
      m_type(type_), m_pixelSize(pixelSize_), m_sampleCount(sampleCount_), m_flags(flags_)
{
}

QRhiTexture::QRhiTexture(QRhiImplementation *rhi, Format format_, const QSize &pixelSize_,
                         int sampleCount_, Flags flags_)
    : QRhiResource(rhi),
      m_format(format_), m_pixelSize(pixelSize_), m_sampleCount(sampleCount_), m_flags(flags_)
{
}

const QRhiNativeHandles *QRhiTexture::nativeHandles()
{
    return nullptr;
}

bool QRhiTexture::buildFrom(const QRhiNativeHandles *src)
{
    Q_UNUSED(src);
    return false;
}

QRhiSampler::QRhiSampler(QRhiImplementation *rhi,
                         Filter magFilter_, Filter minFilter_, Filter mipmapMode_,
                         AddressMode u_, AddressMode v_, AddressMode w_)
    : QRhiResource(rhi),
      m_magFilter(magFilter_), m_minFilter(minFilter_), m_mipmapMode(mipmapMode_),
      m_addressU(u_), m_addressV(v_), m_addressW(w_)
{
}

QRhiRenderPassDescriptor::QRhiRenderPassDescriptor(QRhiImplementation *rhi)
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
                                                 const QRhiTextureRenderTargetDescription &desc_,
                                                 Flags flags_)
    : QRhiRenderTarget(rhi),
      m_desc(desc_),
      m_flags(flags_)
{
}

QRhiShaderResourceBindings::QRhiShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiShaderResourceBinding QRhiShaderResourceBinding::uniformBuffer(
        int binding_, StageFlags stage_, QRhiBuffer *buf_)
{
    QRhiShaderResourceBinding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = UniformBuffer;
    b.ubuf.buf = buf_;
    b.ubuf.offset = 0;
    b.ubuf.maybeSize = 0; // entire buffer
    return b;
}

QRhiShaderResourceBinding QRhiShaderResourceBinding::uniformBuffer(
        int binding_, StageFlags stage_, QRhiBuffer *buf_, int offset_, int size_)
{
    Q_ASSERT(size_ > 0);
    QRhiShaderResourceBinding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = UniformBuffer;
    b.ubuf.buf = buf_;
    b.ubuf.offset = offset_;
    b.ubuf.maybeSize = size_;
    return b;
}

QRhiShaderResourceBinding QRhiShaderResourceBinding::sampledTexture(
        int binding_, StageFlags stage_, QRhiTexture *tex_, QRhiSampler *sampler_)
{
    QRhiShaderResourceBinding b;
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

void QRhiImplementation::sendVMemStatsToProfiler()
{
    // nothing to do in the default implementation
}

bool QRhiImplementation::isCompressedFormat(QRhiTexture::Format format) const
{
    return (format >= QRhiTexture::BC1 && format <= QRhiTexture::BC7)
            || (format >= QRhiTexture::ETC2_RGB8 && format <= QRhiTexture::ETC2_RGBA8)
            || (format >= QRhiTexture::ASTC_4x4 && format <= QRhiTexture::ASTC_12x12);
}

void QRhiImplementation::compressedFormatInfo(QRhiTexture::Format format, const QSize &size,
                                              quint32 *bpl, quint32 *byteSize,
                                              QSize *blockDim) const
{
    int xdim = 4;
    int ydim = 4;
    quint32 blockSize = 0;

    switch (format) {
    case QRhiTexture::BC1:
        blockSize = 8;
        break;
    case QRhiTexture::BC2:
        blockSize = 16;
        break;
    case QRhiTexture::BC3:
        blockSize = 16;
        break;
    case QRhiTexture::BC4:
        blockSize = 8;
        break;
    case QRhiTexture::BC5:
        blockSize = 16;
        break;
    case QRhiTexture::BC6H:
        blockSize = 16;
        break;
    case QRhiTexture::BC7:
        blockSize = 16;
        break;

    case QRhiTexture::ETC2_RGB8:
        blockSize = 8;
        break;
    case QRhiTexture::ETC2_RGB8A1:
        blockSize = 8;
        break;
    case QRhiTexture::ETC2_RGBA8:
        blockSize = 16;
        break;

    case QRhiTexture::ASTC_4x4:
        blockSize = 16;
        break;
    case QRhiTexture::ASTC_5x4:
        blockSize = 16;
        xdim = 5;
        break;
    case QRhiTexture::ASTC_5x5:
        blockSize = 16;
        xdim = ydim = 5;
        break;
    case QRhiTexture::ASTC_6x5:
        blockSize = 16;
        xdim = 6;
        ydim = 5;
        break;
    case QRhiTexture::ASTC_6x6:
        blockSize = 16;
        xdim = ydim = 6;
        break;
    case QRhiTexture::ASTC_8x5:
        blockSize = 16;
        xdim = 8;
        ydim = 5;
        break;
    case QRhiTexture::ASTC_8x6:
        blockSize = 16;
        xdim = 8;
        ydim = 6;
        break;
    case QRhiTexture::ASTC_8x8:
        blockSize = 16;
        xdim = ydim = 8;
        break;
    case QRhiTexture::ASTC_10x5:
        blockSize = 16;
        xdim = 10;
        ydim = 5;
        break;
    case QRhiTexture::ASTC_10x6:
        blockSize = 16;
        xdim = 10;
        ydim = 6;
        break;
    case QRhiTexture::ASTC_10x8:
        blockSize = 16;
        xdim = 10;
        ydim = 8;
        break;
    case QRhiTexture::ASTC_10x10:
        blockSize = 16;
        xdim = ydim = 10;
        break;
    case QRhiTexture::ASTC_12x10:
        blockSize = 16;
        xdim = 12;
        ydim = 10;
        break;
    case QRhiTexture::ASTC_12x12:
        blockSize = 16;
        xdim = ydim = 12;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    const quint32 wblocks = (size.width() + xdim - 1) / xdim;
    const quint32 hblocks = (size.height() + ydim - 1) / ydim;

    if (bpl)
        *bpl = wblocks * blockSize;
    if (byteSize)
        *byteSize = wblocks * hblocks * blockSize;
    if (blockDim)
        *blockDim = QSize(xdim, ydim);
}

void QRhiImplementation::textureFormatInfo(QRhiTexture::Format format, const QSize &size,
                                           quint32 *bpl, quint32 *byteSize) const
{
    if (isCompressedFormat(format)) {
        compressedFormatInfo(format, size, bpl, byteSize, nullptr);
        return;
    }

    quint32 bpc = 0;
    switch (format) {
    case QRhiTexture::RGBA8:
        bpc = 4;
        break;
    case QRhiTexture::BGRA8:
        bpc = 4;
        break;
    case QRhiTexture::R8:
        bpc = 1;
        break;
    case QRhiTexture::R16:
        bpc = 2;
        break;

    case QRhiTexture::D16:
        bpc = 2;
        break;
    case QRhiTexture::D32:
        bpc = 4;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    if (bpl)
        *bpl = size.width() * bpc;
    if (byteSize)
        *byteSize = size.width() * size.height() * bpc;
}

// Approximate because it excludes subresource alignment or multisampling.
quint32 QRhiImplementation::approxByteSizeForTexture(QRhiTexture::Format format, const QSize &baseSize,
                                                     int mipCount, int layerCount)
{
    quint32 approxSize = 0;
    for (int level = 0; level < mipCount; ++level) {
        quint32 byteSize = 0;
        const QSize size(qFloor(float(qMax(1, baseSize.width() >> level))),
                         qFloor(float(qMax(1, baseSize.height() >> level))));
        textureFormatInfo(format, size, nullptr, &byteSize);
        approxSize += byteSize;
    }
    approxSize *= layerCount;
    return approxSize;
}

QRhi::QRhi()
{
}

QRhi::~QRhi()
{
    if (d) {
        d->destroy();
        delete d;
    }
}

QRhi *QRhi::create(Implementation impl, QRhiInitParams *params, Flags flags)
{
    QScopedPointer<QRhi> r(new QRhi);

    switch (impl) {
    case Null:
        r->d = new QRhiNull(params);
        break;
    case Vulkan:
#if QT_CONFIG(vulkan)
        r->d = new QRhiVulkan(params);
        break;
#else
        qWarning("This build of Qt has no Vulkan support");
        break;
#endif
    case OpenGLES2:
#ifndef QT_NO_OPENGL
        r->d = new QRhiGles2(params);
        break;
#else
        qWarning("This build of Qt has no OpenGL support");
        break;
#endif
    case D3D11:
#ifdef Q_OS_WIN
        r->d = new QRhiD3D11(params);
        break;
#else
        qWarning("This platform has no Direct3D 11 support");
        break;
#endif
    case Metal:
#ifdef Q_OS_DARWIN
        r->d = new QRhiMetal(params);
        break;
#else
        qWarning("This platform has no Metal support");
        break;
#endif
    default:
        break;
    }

    if (r->d) {
        if (flags.testFlag(EnableProfiling)) {
            QRhiProfilerPrivate *profD = QRhiProfilerPrivate::get(&r->d->profiler);
            profD->rhi = r.data();
            profD->rhiD = r->d;
        }
        r->d->debugMarkers = flags.testFlag(EnableDebugMarkers);
        if (r->d->create(flags))
            return r.take();
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

void QRhiResourceUpdateBatch::merge(QRhiResourceUpdateBatch *other)
{
    d->merge(other->d);
}

void QRhiResourceUpdateBatch::updateDynamicBuffer(QRhiBuffer *buf, int offset, int size, const void *data)
{
    d->dynamicBufferUpdates.append({ buf, offset, size, data });
}

void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, int offset, int size, const void *data)
{
    d->staticBufferUploads.append({ buf, offset, size, data });
}

void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, const void *data)
{
    d->staticBufferUploads.append({ buf, 0, 0, data });
}

void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QRhiTextureUploadDescription &desc)
{
    d->textureUploads.append({ tex, desc });
}

void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QImage &image)
{
    uploadTexture(tex, {{{{{ image }}}}});
}

void QRhiResourceUpdateBatch::copyTexture(QRhiTexture *dst, QRhiTexture *src, const QRhiTextureCopyDescription &desc)
{
    d->textureCopies.append({ dst, src, desc });
}

void QRhiResourceUpdateBatch::readBackTexture(const QRhiReadbackDescription &rb, QRhiReadbackResult *result)
{
    d->textureReadbacks.append({ rb, result });
}

void QRhiResourceUpdateBatch::generateMips(QRhiTexture *tex)
{
    d->textureMipGens.append(QRhiResourceUpdateBatchPrivate::TextureMipGen(tex));
}

void QRhiResourceUpdateBatch::prepareTextureForUse(QRhiTexture *tex, TexturePrepareFlags flags)
{
    d->texturePrepares.append({ tex, flags });
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
    textureCopies.clear();
    textureReadbacks.clear();
    textureMipGens.clear();
    texturePrepares.clear();

    rhi->resUpdPoolMap.clearBit(poolIndex);
    poolIndex = -1;
}

void QRhiResourceUpdateBatchPrivate::merge(QRhiResourceUpdateBatchPrivate *other)
{
    dynamicBufferUpdates += other->dynamicBufferUpdates;
    staticBufferUploads += other->staticBufferUploads;
    textureUploads += other->textureUploads;
    textureCopies += other->textureCopies;
    textureReadbacks += other->textureReadbacks;
    textureMipGens += other->textureMipGens;
    texturePrepares += other->texturePrepares;
}

void QRhiCommandBuffer::resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->resourceUpdate(this, resourceUpdates);
}

void QRhiCommandBuffer::beginPass(QRhiRenderTarget *rt,
                                  const QRhiColorClearValue &colorClearValue,
                                  const QRhiDepthStencilClearValue &depthStencilClearValue,
                                  QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->beginPass(this, rt, colorClearValue, depthStencilClearValue, resourceUpdates);
}

void QRhiCommandBuffer::endPass(QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->endPass(this, resourceUpdates);
}

void QRhiCommandBuffer::setGraphicsPipeline(QRhiGraphicsPipeline *ps,
                                            QRhiShaderResourceBindings *srb)
{
    rhi->setGraphicsPipeline(this, ps, srb);
}

void QRhiCommandBuffer::setVertexInput(int startBinding, const QVector<VertexInput> &bindings,
                                       QRhiBuffer *indexBuf, quint32 indexOffset,
                                       IndexFormat indexFormat)
{
    rhi->setVertexInput(this, startBinding, bindings, indexBuf, indexOffset, indexFormat);
}

void QRhiCommandBuffer::setViewport(const QRhiViewport &viewport)
{
    rhi->setViewport(this, viewport);
}

void QRhiCommandBuffer::setScissor(const QRhiScissor &scissor)
{
    rhi->setScissor(this, scissor);
}

void QRhiCommandBuffer::setBlendConstants(const QVector4D &c)
{
    rhi->setBlendConstants(this, c);
}

void QRhiCommandBuffer::setStencilRef(quint32 refValue)
{
    rhi->setStencilRef(this, refValue);
}

void QRhiCommandBuffer::draw(quint32 vertexCount,
                             quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    rhi->draw(this, vertexCount, instanceCount, firstVertex, firstInstance);
}

void QRhiCommandBuffer::drawIndexed(quint32 indexCount,
                                    quint32 instanceCount, quint32 firstIndex,
                                    qint32 vertexOffset, quint32 firstInstance)
{
    rhi->drawIndexed(this, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void QRhiCommandBuffer::debugMarkBegin(const QByteArray &name)
{
    rhi->debugMarkBegin(this, name);
}

void QRhiCommandBuffer::debugMarkEnd()
{
    rhi->debugMarkEnd(this);
}

void QRhiCommandBuffer::debugMarkMsg(const QByteArray &msg)
{
    rhi->debugMarkMsg(this, msg);
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
    const int w = qFloor(float(qMax(1, baseLevelSize.width() >> mipLevel)));
    const int h = qFloor(float(qMax(1, baseLevelSize.height() >> mipLevel)));
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

bool QRhi::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const
{
    return d->isTextureFormatSupported(format, flags);
}

bool QRhi::isFeatureSupported(QRhi::Feature feature) const
{
    return d->isFeatureSupported(feature);
}

int QRhi::resourceSizeLimit(ResourceSizeLimit limit) const
{
    return d->resourceSizeLimit(limit);
}

const QRhiNativeHandles *QRhi::nativeHandles()
{
    return d->nativeHandles();
}

QRhiProfiler *QRhi::profiler()
{
    return &d->profiler;
}

QRhiGraphicsPipeline *QRhi::newGraphicsPipeline()
{
    return d->createGraphicsPipeline();
}

QRhiShaderResourceBindings *QRhi::newShaderResourceBindings()
{
    return d->createShaderResourceBindings();
}

QRhiBuffer *QRhi::newBuffer(QRhiBuffer::Type type,
                            QRhiBuffer::UsageFlags usage,
                            int size)
{
    return d->createBuffer(type, usage, size);
}

QRhiRenderBuffer *QRhi::newRenderBuffer(QRhiRenderBuffer::Type type,
                                        const QSize &pixelSize,
                                        int sampleCount,
                                        QRhiRenderBuffer::Flags flags)
{
    return d->createRenderBuffer(type, pixelSize, sampleCount, flags);
}

QRhiTexture *QRhi::newTexture(QRhiTexture::Format format,
                              const QSize &pixelSize,
                              int sampleCount,
                              QRhiTexture::Flags flags)
{
    return d->createTexture(format, pixelSize, sampleCount, flags);
}

QRhiSampler *QRhi::newSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                              QRhiSampler::Filter mipmapMode,
                              QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w)
{
    return d->createSampler(magFilter, minFilter, mipmapMode, u, v, w);
}

QRhiTextureRenderTarget *QRhi::newTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                      QRhiTextureRenderTarget::Flags flags)
{
    return d->createTextureRenderTarget(desc, flags);
}

QRhiSwapChain *QRhi::newSwapChain()
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

QRhi::FrameOpResult QRhi::beginOffscreenFrame(QRhiCommandBuffer **cb)
{
    return d->beginOffscreenFrame(cb);
}

QRhi::FrameOpResult QRhi::endOffscreenFrame()
{
    return d->endOffscreenFrame();
}

QRhi::FrameOpResult QRhi::finish()
{
    return d->finish();
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
