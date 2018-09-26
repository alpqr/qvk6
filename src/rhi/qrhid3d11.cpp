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

#include "qrhid3d11_p.h"
#include <QWindow>

QT_BEGIN_NAMESPACE

QRhiD3D11::QRhiD3D11(QRhiInitParams *params)
{
    QRhiD3D11InitParams *d3dparams = static_cast<QRhiD3D11InitParams *>(params);

    create();
}

QRhiD3D11::~QRhiD3D11()
{
    destroy();
}

void QRhiD3D11::create()
{
}

void QRhiD3D11::destroy()
{
}

QVector<int> QRhiD3D11::supportedSampleCounts() const
{
    return { 1 };
}

QRhiSwapChain *QRhiD3D11::createSwapChain()
{
    return nullptr;
}

QRhiBuffer *QRhiD3D11::createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, int size)
{
    return nullptr;
}

int QRhiD3D11::ubufAlignment() const
{
    return 256;
}

QMatrix4x4 QRhiD3D11::openGLVertexCorrectionMatrix() const
{
    return QMatrix4x4(); // identity
}

bool QRhiD3D11::isYUpInFramebuffer() const
{
    return true;
}

QRhiRenderBuffer *QRhiD3D11::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize,
                                                int sampleCount, QRhiRenderBuffer::Hints hints)
{
    return nullptr;
}

QRhiTexture *QRhiD3D11::createTexture(QRhiTexture::Format format, const QSize &pixelSize, QRhiTexture::Flags flags)
{
    return nullptr;
}

QRhiSampler *QRhiD3D11::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                      QRhiSampler::Filter mipmapMode,
                                      QRhiSampler::AddressMode u, QRhiSampler::AddressMode v)
{
    return nullptr;
}

QRhiTextureRenderTarget *QRhiD3D11::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return nullptr;
}

QRhiTextureRenderTarget *QRhiD3D11::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiRenderBuffer *depthStencilBuffer,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return nullptr;
}

QRhiTextureRenderTarget *QRhiD3D11::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiTexture *depthTexture,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return nullptr;
}

QRhiGraphicsPipeline *QRhiD3D11::createGraphicsPipeline()
{
    return nullptr;
}

QRhiShaderResourceBindings *QRhiD3D11::createShaderResourceBindings()
{
    return nullptr;
}

void QRhiD3D11::setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb)
{
}

void QRhiD3D11::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<QRhi::VertexInput> &bindings,
                               QRhiBuffer *indexBuf, quint32 indexOffset, QRhi::IndexFormat indexFormat)
{
}

void QRhiD3D11::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
}

void QRhiD3D11::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
}

void QRhiD3D11::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
}

void QRhiD3D11::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
}

void QRhiD3D11::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                     quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
}

void QRhiD3D11::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                            quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
}

QRhi::FrameOpResult QRhiD3D11::beginFrame(QRhiSwapChain *swapChain)
{
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiD3D11::endFrame(QRhiSwapChain *swapChain)
{
    return QRhi::FrameOpSuccess;
}

void QRhiD3D11::beginPass(QRhiRenderTarget *rt, QRhiCommandBuffer *cb, const QRhiClearValue *clearValues,
                          const QRhi::PassUpdates &updates)
{
}

void QRhiD3D11::endPass(QRhiCommandBuffer *cb)
{
}

QT_END_NAMESPACE
