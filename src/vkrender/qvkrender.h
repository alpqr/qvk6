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

class QVkRenderPrivate;
class QVulkanWindow;

static const int QVK_FRAMES_IN_FLIGHT = 2;

struct QVkClearValue
{
    QVkClearValue() { }
    QVkClearValue(const QVector4D &rgba_) : rgba(rgba_), isDepthStencil(false) { }
    QVkClearValue(float d_, quint32 s_) : d(d_), s(s_), isDepthStencil(true) { }
    QVector4D rgba;
    float d;
    quint32 s;
    bool isDepthStencil;
};

struct QVkViewport
{
    QVkViewport() { }
    QVkViewport(float x, float y, float w, float h, float minDepth_ = 0.0f, float maxDepth_ = 1.0f)
        : r(x, y, w, h), minDepth(minDepth_), maxDepth(maxDepth_)
    { }
    QRectF r;
    float minDepth;
    float maxDepth;
};

struct QVkScissor
{
    QVkScissor() { }
    QVkScissor(float x, float y, float w, float h)
        : r(x, y, w, h)
    { }
    QRectF r;
};

// should be mappable to D3D12_INPUT_ELEMENT_DESC + D3D12_VERTEX_BUFFER_VIEW...
struct QVkVertexInputLayout
{
    struct Binding {
        enum Classification {
            PerVertex,
            PerInstance
        };
        Binding() { }
        Binding(quint32 stride_, Classification k = PerVertex)
            : stride(stride_), classification(k)
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

struct QVkGraphicsShaderStage
{
    enum Type {
        Vertex,
        Fragment,
        Geometry,
        TessellationControl, // Hull
        TessellationEvaluation // Domain
    };

    QVkGraphicsShaderStage() { }
    QVkGraphicsShaderStage(Type type_, const QByteArray &spirv_, const char *name_ = "main")
        : type(type_), spirv(spirv_), name(name_)
    { }

    Type type;
    QByteArray spirv;
    const char *name;
};

#define Q_VK_RES_PRIVATE(Class) \
public: \
    Class() { } \
protected: \
    Q_DISABLE_COPY(Class) \
    friend class QVkRender; \
    friend class QVkRenderPrivate;

struct QVkRenderPass
{
Q_VK_RES_PRIVATE(QVkRenderPass)
    VkRenderPass rp = VK_NULL_HANDLE;
};

struct QVkBuffer;
struct QVkTexture;
struct QVkSampler;

struct QVkShaderResourceBindings
{
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

        static Binding uniformBuffer(int binding_, StageFlags stage_, QVkBuffer *buf_, int offset_ = 0, int size_ = 0)
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

        static Binding sampledTexture(int binding_, StageFlags stage_, QVkTexture *tex_, QVkSampler *sampler_)
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
            QVkBuffer *buf;
            int offset;
            int size;
        };
        struct SampledTextureData {
            QVkTexture *tex;
            QVkSampler *sampler;
        };
        union {
            UniformBufferData ubuf;
            SampledTextureData stex;
        };
    };

    QVector<Binding> bindings;

Q_VK_RES_PRIVATE(QVkShaderResourceBindings)
    int poolIndex = -1;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet descSets[QVK_FRAMES_IN_FLIGHT]; // multiple sets to support dynamic buffers
    int lastActiveFrameSlot = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkShaderResourceBindings::Binding::StageFlags)

struct QVkGraphicsPipeline
{
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
    QVector<QVkGraphicsShaderStage> shaderStages;
    QVkVertexInputLayout vertexInputLayout;
    QVkShaderResourceBindings *shaderResourceBindings = nullptr;
    const QVkRenderPass *renderPass = nullptr;

Q_VK_RES_PRIVATE(QVkGraphicsPipeline)
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkGraphicsPipeline::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QVkGraphicsPipeline::CullMode)
Q_DECLARE_OPERATORS_FOR_FLAGS(QVkGraphicsPipeline::ColorMask)

typedef void * QVkAlloc;

struct QVkBuffer
{
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

    QVkBuffer(Type type_, UsageFlags usage_, int size_)
        : type(type_), usage(usage_), size(size_)
    { }

    Type type;
    UsageFlags usage;
    int size;

    bool isStatic() const { return type == StaticType; }

Q_VK_RES_PRIVATE(QVkBuffer)
    struct PendingUpdate {
        PendingUpdate() { }
        PendingUpdate(int o, const QByteArray &u)
            : offset(o), data(u)
        { }
        int offset;
        const QByteArray data;
    };
    struct {
        VkBuffer buffer = VK_NULL_HANDLE;
        QVkAlloc allocation = nullptr;
        QVector<PendingUpdate> pendingUpdates;
    } d[QVK_FRAMES_IN_FLIGHT]; // only [0] is used for Static
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    QVkAlloc stagingAlloc = nullptr;
    int lastActiveFrameSlot = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkBuffer::UsageFlags)

struct QVkRenderBuffer
{
    enum Type {
        DepthStencil
    };

    QVkRenderBuffer(Type type_, const QSize &pixelSize_, int sampleCount_ = 1)
        : type(type_), pixelSize(pixelSize_), sampleCount(sampleCount_)
    { }

    Type type;
    QSize pixelSize;
    int sampleCount;

Q_VK_RES_PRIVATE(QVkRenderBuffer)
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImage image;
    VkImageView imageView;
    int lastActiveFrameSlot = -1;
};

struct QVkTexture
{
    enum Flag {
        NoUploadContents = 1 << 0,
        RenderTarget = 1 << 1
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

    QVkTexture(Format format_, const QSize &pixelSize_, Flags flags_ = 0)
        : format(format_), pixelSize(pixelSize_), flags(flags_)
    { }

    Format format;
    QSize pixelSize;
    Flags flags;

Q_VK_RES_PRIVATE(QVkTexture)
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    QVkAlloc allocation = nullptr;
    VkImage stagingImage = VK_NULL_HANDLE;
    QVkAlloc stagingAlloc = nullptr;
    bool wasStaged = false;
    int lastActiveFrameSlot = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkTexture::Flags)

struct QVkSampler
{
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

    QVkSampler(Filter magFilter_, Filter minFilter_, Filter mipmapMode_, AddressMode u_, AddressMode v_)
        : magFilter(magFilter_), minFilter(minFilter_), mipmapMode(mipmapMode_),
          addressU(u_), addressV(v_)
    { }

    Filter magFilter;
    Filter minFilter;
    Filter mipmapMode;
    AddressMode addressU;
    AddressMode addressV;

Q_VK_RES_PRIVATE(QVkSampler)
    VkSampler sampler = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
};

struct QVkCommandBuffer
{
Q_VK_RES_PRIVATE(QVkCommandBuffer)
    VkCommandBuffer cb = VK_NULL_HANDLE;

    void resetState() {
        currentPipeline = nullptr;
        currentSrb = nullptr;
    }
    QVkGraphicsPipeline *currentPipeline;
    QVkShaderResourceBindings *currentSrb;
};

struct QVkRenderTarget
{
    QSize sizeInPixels() const { return pixelSize; }
    const QVkRenderPass *renderPass() const { return &rp; }

Q_VK_RES_PRIVATE(QVkRenderTarget)
    VkFramebuffer fb = VK_NULL_HANDLE;
    QVkRenderPass rp;
    QSize pixelSize;
    int attCount = 0;
    enum Type {
        RtRef,
        RtTexture
    };
    Type type = RtRef;
};

struct QVkTextureRenderTarget : public QVkRenderTarget
{
    enum Flag {
        PreserveColorContents = 1 << 0
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    // color only
    QVkTextureRenderTarget(QVkTexture *texture_, Flags flags_ = 0)
        : texture(texture_), depthTexture(nullptr), depthStencilBuffer(nullptr), flags(flags_)
    { }
    // color and depth-stencil, only color accessed afterwards
    QVkTextureRenderTarget(QVkTexture *texture_, QVkRenderBuffer *depthStencilBuffer_, Flags flags_ = 0)
        : texture(texture_), depthTexture(nullptr), depthStencilBuffer(depthStencilBuffer_), flags(flags_)
    { }
    // color and depth, both as textures accessible afterwards
    QVkTextureRenderTarget(QVkTexture *texture_, QVkTexture *depthTexture_, Flags flags_ = 0)
        : texture(texture_), depthTexture(depthTexture_), depthStencilBuffer(nullptr), flags(flags_)
    { }

    QVkTexture *texture;
    QVkTexture *depthTexture;
    QVkRenderBuffer *depthStencilBuffer;
    Flags flags;

Q_VK_RES_PRIVATE(QVkTextureRenderTarget)
    int lastActiveFrameSlot = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkTextureRenderTarget::Flags)

struct QVkSwapChain
{
    QVkCommandBuffer *currentFrameCommandBuffer() { return &imageRes[currentImage].cmdBuf; }
    QVkRenderTarget *currentFrameRenderTarget() { return &rt; }
    const QVkRenderPass *defaultRenderPass() const { return rt.renderPass(); }
    QSize sizeInPixels() const { return pixelSize; }

Q_VK_RES_PRIVATE(QVkSwapChain)
    static const int DEFAULT_BUFFER_COUNT = 2;
    static const int MAX_BUFFER_COUNT = 3;

    QSize pixelSize;
    bool supportsReadback = false;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    int bufferCount = 0;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VkColorSpaceKHR(0); // this is in fact VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    QVkRenderBuffer *depthStencil = nullptr;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkDeviceMemory msaaImageMem = VK_NULL_HANDLE;

    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        QVkCommandBuffer cmdBuf;
        VkFence cmdFence = VK_NULL_HANDLE;
        bool cmdFenceWaitable = false;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkImage msaaImage = VK_NULL_HANDLE;
        VkImageView msaaImageView = VK_NULL_HANDLE;
    } imageRes[MAX_BUFFER_COUNT];

    struct FrameResources {
        VkFence fence = VK_NULL_HANDLE;
        bool fenceWaitable = false;
        VkSemaphore imageSem = VK_NULL_HANDLE;
        VkSemaphore drawSem = VK_NULL_HANDLE;
        bool imageAcquired = false;
        bool imageSemWaitable = false;
    } frameRes[QVK_FRAMES_IN_FLIGHT];

    quint32 currentImage = 0; // index in imageRes
    quint32 currentFrame = 0; // index in frameRes
    QVkRenderTarget rt;
};

class Q_VKR_EXPORT QVkRender
{
public:
    struct InitParams {
        QVulkanInstance *inst = nullptr;
        VkPhysicalDevice physDev = VK_NULL_HANDLE;
        VkDevice dev = VK_NULL_HANDLE;
        VkCommandPool cmdPool = VK_NULL_HANDLE;
        VkQueue gfxQueue = VK_NULL_HANDLE;
    };

    enum SurfaceImportFlag {
        UseDepthStencil = 1 << 0,
        SurfaceHasPreMulAlpha = 1 << 1,
        SurfaceHasNonPreMulAlpha = 1 << 2
    };
    Q_DECLARE_FLAGS(SurfaceImportFlags, SurfaceImportFlag)

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

    QVkRender(const InitParams &params);
    ~QVkRender();

    QVector<int> supportedSampleCounts() const;

    /*
       The underlying graphics resources are created when calling create* and
       put on the release queue by releaseLater (so this is safe even when
       the resource is used by the still executing/pending frame(s)).

       The QVk* instance itself is not destroyed by the release and it is safe
       to destroy it right away after calling releaseLater.

       Changing any value needs explicit release and rebuilding of the
       underlying resource before it can take effect.

       create(res); <change something>; releaseLater(res); create(res); ...
       is therefore perfectly valid and can be used to recreate things (when
       buffer or texture size changes f.ex.)
     */

    bool createGraphicsPipeline(QVkGraphicsPipeline *ps);

    bool createShaderResourceBindings(QVkShaderResourceBindings *srb);

    // Buffers are immutable like other resources but the underlying data can
    // change. (its size cannot) Having multiple frames in flight is handled
    // transparently, with multiple allocations, recording updates, etc.
    // internally. The underlying memory type may differ for static and dynamic
    // buffers. For best performance, static buffers may be copied to device
    // local (not necessarily host visible) memory via a staging (host visible)
    // buffer. Hence a separate uploadStaticBuffer().
    bool createBuffer(QVkBuffer *buf);
    // This goes on the command buffer so must be within begin/endFrame. But
    // outside begin/endPass!
    void uploadStaticBuffer(QVkCommandBuffer *cb, QVkBuffer *buf, const void *data);
    // Queues a partial update. GPU-visible memory is updated in the next
    // set*Buffer(s) (vertex/index) or setGraphicsPipeline (uniform). (so
    // changing the same region more than once per frame must be avoided as
    // only the values from the last update will be the visible when the
    // commands are executed)
    void updateDynamicBuffer(QVkBuffer *buf, int offset, int size, const void *data);
    int ubufAlignment() const;
    int ubufAligned(int v) const;

    // Transient image, backed by lazily allocated memory (ideal for tiled
    // GPUs). To be used for depth-stencil.
    bool createRenderBuffer(QVkRenderBuffer *rb);

    bool createTexture(QVkTexture *tex);
    QVkTexture::SubImageInfo textureInfo(QVkTexture *tex, int mipLevel = 0, int layer = 0);
    // similar to uploadStaticBuffer. must not be between begin/endPass.
    void uploadTexture(QVkCommandBuffer *cb, QVkTexture *tex, const QImage &data, int mipLevel = 0, int layer = 0);

    bool createSampler(QVkSampler *sampler);

    bool createTextureRenderTarget(QVkTextureRenderTarget *rt);

    void releaseLater(QVkGraphicsPipeline *ps);
    void releaseLater(QVkShaderResourceBindings *srb);
    void releaseLater(QVkBuffer *buf);
    void releaseLater(QVkRenderBuffer *rb);
    void releaseLater(QVkTexture *tex);
    void releaseLater(QVkSampler *sampler);
    void releaseLater(QVkTextureRenderTarget *rt);

    /*
      Render to a QWindow (must be VulkanSurface):
        Call importSurface to create a swapchain and call it whenever the size is different than before.
        Call releaseSwapChain on QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed.
        Then on every frame:
           beginFrame(sc)
           ... // upload
           beginPass(sc->currentFrameRenderTarget(), sc->currentFrameCommandBuffer(), clearValues)
           ... // draw
           endPass(sc->currentFrameCommandBuffer())
           endFrame(sc) // this queues the Present, begin/endFrame manages double buffering internally
     */
    bool importSurface(VkSurfaceKHR surface, const QSize &pixelSize, SurfaceImportFlags flags,
                       QVkRenderBuffer *depthStencil, int sampleCount, QVkSwapChain *outSwapChain);
    void releaseSwapChain(QVkSwapChain *swapChain);
    FrameOpResult beginFrame(QVkSwapChain *sc);
    FrameOpResult endFrame(QVkSwapChain *sc);

    /*
      Render to a QVulkanWindow from a startNextFrame() implementation:
         QVkRenderTarget currentFrameRenderTarget;
         QVkCommandBuffer currentFrameCommandBuffer;
         beginFrame(window, &currentFrameRenderTarget, &currentFrameCommandBuffer)
         ... // upload
         beginPass(currentFrameRenderTarget, currentFrameCommandBuffer, clearValues)
         ... // draw
         endPass(currentFrameCommandBuffer)
         endFrame(window)
     */
    void beginFrame(QVulkanWindow *window,
                    QVkRenderTarget *outCurrentFrameRenderTarget,
                    QVkCommandBuffer *outCurrentFrameCommandBuffer);
    void endFrame(QVulkanWindow *window);
    // the renderpass may be needed before beginFrame can be called
    void importVulkanWindowRenderPass(QVulkanWindow *window, QVkRenderPass *outRenderPass);

    void beginPass(QVkRenderTarget *rt, QVkCommandBuffer *cb, const QVkClearValue *clearValues);
    void endPass(QVkCommandBuffer *cb);

    // Binds the pipeline and manages shader resources (like does the actual
    // update for queued dynamic buffer updates). When specified, srb can be
    // different from ps' srb but the layouts must match. Basic tracking is
    // included: no command is added to the cb when the pipeline or desc.set
    // are the same as in the last call in the same frame.
    void setGraphicsPipeline(QVkCommandBuffer *cb, QVkGraphicsPipeline *ps, QVkShaderResourceBindings *srb = nullptr);

    using VertexInput = QPair<QVkBuffer *, quint32>; // buffer, offset
    // Binds vertex/index buffers and performs dynamic buffer updates.
    void setVertexInput(QVkCommandBuffer *cb, int startBinding, const QVector<VertexInput> &bindings,
                        QVkBuffer *indexBuf = nullptr, quint32 indexOffset = 0, IndexFormat indexFormat = IndexUInt16);

    void setViewport(QVkCommandBuffer *cb, const QVkViewport &viewport);
    void setScissor(QVkCommandBuffer *cb, const QVkScissor &scissor);
    void setBlendConstants(QVkCommandBuffer *cb, const QVector4D &c);
    void setStencilRef(QVkCommandBuffer *cb, quint32 refValue);

    void draw(QVkCommandBuffer *cb, quint32 vertexCount,
              quint32 instanceCount = 1, quint32 firstVertex = 0, quint32 firstInstance = 0);
    void drawIndexed(QVkCommandBuffer *cb, quint32 indexCount,
                     quint32 instanceCount = 1, quint32 firstIndex = 0, qint32 vertexOffset = 0, quint32 firstInstance = 0);

    // make Y up and viewport.min/maxDepth 0/1
    QMatrix4x4 openGLCorrectionMatrix() const;

private:
    QVkRenderPrivate *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkRender::SurfaceImportFlags)

QT_END_NAMESPACE

#endif
