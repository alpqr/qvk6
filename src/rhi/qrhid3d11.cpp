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

#include <comdef.h>

QT_BEGIN_NAMESPACE

QRhiD3D11::QRhiD3D11(QRhiInitParams *params)
{
    QRhiD3D11InitParams *d3dparams = static_cast<QRhiD3D11InitParams *>(params);
    debugLayer = d3dparams->enableDebugLayer;

    create();
}

QRhiD3D11::~QRhiD3D11()
{
    destroy();
}

static QString comErrorMessage(HRESULT hr)
{
#ifndef Q_OS_WINRT
    const _com_error comError(hr);
#else
    const _com_error comError(hr, nullptr);
#endif
    QString result = QLatin1String("Error 0x") + QString::number(ulong(hr), 16);
    if (const wchar_t *msg = comError.ErrorMessage())
        result += QLatin1String(": ") + QString::fromWCharArray(msg);
    return result;
}

void QRhiD3D11::create()
{
    uint flags = 0;
    if (debugLayer)
        flags |= D3D11_CREATE_DEVICE_DEBUG;

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   nullptr, 0, D3D11_SDK_VERSION,
                                   &dev, &featureLevel, &context);
    if (FAILED(hr)) {
        qWarning("Failed to create D3D11 device and context: %s", qPrintable(comErrorMessage(hr)));
        return;
    }

    hr = CreateDXGIFactory2(0, IID_IDXGIFactory2, reinterpret_cast<void **>(&dxgiFactory));
    if (FAILED(hr)) {
        qWarning("Failed to create DXGI factory: %s", qPrintable(comErrorMessage(hr)));
        return;
    }
}

void QRhiD3D11::destroy()
{
    if (!dev)
        return;

    context->Release();
    context = nullptr;

    dev->Release();
    dev = nullptr;
}

QVector<int> QRhiD3D11::supportedSampleCounts() const
{
    return { 1 };
}

QRhiSwapChain *QRhiD3D11::createSwapChain()
{
    return new QD3D11SwapChain(this);
}

QRhiBuffer *QRhiD3D11::createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, int size)
{
    return new QD3D11Buffer(this, type, usage, size);
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
    return new QD3D11RenderBuffer(this, type, pixelSize, sampleCount, hints);
}

QRhiTexture *QRhiD3D11::createTexture(QRhiTexture::Format format, const QSize &pixelSize, QRhiTexture::Flags flags)
{
    return new QD3D11Texture(this, format, pixelSize, flags);
}

QRhiSampler *QRhiD3D11::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                      QRhiSampler::Filter mipmapMode,
                                      QRhiSampler::AddressMode u, QRhiSampler::AddressMode v)
{
    return new QD3D11Sampler(this, magFilter, minFilter, mipmapMode, u, v);
}

QRhiTextureRenderTarget *QRhiD3D11::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QD3D11TextureRenderTarget(this, texture, flags);
}

QRhiTextureRenderTarget *QRhiD3D11::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiRenderBuffer *depthStencilBuffer,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QD3D11TextureRenderTarget(this, texture, depthStencilBuffer, flags);
}

QRhiTextureRenderTarget *QRhiD3D11::createTextureRenderTarget(QRhiTexture *texture,
                                                              QRhiTexture *depthTexture,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QD3D11TextureRenderTarget(this, texture, depthTexture, flags);
}

QRhiGraphicsPipeline *QRhiD3D11::createGraphicsPipeline()
{
    return new QD3D11GraphicsPipeline(this);
}

QRhiShaderResourceBindings *QRhiD3D11::createShaderResourceBindings()
{
    return new QD3D11ShaderResourceBindings(this);
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

QD3D11Buffer::QD3D11Buffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size)
    : QRhiBuffer(rhi, type, usage, size)
{
}

void QD3D11Buffer::release()
{
}

bool QD3D11Buffer::build()
{
    return false;
}

QD3D11RenderBuffer::QD3D11RenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                                       int sampleCount, QRhiRenderBuffer::Hints hints)
    : QRhiRenderBuffer(rhi, type, pixelSize, sampleCount, hints)
{
}

void QD3D11RenderBuffer::release()
{
}

bool QD3D11RenderBuffer::build()
{
    return false;
}

QD3D11Texture::QD3D11Texture(QRhiImplementation *rhi, Format format, const QSize &pixelSize, Flags flags)
    : QRhiTexture(rhi, format, pixelSize, flags)
{
}

void QD3D11Texture::release()
{
}

bool QD3D11Texture::build()
{
    return false;
}

QD3D11Sampler::QD3D11Sampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode, AddressMode u, AddressMode v)
    : QRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v)
{
}

void QD3D11Sampler::release()
{
}

bool QD3D11Sampler::build()
{
    return false;
}

QD3D11RenderPass::QD3D11RenderPass(QRhiImplementation *rhi)
    : QRhiRenderPass(rhi)
{
}

void QD3D11RenderPass::release()
{
}

QD3D11ReferenceRenderTarget::QD3D11ReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiReferenceRenderTarget(rhi),
      d(rhi)
{
}

void QD3D11ReferenceRenderTarget::release()
{
}

QRhiRenderTarget::Type QD3D11ReferenceRenderTarget::type() const
{
    return RtRef;
}

QSize QD3D11ReferenceRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

const QRhiRenderPass *QD3D11ReferenceRenderTarget::renderPass() const
{
    return &d.rp;
}

QD3D11TextureRenderTarget::QD3D11TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, Flags flags)
    : QRhiTextureRenderTarget(rhi, texture, flags),
      d(rhi)
{
}

QD3D11TextureRenderTarget::QD3D11TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiRenderBuffer *depthStencilBuffer, Flags flags)
    : QRhiTextureRenderTarget(rhi, texture, depthStencilBuffer, flags),
      d(rhi)
{
}

QD3D11TextureRenderTarget::QD3D11TextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiTexture *depthTexture, Flags flags)
    : QRhiTextureRenderTarget(rhi, texture, depthTexture, flags),
      d(rhi)
{
}

void QD3D11TextureRenderTarget::release()
{
}

bool QD3D11TextureRenderTarget::build()
{
    return false;
}

QRhiRenderTarget::Type QD3D11TextureRenderTarget::type() const
{
    return RtTexture;
}

QSize QD3D11TextureRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

const QRhiRenderPass *QD3D11TextureRenderTarget::renderPass() const
{
    return &d.rp;
}

QD3D11ShaderResourceBindings::QD3D11ShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiShaderResourceBindings(rhi)
{
}

void QD3D11ShaderResourceBindings::release()
{
}

bool QD3D11ShaderResourceBindings::build()
{
    return false;
}

QD3D11GraphicsPipeline::QD3D11GraphicsPipeline(QRhiImplementation *rhi)
    : QRhiGraphicsPipeline(rhi)
{
}

void QD3D11GraphicsPipeline::release()
{
}

bool QD3D11GraphicsPipeline::build()
{
    return false;
}

QD3D11CommandBuffer::QD3D11CommandBuffer(QRhiImplementation *rhi)
    : QRhiCommandBuffer(rhi)
{
    resetState();
}

void QD3D11CommandBuffer::release()
{
    Q_UNREACHABLE();
}

QD3D11SwapChain::QD3D11SwapChain(QRhiImplementation *rhi)
    : QRhiSwapChain(rhi),
      rt(rhi),
      cb(rhi)
{
}

void QD3D11SwapChain::release()
{
    if (!swapChain)
        return;

    swapChain->Release();
    swapChain = nullptr;
}

QRhiCommandBuffer *QD3D11SwapChain::currentFrameCommandBuffer()
{
    return &cb;
}

QRhiRenderTarget *QD3D11SwapChain::currentFrameRenderTarget()
{
    return &rt;
}

const QRhiRenderPass *QD3D11SwapChain::defaultRenderPass() const
{
    return rt.renderPass();
}

QSize QD3D11SwapChain::sizeInPixels() const
{
    return pixelSize;
}

bool QD3D11SwapChain::build(QWindow *window, const QSize &pixelSize_, SurfaceImportFlags flags,
                            QRhiRenderBuffer *depthStencil, int sampleCount_)
{
    // Can be called multiple times without a call to release()
    // - this is typical when a window is resized.

    Q_UNUSED(flags);
    Q_UNUSED(depthStencil);
    Q_UNUSED(sampleCount_);

    surface = window;
    pixelSize = pixelSize_;

    QD3D11ReferenceRenderTarget *rtD = QRHI_RES(QD3D11ReferenceRenderTarget, &rt);
    rtD->d.pixelSize = pixelSize_;
    rtD->d.attCount = depthStencil ? 2 : 1;

    // We use FLIP_DISCARD which implies a buffer count of 2 (as opposed to the
    // old DISCARD with back buffer count == 1). This makes no difference for
    // the rest of the stuff except that automatic MSAA is unsupported and
    // needs to be implemented via a custom multisample render target and an
    // explicit resolve.

    QRHI_RES_RHI(QRhiD3D11);
    HWND hwnd = reinterpret_cast<HWND>(window->winId());

    DXGI_SWAP_CHAIN_DESC1 desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = pixelSize.width();
    desc.Height = pixelSize.height();
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    HRESULT hr = rhiD->dxgiFactory->CreateSwapChainForHwnd(rhiD->dev, hwnd, &desc, nullptr, nullptr, &swapChain);
    if (FAILED(hr)) {
        qWarning("Failed to create D3D11 swapchain: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    return true;
}

bool QD3D11SwapChain::build(QObject *target)
{
    Q_UNUSED(target);
    return false;
}

QT_END_NAMESPACE
