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
{
    QRhiD3D11InitParams *d3dparams = static_cast<QRhiD3D11InitParams *>(params);
    debugLayer = d3dparams->enableDebugLayer;
    importedDevice = d3dparams->importExistingDevice;
    if (importedDevice) {
        dev = d3dparams->dev;
        context = d3dparams->context;
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

void QRhiD3D11::create()
{
    uint flags = 0;
    if (debugLayer)
        flags |= D3D11_CREATE_DEVICE_DEBUG;

    if (!importedDevice) {
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                       nullptr, 0, D3D11_SDK_VERSION,
                                       &dev, &featureLevel, &context);
        if (FAILED(hr)) {
            qWarning("Failed to create D3D11 device and context: %s", qPrintable(comErrorMessage(hr)));
            return;
        }
    } else {
        Q_ASSERT(dev && context);
        featureLevel = dev->GetFeatureLevel();
    }

    HRESULT hr = CreateDXGIFactory2(0, IID_IDXGIFactory2, reinterpret_cast<void **>(&dxgiFactory));
    if (FAILED(hr)) {
        qWarning("Failed to create DXGI factory: %s", qPrintable(comErrorMessage(hr)));
        return;
    }
}

void QRhiD3D11::destroy()
{
    if (!importedDevice) {
        if (context)
            context->Release();
        context = nullptr;
        if (dev)
            dev->Release();
        dev = nullptr;
    }
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
    Q_ASSERT(inPass);

    if (!srb)
        srb = ps->shaderResourceBindings;

    QD3D11GraphicsPipeline *psD = QRHI_RES(QD3D11GraphicsPipeline, ps);
    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);

    if (cbD->currentPipeline != ps || cbD->currentPipelineGeneration != psD->generation) {
        cbD->currentPipeline = ps;
        cbD->currentPipelineGeneration = psD->generation;

        QD3D11CommandBuffer::Command cmd;
        cmd.cmd = QD3D11CommandBuffer::Command::BindGraphicsPipeline;
        cmd.args.bindGraphicsPipeline.ps = psD;
        cmd.args.bindGraphicsPipeline.srb = QRHI_RES(QD3D11ShaderResourceBindings, srb);
        cbD->commands.append(cmd);
    }
}

void QRhiD3D11::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<QRhi::VertexInput> &bindings,
                               QRhiBuffer *indexBuf, quint32 indexOffset, QRhi::IndexFormat indexFormat)
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
        Q_ASSERT(buf->usage.testFlag(QRhiBuffer::VertexBuffer));
        cmd.args.bindVertexBuffers.buffers[i] = bufD->buffer;
        cmd.args.bindVertexBuffers.offsets[i] = ofs;
        cmd.args.bindVertexBuffers.strides[i] = cbD->currentPipeline->vertexInputLayout.bindings[i].stride;
    }
    cbD->commands.append(cmd);

    if (indexBuf) {
        QD3D11Buffer *ibufD = QRHI_RES(QD3D11Buffer, indexBuf);
        Q_ASSERT(indexBuf->usage.testFlag(QRhiBuffer::IndexBuffer));
        QD3D11CommandBuffer::Command cmd;
        cmd.cmd = QD3D11CommandBuffer::Command::BindIndexBuffer;
        cmd.args.bindIndexBuffer.buffer = ibufD->buffer;
        cmd.args.bindIndexBuffer.offset = indexOffset;
        cmd.args.bindIndexBuffer.format = indexFormat == QRhi::IndexUInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        cbD->commands.append(cmd);
    }
}

void QRhiD3D11::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::Viewport;
    cmd.args.viewport.x = viewport.r.x();
    cmd.args.viewport.y = viewport.r.y();
    cmd.args.viewport.w = viewport.r.width();
    cmd.args.viewport.h = viewport.r.height();
    cmd.args.viewport.d0 = viewport.minDepth;
    cmd.args.viewport.d1 = viewport.maxDepth;
    QRHI_RES(QD3D11CommandBuffer, cb)->commands.append(cmd);
}

void QRhiD3D11::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
    Q_ASSERT(inPass);
    QD3D11CommandBuffer::Command cmd;
    cmd.cmd = QD3D11CommandBuffer::Command::Scissor;
    cmd.args.scissor.x = scissor.r.x();
    cmd.args.scissor.y = scissor.r.y();
    cmd.args.scissor.w = scissor.r.width();
    cmd.args.scissor.h = scissor.r.height();
    QRHI_RES(QD3D11CommandBuffer, cb)->commands.append(cmd);
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

void QRhiD3D11::applyPassUpdates(QRhiCommandBuffer *cb, const QRhi::PassUpdates &updates)
{
    Q_UNUSED(cb);

    for (const QRhi::DynamicBufferUpdate &u : updates.dynamicBufferUpdates) {
        Q_ASSERT(!u.buf->isStatic());
        QD3D11Buffer *bufD = QRHI_RES(QD3D11Buffer, u.buf);
        memcpy(bufD->dynBuf.data() + u.offset, u.data.constData(), u.data.size());
        bufD->hasPendingDynamicUpdates = true;
    }

    for (const QRhi::StaticBufferUpload &u : updates.staticBufferUploads) {
        QD3D11Buffer *bufD = QRHI_RES(QD3D11Buffer, u.buf);
        Q_ASSERT(u.buf->isStatic());
        Q_ASSERT(u.data.size() == u.buf->size);
        // ### box cause size may not be multiple of 16?
        context->UpdateSubresource(bufD->buffer, 0, nullptr, u.data.constData(), 0, 0);
    }

    for (const QRhi::TextureUpload &u : updates.textureUploads) {
    }
}

void QRhiD3D11::beginPass(QRhiRenderTarget *rt, QRhiCommandBuffer *cb, const QRhiClearValue *clearValues,
                          const QRhi::PassUpdates &updates)
{
    Q_ASSERT(!inPass);

    applyPassUpdates(cb, updates);

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

    inPass = true;
}

void QRhiD3D11::endPass(QRhiCommandBuffer *cb)
{
    Q_ASSERT(inPass);
    inPass = false;

    QD3D11CommandBuffer *cbD = QRHI_RES(QD3D11CommandBuffer, cb);
    cbD->currentTarget = nullptr;
}

void QRhiD3D11::setUniformBuffers(QD3D11GraphicsPipeline *psD, QD3D11ShaderResourceBindings *srbD)
{
    QVarLengthArray<ID3D11Buffer *, 4> vsubufs;
    QVarLengthArray<ID3D11Buffer *, 4> fsubufs;
    // ### this assumes srb.bindings is sorted based on the binding index (and that there are no gaps)
    for (int i = 0, ie = srbD->bindings.count(); i != ie; ++i) {
        const QRhiShaderResourceBindings::Binding &b(srbD->bindings[i]);
        switch (b.type) {
        case QRhiShaderResourceBindings::Binding::UniformBuffer:
        {
            QD3D11Buffer *bufD = QRHI_RES(QD3D11Buffer, b.ubuf.buf);
            if (!bufD->isStatic() && bufD->hasPendingDynamicUpdates) {
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
            Q_ASSERT(b.ubuf.offset == 0 && b.ubuf.maybeSize == 0); // ### is this really unsupported?
            if (b.stage.testFlag(QRhiShaderResourceBindings::Binding::VertexStage))
                vsubufs.append(bufD->buffer);
            if (b.stage.testFlag(QRhiShaderResourceBindings::Binding::FragmentStage))
                fsubufs.append(bufD->buffer);
        }
            break;
        case QRhiShaderResourceBindings::Binding::SampledTexture:
            // ###
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
    if (!vsubufs.isEmpty())
        context->VSSetConstantBuffers(0, vsubufs.count(), vsubufs.constData());
    if (!fsubufs.isEmpty())
        context->PSSetConstantBuffers(0, fsubufs.count(), fsubufs.constData());
}

void QRhiD3D11::executeCommandBuffer(QD3D11CommandBuffer *cb)
{
    quint32 stencilRef = 0;
    float blendConstants[] = { 1, 1, 1, 1 };

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
                                      cmd.args.bindIndexBuffer.format);
            break;
        case QD3D11CommandBuffer::Command::BindGraphicsPipeline:
        {
            QD3D11GraphicsPipeline *psD = cmd.args.bindGraphicsPipeline.ps;
            QD3D11ShaderResourceBindings *srbD = cmd.args.bindGraphicsPipeline.srb;
            context->VSSetShader(psD->vs, nullptr, 0);
            context->PSSetShader(psD->fs, nullptr, 0);
            context->IASetPrimitiveTopology(psD->d3dTopology);
            context->IASetInputLayout(psD->inputLayout);
            context->OMSetDepthStencilState(psD->dsState, stencilRef);
            context->OMSetBlendState(psD->blendState, blendConstants, 0xffffffff);
            context->RSSetState(psD->rastState);
            setUniformBuffers(psD, srbD);
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

static inline uint aligned(uint v, uint byteAlign)
{
    return (v + byteAlign - 1) & ~(byteAlign - 1);
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
    desc.ByteWidth = usage.testFlag(QRhiBuffer::UniformBuffer) ? aligned(size, 16) : size;
    desc.Usage = isStatic() ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;
    desc.BindFlags = toD3DBufferUsage(usage);
    desc.CPUAccessFlags = isStatic() ? 0 : D3D11_CPU_ACCESS_WRITE;

    QRHI_RES_RHI(QRhiD3D11);
    HRESULT hr = rhiD->dev->CreateBuffer(&desc, nullptr, &buffer);
    if (FAILED(hr)) {
        qWarning("Failed to create buffer: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    if (!isStatic()) {
        dynBuf.resize(size);
        hasPendingDynamicUpdates = false;
    }

    return true;
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

    tex->Release();
    tex = nullptr;
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
    // nothing to do here
}

bool QD3D11ShaderResourceBindings::build()
{
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

static inline D3D11_CULL_MODE toD3DCullMode(QRhiGraphicsPipeline::CullMode mode)
{
    if (mode == 0)
        return D3D11_CULL_NONE;
    if (mode.testFlag(QRhiGraphicsPipeline::Front)) {
        if (mode.testFlag(QRhiGraphicsPipeline::Back)) {
            qWarning("Culling both front and back is not supported by Direct 3D");
            return D3D11_CULL_BACK;
        }
        return D3D11_CULL_FRONT;
    }
    return D3D11_CULL_BACK;
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
    case QRhiGraphicsPipeline::TriangleFan:
        qWarning("Triangle fans are not supported by Direct 3D");
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
    rastDesc.CullMode = toD3DCullMode(cullMode);
    rastDesc.FrontCounterClockwise = frontFace == CCW;
    rastDesc.ScissorEnable = true; // ###
    //rastDesc.MultisampleEnable;
    HRESULT hr = rhiD->dev->CreateRasterizerState(&rastDesc, &rastState);
    if (FAILED(hr)) {
        qWarning("Failed to create rasterizer state: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC dsDesc;
    memset(&dsDesc, 0, sizeof(dsDesc));
    dsDesc.DepthEnable = depthTest;
    dsDesc.DepthWriteMask = depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = toD3DCompareOp(depthOp);
    dsDesc.StencilEnable = stencilTest;
    if (stencilTest) {
        dsDesc.StencilReadMask = stencilReadMask;
        dsDesc.StencilWriteMask = stencilWriteMask;
        dsDesc.FrontFace.StencilFailOp = toD3DStencilOp(stencilFront.failOp);
        dsDesc.FrontFace.StencilDepthFailOp = toD3DStencilOp(stencilFront.depthFailOp);
        dsDesc.FrontFace.StencilPassOp = toD3DStencilOp(stencilFront.passOp);
        dsDesc.FrontFace.StencilFunc = toD3DCompareOp(stencilFront.compareOp);
        dsDesc.BackFace.StencilFailOp = toD3DStencilOp(stencilBack.failOp);
        dsDesc.BackFace.StencilDepthFailOp = toD3DStencilOp(stencilBack.depthFailOp);
        dsDesc.BackFace.StencilPassOp = toD3DStencilOp(stencilBack.passOp);
        dsDesc.BackFace.StencilFunc = toD3DCompareOp(stencilBack.compareOp);
    }
    hr = rhiD->dev->CreateDepthStencilState(&dsDesc, &dsState);
    if (FAILED(hr)) {
        qWarning("Failed to create depth-stencil state: %s", qPrintable(comErrorMessage(hr)));
        return false;
    }

    D3D11_BLEND_DESC blendDesc;
    memset(&blendDesc, 0, sizeof(blendDesc));
    blendDesc.IndependentBlendEnable = targetBlends.count() > 1;
    for (int i = 0, ie = targetBlends.count(); i != ie; ++i) {
        const QRhiGraphicsPipeline::TargetBlend &b(targetBlends[i]);
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
    if (targetBlends.isEmpty()) {
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
    for (const QRhiGraphicsShaderStage &shaderStage : qAsConst(shaderStages)) {
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

    d3dTopology = toD3DTopology(topology);

    if (!vsByteCode.isEmpty()) {
        QVarLengthArray<D3D11_INPUT_ELEMENT_DESC, 4> inputDescs;
        for (const QRhiVertexInputLayout::Attribute &attribute : vertexInputLayout.attributes) {
            D3D11_INPUT_ELEMENT_DESC desc;
            memset(&desc, 0, sizeof(desc));
            // the output from SPIRV-Cross uses TEXCOORD<location> as the semantic
            desc.SemanticName = "TEXCOORD";
            desc.SemanticIndex = attribute.location;
            desc.Format = toD3DAttributeFormat(attribute.format);
            desc.InputSlot = attribute.binding;
            desc.AlignedByteOffset = attribute.offset;
            const QRhiVertexInputLayout::Binding &binding(vertexInputLayout.bindings[attribute.binding]);
            desc.InputSlotClass = binding.classification == QRhiVertexInputLayout::Binding::PerInstance
                    ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
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