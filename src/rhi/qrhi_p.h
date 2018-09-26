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

#ifndef QRHI_P_H
#define QRHI_P_H

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

#include "qtrhiglobal_p.h"
#include "qrhi.h"

QT_BEGIN_NAMESPACE

#define QRHI_RES(t, x) static_cast<t *>(x)
#define QRHI_RES_RHI(t) t *rhiD = static_cast<t *>(rhi)

class QRhiImplementation
{
public:
    virtual ~QRhiImplementation();

    virtual QRhiGraphicsPipeline *createGraphicsPipeline() = 0;
    virtual QRhiShaderResourceBindings *createShaderResourceBindings() = 0;
    virtual QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                                     QRhiBuffer::UsageFlags usage,
                                     int size) = 0;
    virtual QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                                 const QSize &pixelSize,
                                                 int sampleCount,
                                                 QRhiRenderBuffer::Hints hints) = 0;
    virtual QRhiTexture *createTexture(QRhiTexture::Format format,
                                       const QSize &pixelSize,
                                       QRhiTexture::Flags flags) = 0;
    virtual QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                       QRhiSampler::Filter mipmapMode,
                                       QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v) = 0;

    virtual QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                               QRhiTextureRenderTarget::Flags flags) = 0;
    virtual QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                               QRhiRenderBuffer *depthStencilBuffer,
                                                               QRhiTextureRenderTarget::Flags flags) = 0;
    virtual QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                               QRhiTexture *depthTexture,
                                                               QRhiTextureRenderTarget::Flags flags) = 0;

    virtual QRhiSwapChain *createSwapChain() = 0;
    virtual QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain) = 0;
    virtual QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain) = 0;

    virtual void beginPass(QRhiRenderTarget *rt,
                           QRhiCommandBuffer *cb,
                           const QRhiClearValue *clearValues,
                           const QRhi::PassUpdates &updates) = 0;
    virtual void endPass(QRhiCommandBuffer *cb) = 0;

    virtual void setGraphicsPipeline(QRhiCommandBuffer *cb,
                                     QRhiGraphicsPipeline *ps,
                                     QRhiShaderResourceBindings *srb = nullptr) = 0;

    virtual void setVertexInput(QRhiCommandBuffer *cb,
                                int startBinding, const QVector<QRhi::VertexInput> &bindings,
                                QRhiBuffer *indexBuf, quint32 indexOffset,
                                QRhi::IndexFormat indexFormat) = 0;

    virtual void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport) = 0;
    virtual void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor) = 0;
    virtual void setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c) = 0;
    virtual void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue) = 0;

    virtual void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                      quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) = 0;
    virtual void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                             quint32 instanceCount, quint32 firstIndex,
                             qint32 vertexOffset, quint32 firstInstance) = 0;

    virtual QVector<int> supportedSampleCounts() const = 0;
    virtual int ubufAlignment() const = 0;
    virtual QMatrix4x4 openGLVertexCorrectionMatrix() const = 0;
    virtual bool isYUpInFramebuffer() const = 0;
};

QT_END_NAMESPACE

#endif
