/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt VkRender module
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

#ifndef QVKRENDER_P_H
#define QVKRENDER_P_H

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

#include "qvkrender.h"
#include <QVector>

QT_BEGIN_NAMESPACE

class QVulkanFunctions;
class QVulkanDeviceFunctions;
class QWindow;

static const int QVK_DESC_SETS_PER_POOL = 128;
static const int QVK_UNIFORM_BUFFERS_PER_POOL = 256;
static const int QVK_COMBINED_IMAGE_SAMPLERS_PER_POOL = 256;

class QRhiResourcePrivate
{
public:
    virtual ~QRhiResourcePrivate();
    static QRhiResourcePrivate *get(QRhiResource *r) { return r->d_ptr; }
    static const QRhiResourcePrivate *get(const QRhiResource *r) { return r->d_ptr; }
    QRhi *rhi = nullptr;
};

typedef void * QVkAlloc;

class QVkBuffer : public QRhiBuffer
{
public:
    QVkBuffer(QRhi *rhi, Type type, UsageFlags usage, int size);
    void release() override;
    bool build() override;
};

struct QVkBufferPrivate : public QRhiResourcePrivate
{
    VkBuffer buffers[QVK_FRAMES_IN_FLIGHT];
    QVkAlloc allocations[QVK_FRAMES_IN_FLIGHT];
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    QVkAlloc stagingAlloc = nullptr;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
};

class QVkRenderBuffer : public QRhiRenderBuffer
{
public:
    QVkRenderBuffer(QRhi *rhi, Type type, const QSize &pixelSize, int sampleCount);
    void release() override;
    bool build() override;
};

struct QVkRenderBufferPrivate : public QRhiResourcePrivate
{
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImage image;
    VkImageView imageView;
    int lastActiveFrameSlot = -1;
};

class QVkTexture : public QRhiTexture
{
public:
    QVkTexture(QRhi *rhi, Format format, const QSize &pixelSize, Flags flags);
    void release() override;
    bool build() override;
};

struct QVkTexturePrivate : public QRhiResourcePrivate
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    QVkAlloc allocation = nullptr;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    QVkAlloc stagingAlloc = nullptr;
    VkImageLayout layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
};

class QVkSampler : public QRhiSampler
{
public:
    QVkSampler(QRhi *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode, AddressMode u, AddressMode v);
    void release() override;
    bool build() override;
};

struct QVkSamplerPrivate : public QRhiResourcePrivate
{
    VkSampler sampler = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
    uint generation = 0;
};

class QVkRenderPass : public QRhiRenderPass
{
public:
    QVkRenderPass(QRhi *rhi);
    void release() override;
};

struct QVkRenderPassPrivate : public QRhiResourcePrivate
{
    VkRenderPass rp = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
};

struct QVkRenderTargetPrivate;

class QVkRenderTarget : public QRhiRenderTarget
{
public:
    QVkRenderTarget(QRhi *rhi);
    void release() override;
    QSize sizeInPixels() const override;
    const QRhiRenderPass *renderPass() const override;

protected:
    QVkRenderTarget(QRhi *rhi, QVkRenderTargetPrivate *d);
};

struct QVkRenderTargetPrivate : public QRhiResourcePrivate
{
    QVkRenderTargetPrivate(QRhi *rhi) : rp(rhi) { }
    VkFramebuffer fb = VK_NULL_HANDLE;
    QVkRenderPass rp;
    QSize pixelSize;
    int attCount = 0;
    enum Type {
        RtRef,    // no Vk* are owned directly by the object
        RtTexture // this is a QVkTextureRenderTarget, owns
    };
    Type type = RtRef;
};

class QVkTextureRenderTarget : public QRhiTextureRenderTarget
{
public:
    QVkTextureRenderTarget(QRhi *rhi, QRhiTexture *texture, Flags flags);
    QVkTextureRenderTarget(QRhi *rhi, QRhiTexture *texture, QRhiRenderBuffer *depthStencilBuffer, Flags flags);
    QVkTextureRenderTarget(QRhi *rhi, QRhiTexture *texture, QRhiTexture *depthTexture, Flags flags);
    void release() override;
    bool build() override;
    QSize sizeInPixels() const override;
    const QRhiRenderPass *renderPass() const override;
};

struct QVkTextureRenderTargetPrivate : public QVkRenderTargetPrivate
{
    QVkTextureRenderTargetPrivate(QRhi *rhi)
        : QVkRenderTargetPrivate(rhi)
    {
        type = RtTexture;
    }

    int lastActiveFrameSlot = -1;
};

class QVkShaderResourceBindings : public QRhiShaderResourceBindings
{
public:
    QVkShaderResourceBindings(QRhi *rhi);
    void release() override;
    bool build() override;
};

struct QVkShaderResourceBindingsPrivate : public QRhiResourcePrivate
{
    int poolIndex = -1;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet descSets[QVK_FRAMES_IN_FLIGHT]; // multiple sets to support dynamic buffers
    int lastActiveFrameSlot = -1;

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

class QVkGraphicsPipeline : public QRhiGraphicsPipeline
{
public:
    QVkGraphicsPipeline(QRhi *rhi);
    void release() override;
    bool build() override;
};

struct QVkGraphicsPipelinePrivate : public QRhiResourcePrivate
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    int lastActiveFrameSlot = -1;
};

class QVkCommandBuffer : public QRhiCommandBuffer
{
public:
    QVkCommandBuffer(QRhi *rhi);
    void release() override;
};

struct QVkCommandBufferPrivate : public QRhiResourcePrivate
{
    QVkCommandBufferPrivate() { resetState(); }

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

class QVkSwapChain : public QRhiSwapChain
{
public:
    QVkSwapChain(QRhi *rhi);
    void release() override;
    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;
    const QRhiRenderPass *defaultRenderPass() const override;
    QSize sizeInPixels() const override;
    bool build(QWindow *window, const QSize &pixelSize, SurfaceImportFlags flags,
               QRhiRenderBuffer *depthStencil, int sampleCount) override;
};

struct QVkSwapChainPrivate : public QRhiResourcePrivate
{
    QVkSwapChainPrivate(QRhi *rhi) : rtWrapper(rhi) { }

    static const int DEFAULT_BUFFER_COUNT = 2;
    static const int MAX_BUFFER_COUNT = 3;

    QSize pixelSize;
    bool supportsReadback = false;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    int bufferCount = 0;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    QRhiRenderBuffer *depthStencil = nullptr;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkDeviceMemory msaaImageMem = VK_NULL_HANDLE;
    VkRenderPass rp;
    QVkRenderTarget rtWrapper;

    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
        QVkCommandBuffer *cmdBufWrapper = nullptr;
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

class QRhiPrivate
{
public:
    static QRhiPrivate *get(QRhi *r) { return r->d_ptr; }
};

class QRhiVulkan : public QRhiPrivate
{
public:
    QRhiVulkan(QRhi *q_ptr) : q(q_ptr) { }
    void create();
    void destroy();
    VkResult createDescriptorPool(VkDescriptorPool *pool);
    bool allocateDescriptorSet(VkDescriptorSetAllocateInfo *allocInfo, VkDescriptorSet *result, int *resultPoolIndex);
    uint32_t chooseTransientImageMemType(VkImage img, uint32_t startIndex);
    bool createTransientImage(VkFormat format, const QSize &pixelSize, VkImageUsageFlags usage,
                              VkImageAspectFlags aspectMask, VkSampleCountFlagBits sampleCount,
                              VkDeviceMemory *mem, VkImage *images, VkImageView *views, int count);

    bool rebuildSwapChain(QWindow *window, const QSize &pixelSize,
                          QRhiSwapChain::SurfaceImportFlags flags, QRhiRenderBuffer *depthStencil,
                          int sampleCount, QRhiSwapChain *outSwapChain);
    bool recreateSwapChain(VkSurfaceKHR surface, const QSize &pixelSize, QRhiSwapChain::SurfaceImportFlags flags,
                           QRhiSwapChain *swapChain);
    void releaseSwapChainResources(QRhiSwapChain *swapChain);

    VkFormat optimalDepthStencilFormat();
    VkSampleCountFlagBits effectiveSampleCount(int sampleCount);
    bool createDefaultRenderPass(VkRenderPass *rp, bool hasDepthStencil, VkSampleCountFlagBits sampleCount, VkFormat colorFormat);
    bool ensurePipelineCache();
    VkShaderModule createShader(const QByteArray &spirv);

    void prepareNewFrame(QRhiCommandBuffer *cb);
    void finishFrame();
    void applyPassUpdates(QRhiCommandBuffer *cb, const QRhi::PassUpdates &updates);
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
    QVulkanInstance *inst;
    VkPhysicalDevice physDev;
    VkDevice dev;
    VkCommandPool cmdPool;
    VkQueue gfxQueue;
    VmaAllocator allocator;
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

QT_END_NAMESPACE

#endif
