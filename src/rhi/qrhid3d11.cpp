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
#include <QBakedShader>
#include <qmath.h>

#include <d3dcompiler.h>
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
    : ofr(this)
{
    QRhiD3D11InitParams *d3dparams = static_cast<QRhiD3D11InitParams *>(params);
    debugLayer = d3dparams->enableDebugLayer;
    importedDevice = d3dparams->importExistingDevice;
    if (importedDevice) {
        dev = reinterpret_cast<ID3D11Device *>(d3dparams->dev);
        ID3D11DeviceContext *ctx = reinterpret_cast<ID3D11DeviceContext *>(d3dparams->context);
        if (SUCCEEDED(ctx->QueryInterface(IID_ID3D11DeviceContext1, reinterpret_cast<void **>(&context)))) {
            // get rid of the ref added by QueryInterface
            ctx->Release();
        } else {
            qWarning("ID3D11DeviceContext1 not supported by context, cannot import");
            importedDevice = false;
        }
    }

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

static inline uint aligned(uint v, uint byteAlign)
{
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

void QRhiD3D11::create()
{
    uint flags = 0;
    if (debugLayer)
        flags |= D3D11_CREATE_DEVICE_DEBUG;

    HRESULT hr = CreateDXGIFactory2(0, IID_IDXGIFactory2, reinterpret_cast<void **>(&dxgiFactory));
    if (FAILED(hr)) {
        qWarning("Failed to create DXGI factory: %s", qPrintable(comErrorMessage(hr)));
        return;
    }

    if (!importedDevice) {
        IDXGIAdapter1 *adapterToUse = nullptr;
        IDXGIAdapter1 *adapter;
        int requestedAdapterIndex = -1;
        if (qEnvironmentVariableIsSet("QT_D3D_ADAPTER_INDEX"))
            requestedAdapterIndex = qEnvironmentVariableIntValue("QT_D3D_ADAPTER_INDEX");
        for (int adapterIndex = 0; dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            const QString name = QString::fromUtf16((char16_t *) desc.Description);
            qDebug("Adapter %d: '%s' (flags 0x%x)", adapterIndex, qPrintable(name), desc.Flags);
            if (!adapterToUse && (requestedAdapterIndex < 0 || requestedAdapterIndex == adapterIndex)) {
                adapterToUse = adapter;
                qDebug("  using this adapter");
            } else {
                adapter->Release();
            }
        }
        if (!adapterToUse) {
            qWarning("No adapter");
            return;
        }

        ID3D11DeviceContext *ctx = nullptr;
        HRESULT hr = D3D11CreateDevice(adapterToUse, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
                                       nullptr, 0, D3D11_SDK_VERSION,
                                       &dev, &featureLevel, &ctx);
        adapterToUse->Release();
        if (FAILED(hr)) {
            qWarning("Failed to create D3D11 device and context: %s", qPrintable(comErrorMessage(hr)));
            return;
        }
        if (SUCCEEDED(ctx->QueryInterface(IID_ID3D11DeviceContext1, reinterpret_cast<void **>(&context)))) {
            ctx->Release();
        } else {
            qWarning("ID3D11DeviceContext1 not supported");
            return;
        }
    } else {
        Q_ASSERT(dev && context);
        featureLevel = dev->GetFeatureLevel();
    }
}

void QRhiD3D11::destroy()
{
    finishActiveReadbacks();

    if (!importedDevice) {
        if (context) {
            context->Release();
            context = nullptr;
        }
        if (dev) {
            dev->Release();
            dev = nullptr;
        }
    }

    if (dxgiFactory) {
        dxgiFactory->Release();
        dxgiFactory = nullptr;
    }
}

QVector<int> QRhiD3D11::supportedSampleCounts() const
{
    return { 1, 2, 4, 8 };
}

DXGI_SAMPLE_DESC QRhiD3D11::effectiveSampleCount(int sampleCount) const
{
    DXGI_SAMPLE_DESC desc;
    desc.Count = 1;
    desc.Quality = 0;

    // Stay compatible with QSurfaceFormat and friends where samples == 0 means the same as 1.
    int s = qBound(1, sampleCount, 64);

    if (!supportedSampleCounts().contains(s)) {
        qWarning("Attempted to set unsupported sample count %d", sampleCount);
        return desc;
    }

    desc.Count = s;
    desc.Quality = s > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
    return desc;
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

bool QRhiD3D11::isYUpInFramebuffer() const
{
    return false;
}

QMatrix4x4 QRhiD3D11::clipSpaceCorrMatrix() const
{
    // Like with Vulkan, but Y is already good.

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

bool QRhiD3D11::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const
{
    Q_UNUSED(flags);

    if (format >= QRhiTexture::ETC2_RGB8 && format <= QRhiTexture::ASTC_12x12)
        return false;

    return true;
}

bool QRhiD3D11::isFeatureSupported(QRhi::Feature feature) const
{
    switch (feature) {
    case QRhi::MultisampleTexture:
        Q_FALLTHROUGH();
    case QRhi::MultisampleRenderBuffer:
        return true;
    default:
        Q_UNREACHABLE();
        return false;
    }
}

QRhiRenderBuffer *QRhiD3D11::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize,
                                                int sampleCount, QRhiRenderBuffer::Flags flags)
{
    return new QD3D11RenderBuffer(this, type, pixelSize, sampleCount, flags);
}

QRhiTexture *QRhiD3D11::createTexture(QRhiTexture::Format format, const QSize &pixelSize,
                                      int sampleCount, QRhiTexture::Flags flags)
{
    return new QD3D11Texture(this, format, pixelSize, sampleCount, flags);
}

QRhiSampler *QRhiD3D11::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                      QRhiSampler::Filter mipmapMode,
                                      QRhiSampler::AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w)
{
    return new QD3D11Sampler(this, magFilter, minFilter, mipmapMode, u, v, w);
}

QRhiTextureRenderTarget *QRhiD3D11::createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                              QRhiTextureRenderTarget::Flags flags)
{
    return new QD3D11TextureRenderTarget(this, desc, flags);
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
    Q_ASSERT(inPass);

    QD3D11GraphicsPipeline *psD = QRHI_RES(QD3D11GraphicsPipeline, ps);
    if (!srb)
        srb = psD->m_shaderResourceBindings;

    QD3D11ShaderResourceBindings *srbD = QRHI_RES(QD3D11ShaderResourceBindings, srb);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);

    bool resChanged = false;
    for (int i = 0, ie = srbD->sortedBindings.count(); i != ie; ++i) {
        const QRhiShaderResourceBinding &b(srbD->sortedBindings[i]);
        QD3D11ShaderResourceBindings::BoundResourceData &bd(srbD->boundResourceData[i]);
        switch (b.type) {
        case QRhiShaderResourceBinding::UniformBuffer:
            if (QRHI_RES(QD3D11Buffer, b.ubuf.buf)->generation != bd.ubuf.generation) {
                resChanged = true;
                bd.ubuf.generation = QRHI_RES(QD3D11Buffer, b.ubuf.buf)->generation;
            }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
            if (QRHI_RES(QD3D11Texture, b.stex.tex)->generation != bd.stex.texGeneration
                    || QRHI_RES(QD3D11Sampler, b.stex.sampler)->generation != bd.stex.samplerGeneration)
            {
                resChanged = true;
                bd.stex.texGeneration = QRHI_RES(QD3D11Texture, b.stex.tex)->generation;
                bd.stex.samplerGeneration = QRHI_RES(QD3D11Sampler, b.stex.sampler)->generation;
            }
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
    if (resChanged)
        updateShaderResourceBindings(srbD);

    const bool pipelineChanged = cbD->currentPipeline != ps || cbD->currentPipelineGeneration != psD->generation;
    const bool srbChanged = cbD->currentSrb != srb || cbD->currentSrbGeneration != srbD->generation;

    if (pipelineChanged || srbChanged || resChanged) {
        cbD->currentPipeline = ps;
        cbD->currentPipelineGeneration = psD->generation;
        cbD->currentSrb = srb;
        cbD->currentSrbGeneration = srbD->generation;

        QD3D11CommandBuffer::Command cmd;
        cmd.cmd = QD3D11CommandBuffer::Command::BindGraphicsPipeline;
        cmd.args.bindGraphicsPipeline.ps = psD;
        cmd.args.bindGraphicsPipeline.srb = srbD;
        cmd.args.bindGraphicsPipeline.resOnlyChange = !pipelineChanged && !srbChanged && resChanged;
        cbD->commands.append(cmd);
    }
}

void QRhiD3D11::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<QRhiCommandBuffer::VertexInput> &bindings,
                               QRhiBuffer *indexBuf, quint32 indexOffset, QRhiCommandBuffer::IndexFormat indexFormat)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);

    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::BindVertexBuffers;
    cmd.args.bindVertexBuffers.startSlot = startBinding;
    cmd.args.bindVertexBuffers.slotCount = bindings.count();
    for (int i = 0, ie = bindings.count(); i != ie; ++i) {
        QRhiBuffer *buf = bindings[i].first;
        quint32 ofs = bindings[i].second;
        QD3D11Buffer *bufD = QRHI_RES(QD3D11Buffer, buf);
        Q_ASSERT(bufD->m_usage.testFlag(QRhiBuffer::VertexBuffer));
        cmd.args.bindVertexBuffers.buffers[i] = bufD->buffer;
        cmd.args.bindVertexBuffers.offsets[i] = ofs;
        cmd.args.bindVertexBuffers.strides[i] =
                QRHI_RES(QD3D11GraphicsPipeline, cbD->currentPipeline)->m_vertexInputLayout.bindings[i].stride;
        if (bufD->m_type == QRhiBuffer::Dynamic)
            executeBufferHostWritesForCurrentFrame(bufD);
    }
    cbD->commands.append(cmd);

    if (indexBuf) {
        QD3D11Buffer *ibufD = QRHI_RES(QD3D11Buffer, indexBuf);
        Q_ASSERT(ibufD->m_usage.testFlag(QRhiBuffer::IndexBuffer));
        QD3D11CommandBuffer::Command cmd;
        cmd.cmd = QD3D11CommandBuffer::Command::BindIndexBuffer;
        cmd.args.bindIndexBuffer.buffer = ibufD->buffer;
        cmd.args.bindIndexBuffer.offset = indexOffset;
        cmd.args.bindIndexBuffer.format = indexFormat == QRhiCommandBuffer::IndexUInt16 ? DXGI_FORMAT_R16_UINT
                                                                                        : DXGI_FORMAT_R32_UINT;
        cbD->commands.append(cmd);
        if (ibufD->m_type == QRhiBuffer::Dynamic)
            executeBufferHostWritesForCurrentFrame(ibufD);
    }
}

void QRhiD3D11::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    Q_ASSERT(cbD->currentTarget);
    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::Viewport;
    cmd.args.viewport.x = viewport.r.x();
    // d3d expects top-left, QRhiViewport is bottom-left
    cmd.args.viewport.y = cbD->currentTarget->sizeInPixels().height() - (viewport.r.y() + viewport.r.w() - 1);
    cmd.args.viewport.y = viewport.r.y();
    cmd.args.viewport.w = viewport.r.z();
    cmd.args.viewport.h = viewport.r.w();
    cmd.args.viewport.d0 = viewport.minDepth;
    cmd.args.viewport.d1 = viewport.maxDepth;
    cbD->commands.append(cmd);
}

void QRhiD3D11::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    Q_ASSERT(cbD->currentTarget);
    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::Scissor;
    cmd.args.scissor.x = scissor.r.x();
    // d3d expects top-left, QRhiScissor is bottom-left
    cmd.args.scissor.y = cbD->currentTarget->sizeInPixels().height() - (scissor.r.y() + scissor.r.w() - 1);
    cmd.args.scissor.w = scissor.r.z();
    cmd.args.scissor.h = scissor.r.w();
    cbD->commands.append(cmd);
}

void QRhiD3D11::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::BlendConstants;
    cmd.args.blendConstants.ps = QRHI_RES(QD3D11GraphicsPipeline, cbD->currentPipeline);
    memcpy(cmd.args.blendConstants.c, &c, 4 * sizeof(float));
    cbD->commands.append(cmd);
}

void QRhiD3D11::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::StencilRef;
    cmd.args.stencilRef.ps = QRHI_RES(QD3D11GraphicsPipeline, cbD->currentPipeline);
    cmd.args.stencilRef.ref = refValue;
    cbD->commands.append(cmd);
}

void QRhiD3D11::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                     quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::Draw;
    cmd.args.draw.ps = QRHI_RES(QD3D11GraphicsPipeline, cbD->currentPipeline);
    cmd.args.draw.vertexCount = vertexCount;
    cmd.args.draw.instanceCount = instanceCount;
    cmd.args.draw.firstVertex = firstVertex;
    cmd.args.draw.firstInstance = firstInstance;
    cbD->commands.append(cmd);
}

void QRhiD3D11::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                            quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::DrawIndexed;
    cmd.args.drawIndexed.ps = QRHI_RES(QD3D11GraphicsPipeline, cbD->currentPipeline);
    cmd.args.drawIndexed.indexCount = indexCount;
    cmd.args.drawIndexed.instanceCount = instanceCount;
    cmd.args.drawIndexed.firstIndex = firstIndex;
    cmd.args.drawIndexed.vertexOffset = vertexOffset;
    cmd.args.drawIndexed.firstInstance = firstInstance;
    cbD->commands.append(cmd);
}

QRhi::FrameOpResult QRhiD3D11::beginFrame(QRhiSwapChain *swapChain)
{
    Q_ASSERT(!inFrame);
    inFrame = true;

    QD3D11SwapChain *swapChainD = QRHI_RES(QD3D11SwapChain, swapChain);
    contextState.currentSwapChain = swapChainD;
    swapChainD->cb.resetState();

    swapChainD->rt.d.pixelSize = swapChainD->pixelSize;
    swapChainD->rt.d.rtv[0] = swapChainD->sampleDesc.Count > 1 ?
                swapChainD->msaaRtv[swapChainD->currentFrame] : swapChainD->rtv[swapChainD->currentFrame];
    swapChainD->rt.d.dsv = swapChainD->ds ? swapChainD->ds->dsv : nullptr;

    finishActiveReadbacks();

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiD3D11::endFrame(QRhiSwapChain *swapChain)
{
    Q_ASSERT(inFrame);
    inFrame = false;

    QD3D11SwapChain *swapChainD = QRHI_RES(QD3D11SwapChain, swapChain);
    executeCommandBuffer(&swapChainD->cb);

    if (swapChainD->sampleDesc.Count > 1) {
        context->ResolveSubresource(swapChainD->tex[swapChainD->currentFrame], 0,
                                    swapChainD->msaaTex[swapChainD->currentFrame], 0,
                                    swapChainD->colorFormat);
    }

    const UINT swapInterval = 1;
    const UINT presentFlags = 0;
    HRESULT hr = swapChainD->swapChain->Present(swapInterval, presentFlags);
    if (FAILED(hr))
        qWarning("Failed to present: %s", qPrintable(comErrorMessage(hr)));

    swapChainD->currentFrame = (swapChainD->currentFrame + 1) % QD3D11SwapChain::BUFFER_COUNT;

    contextState.currentSwapChain = nullptr;
    ++finishedFrameCount;
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiD3D11::beginOffscreenFrame(QRhiCommandBuffer **cb)
{
    Q_ASSERT(!inFrame);
    inFrame = true;
    ofr.active = true;

    ofr.cbWrapper.resetState();
    *cb = &ofr.cbWrapper;

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiD3D11::endOffscreenFrame()
{
    Q_ASSERT(inFrame && ofr.active);
    inFrame = false;
    ofr.active = false;

    executeCommandBuffer(&ofr.cbWrapper);
    finishActiveReadbacks();

    ++finishedFrameCount;
    return QRhi::FrameOpSuccess;;
}

static inline DXGI_FORMAT toD3DTextureFormat(QRhiTexture::Format format, QRhiTexture::Flags flags)
{
    const bool srgb = flags.testFlag(QRhiTexture::sRGB);
    switch (format) {
    case QRhiTexture::RGBA8:
        return srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    case QRhiTexture::BGRA8:
        return srgb ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
    case QRhiTexture::R8:
        return DXGI_FORMAT_R8_UNORM;
    case QRhiTexture::R16:
        return DXGI_FORMAT_R16_UNORM;

    case QRhiTexture::D16:
        return DXGI_FORMAT_R16_TYPELESS;
    case QRhiTexture::D32:
        return DXGI_FORMAT_R32_TYPELESS;

    case QRhiTexture::BC1:
        return srgb ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
    case QRhiTexture::BC2:
        return srgb ? DXGI_FORMAT_BC2_UNORM_SRGB : DXGI_FORMAT_BC2_UNORM;
    case QRhiTexture::BC3:
        return srgb ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
    case QRhiTexture::BC4:
        return DXGI_FORMAT_BC4_UNORM;
    case QRhiTexture::BC5:
        return DXGI_FORMAT_BC5_UNORM;
    case QRhiTexture::BC6H:
        return DXGI_FORMAT_BC6H_UF16;
    case QRhiTexture::BC7:
        return srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;

    case QRhiTexture::ETC2_RGB8:
        Q_FALLTHROUGH();
    case QRhiTexture::ETC2_RGB8A1:
        Q_FALLTHROUGH();
    case QRhiTexture::ETC2_RGBA8:
        qWarning("QRhiD3D11 does not support ETC2 textures");
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case QRhiTexture::ASTC_4x4:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_5x4:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_5x5:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_6x5:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_6x6:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_8x5:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_8x6:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_8x8:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_10x5:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_10x6:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_10x8:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_10x10:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_12x10:
        Q_FALLTHROUGH();
    case QRhiTexture::ASTC_12x12:
        qWarning("QRhiD3D11 does not support ASTC textures");
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    default:
        Q_UNREACHABLE();
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

static inline QRhiTexture::Format colorTextureFormatFromDxgiFormat(DXGI_FORMAT format, QRhiTexture::Flags *flags)
{
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return QRhiTexture::RGBA8;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        if (flags)
            (*flags) |= QRhiTexture::sRGB;
        return QRhiTexture::RGBA8;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return QRhiTexture::BGRA8;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        if (flags)
            (*flags) |= QRhiTexture::sRGB;
        return QRhiTexture::BGRA8;
    case DXGI_FORMAT_R8_UNORM:
        return QRhiTexture::R8;
    case DXGI_FORMAT_R16_UNORM:
        return QRhiTexture::R16;
    default: // this cannot assert, must warn and return unknown
        qWarning("DXGI_FORMAT %d is not a recognized uncompressed color format", format);
        break;
    }
    return QRhiTexture::UnknownFormat;
}

static inline bool isDepthTextureFormat(QRhiTexture::Format format)
{
    switch (format) {
    case QRhiTexture::Format::D16:
        Q_FALLTHROUGH();
    case QRhiTexture::Format::D32:
        return true;

    default:
        return false;
    }
}

QRhi::FrameOpResult QRhiD3D11::finish()
{
    Q_ASSERT(!inPass);
    if (inFrame) {
        if (ofr.active) {
            Q_ASSERT(!contextState.currentSwapChain);
            executeCommandBuffer(&ofr.cbWrapper);
            ofr.cbWrapper.resetCommands();
        } else {
            Q_ASSERT(contextState.currentSwapChain);
            executeCommandBuffer(&contextState.currentSwapChain->cb);
            contextState.currentSwapChain->cb.resetCommands();
        }
    }
    finishActiveReadbacks();
    return QRhi::FrameOpSuccess;
}

void QRhiD3D11::enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    QRhiResourceUpdateBatchPrivate *ud = QRhiResourceUpdateBatchPrivate::get(resourceUpdates);

    for (const QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate &u : ud->dynamicBufferUpdates) {
        QD3D11Buffer *bufD = QRHI_RES(QD3D11Buffer, u.buf);
        Q_ASSERT(bufD->m_type == QRhiBuffer::Dynamic);
        memcpy(bufD->dynBuf.data() + u.offset, u.data.constData(), u.data.size());
        bufD->hasPendingDynamicUpdates = true;
    }

    for (const QRhiResourceUpdateBatchPrivate::StaticBufferUpload &u : ud->staticBufferUploads) {
        QD3D11Buffer *bufD = QRHI_RES(QD3D11Buffer, u.buf);
        Q_ASSERT(bufD->m_type != QRhiBuffer::Dynamic);
        Q_ASSERT(u.offset + u.data.size() <= bufD->m_size);
        QD3D11CommandBuffer::Command cmd;
        cmd.cmd = QD3D11CommandBuffer::Command::UpdateSubRes;
        cmd.args.updateSubRes.dst = bufD->buffer;
        cmd.args.updateSubRes.dstSubRes = 0;
        cmd.args.updateSubRes.src = cbD->retainData(u.data);
        cmd.args.updateSubRes.srcRowPitch = 0;
        // Specify the region (even when offset is 0 and all data is provided)
        // since the ID3D11Buffer's size is rounded up to be a multiple of 256
        // while the data we have has the original size.
        D3D11_BOX box;
        box.left = u.offset;
        box.top = box.front = 0;
        box.back = box.bottom = 1;
        box.right = u.offset + u.data.size(); // no -1: right, bottom, back are exclusive, see D3D11_BOX doc
        cmd.args.updateSubRes.hasDstBox = true;
        cmd.args.updateSubRes.dstBox = box;
        cbD->commands.append(cmd);
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureUpload &u : ud->textureUploads) {
        QD3D11Texture *texD = QRHI_RES(QD3D11Texture, u.tex);
        for (int layer = 0, layerCount = u.desc.layers.count(); layer != layerCount; ++layer) {
            const QRhiTextureUploadDescription::Layer &layerDesc(u.desc.layers[layer]);
            for (int level = 0, levelCount = layerDesc.mipImages.count(); level != levelCount; ++level) {
                const QRhiTextureUploadDescription::Layer::MipLevel mipDesc(layerDesc.mipImages[level]);
                UINT subres = D3D11CalcSubresource(level, layer, texD->mipLevelCount);
                const int dx = mipDesc.destinationTopLeft.x();
                const int dy = mipDesc.destinationTopLeft.y();
                D3D11_BOX box;
                box.front = 0;
                // back, right, bottom are exclusive
                box.back = 1;
                QD3D11CommandBuffer::Command cmd;
                cmd.cmd = QD3D11CommandBuffer::Command::UpdateSubRes;
                cmd.args.updateSubRes.dst = texD->tex;
                cmd.args.updateSubRes.dstSubRes = subres;

                if (!mipDesc.image.isNull()) {
                    QImage img = mipDesc.image;
                    int w = img.width();
                    int h = img.height();
                    int bpl = img.bytesPerLine();
                    if (!mipDesc.sourceSize.isEmpty() || !mipDesc.sourceTopLeft.isNull()) {
                        const int sx = mipDesc.sourceTopLeft.x();
                        const int sy = mipDesc.sourceTopLeft.y();
                        if (!mipDesc.sourceSize.isEmpty()) {
                            w = mipDesc.sourceSize.width();
                            h = mipDesc.sourceSize.height();
                        }
                        if (img.depth() == 32) {
                            const int offset = sy * img.bytesPerLine() + sx * 4;
                            cmd.args.updateSubRes.src = cbD->retainImage(img) + offset;
                        } else {
                            img = img.copy(sx, sy, w, h);
                            bpl = img.bytesPerLine();
                            cmd.args.updateSubRes.src = cbD->retainImage(img);
                        }
                    } else {
                        cmd.args.updateSubRes.src = cbD->retainImage(img);
                    }
                    box.left = dx;
                    box.top = dy;
                    box.right = dx + w;
                    box.bottom = dy + h;
                    cmd.args.updateSubRes.hasDstBox = true;
                    cmd.args.updateSubRes.dstBox = box;
                    cmd.args.updateSubRes.srcRowPitch = bpl;
                } else if (!mipDesc.compressedData.isEmpty() && isCompressedFormat(texD->m_format)) {
                    int w, h;
                    if (mipDesc.sourceSize.isEmpty()) {
                        w = qFloor(float(qMax(1, texD->m_pixelSize.width() >> level)));
                        h = qFloor(float(qMax(1, texD->m_pixelSize.height() >> level)));
                    } else {
                        w = mipDesc.sourceSize.width();
                        h = mipDesc.sourceSize.height();
                    }
                    quint32 bpl = 0;
                    QSize blockDim;
                    compressedFormatInfo(texD->m_format, QSize(w, h), &bpl, nullptr, &blockDim);
                    // Everything must be a multiple of the block width and
                    // height, so e.g. a mip level of size 2x2 will be 4x4 when it
                    // comes to the actual data.
                    box.left = aligned(dx, blockDim.width());
                    box.top = aligned(dy, blockDim.height());
                    box.right = aligned(dx + w, blockDim.width());
                    box.bottom = aligned(dy + h, blockDim.height());
                    cmd.args.updateSubRes.hasDstBox = true;
                    cmd.args.updateSubRes.dstBox = box;
                    cmd.args.updateSubRes.src = cbD->retainData(mipDesc.compressedData);
                    cmd.args.updateSubRes.srcRowPitch = bpl;
                }
                cbD->commands.append(cmd);
            }
        }
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureCopy &u : ud->textureCopies) {
        Q_ASSERT(u.src && u.dst);
        QD3D11Texture *srcD = QRHI_RES(QD3D11Texture, u.src);
        QD3D11Texture *dstD = QRHI_RES(QD3D11Texture, u.dst);
        UINT srcSubRes = D3D11CalcSubresource(u.desc.sourceLevel, u.desc.sourceLayer, srcD->mipLevelCount);
        UINT dstSubRes = D3D11CalcSubresource(u.desc.destinationLevel, u.desc.destinationLayer, dstD->mipLevelCount);
        const float dx = u.desc.destinationTopLeft.x();
        const float dy = u.desc.destinationTopLeft.y();
        const QSize size = u.desc.pixelSize.isEmpty() ? srcD->m_pixelSize : u.desc.pixelSize;
        D3D11_BOX srcBox;
        srcBox.left = u.desc.sourceTopLeft.x();
        srcBox.top = u.desc.sourceTopLeft.y();
        srcBox.front = 0;
        // back, right, bottom are exclusive
        srcBox.right = srcBox.left + size.width();
        srcBox.bottom = srcBox.top + size.height();
        srcBox.back = 1;
        QD3D11CommandBuffer::Command cmd;
        cmd.cmd = QD3D11CommandBuffer::Command::CopySubRes;
        cmd.args.copySubRes.dst = dstD->tex;
        cmd.args.copySubRes.dstSubRes = dstSubRes;
        cmd.args.copySubRes.dstX = dx;
        cmd.args.copySubRes.dstY = dy;
        cmd.args.copySubRes.src = srcD->tex;
        cmd.args.copySubRes.srcSubRes = srcSubRes;
        cmd.args.copySubRes.hasSrcBox = true;
        cmd.args.copySubRes.srcBox = srcBox;
        cbD->commands.append(cmd);
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureResolve &u : ud->textureResolves) {
        Q_ASSERT(u.dst);
        Q_ASSERT(u.desc.sourceTexture || u.desc.sourceRenderBuffer);
        QD3D11Texture *dstTexD = QRHI_RES(QD3D11Texture, u.dst);
        if (dstTexD->sampleDesc.Count > 1) {
            qWarning("Cannot resolve into a multisample texture");
            continue;
        }
        QD3D11Texture *srcTexD = QRHI_RES(QD3D11Texture, u.desc.sourceTexture);
        QD3D11RenderBuffer *srcRbD = QRHI_RES(QD3D11RenderBuffer, u.desc.sourceRenderBuffer);
        QD3D11CommandBuffer::Command cmd;
        cmd.cmd = QD3D11CommandBuffer::Command::ResolveSubRes;
        cmd.args.resolveSubRes.dst = dstTexD->tex;
        cmd.args.resolveSubRes.dstSubRes = D3D11CalcSubresource(u.desc.destinationLevel,
                                                                u.desc.destinationLayer,
                                                                dstTexD->mipLevelCount);
        if (srcTexD) {
            cmd.args.resolveSubRes.src = srcTexD->tex;
            if (srcTexD->dxgiFormat != dstTexD->dxgiFormat) {
                qWarning("Resolve source and destination formats do not match");
                continue;
            }
            if (srcTexD->sampleDesc.Count <= 1) {
                qWarning("Cannot resolve a non-multisample texture");
                continue;
            }
            if (srcTexD->m_pixelSize != dstTexD->m_pixelSize) {
                qWarning("Resolve source and destination sizes do not match");
                continue;
            }
        } else {
            cmd.args.resolveSubRes.src = srcRbD->tex;
            if (srcRbD->dxgiFormat != dstTexD->dxgiFormat) {
                qWarning("Resolve source and destination formats do not match");
                continue;
            }
            if (srcRbD->m_pixelSize != dstTexD->m_pixelSize) {
                qWarning("Resolve source and destination sizes do not match");
                continue;
            }
        }
        cmd.args.resolveSubRes.srcSubRes = D3D11CalcSubresource(0, u.desc.sourceLayer, 1);
        cmd.args.resolveSubRes.format = dstTexD->dxgiFormat;
        cbD->commands.append(cmd);
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureRead &u : ud->textureReadbacks) {
        ActiveReadback aRb;
        aRb.desc = u.rb;
        aRb.result = u.result;

        ID3D11Resource *src;
        DXGI_FORMAT dxgiFormat;
        QSize pixelSize;
        QRhiTexture::Format format;
        UINT subres = 0;
        QD3D11Texture *texD = QRHI_RES(QD3D11Texture, u.rb.texture);
        QD3D11SwapChain *swapChainD = nullptr;

        if (texD) {
            if (texD->sampleDesc.Count > 1) {
                qWarning("Multisample texture cannot be read back");
                continue;
            }
            src = texD->tex;
            dxgiFormat = texD->dxgiFormat;
            pixelSize = texD->m_pixelSize;
            if (u.rb.level > 0) {
                pixelSize.setWidth(qFloor(float(qMax(1, pixelSize.width() >> u.rb.level))));
                pixelSize.setHeight(qFloor(float(qMax(1, pixelSize.height() >> u.rb.level))));
            }
            format = texD->m_format;
            subres = D3D11CalcSubresource(u.rb.level, u.rb.layer, texD->mipLevelCount);
        } else {
            Q_ASSERT(contextState.currentSwapChain);
            swapChainD = QRHI_RES(QD3D11SwapChain, contextState.currentSwapChain);
            if (swapChainD->sampleDesc.Count > 1) {
                // Unlike with textures, reading back a multisample swapchain image
                // has to be supported. Insert a resolve.
                QD3D11CommandBuffer::Command rcmd;
                rcmd.cmd = QD3D11CommandBuffer::Command::ResolveSubRes;
                rcmd.args.resolveSubRes.dst = swapChainD->tex[swapChainD->currentFrame];
                rcmd.args.resolveSubRes.dstSubRes = 0;
                rcmd.args.resolveSubRes.src = swapChainD->msaaTex[swapChainD->currentFrame];
                rcmd.args.resolveSubRes.srcSubRes = 0;
                rcmd.args.resolveSubRes.format = swapChainD->colorFormat;
                cbD->commands.append(rcmd);
            }
            src = swapChainD->tex[swapChainD->currentFrame];
            dxgiFormat = swapChainD->colorFormat;
            pixelSize = swapChainD->pixelSize;
            format = colorTextureFormatFromDxgiFormat(dxgiFormat, nullptr);
            if (format == QRhiTexture::UnknownFormat)
                continue;
        }
        quint32 bufSize = 0;
        textureFormatInfo(format, pixelSize, nullptr, &bufSize);

        D3D11_TEXTURE2D_DESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.Width = pixelSize.width();
        desc.Height = pixelSize.height();
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = dxgiFormat;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D *stagingTex;
        HRESULT hr = dev->CreateTexture2D(&desc, nullptr, &stagingTex);
        if (FAILED(hr)) {
            qWarning("Failed to create readback staging texture: %s", qPrintable(comErrorMessage(hr)));
            return;
        }

        QD3D11CommandBuffer::Command cmd;
        cmd.cmd = QD3D11CommandBuffer::Command::CopySubRes;
        cmd.args.copySubRes.dst = stagingTex;
        cmd.args.copySubRes.dstSubRes = 0;
        cmd.args.copySubRes.dstX = 0;
        cmd.args.copySubRes.dstY = 0;
        cmd.args.copySubRes.src = src;
        cmd.args.copySubRes.srcSubRes = subres;
        cmd.args.copySubRes.hasSrcBox = false;
        cbD->commands.append(cmd);

        aRb.stagingTex = stagingTex;
        aRb.bufSize = bufSize;
        aRb.pixelSize = pixelSize;
        aRb.format = format;

        activeReadbacks.append(aRb);
    }

    ud->free();
}

void QRhiD3D11::finishActiveReadbacks()
{
    QVarLengthArray<std::function<void()>, 4> completedCallbacks;

    for (int i = activeReadbacks.count() - 1; i >= 0; --i) {
        const QRhiD3D11::ActiveReadback &aRb(activeReadbacks[i]);
        aRb.result->format = aRb.format;
        aRb.result->pixelSize = aRb.pixelSize;
        aRb.result->data.resize(aRb.bufSize);

        D3D11_MAPPED_SUBRESOURCE mp;
        HRESULT hr = context->Map(aRb.stagingTex, 0, D3D11_MAP_READ, 0, &mp);
        if (FAILED(hr)) {
            qWarning("Failed to map readback staging texture: %s", qPrintable(comErrorMessage(hr)));
            aRb.stagingTex->Release();
            continue;
        }
        memcpy(aRb.result->data.data(), mp.pData, aRb.result->data.size());
        context->Unmap(aRb.stagingTex, 0);

        aRb.stagingTex->Release();

        if (aRb.result->completed)
            completedCallbacks.append(aRb.result->completed);

        activeReadbacks.removeAt(i);
    }

    for (auto f : completedCallbacks)
        f();
}

static inline QD3D11BasicRenderTargetData *basicRtData(QRhiRenderTarget *rt)
{
    switch (rt->type()) {
    case QRhiRenderTarget::RtRef:
        return &QRHI_RES(QD3D11ReferenceRenderTarget, rt)->d;
    case QRhiRenderTarget::RtTexture:
        return &QRHI_RES(QD3D11TextureRenderTarget, rt)->d;
    default:
        Q_UNREACHABLE();
        return nullptr;
    }
}

void QRhiD3D11::resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(inFrame && !inPass);

    enqueueResourceUpdates(cb, resourceUpdates);
}

void QRhiD3D11::beginPass(QRhiCommandBuffer *cb,
                          QRhiRenderTarget *rt,
                          const QRhiColorClearValue &colorClearValue,
                          const QRhiDepthStencilClearValue &depthStencilClearValue,
                          QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(!inPass);

    if (resourceUpdates)
        enqueueResourceUpdates(cb, resourceUpdates);

    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    bool needsColorClear = true;
    QD3D11BasicRenderTargetData *rtD = basicRtData(rt);
    if (rt->type() == QRhiRenderTarget::RtTexture) {
        QD3D11TextureRenderTarget *rtTex = QRHI_RES(QD3D11TextureRenderTarget, rt);
        needsColorClear = !rtTex->m_flags.testFlag(QRhiTextureRenderTarget::PreserveColorContents);
    }

    cbD->currentTarget = rt;

    QD3D11CommandBuffer::Command fbCmd;
    fbCmd.cmd = QD3D11CommandBuffer::Command::SetRenderTarget;
    fbCmd.args.setRenderTarget.rt = rt;
    cbD->commands.append(fbCmd);

    QD3D11CommandBuffer::Command clearCmd;
    clearCmd.cmd = QD3D11CommandBuffer::Command::Clear;
    clearCmd.args.clear.rt = rt;
    clearCmd.args.clear.mask = 0;
    if (!rtD->colorAttCount)
        needsColorClear = false;
    if (rtD->dsAttCount)
        clearCmd.args.clear.mask |= QD3D11CommandBuffer::Command::Depth | QD3D11CommandBuffer::Command::Stencil;
    if (needsColorClear)
        clearCmd.args.clear.mask |= QD3D11CommandBuffer::Command::Color;

    memcpy(clearCmd.args.clear.c, &colorClearValue.rgba, sizeof(float) * 4);
    clearCmd.args.clear.d = depthStencilClearValue.d;
    clearCmd.args.clear.s = depthStencilClearValue.s;

    cbD->commands.append(clearCmd);

    inPass = true;
}

void QRhiD3D11::endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(inPass);
    inPass = false;

    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    cbD->currentTarget = nullptr;

    if (resourceUpdates)
        enqueueResourceUpdates(cb, resourceUpdates);
}

void QRhiD3D11::updateShaderResourceBindings(QD3D11ShaderResourceBindings *srbD)
{
    srbD->vsubufs.clear();
    srbD->vsubufoffsets.clear();
    srbD->vsubufsizes.clear();

    srbD->fsubufs.clear();
    srbD->fsubufoffsets.clear();
    srbD->fsubufsizes.clear();

    srbD->vssamplers.clear();
    srbD->vsshaderresources.clear();

    srbD->fssamplers.clear();
    srbD->fsshaderresources.clear();

    for (int i = 0, ie = srbD->sortedBindings.count(); i != ie; ++i) {
        const QRhiShaderResourceBinding &b(srbD->sortedBindings[i]);
        QD3D11ShaderResourceBindings::BoundResourceData &bd(srbD->boundResourceData[i]);
        switch (b.type) {
        case QRhiShaderResourceBinding::UniformBuffer:
        {
            QD3D11Buffer *bufD = QRHI_RES(QD3D11Buffer, b.ubuf.buf);
            Q_ASSERT(aligned(b.ubuf.offset, 256) == b.ubuf.offset);
            bd.ubuf.generation = bufD->generation;
            const uint offsetInConstants = b.ubuf.offset / 16;
            // size must be 16 mult. (in constants, i.e. multiple of 256 bytes).
            // We can round up if needed since the buffers's actual size
            // (ByteWidth) is always a multiple of 256.
            const uint sizeInConstants = aligned(b.ubuf.maybeSize ? b.ubuf.maybeSize : bufD->m_size, 256) / 16;
            if (b.stage.testFlag(QRhiShaderResourceBinding::VertexStage)) {
                srbD->vsubufs.feed(b.binding, bufD->buffer);
                srbD->vsubufoffsets.feed(b.binding, offsetInConstants);
                srbD->vsubufsizes.feed(b.binding, sizeInConstants);
            }
            if (b.stage.testFlag(QRhiShaderResourceBinding::FragmentStage)) {
                srbD->fsubufs.feed(b.binding, bufD->buffer);
                srbD->fsubufoffsets.feed(b.binding, offsetInConstants);
                srbD->fsubufsizes.feed(b.binding, sizeInConstants);
            }
        }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
            // A sampler with binding N is mapped to a HLSL sampler and texture
            // with registers sN and tN by SPIRV-Cross.
            bd.stex.texGeneration = QRHI_RES(QD3D11Texture, b.stex.tex)->generation;
            bd.stex.samplerGeneration = QRHI_RES(QD3D11Sampler, b.stex.sampler)->generation;
            if (b.stage.testFlag(QRhiShaderResourceBinding::VertexStage)) {
                srbD->vssamplers.feed(b.binding, QRHI_RES(QD3D11Sampler, b.stex.sampler)->samplerState);
                srbD->vsshaderresources.feed(b.binding, QRHI_RES(QD3D11Texture, b.stex.tex)->srv);
            }
            if (b.stage.testFlag(QRhiShaderResourceBinding::FragmentStage)) {
                srbD->fssamplers.feed(b.binding, QRHI_RES(QD3D11Sampler, b.stex.sampler)->samplerState);
                srbD->fsshaderresources.feed(b.binding, QRHI_RES(QD3D11Texture, b.stex.tex)->srv);
            }
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    srbD->vsubufs.finish();
    srbD->vsubufoffsets.finish();
    srbD->vsubufsizes.finish();

    srbD->fsubufs.finish();
    srbD->fsubufoffsets.finish();
    srbD->fsubufsizes.finish();

    srbD->vssamplers.finish();
    srbD->vsshaderresources.finish();

    srbD->fssamplers.finish();
    srbD->fsshaderresources.finish();
}

void QRhiD3D11::executeBufferHostWritesForCurrentFrame(QD3D11Buffer *bufD)
{
    if (!bufD->hasPendingDynamicUpdates)
        return;

    bufD->hasPendingDynamicUpdates = false;
    D3D11_MAPPED_SUBRESOURCE mp;
    HRESULT hr = context->Map(bufD->buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp);
    if (SUCCEEDED(hr)) {
        memcpy(mp.pData, bufD->dynBuf.constData(), bufD->dynBuf.size());
        context->Unmap(bufD->buffer, 0);
    } else {
        qWarning("Failed to map buffer: %s", qPrintable(comErrorMessage(hr)));
    }
}

void QRhiD3D11::setShaderResources(QD3D11ShaderResourceBindings *srbD)
{
    for (int i = 0, ie = srbD->sortedBindings.count(); i != ie; ++i) {
        const QRhiShaderResourceBinding &b(srbD->sortedBindings[i]);
        switch (b.type) {
        case QRhiShaderResourceBinding::UniformBuffer:
        {
            QD3D11Buffer *bufD = QRHI_RES(QD3D11Buffer, b.ubuf.buf);
            if (bufD->m_type == QRhiBuffer::Dynamic)
                executeBufferHostWritesForCurrentFrame(bufD);
        }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    for (const auto &batch : srbD->vssamplers.batches)
        context->VSSetSamplers(batch.startBinding, batch.resources.count(), batch.resources.constData());

    for (const auto &batch : srbD->vsshaderresources.batches) {
        context->VSSetShaderResources(batch.startBinding, batch.resources.count(), batch.resources.constData());
        contextState.vsLastActiveSrvBinding = batch.startBinding + batch.resources.count() - 1;
    }

    for (const auto &batch : srbD->fssamplers.batches)
        context->PSSetSamplers(batch.startBinding, batch.resources.count(), batch.resources.constData());

    for (const auto &batch : srbD->fsshaderresources.batches) {
        context->PSSetShaderResources(batch.startBinding, batch.resources.count(), batch.resources.constData());
        contextState.fsLastActiveSrvBinding = batch.startBinding + batch.resources.count() - 1;
    }

    for (int i = 0, ie = srbD->vsubufs.batches.count(); i != ie; ++i) {
        context->VSSetConstantBuffers1(srbD->vsubufs.batches[i].startBinding,
                                       srbD->vsubufs.batches[i].resources.count(),
                                       srbD->vsubufs.batches[i].resources.constData(),
                                       srbD->vsubufoffsets.batches[i].resources.constData(),
                                       srbD->vsubufsizes.batches[i].resources.constData());
    }

    for (int i = 0, ie = srbD->fsubufs.batches.count(); i != ie; ++i) {
        context->PSSetConstantBuffers1(srbD->fsubufs.batches[i].startBinding,
                                       srbD->fsubufs.batches[i].resources.count(),
                                       srbD->fsubufs.batches[i].resources.constData(),
                                       srbD->fsubufoffsets.batches[i].resources.constData(),
                                       srbD->fsubufsizes.batches[i].resources.constData());
    }
}

void QRhiD3D11::executeCommandBuffer(QD3D11CommandBuffer *cbD)
{
    quint32 stencilRef = 0;
    float blendConstants[] = { 1, 1, 1, 1 };

    for (const QD3D11CommandBuffer::Command &cmd : qAsConst(cbD->commands)) {
        switch (cmd.cmd) {
        case QD3D11CommandBuffer::Command::SetRenderTarget:
        {
            QRhiRenderTarget *rt = cmd.args.setRenderTarget.rt;
            // The new output cannot be bound as input from the previous frame,
            // otherwise the debug layer complains. Avoid this.
            const int nullsrvCount = qMax(contextState.vsLastActiveSrvBinding, contextState.fsLastActiveSrvBinding) + 1;
            QVarLengthArray<ID3D11ShaderResourceView *,
                    D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> nullsrvs(nullsrvCount);
            for (int i = 0; i < nullsrvs.count(); ++i)
                nullsrvs[i] = nullptr;
            context->VSSetShaderResources(0, nullsrvs.count(), nullsrvs.constData());
            context->PSSetShaderResources(0, nullsrvs.count(), nullsrvs.constData());
            QD3D11BasicRenderTargetData *rtD = basicRtData(rt);
            context->OMSetRenderTargets(rtD->colorAttCount, rtD->colorAttCount ? rtD->rtv : nullptr, rtD->dsv);
        }
            break;
        case QD3D11CommandBuffer::Command::Clear:
        {
            QD3D11BasicRenderTargetData *rtD = basicRtData(cmd.args.clear.rt);
            if (cmd.args.clear.mask & QD3D11CommandBuffer::Command::Color) {
                for (int i = 0; i < rtD->colorAttCount; ++i)
                    context->ClearRenderTargetView(rtD->rtv[i], cmd.args.clear.c);
            }
            uint ds = 0;
            if (cmd.args.clear.mask & QD3D11CommandBuffer::Command::Depth)
                ds |= D3D11_CLEAR_DEPTH;
            if (cmd.args.clear.mask & QD3D11CommandBuffer::Command::Stencil)
                ds |= D3D11_CLEAR_STENCIL;
            if (ds)
                context->ClearDepthStencilView(rtD->dsv, ds, cmd.args.clear.d, cmd.args.clear.s);
        }
            break;
        case QD3D11CommandBuffer::Command::Viewport:
        {
            D3D11_VIEWPORT v;
            v.TopLeftX = cmd.args.viewport.x;
            v.TopLeftY = cmd.args.viewport.y;
            v.Width = cmd.args.viewport.w;
            v.Height = cmd.args.viewport.h;
            v.MinDepth = cmd.args.viewport.d0;
            v.MaxDepth = cmd.args.viewport.d1;
            context->RSSetViewports(1, &v);
        }
            break;
        case QD3D11CommandBuffer::Command::Scissor:
        {
            D3D11_RECT r;
            r.left = cmd.args.scissor.x;
            r.top = cmd.args.scissor.y;
            r.right = cmd.args.scissor.x + cmd.args.scissor.w - 1;
            r.bottom = cmd.args.scissor.y + cmd.args.scissor.h - 1;
            context->RSSetScissorRects(1, &r);
        }
            break;
        case QD3D11CommandBuffer::Command::BindVertexBuffers:
            context->IASetVertexBuffers(cmd.args.bindVertexBuffers.startSlot,
                                        cmd.args.bindVertexBuffers.slotCount,
                                        cmd.args.bindVertexBuffers.buffers,
                                        cmd.args.bindVertexBuffers.strides,
                                        cmd.args.bindVertexBuffers.offsets);
            break;
        case QD3D11CommandBuffer::Command::BindIndexBuffer:
            context->IASetIndexBuffer(cmd.args.bindIndexBuffer.buffer,
                                      cmd.args.bindIndexBuffer.format,
                                      cmd.args.bindIndexBuffer.offset);
            break;
        case QD3D11CommandBuffer::Command::BindGraphicsPipeline:
        {
            QD3D11GraphicsPipeline *psD = cmd.args.bindGraphicsPipeline.ps;
            QD3D11ShaderResourceBindings *srbD = cmd.args.bindGraphicsPipeline.srb;
            if (!cmd.args.bindGraphicsPipeline.resOnlyChange) {
                context->VSSetShader(psD->vs, nullptr, 0);
                context->PSSetShader(psD->fs, nullptr, 0);
                context->IASetPrimitiveTopology(psD->d3dTopology);
                context->IASetInputLayout(psD->inputLayout);
                context->OMSetDepthStencilState(psD->dsState, stencilRef);
                context->OMSetBlendState(psD->blendState, blendConstants, 0xffffffff);
                context->RSSetState(psD->rastState);
            }
            setShaderResources(srbD);
        }
            break;
        case QD3D11CommandBuffer::Command::StencilRef:
            stencilRef = cmd.args.stencilRef.ref;
            context->OMSetDepthStencilState(cmd.args.stencilRef.ps->dsState, stencilRef);
            break;
        case QD3D11CommandBuffer::Command::BlendConstants:
            memcpy(blendConstants, cmd.args.blendConstants.c, 4 * sizeof(float));
            context->OMSetBlendState(cmd.args.blendConstants.ps->blendState, blendConstants, 0xffffffff);
            break;
        case QD3D11CommandBuffer::Command::Draw:
            if (cmd.args.draw.ps) {
                if (cmd.args.draw.instanceCount == 1)
                    context->Draw(cmd.args.draw.vertexCount, cmd.args.draw.firstVertex);
                else
                    context->DrawInstanced(cmd.args.draw.vertexCount, cmd.args.draw.instanceCount,
                                           cmd.args.draw.firstVertex, cmd.args.draw.firstInstance);
            } else {
                qWarning("No graphics pipeline active for draw; ignored");
            }
            break;
        case QD3D11CommandBuffer::Command::DrawIndexed:
            if (cmd.args.drawIndexed.ps) {
                if (cmd.args.drawIndexed.instanceCount == 1)
                    context->DrawIndexed(cmd.args.drawIndexed.indexCount, cmd.args.drawIndexed.firstIndex,
                                         cmd.args.drawIndexed.vertexOffset);
                else
                    context->DrawIndexedInstanced(cmd.args.drawIndexed.indexCount, cmd.args.drawIndexed.instanceCount,
                                                  cmd.args.drawIndexed.firstIndex, cmd.args.drawIndexed.vertexOffset,
                                                  cmd.args.drawIndexed.firstInstance);
            } else {
                qWarning("No graphics pipeline active for drawIndexed; ignored");
            }
            break;
        case QD3D11CommandBuffer::Command::UpdateSubRes:
            context->UpdateSubresource(cmd.args.updateSubRes.dst, cmd.args.updateSubRes.dstSubRes,
                                       cmd.args.updateSubRes.hasDstBox ? &cmd.args.updateSubRes.dstBox : nullptr,
                                       cmd.args.updateSubRes.src, cmd.args.updateSubRes.srcRowPitch, 0);
            break;
        case QD3D11CommandBuffer::Command::CopySubRes:
            context->CopySubresourceRegion(cmd.args.copySubRes.dst, cmd.args.copySubRes.dstSubRes,
                                           cmd.args.copySubRes.dstX, cmd.args.copySubRes.dstY, 0,
                                           cmd.args.copySubRes.src, cmd.args.copySubRes.srcSubRes,
                                           cmd.args.copySubRes.hasSrcBox ? &cmd.args.copySubRes.srcBox : nullptr);
            break;
        case QD3D11CommandBuffer::Command::ResolveSubRes:
            context->ResolveSubresource(cmd.args.resolveSubRes.dst, cmd.args.resolveSubRes.dstSubRes,
                                        cmd.args.resolveSubRes.src, cmd.args.resolveSubRes.srcSubRes,
                                        cmd.args.resolveSubRes.format);
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
    if (!buffer)
        return;

    dynBuf.clear();

    buffer->Release();
    buffer = nullptr;
}

static inline uint toD3DBufferUsage(QRhiBuffer::UsageFlags usage)
{
    int u = 0;
    if (usage.testFlag(QRhiBuffer::VertexBuffer))
        u |= D3D11_BIND_VERTEX_BUFFER;;
    if (usage.testFlag(QRhiBuffer::IndexBuffer))
        u |= D3D11_BIND_INDEX_BUFFER;
    if (usage.testFlag(QRhiBuffer::UniformBuffer))
        u |= D3D11_BIND_CONSTANT_BUFFER;
    return u;
}

bool QD3D11Buffer::build()
{
    if (buffer)
        release();

    D3D11_BUFFER_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.ByteWidth = m_usage.testFlag(QRhiBuffer::UniformBuffer) ? aligned(m_size, 256) : m_size;
    desc.Usage = m_type == Dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    desc.BindFlags = toD3DBufferUsage(m_usage);
    desc.CPUAccessFlags = m_type == Dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    QRHI_RES_RHI(QRhiD3D11);
    HRESULT hr = rhiD->dev->CreateBuffer(&desc, nullptr, &buffer);
    if (FAILED(hr)) {
        qWarning("Failed to create buffer: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    if (m_type == Dynamic) {
        dynBuf.resize(m_size);
        hasPendingDynamicUpdates = false;
    }

    generation += 1;
    return true;
}

QD3D11RenderBuffer::QD3D11RenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                                       int sampleCount, QRhiRenderBuffer::Flags flags)
    : QRhiRenderBuffer(rhi, type, pixelSize, sampleCount, flags)
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

    if (rtv) {
        rtv->Release();
        rtv = nullptr;
    }

    tex->Release();
    tex = nullptr;
}

bool QD3D11RenderBuffer::build()
{
    if (tex)
        release();

    QRHI_RES_RHI(QRhiD3D11);
    sampleDesc = rhiD->effectiveSampleCount(m_sampleCount);

    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = m_pixelSize.width();
    desc.Height = m_pixelSize.height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc = sampleDesc;
    desc.Usage = D3D11_USAGE_DEFAULT;

    if (m_type == Color) {
        dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Format = dxgiFormat;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        HRESULT hr = rhiD->dev->CreateTexture2D(&desc, nullptr, &tex);
        if (FAILED(hr)) {
            qWarning("Failed to create color renderbuffer: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        memset(&rtvDesc, 0, sizeof(rtvDesc));
        rtvDesc.Format = dxgiFormat; rtvDesc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS
                                                          : D3D11_RTV_DIMENSION_TEXTURE2D;
        hr = rhiD->dev->CreateRenderTargetView(tex, &rtvDesc, &rtv);
        if (FAILED(hr)) {
            qWarning("Failed to create rtv: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
        return true;
    } else if (m_type == DepthStencil) {
        dxgiFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.Format = dxgiFormat;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        HRESULT hr = rhiD->dev->CreateTexture2D(&desc, nullptr, &tex);
        if (FAILED(hr)) {
            qWarning("Failed to create depth-stencil buffer: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        memset(&dsvDesc, 0, sizeof(dsvDesc));
        dsvDesc.Format = dxgiFormat;
        dsvDesc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS
                                                          : D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = rhiD->dev->CreateDepthStencilView(tex, &dsvDesc, &dsv);
        if (FAILED(hr)) {
            qWarning("Failed to create dsv: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
        return true;
    }

    return false;
}

QRhiTexture::Format QD3D11RenderBuffer::backingFormat() const
{
    return m_type == Color ? QRhiTexture::RGBA8 : QRhiTexture::UnknownFormat;
}

QD3D11Texture::QD3D11Texture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                             int sampleCount, Flags flags)
    : QRhiTexture(rhi, format, pixelSize, sampleCount, flags)
{
}

void QD3D11Texture::release()
{
    if (!tex)
        return;

    if (srv) {
        srv->Release();
        srv = nullptr;
    }

    tex->Release();
    tex = nullptr;
}

static inline DXGI_FORMAT toD3DDepthTextureSRVFormat(QRhiTexture::Format format)
{
    switch (format) {
    case QRhiTexture::Format::D16:
        return DXGI_FORMAT_R16_FLOAT;
    case QRhiTexture::Format::D32:
        return DXGI_FORMAT_R32_FLOAT;
    default:
        Q_UNREACHABLE();
        return DXGI_FORMAT_R32_FLOAT;
    }
}

static inline DXGI_FORMAT toD3DDepthTextureDSVFormat(QRhiTexture::Format format)
{
    switch (format) {
    case QRhiTexture::Format::D16:
        return DXGI_FORMAT_D16_UNORM;
    case QRhiTexture::Format::D32:
        return DXGI_FORMAT_D32_FLOAT;
    default:
        Q_UNREACHABLE();
        return DXGI_FORMAT_D32_FLOAT;
    }
}

bool QD3D11Texture::build()
{
    if (tex)
        release();

    QRHI_RES_RHI(QRhiD3D11);
    const QSize size = m_pixelSize.isEmpty() ? QSize(16, 16) : m_pixelSize;
    const bool isDepth = isDepthTextureFormat(m_format);
    const bool isCube = m_flags.testFlag(CubeMap);
    const bool hasMipMaps = m_flags.testFlag(MipMapped);

    dxgiFormat = toD3DTextureFormat(m_format, m_flags);
    mipLevelCount = hasMipMaps ? qCeil(log2(qMax(size.width(), size.height()))) + 1 : 1;
    sampleDesc = rhiD->effectiveSampleCount(m_sampleCount);
    if (sampleDesc.Count > 1) {
        if (isCube) {
            qWarning("Cubemap texture cannot be multisample");
            return false;
        }
        if (hasMipMaps) {
            qWarning("Multisample texture cannot have mipmaps");
            return false;
        }
    }

    uint bindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (m_flags.testFlag(RenderTarget)) {
        if (isDepth)
            bindFlags |= D3D11_BIND_DEPTH_STENCIL;
        else
            bindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = size.width();
    desc.Height = size.height();
    desc.MipLevels = mipLevelCount;
    desc.ArraySize = isCube ? 6 : 1;;
    desc.Format = dxgiFormat;
    desc.SampleDesc = sampleDesc;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

    HRESULT hr = rhiD->dev->CreateTexture2D(&desc, nullptr, &tex);
    if (FAILED(hr)) {
        qWarning("Failed to create texture: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    memset(&srvDesc, 0, sizeof(srvDesc));
    srvDesc.Format = isDepth ? toD3DDepthTextureSRVFormat(m_format) : desc.Format;
    if (isCube) {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = desc.MipLevels;
    } else {
        if (sampleDesc.Count > 1) {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
        } else {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
        }
    }

    hr = rhiD->dev->CreateShaderResourceView(tex, &srvDesc, &srv);
    if (FAILED(hr)) {
        qWarning("Failed to create srv: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    generation += 1;
    return true;
}

QD3D11Sampler::QD3D11Sampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                             AddressMode u, AddressMode v, AddressMode w)
    : QRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v, w)
{
}

void QD3D11Sampler::release()
{
    if (!samplerState)
        return;

    samplerState->Release();
    samplerState = nullptr;
}

static inline D3D11_FILTER toD3DFilter(QRhiSampler::Filter minFilter, QRhiSampler::Filter magFilter, QRhiSampler::Filter mipFilter)
{
    if (minFilter == QRhiSampler::Nearest) {
        if (magFilter == QRhiSampler::Nearest) {
            if (mipFilter == QRhiSampler::Linear)
                return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            else
                return D3D11_FILTER_MIN_MAG_MIP_POINT;
        } else {
            if (mipFilter == QRhiSampler::Linear)
                return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
            else
                return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        }
    } else {
        if (magFilter == QRhiSampler::Nearest) {
            if (mipFilter == QRhiSampler::Linear)
                return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
            else
                return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        } else {
            if (mipFilter == QRhiSampler::Linear)
                return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            else
                return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        }
    }

    Q_UNREACHABLE();
    return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
}

static inline D3D11_TEXTURE_ADDRESS_MODE toD3DAddressMode(QRhiSampler::AddressMode m)
{
    switch (m) {
    case QRhiSampler::Repeat:
        return D3D11_TEXTURE_ADDRESS_WRAP;
    case QRhiSampler::ClampToEdge:
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    case QRhiSampler::Border:
        return D3D11_TEXTURE_ADDRESS_BORDER;
    case QRhiSampler::Mirror:
        return D3D11_TEXTURE_ADDRESS_MIRROR;
    case QRhiSampler::MirrorOnce:
        return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
    default:
        Q_UNREACHABLE();
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    }
}

bool QD3D11Sampler::build()
{
    if (samplerState)
        release();

    D3D11_SAMPLER_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Filter = toD3DFilter(m_minFilter, m_magFilter, m_mipmapMode);
    desc.AddressU = toD3DAddressMode(m_addressU);
    desc.AddressV = toD3DAddressMode(m_addressV);
    desc.AddressW = toD3DAddressMode(m_addressW);
    desc.MaxAnisotropy = 1.0f;
    desc.MaxLOD = m_mipmapMode == None ? 0.0f : 1000.0f;

    QRHI_RES_RHI(QRhiD3D11);
    HRESULT hr = rhiD->dev->CreateSamplerState(&desc, &samplerState);
    if (FAILED(hr)) {
        qWarning("Failed to create sampler state: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    generation += 1;
    return true;
}

// dummy, no Vulkan-style RenderPass+Framebuffer concept here
QD3D11RenderPassDescriptor::QD3D11RenderPassDescriptor(QRhiImplementation *rhi)
    : QRhiRenderPassDescriptor(rhi)
{
}

void QD3D11RenderPassDescriptor::release()
{
    // nothing to do here
}

QD3D11ReferenceRenderTarget::QD3D11ReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiReferenceRenderTarget(rhi),
      d(rhi)
{
}

void QD3D11ReferenceRenderTarget::release()
{
    // nothing to do here
}

QRhiRenderTarget::Type QD3D11ReferenceRenderTarget::type() const
{
    return RtRef;
}

QSize QD3D11ReferenceRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

QD3D11TextureRenderTarget::QD3D11TextureRenderTarget(QRhiImplementation *rhi,
                                                     const QRhiTextureRenderTargetDescription &desc,
                                                     Flags flags)
    : QRhiTextureRenderTarget(rhi, desc, flags),
      d(rhi)
{
    for (int i = 0; i < QD3D11BasicRenderTargetData::MAX_COLOR_ATTACHMENTS; ++i) {
        ownsRtv[i] = false;
        rtv[i] = nullptr;
    }
}

void QD3D11TextureRenderTarget::release()
{
    if (!rtv[0] && !dsv)
        return;

    if (dsv) {
        if (ownsDsv)
            dsv->Release();
        dsv = nullptr;
    }

    for (int i = 0; i < QD3D11BasicRenderTargetData::MAX_COLOR_ATTACHMENTS; ++i) {
        if (rtv[i]) {
            if (ownsRtv[i])
                rtv[i]->Release();
            rtv[i] = nullptr;
        }
    }
}

QRhiRenderPassDescriptor *QD3D11TextureRenderTarget::newCompatibleRenderPassDescriptor()
{
    return new QD3D11RenderPassDescriptor(rhi);
}

bool QD3D11TextureRenderTarget::build()
{
    if (rtv[0] || dsv)
        release();

    Q_ASSERT(!m_desc.colorAttachments.isEmpty() || m_desc.depthTexture);
    Q_ASSERT(!m_desc.depthStencilBuffer || !m_desc.depthTexture);
    const bool hasDepthStencil = m_desc.depthStencilBuffer || m_desc.depthTexture;

    QRHI_RES_RHI(QRhiD3D11);

    d.colorAttCount = m_desc.colorAttachments.count();
    for (int i = 0; i < d.colorAttCount; ++i) {
        QRhiTexture *texture = m_desc.colorAttachments[i].texture;
        QRhiRenderBuffer *rb = m_desc.colorAttachments[i].renderBuffer;
        Q_ASSERT(texture || rb);
        if (texture) {
            QD3D11Texture *texD = QRHI_RES(QD3D11Texture, texture);
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            memset(&rtvDesc, 0, sizeof(rtvDesc));
            rtvDesc.Format = toD3DTextureFormat(texD->format(), texD->flags());
            if (texD->flags().testFlag(QRhiTexture::CubeMap)) {
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.FirstArraySlice = m_desc.colorAttachments[i].layer;
                rtvDesc.Texture2DArray.ArraySize = 1;
            } else {
                rtvDesc.ViewDimension = texD->sampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS
                                                                   : D3D11_RTV_DIMENSION_TEXTURE2D;
            }
            HRESULT hr = rhiD->dev->CreateRenderTargetView(texD->tex, &rtvDesc, &rtv[i]);
            if (FAILED(hr)) {
                qWarning("Failed to create rtv: %s", qPrintable(comErrorMessage(hr)));
                return false;
            }
            ownsRtv[i] = true;
            if (i == 0)
                d.pixelSize = texD->pixelSize();
        } else if (rb) {
            QD3D11RenderBuffer *rbD = QRHI_RES(QD3D11RenderBuffer, rb);
            ownsRtv[i] = false;
            rtv[i] = rbD->rtv;
            if (i == 0)
                d.pixelSize = rbD->pixelSize();
        } else {
            Q_UNREACHABLE();
        }
    }

    if (hasDepthStencil) {
        if (m_desc.depthTexture) {
            ownsDsv = true;
            QD3D11Texture *depthTexD = QRHI_RES(QD3D11Texture, m_desc.depthTexture);
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            memset(&dsvDesc, 0, sizeof(dsvDesc));
            dsvDesc.Format = toD3DDepthTextureDSVFormat(depthTexD->format());
            dsvDesc.ViewDimension = depthTexD->sampleDesc.Count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS
                                                                    : D3D11_DSV_DIMENSION_TEXTURE2D;
            HRESULT hr = rhiD->dev->CreateDepthStencilView(depthTexD->tex, &dsvDesc, &dsv);
            if (FAILED(hr)) {
                qWarning("Failed to create dsv: %s", qPrintable(comErrorMessage(hr)));
                return false;
            }
            if (d.colorAttCount == 0)
                d.pixelSize = depthTexD->pixelSize();
        } else {
            ownsDsv = false;
            dsv = QRHI_RES(QD3D11RenderBuffer, m_desc.depthStencilBuffer)->dsv;
            if (d.colorAttCount == 0)
                d.pixelSize = m_desc.depthStencilBuffer->pixelSize();
        }
        d.dsAttCount = 1;
    } else {
        d.dsAttCount = 0;
    }

    for (int i = 0; i < QD3D11BasicRenderTargetData::MAX_COLOR_ATTACHMENTS; ++i)
        d.rtv[i] = i < d.colorAttCount ? rtv[i] : nullptr;

    d.dsv = dsv;
    d.rp = QRHI_RES(QD3D11RenderPassDescriptor, m_renderPassDesc);

    return true;
}

QRhiRenderTarget::Type QD3D11TextureRenderTarget::type() const
{
    return RtTexture;
}

QSize QD3D11TextureRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

QD3D11ShaderResourceBindings::QD3D11ShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiShaderResourceBindings(rhi)
{
}

void QD3D11ShaderResourceBindings::release()
{
    sortedBindings.clear();
}

bool QD3D11ShaderResourceBindings::build()
{
    if (!sortedBindings.isEmpty())
        release();

    sortedBindings = m_bindings;
    std::sort(sortedBindings.begin(), sortedBindings.end(),
              [](const QRhiShaderResourceBinding &a, const QRhiShaderResourceBinding &b)
    {
        return a.binding < b.binding;
    });

    boundResourceData.resize(sortedBindings.count());

    QRHI_RES_RHI(QRhiD3D11);
    rhiD->updateShaderResourceBindings(this);

    generation += 1;
    return true;
}

QD3D11GraphicsPipeline::QD3D11GraphicsPipeline(QRhiImplementation *rhi)
    : QRhiGraphicsPipeline(rhi)
{
}

void QD3D11GraphicsPipeline::release()
{
    if (!dsState)
        return;

    dsState->Release();
    dsState = nullptr;

    if (blendState) {
        blendState->Release();
        blendState = nullptr;
    }

    if (inputLayout) {
        inputLayout->Release();
        inputLayout = nullptr;
    }

    if (rastState) {
        rastState->Release();
        rastState = nullptr;
    }

    if (vs) {
        vs->Release();
        vs = nullptr;
    }

    if (fs) {
        fs->Release();
        fs = nullptr;
    }
}

static inline D3D11_CULL_MODE toD3DCullMode(QRhiGraphicsPipeline::CullMode c)
{
    switch (c) {
    case QRhiGraphicsPipeline::None:
        return D3D11_CULL_NONE;
    case QRhiGraphicsPipeline::Front:
        return D3D11_CULL_FRONT;
    case QRhiGraphicsPipeline::Back:
        return D3D11_CULL_BACK;
    default:
        Q_UNREACHABLE();
        return D3D11_CULL_NONE;
    }
}

static inline D3D11_COMPARISON_FUNC toD3DCompareOp(QRhiGraphicsPipeline::CompareOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::Never:
        return D3D11_COMPARISON_NEVER;
    case QRhiGraphicsPipeline::Less:
        return D3D11_COMPARISON_LESS;
    case QRhiGraphicsPipeline::Equal:
        return D3D11_COMPARISON_EQUAL;
    case QRhiGraphicsPipeline::LessOrEqual:
        return D3D11_COMPARISON_LESS_EQUAL;
    case QRhiGraphicsPipeline::Greater:
        return D3D11_COMPARISON_GREATER;
    case QRhiGraphicsPipeline::NotEqual:
        return D3D11_COMPARISON_NOT_EQUAL;
    case QRhiGraphicsPipeline::GreaterOrEqual:
        return D3D11_COMPARISON_GREATER_EQUAL;
    case QRhiGraphicsPipeline::Always:
        return D3D11_COMPARISON_ALWAYS;
    default:
        Q_UNREACHABLE();
        return D3D11_COMPARISON_ALWAYS;
    }
}

static inline D3D11_STENCIL_OP toD3DStencilOp(QRhiGraphicsPipeline::StencilOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::StencilZero:
        return D3D11_STENCIL_OP_ZERO;
    case QRhiGraphicsPipeline::Keep:
        return D3D11_STENCIL_OP_KEEP;
    case QRhiGraphicsPipeline::Replace:
        return D3D11_STENCIL_OP_REPLACE;
    case QRhiGraphicsPipeline::IncrementAndClamp:
        return D3D11_STENCIL_OP_INCR_SAT;
    case QRhiGraphicsPipeline::DecrementAndClamp:
        return D3D11_STENCIL_OP_DECR_SAT;
    case QRhiGraphicsPipeline::Invert:
        return D3D11_STENCIL_OP_INVERT;
    case QRhiGraphicsPipeline::IncrementAndWrap:
        return D3D11_STENCIL_OP_INCR;
    case QRhiGraphicsPipeline::DecrementAndWrap:
        return D3D11_STENCIL_OP_DECR;
    default:
        Q_UNREACHABLE();
        return D3D11_STENCIL_OP_KEEP;
    }
}

static inline DXGI_FORMAT toD3DAttributeFormat(QRhiVertexInputLayout::Attribute::Format format)
{
    switch (format) {
    case QRhiVertexInputLayout::Attribute::Float4:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case QRhiVertexInputLayout::Attribute::Float3:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case QRhiVertexInputLayout::Attribute::Float2:
        return DXGI_FORMAT_R32G32_FLOAT;
    case QRhiVertexInputLayout::Attribute::Float:
        return DXGI_FORMAT_R32_FLOAT;
    case QRhiVertexInputLayout::Attribute::UNormByte4:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case QRhiVertexInputLayout::Attribute::UNormByte2:
        return DXGI_FORMAT_R8G8_UNORM;
    case QRhiVertexInputLayout::Attribute::UNormByte:
        return DXGI_FORMAT_R8_UNORM;
    default:
        Q_UNREACHABLE();
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
}

static inline D3D11_PRIMITIVE_TOPOLOGY toD3DTopology(QRhiGraphicsPipeline::Topology t)
{
    switch (t) {
    case QRhiGraphicsPipeline::Triangles:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case QRhiGraphicsPipeline::TriangleStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case QRhiGraphicsPipeline::Lines:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case QRhiGraphicsPipeline::LineStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case QRhiGraphicsPipeline::Points:
        return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    default:
        Q_UNREACHABLE();
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

static inline uint toD3DColorWriteMask(QRhiGraphicsPipeline::ColorMask c)
{
    uint f = 0;
    if (c.testFlag(QRhiGraphicsPipeline::R))
        f |= D3D11_COLOR_WRITE_ENABLE_RED;
    if (c.testFlag(QRhiGraphicsPipeline::G))
        f |= D3D11_COLOR_WRITE_ENABLE_GREEN;
    if (c.testFlag(QRhiGraphicsPipeline::B))
        f |= D3D11_COLOR_WRITE_ENABLE_BLUE;
    if (c.testFlag(QRhiGraphicsPipeline::A))
        f |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
    return f;
}

static inline D3D11_BLEND toD3DBlendFactor(QRhiGraphicsPipeline::BlendFactor f)
{
    switch (f) {
    case QRhiGraphicsPipeline::Zero:
        return D3D11_BLEND_ZERO;
    case QRhiGraphicsPipeline::One:
        return D3D11_BLEND_ONE;
    case QRhiGraphicsPipeline::SrcColor:
        return D3D11_BLEND_SRC_COLOR;
    case QRhiGraphicsPipeline::OneMinusSrcColor:
        return D3D11_BLEND_INV_SRC_COLOR;
    case QRhiGraphicsPipeline::DstColor:
        return D3D11_BLEND_DEST_COLOR;
    case QRhiGraphicsPipeline::OneMinusDstColor:
        return D3D11_BLEND_INV_DEST_COLOR;
    case QRhiGraphicsPipeline::SrcAlpha:
        return D3D11_BLEND_SRC_ALPHA;
    case QRhiGraphicsPipeline::OneMinusSrcAlpha:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case QRhiGraphicsPipeline::DstAlpha:
        return D3D11_BLEND_DEST_ALPHA;
    case QRhiGraphicsPipeline::OneMinusDstAlpha:
        return D3D11_BLEND_INV_DEST_ALPHA;
    case QRhiGraphicsPipeline::ConstantColor:
        Q_FALLTHROUGH();
    case QRhiGraphicsPipeline::ConstantAlpha:
        return D3D11_BLEND_BLEND_FACTOR;
    case QRhiGraphicsPipeline::OneMinusConstantColor:
        Q_FALLTHROUGH();
    case QRhiGraphicsPipeline::OneMinusConstantAlpha:
        return D3D11_BLEND_INV_BLEND_FACTOR;
    case QRhiGraphicsPipeline::SrcAlphaSaturate:
        return D3D11_BLEND_SRC_ALPHA_SAT;
    case QRhiGraphicsPipeline::Src1Color:
        return D3D11_BLEND_SRC1_COLOR;
    case QRhiGraphicsPipeline::OneMinusSrc1Color:
        return D3D11_BLEND_INV_SRC1_COLOR;
    case QRhiGraphicsPipeline::Src1Alpha:
        return D3D11_BLEND_SRC1_ALPHA;
    case QRhiGraphicsPipeline::OneMinusSrc1Alpha:
        return D3D11_BLEND_INV_SRC1_ALPHA;
    default:
        Q_UNREACHABLE();
        return D3D11_BLEND_ZERO;
    }
}

static inline D3D11_BLEND_OP toD3DBlendOp(QRhiGraphicsPipeline::BlendOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::Add:
        return D3D11_BLEND_OP_ADD;
    case QRhiGraphicsPipeline::Subtract:
        return D3D11_BLEND_OP_SUBTRACT;
    case QRhiGraphicsPipeline::ReverseSubtract:
        return D3D11_BLEND_OP_REV_SUBTRACT;
    case QRhiGraphicsPipeline::Min:
        return D3D11_BLEND_OP_MIN;
    case QRhiGraphicsPipeline::Max:
        return D3D11_BLEND_OP_MAX;
    default:
        Q_UNREACHABLE();
        return D3D11_BLEND_OP_ADD;
    }
}

static QByteArray compileHlslShaderSource(const QBakedShader &shader, QString *error)
{
    QBakedShader::Shader dxbc = shader.shader({ QBakedShader::DxbcShader, 50 });
    if (!dxbc.shader.isEmpty())
        return dxbc.shader;

    QBakedShader::Shader hlslSource = shader.shader({ QBakedShader::HlslShader, 50 });
    if (hlslSource.shader.isEmpty()) {
        qWarning() << "No HLSL (shader model 5.0) code found in baked shader" << shader;
        return QByteArray();
    }

    const char *target;
    switch (shader.stage()) {
    case QBakedShader::VertexStage:
        target = "vs_5_0";
        break;
    case QBakedShader::TessControlStage:
        target = "hs_5_0";
        break;
    case QBakedShader::TessEvaluationStage:
        target = "ds_5_0";
        break;
    case QBakedShader::GeometryStage:
        target = "gs_5_0";
        break;
    case QBakedShader::FragmentStage:
        target = "ps_5_0";
        break;
    case QBakedShader::ComputeStage:
        target = "cs_5_0";
        break;
    default:
        Q_UNREACHABLE();
        return QByteArray();
    }

    ID3DBlob *bytecode = nullptr;
    ID3DBlob *errors = nullptr;
    HRESULT hr = D3DCompile(hlslSource.shader.constData(), hlslSource.shader.size(),
                            nullptr, nullptr, nullptr,
                            hlslSource.entryPoint.constData(), target, 0, 0, &bytecode, &errors);
    if (FAILED(hr) || !bytecode) {
        qWarning("HLSL shader compilation failed: 0x%x", hr);
        if (errors) {
            *error = QString::fromUtf8(static_cast<const char *>(errors->GetBufferPointer()),
                                       errors->GetBufferSize());
            errors->Release();
        }
        return QByteArray();
    }

    QByteArray result;
    result.resize(bytecode->GetBufferSize());
    memcpy(result.data(), bytecode->GetBufferPointer(), result.size());
    bytecode->Release();
    return result;
}

bool QD3D11GraphicsPipeline::build()
{
    if (dsState)
        release();

    QRHI_RES_RHI(QRhiD3D11);

    D3D11_RASTERIZER_DESC rastDesc;
    memset(&rastDesc, 0, sizeof(rastDesc));
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = toD3DCullMode(m_cullMode);
    rastDesc.FrontCounterClockwise = m_frontFace == CCW;
    rastDesc.ScissorEnable = m_flags.testFlag(UsesScissor);
    rastDesc.MultisampleEnable = rhiD->effectiveSampleCount(m_sampleCount).Count > 1;
    HRESULT hr = rhiD->dev->CreateRasterizerState(&rastDesc, &rastState);
    if (FAILED(hr)) {
        qWarning("Failed to create rasterizer state: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC dsDesc;
    memset(&dsDesc, 0, sizeof(dsDesc));
    dsDesc.DepthEnable = m_depthTest;
    dsDesc.DepthWriteMask = m_depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = toD3DCompareOp(m_depthOp);
    dsDesc.StencilEnable = m_stencilTest;
    if (m_stencilTest) {
        dsDesc.StencilReadMask = m_stencilReadMask;
        dsDesc.StencilWriteMask = m_stencilWriteMask;
        dsDesc.FrontFace.StencilFailOp = toD3DStencilOp(m_stencilFront.failOp);
        dsDesc.FrontFace.StencilDepthFailOp = toD3DStencilOp(m_stencilFront.depthFailOp);
        dsDesc.FrontFace.StencilPassOp = toD3DStencilOp(m_stencilFront.passOp);
        dsDesc.FrontFace.StencilFunc = toD3DCompareOp(m_stencilFront.compareOp);
        dsDesc.BackFace.StencilFailOp = toD3DStencilOp(m_stencilBack.failOp);
        dsDesc.BackFace.StencilDepthFailOp = toD3DStencilOp(m_stencilBack.depthFailOp);
        dsDesc.BackFace.StencilPassOp = toD3DStencilOp(m_stencilBack.passOp);
        dsDesc.BackFace.StencilFunc = toD3DCompareOp(m_stencilBack.compareOp);
    }
    hr = rhiD->dev->CreateDepthStencilState(&dsDesc, &dsState);
    if (FAILED(hr)) {
        qWarning("Failed to create depth-stencil state: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    D3D11_BLEND_DESC blendDesc;
    memset(&blendDesc, 0, sizeof(blendDesc));
    blendDesc.IndependentBlendEnable = m_targetBlends.count() > 1;
    for (int i = 0, ie = m_targetBlends.count(); i != ie; ++i) {
        const QRhiGraphicsPipeline::TargetBlend &b(m_targetBlends[i]);
        D3D11_RENDER_TARGET_BLEND_DESC blend;
        memset(&blend, 0, sizeof(blend));
        blend.BlendEnable = b.enable;
        blend.SrcBlend = toD3DBlendFactor(b.srcColor);
        blend.DestBlend = toD3DBlendFactor(b.dstColor);
        blend.BlendOp = toD3DBlendOp(b.opColor);
        blend.SrcBlendAlpha = toD3DBlendFactor(b.srcAlpha);
        blend.DestBlendAlpha = toD3DBlendFactor(b.dstAlpha);
        blend.BlendOpAlpha = toD3DBlendOp(b.opAlpha);
        blend.RenderTargetWriteMask = toD3DColorWriteMask(b.colorWrite);
        blendDesc.RenderTarget[i] = blend;
    }
    if (m_targetBlends.isEmpty()) {
        D3D11_RENDER_TARGET_BLEND_DESC blend;
        memset(&blend, 0, sizeof(blend));
        blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0] = blend;
    }
    hr = rhiD->dev->CreateBlendState(&blendDesc, &blendState);
    if (FAILED(hr)) {
        qWarning("Failed to create blend state: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    QByteArray vsByteCode;
    for (const QRhiGraphicsShaderStage &shaderStage : qAsConst(m_shaderStages)) {
        QString error;
        QByteArray bytecode = compileHlslShaderSource(shaderStage.shader, &error);
        if (bytecode.isEmpty()) {
            qWarning("HLSL shader compilation failed: %s", qPrintable(error));
            return false;
        }
        switch (shaderStage.type) {
        case QRhiGraphicsShaderStage::Vertex:
            hr = rhiD->dev->CreateVertexShader(bytecode.constData(), bytecode.size(), nullptr, &vs);
            if (FAILED(hr)) {
                qWarning("Failed to create vertex shader: %s", qPrintable(comErrorMessage(hr)));
                return false;
            }
            vsByteCode = bytecode;
            break;
        case QRhiGraphicsShaderStage::Fragment:
            hr = rhiD->dev->CreatePixelShader(bytecode.constData(), bytecode.size(), nullptr, &fs);
            if (FAILED(hr)) {
                qWarning("Failed to create pixel shader: %s", qPrintable(comErrorMessage(hr)));
                return false;
            }
            break;
        default:
            break;
        }
    }

    d3dTopology = toD3DTopology(m_topology);

    if (!vsByteCode.isEmpty()) {
        QVarLengthArray<D3D11_INPUT_ELEMENT_DESC, 4> inputDescs;
        for (const QRhiVertexInputLayout::Attribute &attribute : m_vertexInputLayout.attributes) {
            D3D11_INPUT_ELEMENT_DESC desc;
            memset(&desc, 0, sizeof(desc));
            // the output from SPIRV-Cross uses TEXCOORD<location> as the semantic
            desc.SemanticName = "TEXCOORD";
            desc.SemanticIndex = attribute.location;
            desc.Format = toD3DAttributeFormat(attribute.format);
            desc.InputSlot = attribute.binding;
            desc.AlignedByteOffset = attribute.offset;
            const QRhiVertexInputLayout::Binding &binding(m_vertexInputLayout.bindings[attribute.binding]);
            if (binding.classification == QRhiVertexInputLayout::Binding::PerInstance) {
                desc.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
                desc.InstanceDataStepRate = binding.instanceStepRate;
            } else {
                desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            }
            inputDescs.append(desc);
        }
        hr = rhiD->dev->CreateInputLayout(inputDescs.constData(), inputDescs.count(), vsByteCode, vsByteCode.size(), &inputLayout);
        if (FAILED(hr)) {
            qWarning("Failed to create input layout: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
    }

    generation += 1;
    return true;
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
        msaaTex[i] = nullptr;
        msaaRtv[i] = nullptr;
    }
}

void QD3D11SwapChain::releaseBuffers()
{
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        if (rtv[i]) {
            rtv[i]->Release();
            rtv[i] = nullptr;
        }
        if (tex[i]) {
            tex[i]->Release();
            tex[i] = nullptr;
        }
        if (msaaRtv[i]) {
            msaaRtv[i]->Release();
            msaaRtv[i] = nullptr;
        }
        if (msaaTex[i]) {
            msaaTex[i]->Release();
            msaaTex[i] = nullptr;
        }
    }
}

void QD3D11SwapChain::release()
{
    if (!swapChain)
        return;

    releaseBuffers();

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

QSize QD3D11SwapChain::effectivePixelSize() const
{
    return pixelSize;
}

QRhiRenderPassDescriptor *QD3D11SwapChain::newCompatibleRenderPassDescriptor()
{
    return new QD3D11RenderPassDescriptor(rhi);
}

bool QD3D11SwapChain::newColorBuffer(const QSize &size, DXGI_FORMAT format, DXGI_SAMPLE_DESC sampleDesc,
                                     ID3D11Texture2D **tex, ID3D11RenderTargetView **rtv) const
{
    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = size.width();
    desc.Height = size.height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc = sampleDesc;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    QRHI_RES_RHI(QRhiD3D11);
    HRESULT hr = rhiD->dev->CreateTexture2D(&desc, nullptr, tex);
    if (FAILED(hr)) {
        qWarning("Failed to create color buffer texture: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    memset(&rtvDesc, 0, sizeof(rtvDesc));
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = sampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = rhiD->dev->CreateRenderTargetView(*tex, &rtvDesc, rtv);
    if (FAILED(hr)) {
        qWarning("Failed to create color buffer rtv: %s", qPrintable(comErrorMessage(hr)));
        (*tex)->Release();
        *tex = nullptr;
        return false;
    }

    return true;
}

bool QD3D11SwapChain::buildOrResize()
{
    // Can be called multiple times due to window resizes - that is not the
    // same as a simple release+build (as with other resources). Just need to
    // resize the buffers then.

    Q_ASSERT(!swapChain || window == m_window);

    window = m_window;
    pixelSize = m_requestedPixelSize;

    colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    const DXGI_FORMAT srgbAdjustedFormat = m_flags.testFlag(sRGB) ?
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

    const UINT swapChainFlags = 0;

    QRHI_RES_RHI(QRhiD3D11);
    if (!swapChain) {
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        sampleDesc = rhiD->effectiveSampleCount(m_sampleCount);

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
        if (m_flags.testFlag(SurfaceHasPreMulAlpha))
            desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        else if (m_flags.testFlag(SurfaceHasNonPreMulAlpha))
            desc.AlphaMode = DXGI_ALPHA_MODE_STRAIGHT;
        desc.Flags = swapChainFlags;

        HRESULT hr = rhiD->dxgiFactory->CreateSwapChainForHwnd(rhiD->dev, hwnd, &desc, nullptr, nullptr, &swapChain);
        if (FAILED(hr)) {
            qWarning("Failed to create D3D11 swapchain: %s", qPrintable(comErrorMessage(hr)));
            return false;
        }
    } else {
        releaseBuffers();
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
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        memset(&rtvDesc, 0, sizeof(rtvDesc));
        rtvDesc.Format = srgbAdjustedFormat;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        hr = rhiD->dev->CreateRenderTargetView(tex[i], &rtvDesc, &rtv[i]);
        if (FAILED(hr)) {
            qWarning("Failed to create rtv for swapchain buffer %d: %s", i, qPrintable(comErrorMessage(hr)));
            return false;
        }
        if (sampleDesc.Count > 1) {
            if (!newColorBuffer(pixelSize, srgbAdjustedFormat, sampleDesc, &msaaTex[i], &msaaRtv[i]))
                return false;
        }
    }

    currentFrame = 0;
    ds = m_depthStencil ? QRHI_RES(QD3D11RenderBuffer, m_depthStencil) : nullptr;

    QD3D11ReferenceRenderTarget *rtD = QRHI_RES(QD3D11ReferenceRenderTarget, &rt);
    rtD->d.rp = QRHI_RES(QD3D11RenderPassDescriptor, m_renderPassDesc);
    rtD->d.pixelSize = pixelSize;
    rtD->d.colorAttCount = 1;
    rtD->d.dsAttCount = m_depthStencil ? 1 : 0;

    return true;
}

QT_END_NAMESPACE
