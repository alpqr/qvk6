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

#ifndef QRHIGLES2_P_H
#define QRHIGLES2_P_H

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

#include "qrhigles2.h"
#include "qrhi_p.h"
#include <QOpenGLFunctions>
#include <QShaderDescription>

QT_BEGIN_NAMESPACE

struct QGles2Buffer : public QRhiBuffer
{
    QGles2Buffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    void release() override;
    bool build() override;

    GLuint buffer = 0;
    GLenum target;
    QByteArray ubuf;
    struct ChangeRange {
        ChangeRange(int b = -1, int e = -1)
            : changeBegin(b), changeEnd(e)
        { }
        bool isNull() const { return changeBegin == -1 && changeEnd == -1; }
        int changeBegin;
        int changeEnd;
    };
    ChangeRange ubufChangeRange;
    friend class QRhiGles2;
};

struct QGles2RenderBuffer : public QRhiRenderBuffer
{
    QGles2RenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                       int sampleCount, QRhiRenderBuffer::Hints hints);
    void release() override;
    bool build() override;

    GLuint renderbuffer = 0;
    friend class QRhiGles2;
};

struct QGles2Texture : public QRhiTexture
{
    QGles2Texture(QRhiImplementation *rhi, Format format, const QSize &pixelSize, Flags flags);
    void release() override;
    bool build() override;

    GLuint texture = 0;
    GLenum target;
    GLenum glintformat;
    GLenum glformat;
    GLenum gltype;
    friend class QRhiGles2;
};

struct QGles2Sampler : public QRhiSampler
{
    QGles2Sampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                  AddressMode u, AddressMode v, AddressMode w);
    void release() override;
    bool build() override;

    GLenum glminfilter;
    GLenum glmagfilter;
    GLenum glwraps;
    GLenum glwrapt;
    GLenum glwrapr;
    friend class QRhiGles2;
};

struct QGles2RenderPassDescriptor : public QRhiRenderPassDescriptor
{
    QGles2RenderPassDescriptor(QRhiImplementation *rhi);
    void release() override;
};

struct QGles2BasicRenderTargetData
{
    QGles2BasicRenderTargetData(QRhiImplementation *) { }

    QGles2RenderPassDescriptor *rp = nullptr;
    QSize pixelSize;
    int attCount;
};

struct QGles2ReferenceRenderTarget : public QRhiReferenceRenderTarget
{
    QGles2ReferenceRenderTarget(QRhiImplementation *rhi);
    void release() override;
    Type type() const override;
    QSize sizeInPixels() const override;

    QGles2BasicRenderTargetData d;
};

struct QGles2TextureRenderTarget : public QRhiTextureRenderTarget
{
    QGles2TextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags);
    void release() override;

    Type type() const override;
    QSize sizeInPixels() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool build() override;

    QGles2BasicRenderTargetData d;
    GLuint framebuffer = 0;
    friend class QRhiGles2;
};

struct QGles2ShaderResourceBindings : public QRhiShaderResourceBindings
{
    QGles2ShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    uint generation = 0;
    friend class QRhiGles2;
};

struct QGles2GraphicsPipeline : public QRhiGraphicsPipeline
{
    QGles2GraphicsPipeline(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    GLuint program = 0;
    GLenum drawMode = GL_TRIANGLES;
    uint generation = 0;
    QShaderDescription vsDesc;
    QShaderDescription fsDesc;

    struct Uniform {
        QShaderDescription::VarType type;
        int glslLocation;
        int binding;
        uint offset;
        QByteArray data;
    };
    QVector<Uniform> uniforms;

    struct Sampler {
        int glslLocation;
        int binding;
    };
    QVector<Sampler> samplers;

    friend class QRhiGles2;
};

Q_DECLARE_TYPEINFO(QGles2GraphicsPipeline::Uniform, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QGles2GraphicsPipeline::Sampler, Q_MOVABLE_TYPE);

struct QGles2CommandBuffer : public QRhiCommandBuffer
{
    QGles2CommandBuffer(QRhiImplementation *rhi);
    void release() override;

    struct Command {
        enum Cmd {
            Viewport,
            Scissor,
            BlendConstants,
            StencilRef,
            BindVertexBuffer,
            BindIndexBuffer,
            Draw,
            DrawIndexed,
            BindGraphicsPipeline,
            BindFramebuffer,
            Clear
        };
        Cmd cmd;
        union {
            struct {
                float x, y, w, h;
                float d0, d1;
            } viewport;
            struct {
                int x, y, w, h;
            } scissor;
            struct {
                float r, g, b, a;
            } blendConstants;
            struct {
                quint32 ref;
                QRhiGraphicsPipeline *ps;
            } stencilRef;
            struct {
                QRhiGraphicsPipeline *ps;
                GLuint buffer;
                quint32 offset;
                int binding;
            } bindVertexBuffer;
            struct {
                GLuint buffer;
                quint32 offset;
                GLenum type;
            } bindIndexBuffer;
            struct {
                QRhiGraphicsPipeline *ps;
                quint32 vertexCount;
                quint32 firstVertex;
            } draw;
            struct {
                QRhiGraphicsPipeline *ps;
                quint32 indexCount;
                quint32 firstIndex;
            } drawIndexed;
            struct {
                QRhiGraphicsPipeline *ps;
                QRhiShaderResourceBindings *srb;
                bool resOnlyChange;
            } bindGraphicsPipeline;
            struct {
                GLbitfield mask;
                float c[4];
                float d;
                quint32 s;
            } clear;
            struct {
                QRhiTextureRenderTarget *rt;
            } bindFramebuffer;
        } args;
    };

    QVector<Command> commands;
    QRhiRenderTarget *currentTarget;
    QRhiGraphicsPipeline *currentPipeline;
    uint currentPipelineGeneration;
    QRhiShaderResourceBindings *currentSrb;
    uint currentSrbGeneration;

    void resetState() {
        commands.clear();
        currentTarget = nullptr;
        currentPipeline = nullptr;
        currentPipelineGeneration = 0;
        currentSrb = nullptr;
        currentSrbGeneration = 0;
    }
};

Q_DECLARE_TYPEINFO(QGles2CommandBuffer::Command, Q_MOVABLE_TYPE);

struct QGles2SwapChain : public QRhiSwapChain
{
    QGles2SwapChain(QRhiImplementation *rhi);
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;

    QSize effectiveSizeInPixels() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool buildOrResize() override;

    QSurface *surface = nullptr;
    QSize pixelSize;
    QGles2ReferenceRenderTarget rt;
    QGles2CommandBuffer cb;
};

class QRhiGles2 : public QRhiImplementation
{
public:
    QRhiGles2(QRhiInitParams *params);
    ~QRhiGles2();

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
                               QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w) override;

    QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                       QRhiTextureRenderTarget::Flags flags) override;

    QRhiSwapChain *createSwapChain() override;
    QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain) override;
    QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain) override;

    void beginPass(QRhiRenderTarget *rt,
                   QRhiCommandBuffer *cb,
                   const QRhiColorClearValue &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates) override;
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
    bool isYUpInFramebuffer() const override;
    QMatrix4x4 clipSpaceCorrMatrix() const override;

    void ensureContext(QSurface *surface = nullptr);
    void create();
    void destroy();
    void executeDeferredReleases();
    void commitResourceUpdates(QRhiResourceUpdateBatch *resourceUpdates);
    void executeCommandBuffer(QRhiCommandBuffer *cb);
    void executeBindGraphicsPipeline(QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb);
    void setChangedUniforms(QGles2GraphicsPipeline *psD, QRhiShaderResourceBindings *srb, bool changedOnly);

    QOpenGLContext *ctx = nullptr;
    QWindow *maybeWindow = nullptr;
    QSurface *fallbackSurface = nullptr;
    bool buffersSwapped = false;
    QOpenGLFunctions *f = nullptr;
    bool inFrame = false;
    int finishedFrameCount = 0;
    bool inPass = false;

    struct DeferredReleaseEntry {
        enum Type {
            Buffer,
            Pipeline,
            Texture,
            RenderBuffer,
            TextureRenderTarget
        };
        Type type;
        union {
            struct {
                GLuint buffer;
            } buffer;
            struct {
                GLuint program;
            } pipeline;
            struct {
                GLuint texture;
            } texture;
            struct {
                GLuint renderbuffer;
            } renderbuffer;
            struct {
                GLuint framebuffer;
            } textureRenderTarget;
        };
    };
    QVector<DeferredReleaseEntry> releaseQueue;
};

Q_DECLARE_TYPEINFO(QRhiGles2::DeferredReleaseEntry, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
