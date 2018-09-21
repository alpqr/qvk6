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

    create();
}

QRhiGles2::~QRhiGles2()
{
    destroy();
}

void QRhiGles2::create()
{
    Q_ASSERT(ctx);

    f = ctx->functions();
}

void QRhiGles2::destroy()
{
    if (!f)
        return;

    f = nullptr;
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

void QRhiGles2::setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb)
{
    Q_ASSERT(inPass);
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

QRhi::FrameOpResult QRhiGles2::beginFrame(QRhiSwapChain *swapChain)
{
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiGles2::endFrame(QRhiSwapChain *swapChain)
{
    QGles2SwapChain *swapChainD = QRHI_RES(QGles2SwapChain, swapChain);
    if (swapChainD->surface)
        ctx->swapBuffers(swapChainD->surface);

    return QRhi::FrameOpSuccess;
}

void QRhiGles2::beginPass(QRhiRenderTarget *rt, QRhiCommandBuffer *cb, const QRhiClearValue *clearValues, const QRhi::PassUpdates &updates)
{
    Q_ASSERT(!inPass);

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
        //activateTextureRenderTarget(cb, rtTex);
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

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
}

QGles2Buffer::QGles2Buffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size)
    : QRhiBuffer(rhi, type, usage, size)
{
}

void QGles2Buffer::release()
{
}

bool QGles2Buffer::build()
{
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
    return RtRef; // no Gles2* are owned directly by the object
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
    return RtTexture; // this is a QGles2TextureRenderTarget, owns
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
    return true;
}

QGles2GraphicsPipeline::QGles2GraphicsPipeline(QRhiImplementation *rhi)
    : QRhiGraphicsPipeline(rhi)
{
}

void QGles2GraphicsPipeline::release()
{
}

bool QGles2GraphicsPipeline::build()
{
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
      rtWrapper(rhi),
      cbWrapper(rhi)
{
}

void QGles2SwapChain::release()
{
}

QRhiCommandBuffer *QGles2SwapChain::currentFrameCommandBuffer()
{
    return &cbWrapper;
}

QRhiRenderTarget *QGles2SwapChain::currentFrameRenderTarget()
{
    return &rtWrapper;
}

const QRhiRenderPass *QGles2SwapChain::defaultRenderPass() const
{
    return rtWrapper.renderPass();
}

QSize QGles2SwapChain::sizeInPixels() const
{
    return pixelSize;
}

bool QGles2SwapChain::build(QWindow *window, const QSize &pixelSize_, SurfaceImportFlags flags,
                            QRhiRenderBuffer *depthStencil, int sampleCount_)
{
    surface = window;
    pixelSize = pixelSize_;

    return true;
}

bool QGles2SwapChain::build(QObject *target)
{
    // some day this could support QOpenGLWindow, OpenGLWidget, ...
    Q_UNUSED(target);
    return false;
}

QT_END_NAMESPACE
