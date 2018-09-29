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

#ifndef QRHIVULKAN_P_H
#define QRHIVULKAN_P_H

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

#include "qrhivulkan.h"
#include "qrhi_p.h"

QT_BEGIN_NAMESPACE

class QVulkanFunctions;
class QVulkanDeviceFunctions;
class QVulkanWindow;

static const int QVK_FRAMES_IN_FLIGHT = 2;

static const int QVK_DESC_SETS_PER_POOL = 128;
static const int QVK_UNIFORM_BUFFERS_PER_POOL = 256;
static const int QVK_COMBINED_IMAGE_SAMPLERS_PER_POOL = 256;

// no vk_mem_alloc.h available here, void* is good enough
typedef void * QVkAlloc;
typedef void * QVkAllocator;

struct QVkBuffer : public QRhiBuffer
{
    QVkBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    void release() override;
    bool build() override;

    VkBuffer buffers[QVK_FRAMES_IN_FLIGHT];
    QVkAlloc allocations[QVK_FRAMES_IN_FLIGHT];
    QVector<QRhi::DynamicBufferUpdate> pendingDynamicUpdates[QVK_FRAMES_IN_FLIGHT];
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    QVkAlloc stagingAlloc = nullptr;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
};

struct QVkRenderBuffer : public QRhiRenderBuffer
{
    QVkRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                    int sampleCount, Hints hints);
    void release() override;
    bool build() override;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImage image;
    VkImageView imageView;
    int lastActiveFrameSlot = -1;
};

struct QVkTexture : public QRhiTexture
{
    QVkTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize, Flags flags);
    void release() override;
    bool build() override;

    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    QVkAlloc imageAlloc = nullptr;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    QVkAlloc stagingAlloc = nullptr;
    VkImageLayout layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
};

struct QVkSampler : public QRhiSampler
{
    QVkSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode, AddressMode u, AddressMode v);
    void release() override;
    bool build() override;

    VkSampler sampler = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
};

struct QVkRenderPass : public QRhiRenderPass
{
    QVkRenderPass(QRhiImplementation *rhi);
    void release() override;

    VkRenderPass rp = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
};

struct QVkBasicRenderTargetData
{
    QVkBasicRenderTargetData(QRhiImplementation *rhi) : rp(rhi) { }
    VkFramebuffer fb = VK_NULL_HANDLE;
    QVkRenderPass rp;
    QSize pixelSize;
    int attCount = 0;
};

struct QVkReferenceRenderTarget : public QRhiReferenceRenderTarget
{
    QVkReferenceRenderTarget(QRhiImplementation *rhi);
    void release() override;
    Type type() const override;
    QSize sizeInPixels() const override;
    const QRhiRenderPass *renderPass() const override;

    QVkBasicRenderTargetData d;
};

struct QVkTextureRenderTarget : public QRhiTextureRenderTarget
{
    QVkTextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, Flags flags);
    QVkTextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiRenderBuffer *depthStencilBuffer, Flags flags);
    QVkTextureRenderTarget(QRhiImplementation *rhi, QRhiTexture *texture, QRhiTexture *depthTexture, Flags flags);
    void release() override;
    Type type() const override;
    bool build() override;
    QSize sizeInPixels() const override;
    const QRhiRenderPass *renderPass() const override;

    QVkBasicRenderTargetData d;
    int lastActiveFrameSlot = -1;
};

struct QVkShaderResourceBindings : public QRhiShaderResourceBindings
{
    QVkShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    int poolIndex = -1;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet descSets[QVK_FRAMES_IN_FLIGHT]; // multiple sets to support dynamic buffers
    int lastActiveFrameSlot = -1;
    uint generation = 0;

    // Keep track of the generation number of each referenced QRhi* to be able
    // to detect that the underlying descriptor set became out of date and they
    // need to be written again with the up-to-date VkBuffer etc. objects.
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
    QVector<BoundResourceData> boundResourceData[QVK_FRAMES_IN_FLIGHT];
};

Q_DECLARE_TYPEINFO(QVkShaderResourceBindings::BoundResourceData, Q_MOVABLE_TYPE);

struct QVkGraphicsPipeline : public QRhiGraphicsPipeline
{
    QVkGraphicsPipeline(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
};

struct QVkCommandBuffer : public QRhiCommandBuffer
{
    QVkCommandBuffer(QRhiImplementation *rhi);
    void release() override;

    VkCommandBuffer cb = VK_NULL_HANDLE;

    void resetState() {
        currentTarget = nullptr;
        currentPipeline = nullptr;
        currentPipelineGeneration = 0;
        currentSrb = nullptr;
        currentSrbGeneration = 0;
    }
    QRhiRenderTarget *currentTarget;
    QRhiGraphicsPipeline *currentPipeline;
    uint currentPipelineGeneration;
    QRhiShaderResourceBindings *currentSrb;
    uint currentSrbGeneration;
};

struct QVkSwapChain : public QRhiSwapChain
{
    QVkSwapChain(QRhiImplementation *rhi);
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;
    const QRhiRenderPass *defaultRenderPass() const override;
    QSize sizeInPixels() const override;

    bool build(QWindow *window, const QSize &pixelSize, SurfaceImportFlags flags,
               QRhiRenderBuffer *depthStencil, int sampleCount) override;

    bool build(QObject *target) override;

    static const int DEFAULT_BUFFER_COUNT = 2;
    static const int MAX_BUFFER_COUNT = 3;

    QVulkanWindow *wrapWindow = nullptr;
    QSize pixelSize;
    bool supportsReadback = false;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    int bufferCount = 0;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    QVkRenderBuffer *ds = nullptr;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkDeviceMemory msaaImageMem = VK_NULL_HANDLE;
    VkRenderPass rp;
    QVkReferenceRenderTarget rtWrapper;
    QVkCommandBuffer cbWrapper;

    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
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
};

class QRhiVulkan : public QRhiImplementation
{
public:
    QRhiVulkan(QRhiInitParams *params);
    ~QRhiVulkan();

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
                               QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v) override;

    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiTextureRenderTarget::Flags flags) override;
    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiRenderBuffer *depthStencilBuffer,
                                                       QRhiTextureRenderTarget::Flags flags) override;
    QRhiTextureRenderTarget *createTextureRenderTarget(QRhiTexture *texture,
                                                       QRhiTexture *depthTexture,
                                                       QRhiTextureRenderTarget::Flags flags) override;

    QRhiSwapChain *createSwapChain() override;
    QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain) override;
    QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain) override;

    void beginPass(QRhiRenderTarget *rt,
                   QRhiCommandBuffer *cb,
                   const QRhiClearValue *clearValues,
                   const QRhi::PassUpdates &updates) override;
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
    QMatrix4x4 openGLVertexCorrectionMatrix() const override;
    bool isYUpInFramebuffer() const override;

    void create();
    void destroy();
    VkResult createDescriptorPool(VkDescriptorPool *pool);
    bool allocateDescriptorSet(VkDescriptorSetAllocateInfo *allocInfo, VkDescriptorSet *result, int *resultPoolIndex);
    uint32_t chooseTransientImageMemType(VkImage img, uint32_t startIndex);
    bool createTransientImage(VkFormat format, const QSize &pixelSize, VkImageUsageFlags usage,
                              VkImageAspectFlags aspectMask, VkSampleCountFlagBits sampleCount,
                              VkDeviceMemory *mem, VkImage *images, VkImageView *views, int count);

    bool recreateSwapChain(VkSurfaceKHR surface, const QSize &pixelSize, QRhiSwapChain::SurfaceImportFlags flags,
                           QRhiSwapChain *swapChain);
    void releaseSwapChainResources(QRhiSwapChain *swapChain);

    VkFormat optimalDepthStencilFormat();
    VkSampleCountFlagBits effectiveSampleCount(int sampleCount);
    bool createDefaultRenderPass(VkRenderPass *rp, bool hasDepthStencil, VkSampleCountFlagBits sampleCount, VkFormat colorFormat);
    bool ensurePipelineCache();
    VkShaderModule createShader(const QByteArray &spirv);

    QRhi::FrameOpResult beginWrapperFrame(QRhiSwapChain *swapChain);
    QRhi::FrameOpResult endWrapperFrame(QRhiSwapChain *swapChain);
    QRhi::FrameOpResult beginNonWrapperFrame(QRhiSwapChain *swapChain);
    QRhi::FrameOpResult endNonWrapperFrame(QRhiSwapChain *swapChain);
    void prepareNewFrame(QRhiCommandBuffer *cb);
    void finishFrame();
    void applyPassUpdates(QRhiCommandBuffer *cb, const QRhi::PassUpdates &updates);
    void executeBufferHostWritesForCurrentFrame(QVkBuffer *bufD);
    void activateTextureRenderTarget(QRhiCommandBuffer *cb, QRhiTextureRenderTarget *rt);
    void deactivateTextureRenderTarget(QRhiCommandBuffer *cb, QRhiTextureRenderTarget *rt);
    void executeDeferredReleases(bool forced = false);

    void bufferBarrier(QRhiCommandBuffer *cb, QRhiBuffer *buf);
    void imageBarrier(QRhiCommandBuffer *cb, QRhiTexture *tex,
                      VkImageLayout newLayout,
                      VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

    // Lighter than release+build, does not allow layout change, but pulls in
    // any new underlying resources from the referenced buffers, textures, etc.
    // in case they changed in the meantime.
    void updateShaderResourceBindings(QRhiShaderResourceBindings *srb, int descSetIdx = -1);

    QRhi *q;
    QVulkanInstance *inst = nullptr;
    QWindow *maybeWindow = nullptr;
    bool importedDevPoolQueue = false;
    VkPhysicalDevice physDev = VK_NULL_HANDLE;
    VkDevice dev = VK_NULL_HANDLE;
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VkQueue gfxQueue = VK_NULL_HANDLE;
    QVkAllocator allocator;
    QVulkanFunctions *f = nullptr;
    QVulkanDeviceFunctions *df = nullptr;
    VkPhysicalDeviceProperties physDevProperties;
    VkDeviceSize ubufAlign;

    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;

    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    struct DescriptorPoolData {
        DescriptorPoolData() { }
        DescriptorPoolData(VkDescriptorPool pool_)
            : pool(pool_)
        { }
        VkDescriptorPool pool = VK_NULL_HANDLE;
        int refCount = 0;
        int allocedDescSets = 0;
    };
    QVector<DescriptorPoolData> descriptorPools;

    VkFormat optimalDsFormat = VK_FORMAT_UNDEFINED;
    QMatrix4x4 clipCorrectMatrix;

    int currentFrameSlot = 0; // 0..FRAMES_IN_FLIGHT-1
    bool inFrame = false;
    int finishedFrameCount = 0;
    bool inPass = false;

    struct DeferredReleaseEntry {
        enum Type {
            Pipeline,
            ShaderResourceBindings,
            Buffer,
            RenderBuffer,
            Texture,
            Sampler,
            TextureRenderTarget,
            RenderPass
        };
        Type type;
        int lastActiveFrameSlot; // -1 if not used otherwise 0..FRAMES_IN_FLIGHT-1
        union {
            struct {
                VkPipeline pipeline;
                VkPipelineLayout layout;
            } pipelineState;
            struct {
                int poolIndex;
                VkDescriptorSetLayout layout;
            } shaderResourceBindings;
            struct {
                VkBuffer buffers[QVK_FRAMES_IN_FLIGHT];
                QVkAlloc allocations[QVK_FRAMES_IN_FLIGHT];
                VkBuffer stagingBuffer;
                QVkAlloc stagingAlloc;
            } buffer;
            struct {
                VkDeviceMemory memory;
                VkImage image;
                VkImageView imageView;
            } renderBuffer;
            struct {
                VkImage image;
                VkImageView imageView;
                QVkAlloc allocation;
                VkBuffer stagingBuffer;
                QVkAlloc stagingAlloc;
            } texture;
            struct {
                VkSampler sampler;
            } sampler;
            struct {
                VkFramebuffer fb;
            } textureRenderTarget;
            struct {
                VkRenderPass rp;
            } renderPass;
        };
    };
    QVector<DeferredReleaseEntry> releaseQueue;
};

Q_DECLARE_TYPEINFO(QRhiVulkan::DescriptorPoolData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiVulkan::DeferredReleaseEntry, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
