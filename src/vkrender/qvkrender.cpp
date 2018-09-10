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

#include "qvkrender.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_RECORDING_ENABLED 0
#ifdef QT_DEBUG
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#endif
#include "vk_mem_alloc.h"

#include "qvkrender_p.h"

#include <QVulkanFunctions>
#include <QVulkanWindow>
#include <QElapsedTimer>

QT_BEGIN_NAMESPACE

QVkRender::QVkRender(const InitParams &params)
    : d(new QVkRenderPrivate(this))
{
    d->inst = params.inst;
    d->physDev = params.physDev;
    d->dev = params.dev;
    d->cmdPool = params.cmdPool;
    d->gfxQueue = params.gfxQueue;

    d->create();
}

QVkRender::~QVkRender()
{
    d->destroy();
    delete d;
}

static inline VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign)
{
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

static QVulkanInstance *globalVulkanInstance;

static void VKAPI_PTR wrap_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties)
{
    globalVulkanInstance->functions()->vkGetPhysicalDeviceProperties(physicalDevice, pProperties);
}

static void VKAPI_PTR wrap_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    globalVulkanInstance->functions()->vkGetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

static VkResult VKAPI_PTR wrap_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
{
    return globalVulkanInstance->deviceFunctions(device)->vkAllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
}

void VKAPI_PTR wrap_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
{
    globalVulkanInstance->deviceFunctions(device)->vkFreeMemory(device, memory, pAllocator);
}

VkResult VKAPI_PTR wrap_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)
{
    return globalVulkanInstance->deviceFunctions(device)->vkMapMemory(device, memory, offset, size, flags, ppData);
}

void VKAPI_PTR wrap_vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
    globalVulkanInstance->deviceFunctions(device)->vkUnmapMemory(device, memory);
}

VkResult VKAPI_PTR wrap_vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
    return globalVulkanInstance->deviceFunctions(device)->vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

VkResult VKAPI_PTR wrap_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
    return globalVulkanInstance->deviceFunctions(device)->vkInvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

VkResult VKAPI_PTR wrap_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    return globalVulkanInstance->deviceFunctions(device)->vkBindBufferMemory(device, buffer, memory, memoryOffset);
}

VkResult VKAPI_PTR wrap_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    return globalVulkanInstance->deviceFunctions(device)->vkBindImageMemory(device, image, memory, memoryOffset);
}

void VKAPI_PTR wrap_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements)
{
    globalVulkanInstance->deviceFunctions(device)->vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

void VKAPI_PTR wrap_vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements)
{
    globalVulkanInstance->deviceFunctions(device)->vkGetImageMemoryRequirements(device, image, pMemoryRequirements);
}

VkResult VKAPI_PTR wrap_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
    return globalVulkanInstance->deviceFunctions(device)->vkCreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
}

void VKAPI_PTR wrap_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator)
{
    globalVulkanInstance->deviceFunctions(device)->vkDestroyBuffer(device, buffer, pAllocator);
}

VkResult VKAPI_PTR wrap_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
{
    return globalVulkanInstance->deviceFunctions(device)->vkCreateImage(device, pCreateInfo, pAllocator, pImage);
}

void VKAPI_PTR wrap_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
{
    globalVulkanInstance->deviceFunctions(device)->vkDestroyImage(device, image, pAllocator);
}

void QVkRenderPrivate::create()
{
    Q_ASSERT(inst && physDev && dev && cmdPool && gfxQueue);

    globalVulkanInstance = inst; // assume this will not change during the lifetime of the entire application

    f = inst->functions();
    df = inst->deviceFunctions(dev);

    VmaVulkanFunctions afuncs;
    afuncs.vkGetPhysicalDeviceProperties = wrap_vkGetPhysicalDeviceProperties;
    afuncs.vkGetPhysicalDeviceMemoryProperties = wrap_vkGetPhysicalDeviceMemoryProperties;
    afuncs.vkAllocateMemory = wrap_vkAllocateMemory;
    afuncs.vkFreeMemory = wrap_vkFreeMemory;
    afuncs.vkMapMemory = wrap_vkMapMemory;
    afuncs.vkUnmapMemory = wrap_vkUnmapMemory;
    afuncs.vkFlushMappedMemoryRanges = wrap_vkFlushMappedMemoryRanges;
    afuncs.vkInvalidateMappedMemoryRanges = wrap_vkInvalidateMappedMemoryRanges;
    afuncs.vkBindBufferMemory = wrap_vkBindBufferMemory;
    afuncs.vkBindImageMemory = wrap_vkBindImageMemory;
    afuncs.vkGetBufferMemoryRequirements = wrap_vkGetBufferMemoryRequirements;
    afuncs.vkGetImageMemoryRequirements = wrap_vkGetImageMemoryRequirements;
    afuncs.vkCreateBuffer = wrap_vkCreateBuffer;
    afuncs.vkDestroyBuffer = wrap_vkDestroyBuffer;
    afuncs.vkCreateImage = wrap_vkCreateImage;
    afuncs.vkDestroyImage = wrap_vkDestroyImage;

    f->vkGetPhysicalDeviceProperties(physDev, &physDevProperties);
    ubufAlign = physDevProperties.limits.minUniformBufferOffsetAlignment;

    VmaAllocatorCreateInfo allocatorInfo;
    memset(&allocatorInfo, 0, sizeof(allocatorInfo));
    allocatorInfo.physicalDevice = physDev;
    allocatorInfo.device = dev;
    allocatorInfo.pVulkanFunctions = &afuncs;
    vmaCreateAllocator(&allocatorInfo, &allocator);

    VkDescriptorPool pool;
    VkResult err = createDescriptorPool(&pool);
    if (err == VK_SUCCESS)
        descriptorPools.append(pool);
    else
        qWarning("Failed to create initial descriptor pool: %d", err);
}

VkResult QVkRenderPrivate::createDescriptorPool(VkDescriptorPool *pool)
{
    VkDescriptorPoolSize descPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, QVK_UNIFORM_BUFFERS_PER_POOL },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, QVK_COMBINED_IMAGE_SAMPLERS_PER_POOL }
    };
    VkDescriptorPoolCreateInfo descPoolInfo;
    memset(&descPoolInfo, 0, sizeof(descPoolInfo));
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    // Do not enable vkFreeDescriptorSets - sets are never freed on their own
    // (good so no trouble with fragmentation), they just deref their pool
    // which is then reset at some point (or not).
    descPoolInfo.flags = 0;
    descPoolInfo.maxSets = QVK_DESC_SETS_PER_POOL;
    descPoolInfo.poolSizeCount = sizeof(descPoolSizes) / sizeof(descPoolSizes[0]);
    descPoolInfo.pPoolSizes = descPoolSizes;
    return df->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, pool);
}

bool QVkRenderPrivate::allocateDescriptorSet(VkDescriptorSetAllocateInfo *allocInfo, VkDescriptorSet *result, int *resultPoolIndex)
{
    auto tryAllocate = [this, allocInfo, result](int poolIndex) {
        allocInfo->descriptorPool = descriptorPools[poolIndex].pool;
        VkResult r = df->vkAllocateDescriptorSets(dev, allocInfo, result);
        if (r == VK_SUCCESS)
            descriptorPools[poolIndex].activeSets += 1;
        return r;
    };

    int lastPoolIdx = descriptorPools.count() - 1;
    for (int i = lastPoolIdx; i >= 0; --i) {
        if (descriptorPools[i].activeSets == 0)
            df->vkResetDescriptorPool(dev, descriptorPools[i].pool, 0);
        VkResult err = tryAllocate(i);
        if (err == VK_SUCCESS) {
            *resultPoolIndex = i;
            return true;
        }
    }

    VkDescriptorPool newPool;
    VkResult poolErr = createDescriptorPool(&newPool);
    if (poolErr == VK_SUCCESS) {
        descriptorPools.append(newPool);
        lastPoolIdx = descriptorPools.count() - 1;
        VkResult err = tryAllocate(lastPoolIdx);
        if (err != VK_SUCCESS) {
            qWarning("Failed to allocate descriptor set from new pool too, giving up: %d", err);
            return false;
        }
        *resultPoolIndex = lastPoolIdx;
        return true;
    } else {
        qWarning("Failed to allocate new descriptor pool: %d", poolErr);
        return false;
    }
}

// Transient images ("render buffers") backed by lazily allocated memory are
// managed manually without going through vk_mem_alloc since it does not offer
// any support for such images. This should be ok since in practice there
// should be very few of such images.

uint32_t QVkRenderPrivate::chooseTransientImageMemType(VkImage img, uint32_t startIndex)
{
    VkPhysicalDeviceMemoryProperties physDevMemProps;
    f->vkGetPhysicalDeviceMemoryProperties(physDev, &physDevMemProps);

    VkMemoryRequirements memReq;
    df->vkGetImageMemoryRequirements(dev, img, &memReq);
    uint32_t memTypeIndex = uint32_t(-1);

    if (memReq.memoryTypeBits) {
        // Find a device local + lazily allocated, or at least device local memtype.
        const VkMemoryType *memType = physDevMemProps.memoryTypes;
        bool foundDevLocal = false;
        for (uint32_t i = startIndex; i < physDevMemProps.memoryTypeCount; ++i) {
            if (memReq.memoryTypeBits & (1 << i)) {
                if (memType[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                    if (!foundDevLocal) {
                        foundDevLocal = true;
                        memTypeIndex = i;
                    }
                    if (memType[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
                        memTypeIndex = i;
                        break;
                    }
                }
            }
        }
    }

    return memTypeIndex;
}

bool QVkRenderPrivate::createTransientImage(VkFormat format,
                                            const QSize &pixelSize,
                                            VkImageUsageFlags usage,
                                            VkImageAspectFlags aspectMask,
                                            VkSampleCountFlagBits sampleCount,
                                            VkDeviceMemory *mem,
                                            VkImage *images,
                                            VkImageView *views,
                                            int count)
{
    VkMemoryRequirements memReq;
    VkResult err;

    for (int i = 0; i < count; ++i) {
        VkImageCreateInfo imgInfo;
        memset(&imgInfo, 0, sizeof(imgInfo));
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = format;
        imgInfo.extent.width = pixelSize.width();
        imgInfo.extent.height = pixelSize.height();
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = imgInfo.arrayLayers = 1;
        imgInfo.samples = sampleCount;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = usage | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

        err = df->vkCreateImage(dev, &imgInfo, nullptr, images + i);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create image: %d", err);
            return false;
        }

        // Assume the reqs are the same since the images are same in every way.
        // Still, call GetImageMemReq for every image, in order to prevent the
        // validation layer from complaining.
        df->vkGetImageMemoryRequirements(dev, images[i], &memReq);
    }

    VkMemoryAllocateInfo memInfo;
    memset(&memInfo, 0, sizeof(memInfo));
    memInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memInfo.allocationSize = aligned(memReq.size, memReq.alignment) * count;

    uint32_t startIndex = 0;
    do {
        memInfo.memoryTypeIndex = chooseTransientImageMemType(images[0], startIndex);
        if (memInfo.memoryTypeIndex == uint32_t(-1)) {
            qWarning("No suitable memory type found");
            return false;
        }
        startIndex = memInfo.memoryTypeIndex + 1;
        err = df->vkAllocateMemory(dev, &memInfo, nullptr, mem);
        if (err != VK_SUCCESS && err != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            qWarning("Failed to allocate image memory: %d", err);
            return false;
        }
    } while (err != VK_SUCCESS);

    VkDeviceSize ofs = 0;
    for (int i = 0; i < count; ++i) {
        err = df->vkBindImageMemory(dev, images[i], *mem, ofs);
        if (err != VK_SUCCESS) {
            qWarning("Failed to bind image memory: %d", err);
            return false;
        }
        ofs += aligned(memReq.size, memReq.alignment);

        VkImageViewCreateInfo imgViewInfo;
        memset(&imgViewInfo, 0, sizeof(imgViewInfo));
        imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imgViewInfo.image = images[i];
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = format;
        imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imgViewInfo.subresourceRange.aspectMask = aspectMask;
        imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;

        err = df->vkCreateImageView(dev, &imgViewInfo, nullptr, views + i);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create image view: %d", err);
            return false;
        }
    }

    return true;
}

void QVkRenderPrivate::destroy()
{
    if (!df)
        return;

    df->vkDeviceWaitIdle(dev);

    executeDeferredReleases(true);

    if (pipelineCache) {
        df->vkDestroyPipelineCache(dev, pipelineCache, nullptr);
        pipelineCache = VK_NULL_HANDLE;
    }

    for (const DescriptorPoolData &pool : descriptorPools)
        df->vkDestroyDescriptorPool(dev, pool.pool, nullptr);

    descriptorPools.clear();

    vmaDestroyAllocator(allocator);

    f = nullptr;
    df = nullptr;
}

static inline VmaAllocation toVmaAllocation(QVkAlloc a)
{
    return reinterpret_cast<VmaAllocation>(a);
}

static const VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

VkFormat QVkRenderPrivate::optimalDepthStencilFormat()
{
    if (optimalDsFormat != VK_FORMAT_UNDEFINED)
        return optimalDsFormat;

    const VkFormat dsFormatCandidates[] = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT
    };
    const int dsFormatCandidateCount = sizeof(dsFormatCandidates) / sizeof(VkFormat);
    int dsFormatIdx = 0;
    while (dsFormatIdx < dsFormatCandidateCount) {
        optimalDsFormat = dsFormatCandidates[dsFormatIdx];
        VkFormatProperties fmtProp;
        f->vkGetPhysicalDeviceFormatProperties(physDev, optimalDsFormat, &fmtProp);
        if (fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            break;
        ++dsFormatIdx;
    }
    if (dsFormatIdx == dsFormatCandidateCount)
        qWarning("Failed to find an optimal depth-stencil format");

    return optimalDsFormat;
}

bool QVkRenderPrivate::createDefaultRenderPass(QVkRenderPass *rp, bool hasDepthStencil, VkSampleCountFlagBits sampleCount, VkFormat colorFormat)
{
    VkAttachmentDescription attDesc[3];
    memset(attDesc, 0, sizeof(attDesc));

    uint32_t colorAttIndex = 0;
    attDesc[0].format = colorFormat;
    attDesc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // clear on load + no store + lazy alloc + transient image should play
    // nicely with tiled GPUs (no physical backing necessary for ds buffer)
    attDesc[1].format = optimalDepthStencilFormat();
    attDesc[1].samples = sampleCount;
    attDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    if (sampleCount > VK_SAMPLE_COUNT_1_BIT) {
        colorAttIndex = 2;
        attDesc[2].format = colorFormat;
        attDesc[2].samples = sampleCount;
        attDesc[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attDesc[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attDesc[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attDesc[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attDesc[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attDesc[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkAttachmentReference colorRef = { colorAttIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference resolveRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference dsRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subPassDesc;
    memset(&subPassDesc, 0, sizeof(subPassDesc));
    subPassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPassDesc.colorAttachmentCount = 1;
    subPassDesc.pColorAttachments = &colorRef;
    subPassDesc.pDepthStencilAttachment = hasDepthStencil ? &dsRef : nullptr;

    VkRenderPassCreateInfo rpInfo;
    memset(&rpInfo, 0, sizeof(rpInfo));
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = attDesc;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subPassDesc;

    if (hasDepthStencil)
        rpInfo.attachmentCount += 1;

    if (sampleCount > VK_SAMPLE_COUNT_1_BIT) {
        rpInfo.attachmentCount += 1;
        subPassDesc.pResolveAttachments = &resolveRef;
    }

    VkResult err = df->vkCreateRenderPass(dev, &rpInfo, nullptr, &rp->rp);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create renderpass: %d", err);
        return false;
    }

    return true;
}

bool QVkRender::importSurface(VkSurfaceKHR surface, const QSize &pixelSize,
                              SurfaceImportFlags flags, QVkRenderBuffer *depthStencil,
                              int sampleCount, QVkSwapChain *outSwapChain)
{
    // Can be called multiple times without a call to releaseSwapChain - this
    // is typical when a window is resized.

    if (!d->vkGetPhysicalDeviceSurfaceCapabilitiesKHR) {
        d->vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            d->inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
        d->vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            d->inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
        if (!d->vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !d->vkGetPhysicalDeviceSurfaceFormatsKHR) {
            qWarning("Physical device surface queries not available");
            return false;
        }
    }

    quint32 formatCount = 0;
    d->vkGetPhysicalDeviceSurfaceFormatsKHR(d->physDev, surface, &formatCount, nullptr);
    QVector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount)
        d->vkGetPhysicalDeviceSurfaceFormatsKHR(d->physDev, surface, &formatCount, formats.data());

    // Pick the preferred format, if there is one.
    if (!formats.isEmpty() && formats[0].format != VK_FORMAT_UNDEFINED) {
        outSwapChain->colorFormat = formats[0].format;
        outSwapChain->colorSpace = formats[0].colorSpace;
    }

    outSwapChain->depthStencil = flags.testFlag(UseDepthStencil) ? depthStencil : nullptr;
    if (outSwapChain->depthStencil && outSwapChain->depthStencil->sampleCount != sampleCount) {
        qWarning("Depth-stencil buffer's sampleCount (%d) does not match color buffers' sample count (%d). Expect problems.",
                 outSwapChain->depthStencil->sampleCount, sampleCount);
    }
    outSwapChain->sampleCount = d->effectiveSampleCount(sampleCount);

    if (!d->recreateSwapChain(surface, pixelSize, flags, outSwapChain))
        return false;

    d->createDefaultRenderPass(&outSwapChain->rp, outSwapChain->depthStencil != nullptr,
                               outSwapChain->sampleCount, outSwapChain->colorFormat);

    for (int i = 0; i < outSwapChain->bufferCount; ++i) {
        QVkSwapChain::ImageResources &image(outSwapChain->imageRes[i]);

        VkImageView views[3] = {
            image.imageView,
            outSwapChain->depthStencil ? outSwapChain->depthStencil->imageView : VK_NULL_HANDLE,
            outSwapChain->sampleCount > VK_SAMPLE_COUNT_1_BIT ? image.msaaImageView : VK_NULL_HANDLE
        };
        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = outSwapChain->rp.rp;
        fbInfo.attachmentCount = 1;
        if (outSwapChain->depthStencil)
            fbInfo.attachmentCount += 1;
        if (sampleCount > VK_SAMPLE_COUNT_1_BIT)
            fbInfo.attachmentCount += 1;
        fbInfo.pAttachments = views;
        fbInfo.width = outSwapChain->pixelSize.width();
        fbInfo.height = outSwapChain->pixelSize.height();
        fbInfo.layers = 1;
        VkResult err = d->df->vkCreateFramebuffer(d->dev, &fbInfo, nullptr, &image.fb);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create framebuffer: %d", err);
            return false;
        }
    }

    return true;
}

bool QVkRenderPrivate::recreateSwapChain(VkSurfaceKHR surface, const QSize &pixelSize,
                                         QVkRender::SurfaceImportFlags flags, QVkSwapChain *swapChain)
{
    swapChain->pixelSize = pixelSize;
    if (swapChain->pixelSize.isEmpty())
        return false;

    df->vkDeviceWaitIdle(dev);

    if (!vkCreateSwapchainKHR) {
        vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(f->vkGetDeviceProcAddr(dev, "vkCreateSwapchainKHR"));
        vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(f->vkGetDeviceProcAddr(dev, "vkDestroySwapchainKHR"));
        vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(f->vkGetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR"));
        vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(f->vkGetDeviceProcAddr(dev, "vkAcquireNextImageKHR"));
        vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(f->vkGetDeviceProcAddr(dev, "vkQueuePresentKHR"));
        if (!vkCreateSwapchainKHR || !vkDestroySwapchainKHR || !vkGetSwapchainImagesKHR || !vkAcquireNextImageKHR || !vkQueuePresentKHR) {
            qWarning("Swapchain functions not available");
            return false;
        }
    }

    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, surface, &surfaceCaps);
    quint32 reqBufferCount = QVkSwapChain::DEFAULT_BUFFER_COUNT;
    if (surfaceCaps.maxImageCount)
        reqBufferCount = qBound(surfaceCaps.minImageCount, reqBufferCount, surfaceCaps.maxImageCount);

    VkExtent2D bufferSize = surfaceCaps.currentExtent;
    if (bufferSize.width == quint32(-1)) {
        Q_ASSERT(bufferSize.height == quint32(-1));
        bufferSize.width = swapChain->pixelSize.width();
        bufferSize.height = swapChain->pixelSize.height();
    } else {
        swapChain->pixelSize = QSize(bufferSize.width, bufferSize.height);
    }

    VkSurfaceTransformFlagBitsKHR preTransform =
        (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
        : surfaceCaps.currentTransform;

    VkCompositeAlphaFlagBitsKHR compositeAlpha =
        (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
        : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    if (flags.testFlag(QVkRender::SurfaceHasPreMulAlpha)
            && (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR))
    {
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }

    if (flags.testFlag(QVkRender::SurfaceHasNonPreMulAlpha)
            && (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR))
    {
        compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChain->supportsReadback = (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (swapChain->supportsReadback)
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    qDebug("Creating new swap chain of %d buffers, size %dx%d", reqBufferCount, bufferSize.width, bufferSize.height);

    VkSwapchainKHR oldSwapChain = swapChain->sc;
    VkSwapchainCreateInfoKHR swapChainInfo;
    memset(&swapChainInfo, 0, sizeof(swapChainInfo));
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = surface;
    swapChainInfo.minImageCount = reqBufferCount;
    swapChainInfo.imageFormat = swapChain->colorFormat;
    swapChainInfo.imageColorSpace = swapChain->colorSpace;
    swapChainInfo.imageExtent = bufferSize;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = usage;
    swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainInfo.preTransform = preTransform;
    swapChainInfo.compositeAlpha = compositeAlpha;
    swapChainInfo.presentMode = presentMode;
    swapChainInfo.clipped = true;
    swapChainInfo.oldSwapchain = oldSwapChain;

    VkSwapchainKHR newSwapChain;
    VkResult err = vkCreateSwapchainKHR(dev, &swapChainInfo, nullptr, &newSwapChain);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create swapchain: %d", err);
        return false;
    }

    if (oldSwapChain)
        releaseSwapChain(swapChain);

    swapChain->sc = newSwapChain;

    quint32 actualSwapChainBufferCount = 0;
    err = vkGetSwapchainImagesKHR(dev, swapChain->sc, &actualSwapChainBufferCount, nullptr);
    if (err != VK_SUCCESS || actualSwapChainBufferCount < 2) {
        qWarning("Failed to get swapchain images: %d (count=%d)", err, actualSwapChainBufferCount);
        return false;
    }

    if (actualSwapChainBufferCount > QVkSwapChain::MAX_BUFFER_COUNT) {
        qWarning("Too many swapchain buffers (%d)", actualSwapChainBufferCount);
        return false;
    }
    swapChain->bufferCount = actualSwapChainBufferCount;

    VkImage swapChainImages[QVkSwapChain::MAX_BUFFER_COUNT];
    err = vkGetSwapchainImagesKHR(dev, swapChain->sc, &actualSwapChainBufferCount, swapChainImages);
    if (err != VK_SUCCESS) {
        qWarning("Failed to get swapchain images: %d", err);
        return false;
    }

    VkImage msaaImages[QVkSwapChain::MAX_BUFFER_COUNT];
    VkImageView msaaViews[QVkSwapChain::MAX_BUFFER_COUNT];
    if (swapChain->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
        if (!createTransientImage(swapChain->colorFormat,
                                  swapChain->pixelSize,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  swapChain->sampleCount,
                                  &swapChain->msaaImageMem,
                                  msaaImages,
                                  msaaViews,
                                  swapChain->bufferCount))
        {
            return false;
        }
    }

    VkFenceCreateInfo fenceInfo;
    memset(&fenceInfo, 0, sizeof(fenceInfo));
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < swapChain->bufferCount; ++i) {
        QVkSwapChain::ImageResources &image(swapChain->imageRes[i]);
        image.image = swapChainImages[i];
        if (swapChain->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
            image.msaaImage = msaaImages[i];
            image.msaaImageView = msaaViews[i];
        }

        VkImageViewCreateInfo imgViewInfo;
        memset(&imgViewInfo, 0, sizeof(imgViewInfo));
        imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imgViewInfo.image = swapChainImages[i];
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = swapChain->colorFormat;
        imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;
        err = df->vkCreateImageView(dev, &imgViewInfo, nullptr, &image.imageView);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create swapchain image view %d: %d", i, err);
            return false;
        }

        err = df->vkCreateFence(dev, &fenceInfo, nullptr, &image.cmdFence);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create command buffer fence: %d", err);
            return false;
        }
        image.cmdFenceWaitable = true; // fence was created in signaled state
    }

    swapChain->currentImage = 0;

    VkSemaphoreCreateInfo semInfo;
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        QVkSwapChain::FrameResources &frame(swapChain->frameRes[i]);

        frame.imageAcquired = false;
        frame.imageSemWaitable = false;

        df->vkCreateFence(dev, &fenceInfo, nullptr, &frame.fence);
        frame.fenceWaitable = true; // fence was created in signaled state

        df->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.imageSem);
        df->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.drawSem);
    }

    swapChain->currentFrame = 0;

    return true;
}

void QVkRender::releaseSwapChain(QVkSwapChain *swapChain)
{
    d->releaseSwapChain(swapChain);
}

void QVkRenderPrivate::releaseSwapChain(QVkSwapChain *swapChain)
{
    if (swapChain->sc == VK_NULL_HANDLE)
        return;

    df->vkDeviceWaitIdle(dev);

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        QVkSwapChain::FrameResources &frame(swapChain->frameRes[i]);
        if (frame.fence) {
            if (frame.fenceWaitable)
                df->vkWaitForFences(dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
            df->vkDestroyFence(dev, frame.fence, nullptr);
            frame.fence = VK_NULL_HANDLE;
            frame.fenceWaitable = false;
        }
        if (frame.imageSem) {
            df->vkDestroySemaphore(dev, frame.imageSem, nullptr);
            frame.imageSem = VK_NULL_HANDLE;
        }
        if (frame.drawSem) {
            df->vkDestroySemaphore(dev, frame.drawSem, nullptr);
            frame.drawSem = VK_NULL_HANDLE;
        }
    }

    for (int i = 0; i < swapChain->bufferCount; ++i) {
        QVkSwapChain::ImageResources &image(swapChain->imageRes[i]);
        if (image.cmdFence) {
            if (image.cmdFenceWaitable)
                df->vkWaitForFences(dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
            df->vkDestroyFence(dev, image.cmdFence, nullptr);
            image.cmdFence = VK_NULL_HANDLE;
            image.cmdFenceWaitable = false;
        }
        if (image.fb) {
            df->vkDestroyFramebuffer(dev, image.fb, nullptr);
            image.fb = VK_NULL_HANDLE;
        }
        if (image.imageView) {
            df->vkDestroyImageView(dev, image.imageView, nullptr);
            image.imageView = VK_NULL_HANDLE;
        }
        if (image.cmdBuf.cb) {
            df->vkFreeCommandBuffers(dev, cmdPool, 1, &image.cmdBuf.cb);
            image.cmdBuf.cb = VK_NULL_HANDLE;
        }
        if (image.msaaImageView) {
            df->vkDestroyImageView(dev, image.msaaImageView, nullptr);
            image.msaaImageView = VK_NULL_HANDLE;
        }
        if (image.msaaImage) {
            df->vkDestroyImage(dev, image.msaaImage, nullptr);
            image.msaaImage = VK_NULL_HANDLE;
        }
    }

    if (swapChain->msaaImageMem) {
        df->vkFreeMemory(dev, swapChain->msaaImageMem, nullptr);
        swapChain->msaaImageMem = VK_NULL_HANDLE;
    }

    if (swapChain->rp.rp) {
        df->vkDestroyRenderPass(dev, swapChain->rp.rp, nullptr);
        swapChain->rp.rp = VK_NULL_HANDLE;
    }

    vkDestroySwapchainKHR(dev, swapChain->sc, nullptr);
    swapChain->sc = VK_NULL_HANDLE;
}

static inline bool checkDeviceLost(VkResult err)
{
    if (err == VK_ERROR_DEVICE_LOST) {
        qWarning("Device lost");
        return true;
    }
    return false;
}

QVkRender::FrameOpResult QVkRender::beginFrame(QVkSwapChain *sc)
{
    QVkSwapChain::FrameResources &frame(sc->frameRes[sc->currentFrame]);

    if (!frame.imageAcquired) {
        // Wait if we are too far ahead, i.e. the thread gets throttled based on the presentation rate
        // (note that we are using FIFO mode -> vsync)
        if (frame.fenceWaitable) {
            d->df->vkWaitForFences(d->dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
            d->df->vkResetFences(d->dev, 1, &frame.fence);
            frame.fenceWaitable = false;
        }

        // move on to next swapchain image
        VkResult err = d->vkAcquireNextImageKHR(d->dev, sc->sc, UINT64_MAX,
                                                frame.imageSem, frame.fence, &sc->currentImage);
        if (err == VK_SUCCESS || err == VK_SUBOPTIMAL_KHR) {
            frame.imageSemWaitable = true;
            frame.imageAcquired = true;
            frame.fenceWaitable = true;
        } else if (err == VK_ERROR_OUT_OF_DATE_KHR) {
            return FrameOpSwapChainOutOfDate;
        } else {
            if (checkDeviceLost(err))
                return FrameOpDeviceLost;
            else
                qWarning("Failed to acquire next swapchain image: %d", err);
            return FrameOpError;
        }
    }

    // make sure the previous draw for the same image has finished
    QVkSwapChain::ImageResources &image(sc->imageRes[sc->currentImage]);
    if (image.cmdFenceWaitable) {
        d->df->vkWaitForFences(d->dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
        d->df->vkResetFences(d->dev, 1, &image.cmdFence);
        image.cmdFenceWaitable = false;
    }

    // build new draw command buffer
    if (image.cmdBuf.cb) {
        d->df->vkFreeCommandBuffers(d->dev, d->cmdPool, 1, &image.cmdBuf.cb);
        image.cmdBuf.cb = VK_NULL_HANDLE;
    }

    VkCommandBufferAllocateInfo cmdBufInfo;
    memset(&cmdBufInfo, 0, sizeof(cmdBufInfo));
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufInfo.commandPool = d->cmdPool;
    cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufInfo.commandBufferCount = 1;

    VkResult err = d->df->vkAllocateCommandBuffers(d->dev, &cmdBufInfo, &image.cmdBuf.cb);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return FrameOpDeviceLost;
        else
            qWarning("Failed to allocate frame command buffer: %d", err);
        return FrameOpError;
    }

    VkCommandBufferBeginInfo cmdBufBeginInfo;
    memset(&cmdBufBeginInfo, 0, sizeof(cmdBufBeginInfo));
    cmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    err = d->df->vkBeginCommandBuffer(image.cmdBuf.cb, &cmdBufBeginInfo);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return FrameOpDeviceLost;
        else
            qWarning("Failed to begin frame command buffer: %d", err);
        return FrameOpError;
    }

    d->currentFrameSlot = sc->currentFrame;
    if (sc->depthStencil)
        sc->depthStencil->lastActiveFrameSlot = d->currentFrameSlot;

    d->prepareNewFrame(&image.cmdBuf);

    return FrameOpSuccess;
}

QVkRender::FrameOpResult QVkRender::endFrame(QVkSwapChain *sc)
{
    d->finishFrame();

    QVkSwapChain::FrameResources &frame(sc->frameRes[sc->currentFrame]);
    QVkSwapChain::ImageResources &image(sc->imageRes[sc->currentImage]);

    VkResult err = d->df->vkEndCommandBuffer(image.cmdBuf.cb);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return FrameOpDeviceLost;
        else
            qWarning("Failed to end frame command buffer: %d", err);
        return FrameOpError;
    }

    // submit draw calls
    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &image.cmdBuf.cb;
    if (frame.imageSemWaitable) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frame.imageSem;
    }
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.drawSem;

    VkPipelineStageFlags psf = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.pWaitDstStageMask = &psf;

    Q_ASSERT(!image.cmdFenceWaitable);

    err = d->df->vkQueueSubmit(d->gfxQueue, 1, &submitInfo, image.cmdFence);
    if (err == VK_SUCCESS) {
        frame.imageSemWaitable = false;
        image.cmdFenceWaitable = true;
    } else {
        if (checkDeviceLost(err))
            return FrameOpDeviceLost;
        else
            qWarning("Failed to submit to graphics queue: %d", err);
        return FrameOpError;
    }

    VkPresentInfoKHR presInfo;
    memset(&presInfo, 0, sizeof(presInfo));
    presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presInfo.swapchainCount = 1;
    presInfo.pSwapchains = &sc->sc;
    presInfo.pImageIndices = &sc->currentImage;
    presInfo.waitSemaphoreCount = 1;
    presInfo.pWaitSemaphores = &frame.drawSem; // gfxQueueFamilyIdx == presQueueFamilyIdx ? &frame.drawSem : &frame.presTransSem;

    err = d->vkQueuePresentKHR(d->gfxQueue, &presInfo);
    if (err != VK_SUCCESS) {
        if (err == VK_ERROR_OUT_OF_DATE_KHR) {
            return FrameOpSwapChainOutOfDate;
        } else if (err != VK_SUBOPTIMAL_KHR) {
            if (checkDeviceLost(err))
                return FrameOpDeviceLost;
            else
                qWarning("Failed to present: %d", err);
            return FrameOpError;
        }
    }

    frame.imageAcquired = false;

    sc->currentFrame = (sc->currentFrame + 1) % QVK_FRAMES_IN_FLIGHT;

    return FrameOpSuccess;
}

void QVkRender::beginPass(QVkSwapChain *sc, const QVkClearValue *clearValues)
{
    QVkRenderTarget rt;
    rt.fb = sc->imageRes[sc->currentImage].fb;
    rt.rp.rp = sc->rp.rp;
    rt.pixelSize = sc->pixelSize;
    rt.attCount = 1;
    if (sc->depthStencil)
        rt.attCount += 1;
    if (sc->sampleCount > VK_SAMPLE_COUNT_1_BIT)
        rt.attCount += 1;

    beginPass(&rt, &sc->imageRes[sc->currentImage].cmdBuf, clearValues);
}

void QVkRender::endPass(QVkSwapChain *sc)
{
    endPass(&sc->imageRes[sc->currentImage].cmdBuf);
}

void QVkRender::importVulkanWindowRenderPass(QVulkanWindow *window, QVkRenderPass *outRp)
{
    outRp->rp = window->defaultRenderPass();
}

void QVkRender::beginFrame(QVulkanWindow *window, QVkRenderTarget *outRt, QVkCommandBuffer *outCb)
{
    importVulkanWindowRenderPass(window, &outRt->rp);
    outRt->fb = window->currentFramebuffer();
    outRt->pixelSize = window->swapChainImageSize();
    outRt->attCount = window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;

    outCb->cb = window->currentCommandBuffer();

    d->currentFrameSlot = window->currentFrame();
    d->prepareNewFrame(outCb);
}

void QVkRender::endFrame(QVulkanWindow *window)
{
    Q_UNUSED(window);

    d->finishFrame();
}

void QVkRender::beginPass(QVkRenderTarget *rt, QVkCommandBuffer *cb, const QVkClearValue *clearValues)
{
    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = rt->rp.rp;
    rpBeginInfo.framebuffer = rt->fb;
    rpBeginInfo.renderArea.extent.width = rt->pixelSize.width();
    rpBeginInfo.renderArea.extent.height = rt->pixelSize.height();
    rpBeginInfo.clearValueCount = rt->attCount;
    QVarLengthArray<VkClearValue, 4> cvs;
    for (int i = 0; i < rt->attCount; ++i) {
        VkClearValue cv;
        if (clearValues[i].isDepthStencil)
            cv.depthStencil = { clearValues[i].d, clearValues[i].s };
        else
            cv.color = { { clearValues[i].rgba.x(), clearValues[i].rgba.y(), clearValues[i].rgba.z(), clearValues[i].rgba.w() } };
        cvs.append(cv);
    }
    rpBeginInfo.pClearValues = cvs.constData();

    d->df->vkCmdBeginRenderPass(cb->cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void QVkRender::endPass(QVkCommandBuffer *cb)
{
    d->df->vkCmdEndRenderPass(cb->cb);
}

QMatrix4x4 QVkRender::openGLCorrectionMatrix() const
{
    if (d->clipCorrectMatrix.isIdentity()) {
        // NB the ctor takes row-major
        d->clipCorrectMatrix = QMatrix4x4(1.0f, 0.0f, 0.0f, 0.0f,
                                          0.0f, -1.0f, 0.0f, 0.0f,
                                          0.0f, 0.0f, 0.5f, 0.5f,
                                          0.0f, 0.0f, 0.0f, 1.0f);
    }
    return d->clipCorrectMatrix;
}

static inline VkBufferUsageFlagBits toVkBufferUsage(QVkBuffer::UsageFlags usage)
{
    int u = 0;
    if (usage.testFlag(QVkBuffer::VertexBuffer))
        u |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage.testFlag(QVkBuffer::IndexBuffer))
        u |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage.testFlag(QVkBuffer::UniformBuffer))
        u |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    return VkBufferUsageFlagBits(u);
}

bool QVkRender::createBuffer(QVkBuffer *buf)
{
    if (buf->d[0].buffer) // no repeated create without a releaseLater first
        return false;

    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = buf->size;
    bufferInfo.usage = toVkBufferUsage(buf->usage);

    VmaAllocationCreateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));

    if (buf->isStatic()) {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    } else {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    }

    VkResult err = VK_SUCCESS;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        buf->d[i].allocation = VK_NULL_HANDLE;
        buf->d[i].buffer = VK_NULL_HANDLE;
        if (i == 0 || !buf->isStatic()) {
            VmaAllocation allocation;
            err = vmaCreateBuffer(d->allocator, &bufferInfo, &allocInfo, &buf->d[i].buffer, &allocation, nullptr);
            if (err != VK_SUCCESS)
                break;
            buf->d[i].allocation = allocation;
        }
    }

    if (err == VK_SUCCESS && buf->isStatic()) {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocation allocation;
        err = vmaCreateBuffer(d->allocator, &bufferInfo, &allocInfo, &buf->stagingBuffer, &allocation, nullptr);
        if (err == VK_SUCCESS)
            buf->stagingAlloc = allocation;
    }

    if (err == VK_SUCCESS) {
        buf->lastActiveFrameSlot = -1;
        return true;
    } else {
        qWarning("Failed to create buffer: %d", err);
        return false;
    }
}

void QVkRender::uploadStaticBuffer(QVkCommandBuffer *cb, QVkBuffer *buf, const void *data)
{
    Q_ASSERT(buf->isStatic());
    Q_ASSERT(buf->stagingBuffer);

    void *p = nullptr;
    VmaAllocation a = toVmaAllocation(buf->stagingAlloc);
    VkResult err = vmaMapMemory(d->allocator, a, &p);
    if (err != VK_SUCCESS) {
        qWarning("Failed to map buffer: %d", err);
        return;
    }
    memcpy(p, data, buf->size);
    vmaUnmapMemory(d->allocator, a);
    vmaFlushAllocation(d->allocator, a, 0, buf->size);

    VkBufferCopy copyInfo;
    memset(&copyInfo, 0, sizeof(copyInfo));
    copyInfo.size = buf->size;

    VkBuffer dstBuf = buf->d[0].buffer;
    d->df->vkCmdCopyBuffer(cb->cb, buf->stagingBuffer, dstBuf, 1, &copyInfo);

    int dstAccess = 0;
    VkPipelineStageFlagBits dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

    if (buf->usage.testFlag(QVkBuffer::VertexBuffer))
        dstAccess |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    if (buf->usage.testFlag(QVkBuffer::IndexBuffer))
        dstAccess |= VK_ACCESS_INDEX_READ_BIT;
    if (buf->usage.testFlag(QVkBuffer::UniformBuffer)) {
        dstAccess |= VK_ACCESS_UNIFORM_READ_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    VkBufferMemoryBarrier bufMemBarrier;
    memset(&bufMemBarrier, 0, sizeof(bufMemBarrier));
    bufMemBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufMemBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    bufMemBarrier.dstAccessMask = dstAccess;
    bufMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.buffer = dstBuf;
    bufMemBarrier.size = buf->size;

    d->df->vkCmdPipelineBarrier(cb->cb, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage,
                                0, 0, nullptr, 1, &bufMemBarrier, 0, nullptr);
}

void QVkRender::updateDynamicBuffer(QVkBuffer *buf, int offset, int size, const void *data)
{
    Q_ASSERT(!buf->isStatic());
    Q_ASSERT(offset + size <= buf->size);

    if (size < 1)
        return;

    const QByteArray u(static_cast<const char *>(data), size);
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        buf->d[i].pendingUpdates.append(QVkBuffer::PendingUpdate(offset, u));
}

int QVkRender::ubufAlignment() const
{
    return d->ubufAlign; // typically 256 (bytes)
}

int QVkRender::ubufAligned(int v) const
{
    return aligned(v, d->ubufAlign);
}

bool QVkRender::createRenderBuffer(QVkRenderBuffer *rb)
{
    if (rb->memory) // no repeated create without a releaseLater first
        return false;

    switch (rb->type) {
    case QVkRenderBuffer::DepthStencil:
        if (!d->createTransientImage(d->optimalDepthStencilFormat(),
                                     rb->pixelSize,
                                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                     VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                     d->effectiveSampleCount(rb->sampleCount),
                                     &rb->memory,
                                     &rb->image,
                                     &rb->imageView,
                                     1))
        {
            return false;
        }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    rb->lastActiveFrameSlot = -1;
    return true;
}

static inline VkFormat toVkTextureFormat(QVkTexture::Format format)
{
    switch (format) {
    case QVkTexture::RGBA8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case QVkTexture::BGRA8:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case QVkTexture::R8:
        return VK_FORMAT_R8_UNORM;
    case QVkTexture::R16:
        return VK_FORMAT_R16_UNORM;

    case QVkTexture::D16:
        return VK_FORMAT_D16_UNORM;
    case QVkTexture::D32:
        return VK_FORMAT_D32_SFLOAT;

    default:
        Q_UNREACHABLE();
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

static inline bool isDepthStencilTextureFormat(QVkTexture::Format format)
{
    switch (format) {
    case QVkTexture::Format::D16:
        Q_FALLTHROUGH();
    case QVkTexture::Format::D32:
        return true;

    default:
        return false;
    }
}

bool QVkRender::createTexture(QVkTexture *tex)
{
    if (tex->image) // no repeated create without a releaseLater first
        return false;

    VkFormat vkformat = toVkTextureFormat(tex->format);
    VkFormatProperties props;
    d->f->vkGetPhysicalDeviceFormatProperties(d->physDev, vkformat, &props);
    const bool canSampleOptimal = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    if (!canSampleOptimal) {
        qWarning("Texture sampling not supported?!");
        return false;
    }

    VkSampleCountFlagBits samples = d->effectiveSampleCount(tex->sampleCount);
    QSize size = tex->pixelSize;
    if (size.isEmpty())
        size = QSize(16, 16);

    VkImageCreateInfo imageInfo;
    memset(&imageInfo, 0, sizeof(imageInfo));
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkformat;
    imageInfo.extent.width = size.width();
    imageInfo.extent.height = size.height();
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = samples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (tex->flags.testFlag(QVkTexture::RenderTarget)) {
        if (isDepthStencilTextureFormat(tex->format))
            imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    VmaAllocationCreateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VmaAllocation allocation;
    VkResult err = vmaCreateImage(d->allocator, &imageInfo, &allocInfo, &tex->image, &allocation, nullptr);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create image: %d", err);
        return false;
    }
    tex->allocation = allocation;

    VkImageViewCreateInfo viewInfo;
    memset(&viewInfo, 0, sizeof(viewInfo));
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkformat;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = viewInfo.subresourceRange.layerCount = 1;

    err = d->df->vkCreateImageView(d->dev, &viewInfo, nullptr, &tex->imageView);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create image view: %d", err);
        return false;
    }

    if (!tex->flags.testFlag(QVkTexture::NoUploadContents)) {
        imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        err = vmaCreateImage(d->allocator, &imageInfo, &allocInfo, &tex->stagingImage, &allocation, nullptr);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create staging image buffer: %d", err);
            return false;
        }
        tex->stagingAlloc = allocation;
        tex->wasStaged = false;
    }

    tex->lastActiveFrameSlot = -1;
    return true;
}

QVkTexture::SubImageInfo QVkRender::textureInfo(QVkTexture *tex, int mipLevel, int layer)
{
    VkImageSubresource subres = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        uint32_t(mipLevel),
        uint32_t(layer)
    };

    VkSubresourceLayout layout;
    d->df->vkGetImageSubresourceLayout(d->dev, tex->stagingImage, &subres, &layout);

    QVkTexture::SubImageInfo t;
    t.size = layout.size;
    t.stride = layout.rowPitch;
    t.offset = layout.offset;
    return t;
}

void QVkRender::uploadTexture(QVkCommandBuffer *cb, QVkTexture *tex, const QImage &image, int mipLevel, int layer)
{
    const QVkTexture::SubImageInfo t = textureInfo(tex, mipLevel, layer);

    void *mp = nullptr;
    VmaAllocation a = toVmaAllocation(tex->stagingAlloc);
    VkResult err = vmaMapMemory(d->allocator, a, &mp);
    if (err != VK_SUCCESS) {
        qWarning("Failed to map image data: %d", err);
        return;
    }
    uchar *p = static_cast<uchar *>(mp);
    Q_ASSERT(image.bytesPerLine() <= t.stride);
    for (int y = 0, h = image.height(), bpl = image.bytesPerLine(); y < h; ++y) {
        const uchar *line = image.constScanLine(y);
        memcpy(p, line, bpl);
        p += t.stride;
    }
    vmaUnmapMemory(d->allocator, a);
    vmaFlushAllocation(d->allocator, a, 0, t.size);

    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;

    barrier.oldLayout = tex->wasStaged ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_PREINITIALIZED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.image = tex->stagingImage;
    d->df->vkCmdPipelineBarrier(cb->cb,
                                VK_PIPELINE_STAGE_HOST_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0, 0, nullptr, 0, nullptr,
                                1, &barrier);

    if (tex->wasStaged) {
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.image = tex->image;
        d->df->vkCmdPipelineBarrier(cb->cb,
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    0, 0, nullptr, 0, nullptr,
                                    1, &barrier);
    } else {
        barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.image = tex->image;
        d->df->vkCmdPipelineBarrier(cb->cb,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    0, 0, nullptr, 0, nullptr,
                                    1, &barrier);
    }

    QSize size = tex->pixelSize;
    if (size.isEmpty())
        size = QSize(16, 16);

    VkImageCopy copyInfo;
    memset(&copyInfo, 0, sizeof(copyInfo));
    copyInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyInfo.srcSubresource.layerCount = 1;
    copyInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyInfo.dstSubresource.layerCount = 1;
    copyInfo.extent.width = size.width();
    copyInfo.extent.height = size.height();
    copyInfo.extent.depth = 1;
    d->df->vkCmdCopyImage(cb->cb,
                          tex->stagingImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          1, &copyInfo);

    // The staging image's layout has to transition back to general if we want
    // to reuse it in a subsequent upload request.
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.image = tex->stagingImage;
    d->df->vkCmdPipelineBarrier(cb->cb,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_HOST_BIT,
                                0, 0, nullptr, 0, nullptr,
                                1, &barrier);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.image = tex->image;
    d->df->vkCmdPipelineBarrier(cb->cb,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0, 0, nullptr, 0, nullptr,
                                1, &barrier);

    tex->wasStaged = true;
}

static inline VkFilter toVkFilter(QVkSampler::Filter f)
{
    switch (f) {
    case QVkSampler::Nearest:
        return VK_FILTER_NEAREST;
    case QVkSampler::Linear:
        return VK_FILTER_LINEAR;
    default:
        Q_UNREACHABLE();
        return VK_FILTER_NEAREST;
    }
}

static inline VkSamplerMipmapMode toVkMipmapMode(QVkSampler::Filter f)
{
    switch (f) {
    case QVkSampler::Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case QVkSampler::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        Q_UNREACHABLE();
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
}

static inline VkSamplerAddressMode toVkAddressMode(QVkSampler::AddressMode m)
{
    switch (m) {
    case QVkSampler::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case QVkSampler::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case QVkSampler::Border:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case QVkSampler::Mirror:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case QVkSampler::MirrorOnce:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:
        Q_UNREACHABLE();
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}

bool QVkRender::createSampler(QVkSampler *sampler)
{
    if (sampler->sampler) // no repeated create without a releaseLater first
        return false;

    VkSamplerCreateInfo samplerInfo;
    memset(&samplerInfo, 0, sizeof(samplerInfo));
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = toVkFilter(sampler->magFilter);
    samplerInfo.minFilter = toVkFilter(sampler->minFilter);
    samplerInfo.mipmapMode = toVkMipmapMode(sampler->mipmapMode);
    samplerInfo.addressModeU = toVkAddressMode(sampler->addressU);
    samplerInfo.addressModeV = toVkAddressMode(sampler->addressV);
    samplerInfo.maxAnisotropy = 1.0f;

    VkResult err = d->df->vkCreateSampler(d->dev, &samplerInfo, nullptr, &sampler->sampler);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create sampler: %d", err);
        return false;
    }

    sampler->lastActiveFrameSlot = -1;
    return true;
}

VkShaderModule QVkRenderPrivate::createShader(const QByteArray &spirv)
{
    VkShaderModuleCreateInfo shaderInfo;
    memset(&shaderInfo, 0, sizeof(shaderInfo));
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = spirv.size();
    shaderInfo.pCode = reinterpret_cast<const quint32 *>(spirv.constData());
    VkShaderModule shaderModule;
    VkResult err = df->vkCreateShaderModule(dev, &shaderInfo, nullptr, &shaderModule);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create shader module: %d", err);
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

bool QVkRenderPrivate::ensurePipelineCache()
{
    if (pipelineCache)
        return true;

    VkPipelineCacheCreateInfo pipelineCacheInfo;
    memset(&pipelineCacheInfo, 0, sizeof(pipelineCacheInfo));
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkResult err = df->vkCreatePipelineCache(dev, &pipelineCacheInfo, nullptr, &pipelineCache);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create pipeline cache: %d", err);
        return false;
    }
    return true;
}

static inline VkShaderStageFlagBits toVkShaderStage(QVkGraphicsShaderStage::Type type)
{
    switch (type) {
    case QVkGraphicsShaderStage::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case QVkGraphicsShaderStage::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case QVkGraphicsShaderStage::Geometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case QVkGraphicsShaderStage::TessellationControl:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case QVkGraphicsShaderStage::TessellationEvaluation:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    default:
        Q_UNREACHABLE();
        return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

static inline VkFormat toVkAttributeFormat(QVkVertexInputLayout::Attribute::Format format)
{
    switch (format) {
    case QVkVertexInputLayout::Attribute::Float4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case QVkVertexInputLayout::Attribute::Float3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case QVkVertexInputLayout::Attribute::Float2:
        return VK_FORMAT_R32G32_SFLOAT;
    case QVkVertexInputLayout::Attribute::Float:
        return VK_FORMAT_R32_SFLOAT;
    case QVkVertexInputLayout::Attribute::UNormByte4:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case QVkVertexInputLayout::Attribute::UNormByte2:
        return VK_FORMAT_R8G8_UNORM;
    case QVkVertexInputLayout::Attribute::UNormByte:
        return VK_FORMAT_R8_UNORM;
    default:
        Q_UNREACHABLE();
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

static inline VkPrimitiveTopology toVkTopology(QVkGraphicsPipeline::Topology t)
{
    switch (t) {
    case QVkGraphicsPipeline::Triangles:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case QVkGraphicsPipeline::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case QVkGraphicsPipeline::TriangleFan:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case QVkGraphicsPipeline::Lines:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case QVkGraphicsPipeline::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case QVkGraphicsPipeline::Points:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    default:
        Q_UNREACHABLE();
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

static inline VkCullModeFlags toVkCullMode(QVkGraphicsPipeline::CullMode c)
{
    int m = 0;
    if (c.testFlag(QVkGraphicsPipeline::Front))
        m |= VK_CULL_MODE_FRONT_BIT;
    if (c.testFlag(QVkGraphicsPipeline::Back))
        m |= VK_CULL_MODE_BACK_BIT;
    return VkCullModeFlags(m);
}

static inline VkFrontFace toVkFrontFace(QVkGraphicsPipeline::FrontFace f)
{
    switch (f) {
    case QVkGraphicsPipeline::CCW:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case QVkGraphicsPipeline::CW:
        return VK_FRONT_FACE_CLOCKWISE;
    default:
        Q_UNREACHABLE();
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

static inline VkColorComponentFlags toVkColorComponents(QVkGraphicsPipeline::ColorMask c)
{
    int f = 0;
    if (c.testFlag(QVkGraphicsPipeline::R))
        f |= VK_COLOR_COMPONENT_R_BIT;
    if (c.testFlag(QVkGraphicsPipeline::G))
        f |= VK_COLOR_COMPONENT_G_BIT;
    if (c.testFlag(QVkGraphicsPipeline::B))
        f |= VK_COLOR_COMPONENT_B_BIT;
    if (c.testFlag(QVkGraphicsPipeline::A))
        f |= VK_COLOR_COMPONENT_A_BIT;
    return VkColorComponentFlags(f);
}

static inline VkBlendFactor toVkBlendFactor(QVkGraphicsPipeline::BlendFactor f)
{
    switch (f) {
    case QVkGraphicsPipeline::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case QVkGraphicsPipeline::One:
        return VK_BLEND_FACTOR_ONE;
    case QVkGraphicsPipeline::SrcColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case QVkGraphicsPipeline::OneMinusSrcColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case QVkGraphicsPipeline::DstColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case QVkGraphicsPipeline::OneMinusDstColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case QVkGraphicsPipeline::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case QVkGraphicsPipeline::OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case QVkGraphicsPipeline::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case QVkGraphicsPipeline::OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case QVkGraphicsPipeline::ConstantColor:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case QVkGraphicsPipeline::OneMinusConstantColor:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case QVkGraphicsPipeline::ConstantAlpha:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case QVkGraphicsPipeline::OneMinusConstantAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case QVkGraphicsPipeline::SrcAlphaSaturate:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case QVkGraphicsPipeline::Src1Color:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case QVkGraphicsPipeline::OneMinusSrc1Color:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case QVkGraphicsPipeline::Src1Alpha:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case QVkGraphicsPipeline::OneMinusSrc1Alpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    default:
        Q_UNREACHABLE();
        return VK_BLEND_FACTOR_ZERO;
    }
}

static inline VkBlendOp toVkBlendOp(QVkGraphicsPipeline::BlendOp op)
{
    switch (op) {
    case QVkGraphicsPipeline::Add:
        return VK_BLEND_OP_ADD;
    case QVkGraphicsPipeline::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case QVkGraphicsPipeline::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case QVkGraphicsPipeline::Min:
        return VK_BLEND_OP_MIN;
    case QVkGraphicsPipeline::Max:
        return VK_BLEND_OP_MAX;
    default:
        Q_UNREACHABLE();
        return VK_BLEND_OP_ADD;
    }
}

static inline VkCompareOp toVkCompareOp(QVkGraphicsPipeline::CompareOp op)
{
    switch (op) {
    case QVkGraphicsPipeline::Never:
        return VK_COMPARE_OP_NEVER;
    case QVkGraphicsPipeline::Less:
        return VK_COMPARE_OP_LESS;
    case QVkGraphicsPipeline::Equal:
        return VK_COMPARE_OP_EQUAL;
    case QVkGraphicsPipeline::LessOrEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case QVkGraphicsPipeline::Greater:
        return VK_COMPARE_OP_GREATER;
    case QVkGraphicsPipeline::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case QVkGraphicsPipeline::GreaterOrEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case QVkGraphicsPipeline::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        Q_UNREACHABLE();
        return VK_COMPARE_OP_ALWAYS;
    }
}

static inline VkStencilOp toVkStencilOp(QVkGraphicsPipeline::StencilOp op)
{
    switch (op) {
    case QVkGraphicsPipeline::StencilZero:
        return VK_STENCIL_OP_ZERO;
    case QVkGraphicsPipeline::Keep:
        return VK_STENCIL_OP_KEEP;
    case QVkGraphicsPipeline::Replace:
        return VK_STENCIL_OP_REPLACE;
    case QVkGraphicsPipeline::IncrementAndClamp:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case QVkGraphicsPipeline::DecrementAndClamp:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case QVkGraphicsPipeline::Invert:
        return VK_STENCIL_OP_INVERT;
    case QVkGraphicsPipeline::IncrementAndWrap:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case QVkGraphicsPipeline::DecrementAndWrap:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    default:
        Q_UNREACHABLE();
        return VK_STENCIL_OP_KEEP;
    }
}

static inline void fillVkStencilOpState(VkStencilOpState *dst, const QVkGraphicsPipeline::StencilOpState &src)
{
    dst->failOp = toVkStencilOp(src.failOp);
    dst->passOp = toVkStencilOp(src.passOp);
    dst->depthFailOp = toVkStencilOp(src.depthFailOp);
    dst->compareOp = toVkCompareOp(src.compareOp);
}

bool QVkRender::createGraphicsPipeline(QVkGraphicsPipeline *ps)
{
    if (ps->pipeline) // no repeated create without a releaseLater first
        return false;

    if (!d->ensurePipelineCache())
        return false;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    Q_ASSERT(ps->shaderResourceBindings && ps->shaderResourceBindings->layout);
    pipelineLayoutInfo.pSetLayouts = &ps->shaderResourceBindings->layout;
    VkResult err = d->df->vkCreatePipelineLayout(d->dev, &pipelineLayoutInfo, nullptr, &ps->layout);
    if (err != VK_SUCCESS)
        qWarning("Failed to create pipeline layout: %d", err);

    VkGraphicsPipelineCreateInfo pipelineInfo;
    memset(&pipelineInfo, 0, sizeof(pipelineInfo));
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    QVarLengthArray<VkShaderModule, 4> shaders;
    QVarLengthArray<VkPipelineShaderStageCreateInfo, 4> shaderStageCreateInfos;
    for (const QVkGraphicsShaderStage &shaderStage : ps->shaderStages) {
        VkShaderModule shader = d->createShader(shaderStage.spirv);
        if (shader) {
            shaders.append(shader);
            VkPipelineShaderStageCreateInfo shaderInfo;
            memset(&shaderInfo, 0, sizeof(shaderInfo));
            shaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderInfo.stage = toVkShaderStage(shaderStage.type);
            shaderInfo.module = shader;
            shaderInfo.pName = shaderStage.name;
            shaderStageCreateInfos.append(shaderInfo);
        }
    }
    pipelineInfo.stageCount = shaderStageCreateInfos.count();
    pipelineInfo.pStages = shaderStageCreateInfos.constData();

    QVarLengthArray<VkVertexInputBindingDescription, 4> vertexBindings;
    for (int i = 0, ie = ps->vertexInputLayout.bindings.count(); i != ie; ++i) {
        const QVkVertexInputLayout::Binding &binding(ps->vertexInputLayout.bindings[i]);
        VkVertexInputBindingDescription bindingInfo = {
            uint32_t(i),
            binding.stride,
            binding.classification == QVkVertexInputLayout::Binding::PerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE
        };
        vertexBindings.append(bindingInfo);
    }
    QVarLengthArray<VkVertexInputAttributeDescription, 4> vertexAttributes;
    for (const QVkVertexInputLayout::Attribute &attribute : ps->vertexInputLayout.attributes) {
        VkVertexInputAttributeDescription attributeInfo = {
            uint32_t(attribute.location),
            uint32_t(attribute.binding),
            toVkAttributeFormat(attribute.format),
            attribute.offset
        };
        vertexAttributes.append(attributeInfo);
    }
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    memset(&vertexInputInfo, 0, sizeof(vertexInputInfo));
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = vertexBindings.count();
    vertexInputInfo.pVertexBindingDescriptions = vertexBindings.constData();
    vertexInputInfo.vertexAttributeDescriptionCount = vertexAttributes.count();
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.constData();
    pipelineInfo.pVertexInputState = &vertexInputInfo;

    QVarLengthArray<VkDynamicState, 8> dynEnable;
    dynEnable << VK_DYNAMIC_STATE_VIEWPORT;
    dynEnable << VK_DYNAMIC_STATE_SCISSOR;
    if (ps->flags.testFlag(QVkGraphicsPipeline::UsesBlendConstants))
        dynEnable << VK_DYNAMIC_STATE_BLEND_CONSTANTS;
    if (ps->flags.testFlag(QVkGraphicsPipeline::UsesStencilRef))
        dynEnable << VK_DYNAMIC_STATE_STENCIL_REFERENCE;

    VkPipelineDynamicStateCreateInfo dynamicInfo;
    memset(&dynamicInfo, 0, sizeof(dynamicInfo));
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = dynEnable.count();
    dynamicInfo.pDynamicStates = dynEnable.constData();
    pipelineInfo.pDynamicState = &dynamicInfo;

    VkPipelineViewportStateCreateInfo viewportInfo;
    memset(&viewportInfo, 0, sizeof(viewportInfo));
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;
    pipelineInfo.pViewportState = &viewportInfo;

    VkPipelineInputAssemblyStateCreateInfo inputAsmInfo;
    memset(&inputAsmInfo, 0, sizeof(inputAsmInfo));
    inputAsmInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAsmInfo.topology = toVkTopology(ps->topology);
    pipelineInfo.pInputAssemblyState = &inputAsmInfo;

    VkPipelineRasterizationStateCreateInfo rastInfo;
    memset(&rastInfo, 0, sizeof(rastInfo));
    rastInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastInfo.rasterizerDiscardEnable = ps->rasterizerDiscard;
    rastInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rastInfo.cullMode = toVkCullMode(ps->cullMode);
    rastInfo.frontFace = toVkFrontFace(ps->frontFace);
    rastInfo.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rastInfo;

    VkPipelineMultisampleStateCreateInfo msInfo;
    memset(&msInfo, 0, sizeof(msInfo));
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = d->effectiveSampleCount(ps->sampleCount);
    pipelineInfo.pMultisampleState = &msInfo;

    VkPipelineDepthStencilStateCreateInfo dsInfo;
    memset(&dsInfo, 0, sizeof(dsInfo));
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.depthTestEnable = ps->depthTest;
    dsInfo.depthWriteEnable = ps->depthWrite;
    dsInfo.depthCompareOp = toVkCompareOp(ps->depthOp);
    dsInfo.stencilTestEnable = ps->stencilTest;
    fillVkStencilOpState(&dsInfo.front, ps->stencilFront);
    dsInfo.front.compareMask = ps->stencilReadMask;
    dsInfo.front.writeMask = ps->stencilWriteMask;
    fillVkStencilOpState(&dsInfo.back, ps->stencilBack);
    dsInfo.back.compareMask = ps->stencilReadMask;
    dsInfo.back.writeMask = ps->stencilWriteMask;
    pipelineInfo.pDepthStencilState = &dsInfo;

    VkPipelineColorBlendStateCreateInfo blendInfo;
    memset(&blendInfo, 0, sizeof(blendInfo));
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    QVarLengthArray<VkPipelineColorBlendAttachmentState, 4> targetBlends;
    for (const QVkGraphicsPipeline::TargetBlend &b : qAsConst(ps->targetBlends)) {
        VkPipelineColorBlendAttachmentState blend;
        memset(&blend, 0, sizeof(blend));
        blend.blendEnable = b.enable;
        blend.srcColorBlendFactor = toVkBlendFactor(b.srcColor);
        blend.dstColorBlendFactor = toVkBlendFactor(b.dstColor);
        blend.colorBlendOp = toVkBlendOp(b.opColor);
        blend.srcAlphaBlendFactor = toVkBlendFactor(b.srcAlpha);
        blend.dstAlphaBlendFactor = toVkBlendFactor(b.dstAlpha);
        blend.alphaBlendOp = toVkBlendOp(b.opAlpha);
        blend.colorWriteMask = toVkColorComponents(b.colorWrite);
        targetBlends.append(blend);
    }
    blendInfo.attachmentCount = targetBlends.count();
    blendInfo.pAttachments = targetBlends.constData();
    pipelineInfo.pColorBlendState = &blendInfo;

    pipelineInfo.layout = ps->layout;

    Q_ASSERT(ps->renderPass && ps->renderPass->rp);
    pipelineInfo.renderPass = ps->renderPass->rp;

    err = d->df->vkCreateGraphicsPipelines(d->dev, d->pipelineCache, 1, &pipelineInfo, nullptr, &ps->pipeline);

    for (VkShaderModule shader : shaders)
        d->df->vkDestroyShaderModule(d->dev, shader, nullptr);

    if (err == VK_SUCCESS) {
        ps->lastActiveFrameSlot = -1;
        return true;
    } else {
        qWarning("Failed to create graphics pipeline: %d", err);
        return false;
    }
}

static inline VkDescriptorType toVkDescriptorType(QVkShaderResourceBindings::Binding::Type type)
{
    switch (type) {
    case QVkShaderResourceBindings::Binding::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case QVkShaderResourceBindings::Binding::SampledTexture:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    default:
        Q_UNREACHABLE();
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

static inline VkShaderStageFlags toVkShaderStageFlags(QVkShaderResourceBindings::Binding::StageFlags stage)
{
    int s = 0;
    if (stage.testFlag(QVkShaderResourceBindings::Binding::VertexStage))
        s |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stage.testFlag(QVkShaderResourceBindings::Binding::FragmentStage))
        s |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stage.testFlag(QVkShaderResourceBindings::Binding::GeometryStage))
        s |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (stage.testFlag(QVkShaderResourceBindings::Binding::TessellationControlStage))
        s |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stage.testFlag(QVkShaderResourceBindings::Binding::TessellationEvaluationStage))
        s |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return VkShaderStageFlags(s);
}

bool QVkRender::createShaderResourceBindings(QVkShaderResourceBindings *srb)
{
    if (srb->layout) // no repeated create without a releaseLater first
        return false;

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        srb->descSets[i] = VK_NULL_HANDLE;

    QVarLengthArray<VkDescriptorSetLayoutBinding, 4> bindings;
    for (const QVkShaderResourceBindings::Binding &b : qAsConst(srb->bindings)) {
        VkDescriptorSetLayoutBinding binding;
        memset(&binding, 0, sizeof(binding));
        binding.binding = b.binding;
        binding.descriptorType = toVkDescriptorType(b.type);
        binding.descriptorCount = 1; // no array support yet
        binding.stageFlags = toVkShaderStageFlags(b.stage);
        bindings.append(binding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    memset(&layoutInfo, 0, sizeof(layoutInfo));
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = uint32_t(bindings.count());
    layoutInfo.pBindings = bindings.constData();

    VkResult err = d->df->vkCreateDescriptorSetLayout(d->dev, &layoutInfo, nullptr, &srb->layout);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create descriptor set layout: %d", err);
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorSetCount = QVK_FRAMES_IN_FLIGHT;
    VkDescriptorSetLayout layouts[QVK_FRAMES_IN_FLIGHT];
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        layouts[i] = srb->layout;
    allocInfo.pSetLayouts = layouts;
    if (!d->allocateDescriptorSet(&allocInfo, srb->descSets, &srb->poolIndex))
        return false;

    QVarLengthArray<VkDescriptorBufferInfo, 4> bufferInfos;
    QVarLengthArray<VkDescriptorImageInfo, 4> imageInfos;
    QVarLengthArray<VkWriteDescriptorSet, 8> writeInfos;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        for (const QVkShaderResourceBindings::Binding &b : qAsConst(srb->bindings)) {
            VkWriteDescriptorSet writeInfo;
            memset(&writeInfo, 0, sizeof(writeInfo));
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.dstSet = srb->descSets[i];
            writeInfo.dstBinding = b.binding;
            writeInfo.descriptorCount = 1;

            switch (b.type) {
            case QVkShaderResourceBindings::Binding::UniformBuffer:
            {
                writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                VkDescriptorBufferInfo bufInfo;
                QVkBuffer *buf = b.ubuf.buf;
                bufInfo.buffer = buf->isStatic() ? buf->d[0].buffer : buf->d[i].buffer;
                bufInfo.offset = b.ubuf.offset;
                bufInfo.range = b.ubuf.size <= 0 ? buf->size : b.ubuf.size;
                // be nice and assert when we know the vulkan device would die a horrible death due to non-aligned reads
                Q_ASSERT(aligned(bufInfo.offset, d->ubufAlign) == bufInfo.offset);
                bufferInfos.append(bufInfo);
                writeInfo.pBufferInfo = &bufferInfos.last();
            }
                break;
            case QVkShaderResourceBindings::Binding::SampledTexture:
            {
                writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                VkDescriptorImageInfo imageInfo;
                imageInfo.sampler = b.stex.sampler->sampler;
                imageInfo.imageView = b.stex.tex->imageView;
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfos.append(imageInfo);
                writeInfo.pImageInfo = &imageInfos.last();
            }
                break;
            default:
                continue;
            }

            writeInfos.append(writeInfo);
        }
    }

    d->df->vkUpdateDescriptorSets(d->dev, writeInfos.count(), writeInfos.constData(), 0, nullptr);

    srb->lastActiveFrameSlot = -1;
    return true;
}

void QVkRenderPrivate::prepareBufferForUse(QVkBuffer *buf)
{
    buf->lastActiveFrameSlot = currentFrameSlot;

    auto &pendingUpdates(buf->d[currentFrameSlot].pendingUpdates);
    if (!pendingUpdates.isEmpty()) {
        Q_ASSERT(!buf->isStatic());
        void *p = nullptr;
        VmaAllocation a = toVmaAllocation(buf->d[currentFrameSlot].allocation);
        VkResult err = vmaMapMemory(allocator, a, &p);
        if (err != VK_SUCCESS) {
            qWarning("Failed to map buffer: %d", err);
            return;
        }

        int changeBegin = -1;
        int changeEnd = -1;
        for (const QVkBuffer::PendingUpdate &u : pendingUpdates) {
            memcpy(static_cast<char *>(p) + u.offset, u.data.constData(), u.data.size());
            if (changeBegin == -1 || u.offset < changeBegin)
                changeBegin = u.offset;
            if (changeEnd == -1 || u.offset + u.data.size() > changeEnd)
                changeEnd = u.offset + u.data.size();
        }

        vmaUnmapMemory(allocator, a);
        vmaFlushAllocation(allocator, a, changeBegin, changeEnd - changeBegin);

        pendingUpdates.clear();
    }
}

void QVkRender::setVertexInput(QVkCommandBuffer *cb, int startBinding, const QVector<VertexInput> &bindings,
                               QVkBuffer *indexBuf, quint32 indexOffset, IndexFormat indexFormat)
{
    QVarLengthArray<VkBuffer, 4> bufs;
    QVarLengthArray<VkDeviceSize, 4> ofs;
    for (int i = 0, ie = bindings.count(); i != ie; ++i) {
        QVkBuffer *buf = bindings[i].first;
        Q_ASSERT(buf->usage.testFlag(QVkBuffer::VertexBuffer));
        d->prepareBufferForUse(buf);
        const int idx = buf->isStatic() ? 0 : d->currentFrameSlot;
        bufs.append(buf->d[idx].buffer);
        ofs.append(bindings[i].second);
    }
    if (!bufs.isEmpty())
        d->df->vkCmdBindVertexBuffers(cb->cb, startBinding, bufs.count(), bufs.constData(), ofs.constData());

    if (indexBuf) {
        Q_ASSERT(indexBuf->usage.testFlag(QVkBuffer::IndexBuffer));
        d->prepareBufferForUse(indexBuf);
        const int idx = indexBuf->isStatic() ? 0 : d->currentFrameSlot;
        const VkIndexType type = indexFormat == IndexUInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        d->df->vkCmdBindIndexBuffer(cb->cb, indexBuf->d[idx].buffer, indexOffset, type);
    }
}

void QVkRender::setGraphicsPipeline(QVkCommandBuffer *cb, QVkGraphicsPipeline *ps, QVkShaderResourceBindings *srb)
{
    Q_ASSERT(ps->pipeline);

    if (!srb)
        srb = ps->shaderResourceBindings;

    bool hasDynamicBuffer = false; // excluding vertex and index
    for (const QVkShaderResourceBindings::Binding &b : qAsConst(srb->bindings)) {
        switch (b.type) {
        case QVkShaderResourceBindings::Binding::UniformBuffer:
            Q_ASSERT(b.ubuf.buf->usage.testFlag(QVkBuffer::UniformBuffer));
            d->prepareBufferForUse(b.ubuf.buf);
            if (!b.ubuf.buf->isStatic())
                hasDynamicBuffer = true;
            break;
        case QVkShaderResourceBindings::Binding::SampledTexture:
            b.stex.tex->lastActiveFrameSlot = d->currentFrameSlot;
            b.stex.sampler->lastActiveFrameSlot = d->currentFrameSlot;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    if (cb->currentPipeline != ps) {
        ps->lastActiveFrameSlot = d->currentFrameSlot;
        d->df->vkCmdBindPipeline(cb->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, ps->pipeline);
        cb->currentPipeline = ps;
    }

    if (hasDynamicBuffer || cb->currentSrb != srb) {
        srb->lastActiveFrameSlot = d->currentFrameSlot;
        const int descSetIdx = hasDynamicBuffer ? d->currentFrameSlot : 0;
        d->df->vkCmdBindDescriptorSets(cb->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, ps->layout, 0, 1,
                                       &srb->descSets[descSetIdx], 0, nullptr);
        cb->currentSrb = srb;
    }
}

static inline VkViewport toVkViewport(const QVkViewport &viewport)
{
    VkViewport vp;
    vp.x = viewport.r.x();
    vp.y = viewport.r.y();
    vp.width = viewport.r.width();
    vp.height = viewport.r.height();
    vp.minDepth = viewport.minDepth;
    vp.maxDepth = viewport.maxDepth;
    return vp;
}

void QVkRender::setViewport(QVkCommandBuffer *cb, const QVkViewport &viewport)
{
    VkViewport vp = toVkViewport(viewport);
    d->df->vkCmdSetViewport(cb->cb, 0, 1, &vp);
}

static inline VkRect2D toVkScissor(const QVkScissor &scissor)
{
    VkRect2D s;
    s.offset.x = scissor.r.x();
    s.offset.y = scissor.r.y();
    s.extent.width = scissor.r.width();
    s.extent.height = scissor.r.height();
    return s;
}

void QVkRender::setScissor(QVkCommandBuffer *cb, const QVkScissor &scissor)
{
    VkRect2D s = toVkScissor(scissor);
    d->df->vkCmdSetScissor(cb->cb, 0, 1, &s);
}

void QVkRender::setBlendConstants(QVkCommandBuffer *cb, const QVector4D &c)
{
    const float bc[4] = { c.x(), c.y(), c.z(), c.w() };
    d->df->vkCmdSetBlendConstants(cb->cb, bc);
}

void QVkRender::setStencilRef(QVkCommandBuffer *cb, quint32 refValue)
{
    d->df->vkCmdSetStencilReference(cb->cb, VK_STENCIL_FRONT_AND_BACK, refValue);
}

void QVkRender::draw(QVkCommandBuffer *cb, quint32 vertexCount,
                     quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    d->df->vkCmdDraw(cb->cb, vertexCount, instanceCount, firstVertex, firstInstance);
}

void QVkRender::drawIndexed(QVkCommandBuffer *cb, quint32 indexCount,
                            quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
    d->df->vkCmdDrawIndexed(cb->cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void QVkRenderPrivate::prepareNewFrame(QVkCommandBuffer *cb)
{
    Q_ASSERT(!inFrame);
    inFrame = true;

    executeDeferredReleases();

    cb->resetState();
}

void QVkRenderPrivate::finishFrame()
{
    Q_ASSERT(inFrame);
    inFrame = false;
    ++finishedFrameCount;
}

void QVkRenderPrivate::executeDeferredReleases(bool forced)
{
    for (int i = releaseQueue.count() - 1; i >= 0; --i) {
        const QVkRenderPrivate::DeferredReleaseEntry &e(releaseQueue[i]);
        if (forced || currentFrameSlot == e.lastActiveFrameSlot || e.lastActiveFrameSlot < 0) {
            switch (e.type) {
            case QVkRenderPrivate::DeferredReleaseEntry::Pipeline:
                df->vkDestroyPipeline(dev, e.pipelineState.pipeline, nullptr);
                df->vkDestroyPipelineLayout(dev, e.pipelineState.layout, nullptr);
                break;
            case QVkRenderPrivate::DeferredReleaseEntry::ShaderResourceBindings:
                df->vkDestroyDescriptorSetLayout(dev, e.shaderResourceBindings.layout, nullptr);
                if (e.shaderResourceBindings.poolIndex >= 0) {
                    descriptorPools[e.shaderResourceBindings.poolIndex].activeSets -= 1;
                    Q_ASSERT(descriptorPools[e.shaderResourceBindings.poolIndex].activeSets >= 0);
                }
                break;
            case QVkRenderPrivate::DeferredReleaseEntry::Buffer:
                for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
                    vmaDestroyBuffer(allocator, e.buffer.buffers[i], toVmaAllocation(e.buffer.allocations[i]));
                vmaDestroyBuffer(allocator, e.buffer.stagingBuffer, toVmaAllocation(e.buffer.stagingAlloc));
                break;
            case QVkRenderPrivate::DeferredReleaseEntry::RenderBuffer:
                df->vkDestroyImageView(dev, e.renderBuffer.imageView, nullptr);
                df->vkDestroyImage(dev, e.renderBuffer.image, nullptr);
                df->vkFreeMemory(dev, e.renderBuffer.memory, nullptr);
                break;
            case QVkRenderPrivate::DeferredReleaseEntry::Texture:
                df->vkDestroyImageView(dev, e.texture.imageView, nullptr);
                vmaDestroyImage(allocator, e.texture.image, toVmaAllocation(e.texture.allocation));
                vmaDestroyImage(allocator, e.texture.stagingImage, toVmaAllocation(e.texture.stagingAlloc));
                break;
            case QVkRenderPrivate::DeferredReleaseEntry::Sampler:
                df->vkDestroySampler(dev, e.sampler.sampler, nullptr);
                break;
            default:
                break;
            }
            releaseQueue.removeAt(i);
        }
    }
}

void QVkRender::releaseLater(QVkGraphicsPipeline *ps)
{
    if (!ps->pipeline && !ps->layout)
        return;

    QVkRenderPrivate::DeferredReleaseEntry e;
    e.type = QVkRenderPrivate::DeferredReleaseEntry::Pipeline;
    e.lastActiveFrameSlot = ps->lastActiveFrameSlot;

    e.pipelineState.pipeline = ps->pipeline;
    e.pipelineState.layout = ps->layout;

    ps->pipeline = VK_NULL_HANDLE;
    ps->layout = VK_NULL_HANDLE;

    d->releaseQueue.append(e);
}

void QVkRender::releaseLater(QVkShaderResourceBindings *srb)
{
    if (!srb->layout)
        return;

    QVkRenderPrivate::DeferredReleaseEntry e;
    e.type = QVkRenderPrivate::DeferredReleaseEntry::ShaderResourceBindings;
    e.lastActiveFrameSlot = srb->lastActiveFrameSlot;

    e.shaderResourceBindings.poolIndex = srb->poolIndex;
    e.shaderResourceBindings.layout = srb->layout;

    srb->poolIndex = -1;
    srb->layout = VK_NULL_HANDLE;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        srb->descSets[i] = VK_NULL_HANDLE;

    d->releaseQueue.append(e);
}

void QVkRender::releaseLater(QVkBuffer *buf)
{
    int nullBufferCount = 0;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        if (!buf->d[i].buffer)
            ++nullBufferCount;
    }
    if (nullBufferCount == QVK_FRAMES_IN_FLIGHT)
        return;

    QVkRenderPrivate::DeferredReleaseEntry e;
    e.type = QVkRenderPrivate::DeferredReleaseEntry::Buffer;
    e.lastActiveFrameSlot = buf->lastActiveFrameSlot;

    e.buffer.stagingBuffer = buf->stagingBuffer;
    e.buffer.stagingAlloc = buf->stagingAlloc;

    buf->stagingBuffer = VK_NULL_HANDLE;
    buf->stagingAlloc = nullptr;

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        e.buffer.buffers[i] = buf->d[i].buffer;
        e.buffer.allocations[i] = buf->d[i].allocation;

        buf->d[i].buffer = VK_NULL_HANDLE;
        buf->d[i].allocation = nullptr;
    }

    d->releaseQueue.append(e);
}

void QVkRender::releaseLater(QVkRenderBuffer *rb)
{
    if (!rb->memory)
        return;

    QVkRenderPrivate::DeferredReleaseEntry e;
    e.type = QVkRenderPrivate::DeferredReleaseEntry::RenderBuffer;
    e.lastActiveFrameSlot = rb->lastActiveFrameSlot;

    e.renderBuffer.memory = rb->memory;
    e.renderBuffer.image = rb->image;
    e.renderBuffer.imageView = rb->imageView;

    rb->memory = VK_NULL_HANDLE;
    rb->image = VK_NULL_HANDLE;
    rb->imageView = VK_NULL_HANDLE;

    d->releaseQueue.append(e);
}

void QVkRender::releaseLater(QVkTexture *tex)
{
    if (!tex->image)
        return;

    QVkRenderPrivate::DeferredReleaseEntry e;
    e.type = QVkRenderPrivate::DeferredReleaseEntry::Texture;
    e.lastActiveFrameSlot = tex->lastActiveFrameSlot;

    e.texture.image = tex->image;
    e.texture.imageView = tex->imageView;
    e.texture.allocation = tex->allocation;
    e.texture.stagingImage = tex->stagingImage;
    e.texture.stagingAlloc = tex->stagingAlloc;

    tex->image = VK_NULL_HANDLE;
    tex->imageView = VK_NULL_HANDLE;
    tex->allocation = nullptr;
    tex->stagingImage = VK_NULL_HANDLE;
    tex->stagingAlloc = nullptr;

    d->releaseQueue.append(e);
}

void QVkRender::releaseLater(QVkSampler *sampler)
{
    if (!sampler->sampler)
        return;

    QVkRenderPrivate::DeferredReleaseEntry e;
    e.type = QVkRenderPrivate::DeferredReleaseEntry::Sampler;
    e.lastActiveFrameSlot = sampler->lastActiveFrameSlot;

    e.sampler.sampler = sampler->sampler;
    sampler->sampler = VK_NULL_HANDLE;

    d->releaseQueue.append(e);
}

static struct {
    VkSampleCountFlagBits mask;
    int count;
} qvk_sampleCounts[] = {
    // keep this sorted by 'count'
    { VK_SAMPLE_COUNT_1_BIT, 1 },
    { VK_SAMPLE_COUNT_2_BIT, 2 },
    { VK_SAMPLE_COUNT_4_BIT, 4 },
    { VK_SAMPLE_COUNT_8_BIT, 8 },
    { VK_SAMPLE_COUNT_16_BIT, 16 },
    { VK_SAMPLE_COUNT_32_BIT, 32 },
    { VK_SAMPLE_COUNT_64_BIT, 64 }
};

QVector<int> QVkRender::supportedSampleCounts() const
{
    const VkPhysicalDeviceLimits *limits = &d->physDevProperties.limits;
    VkSampleCountFlags color = limits->framebufferColorSampleCounts;
    VkSampleCountFlags depth = limits->framebufferDepthSampleCounts;
    VkSampleCountFlags stencil = limits->framebufferStencilSampleCounts;
    QVector<int> result;

    for (size_t i = 0; i < sizeof(qvk_sampleCounts) / sizeof(qvk_sampleCounts[0]); ++i) {
        if ((color & qvk_sampleCounts[i].mask)
                && (depth & qvk_sampleCounts[i].mask)
                && (stencil & qvk_sampleCounts[i].mask))
        {
            result.append(qvk_sampleCounts[i].count);
        }
    }

    return result;
}

VkSampleCountFlagBits QVkRenderPrivate::effectiveSampleCount(int sampleCount)
{
    // Stay compatible with QSurfaceFormat and friends where samples == 0 means the same as 1.
    sampleCount = qBound(1, sampleCount, 64);

    if (!q->supportedSampleCounts().contains(sampleCount)) {
        qWarning("Attempted to set unsupported sample count %d", sampleCount);
        return VK_SAMPLE_COUNT_1_BIT;
    }

    for (size_t i = 0; i < sizeof(qvk_sampleCounts) / sizeof(qvk_sampleCounts[0]); ++i) {
        if (qvk_sampleCounts[i].count == sampleCount)
            return qvk_sampleCounts[i].mask;
    }

    Q_UNREACHABLE();
}

QT_END_NAMESPACE
