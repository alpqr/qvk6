/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt VkRender module
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

#ifndef QVKRENDER_H
#define QVKRENDER_H

#include <QtVkRender/qtvkrglobal.h>
#include <QVector4D>
#include <QVector2D>
#include <QSize>
#include <QMatrix4x4>
#include <QVector>
#include <QImage>

QT_BEGIN_NAMESPACE

class QRhiPrivate;
class QVulkanWindow;

static const int QVK_FRAMES_IN_FLIGHT = 2;

struct Q_VKR_EXPORT QRhiClearValue
{
    QRhiClearValue() { }
    QRhiClearValue(const QVector4D &rgba_) : rgba(rgba_), isDepthStencil(false) { }
    QRhiClearValue(float d_, quint32 s_) : d(d_), s(s_), isDepthStencil(true) { }
    QVector4D rgba;
    float d;
    quint32 s;
    bool isDepthStencil;
};

struct Q_VKR_EXPORT QRhiViewport
{
    QRhiViewport() { }
    QRhiViewport(float x, float y, float w, float h, float minDepth_ = 0.0f, float maxDepth_ = 1.0f)
        : r(x, y, w, h), minDepth(minDepth_), maxDepth(maxDepth_)
    { }
    QRectF r;
    float minDepth;
    float maxDepth;
};

struct Q_VKR_EXPORT QRhiScissor
{
    QRhiScissor() { }
    QRhiScissor(float x, float y, float w, float h)
        : r(x, y, w, h)
    { }
    QRectF r;
};

// should be mappable to D3D12_INPUT_ELEMENT_DESC + D3D12_VERTEX_BUFFER_VIEW...
struct Q_VKR_EXPORT QRhiVertexInputLayout
{
    struct Binding {
        enum Classification {
            PerVertex,
            PerInstance
        };
        Binding() { }
        Binding(quint32 stride_, Classification cls = PerVertex)
            : stride(stride_), classification(cls)
        { }
        quint32 stride; // if another api needs this in setVertexBuffer (d3d12), make the cb store a ptr to the current ps and look up the stride via that
        Classification classification;
    };

    struct Attribute {
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
        Attribute(int binding_, int location_, Format format_, quint32 offset_,
                  const char *semanticName_, int semanticIndex_ = 0)
            : binding(binding_), location(location_), format(format_), offset(offset_),
              semanticName(semanticName_), semanticIndex(semanticIndex_)
        { }
        int binding;
        int location;
        Format format;
        quint32 offset;
        const char *semanticName; // POSITION, COLOR, TEXCOORD, ...
        int semanticIndex; // matters for TEXCOORD
    };

    QVector<Binding> bindings; // aka slot (d3d12)
    QVector<Attribute> attributes;
};

struct Q_VKR_EXPORT QRhiGraphicsShaderStage
{
    enum Type {
        Vertex,
        Fragment,
        Geometry,
        TessellationControl, // Hull
        TessellationEvaluation // Domain
    };

    QRhiGraphicsShaderStage() { }
    QRhiGraphicsShaderStage(Type type_, const QByteArray &shader_, const char *name_ = "main")
        : type(type_), shader(shader_), name(name_)
    { }

    Type type;
    QByteArray shader;
    const char *name;
};

class QRhi;
class QRhiResourcePrivate;

class QRhiResource
{
public:
    virtual ~QRhiResource();
    virtual void release() = 0;

protected:
    QRhiResource(QRhi *rhi, QRhiResourcePrivate *d);
    QRhiResourcePrivate *d_ptr = nullptr;
    friend class QRhiResourcePrivate;
    Q_DISABLE_COPY(QRhiResource)
};

#define Q_VK_RES_PRIVATE(Class) \
public: \
    Class() { } \
protected: \
    Q_DISABLE_COPY(Class) \
    friend class QRhi; \
    friend class QRhiVulkan;

class Q_VKR_EXPORT QRhiBuffer : public QRhiResource
{
public:
    enum Type {
        StaticType,
        DynamicType
    };

    enum UsageFlag {
        VertexBuffer = 1 << 0,
        IndexBuffer = 1 << 1,
        UniformBuffer = 1 << 2
    };
    Q_DECLARE_FLAGS(UsageFlags, UsageFlag)

    Type type;
    UsageFlags usage;
    int size;

    bool isStatic() const { return type == StaticType; }

    virtual bool build() = 0;

protected:
    QRhiBuffer(QRhi *rhi, QRhiResourcePrivate *d, Type type_, UsageFlags usage_, int size_);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiBuffer::UsageFlags)

class Q_VKR_EXPORT QRhiRenderBuffer : public QRhiResource
{
public:
    enum Type {
        DepthStencil
    };

    Type type;
    QSize pixelSize;
    int sampleCount;

    virtual bool build() = 0;

protected:
    QRhiRenderBuffer(QRhi *rhi, QRhiResourcePrivate *d,
                     Type type_, const QSize &pixelSize_, int sampleCount_);
};

class Q_VKR_EXPORT QRhiTexture : public QRhiResource
{
public:
    enum Flag {
        RenderTarget = 1 << 0
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

    struct SubImageInfo {
        int size = 0;
        int stride = 0;
        int offset = 0;
    };

    Format format;
    QSize pixelSize;
    Flags flags;

    virtual bool build() = 0;

protected:
    QRhiTexture(QRhi *rhi, QRhiResourcePrivate *d,
                Format format_, const QSize &pixelSize_, Flags flags_);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTexture::Flags)

class Q_VKR_EXPORT QRhiSampler : public QRhiResource
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
    QRhiSampler(QRhi *rhi, QRhiResourcePrivate *d,
                Filter magFilter_, Filter minFilter_, Filter mipmapMode_, AddressMode u_, AddressMode v_);
};

struct Q_VKR_EXPORT QRhiRenderPass
{
Q_VK_RES_PRIVATE(QRhiRenderPass)
    VkRenderPass rp = VK_NULL_HANDLE;
    friend class QVkGraphicsPipeline;
};

struct Q_VKR_EXPORT QRhiRenderTarget
{
    QSize sizeInPixels() const { return pixelSize; }
    const QRhiRenderPass *renderPass() const { return &rp; }

Q_VK_RES_PRIVATE(QRhiRenderTarget)
    VkFramebuffer fb = VK_NULL_HANDLE;
    QRhiRenderPass rp;
    QSize pixelSize;
    int attCount = 0;
    enum Type {
        RtRef,
        RtTexture
    };
    Type type = RtRef;
};

struct Q_VKR_EXPORT QRhiTextureRenderTarget : public QRhiRenderTarget
{
    enum Flag {
        PreserveColorContents = 1 << 0
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    // color only
    QRhiTextureRenderTarget(QRhiTexture *texture_, Flags flags_ = 0)
        : texture(texture_), depthTexture(nullptr), depthStencilBuffer(nullptr), flags(flags_)
    { }
    // color and depth-stencil, only color accessed afterwards
    QRhiTextureRenderTarget(QRhiTexture *texture_, QRhiRenderBuffer *depthStencilBuffer_, Flags flags_ = 0)
        : texture(texture_), depthTexture(nullptr), depthStencilBuffer(depthStencilBuffer_), flags(flags_)
    { }
    // color and depth, both as textures accessible afterwards
    QRhiTextureRenderTarget(QRhiTexture *texture_, QRhiTexture *depthTexture_, Flags flags_ = 0)
        : texture(texture_), depthTexture(depthTexture_), depthStencilBuffer(nullptr), flags(flags_)
    { }

    QRhiTexture *texture;
    QRhiTexture *depthTexture;
    QRhiRenderBuffer *depthStencilBuffer;
    Flags flags;

Q_VK_RES_PRIVATE(QRhiTextureRenderTarget)
    int lastActiveFrameSlot = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTextureRenderTarget::Flags)

class Q_VKR_EXPORT QRhiShaderResourceBindings : public QRhiResource
{
public:
    struct Binding {
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

        static Binding uniformBuffer(int binding_, StageFlags stage_, QRhiBuffer *buf_, int offset_ = 0, int size_ = 0)
        {
            Binding b;
            b.binding = binding_;
            b.stage = stage_;
            b.type = UniformBuffer;
            b.ubuf.buf = buf_;
            b.ubuf.offset = offset_;
            b.ubuf.size = size_;
            return b;
        }

        static Binding sampledTexture(int binding_, StageFlags stage_, QRhiTexture *tex_, QRhiSampler *sampler_)
        {
            Binding b;
            b.binding = binding_;
            b.stage = stage_;
            b.type = SampledTexture;
            b.stex.tex = tex_;
            b.stex.sampler = sampler_;
            return b;
        }

        int binding;
        StageFlags stage;
        Type type;
        struct UniformBufferData {
            QRhiBuffer *buf;
            int offset;
            int size;
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
    QRhiShaderResourceBindings(QRhi *rhi, QRhiResourcePrivate *d);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiShaderResourceBindings::Binding::StageFlags)

class Q_VKR_EXPORT QRhiGraphicsPipeline : public QRhiResource
{
public:
    enum Flag {
        UsesBlendConstants = 1 << 0,
        UsesStencilRef = 1 << 1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Topology {
        Triangles,
        TriangleStrip,
        TriangleFan,
        Lines,
        LineStrip,
        Points
    };

    enum CullModeFlag {
        Front = 1 << 0,
        Back = 1 << 1
    };
    Q_DECLARE_FLAGS(CullMode, CullModeFlag)

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
    bool rasterizerDiscard = false;
    CullMode cullMode;
    FrontFace frontFace = CCW;
    QVector<TargetBlend> targetBlends;
    bool depthTest = false;
    bool depthWrite = false;
    CompareOp depthOp = Less;
    bool stencilTest = false;
    StencilOpState stencilFront;
    StencilOpState stencilBack;
    // use the same read (compare) and write masks for both faces (see d3d12).
    // have the reference value dynamically settable.
    quint32 stencilReadMask = 0xFF;
    quint32 stencilWriteMask = 0xFF;
    int sampleCount = 1; // MSAA, swapchain+depthstencil must match
    QVector<QRhiGraphicsShaderStage> shaderStages;
    QRhiVertexInputLayout vertexInputLayout;
    QRhiShaderResourceBindings *shaderResourceBindings = nullptr;
    const QRhiRenderPass *renderPass = nullptr;

    virtual bool build() = 0;

protected:
    QRhiGraphicsPipeline(QRhi *rhi, QRhiResourcePrivate *d);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::CullMode)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::ColorMask)

struct Q_VKR_EXPORT QRhiCommandBuffer
{
Q_VK_RES_PRIVATE(QRhiCommandBuffer)
    VkCommandBuffer cb = VK_NULL_HANDLE;

    void resetState() {
        currentTarget = nullptr;
        currentPipeline = nullptr;
        currentSrb = nullptr;
    }
    QRhiRenderTarget *currentTarget;
    QRhiGraphicsPipeline *currentPipeline;
    QRhiShaderResourceBindings *currentSrb;
};

class Q_VKR_EXPORT QRhiSwapChain : public QRhiResource
{
public:
    enum SurfaceImportFlag {
        UseDepthStencil = 1 << 0,
        SurfaceHasPreMulAlpha = 1 << 1,
        SurfaceHasNonPreMulAlpha = 1 << 2
    };
    Q_DECLARE_FLAGS(SurfaceImportFlags, SurfaceImportFlag)

    virtual QRhiCommandBuffer *currentFrameCommandBuffer() = 0;
    virtual QRhiRenderTarget *currentFrameRenderTarget() = 0;
    virtual const QRhiRenderPass *defaultRenderPass() const = 0;
    virtual QSize sizeInPixels() const = 0;

    virtual bool build(QWindow *window, const QSize &pixelSize, SurfaceImportFlags flags,
                       QRhiRenderBuffer *depthStencil, int sampleCount) = 0;

protected:
    QRhiSwapChain(QRhi *rhi, QRhiResourcePrivate *d);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiSwapChain::SurfaceImportFlags)

class Q_VKR_EXPORT QRhi
{
public:
    struct InitParams {
        QVulkanInstance *inst = nullptr;
        VkPhysicalDevice physDev = VK_NULL_HANDLE;
        VkDevice dev = VK_NULL_HANDLE;
        VkCommandPool cmdPool = VK_NULL_HANDLE;
        VkQueue gfxQueue = VK_NULL_HANDLE;
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

    struct DynamicBufferUpdate {
        DynamicBufferUpdate() { }
        DynamicBufferUpdate(QRhiBuffer *buf_, int offset_, int size_, const void *data_)
            : buf(buf_), offset(offset_), data(reinterpret_cast<const char *>(data_), size_)
        { }

        QRhiBuffer *buf = nullptr;
        int offset = 0;
        QByteArray data;
    };

    struct StaticBufferUpload {
        StaticBufferUpload() { }
        StaticBufferUpload(QRhiBuffer *buf_, const void *data_)
            : buf(buf_), data(reinterpret_cast<const char *>(data_), buf_->size)
        { }

        QRhiBuffer *buf = nullptr;
        QByteArray data;
    };

    struct TextureUpload {
        TextureUpload() { }
        TextureUpload(QRhiTexture *tex_, const QImage &image_, int mipLevel_ = 0, int layer_ = 0)
            : tex(tex_), image(image_), mipLevel(mipLevel_), layer(layer_)
        { }

        QRhiTexture *tex = nullptr;
        QImage image;
        int mipLevel = 0;
        int layer = 0;
    };

    struct PassUpdates {
        QVector<DynamicBufferUpdate> dynamicBufferUpdates;
        QVector<StaticBufferUpload> staticBufferUploads;
        QVector<TextureUpload> textureUploads;

        PassUpdates &operator+=(const PassUpdates &u)
        {
            dynamicBufferUpdates += u.dynamicBufferUpdates;
            staticBufferUploads += u.staticBufferUploads;
            textureUploads += u.textureUploads;
            return *this;
        }
    };

    QRhi(const InitParams &params);
    ~QRhi();

    QVector<int> supportedSampleCounts() const;

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
    QRhiBuffer *createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, int size);

    int ubufAlignment() const;
    int ubufAligned(int v) const;

    // Transient image, backed by lazily allocated memory (ideal for tiled
    // GPUs). To be used for depth-stencil.
    QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize, int sampleCount = 1);

    QRhiTexture *createTexture(QRhiTexture::Format format, const QSize &pixelSize, QRhiTexture::Flags flags = 0);

    QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                               QRhiSampler::Filter mipmapMode,
                               QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v);

    bool createTextureRenderTarget(QRhiTextureRenderTarget *rt);

    void releaseLater(QRhiTextureRenderTarget *rt);

    /*
      Render to a QWindow (must be VulkanSurface):
        Create a swapchain.
        Call build() on the swapchain whenever the size is different than before.
        Call release() on QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed.
        Then on every frame:
           beginFrame(sc)
           beginPass(sc->currentFrameRenderTarget(), sc->currentFrameCommandBuffer(), clearValues, updates)
           ...
           endPass(sc->currentFrameCommandBuffer())
           endFrame(sc) // this queues the Present, begin/endFrame manages double buffering internally
     */
    QRhiSwapChain *createSwapChain();
    FrameOpResult beginFrame(QRhiSwapChain *swapChain);
    FrameOpResult endFrame(QRhiSwapChain *swapChain);

    /*
      Render to a QVulkanWindow from a startNextFrame() implementation:
         QRhiRenderTarget currentFrameRenderTarget;
         QRhiCommandBuffer currentFrameCommandBuffer;
         beginFrame(window, &currentFrameRenderTarget, &currentFrameCommandBuffer)
         beginPass(currentFrameRenderTarget, currentFrameCommandBuffer, clearValues, updates)
         ...
         endPass(currentFrameCommandBuffer)
         endFrame(window)
     */
    void beginFrame(QVulkanWindow *window,
                    QRhiRenderTarget *outCurrentFrameRenderTarget,
                    QRhiCommandBuffer *outCurrentFrameCommandBuffer);
    void endFrame(QVulkanWindow *window);
    // the renderpass may be needed before beginFrame can be called
    void importVulkanWindowRenderPass(QVulkanWindow *window, QRhiRenderPass *outRenderPass);

    void beginPass(QRhiRenderTarget *rt, QRhiCommandBuffer *cb, const QRhiClearValue *clearValues, const PassUpdates &updates);
    void endPass(QRhiCommandBuffer *cb);

    // When specified, srb can be different from ps' srb but the layouts must
    // match. Basic tracking is included: no command is added to the cb when
    // the pipeline or desc.set are the same as in the last call in the same
    // frame; srb is updated automatically at this point whenever a referenced
    // buffer, texture, etc. is out of date internally (due to release+create
    // since the creation of the srb) - hence no need to manually recreate the
    // srb in case a QRhiBuffer is "resized" etc.
    void setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb = nullptr);

    using VertexInput = QPair<QRhiBuffer *, quint32>; // buffer, offset
    void setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<VertexInput> &bindings,
                        QRhiBuffer *indexBuf = nullptr, quint32 indexOffset = 0, IndexFormat indexFormat = IndexUInt16);

    void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport);
    void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor);
    void setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c);
    void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue);

    void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
              quint32 instanceCount = 1, quint32 firstVertex = 0, quint32 firstInstance = 0);
    void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                     quint32 instanceCount = 1, quint32 firstIndex = 0, qint32 vertexOffset = 0, quint32 firstInstance = 0);

    // make Y up and viewport.min/maxDepth 0/1
    QMatrix4x4 openGLCorrectionMatrix() const;

private:
    QRhiPrivate *d_ptr;
    friend class QRhiPrivate;
    Q_DISABLE_COPY(QRhi)
};

QT_END_NAMESPACE

#endif
