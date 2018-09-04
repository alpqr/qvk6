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

QT_BEGIN_NAMESPACE

class QVkRenderPrivate;
class QVulkanWindow;

static const int QVK_FRAMES_IN_FLIGHT = 2;
static const int QVK_MAX_SHADER_RESOURCE_BINDINGS = 4;
static const int QVK_MAX_UNIFORM_BUFFERS_PER_SRB = 4;

struct QVkClearValue
{
    QVkClearValue(const QVector4D &rgba_) : rgba(rgba_) { }
    QVkClearValue(float d_, quint32 s_) : d(d_), s(s_), isDepthStencil(true) { }
    QVector4D rgba;
    float d = 1;
    quint32 s = 0;
    bool isDepthStencil = false;
};

struct QVkViewport
{
    QRectF r;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct QVkScissor
{
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
        quint32 stride = 0; // if another api needs this in setVertexBuffer (d3d12), make the cb store a ptr to the current ps and look up the stride via that
        Classification classification = PerVertex;
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
        int binding = 0;
        int location = 0;
        Format format = Float4;
        quint32 offset = 0;
        const char *semanticName = nullptr; // POSITION, COLOR, TEXCOORD, ...
        int semanticIndex = 0; // matters for TEXCOORD
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

    Type type = Vertex;
    QByteArray spirv;
    const char *entryPoint = "main";
};

#define Q_VK_RES_PRIVATE(Class) \
public: \
    Class() { } \
private: \
    Q_DISABLE_COPY(Class) \
    friend class QVkRender; \
    friend class QVkRenderPrivate;

struct QVkRenderPass
{
Q_VK_RES_PRIVATE(QVkRenderPass)
    VkRenderPass rp = VK_NULL_HANDLE;
};

struct QVkBuffer;

struct QVkShaderResourceBindings
{
    struct Binding {
        enum Type {
            UniformBuffer
        };
        int binding = 0;
        QVkGraphicsShaderStage::Type stage = QVkGraphicsShaderStage::Vertex;
        Type type = UniformBuffer;
        union {
            struct {
                QVkBuffer *buf;
            } uniformBuffer;
        };
    };

    QVector<Binding> bindings;

Q_VK_RES_PRIVATE(QVkShaderResourceBindings)
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet descSets[QVK_FRAMES_IN_FLIGHT];
    int lastActiveFrameSlot = -1;
};

struct QVkGraphicsPipelineState
{
    QVector<QVkGraphicsShaderStage> shaderStages;
    QVkVertexInputLayout vertexInputLayout;
    const QVkRenderPass *renderPass = nullptr;
    QVkShaderResourceBindings *shaderResourceBindings = nullptr;

Q_VK_RES_PRIVATE(QVkGraphicsPipelineState)
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
};

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

    Type type = DynamicType;
    UsageFlags usage = VertexBuffer;
    int size = 0;

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
    } d[QVK_FRAMES_IN_FLIGHT];
    int lastActiveFrameSlot = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkBuffer::UsageFlags)

struct QVkTexture
{
Q_VK_RES_PRIVATE(QVkTexture)
    struct {
        VkImage image = VK_NULL_HANDLE;
        QVkAlloc allocation = nullptr;
        VkImageView imageView = VK_NULL_HANDLE;
    } d[QVK_FRAMES_IN_FLIGHT];
    int lastActiveFrameSlot = -1;
};

struct QVkSampler
{
};

struct QVkCommandBuffer
{
Q_VK_RES_PRIVATE(QVkCommandBuffer)
    VkCommandBuffer cb = VK_NULL_HANDLE;
};

struct QVkSwapChain
{
    QVkCommandBuffer *currentFrameCommandBuffer() { return &imageRes[currentImage].cmdBuf; }
    QSize sizeInPixels() const { return pixelSize; }
    const QVkRenderPass *renderPass() const { return &rp; }

Q_VK_RES_PRIVATE(QVkSwapChain)
    static const int DEFAULT_BUFFER_COUNT = 2;
    static const int MAX_BUFFER_COUNT = 3;

    QSize pixelSize;
    bool supportsReadback = false;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    int bufferCount = 0;
    bool hasDepthStencil = false;

    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        QVkCommandBuffer cmdBuf;
        VkFence cmdFence = VK_NULL_HANDLE;
        bool cmdFenceWaitable = false;
        VkFramebuffer fb = VK_NULL_HANDLE;
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
    QVkRenderPass rp;
};

struct QVkRenderTarget
{
    const QVkRenderPass *renderPass() const { return &rp; }

Q_VK_RES_PRIVATE(QVkRenderTarget)
    VkFramebuffer fb = VK_NULL_HANDLE;
    QVkRenderPass rp;
    QSize pixelSize;
    int attCount = 0;
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
        ImportWithAlpha = 0x01,
        ImportWithDepthStencil = 0x02
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

    VkFormat optimalDepthStencilFormat() const;

    /* some basic use cases:

      1. render to a QVulkanWindow from a startNextFrame() implementation
         This is simple, repeat on every frame:
           importVulkanWindowCurrentFrame
           beginPass(rt, cb, ...)
           ...
           endPass(cb)

      2. render to a QWindow (must be VulkanSurface), VkDevice and friends must be provided as well
           call importSurface to create a swapchain + call it whenever the size is different than before
           call releaseSwapChain on QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed
           Then on every frame:
             beginFrame
             beginPass(sc, ...)
             ...
             endPass(sc)
             endFrame (this queues the Present, begin/endFrame manages double buffering internally)

      3. render to a texture:
        3.1 as part of normal frame as described in #1 or #2:
          createTexture
          createRenderTarget
          ...
          [cb = sc.currentFrameCommandBuffer() if #2]
          beginPass(rt, cb, ...)
          ...
          endPass(cb)

        TBD. everything is subject to change.
      */

    /*
       The underlying graphics resources are created when calling create* and
       put on the release queue by scheduleRelease (so this is safe even when
       the resource is used by the still executing/pending frame(s)).

       The QVk* instance itself is not destroyed by the release and it is safe
       to destroy it right away after calling scheduleRelease.

       create(res); scheduleRelease(res); create(res); ... is valid and can be used to recreate things (when buffer or texture size changes f.ex.)
     */

    bool createGraphicsPipelineState(QVkGraphicsPipelineState *ps);
    //bool createComputePipelineState(QVkComputePipelineState *ps);

    bool createShaderResourceBindings(QVkShaderResourceBindings *srb);

    // Buffers are immutable like other resources but (for non-static
    // buffers) the underlying data can change. (its size cannot)
    // Having multiple frames in flight is handled transparently, with
    // multiple allocations, recording updates, etc. internally.
    bool createBuffer(QVkBuffer *buf, const void *data = nullptr);
    // Queues a partial update. Memory is updated in the next set*Buffer (vertex/index) or setGraphicsPipelineState (uniform).
    void updateBuffer(QVkBuffer *buf, int offset, int size, const void *data);

    //    bool createTexture(int whatever, QVkTexture *outTex);
    //    bool createRenderTarget(QVkTexture *color, QVkTexture *ds, QVkRenderTarget *outRt);

    void scheduleRelease(QVkGraphicsPipelineState *ps);
    //void scheduleRelease(QVkComputePipelineState *ps);

    void scheduleRelease(QVkShaderResourceBindings *srb);

    void scheduleRelease(QVkBuffer *buf);
//    void scheduleRelease(QVkTexture *tex);

    void forceRelease();

    bool importSurface(VkSurfaceKHR surface, const QSize &pixelSize, SurfaceImportFlags flags, QVkTexture *depthStencil, QVkSwapChain *outSwapChain);
    void releaseSwapChain(QVkSwapChain *swapChain);
    FrameOpResult beginFrame(QVkSwapChain *sc);
    FrameOpResult endFrame(QVkSwapChain *sc);
    void beginPass(QVkSwapChain *sc, const QVkClearValue *clearValues);
    void endPass(QVkSwapChain *sc);

    void importVulkanWindowCurrentFrame(QVulkanWindow *window, QVkRenderTarget *outRt, QVkCommandBuffer *outCb);
    void beginFrame(QVulkanWindow *window);
    void endFrame(QVulkanWindow *window);
    void beginPass(QVkRenderTarget *rt, QVkCommandBuffer *cb, const QVkClearValue *clearValues);
    void endPass(QVkCommandBuffer *cb);

    void cmdSetGraphicsPipelineState(QVkCommandBuffer *cb, QVkGraphicsPipelineState *ps);
    //void cmdSetComputePipelineState(QVkComputePipelineState *ps);

    // pipeline must be set first
    void cmdSetVertexBuffer(QVkCommandBuffer *cb, int binding, QVkBuffer *vb, quint32 offset);
    void cmdSetVertexBuffers(QVkCommandBuffer *cb, int startBinding, const QVector<QVkBuffer *> &vb, const QVector<quint32> &offset);
    void cmdSetIndexBuffer(QVkCommandBuffer *cb, QVkBuffer *ib, quint32 offset, IndexFormat format);

    void cmdViewport(QVkCommandBuffer *cb, const QVkViewport &viewport);
    void cmdScissor(QVkCommandBuffer *cb, const QVkScissor &scissor);

    void cmdDraw(QVkCommandBuffer *cb, quint32 vertexCount, quint32 instanceCount, quint32 firstVertex, quint32 firstInstance);

    // make Y up and viewport.min/maxDepth 0/1
    QMatrix4x4 openGLCorrectionMatrix() const;

private:
    QVkRenderPrivate *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkRender::SurfaceImportFlags)

QT_END_NAMESPACE

#endif
