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

#include <d3d11_1.h>
#include <dxgi1_3.h>

QT_BEGIN_NAMESPACE

struct QD3D11Buffer : public QRhiBuffer
{
    QD3D11Buffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    void release() override;
    bool build() override;

    ID3D11Buffer *buffer = nullptr;
    QByteArray dynBuf;
    bool hasPendingDynamicUpdates = false;
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11RenderBuffer : public QRhiRenderBuffer
{
    QD3D11RenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                       int sampleCount, QRhiRenderBuffer::Flags flags);
    void release() override;
    bool build() override;

    ID3D11Texture2D *tex = nullptr;
    ID3D11DepthStencilView *dsv = nullptr;
    friend class QRhiD3D11;
};

struct QD3D11Texture : public QRhiTexture
{
    QD3D11Texture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                  int sampleCount, Flags flags);
    void release() override;
    bool build() override;

    ID3D11Texture2D *tex = nullptr;
    ID3D11ShaderResourceView *srv = nullptr;
    uint mipLevelCount = 0;
    DXGI_SAMPLE_DESC sampleDesc;
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11Sampler : public QRhiSampler
{
    QD3D11Sampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                  AddressMode u, AddressMode v, AddressMode w);
    void release() override;
    bool build() override;

    ID3D11SamplerState *samplerState = nullptr;
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11RenderPassDescriptor : public QRhiRenderPassDescriptor
{
    QD3D11RenderPassDescriptor(QRhiImplementation *rhi);
    void release() override;
};

struct QD3D11BasicRenderTargetData
{
    QD3D11BasicRenderTargetData(QRhiImplementation *)
    {
        for (int i = 0; i < MAX_COLOR_ATTACHMENTS; ++i)
            rtv[i] = nullptr;
    }

    QD3D11RenderPassDescriptor *rp = nullptr;
    QSize pixelSize;
    int colorAttCount = 0;
    int dsAttCount = 0;

    static const int MAX_COLOR_ATTACHMENTS = 8;
    ID3D11RenderTargetView *rtv[MAX_COLOR_ATTACHMENTS];
    ID3D11DepthStencilView *dsv = nullptr;
};

struct QD3D11ReferenceRenderTarget : public QRhiReferenceRenderTarget
{
    QD3D11ReferenceRenderTarget(QRhiImplementation *rhi);
    void release() override;
    Type type() const override;
    QSize sizeInPixels() const override;

    QD3D11BasicRenderTargetData d;
};

struct QD3D11TextureRenderTarget : public QRhiTextureRenderTarget
{
    QD3D11TextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags);
    void release() override;

    Type type() const override;
    QSize sizeInPixels() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool build() override;

    QD3D11BasicRenderTargetData d;
    ID3D11RenderTargetView *rtv[QD3D11BasicRenderTargetData::MAX_COLOR_ATTACHMENTS];
    bool ownsDsv = false;
    ID3D11DepthStencilView *dsv = nullptr;
    friend class QRhiD3D11;
};

struct QD3D11ShaderResourceBindings : public QRhiShaderResourceBindings
{
    QD3D11ShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    QVector<QRhiShaderResourceBinding> sortedBindings;
    uint generation = 0;

    // Keep track of the generation number of each referenced QRhi* to be able
    // to detect that the batched bindings are out of date.
    struct BoundUniformBufferData {
        uint generation;
    };
    struct BoundSampledTextureData {
        uint texGeneration;
        uint samplerGeneration;
    };
    struct BoundResourceData {
        union {
            BoundUniformBufferData ubuf;
            BoundSampledTextureData stex;
        };
    };
    QVector<BoundResourceData> boundResourceData;

    QRhiBatchedBindings<ID3D11Buffer *> vsubufs;
    QRhiBatchedBindings<UINT> vsubufoffsets;
    QRhiBatchedBindings<UINT> vsubufsizes;

    QRhiBatchedBindings<ID3D11Buffer *> fsubufs;
    QRhiBatchedBindings<UINT> fsubufoffsets;
    QRhiBatchedBindings<UINT> fsubufsizes;

    QRhiBatchedBindings<ID3D11SamplerState *> vssamplers;
    QRhiBatchedBindings<ID3D11ShaderResourceView *> vsshaderresources;

    QRhiBatchedBindings<ID3D11SamplerState *> fssamplers;
    QRhiBatchedBindings<ID3D11ShaderResourceView *> fsshaderresources;

    friend class QRhiD3D11;
};

Q_DECLARE_TYPEINFO(QD3D11ShaderResourceBindings::BoundResourceData, Q_MOVABLE_TYPE);

struct QD3D11GraphicsPipeline : public QRhiGraphicsPipeline
{
    QD3D11GraphicsPipeline(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    ID3D11DepthStencilState *dsState = nullptr;
    ID3D11BlendState *blendState = nullptr;
    ID3D11VertexShader *vs = nullptr;
    ID3D11PixelShader *fs = nullptr;
    ID3D11InputLayout *inputLayout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ID3D11RasterizerState *rastState = nullptr;
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11SwapChain;

struct QD3D11CommandBuffer : public QRhiCommandBuffer
{
    QD3D11CommandBuffer(QRhiImplementation *rhi);
    void release() override;

    // Technically it's not like we really need to queue up the commands, but
    // have it this way since it helps keeping things concise and may become
    // essential if "command buffers" become application creatable some day.
    struct Command {
        enum Cmd {
            SetRenderTarget,
            Clear,
            Viewport,
            Scissor,
            BindVertexBuffers,
            BindIndexBuffer,
            BindGraphicsPipeline,
            StencilRef,
            BlendConstants,
            Draw,
            DrawIndexed,
            UpdateSubRes,
            CopySubRes
        };
        enum ClearFlag { Color = 1, Depth = 2, Stencil = 4 };
        Cmd cmd;
        union {
            struct {
                QRhiRenderTarget *rt;
            } setRenderTarget;
            struct {
                QRhiRenderTarget *rt;
                int mask;
                float c[4];
                float d;
                quint32 s;
            } clear;
            struct {
                float x, y, w, h;
                float d0, d1;
            } viewport;
            struct {
                int x, y, w, h;
            } scissor;
            struct {
                int startSlot;
                int slotCount;
                ID3D11Buffer *buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
                UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
                UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
            } bindVertexBuffers;
            struct {
                ID3D11Buffer *buffer;
                quint32 offset;
                DXGI_FORMAT format;
            } bindIndexBuffer;
            struct {
                QD3D11GraphicsPipeline *ps;
                QD3D11ShaderResourceBindings *srb;
                bool resOnlyChange;
            } bindGraphicsPipeline;
            struct {
                QD3D11GraphicsPipeline *ps;
                quint32 ref;
            } stencilRef;
            struct {
                QD3D11GraphicsPipeline *ps;
                float c[4];
            } blendConstants;
            struct {
                QD3D11GraphicsPipeline *ps;
                quint32 vertexCount;
                quint32 instanceCount;
                quint32 firstVertex;
                quint32 firstInstance;
            } draw;
            struct {
                QD3D11GraphicsPipeline *ps;
                quint32 indexCount;
                quint32 instanceCount;
                quint32 firstIndex;
                qint32 vertexOffset;
                quint32 firstInstance;
            } drawIndexed;
            struct {
                ID3D11Resource *dst;
                UINT dstSubRes;
                bool hasDstBox;
                D3D11_BOX dstBox;
                const void *src; // must come from retain*()
                UINT srcRowPitch;
            } updateSubRes;
            struct {
                ID3D11Resource *dst;
                UINT dstSubRes;
                UINT dstX;
                UINT dstY;
                ID3D11Resource *src;
                UINT srcSubRes;
                bool hasSrcBox;
                D3D11_BOX srcBox;
            } copySubRes;
        } args;
    };

    QVector<Command> commands;
    QRhiRenderTarget *currentTarget;
    QRhiGraphicsPipeline *currentPipeline;
    uint currentPipelineGeneration;
    QRhiShaderResourceBindings *currentSrb;
    uint currentSrbGeneration;

    QVector<QByteArray> dataRetainPool;
    QVector<QImage> imageRetainPool;

    // relies heavily on implicit sharing (no copies of the actual data will be made)
    const uchar *retainData(const QByteArray &data) {
        dataRetainPool.append(data);
        return reinterpret_cast<const uchar *>(dataRetainPool.constLast().constData());
    }
    const uchar *retainImage(const QImage &image) {
        imageRetainPool.append(image);
        return imageRetainPool.constLast().constBits();
    }
    void resetCommands() {
        commands.clear();
        dataRetainPool.clear();
        imageRetainPool.clear();
    }
    void resetState() {
        resetCommands();
        currentTarget = nullptr;
        currentPipeline = nullptr;
        currentPipelineGeneration = 0;
        currentSrb = nullptr;
        currentSrbGeneration = 0;
    }
};

Q_DECLARE_TYPEINFO(QD3D11CommandBuffer::Command, Q_MOVABLE_TYPE);

struct QD3D11SwapChain : public QRhiSwapChain
{
    QD3D11SwapChain(QRhiImplementation *rhi);
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;

    QSize effectivePixelSize() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool buildOrResize();

    void releaseBuffers();
    bool newColorBuffer(const QSize &size, DXGI_FORMAT format, DXGI_SAMPLE_DESC sampleDesc,
                        ID3D11Texture2D **tex, ID3D11RenderTargetView **rtv) const;

    QWindow *window = nullptr;
    QSize pixelSize;
    QD3D11ReferenceRenderTarget rt;
    QD3D11CommandBuffer cb;
    DXGI_FORMAT colorFormat;
    IDXGISwapChain1 *swapChain = nullptr;
    static const int BUFFER_COUNT = 2;
    ID3D11Texture2D *tex[BUFFER_COUNT];
    ID3D11RenderTargetView *rtv[BUFFER_COUNT];
    ID3D11Texture2D *msaaTex[BUFFER_COUNT];
    ID3D11RenderTargetView *msaaRtv[BUFFER_COUNT];
    DXGI_SAMPLE_DESC sampleDesc;
    int currentFrame = 0;
    QD3D11RenderBuffer *ds = nullptr;
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
                                         QRhiRenderBuffer::Flags flags) override;
    QRhiTexture *createTexture(QRhiTexture::Format format,
                               const QSize &pixelSize,
                               int sampleCount,
                               QRhiTexture::Flags flags) override;
    QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                               QRhiSampler::Filter mipmapMode,
                               QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w) override;

    QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                       QRhiTextureRenderTarget::Flags flags) override;

    QRhiSwapChain *createSwapChain() override;
    QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain) override;
    QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain) override;
    QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb) override;
    QRhi::FrameOpResult endOffscreenFrame() override;
    QRhi::FrameOpResult finish() override;

    void resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;

    void beginPass(QRhiCommandBuffer *cb,
                   QRhiRenderTarget *rt,
                   const QRhiColorClearValue &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates) override;
    void endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;

    void setGraphicsPipeline(QRhiCommandBuffer *cb,
                             QRhiGraphicsPipeline *ps,
                             QRhiShaderResourceBindings *srb) override;

    void setVertexInput(QRhiCommandBuffer *cb,
                        int startBinding, const QVector<QRhiCommandBuffer::VertexInput> &bindings,
                        QRhiBuffer *indexBuf, quint32 indexOffset,
                        QRhiCommandBuffer::IndexFormat indexFormat) override;

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
    bool isYUpInFramebuffer() const override;
    QMatrix4x4 clipSpaceCorrMatrix() const override;
    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const override;

    void create();
    void destroy();
    void enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates);
    void updateShaderResourceBindings(QD3D11ShaderResourceBindings *srbD);
    void executeBufferHostWritesForCurrentFrame(QD3D11Buffer *bufD);
    void setShaderResources(QD3D11ShaderResourceBindings *srbD);
    void executeCommandBuffer(QD3D11CommandBuffer *cbD);
    DXGI_SAMPLE_DESC effectiveSampleCount(int sampleCount) const;
    void finishActiveReadbacks();

    bool debugLayer = false;
    bool importedDevice = false;
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext1 *context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    IDXGIFactory2 *dxgiFactory = nullptr;

    bool inFrame = false;
    int finishedFrameCount = 0;
    bool inPass = false;

    struct {
        int vsLastActiveSrvBinding = 0;
        int fsLastActiveSrvBinding = 0;
        QD3D11SwapChain *currentSwapChain = nullptr;
    } contextState;

    struct OffscreenFrame {
        OffscreenFrame(QRhiImplementation *rhi) : cbWrapper(rhi) { }
        bool active = false;
        QD3D11CommandBuffer cbWrapper;
    } ofr;

    struct ActiveReadback {
        QRhiReadbackDescription desc;
        QRhiReadbackResult *result;
        ID3D11Texture2D *stagingTex;
        quint32 bufSize;
        QSize pixelSize;
        QRhiTexture::Format format;
    };
    QVector<ActiveReadback> activeReadbacks;
};

Q_DECLARE_TYPEINFO(QRhiD3D11::ActiveReadback, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
