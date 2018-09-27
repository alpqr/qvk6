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

#ifndef QRHID3D11_P_H
#define QRHID3D11_P_H

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

#include "qrhid3d11.h"
#include "qrhi_p.h"
#include <QShaderDescription>
#include <QWindow>

#include <d3d11.h>
#include <dxgi1_3.h>

QT_BEGIN_NAMESPACE

struct QD3D11Buffer : public QRhiBuffer
{
    QD3D11Buffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    void release() override;
    bool build() override;
};

struct QD3D11RenderBuffer : public QRhiRenderBuffer
{
    QD3D11RenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                       int sampleCount, QRhiRenderBuffer::Hints hints);
    void release() override;
    bool build() override;
};

struct QD3D11Texture : public QRhiTexture
{
    QD3D11Texture(QRhiImplementation *rhi, Format format, const QSize &pixelSize, Flags flags);
    void release() override;
    bool build() override;
};

struct QD3D11Sampler : public QRhiSampler
{
    QD3D11Sampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode, AddressMode u, AddressMode v);
    void release() override;
    bool build() override;
};

struct QD3D11RenderPass : public QRhiRenderPass
{
    QD3D11RenderPass(QRhiImplementation *rhi);
    void release() override;
};

struct QD3D11BasicRenderTargetData
{
    QD3D11BasicRenderTargetData(QRhiImplementation *rhi) : rp(rhi) { }

    QD3D11RenderPass rp;
    QSize pixelSize;
    int attCount;
};

struct QD3D11ReferenceRenderTarget : public QRhiReferenceRenderTarget
{
    QD3D11ReferenceRenderTarget(QRhiImplementation *rhi);
    void release() override;
    Type type() const override;
    QSize sizeInPixels() const override;
    const QRhiRenderPass *renderPass() const override;

    QD3D11BasicRenderTargetData d;
};

struct QD3D11TextureRenderTarget : public QRhiTextureRenderTarget
{
    QD3D11TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, Flags flags);
    QD3D11TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiRenderBuffer *depthStencilBuffer, Flags flags);
    QD3D11TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiTexture *depthTexture, Flags flags);
    void release() override;
    Type type() const override;
    bool build() override;
    QSize sizeInPixels() const override;
    const QRhiRenderPass *renderPass() const override;

    QD3D11BasicRenderTargetData d;
};

struct QD3D11ShaderResourceBindings : public QRhiShaderResourceBindings
{
    QD3D11ShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;
};

struct QD3D11GraphicsPipeline : public QRhiGraphicsPipeline
{
    QD3D11GraphicsPipeline(QRhiImplementation *rhi);
    void release() override;
    bool build() override;
};

struct QD3D11CommandBuffer : public QRhiCommandBuffer
{
    QD3D11CommandBuffer(QRhiImplementation *rhi);
    void release() override;

    QRhiRenderTarget *currentTarget;
    QRhiGraphicsPipeline *currentPipeline;
    uint currentPipelineGeneration;

    void resetState() {
        currentTarget = nullptr;
        currentPipeline = nullptr;
        currentPipelineGeneration = 0;
    }
};

struct QD3D11SwapChain : public QRhiSwapChain
{
    QD3D11SwapChain(QRhiImplementation *rhi);
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;
    const QRhiRenderPass *defaultRenderPass() const override;
    QSize sizeInPixels() const override;

    bool build(QWindow *window, const QSize &pixelSize, SurfaceImportFlags flags,
               QRhiRenderBuffer *depthStencil, int sampleCount) override;

    bool build(QObject *target) override;

    QSurface *surface = nullptr;
    QSize pixelSize;
    QD3D11ReferenceRenderTarget rt;
    QD3D11CommandBuffer cb;
    IDXGISwapChain1 *swapChain = nullptr;
};

class QRhiD3D11 : public QRhiImplementation
{
public:
    QRhiD3D11(QRhiInitParams *params);
    ~QRhiD3D11();

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
                   const QRhi::PassUpdates &updates) override;
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
    QMatrix4x4 openGLVertexCorrectionMatrix() const override;
    bool isYUpInFramebuffer() const override;

    void create();
    void destroy();

    bool debugLayer = false;
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    IDXGIFactory2 *dxgiFactory;
};

QT_END_NAMESPACE

#endif
