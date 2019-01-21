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
#include <qopengl.h>
#include <QSurface>
#include <QShaderDescription>

QT_BEGIN_NAMESPACE

class QOpenGLExtensions;

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
                       int sampleCount, QRhiRenderBuffer::Flags flags);
    void release() override;
    bool build() override;
    QRhiTexture::Format backingFormat() const override;

    GLuint renderbuffer = 0;
    int samples;
    friend class QRhiGles2;
};

struct QGles2Texture : public QRhiTexture
{
    QGles2Texture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                  int sampleCount, Flags flags);
    void release() override;
    bool build() override;
    bool buildFrom(const QRhiNativeHandles *src) override;
    const QRhiNativeHandles *nativeHandles() override;

    bool prepareBuild(QSize *adjustedSize = nullptr);

    GLuint texture = 0;
    bool owns = true;
    GLenum target;
    GLenum glintformat;
    GLenum glformat;
    GLenum gltype;
    bool specified = false;
    int mipLevelCount = 0;
    QRhiGles2TextureNativeHandles nativeHandlesStruct;

    uint generation = 0;
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

    uint generation = 0;
    friend class QRhiGles2;
};

struct QGles2RenderPassDescriptor : public QRhiRenderPassDescriptor
{
    QGles2RenderPassDescriptor(QRhiImplementation *rhi);
    void release() override;
};

struct QGles2RenderTargetData
{
    QGles2RenderTargetData(QRhiImplementation *) { }

    QGles2RenderPassDescriptor *rp = nullptr;
    QSize pixelSize;
    float dpr = 1;
    int attCount = 0;
};

struct QGles2ReferenceRenderTarget : public QRhiReferenceRenderTarget
{
    QGles2ReferenceRenderTarget(QRhiImplementation *rhi);
    void release() override;
    Type type() const override;
    QSize sizeInPixels() const override;
    float devicePixelRatio() const override;

    QGles2RenderTargetData d;
};

struct QGles2TextureRenderTarget : public QRhiTextureRenderTarget
{
    QGles2TextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags);
    void release() override;

    Type type() const override;
    QSize sizeInPixels() const override;
    float devicePixelRatio() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool build() override;

    QGles2RenderTargetData d;
    GLuint framebuffer = 0;
    friend class QRhiGles2;
};

struct QGles2ShaderResourceBindings : public QRhiShaderResourceBindings
{
    QGles2ShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    struct BoundSampledTextureData {
        uint texGeneration;
        uint samplerGeneration;
    };
    struct BoundResourceData {
        union {
            BoundSampledTextureData stex;
        };
    };
    QVector<BoundResourceData> boundResourceData;

    uint generation = 0;
    friend class QRhiGles2;
};

Q_DECLARE_TYPEINFO(QGles2ShaderResourceBindings::BoundResourceData, Q_MOVABLE_TYPE);

struct QGles2GraphicsPipeline : public QRhiGraphicsPipeline
{
    QGles2GraphicsPipeline(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    GLuint program = 0;
    GLenum drawMode = GL_TRIANGLES;
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

    uint generation = 0;
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
            Clear,
            BufferData,
            BufferSubData,
            CopyTex,
            ReadPixels,
            SubImage,
            CompressedImage,
            CompressedSubImage,
            BlitFromRenderbuffer,
            GenMip
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
            struct {
                GLenum target;
                GLuint buffer;
                int offset;
                int size;
                const void *data; // must come from retainData()
            } bufferSubData;
            struct {
                GLenum srcFaceTarget;
                GLuint srcTexture;
                int srcLevel;
                int srcX;
                int srcY;
                QGles2Texture *dst;
                GLenum dstFaceTarget;
                int dstLevel;
                int dstX;
                int dstY;
                int w;
                int h;
            } copyTex;
            struct {
                QRhiReadbackResult *result;
                QGles2Texture *texture;
                int layer;
                int level;
            } readPixels;
            struct {
                QGles2Texture *dst;
                GLenum faceTarget;
                int level;
                int dx;
                int dy;
                int w;
                int h;
                GLenum glformat;
                GLenum gltype;
                const void *data; // must come from retainImage()
            } subImage;
            struct {
                QGles2Texture *dst;
                GLenum faceTarget;
                int level;
                GLenum glintformat;
                int w;
                int h;
                int size;
                const void *data; // must come from retainData()
            } compressedImage;
            struct {
                QGles2Texture *dst;
                GLenum faceTarget;
                int level;
                int dx;
                int dy;
                int w;
                int h;
                GLenum glintformat;
                int size;
                const void *data; // must come from retainData()
            } compressedSubImage;
            struct {
                GLuint renderbuffer;
                int w;
                int h;
                QGles2Texture *dst;
                int dstLayer;
                int dstLevel;
            } blitFromRb;
            struct {
                QGles2Texture *tex;
            } genMip;
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
    const void *retainData(const QByteArray &data) {
        dataRetainPool.append(data);
        return dataRetainPool.constLast().constData();
    }
    const void *retainImage(const QImage &image) {
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

Q_DECLARE_TYPEINFO(QGles2CommandBuffer::Command, Q_MOVABLE_TYPE);

struct QGles2SwapChain : public QRhiSwapChain
{
    QGles2SwapChain(QRhiImplementation *rhi);
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;

    QSize surfacePixelSize() override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool buildOrResize() override;

    QSurface *surface = nullptr;
    QSize pixelSize;
    QGles2ReferenceRenderTarget rt;
    QGles2CommandBuffer cb;
    int frameCount = 0;
};

class QRhiGles2 : public QRhiImplementation
{
public:
    QRhiGles2(QRhiGles2InitParams *params, QRhiGles2NativeHandles *importDevice = nullptr);

    bool create(QRhi::Flags flags) override;
    void destroy() override;

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

    void debugMarkBegin(QRhiCommandBuffer *cb, const QByteArray &name) override;
    void debugMarkEnd(QRhiCommandBuffer *cb) override;
    void debugMarkMsg(QRhiCommandBuffer *cb, const QByteArray &msg) override;

    QVector<int> supportedSampleCounts() const override;
    int ubufAlignment() const override;
    bool isYUpInFramebuffer() const override;
    QMatrix4x4 clipSpaceCorrMatrix() const override;
    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const override;
    bool isFeatureSupported(QRhi::Feature feature) const override;
    int resourceSizeLimit(QRhi::ResourceSizeLimit limit) const override;
    const QRhiNativeHandles *nativeHandles() override;

    bool ensureContext(QSurface *surface = nullptr) const;
    void executeDeferredReleases();
    void enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates);
    void executeCommandBuffer(QRhiCommandBuffer *cb);
    void executeBindGraphicsPipeline(QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb);
    void setChangedUniforms(QGles2GraphicsPipeline *psD, QRhiShaderResourceBindings *srb, bool changedOnly);

    QOpenGLContext *ctx = nullptr;
    bool importedContext = false;
    QWindow *maybeWindow = nullptr;
    QSurface *fallbackSurface = nullptr;
    mutable bool buffersSwapped = false;
    QOpenGLExtensions *f = nullptr;
    struct {
        // Multisample fb and blit are supported (GLES 3.0 or OpenGL 3.x). Not
        // the same as multisample textures!
        bool msaaRenderBuffer = false;
        int maxTextureSize = 2048;
    } caps;
    bool inFrame = false;
    bool inPass = false;
    QGles2SwapChain *currentSwapChain = nullptr;
    QVector<GLint> supportedCompressedFormats;
    QRhiGles2NativeHandles nativeHandlesStruct;

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

    struct OffscreenFrame {
        OffscreenFrame(QRhiImplementation *rhi) : cbWrapper(rhi) { }
        bool active = false;
        QGles2CommandBuffer cbWrapper;
    } ofr;
};

Q_DECLARE_TYPEINFO(QRhiGles2::DeferredReleaseEntry, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
