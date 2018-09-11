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

class QVkRenderPrivate
{
public:
    QVkRenderPrivate(QVkRender *q_ptr) : q(q_ptr) { }
    void create();
    void destroy();
    VkResult createDescriptorPool(VkDescriptorPool *pool);
    bool allocateDescriptorSet(VkDescriptorSetAllocateInfo *allocInfo, VkDescriptorSet *result, int *resultPoolIndex);
    uint32_t chooseTransientImageMemType(VkImage img, uint32_t startIndex);
    bool createTransientImage(VkFormat format, const QSize &pixelSize, VkImageUsageFlags usage,
                              VkImageAspectFlags aspectMask, VkSampleCountFlagBits sampleCount,
                              VkDeviceMemory *mem, VkImage *images, VkImageView *views, int count);

    bool recreateSwapChain(VkSurfaceKHR surface, const QSize &pixelSize, QVkRender::SurfaceImportFlags flags, QVkSwapChain *swapChain);
    void releaseSwapChain(QVkSwapChain *swapChain);

    VkFormat optimalDepthStencilFormat();
    VkSampleCountFlagBits effectiveSampleCount(int sampleCount);
    bool createDefaultRenderPass(QVkRenderPass *rp, bool hasDepthStencil, VkSampleCountFlagBits sampleCount, VkFormat colorFormat);
    bool ensurePipelineCache();
    VkShaderModule createShader(const QByteArray &spirv);

    void prepareNewFrame(QVkCommandBuffer *cb);
    void finishFrame();
    void applyPassUpdates(QVkCommandBuffer *cb, const QVkRender::PassUpdates &updates);
    void activateTextureRenderTarget(QVkCommandBuffer *cb, QVkTextureRenderTarget *rt);
    void deactivateTextureRenderTarget(QVkCommandBuffer *cb, QVkTextureRenderTarget *rt);
    void executeDeferredReleases(bool forced = false);

    void bufferBarrier(QVkCommandBuffer *cb, QVkBuffer *buf);
    enum WhichImage { TextureImage, StagingImage };
    void imageBarrier(QVkCommandBuffer *cb, QVkTexture *tex, WhichImage which,
                      VkImageLayout newLayout,
                      VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

    QVkRender *q;
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
        int activeSets = 0;
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
            TextureRenderTarget
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
                VkImage stagingImage;
                QVkAlloc stagingAlloc;
            } texture;
            struct {
                VkSampler sampler;
            } sampler;
            struct {
                VkFramebuffer fb;
                VkRenderPass rp;
            } textureRenderTarget;
        };
    };
    QVector<DeferredReleaseEntry> releaseQueue;
};

QT_END_NAMESPACE

#endif
