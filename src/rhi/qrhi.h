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

#ifndef QRHI_H
#define QRHI_H

#include <QtRhi/qtrhiglobal.h>
#include <QVector4D>
#include <QVector2D>
#include <QSize>
#include <QMatrix4x4>
#include <QVector>
#include <QImage>
#include <QBakedShader>

QT_BEGIN_NAMESPACE

class QRhiImplementation;
class QWindow;

struct Q_RHI_EXPORT QRhiColorClearValue
{
    QRhiColorClearValue() : rgba(0, 0, 0, 1) { }
    explicit QRhiColorClearValue(const QVector4D &rgba_) : rgba(rgba_) { }
    QRhiColorClearValue(float r, float g, float b, float a) : rgba(r, g, b, a) { }
    QVector4D rgba;
};

Q_DECLARE_TYPEINFO(QRhiColorClearValue, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiDepthStencilClearValue
{
    QRhiDepthStencilClearValue() : d(1), s(0) { }
    QRhiDepthStencilClearValue(float d_, quint32 s_) : d(d_), s(s_) { }
    float d;
    quint32 s;
};

Q_DECLARE_TYPEINFO(QRhiDepthStencilClearValue, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiViewport
{
    QRhiViewport() { }
    // x,y is bottom-left, like in OpenGL, regardless of what isYUpInFramebuffer() says
    QRhiViewport(float x, float y, float w, float h, float minDepth_ = 0.0f, float maxDepth_ = 1.0f)
        : r(x, y, w, h), minDepth(minDepth_), maxDepth(maxDepth_)
    { }
    QVector4D r;
    float minDepth;
    float maxDepth;
};

Q_DECLARE_TYPEINFO(QRhiViewport, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiScissor
{
    QRhiScissor() { }
    // x,y is bottom-left, like in OpenGL, regardless of what isYUpInFramebuffer() says
    QRhiScissor(int x, int y, int w, int h)
        : r(x, y, w, h)
    { }
    QVector4D r;
};

Q_DECLARE_TYPEINFO(QRhiScissor, Q_MOVABLE_TYPE);

// should be mappable to D3D12_INPUT_ELEMENT_DESC + D3D12_VERTEX_BUFFER_VIEW...
struct Q_RHI_EXPORT QRhiVertexInputLayout
{
    struct Q_RHI_EXPORT Binding {
        enum Classification {
            PerVertex,
            PerInstance
        };
        Binding() { }
        Binding(quint32 stride_, Classification cls = PerVertex)
            : stride(stride_), classification(cls)
        { }
        // stride must be a multiple of 4
        quint32 stride; // if another api needs this in setVertexInput, make the cb store a ptr to the current ps and look up the stride via that
        Classification classification;
    };

    struct Q_RHI_EXPORT Attribute {
        enum Format {
            Float4,
            Float3,
            Float2,
            Float,
            UNormByte4,
            UNormByte2,
            UNormByte
        };
        Attribute() { }
        Attribute(int binding_, int location_, Format format_, quint32 offset_)
            : binding(binding_), location(location_), format(format_), offset(offset_)
        { }
        int binding;
        // With HLSL we assume the vertex shader uses TEXCOORD<location> as the
        // semantic for each input. Hence no separate semantic name and index.
        int location;
        Format format;
        quint32 offset;
    };

    QVector<Binding> bindings; // slots
    QVector<Attribute> attributes;
};

Q_DECLARE_TYPEINFO(QRhiVertexInputLayout::Binding, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiVertexInputLayout::Attribute, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiVertexInputLayout, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiGraphicsShaderStage
{
    enum Type {
        Vertex,
        Fragment,
        Geometry,
        TessellationControl, // Hull
        TessellationEvaluation // Domain
    };

    QRhiGraphicsShaderStage() { }
    QRhiGraphicsShaderStage(Type type_, const QBakedShader &shader_)
        : type(type_), shader(shader_)
    { }

    Type type;
    QBakedShader shader;
};

Q_DECLARE_TYPEINFO(QRhiGraphicsShaderStage, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiResource
{
public:
    virtual ~QRhiResource();
    virtual void release() = 0;
    void releaseAndDestroy();

protected:
    QRhiImplementation *rhi = nullptr;
    QRhiResource(QRhiImplementation *rhi_);
    Q_DISABLE_COPY(QRhiResource)
};

class Q_RHI_EXPORT QRhiBuffer : public QRhiResource
{
public:
    enum Type {
        Immutable, // data never changes after initial upload - under the hood typically in device local (GPU) memory
        Static,    // data changes infrequently - under the hood typically device local and updated via a separate, host visible staging buffer
        Dynamic    // data changes frequently - under the hood typically host visible
    };

    enum UsageFlag {
        VertexBuffer = 1 << 0,
        IndexBuffer = 1 << 1,
        UniformBuffer = 1 << 2
    };
    Q_DECLARE_FLAGS(UsageFlags, UsageFlag)

    Type type;
    UsageFlags usage;
    int size; // no restrictions here, up to the backend to round up if needed

    virtual bool build() = 0;

protected:
    QRhiBuffer(QRhiImplementation *rhi, Type type_, UsageFlags usage_, int size_);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiBuffer::UsageFlags)

class Q_RHI_EXPORT QRhiRenderBuffer : public QRhiResource
{
public:
    enum Type {
        DepthStencil
    };

    enum Hint {
        ToBeUsedWithSwapChainOnly = 1 << 0 // use implicit winsys buffers, don't create anything (GL)
    };
    Q_DECLARE_FLAGS(Hints, Hint)

    Type type;
    QSize pixelSize;
    int sampleCount;
    Hints hints;

    virtual bool build() = 0;

protected:
    QRhiRenderBuffer(QRhiImplementation *rhi, Type type_, const QSize &pixelSize_,
                     int sampleCount_, Hints hints_);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiRenderBuffer::Hints)

class Q_RHI_EXPORT QRhiTexture : public QRhiResource
{
public:
    enum Flag {
        RenderTarget = 1 << 0,
        ChangesInfrequently = 1 << 1, // hint for backend not to keep staging resources around
        CubeMap = 1 << 2,
        MipMapped = 1 << 3
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Format {
        RGBA8,
        BGRA8,
        R8,
        R16,

        D16,
        D32
    };

    Format format;
    QSize pixelSize;
    Flags flags;

    virtual bool build() = 0;

protected:
    QRhiTexture(QRhiImplementation *rhi, Format format_, const QSize &pixelSize_, Flags flags_);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTexture::Flags)

class Q_RHI_EXPORT QRhiSampler : public QRhiResource
{
public:
    enum Filter {
        Nearest,
        Linear
    };

    enum AddressMode {
        Repeat,
        ClampToEdge,
        Border,
        Mirror,
        MirrorOnce
    };

    Filter magFilter;
    Filter minFilter;
    Filter mipmapMode;
    AddressMode addressU;
    AddressMode addressV;

    virtual bool build() = 0;

protected:
    QRhiSampler(QRhiImplementation *rhi,
                Filter magFilter_, Filter minFilter_, Filter mipmapMode_, AddressMode u_, AddressMode v_);
};

class Q_RHI_EXPORT QRhiRenderPass : public QRhiResource
{
protected:
    QRhiRenderPass(QRhiImplementation *rhi);
};

class Q_RHI_EXPORT QRhiRenderTarget : public QRhiResource
{
public:
    enum Type {
        RtRef,
        RtTexture
    };

    virtual Type type() const = 0;
    virtual QSize sizeInPixels() const = 0;
    virtual const QRhiRenderPass *renderPass() const = 0;

protected:
    QRhiRenderTarget(QRhiImplementation *rhi);
};

class Q_RHI_EXPORT QRhiReferenceRenderTarget : public QRhiRenderTarget
{
protected:
    QRhiReferenceRenderTarget(QRhiImplementation *rhi);
};

class Q_RHI_EXPORT QRhiTextureRenderTarget : public QRhiRenderTarget
{
public:
    enum Flag {
        PreserveColorContents = 1 << 0
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QRhiTexture *texture;
    QRhiTexture *depthTexture;
    QRhiRenderBuffer *depthStencilBuffer;
    Flags flags;

    virtual bool build() = 0;

protected:
    QRhiTextureRenderTarget(QRhiImplementation *rhi,
                            QRhiTexture *texture_, Flags flags_);
    QRhiTextureRenderTarget(QRhiImplementation *rhi,
                            QRhiTexture *texture_, QRhiRenderBuffer *depthStencilBuffer_, Flags flags_);
    QRhiTextureRenderTarget(QRhiImplementation *rhi,
                            QRhiTexture *texture_, QRhiTexture *depthTexture_, Flags flags_);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTextureRenderTarget::Flags)

class Q_RHI_EXPORT QRhiShaderResourceBindings : public QRhiResource
{
public:
    struct Q_RHI_EXPORT Binding {
        enum Type {
            UniformBuffer,
            SampledTexture
        };

        enum StageFlag {
            VertexStage = 1 << 0,
            FragmentStage = 1 << 1,
            GeometryStage = 1 << 2,
            TessellationControlStage = 1 << 3,
            TessellationEvaluationStage = 1 << 4
        };
        Q_DECLARE_FLAGS(StageFlags, StageFlag)

        static Binding uniformBuffer(int binding_, StageFlags stage_, QRhiBuffer *buf_);

        // Bind a region only. Up to the user to ensure offset is aligned to ubufAlignment.
        static Binding uniformBuffer(int binding_, StageFlags stage_, QRhiBuffer *buf_, int offset_, int size_);

        static Binding sampledTexture(int binding_, StageFlags stage_, QRhiTexture *tex_, QRhiSampler *sampler_);

        int binding;
        StageFlags stage;
        Type type;
        struct UniformBufferData {
            QRhiBuffer *buf;
            int offset;
            int maybeSize;
        };
        struct SampledTextureData {
            QRhiTexture *tex;
            QRhiSampler *sampler;
        };
        union {
            UniformBufferData ubuf;
            SampledTextureData stex;
        };
    };

    QVector<Binding> bindings;

    virtual bool build() = 0;

protected:
    QRhiShaderResourceBindings(QRhiImplementation *rhi);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiShaderResourceBindings::Binding::StageFlags)
Q_DECLARE_TYPEINFO(QRhiShaderResourceBindings::Binding, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiGraphicsPipeline : public QRhiResource
{
public:
    enum Flag {
        UsesBlendConstants = 1 << 0,
        UsesStencilRef = 1 << 1,
        UsesScissor = 1 << 2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Topology {
        Triangles,
        TriangleStrip,
        Lines,
        LineStrip,
        Points
    };

    enum CullMode { // not a bitmask since some apis use a mask, some don't
        None,
        Front,
        Back
    };

    enum FrontFace {
        CCW,
        CW
    };

    enum ColorMaskComponent {
        R = 1 << 0,
        G = 1 << 1,
        B = 1 << 2,
        A = 1 << 3
    };
    Q_DECLARE_FLAGS(ColorMask, ColorMaskComponent)

    enum BlendFactor {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        SrcAlphaSaturate,
        Src1Color,
        OneMinusSrc1Color,
        Src1Alpha,
        OneMinusSrc1Alpha
    };

    enum BlendOp {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    struct TargetBlend {
        ColorMask colorWrite = ColorMask(0xF); // R | G | B | A
        bool enable = false;
        BlendFactor srcColor = One;
        BlendFactor dstColor = OneMinusSrcAlpha;
        BlendOp opColor = Add;
        BlendFactor srcAlpha = One;
        BlendFactor dstAlpha = OneMinusSrcAlpha;
        BlendOp opAlpha = Add;
    };

    enum CompareOp {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum StencilOp {
        StencilZero,
        Keep,
        Replace,
        IncrementAndClamp,
        DecrementAndClamp,
        Invert,
        IncrementAndWrap,
        DecrementAndWrap
    };

    struct StencilOpState {
        StencilOp failOp = Keep;
        StencilOp depthFailOp = Keep;
        StencilOp passOp = Keep;
        CompareOp compareOp = Always;
    };

    Flags flags;
    Topology topology = Triangles;
    CullMode cullMode = None;
    FrontFace frontFace = CCW;
    QVector<TargetBlend> targetBlends; // no blend when empty
    bool depthTest = false;
    bool depthWrite = false;
    CompareOp depthOp = Less;
    bool stencilTest = false;
    StencilOpState stencilFront;
    StencilOpState stencilBack;
    // use the same read (compare) and write masks for both faces (see d3d).
    // have the reference value dynamically settable.
    quint32 stencilReadMask = 0xFF;
    quint32 stencilWriteMask = 0xFF;
    int sampleCount = 1; // MSAA, swapchain+depthstencil must match
    QVector<QRhiGraphicsShaderStage> shaderStages;
    QRhiVertexInputLayout vertexInputLayout;
    QRhiShaderResourceBindings *shaderResourceBindings = nullptr; // must be built by the time ps' build() is called
    const QRhiRenderPass *renderPass = nullptr;

    virtual bool build() = 0;

protected:
    QRhiGraphicsPipeline(QRhiImplementation *rhi);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::ColorMask)
Q_DECLARE_TYPEINFO(QRhiGraphicsPipeline::TargetBlend, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiCommandBuffer : public QRhiResource
{
protected:
    QRhiCommandBuffer(QRhiImplementation *rhi);
};

class Q_RHI_EXPORT QRhiSwapChain : public QRhiResource
{
public:
    enum SurfaceImportFlag {
        SurfaceHasPreMulAlpha = 1 << 0,
        SurfaceHasNonPreMulAlpha = 1 << 1
    };
    Q_DECLARE_FLAGS(SurfaceImportFlags, SurfaceImportFlag)

    virtual QRhiCommandBuffer *currentFrameCommandBuffer() = 0;
    virtual QRhiRenderTarget *currentFrameRenderTarget() = 0;
    virtual const QRhiRenderPass *defaultRenderPass() const = 0;

    // Some backends use the requested size, others ignore it and get the actual
    // size on their own. Keep track of both - application logic will need the
    // requested size (to do their "if qwindow->size() * dpr != req.size then
    // rebuild_swapchain" logic) and the actual size as well (for all graphics
    // calculations like viewport).
    virtual QSize requestedSizeInPixels() const = 0;
    virtual QSize effectiveSizeInPixels() const = 0;

    virtual bool build(QWindow *window, const QSize &requestedPixelSize, SurfaceImportFlags flags,
                       QRhiRenderBuffer *depthStencil, int sampleCount) = 0;

    virtual bool build(QObject *target) = 0; // integrate with an existing swapchain, f.ex. QVulkanWindow

protected:
    QRhiSwapChain(QRhiImplementation *rhi);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiSwapChain::SurfaceImportFlags)

struct QRhiResourceUpdateBatchPrivate;

class Q_RHI_EXPORT QRhiResourceUpdateBatch // sort of a command buffer for copy type of operations
{
public:
    struct TextureUploadDescription {
        struct Layer {
            struct MipLevel {
                MipLevel() { }
                MipLevel(const QImage &image_) : image(image_) { }
                QImage image;
            };
            Layer() { }
            Layer(const QVector<MipLevel> &mipImages_) : mipImages(mipImages_) { }
            QVector<MipLevel> mipImages;
        };
        TextureUploadDescription() { }
        TextureUploadDescription(const QVector<Layer> &layers_) : layers(layers_) { }
        QVector<Layer> layers; // 6 layers for cubemaps, 1 otherwise
    };

    ~QRhiResourceUpdateBatch();
    // Puts the batch back to the pool without any processing.
    void release();

    // None of these execute anything. Deferred to beginPass. What exactly then
    // happens underneath is hidden from the applications.
    void updateDynamicBuffer(QRhiBuffer *buf, int offset, int size, const void *data);
    void uploadStaticBuffer(QRhiBuffer *buf, const void *data);
    void uploadTexture(QRhiTexture *tex, const TextureUploadDescription &desc);
    void uploadTexture(QRhiTexture *tex, const QImage &image);

private:
    QRhiResourceUpdateBatch(QRhiImplementation *rhi);
    Q_DISABLE_COPY(QRhiResourceUpdateBatch)
    QRhiResourceUpdateBatchPrivate *d;
    friend struct QRhiResourceUpdateBatchPrivate;
    friend class QRhi;
};

Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatch::TextureUploadDescription::Layer::MipLevel, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatch::TextureUploadDescription::Layer, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatch::TextureUploadDescription, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiInitParams
{
};

class Q_RHI_EXPORT QRhi
{
public:
    enum Implementation {
        Vulkan,
        OpenGLES2,
        D3D11,
        Metal
    };

    enum FrameOpResult {
        FrameOpSuccess = 0,
        FrameOpError,
        FrameOpSwapChainOutOfDate,
        FrameOpDeviceLost
    };

    enum IndexFormat {
        IndexUInt16,
        IndexUInt32
    };

    ~QRhi();

    static QRhi *create(Implementation impl, QRhiInitParams *params);

    /*
       The underlying graphics resources are created when calling build() and
       put on the release queue by release() (so this is safe even when the
       resource is used by the still executing/pending frame(s)).

       The QRhi* instance itself is not destroyed by the release and it is safe
       to destroy it right away after calling release().

       Changing any value needs explicit release and rebuilding of the
       underlying resource before it can take effect.

       res->build(); <change something>; res->release(); res->build(); ...
       is therefore perfectly valid and can be used to recreate things (when
       buffer or texture size changes f.ex.)

       In addition, just doing res->build(); ...; res->build() is valid too and
       has the same effect due to an implicit release() call made by build()
       when invoked on an object with valid resources underneath.
     */

    QRhiGraphicsPipeline *createGraphicsPipeline();
    QRhiShaderResourceBindings *createShaderResourceBindings();

    // Buffers are immutable like other resources but the underlying data can
    // change. (its size cannot) Having multiple frames in flight is handled
    // transparently, with multiple allocations, recording updates, etc.
    // internally. The underlying memory type may differ for static and dynamic
    // buffers. For best performance, static buffers may be copied to device
    // local (not necessarily host visible) memory via a staging (host visible)
    // buffer. Hence separate update-dynamic and upload-static operations.
    QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                             QRhiBuffer::UsageFlags usage,
                             int size);

    // To be used for depth-stencil when no access is needed afterwards.
    // Transient image, backed by lazily allocated memory (Vulkan, ideal for
    // tiled GPUs). May also be a dummy internally depending on the backend and
    // the hints (OpenGL, where the winsys interface provides the depth-stencil
    // buffer via the window surface).
    QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                         const QSize &pixelSize,
                                         int sampleCount = 1,
                                         QRhiRenderBuffer::Hints hints = QRhiRenderBuffer::Hints());

    QRhiTexture *createTexture(QRhiTexture::Format format,
                               const QSize &pixelSize,
                               QRhiTexture::Flags flags = QRhiTexture::Flags());

    QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                               QRhiSampler::Filter mipmapMode,
                               QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v);

    // color only
    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiTextureRenderTarget::Flags flags = QRhiTextureRenderTarget::Flags());
    // color and depth-stencil, only color accessed afterwards
    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiRenderBuffer *depthStencilBuffer,
                                                       QRhiTextureRenderTarget::Flags flags = QRhiTextureRenderTarget::Flags());
    // color and depth, both as textures accessible afterwards
    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiTexture *depthTexture,
                                                       QRhiTextureRenderTarget::Flags flags = QRhiTextureRenderTarget::Flags());

    /*
      Render to a QWindow (must be VulkanSurface):
        Create a swapchain.
        Call build() on the swapchain whenever the size is different than before.
        Call release() on QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed.
        Then on every frame:
           beginFrame(sc);
           beginPass(sc->currentFrameRenderTarget(), sc->currentFrameCommandBuffer(), clearValues, updates);
           ...
           endPass(sc->currentFrameCommandBuffer());
           endFrame(sc); // this queues the Present, begin/endFrame manages double buffering internally

      Also works with a QVulkanWindow from startNextFrame(). Use the overload of build() in initSwapChainResources().
     */
    QRhiSwapChain *createSwapChain();
    FrameOpResult beginFrame(QRhiSwapChain *swapChain);
    FrameOpResult endFrame(QRhiSwapChain *swapChain);

    // Returns an instance to which updates can be queued. Batch instances are
    // pooled and never owned by the application. An instance is returned to
    // the pool after a beginPass() processes it or when it is "canceled" by
    // calling release().
    QRhiResourceUpdateBatch *nextResourceUpdateBatch();

    void beginPass(QRhiRenderTarget *rt,
                   QRhiCommandBuffer *cb,
                   const QRhiColorClearValue &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates = nullptr);
    void endPass(QRhiCommandBuffer *cb);

    // When specified, srb can be different from ps' srb but the layouts must
    // match. Basic tracking is included: no command is added to the cb when
    // the pipeline or desc.set are the same as in the last call in the same
    // frame; srb is updated automatically at this point whenever a referenced
    // buffer, texture, etc. is out of date internally (due to rebuilding since
    // the creation of the srb) - hence no need to manually recreate the srb in
    // case a QRhiBuffer is "resized" etc.
    void setGraphicsPipeline(QRhiCommandBuffer *cb,
                             QRhiGraphicsPipeline *ps,
                             QRhiShaderResourceBindings *srb = nullptr);

    // The following functions (taking a command buffer) expect to have the
    // pipeline set already on the command buffer. Otherwise, unspecified
    // issues may arise depending on the backend.
    using VertexInput = QPair<QRhiBuffer *, quint32>; // buffer, offset
    void setVertexInput(QRhiCommandBuffer *cb,
                        int startBinding, const QVector<VertexInput> &bindings,
                        QRhiBuffer *indexBuf = nullptr, quint32 indexOffset = 0,
                        IndexFormat indexFormat = IndexUInt16);

    void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport);
    void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor);
    void setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c);
    void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue);

    void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
              quint32 instanceCount = 1, quint32 firstVertex = 0, quint32 firstInstance = 0);

    void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                     quint32 instanceCount = 1, quint32 firstIndex = 0,
                     qint32 vertexOffset = 0, quint32 firstInstance = 0);

    QVector<int> supportedSampleCounts() const;

    int ubufAlignment() const;
    int ubufAligned(int v) const;

    int mipLevelsForSize(const QSize &size) const;

    bool isYUpInFramebuffer() const;

    // Make Y up and allow using 0..1 as the depth range. This lets
    // applications keep using OpenGL-targeted vertex data and perspective
    // matrices regardless of the backend. (by passing this_matrix * mvp,
    // instead of just mvp, to their vertex shaders)
    QMatrix4x4 clipSpaceCorrMatrix() const;

protected:
    QRhi();

private:
    Q_DISABLE_COPY(QRhi)
    QRhiImplementation *d = nullptr;
};

QT_END_NAMESPACE

#endif
