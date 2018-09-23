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

#include "qrhigles2_p.h"
#include <QWindow>

QT_BEGIN_NAMESPACE

QRhiGles2::QRhiGles2(QRhiInitParams *params)
{
    QRhiGles2InitParams *glparams = static_cast<QRhiGles2InitParams *>(params);
    ctx = glparams->context;
    fallbackSurface = glparams->nonVisualSurface;

    create();
}

QRhiGles2::~QRhiGles2()
{
    destroy();
}

// Initialization, teardown, beginFrame(), and every build() take care of
// making the context and the (window or fallback) surface current, if needed.
// Others do not - if the applications mess with the GL context on the thread
// within a begin-endFrame, it is up to them to restore before entering the
// next rhi function that may issue GL calls.

void QRhiGles2::ensureContext(QSurface *surface)
{
    bool nativeWindowGone = false;
    if (surface && surface->surfaceClass() == QSurface::Window && !surface->surfaceHandle()) {
        surface = fallbackSurface;
        nativeWindowGone = true;
    }

    // When surface is null, we do not know what surface to use (since only
    // begin-endFrame is tied to a swapchain; the concept maps badly to GL
    // where any build() needs a current context as well). Use the
    // QOffscreenSurface in this case - but note the early out below which
    // minimizes changes since a window surface (from the swapchain) is good
    // enough as well when it's still current.
    if (!surface)
        surface = fallbackSurface;

    // Minimize makeCurrent calls since it is not guaranteed to have any
    // return-if-same checks internally. Make sure the makeCurrent is never
    // omitted after a swapBuffers, and when surface was specified explicitly.
    if (buffersSwapped)
        buffersSwapped = false;
    else if (!nativeWindowGone && QOpenGLContext::currentContext() == ctx && (surface == fallbackSurface || ctx->surface() == surface))
        return;

    if (!ctx->makeCurrent(surface))
        qWarning("QRhiGles2: Failed to make context current. Expect bad things to happen.");
}

void QRhiGles2::create()
{
    Q_ASSERT(ctx);
    Q_ASSERT(fallbackSurface);

    ensureContext();

    f = ctx->functions();
}

void QRhiGles2::destroy()
{
    if (!f)
        return;

    ensureContext();
    executeDeferredReleases();

    f = nullptr;
}

void QRhiGles2::executeDeferredReleases()
{
    for (int i = releaseQueue.count() - 1; i >= 0; --i) {
        const QRhiGles2::DeferredReleaseEntry &e(releaseQueue[i]);
        switch (e.type) {
        case QRhiGles2::DeferredReleaseEntry::Buffer:
            f->glDeleteBuffers(1, &e.buffer.buffer);
            break;
        case QRhiGles2::DeferredReleaseEntry::Pipeline:
            f->glDeleteProgram(e.pipeline.program);
            break;
        default:
            break;
        }
        releaseQueue.removeAt(i);
    }
}

QVector<int> QRhiGles2::supportedSampleCounts() const
{
    return { 1 };
}

QRhiSwapChain *QRhiGles2::createSwapChain()
{
    return new QGles2SwapChain(this);
}

QRhiBuffer *QRhiGles2::createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, int size)
{
    return new QGles2Buffer(this, type, usage, size);
}

int QRhiGles2::ubufAlignment() const
{
    return 256;
}

QRhiRenderBuffer *QRhiGles2::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize, int sampleCount)
{
    return new QGles2RenderBuffer(this, type, pixelSize, sampleCount);
}

QRhiTexture *QRhiGles2::createTexture(QRhiTexture::Format format, const QSize &pixelSize, QRhiTexture::Flags flags)
{
    return new QGles2Texture(this, format, pixelSize, flags);
}

QRhiSampler *QRhiGles2::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                      QRhiSampler::Filter mipmapMode,
                                      QRhiSampler::AddressMode u, QRhiSampler::AddressMode v)
{
    return new QGles2Sampler(this, magFilter, minFilter, mipmapMode, u, v);
}

QRhiTextureRenderTarget *QRhiGles2::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QGles2TextureRenderTarget(this, texture, flags);
}

QRhiTextureRenderTarget *QRhiGles2::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiRenderBuffer *depthStencilBuffer,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QGles2TextureRenderTarget(this, texture, depthStencilBuffer, flags);
}

QRhiTextureRenderTarget *QRhiGles2::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiTexture *depthTexture,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QGles2TextureRenderTarget(this, texture, depthTexture, flags);
}

QRhiGraphicsPipeline *QRhiGles2::createGraphicsPipeline()
{
    return new QGles2GraphicsPipeline(this);
}

QRhiShaderResourceBindings *QRhiGles2::createShaderResourceBindings()
{
    return new QGles2ShaderResourceBindings(this);
}

static inline GLenum toGlCullMode(QRhiGraphicsPipeline::CullMode mode)
{
    if (mode.testFlag(QRhiGraphicsPipeline::Front)) {
        if (mode.testFlag(QRhiGraphicsPipeline::Back))
            return GL_FRONT_AND_BACK;
        return GL_FRONT;
    }
    return GL_BACK;
}

static inline GLenum toGlFrontFace(QRhiGraphicsPipeline::FrontFace f)
{
    switch (f) {
    case QRhiGraphicsPipeline::CCW:
        return GL_CCW;
    case QRhiGraphicsPipeline::CW:
        return GL_CW;
    default:
        Q_UNREACHABLE();
        return GL_CCW;
    }
}

static inline GLenum toGlBlendFactor(QRhiGraphicsPipeline::BlendFactor f)
{
    switch (f) {
    case QRhiGraphicsPipeline::Zero:
        return GL_ZERO;
    case QRhiGraphicsPipeline::One:
        return GL_ONE;
    case QRhiGraphicsPipeline::SrcColor:
        return GL_SRC_COLOR;
    case QRhiGraphicsPipeline::OneMinusSrcColor:
        return GL_ONE_MINUS_SRC_COLOR;
    case QRhiGraphicsPipeline::DstColor:
        return GL_DST_COLOR;
    case QRhiGraphicsPipeline::OneMinusDstColor:
        return GL_ONE_MINUS_DST_COLOR;
    case QRhiGraphicsPipeline::SrcAlpha:
        return GL_SRC_ALPHA;
    case QRhiGraphicsPipeline::OneMinusSrcAlpha:
        return GL_ONE_MINUS_SRC_ALPHA;
    case QRhiGraphicsPipeline::DstAlpha:
        return GL_DST_ALPHA;
    case QRhiGraphicsPipeline::OneMinusDstAlpha:
        return GL_ONE_MINUS_DST_ALPHA;
    case QRhiGraphicsPipeline::ConstantColor:
        return GL_CONSTANT_COLOR;
    case QRhiGraphicsPipeline::OneMinusConstantColor:
        return GL_ONE_MINUS_CONSTANT_COLOR;
    case QRhiGraphicsPipeline::ConstantAlpha:
        return GL_CONSTANT_ALPHA;
    case QRhiGraphicsPipeline::OneMinusConstantAlpha:
        return GL_ONE_MINUS_CONSTANT_ALPHA;
    case QRhiGraphicsPipeline::SrcAlphaSaturate:
        return GL_SRC_ALPHA_SATURATE;
    case QRhiGraphicsPipeline::Src1Color:
        Q_FALLTHROUGH();
    case QRhiGraphicsPipeline::OneMinusSrc1Color:
        Q_FALLTHROUGH();
    case QRhiGraphicsPipeline::Src1Alpha:
        Q_FALLTHROUGH();
    case QRhiGraphicsPipeline::OneMinusSrc1Alpha:
        qWarning("Unsupported blend factor %x", f);
        return GL_ZERO;
    default:
        Q_UNREACHABLE();
        return GL_ZERO;
    }
}

static inline GLenum toGlBlendOp(QRhiGraphicsPipeline::BlendOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::Add:
        return GL_ADD;
    case QRhiGraphicsPipeline::Subtract:
        return GL_SUBTRACT;
    case QRhiGraphicsPipeline::ReverseSubtract:
        return GL_FUNC_REVERSE_SUBTRACT;
    case QRhiGraphicsPipeline::Min:
        return GL_MIN;
    case QRhiGraphicsPipeline::Max:
        return GL_MAX;
    default:
        Q_UNREACHABLE();
        return GL_ADD;
    }
}

static inline GLenum toGlCompareOp(QRhiGraphicsPipeline::CompareOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::Never:
        return GL_NEVER;
    case QRhiGraphicsPipeline::Less:
        return GL_LESS;
    case QRhiGraphicsPipeline::Equal:
        return GL_EQUAL;
    case QRhiGraphicsPipeline::LessOrEqual:
        return GL_LEQUAL;
    case QRhiGraphicsPipeline::Greater:
        return GL_GREATER;
    case QRhiGraphicsPipeline::NotEqual:
        return GL_NOTEQUAL;
    case QRhiGraphicsPipeline::GreaterOrEqual:
        return GL_GEQUAL;
    case QRhiGraphicsPipeline::Always:
        return GL_ALWAYS;
    default:
        Q_UNREACHABLE();
        return GL_ALWAYS;
    }
}

static inline GLenum toGlStencilOp(QRhiGraphicsPipeline::StencilOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::StencilZero:
        return GL_ZERO;
    case QRhiGraphicsPipeline::Keep:
        return GL_KEEP;
    case QRhiGraphicsPipeline::Replace:
        return GL_REPLACE;
    case QRhiGraphicsPipeline::IncrementAndClamp:
        return GL_INCR;
    case QRhiGraphicsPipeline::DecrementAndClamp:
        return GL_DECR;
    case QRhiGraphicsPipeline::Invert:
        return GL_INVERT;
    case QRhiGraphicsPipeline::IncrementAndWrap:
        return GL_INCR_WRAP;
    case QRhiGraphicsPipeline::DecrementAndWrap:
        return GL_DECR_WRAP;
    default:
        Q_UNREACHABLE();
        return GL_KEEP;
    }
}

void QRhiGles2::setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb)
{
    Q_ASSERT(inPass);

    if (!srb)
        srb = ps->shaderResourceBindings;

    QGles2GraphicsPipeline *psD = QRHI_RES(QGles2GraphicsPipeline, ps);
    QGles2ShaderResourceBindings *srbD = QRHI_RES(QGles2ShaderResourceBindings, srb);
    QGles2CommandBuffer *cbD = QRHI_RES(QGles2CommandBuffer, cb);

    if (cbD->currentPipeline != ps || cbD->currentPipelineGeneration != psD->generation) {
        cbD->currentPipeline = ps;
        cbD->currentPipelineGeneration = psD->generation;

        // ### this needs some proper caching later on to minimize state changes

        f->glCullFace(toGlCullMode(ps->cullMode));
        f->glFrontFace(toGlFrontFace(ps->frontFace));
        if (!ps->targetBlends.isEmpty()) {
            const QRhiGraphicsPipeline::TargetBlend &blend(ps->targetBlends.first()); // no MRT
            GLboolean wr = blend.colorWrite.testFlag(QRhiGraphicsPipeline::R);
            GLboolean wg = blend.colorWrite.testFlag(QRhiGraphicsPipeline::G);
            GLboolean wb = blend.colorWrite.testFlag(QRhiGraphicsPipeline::B);
            GLboolean wa = blend.colorWrite.testFlag(QRhiGraphicsPipeline::A);
            f->glColorMask(wr, wg, wb, wa);
            if (blend.enable) {
                f->glEnable(GL_BLEND);
                f->glBlendFuncSeparate(toGlBlendFactor(blend.srcColor),
                                       toGlBlendFactor(blend.dstColor),
                                       toGlBlendFactor(blend.srcAlpha),
                                       toGlBlendFactor(blend.dstAlpha));
                f->glBlendEquationSeparate(toGlBlendOp(blend.opColor), toGlBlendOp(blend.opAlpha));
            } else {
                f->glDisable(GL_BLEND);
            }
        }
        if (ps->depthTest)
            f->glEnable(GL_DEPTH_TEST);
        else
            f->glDisable(GL_DEPTH_TEST);
        if (ps->depthWrite)
            f->glDepthMask(GL_TRUE);
        else
            f->glDepthMask(GL_FALSE);
        f->glDepthFunc(toGlCompareOp(ps->depthOp));
        if (ps->stencilTest) {
            f->glEnable(GL_STENCIL_TEST);
            f->glStencilFuncSeparate(GL_FRONT, toGlCompareOp(ps->stencilFront.compareOp), 0, ps->stencilReadMask);
            f->glStencilOpSeparate(GL_FRONT,
                                   toGlStencilOp(ps->stencilFront.failOp),
                                   toGlStencilOp(ps->stencilFront.depthFailOp),
                                   toGlStencilOp(ps->stencilFront.passOp));
            f->glStencilMaskSeparate(GL_FRONT, ps->stencilWriteMask);
            f->glStencilFuncSeparate(GL_BACK, toGlCompareOp(ps->stencilBack.compareOp), 0, ps->stencilReadMask);
            f->glStencilOpSeparate(GL_BACK,
                                   toGlStencilOp(ps->stencilBack.failOp),
                                   toGlStencilOp(ps->stencilBack.depthFailOp),
                                   toGlStencilOp(ps->stencilBack.passOp));
            f->glStencilMaskSeparate(GL_BACK, ps->stencilWriteMask);
        } else {
            f->glDisable(GL_STENCIL_TEST);
        }
        f->glUseProgram(psD->program);
    }

    if (cbD->currentSrb != srb || cbD->currentSrbGeneration != srbD->generation) {
        cbD->currentSrb = srb;
        cbD->currentSrbGeneration = srbD->generation;
        // ###
    }
}

void QRhiGles2::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<QRhi::VertexInput> &bindings,
                               QRhiBuffer *indexBuf, quint32 indexOffset, QRhi::IndexFormat indexFormat)
{
    Q_ASSERT(inPass);
}

void QRhiGles2::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
    Q_ASSERT(inPass);
}

void QRhiGles2::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
    Q_ASSERT(inPass);
}

void QRhiGles2::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
    Q_ASSERT(inPass);
}

void QRhiGles2::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
    Q_ASSERT(inPass);
}

void QRhiGles2::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                     quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    Q_ASSERT(inPass);
}

void QRhiGles2::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                            quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
    Q_ASSERT(inPass);
}

void QRhiGles2::prepareNewFrame(QRhiCommandBuffer *cb)
{
    Q_ASSERT(!inFrame);
    inFrame = true;

    executeDeferredReleases();

    QRHI_RES(QGles2CommandBuffer, cb)->resetState();
}

void QRhiGles2::finishFrame()
{
    Q_ASSERT(inFrame);
    inFrame = false;
    ++finishedFrameCount;
}

QRhi::FrameOpResult QRhiGles2::beginFrame(QRhiSwapChain *swapChain)
{
    QGles2SwapChain *swapChainD = QRHI_RES(QGles2SwapChain, swapChain);
    ensureContext(swapChainD->surface);

    prepareNewFrame(&swapChainD->cb);

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiGles2::endFrame(QRhiSwapChain *swapChain)
{
    finishFrame();

    QGles2SwapChain *swapChainD = QRHI_RES(QGles2SwapChain, swapChain);
    if (swapChainD->surface) {
        ctx->swapBuffers(swapChainD->surface);
        buffersSwapped = true;
    }

    return QRhi::FrameOpSuccess;
}

void QRhiGles2::applyPassUpdates(QRhiCommandBuffer *cb, const QRhi::PassUpdates &updates)
{
    Q_UNUSED(cb);

    for (const QRhi::DynamicBufferUpdate &u : updates.dynamicBufferUpdates) {
        Q_ASSERT(!u.buf->isStatic());
        QGles2Buffer *bufD = QRHI_RES(QGles2Buffer, u.buf);
        if (u.buf->usage.testFlag(QRhiBuffer::UniformBuffer)) {
            memcpy(bufD->ubuf.data() + u.offset, u.data.constData(), u.data.size());
            QGles2Buffer::ChangeRange &r(bufD->ubufChangeRange);
            if (r.changeBegin == -1 || u.offset < r.changeBegin)
                r.changeBegin = u.offset;
            if (r.changeEnd == -1 || u.offset + u.data.size() > r.changeEnd)
                r.changeEnd = u.offset + u.data.size();
        } else {
            f->glBindBuffer(bufD->target, bufD->buffer);
            f->glBufferSubData(bufD->target, u.offset, u.data.size(), u.data.constData());
        }
    }

    for (const QRhi::StaticBufferUpload &u : updates.staticBufferUploads) {
        Q_ASSERT(u.buf->isStatic());
        QGles2Buffer *bufD = QRHI_RES(QGles2Buffer, u.buf);
        Q_ASSERT(u.data.size() == u.buf->size);
        if (u.buf->usage.testFlag(QRhiBuffer::UniformBuffer)) {
            memcpy(bufD->ubuf.data(), u.data.constData(), u.data.size());
        } else {
            f->glBindBuffer(bufD->target, bufD->buffer);
            f->glBufferData(bufD->target, u.data.size(), u.data.constData(), GL_STATIC_DRAW);
        }
    }

    for (const QRhi::TextureUpload &u : updates.textureUploads) {
    }
}

void QRhiGles2::beginPass(QRhiRenderTarget *rt, QRhiCommandBuffer *cb, const QRhiClearValue *clearValues, const QRhi::PassUpdates &updates)
{
    Q_ASSERT(!inPass);

    applyPassUpdates(cb, updates);

    bool needsColorClear = true;
    QGles2BasicRenderTargetData *rtD = nullptr;
    switch (rt->type()) {
    case QRhiRenderTarget::RtRef:
        rtD = &static_cast<QGles2ReferenceRenderTarget *>(rt)->d;
        break;
    case QRhiRenderTarget::RtTexture:
    {
        QGles2TextureRenderTarget *rtTex = static_cast<QGles2TextureRenderTarget *>(rt);
        rtD = &rtTex->d;
        needsColorClear = !rtTex->flags.testFlag(QRhiTextureRenderTarget::PreserveColorContents);
        // ### activateTextureRenderTarget(cb, rtTex);
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    QGles2CommandBuffer *cbD = QRHI_RES(QGles2CommandBuffer, cb);
    cbD->currentTarget = rt;

    GLbitfield clearMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    if (needsColorClear) {
        clearMask |= GL_COLOR_BUFFER_BIT;
        const QVector4D &c(clearValues->rgba);
        f->glClearColor(c.x(), c.y(), c.z(), c.w());
    }
    f->glClear(clearMask);

    inPass = true;
}

void QRhiGles2::endPass(QRhiCommandBuffer *cb)
{
    Q_ASSERT(inPass);
    inPass = false;

    QGles2CommandBuffer *cbD = QRHI_RES(QGles2CommandBuffer, cb);
    if (cbD->currentTarget->type() == QRhiRenderTarget::RtTexture)
    {}
        // ### deactivateTextureRenderTarget(cb, static_cast<QRhiTextureRenderTarget *>(cbD->currentTarget));

    cbD->currentTarget = nullptr;
}

QGles2Buffer::QGles2Buffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size)
    : QRhiBuffer(rhi, type, usage, size)
{
}

void QGles2Buffer::release()
{
    if (!buffer)
        return;

    QRhiGles2::DeferredReleaseEntry e;
    e.type = QRhiGles2::DeferredReleaseEntry::Buffer;

    e.buffer.buffer = buffer;

    buffer = 0;

    QRHI_RES_RHI(QRhiGles2);
    rhiD->releaseQueue.append(e);
}

bool QGles2Buffer::build()
{
    QRHI_RES_RHI(QRhiGles2);

    if (buffer)
        release();

    if (usage.testFlag(QRhiBuffer::UniformBuffer)) {
        // special since we do not support uniform blocks in this backend
        ubuf.resize(size);
        return true;
    }

    rhiD->ensureContext();

    if (usage.testFlag(QRhiBuffer::VertexBuffer))
        target = GL_ARRAY_BUFFER;
    if (usage.testFlag(QRhiBuffer::IndexBuffer))
        target = GL_ELEMENT_ARRAY_BUFFER;

    rhiD->f->glGenBuffers(1, &buffer);
    rhiD->f->glBindBuffer(target, buffer);
    rhiD->f->glBufferData(target, size, nullptr, isStatic() ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

    return true;
}

QGles2RenderBuffer::QGles2RenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize, int sampleCount)
    : QRhiRenderBuffer(rhi, type, pixelSize, sampleCount)
{
}

void QGles2RenderBuffer::release()
{
}

bool QGles2RenderBuffer::build()
{
    return true;
}

QGles2Texture::QGles2Texture(QRhiImplementation *rhi, Format format, const QSize &pixelSize, Flags flags)
    : QRhiTexture(rhi, format, pixelSize, flags)
{
}

void QGles2Texture::release()
{
}

bool QGles2Texture::build()
{
    return true;
}

QGles2Sampler::QGles2Sampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode, AddressMode u, AddressMode v)
    : QRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v)
{
}

void QGles2Sampler::release()
{
}

bool QGles2Sampler::build()
{
    return true;
}

QGles2RenderPass::QGles2RenderPass(QRhiImplementation *rhi)
    : QRhiRenderPass(rhi)
{
}

void QGles2RenderPass::release()
{
}

QGles2ReferenceRenderTarget::QGles2ReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiReferenceRenderTarget(rhi),
      d(rhi)
{
}

void QGles2ReferenceRenderTarget::release()
{
    // nothing to do here
}

QRhiRenderTarget::Type QGles2ReferenceRenderTarget::type() const
{
    return RtRef;
}

QSize QGles2ReferenceRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

const QRhiRenderPass *QGles2ReferenceRenderTarget::renderPass() const
{
    return &d.rp;
}

QGles2TextureRenderTarget::QGles2TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, Flags flags)
    : QRhiTextureRenderTarget(rhi, texture, flags),
      d(rhi)
{
}

QGles2TextureRenderTarget::QGles2TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiRenderBuffer *depthStencilBuffer, Flags flags)
    : QRhiTextureRenderTarget(rhi, texture, depthStencilBuffer, flags),
      d(rhi)
{
}

QGles2TextureRenderTarget::QGles2TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiTexture *depthTexture, Flags flags)
    : QRhiTextureRenderTarget(rhi, texture, depthTexture, flags),
      d(rhi)
{
}

void QGles2TextureRenderTarget::release()
{
}

bool QGles2TextureRenderTarget::build()
{
    return true;
}

QRhiRenderTarget::Type QGles2TextureRenderTarget::type() const
{
    return RtTexture;
}

QSize QGles2TextureRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

const QRhiRenderPass *QGles2TextureRenderTarget::renderPass() const
{
    return &d.rp;
}

QGles2ShaderResourceBindings::QGles2ShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiShaderResourceBindings(rhi)
{
}

void QGles2ShaderResourceBindings::release()
{
}

bool QGles2ShaderResourceBindings::build()
{
    generation += 1;
    return true;
}

QGles2GraphicsPipeline::QGles2GraphicsPipeline(QRhiImplementation *rhi)
    : QRhiGraphicsPipeline(rhi)
{
}

void QGles2GraphicsPipeline::release()
{
    if (!program)
        return;

    QRhiGles2::DeferredReleaseEntry e;
    e.type = QRhiGles2::DeferredReleaseEntry::Pipeline;

    e.pipeline.program = program;

    program = 0;

    QRHI_RES_RHI(QRhiGles2);
    rhiD->releaseQueue.append(e);
}

bool QGles2GraphicsPipeline::build()
{
    QRHI_RES_RHI(QRhiGles2);

    if (program)
        release();

    program = rhiD->f->glCreateProgram();

    for (const QRhiGraphicsShaderStage &shaderStage : qAsConst(shaderStages)) {
        // ###
    }

    generation += 1;
    return true;
}

QGles2CommandBuffer::QGles2CommandBuffer(QRhiImplementation *rhi)
    : QRhiCommandBuffer(rhi)
{
    resetState();
}

void QGles2CommandBuffer::release()
{
    Q_UNREACHABLE();
}

QGles2SwapChain::QGles2SwapChain(QRhiImplementation *rhi)
    : QRhiSwapChain(rhi),
      rt(rhi),
      cb(rhi)
{
}

void QGles2SwapChain::release()
{
}

QRhiCommandBuffer *QGles2SwapChain::currentFrameCommandBuffer()
{
    return &cb;
}

QRhiRenderTarget *QGles2SwapChain::currentFrameRenderTarget()
{
    return &rt;
}

const QRhiRenderPass *QGles2SwapChain::defaultRenderPass() const
{
    return rt.renderPass();
}

QSize QGles2SwapChain::sizeInPixels() const
{
    return pixelSize;
}

bool QGles2SwapChain::build(QWindow *window, const QSize &pixelSize_, SurfaceImportFlags flags,
                            QRhiRenderBuffer *depthStencil, int sampleCount_)
{
    Q_UNUSED(flags);
    Q_UNUSED(depthStencil);
    Q_UNUSED(sampleCount_);

    surface = window;
    pixelSize = pixelSize_;
    QRHI_RES(QGles2ReferenceRenderTarget, &rt)->d.pixelSize = pixelSize_;

    return true;
}

bool QGles2SwapChain::build(QObject *target)
{
    // ### some day this could support QOpenGLWindow, OpenGLWidget, ...
    Q_UNUSED(target);
    return false;
}

QT_END_NAMESPACE
