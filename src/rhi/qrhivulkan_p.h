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
class QRhiResourceSharingHostPrivate;

static const int QVK_FRAMES_IN_FLIGHT = 2;

static const int QVK_DESC_SETS_PER_POOL = 128;
static const int QVK_UNIFORM_BUFFERS_PER_POOL = 256;
static const int QVK_COMBINED_IMAGE_SAMPLERS_PER_POOL = 256;

static const int QVK_MAX_ACTIVE_TIMESTAMP_PAIRS = 16;

// no vk_mem_alloc.h available here, void* is good enough
typedef void * QVkAlloc;
typedef void * QVkAllocator;

struct QVkBuffer : public QRhiBuffer
{
    QVkBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    bool isShareable() const override;
    void release() override;
    bool build() override;

    VkBuffer buffers[QVK_FRAMES_IN_FLIGHT];
    QVkAlloc allocations[QVK_FRAMES_IN_FLIGHT];
    QVector<QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate> pendingDynamicUpdates[QVK_FRAMES_IN_FLIGHT];
    VkBuffer stagingBuffers[QVK_FRAMES_IN_FLIGHT];
    QVkAlloc stagingAllocations[QVK_FRAMES_IN_FLIGHT];
    int lastActiveFrameSlot = -1;
    uint generation = 0;
    friend class QRhiVulkan;
};

struct QVkTexture;

struct QVkRenderBuffer : public QRhiRenderBuffer
{
    QVkRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                    int sampleCount, Flags flags);
    ~QVkRenderBuffer();
    bool isShareable() const override;
    void release() override;
    bool build() override;
    QRhiTexture::Format backingFormat() const override;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampleCountFlagBits samples;
    QVkTexture *backingTexture = nullptr;
    VkFormat vkformat;
    int lastActiveFrameSlot = -1;
    friend class QRhiVulkan;
};

struct QVkTexture : public QRhiTexture
{
    QVkTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
               int sampleCount, Flags flags);
    bool isShareable() const override;
    void release() override;
    bool build() override;
    bool buildFrom(const QRhiNativeHandles *src) override;
    const QRhiNativeHandles *nativeHandles() override;

    bool prepareBuild(QSize *adjustedSize = nullptr);
    bool finishBuild();

    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    QVkAlloc imageAlloc = nullptr;
    VkBuffer stagingBuffers[QVK_FRAMES_IN_FLIGHT];
    QVkAlloc stagingAllocations[QVK_FRAMES_IN_FLIGHT];
    bool owns = true;
    QRhiVulkanTextureNativeHandles nativeHandlesStruct;
    VkImageLayout layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    VkFormat vkformat;
    uint mipLevelCount = 0;
    VkSampleCountFlagBits samples;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
    friend class QRhiVulkan;
};

struct QVkSampler : public QRhiSampler
{
    QVkSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
               AddressMode u, AddressMode v, AddressMode w);
    bool isShareable() const override;
    void release() override;
    bool build() override;

    VkSampler sampler = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
    friend class QRhiVulkan;
};

struct QVkRenderPassDescriptor : public QRhiRenderPassDescriptor
{
    QVkRenderPassDescriptor(QRhiImplementation *rhi);
    void release() override;

    VkRenderPass rp = VK_NULL_HANDLE;
    bool ownsRp = false;
    int lastActiveFrameSlot = -1;
};

struct QVkRenderTargetData
{
    VkFramebuffer fb = VK_NULL_HANDLE;
    QVkRenderPassDescriptor *rp = nullptr;
    QSize pixelSize;
    float dpr = 1;
    int colorAttCount = 0;
    int dsAttCount = 0;
    int resolveAttCount = 0;
    static const int MAX_COLOR_ATTACHMENTS = 8;
};

struct QVkReferenceRenderTarget : public QRhiReferenceRenderTarget
{
    QVkReferenceRenderTarget(QRhiImplementation *rhi);
    void release() override;
    Type type() const override;
    QSize sizeInPixels() const override;
    float devicePixelRatio() const override;

    QVkRenderTargetData d;
};

struct QVkTextureRenderTarget : public QRhiTextureRenderTarget
{
    QVkTextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags);
    void release() override;

    Type type() const override;
    QSize sizeInPixels() const override;
    float devicePixelRatio() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool build() override;

    QVkRenderTargetData d;
    VkImageView rtv[QVkRenderTargetData::MAX_COLOR_ATTACHMENTS];
    VkImageView resrtv[QVkRenderTargetData::MAX_COLOR_ATTACHMENTS];
    int lastActiveFrameSlot = -1;
    friend class QRhiVulkan;
};

struct QVkShaderResourceBindings : public QRhiShaderResourceBindings
{
    QVkShaderResourceBindings(QRhiImplementation *rhi);
    void release() override;
    bool build() override;

    QVector<QRhiShaderResourceBinding> sortedBindings;
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

    friend class QRhiVulkan;
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
    friend class QRhiVulkan;
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
        currentDescSetSlot = -1;
        currentIndexBuffer = VK_NULL_HANDLE;
        currentIndexOffset = 0;
        currentIndexFormat = VK_INDEX_TYPE_UINT16;
        memset(currentVertexBuffers, 0, sizeof(currentVertexBuffers));
        memset(currentVertexOffsets, 0, sizeof(currentVertexOffsets));
    }

    QRhiRenderTarget *currentTarget;
    QRhiGraphicsPipeline *currentPipeline;
    uint currentPipelineGeneration;
    QRhiShaderResourceBindings *currentSrb;
    uint currentSrbGeneration;
    int currentDescSetSlot;
    VkBuffer currentIndexBuffer;
    quint32 currentIndexOffset;
    VkIndexType currentIndexFormat;
    static const int VERTEX_INPUT_RESOURCE_SLOT_COUNT = 32;
    VkBuffer currentVertexBuffers[VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    quint32 currentVertexOffsets[VERTEX_INPUT_RESOURCE_SLOT_COUNT];

    friend class QRhiVulkan;
};

struct QVkSwapChain : public QRhiSwapChain
{
    QVkSwapChain(QRhiImplementation *rhi);
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;

    QSize surfacePixelSize() override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool buildOrResize() override;

    bool ensureSurface();

    static const int DEFAULT_BUFFER_COUNT = 2;
    static const int MAX_BUFFER_COUNT = 3;

    QVulkanWindow *wrapWindow = nullptr;
    QWindow *window = nullptr;
    QSize pixelSize;
    bool supportsReadback = false;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    int bufferCount = 0;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    QVkRenderBuffer *ds = nullptr;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    QVector<VkPresentModeKHR> supportedPresentationModes;
    VkDeviceMemory msaaImageMem = VK_NULL_HANDLE;
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
        bool presentableLayout = true;
    } imageRes[MAX_BUFFER_COUNT];

    struct FrameResources {
        VkFence fence = VK_NULL_HANDLE;
        bool fenceWaitable = false;
        VkSemaphore imageSem = VK_NULL_HANDLE;
        VkSemaphore drawSem = VK_NULL_HANDLE;
        bool imageAcquired = false;
        bool imageSemWaitable = false;
        quint32 imageIndex = 0;
        int timestampQueryIndex = -1;
    } frameRes[QVK_FRAMES_IN_FLIGHT];

    quint32 currentImageIndex = 0; // index in imageRes
    quint32 currentFrameSlot = 0; // index in frameRes
    int frameCount = 0;

    friend class QRhiVulkan;
};

class QRhiVulkan : public QRhiImplementation
{
public:
    QRhiVulkan(QRhiVulkanInitParams *params, QRhiVulkanNativeHandles *importDevice = nullptr);

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
    QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain, QRhi::BeginFrameFlags flags) override;
    QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain, QRhi::EndFrameFlags flags) override;
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
                             QRhiGraphicsPipeline *ps) override;

    void setShaderResources(QRhiCommandBuffer *cb,
                            QRhiShaderResourceBindings *srb,
                            const QVector<QRhiCommandBuffer::DynamicOffset> &dynamicOffsets) override;

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
    bool isYUpInNDC() const override;
    QMatrix4x4 clipSpaceCorrMatrix() const override;
    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const override;
    bool isFeatureSupported(QRhi::Feature feature) const override;
    int resourceSizeLimit(QRhi::ResourceSizeLimit limit) const override;
    const QRhiNativeHandles *nativeHandles() override;
    void sendVMemStatsToProfiler() override;

    VkResult createDescriptorPool(VkDescriptorPool *pool);
    bool allocateDescriptorSet(VkDescriptorSetAllocateInfo *allocInfo, VkDescriptorSet *result, int *resultPoolIndex);
    uint32_t chooseTransientImageMemType(VkImage img, uint32_t startIndex);
    bool createTransientImage(VkFormat format, const QSize &pixelSize, VkImageUsageFlags usage,
                              VkImageAspectFlags aspectMask, VkSampleCountFlagBits samples,
                              VkDeviceMemory *mem, VkImage *images, VkImageView *views, int count);

    bool recreateSwapChain(QRhiSwapChain *swapChain);
    void releaseSwapChainResources(QRhiSwapChain *swapChain);

    VkFormat optimalDepthStencilFormat();
    VkSampleCountFlagBits effectiveSampleCount(int sampleCount);
    bool createDefaultRenderPass(VkRenderPass *rp,
                                 bool hasDepthStencil,
                                 VkSampleCountFlagBits samples,
                                 VkFormat colorFormat);
    bool createOffscreenRenderPass(VkRenderPass *rp,
                                   const QVector<QRhiColorAttachment> &colorAttachments,
                                   bool preserveColor,
                                   bool preserveDs,
                                   QRhiRenderBuffer *depthStencilBuffer,
                                   QRhiTexture *depthTexture);
    bool ensurePipelineCache();
    VkShaderModule createShader(const QByteArray &spirv);

    QRhi::FrameOpResult beginWrapperFrame(QRhiSwapChain *swapChain);
    QRhi::FrameOpResult endWrapperFrame(QRhiSwapChain *swapChain);
    QRhi::FrameOpResult startCommandBuffer(VkCommandBuffer *cb);
    QRhi::FrameOpResult endAndSubmitCommandBuffer(VkCommandBuffer cb, VkFence cmdFence,
                                                  VkSemaphore *waitSem, VkSemaphore *signalSem);
    void waitCommandCompletion(int frameSlot);
    QRhi::FrameOpResult beginNonWrapperFrame(QRhiSwapChain *swapChain);
    QRhi::FrameOpResult endNonWrapperFrame(QRhiSwapChain *swapChain, QRhi::EndFrameFlags flags);
    void prepareNewFrame(QRhiCommandBuffer *cb);
    void prepareForTransferDest(QRhiCommandBuffer *cb, QVkTexture *texD);
    void prepareForTransferSrc(QRhiCommandBuffer *cb, QVkTexture *texD);
    void finishTransferDest(QRhiCommandBuffer *cb, QVkTexture *texD);
    void finishTransferSrc(QRhiCommandBuffer *cb, QVkTexture *texD);
    void enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates);
    void executeBufferHostWritesForCurrentFrame(QVkBuffer *bufD);
    void activateTextureRenderTarget(QRhiCommandBuffer *cb, QRhiTextureRenderTarget *rt);
    void deactivateTextureRenderTarget(QRhiCommandBuffer *cb, QRhiTextureRenderTarget *rt);
    void executeDeferredReleases(bool forced = false);
    void finishActiveReadbacks(bool forced = false);

    void setObjectName(uint64_t object, VkDebugReportObjectTypeEXT type, const QByteArray &name, int slot = -1);
    void bufferBarrier(QRhiCommandBuffer *cb, QRhiBuffer *buf);
    void imageSubResBarrier(QRhiCommandBuffer *cb, QRhiTexture *tex,
                            VkImageLayout oldLayout, VkImageLayout newLayout,
                            VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                            VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                            int startLayer, int layerCount,
                            int startLevel, int levelCount);
    void imageBarrier(QRhiCommandBuffer *cb, QRhiTexture *tex,
                      VkImageLayout newLayout,
                      VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

    // Lighter than release+build, does not allow layout change, but pulls in
    // any new underlying resources from the referenced buffers, textures, etc.
    // in case they changed in the meantime.
    void updateShaderResourceBindings(QRhiShaderResourceBindings *srb, int descSetIdx = -1);

    QVulkanInstance *inst = nullptr;
    QWindow *maybeWindow = nullptr;
    bool importedDevice = false;
    VkPhysicalDevice physDev = VK_NULL_HANDLE;
    VkDevice dev = VK_NULL_HANDLE;
    bool importedCmdPool = false;
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    int gfxQueueFamilyIdx = -1;
    VkQueue gfxQueue = VK_NULL_HANDLE;
    quint32 timestampValidBits = 0;
    bool importedAllocator = false;
    QVkAllocator allocator = nullptr;
    QVulkanFunctions *f = nullptr;
    QVulkanDeviceFunctions *df = nullptr;
    VkPhysicalDeviceProperties physDevProperties;
    VkDeviceSize ubufAlign;
    VkDeviceSize texbufAlign;

    bool debugMarkersAvailable = false;
    bool vertexAttribDivisorAvailable = false;
    PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin = nullptr;
    PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd = nullptr;
    PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert = nullptr;
    PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName = nullptr;

    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;

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

    VkQueryPool timestampQueryPool = VK_NULL_HANDLE;
    QBitArray timestampQueryPoolMap;

    VkFormat optimalDsFormat = VK_FORMAT_UNDEFINED;
    QMatrix4x4 clipCorrectMatrix;

    int currentFrameSlot = 0; // 0..FRAMES_IN_FLIGHT-1
    bool inFrame = false;
    bool inPass = false;
    QVkSwapChain *currentSwapChain = nullptr;
    QSet<QVkSwapChain *> swapchains;
    QRhiVulkanNativeHandles nativeHandlesStruct;

    struct OffscreenFrame {
        OffscreenFrame(QRhiImplementation *rhi) : cbWrapper(rhi) { }
        bool active = false;
        QVkCommandBuffer cbWrapper;
        VkFence cmdFence = VK_NULL_HANDLE;
    } ofr;

    struct ActiveReadback {
        int activeFrameSlot = -1;
        QRhiReadbackDescription desc;
        QRhiReadbackResult *result;
        VkBuffer buf;
        QVkAlloc bufAlloc;
        quint32 bufSize;
        QSize pixelSize;
        QRhiTexture::Format format;
    };
    QVector<ActiveReadback> activeReadbacks;

    struct DeferredReleaseEntry {
        enum Type {
            Pipeline,
            ShaderResourceBindings,
            Buffer,
            RenderBuffer,
            Texture,
            Sampler,
            TextureRenderTarget,
            RenderPass,
            StagingBuffer
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
                VkBuffer stagingBuffers[QVK_FRAMES_IN_FLIGHT];
                QVkAlloc stagingAllocations[QVK_FRAMES_IN_FLIGHT];
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
                VkBuffer stagingBuffers[QVK_FRAMES_IN_FLIGHT];
                QVkAlloc stagingAllocations[QVK_FRAMES_IN_FLIGHT];
            } texture;
            struct {
                VkSampler sampler;
            } sampler;
            struct {
                VkFramebuffer fb;
                VkImageView rtv[QVkRenderTargetData::MAX_COLOR_ATTACHMENTS];
                VkImageView resrtv[QVkRenderTargetData::MAX_COLOR_ATTACHMENTS];
            } textureRenderTarget;
            struct {
                VkRenderPass rp;
            } renderPass;
            struct {
                VkBuffer stagingBuffer;
                QVkAlloc stagingAllocation;
            } stagingBuffer;
        };
    };
    QVector<DeferredReleaseEntry> releaseQueue;

    static void executeDeferredReleasesOnRshNow(QRhiResourceSharingHostPrivate *rsh,
                                                QVector<DeferredReleaseEntry> *rshRelQueue);
};

Q_DECLARE_TYPEINFO(QRhiVulkan::DescriptorPoolData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiVulkan::DeferredReleaseEntry, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiVulkan::ActiveReadback, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
