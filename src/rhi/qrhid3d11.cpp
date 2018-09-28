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

/*
  Direct3D 11 backend. Provides a double-buffered flip model (FLIP_DISCARD)
  swapchain. Textures and "static" buffers are USAGE_DEFAULT, leaving it to
  UpdateSubResource to upload the data in any way it sees fit. "Dynamic"
  buffers are USAGE_DYNAMIC and updating is done by mapping with WRITE_DISCARD.
  (so here QRhiBuffer keeps a copy of the buffer contents and all of it is
  memcpy'd every time, leaving the rest (juggling with the memory area Map
  returns) to the driver).
*/

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
    Q_ASSERT(!inFrame);
    inFrame = true;

    QD3D11SwapChain *swapChainD = QRHI_RES(QD3D11SwapChain, swapChain);
    swapChainD->cb.resetState();

    currentFrameSlot = swapChainD->currentFrame;

    swapChainD->rt.d.pixelSize = swapChainD->pixelSize;
    swapChainD->rt.d.rp.rtv = swapChainD->rtv[currentFrameSlot];
    swapChainD->rt.d.rp.dsv = swapChainD->ds ? swapChainD->ds->dsv : nullptr;

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiD3D11::endFrame(QRhiSwapChain *swapChain)
{
    Q_ASSERT(inFrame);
    inFrame = false;

    QD3D11SwapChain *swapChainD = QRHI_RES(QD3D11SwapChain, swapChain);
    executeCommandBuffer(&swapChainD->cb);

    const UINT swapInterval = 1;
    const UINT presentFlags = 0;
    HRESULT hr = swapChainD->swapChain->Present(swapInterval, presentFlags);
    if (FAILED(hr))
        qWarning("Failed to present: %s", qPrintable(comErrorMessage(hr)));

    swapChainD->currentFrame = (swapChainD->currentFrame + 1) % FRAMES_IN_FLIGHT;

    ++finishedFrameCount;
    return QRhi::FrameOpSuccess;
}

void QRhiD3D11::beginPass(QRhiRenderTarget *rt, QRhiCommandBuffer *cb, const QRhiClearValue *clearValues,
                          const QRhi::PassUpdates &updates)
{
    Q_ASSERT(!inPass);

    inPass = true;

    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    bool needsColorClear = true;
    QD3D11BasicRenderTargetData *rtD = nullptr;
    switch (rt->type()) {
    case QRhiRenderTarget::RtRef:
        rtD = &QRHI_RES(QD3D11ReferenceRenderTarget, rt)->d;
        break;
    case QRhiRenderTarget::RtTexture:
    {
        QD3D11TextureRenderTarget *rtTex = QRHI_RES(QD3D11TextureRenderTarget, rt);
        rtD = &rtTex->d;
        needsColorClear = !rtTex->flags.testFlag(QRhiTextureRenderTarget::PreserveColorContents);
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    cbD->currentTarget = rt;

    QD3D11CommandBuffer::Command fbCmd;
    fbCmd.cmd = QD3D11CommandBuffer::Command::SetRenderTarget;
    fbCmd.args.setRenderTarget.rt = rt;
    cbD->commands.append(fbCmd);

    Q_ASSERT(rtD->attCount == 1 || rtD->attCount == 2);
    QD3D11CommandBuffer::Command clearCmd;
    clearCmd.cmd = QD3D11CommandBuffer::Command::Clear;
    clearCmd.args.clear.rt = rt;
    clearCmd.args.clear.mask = 0;
    if (rtD->attCount > 1)
        clearCmd.args.clear.mask |= QD3D11CommandBuffer::Command::Depth | QD3D11CommandBuffer::Command::Stencil;
    if (needsColorClear)
        clearCmd.args.clear.mask |= QD3D11CommandBuffer::Command::Color;
    for (int i = 0; i < rtD->attCount; ++i) {
        if (clearValues[i].isDepthStencil) {
            clearCmd.args.clear.d = clearValues[i].d;
            clearCmd.args.clear.s = clearValues[i].s;
        } else {
            memcpy(clearCmd.args.clear.c, &clearValues[i].rgba, sizeof(float) * 4);
        }
    }
    cbD->commands.append(clearCmd);
}

void QRhiD3D11::endPass(QRhiCommandBuffer *cb)
{
    Q_ASSERT(inPass);

    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    cbD->currentTarget = nullptr;

    inPass = false;
}

void QRhiD3D11::executeCommandBuffer(QD3D11CommandBuffer *cb)
{
    for (const QD3D11CommandBuffer::Command &cmd : qAsConst(cb->commands)) {
        switch (cmd.cmd) {
        case QD3D11CommandBuffer::Command::SetRenderTarget:
        {
            QRhiRenderTarget *rt = cmd.args.setRenderTarget.rt;
            const QD3D11RenderPass *rp = QRHI_RES(const QD3D11RenderPass, rt->renderPass());
            Q_ASSERT(rp);
            context->OMSetRenderTargets(rp->rtv ? 1 : 0, rp->rtv ? &rp->rtv : nullptr, rp->dsv);
        }
            break;
        case QD3D11CommandBuffer::Command::Clear:
        {
            const QD3D11RenderPass *rp = QRHI_RES(const QD3D11RenderPass, cmd.args.clear.rt->renderPass());
            if (cmd.args.clear.mask & QD3D11CommandBuffer::Command::Color)
                context->ClearRenderTargetView(rp->rtv, cmd.args.clear.c);
            uint ds = 0;
            if (cmd.args.clear.mask & QD3D11CommandBuffer::Command::Depth)
                ds |= D3D11_CLEAR_DEPTH;
            if (cmd.args.clear.mask & QD3D11CommandBuffer::Command::Stencil)
                ds |= D3D11_CLEAR_STENCIL;
            if (ds)
                context->ClearDepthStencilView(rp->dsv, ds, cmd.args.clear.d, cmd.args.clear.s);
        }
            break;
        default:
            break;
        }
    }
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
    if (!tex)
        return;

    if (dsv) {
        dsv->Release();
        dsv = nullptr;
    }

    if (tex) {
        tex->Release();
        tex = nullptr;
    }
}

bool QD3D11RenderBuffer::build()
{
    if (tex)
        release();

    QRHI_RES_RHI(QRhiD3D11);
    static const DXGI_FORMAT dsFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    if (type == DepthStencil) {
        D3D11_TEXTURE2D_DESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.Width = pixelSize.width();
        desc.Height = pixelSize.height();
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = dsFormat;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        HRESULT hr = rhiD->dev->CreateTexture2D(&desc, nullptr, &tex);
        if (FAILED(hr)) {
            qWarning("Failed to create depth-stencil buffer: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        memset(&dsvDesc, 0, sizeof(dsvDesc));
        dsvDesc.Format = dsFormat;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = rhiD->dev->CreateDepthStencilView(tex, &dsvDesc, &dsv);
        if (FAILED(hr)) {
            qWarning("Failed to create dsv: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
        return true;
    }

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
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        tex[i] = nullptr;
        rtv[i] = nullptr;
    }
}

void QD3D11SwapChain::release()
{
    if (!swapChain)
        return;

    for (int i = 0; i < BUFFER_COUNT; ++i) {
        if (rtv[i]) {
            rtv[i]->Release();
            rtv[i] = nullptr;
        }
        if (tex[i]) {
            tex[i]->Release();
            tex[i] = nullptr;
        }
    }

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

bool QD3D11SwapChain::build(QWindow *window_, const QSize &pixelSize_, SurfaceImportFlags flags,
                            QRhiRenderBuffer *depthStencil, int sampleCount)
{
    // Can be called multiple times without a call to release() - this is typical when a window is resized.
    Q_ASSERT(!swapChain || window == window_);

    Q_UNUSED(sampleCount); // ### MSAA

    window = window_;
    pixelSize = pixelSize_;

    QRHI_RES_RHI(QRhiD3D11);

    const DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    const UINT swapChainFlags = 0;

    if (!swapChain) {
        HWND hwnd = reinterpret_cast<HWND>(window->winId());

        // We use FLIP_DISCARD which implies a buffer count of 2 (as opposed to the
        // old DISCARD with back buffer count == 1). This makes no difference for
        // the rest of the stuff except that automatic MSAA is unsupported and
        // needs to be implemented via a custom multisample render target and an
        // explicit resolve.
        DXGI_SWAP_CHAIN_DESC1 desc;
        memset(&desc, 0, sizeof(desc));
        desc.Width = pixelSize.width();
        desc.Height = pixelSize.height();
        desc.Format = colorFormat;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = BUFFER_COUNT;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (flags.testFlag(SurfaceHasPreMulAlpha))
            desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        else if (flags.testFlag(SurfaceHasNonPreMulAlpha))
            desc.AlphaMode = DXGI_ALPHA_MODE_STRAIGHT;
        desc.Flags = swapChainFlags;

        HRESULT hr = rhiD->dxgiFactory->CreateSwapChainForHwnd(rhiD->dev, hwnd, &desc, nullptr, nullptr, &swapChain);
        if (FAILED(hr)) {
            qWarning("Failed to create D3D11 swapchain: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
    } else {
        for (int i = 0; i < BUFFER_COUNT; ++i) {
            if (rtv[i]) {
                rtv[i]->Release();
                rtv[i] = nullptr;
            }
            if (tex[i]) {
                tex[i]->Release();
                tex[i] = nullptr;
            }
        }
        HRESULT hr = swapChain->ResizeBuffers(2, pixelSize.width(), pixelSize.height(), colorFormat, swapChainFlags);
        if (FAILED(hr)) {
            qWarning("Failed to resize D3D11 swapchain: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
    }

    for (int i = 0; i < BUFFER_COUNT; ++i) {
        HRESULT hr = swapChain->GetBuffer(0, IID_ID3D11Texture2D, reinterpret_cast<void **>(&tex[i]));
        if (FAILED(hr)) {
            qWarning("Failed to query swapchain buffer %d: %s", i, qPrintable(comErrorMessage(hr)));
            return false;
        }
        hr = rhiD->dev->CreateRenderTargetView(tex[i], nullptr, &rtv[i]);
        if (FAILED(hr)) {
            qWarning("Failed to create rtv for swapchain buffer %d: %s", i, qPrintable(comErrorMessage(hr)));
            return false;
        }
    }

    currentFrame = 0;
    ds = depthStencil ? QRHI_RES(QD3D11RenderBuffer, depthStencil) : nullptr;

    QD3D11ReferenceRenderTarget *rtD = QRHI_RES(QD3D11ReferenceRenderTarget, &rt);
    rtD->d.pixelSize = pixelSize_;
    rtD->d.attCount = depthStencil ? 2 : 1;

    return true;
}

bool QD3D11SwapChain::build(QObject *target)
{
    Q_UNUSED(target);
    return false;
}

QT_END_NAMESPACE
