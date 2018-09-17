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

#define RES_D(t) t##Private *d = static_cast<t##Private *>(d_ptr)
#define RES_GET_D(t, x) static_cast<t##Private *>(QRhiResourcePrivate::get(x))
#define RHI_D(t) t *d = static_cast<t *>(d_ptr)
#define RHI_GET_D(t, x) static_cast<t *>(QRhiPrivate::get(x))
#define RES_RHI(t) t *rhiD = RHI_GET_D(t, d->rhi)

QRhi::QRhi(const InitParams &params)
    : d_ptr(new QRhiVulkan(this))
{
    RHI_D(QRhiVulkan);

    d->inst = params.inst;
    d->physDev = params.physDev;
    d->dev = params.dev;
    d->cmdPool = params.cmdPool;
    d->gfxQueue = params.gfxQueue;

    d->create();
}

QRhi::~QRhi()
{
    RHI_D(QRhiVulkan);
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

void QRhiVulkan::create()
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

VkResult QRhiVulkan::createDescriptorPool(VkDescriptorPool *pool)
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

bool QRhiVulkan::allocateDescriptorSet(VkDescriptorSetAllocateInfo *allocInfo, VkDescriptorSet *result, int *resultPoolIndex)
{
    auto tryAllocate = [this, allocInfo, result](int poolIndex) {
        allocInfo->descriptorPool = descriptorPools[poolIndex].pool;
        VkResult r = df->vkAllocateDescriptorSets(dev, allocInfo, result);
        if (r == VK_SUCCESS)
            descriptorPools[poolIndex].refCount += 1;
        return r;
    };

    int lastPoolIdx = descriptorPools.count() - 1;
    for (int i = lastPoolIdx; i >= 0; --i) {
        if (descriptorPools[i].refCount == 0) {
            df->vkResetDescriptorPool(dev, descriptorPools[i].pool, 0);
            descriptorPools[i].allocedDescSets = 0;
        }
        if (descriptorPools[i].allocedDescSets + allocInfo->descriptorSetCount <= QVK_DESC_SETS_PER_POOL) {
            VkResult err = tryAllocate(i);
            if (err == VK_SUCCESS) {
                descriptorPools[i].allocedDescSets += allocInfo->descriptorSetCount;
                *resultPoolIndex = i;
                return true;
            }
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
        descriptorPools[lastPoolIdx].allocedDescSets += allocInfo->descriptorSetCount;
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

uint32_t QRhiVulkan::chooseTransientImageMemType(VkImage img, uint32_t startIndex)
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

bool QRhiVulkan::createTransientImage(VkFormat format,
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
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

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

void QRhiVulkan::destroy()
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

VkFormat QRhiVulkan::optimalDepthStencilFormat()
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

bool QRhiVulkan::createDefaultRenderPass(QRhiRenderPass *rp, bool hasDepthStencil, VkSampleCountFlagBits sampleCount, VkFormat colorFormat)
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

QRhiSwapChain *QRhi::createSwapChain()
{
    return new QVkSwapChain(this);
}

bool QRhiVulkan::rebuildSwapChain(QWindow *window, const QSize &pixelSize,
                                  QRhiSwapChain::SurfaceImportFlags flags, QRhiRenderBuffer *depthStencil,
                                  int sampleCount, QRhiSwapChain *outSwapChain)
{
    // Can be called multiple times without a call to releaseSwapChainResources
    // - this is typical when a window is resized.

    VkSurfaceKHR surface = QVulkanInstance::surfaceForWindow(window);
    if (!surface) {
        qWarning("Failed to get surface for window");
        return false;
    }

    if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHR) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
        vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
        if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !vkGetPhysicalDeviceSurfaceFormatsKHR) {
            qWarning("Physical device surface queries not available");
            return false;
        }
    }

    quint32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatCount, nullptr);
    QVector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount)
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatCount, formats.data());

    QVkSwapChainPrivate *swapChainD = RES_GET_D(QVkSwapChain, outSwapChain);

    // Pick the preferred format, if there is one.
    if (!formats.isEmpty() && formats[0].format != VK_FORMAT_UNDEFINED) {
        swapChainD->colorFormat = formats[0].format;
        swapChainD->colorSpace = formats[0].colorSpace;
    }

    swapChainD->depthStencil = flags.testFlag(QRhiSwapChain::UseDepthStencil) ? depthStencil : nullptr;
    if (swapChainD->depthStencil && swapChainD->depthStencil->sampleCount != sampleCount) {
        qWarning("Depth-stencil buffer's sampleCount (%d) does not match color buffers' sample count (%d). Expect problems.",
                 swapChainD->depthStencil->sampleCount, sampleCount);
    }
    swapChainD->sampleCount = effectiveSampleCount(sampleCount);

    if (!recreateSwapChain(surface, pixelSize, flags, outSwapChain))
        return false;

    createDefaultRenderPass(&swapChainD->rt.rp, swapChainD->depthStencil != nullptr,
                            swapChainD->sampleCount, swapChainD->colorFormat);

    swapChainD->rt.attCount = 1;
    if (swapChainD->depthStencil)
        swapChainD->rt.attCount += 1;
    if (swapChainD->sampleCount > VK_SAMPLE_COUNT_1_BIT)
        swapChainD->rt.attCount += 1;

    for (int i = 0; i < swapChainD->bufferCount; ++i) {
        QVkSwapChainPrivate::ImageResources &image(swapChainD->imageRes[i]);

        VkImageView views[3] = {
            image.imageView,
            swapChainD->depthStencil ? RES_GET_D(QVkRenderBuffer, swapChainD->depthStencil)->imageView : VK_NULL_HANDLE,
            swapChainD->sampleCount > VK_SAMPLE_COUNT_1_BIT ? image.msaaImageView : VK_NULL_HANDLE
        };
        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = swapChainD->rt.rp.rp;
        fbInfo.attachmentCount = swapChainD->rt.attCount;
        fbInfo.pAttachments = views;
        fbInfo.width = swapChainD->pixelSize.width();
        fbInfo.height = swapChainD->pixelSize.height();
        fbInfo.layers = 1;
        VkResult err = df->vkCreateFramebuffer(dev, &fbInfo, nullptr, &image.fb);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create framebuffer: %d", err);
            return false;
        }
    }

    return true;
}

bool QRhiVulkan::recreateSwapChain(VkSurfaceKHR surface, const QSize &pixelSize,
                                    QRhiSwapChain::SurfaceImportFlags flags, QRhiSwapChain *swapChain)
{
    QVkSwapChainPrivate *swapChainD = RES_GET_D(QVkSwapChain, swapChain);

    swapChainD->pixelSize = pixelSize;
    if (swapChainD->pixelSize.isEmpty())
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
    quint32 reqBufferCount = QVkSwapChainPrivate::DEFAULT_BUFFER_COUNT;
    if (surfaceCaps.maxImageCount)
        reqBufferCount = qBound(surfaceCaps.minImageCount, reqBufferCount, surfaceCaps.maxImageCount);

    VkExtent2D bufferSize = surfaceCaps.currentExtent;
    if (bufferSize.width == quint32(-1)) {
        Q_ASSERT(bufferSize.height == quint32(-1));
        bufferSize.width = swapChainD->pixelSize.width();
        bufferSize.height = swapChainD->pixelSize.height();
    } else {
        swapChainD->pixelSize = QSize(bufferSize.width, bufferSize.height);
    }

    VkSurfaceTransformFlagBitsKHR preTransform =
        (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
        : surfaceCaps.currentTransform;

    VkCompositeAlphaFlagBitsKHR compositeAlpha =
        (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
        : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    if (flags.testFlag(QRhiSwapChain::SurfaceHasPreMulAlpha)
            && (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR))
    {
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }

    if (flags.testFlag(QRhiSwapChain::SurfaceHasNonPreMulAlpha)
            && (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR))
    {
        compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainD->supportsReadback = (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (swapChainD->supportsReadback)
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    qDebug("Creating new swap chain of %d buffers, size %dx%d", reqBufferCount, bufferSize.width, bufferSize.height);

    VkSwapchainKHR oldSwapChain = swapChainD->sc;
    VkSwapchainCreateInfoKHR swapChainInfo;
    memset(&swapChainInfo, 0, sizeof(swapChainInfo));
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = surface;
    swapChainInfo.minImageCount = reqBufferCount;
    swapChainInfo.imageFormat = swapChainD->colorFormat;
    swapChainInfo.imageColorSpace = swapChainD->colorSpace;
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
        releaseSwapChainResources(swapChain);

    swapChainD->sc = newSwapChain;

    quint32 actualSwapChainBufferCount = 0;
    err = vkGetSwapchainImagesKHR(dev, swapChainD->sc, &actualSwapChainBufferCount, nullptr);
    if (err != VK_SUCCESS || actualSwapChainBufferCount < 2) {
        qWarning("Failed to get swapchain images: %d (count=%d)", err, actualSwapChainBufferCount);
        return false;
    }

    if (actualSwapChainBufferCount > QVkSwapChainPrivate::MAX_BUFFER_COUNT) {
        qWarning("Too many swapchain buffers (%d)", actualSwapChainBufferCount);
        return false;
    }
    swapChainD->bufferCount = actualSwapChainBufferCount;

    VkImage swapChainImages[QVkSwapChainPrivate::MAX_BUFFER_COUNT];
    err = vkGetSwapchainImagesKHR(dev, swapChainD->sc, &actualSwapChainBufferCount, swapChainImages);
    if (err != VK_SUCCESS) {
        qWarning("Failed to get swapchain images: %d", err);
        return false;
    }

    VkImage msaaImages[QVkSwapChainPrivate::MAX_BUFFER_COUNT];
    VkImageView msaaViews[QVkSwapChainPrivate::MAX_BUFFER_COUNT];
    if (swapChainD->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
        if (!createTransientImage(swapChainD->colorFormat,
                                  swapChainD->pixelSize,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  swapChainD->sampleCount,
                                  &swapChainD->msaaImageMem,
                                  msaaImages,
                                  msaaViews,
                                  swapChainD->bufferCount))
        {
            return false;
        }
    }

    VkFenceCreateInfo fenceInfo;
    memset(&fenceInfo, 0, sizeof(fenceInfo));
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < swapChainD->bufferCount; ++i) {
        QVkSwapChainPrivate::ImageResources &image(swapChainD->imageRes[i]);
        image.image = swapChainImages[i];
        if (swapChainD->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
            image.msaaImage = msaaImages[i];
            image.msaaImageView = msaaViews[i];
        }

        VkImageViewCreateInfo imgViewInfo;
        memset(&imgViewInfo, 0, sizeof(imgViewInfo));
        imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imgViewInfo.image = swapChainImages[i];
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = swapChainD->colorFormat;
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

    swapChainD->currentImage = 0;

    VkSemaphoreCreateInfo semInfo;
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        QVkSwapChainPrivate::FrameResources &frame(swapChainD->frameRes[i]);

        frame.imageAcquired = false;
        frame.imageSemWaitable = false;

        df->vkCreateFence(dev, &fenceInfo, nullptr, &frame.fence);
        frame.fenceWaitable = true; // fence was created in signaled state

        df->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.imageSem);
        df->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.drawSem);
    }

    swapChainD->currentFrame = 0;

    return true;
}

void QRhiVulkan::releaseSwapChainResources(QRhiSwapChain *swapChain)
{
    QVkSwapChainPrivate *swapChainD = RES_GET_D(QVkSwapChain, swapChain);

    if (swapChainD->sc == VK_NULL_HANDLE)
        return;

    df->vkDeviceWaitIdle(dev);

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        QVkSwapChainPrivate::FrameResources &frame(swapChainD->frameRes[i]);
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

    for (int i = 0; i < swapChainD->bufferCount; ++i) {
        QVkSwapChainPrivate::ImageResources &image(swapChainD->imageRes[i]);
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

    if (swapChainD->msaaImageMem) {
        df->vkFreeMemory(dev, swapChainD->msaaImageMem, nullptr);
        swapChainD->msaaImageMem = VK_NULL_HANDLE;
    }

    if (swapChainD->rt) {
        swapChainD->rt->release();
        delete swapChainD->rt;
        swapChainD->rt = nullptr;
    }
//    if (swapChainD->rt.rp.rp) {
//        df->vkDestroyRenderPass(dev, swapChainD->rt.rp.rp, nullptr);
//        swapChainD->rt.rp.rp = VK_NULL_HANDLE;
//    }

    vkDestroySwapchainKHR(dev, swapChainD->sc, nullptr);
    swapChainD->sc = VK_NULL_HANDLE;
}

static inline bool checkDeviceLost(VkResult err)
{
    if (err == VK_ERROR_DEVICE_LOST) {
        qWarning("Device lost");
        return true;
    }
    return false;
}

QRhi::FrameOpResult QRhi::beginFrame(QRhiSwapChain *swapChain)
{
    RHI_D(QRhiVulkan);
    QVkSwapChainPrivate *swapChainD = RES_GET_D(QVkSwapChain, swapChain);

    QVkSwapChainPrivate::FrameResources &frame(swapChainD->frameRes[swapChainD->currentFrame]);

    if (!frame.imageAcquired) {
        // Wait if we are too far ahead, i.e. the thread gets throttled based on the presentation rate
        // (note that we are using FIFO mode -> vsync)
        if (frame.fenceWaitable) {
            d->df->vkWaitForFences(d->dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
            d->df->vkResetFences(d->dev, 1, &frame.fence);
            frame.fenceWaitable = false;
        }

        // move on to next swapchain image
        VkResult err = d->vkAcquireNextImageKHR(d->dev, swapChainD->sc, UINT64_MAX,
                                                frame.imageSem, frame.fence, &swapChainD->currentImage);
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
    QVkSwapChainPrivate::ImageResources &image(swapChainD->imageRes[swapChainD->currentImage]);
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

    swapChainD->rt.fb = image.fb;
    swapChainD->rt.pixelSize = swapChainD->pixelSize;

    d->currentFrameSlot = swapChainD->currentFrame;
    RES_GET_D(QVkRenderPass, swapChainD->rt.rp)->lastActiveFrameSlot = d->currentFrameSlot;
    if (swapChainD->depthStencil)
        RES_GET_D(QVkRenderBuffer, swapChainD->depthStencil)->lastActiveFrameSlot = d->currentFrameSlot;

    d->prepareNewFrame(&image.cmdBuf);

    return FrameOpSuccess;
}

QRhi::FrameOpResult QRhi::endFrame(QRhiSwapChain *swapChain)
{
    RHI_D(QRhiVulkan);
    QVkSwapChainPrivate *swapChainD = RES_GET_D(QVkSwapChain, swapChain);

    d->finishFrame();

    QVkSwapChainPrivate::FrameResources &frame(swapChainD->frameRes[swapChainD->currentFrame]);
    QVkSwapChainPrivate::ImageResources &image(swapChainD->imageRes[swapChainD->currentImage]);

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
    presInfo.pSwapchains = &swapChainD->sc;
    presInfo.pImageIndices = &swapChainD->currentImage;
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

    swapChainD->currentFrame = (swapChainD->currentFrame + 1) % QVK_FRAMES_IN_FLIGHT;

    return FrameOpSuccess;
}

void QRhi::importVulkanWindowRenderPass(QVulkanWindow *window, QRhiRenderPass *outRp)
{
    outRp->rp = window->defaultRenderPass();
}

void QRhi::beginFrame(QVulkanWindow *window,
                      QRhiRenderTarget *outCurrentFrameRenderTarget,
                      QRhiCommandBuffer *outCurrentFrameCommandBuffer)
{
    RHI_D(QRhiVulkan);

    outCurrentFrameRenderTarget->fb = window->currentFramebuffer();
    importVulkanWindowRenderPass(window, &outCurrentFrameRenderTarget->rp);
    outCurrentFrameRenderTarget->pixelSize = window->swapChainImageSize();
    outCurrentFrameRenderTarget->attCount = window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;

    outCurrentFrameCommandBuffer->cb = window->currentCommandBuffer();

    d->currentFrameSlot = window->currentFrame();
    d->prepareNewFrame(outCurrentFrameCommandBuffer);
}

void QRhi::endFrame(QVulkanWindow *window)
{
    Q_UNUSED(window);
    RHI_D(QRhiVulkan);
    d->finishFrame();
}

static inline VkBufferUsageFlagBits toVkBufferUsage(QRhiBuffer::UsageFlags usage)
{
    int u = 0;
    if (usage.testFlag(QRhiBuffer::VertexBuffer))
        u |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage.testFlag(QRhiBuffer::IndexBuffer))
        u |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage.testFlag(QRhiBuffer::UniformBuffer))
        u |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    return VkBufferUsageFlagBits(u);
}

QRhiBuffer *QRhi::createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, int size)
{
    return new QVkBuffer(this, type, usage, size);
}

int QRhi::ubufAlignment() const
{
    RHI_D(const QRhiVulkan);
    return d->ubufAlign; // typically 256 (bytes)
}

int QRhi::ubufAligned(int v) const
{
    RHI_D(const QRhiVulkan);
    return aligned(v, d->ubufAlign);
}

QRhiRenderBuffer *QRhi::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize, int sampleCount)
{
    return new QVkRenderBuffer(this, type, pixelSize, sampleCount);
}

static inline VkFormat toVkTextureFormat(QRhiTexture::Format format)
{
    switch (format) {
    case QRhiTexture::RGBA8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case QRhiTexture::BGRA8:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case QRhiTexture::R8:
        return VK_FORMAT_R8_UNORM;
    case QRhiTexture::R16:
        return VK_FORMAT_R16_UNORM;

    case QRhiTexture::D16:
        return VK_FORMAT_D16_UNORM;
    case QRhiTexture::D32:
        return VK_FORMAT_D32_SFLOAT;

    default:
        Q_UNREACHABLE();
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

static inline bool isDepthStencilTextureFormat(QRhiTexture::Format format)
{
    switch (format) {
    case QRhiTexture::Format::D16:
        Q_FALLTHROUGH();
    case QRhiTexture::Format::D32:
        return true;

    default:
        return false;
    }
}

static inline QSize safeSize(const QSize &size)
{
    return size.isEmpty() ? QSize(16, 16) : size;
}

QRhiTexture *QRhi::createTexture(QRhiTexture::Format format, const QSize &pixelSize, QRhiTexture::Flags flags)
{
    return new QVkTexture(this, format, pixelSize, flags);
}

static inline VkFilter toVkFilter(QRhiSampler::Filter f)
{
    switch (f) {
    case QRhiSampler::Nearest:
        return VK_FILTER_NEAREST;
    case QRhiSampler::Linear:
        return VK_FILTER_LINEAR;
    default:
        Q_UNREACHABLE();
        return VK_FILTER_NEAREST;
    }
}

static inline VkSamplerMipmapMode toVkMipmapMode(QRhiSampler::Filter f)
{
    switch (f) {
    case QRhiSampler::Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case QRhiSampler::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        Q_UNREACHABLE();
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
}

static inline VkSamplerAddressMode toVkAddressMode(QRhiSampler::AddressMode m)
{
    switch (m) {
    case QRhiSampler::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case QRhiSampler::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case QRhiSampler::Border:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case QRhiSampler::Mirror:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case QRhiSampler::MirrorOnce:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:
        Q_UNREACHABLE();
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}

QRhiSampler *QRhi::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                 QRhiSampler::Filter mipmapMode,
                                 QRhiSampler::AddressMode u, QRhiSampler::AddressMode v)
{
    return new QVkSampler(this, magFilter, minFilter, mipmapMode, u, v);
}

QRhiTextureRenderTarget *QRhi::createTextureRenderTarget()
{
    return new QVkTextureRenderTarget(this);
}

VkShaderModule QRhiVulkan::createShader(const QByteArray &spirv)
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

bool QRhiVulkan::ensurePipelineCache()
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

static inline VkShaderStageFlagBits toVkShaderStage(QRhiGraphicsShaderStage::Type type)
{
    switch (type) {
    case QRhiGraphicsShaderStage::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case QRhiGraphicsShaderStage::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case QRhiGraphicsShaderStage::Geometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case QRhiGraphicsShaderStage::TessellationControl:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case QRhiGraphicsShaderStage::TessellationEvaluation:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    default:
        Q_UNREACHABLE();
        return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

static inline VkFormat toVkAttributeFormat(QRhiVertexInputLayout::Attribute::Format format)
{
    switch (format) {
    case QRhiVertexInputLayout::Attribute::Float4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case QRhiVertexInputLayout::Attribute::Float3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case QRhiVertexInputLayout::Attribute::Float2:
        return VK_FORMAT_R32G32_SFLOAT;
    case QRhiVertexInputLayout::Attribute::Float:
        return VK_FORMAT_R32_SFLOAT;
    case QRhiVertexInputLayout::Attribute::UNormByte4:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case QRhiVertexInputLayout::Attribute::UNormByte2:
        return VK_FORMAT_R8G8_UNORM;
    case QRhiVertexInputLayout::Attribute::UNormByte:
        return VK_FORMAT_R8_UNORM;
    default:
        Q_UNREACHABLE();
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

static inline VkPrimitiveTopology toVkTopology(QRhiGraphicsPipeline::Topology t)
{
    switch (t) {
    case QRhiGraphicsPipeline::Triangles:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case QRhiGraphicsPipeline::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case QRhiGraphicsPipeline::TriangleFan:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case QRhiGraphicsPipeline::Lines:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case QRhiGraphicsPipeline::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case QRhiGraphicsPipeline::Points:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    default:
        Q_UNREACHABLE();
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

static inline VkCullModeFlags toVkCullMode(QRhiGraphicsPipeline::CullMode c)
{
    int m = 0;
    if (c.testFlag(QRhiGraphicsPipeline::Front))
        m |= VK_CULL_MODE_FRONT_BIT;
    if (c.testFlag(QRhiGraphicsPipeline::Back))
        m |= VK_CULL_MODE_BACK_BIT;
    return VkCullModeFlags(m);
}

static inline VkFrontFace toVkFrontFace(QRhiGraphicsPipeline::FrontFace f)
{
    switch (f) {
    case QRhiGraphicsPipeline::CCW:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case QRhiGraphicsPipeline::CW:
        return VK_FRONT_FACE_CLOCKWISE;
    default:
        Q_UNREACHABLE();
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

static inline VkColorComponentFlags toVkColorComponents(QRhiGraphicsPipeline::ColorMask c)
{
    int f = 0;
    if (c.testFlag(QRhiGraphicsPipeline::R))
        f |= VK_COLOR_COMPONENT_R_BIT;
    if (c.testFlag(QRhiGraphicsPipeline::G))
        f |= VK_COLOR_COMPONENT_G_BIT;
    if (c.testFlag(QRhiGraphicsPipeline::B))
        f |= VK_COLOR_COMPONENT_B_BIT;
    if (c.testFlag(QRhiGraphicsPipeline::A))
        f |= VK_COLOR_COMPONENT_A_BIT;
    return VkColorComponentFlags(f);
}

static inline VkBlendFactor toVkBlendFactor(QRhiGraphicsPipeline::BlendFactor f)
{
    switch (f) {
    case QRhiGraphicsPipeline::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case QRhiGraphicsPipeline::One:
        return VK_BLEND_FACTOR_ONE;
    case QRhiGraphicsPipeline::SrcColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case QRhiGraphicsPipeline::OneMinusSrcColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case QRhiGraphicsPipeline::DstColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case QRhiGraphicsPipeline::OneMinusDstColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case QRhiGraphicsPipeline::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case QRhiGraphicsPipeline::OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case QRhiGraphicsPipeline::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case QRhiGraphicsPipeline::OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case QRhiGraphicsPipeline::ConstantColor:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case QRhiGraphicsPipeline::OneMinusConstantColor:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case QRhiGraphicsPipeline::ConstantAlpha:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case QRhiGraphicsPipeline::OneMinusConstantAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case QRhiGraphicsPipeline::SrcAlphaSaturate:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case QRhiGraphicsPipeline::Src1Color:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case QRhiGraphicsPipeline::OneMinusSrc1Color:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case QRhiGraphicsPipeline::Src1Alpha:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case QRhiGraphicsPipeline::OneMinusSrc1Alpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    default:
        Q_UNREACHABLE();
        return VK_BLEND_FACTOR_ZERO;
    }
}

static inline VkBlendOp toVkBlendOp(QRhiGraphicsPipeline::BlendOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::Add:
        return VK_BLEND_OP_ADD;
    case QRhiGraphicsPipeline::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case QRhiGraphicsPipeline::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case QRhiGraphicsPipeline::Min:
        return VK_BLEND_OP_MIN;
    case QRhiGraphicsPipeline::Max:
        return VK_BLEND_OP_MAX;
    default:
        Q_UNREACHABLE();
        return VK_BLEND_OP_ADD;
    }
}

static inline VkCompareOp toVkCompareOp(QRhiGraphicsPipeline::CompareOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::Never:
        return VK_COMPARE_OP_NEVER;
    case QRhiGraphicsPipeline::Less:
        return VK_COMPARE_OP_LESS;
    case QRhiGraphicsPipeline::Equal:
        return VK_COMPARE_OP_EQUAL;
    case QRhiGraphicsPipeline::LessOrEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case QRhiGraphicsPipeline::Greater:
        return VK_COMPARE_OP_GREATER;
    case QRhiGraphicsPipeline::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case QRhiGraphicsPipeline::GreaterOrEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case QRhiGraphicsPipeline::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        Q_UNREACHABLE();
        return VK_COMPARE_OP_ALWAYS;
    }
}

static inline VkStencilOp toVkStencilOp(QRhiGraphicsPipeline::StencilOp op)
{
    switch (op) {
    case QRhiGraphicsPipeline::StencilZero:
        return VK_STENCIL_OP_ZERO;
    case QRhiGraphicsPipeline::Keep:
        return VK_STENCIL_OP_KEEP;
    case QRhiGraphicsPipeline::Replace:
        return VK_STENCIL_OP_REPLACE;
    case QRhiGraphicsPipeline::IncrementAndClamp:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case QRhiGraphicsPipeline::DecrementAndClamp:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case QRhiGraphicsPipeline::Invert:
        return VK_STENCIL_OP_INVERT;
    case QRhiGraphicsPipeline::IncrementAndWrap:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case QRhiGraphicsPipeline::DecrementAndWrap:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    default:
        Q_UNREACHABLE();
        return VK_STENCIL_OP_KEEP;
    }
}

static inline void fillVkStencilOpState(VkStencilOpState *dst, const QRhiGraphicsPipeline::StencilOpState &src)
{
    dst->failOp = toVkStencilOp(src.failOp);
    dst->passOp = toVkStencilOp(src.passOp);
    dst->depthFailOp = toVkStencilOp(src.depthFailOp);
    dst->compareOp = toVkCompareOp(src.compareOp);
}

QRhiGraphicsPipeline *QRhi::createGraphicsPipeline()
{
    return new QVkGraphicsPipeline(this);
}

static inline VkDescriptorType toVkDescriptorType(QRhiShaderResourceBindings::Binding::Type type)
{
    switch (type) {
    case QRhiShaderResourceBindings::Binding::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case QRhiShaderResourceBindings::Binding::SampledTexture:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    default:
        Q_UNREACHABLE();
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

static inline VkShaderStageFlags toVkShaderStageFlags(QRhiShaderResourceBindings::Binding::StageFlags stage)
{
    int s = 0;
    if (stage.testFlag(QRhiShaderResourceBindings::Binding::VertexStage))
        s |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stage.testFlag(QRhiShaderResourceBindings::Binding::FragmentStage))
        s |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stage.testFlag(QRhiShaderResourceBindings::Binding::GeometryStage))
        s |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (stage.testFlag(QRhiShaderResourceBindings::Binding::TessellationControlStage))
        s |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stage.testFlag(QRhiShaderResourceBindings::Binding::TessellationEvaluationStage))
        s |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return VkShaderStageFlags(s);
}

QRhiShaderResourceBindings *QRhi::createShaderResourceBindings()
{
    return new QVkShaderResourceBindings(this);
}

void QRhiVulkan::updateShaderResourceBindings(QRhiShaderResourceBindings *srb, int descSetIdx)
{
    QVkShaderResourceBindingsPrivate *srbD = RES_GET_D(QVkShaderResourceBindings, srb);

    QVarLengthArray<VkDescriptorBufferInfo, 4> bufferInfos;
    QVarLengthArray<VkDescriptorImageInfo, 4> imageInfos;
    QVarLengthArray<VkWriteDescriptorSet, 8> writeInfos;

    const bool updateAll = descSetIdx < 0;
    int frameSlot = updateAll ? 0 : descSetIdx;
    while (frameSlot < (updateAll ? QVK_FRAMES_IN_FLIGHT : descSetIdx + 1)) {
        srbD->boundResourceData[frameSlot].resize(srb->bindings.count());
        for (int i = 0, ie = srb->bindings.count(); i != ie; ++i) {
            const QRhiShaderResourceBindings::Binding &b(srb->bindings[i]);
            QVkShaderResourceBindingsPrivate::BoundResourceData &bd(srbD->boundResourceData[frameSlot][i]);

            VkWriteDescriptorSet writeInfo;
            memset(&writeInfo, 0, sizeof(writeInfo));
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.dstSet = srbD->descSets[frameSlot];
            writeInfo.dstBinding = b.binding;
            writeInfo.descriptorCount = 1;

            switch (b.type) {
            case QRhiShaderResourceBindings::Binding::UniformBuffer:
            {
                writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                QRhiBuffer *buf = b.ubuf.buf;
                QVkBufferPrivate *bufD = RES_GET_D(QVkBuffer, buf);
                bd.ubuf.generation = bufD->generation;
                VkDescriptorBufferInfo bufInfo;
                bufInfo.buffer = buf->isStatic() ? bufD->buffers[0] : bufD->buffers[frameSlot];
                bufInfo.offset = b.ubuf.offset;
                bufInfo.range = b.ubuf.size <= 0 ? buf->size : b.ubuf.size;
                // be nice and assert when we know the vulkan device would die a horrible death due to non-aligned reads
                Q_ASSERT(aligned(bufInfo.offset, ubufAlign) == bufInfo.offset);
                bufferInfos.append(bufInfo);
                writeInfo.pBufferInfo = &bufferInfos.last();
            }
                break;
            case QRhiShaderResourceBindings::Binding::SampledTexture:
            {
                writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bd.stex.texGeneration = RES_GET_D(QVkTexture, b.stex.tex)->generation;
                bd.stex.samplerGeneration = RES_GET_D(QVkSampler, b.stex.sampler)->generation;
                VkDescriptorImageInfo imageInfo;
                imageInfo.sampler = RES_GET_D(QVkSampler, b.stex.sampler)->sampler;
                imageInfo.imageView = RES_GET_D(QVkTexture, b.stex.tex)->imageView;
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
        ++frameSlot;
    }

    df->vkUpdateDescriptorSets(dev, writeInfos.count(), writeInfos.constData(), 0, nullptr);
}

void QRhiVulkan::bufferBarrier(QRhiCommandBuffer *cb, QRhiBuffer *buf)
{
    VkBufferMemoryBarrier bufMemBarrier;
    memset(&bufMemBarrier, 0, sizeof(bufMemBarrier));
    bufMemBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    int dstAccess = 0;
    VkPipelineStageFlagBits dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

    if (buf->usage.testFlag(QRhiBuffer::VertexBuffer))
        dstAccess |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    if (buf->usage.testFlag(QRhiBuffer::IndexBuffer))
        dstAccess |= VK_ACCESS_INDEX_READ_BIT;
    if (buf->usage.testFlag(QRhiBuffer::UniformBuffer)) {
        dstAccess |= VK_ACCESS_UNIFORM_READ_BIT;
        dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; // don't know where it's used, assume vertex to be safe
    }

    bufMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufMemBarrier.dstAccessMask = dstAccess;
    bufMemBarrier.buffer = RES_GET_D(QVkBuffer, buf)->buffers[0];
    bufMemBarrier.size = buf->size;

    df->vkCmdPipelineBarrier(cb->cb, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage,
                             0, 0, nullptr, 1, &bufMemBarrier, 0, nullptr);
}

void QRhiVulkan::imageBarrier(QRhiCommandBuffer *cb, QRhiTexture *tex,
                               VkImageLayout newLayout,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;

    QVkTexturePrivate *texD = RES_GET_D(QVkTexture, tex);
    barrier.oldLayout = texD->layout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.image = texD->image;

    df->vkCmdPipelineBarrier(cb->cb,
                             srcStage,
                             dstStage,
                             0, 0, nullptr, 0, nullptr,
                             1, &barrier);

    texD->layout = newLayout;
}

void QRhiVulkan::applyPassUpdates(QRhiCommandBuffer *cb, const QRhi::PassUpdates &updates)
{
    struct ChangeRange {
        int changeBegin = -1;
        int changeEnd = -1;
    };
    QHash<QRhiBuffer *, ChangeRange> changeRanges;
    for (const QRhi::DynamicBufferUpdate &u : updates.dynamicBufferUpdates) {
        Q_ASSERT(!u.buf->isStatic());
        void *p = nullptr;
        VmaAllocation a = toVmaAllocation(RES_GET_D(QVkBuffer, u.buf)->allocations[currentFrameSlot]);
        VkResult err = vmaMapMemory(allocator, a, &p);
        if (err != VK_SUCCESS) {
            qWarning("Failed to map buffer: %d", err);
            continue;
        }
        memcpy(static_cast<char *>(p) + u.offset, u.data.constData(), u.data.size());
        vmaUnmapMemory(allocator, a);
        ChangeRange &r(changeRanges[u.buf]);
        if (r.changeBegin == -1 || u.offset < r.changeBegin)
            r.changeBegin = u.offset;
        if (r.changeEnd == -1 || u.offset + u.data.size() > r.changeEnd)
            r.changeEnd = u.offset + u.data.size();
    }
    for (auto it = changeRanges.cbegin(), itEnd = changeRanges.cend(); it != itEnd; ++it) {
        VmaAllocation a = toVmaAllocation(RES_GET_D(QVkBuffer, it.key())->allocations[currentFrameSlot]);
        vmaFlushAllocation(allocator, a, it->changeBegin, it->changeEnd - it->changeBegin);
    }

    for (const QRhi::StaticBufferUpload &u : updates.staticBufferUploads) {
        QVkBufferPrivate *ubufD = RES_GET_D(QVkBuffer, u.buf);
        Q_ASSERT(u.buf->isStatic());
        Q_ASSERT(ubufD->stagingBuffer);
        Q_ASSERT(u.data.size() == u.buf->size);

        void *p = nullptr;
        VmaAllocation a = toVmaAllocation(ubufD->stagingAlloc);
        VkResult err = vmaMapMemory(allocator, a, &p);
        if (err != VK_SUCCESS) {
            qWarning("Failed to map buffer: %d", err);
            continue;
        }
        memcpy(p, u.data.constData(), u.buf->size);
        vmaUnmapMemory(allocator, a);
        vmaFlushAllocation(allocator, a, 0, u.buf->size);

        VkBufferCopy copyInfo;
        memset(&copyInfo, 0, sizeof(copyInfo));
        copyInfo.size = u.buf->size;

        df->vkCmdCopyBuffer(cb->cb, ubufD->stagingBuffer, ubufD->buffers[0], 1, &copyInfo);
        bufferBarrier(cb, u.buf);
        ubufD->lastActiveFrameSlot = currentFrameSlot;
    }

    for (const QRhi::TextureUpload &u : updates.textureUploads) {
        const qsizetype imageSize = u.image.sizeInBytes();
        if (imageSize < 1) {
            qWarning("Not uploading empty image");
            continue;
        }
        if (u.image.size() != u.tex->pixelSize) {
            qWarning("Attempted to upload data of size %dx%d to texture of size %dx%d",
                     u.image.width(), u.image.height(), u.tex->pixelSize.width(), u.tex->pixelSize.height());
            continue;
        }

        QVkTexturePrivate *utexD = RES_GET_D(QVkTexture, u.tex);
        if (!utexD->stagingBuffer) {
            VkBufferCreateInfo bufferInfo;
            memset(&bufferInfo, 0, sizeof(bufferInfo));
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = imageSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo allocInfo;
            memset(&allocInfo, 0, sizeof(allocInfo));
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            VmaAllocation allocation;
            VkResult err = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &utexD->stagingBuffer, &allocation, nullptr);
            if (err != VK_SUCCESS) {
                qWarning("Failed to create image staging buffer of size %d: %d", imageSize, err);
                continue;
            }
            utexD->stagingAlloc = allocation;
        }

        void *mp = nullptr;
        VmaAllocation a = toVmaAllocation(utexD->stagingAlloc);
        VkResult err = vmaMapMemory(allocator, a, &mp);
        if (err != VK_SUCCESS) {
            qWarning("Failed to map image data: %d", err);
            continue;
        }
        memcpy(mp, u.image.constBits(), imageSize);
        vmaUnmapMemory(allocator, a);
        vmaFlushAllocation(allocator, a, 0, imageSize);

        if (utexD->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            if (utexD->layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
                imageBarrier(cb, u.tex,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             0, VK_ACCESS_TRANSFER_WRITE_BIT,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            } else {
                imageBarrier(cb, u.tex,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            }
        }

        VkBufferImageCopy copyInfo;
        memset(&copyInfo, 0, sizeof(copyInfo));
        copyInfo.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyInfo.imageSubresource.layerCount = 1;
        copyInfo.imageExtent.width = u.image.width();
        copyInfo.imageExtent.height = u.image.height();
        copyInfo.imageExtent.depth = 1;

        df->vkCmdCopyBufferToImage(cb->cb, utexD->stagingBuffer, utexD->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfo);
        utexD->lastActiveFrameSlot = currentFrameSlot;

        imageBarrier(cb, u.tex,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
}

void QRhiVulkan::activateTextureRenderTarget(QRhiCommandBuffer *, QRhiTextureRenderTarget *rt)
{
    QVkTextureRenderTargetPrivate *rtD = RES_GET_D(QVkTextureRenderTarget, rt);
    rtD->lastActiveFrameSlot = currentFrameSlot;
    RES_GET_D(QVkRenderPass, rtD->rp)->lastActiveFrameSlot = currentFrameSlot;
    // the renderpass will implicitly transition so no barrier needed here
    RES_GET_D(QVkTexture, rt->texture)->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

void QRhiVulkan::deactivateTextureRenderTarget(QRhiCommandBuffer *, QRhiTextureRenderTarget *rt)
{
    // already in the right layout when the renderpass ends
    RES_GET_D(QVkTexture, rt->texture)->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void QRhi::beginPass(QRhiRenderTarget *rt, QRhiCommandBuffer *cb, const QRhiClearValue *clearValues, const PassUpdates &updates)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(!d->inPass);

    d->applyPassUpdates(cb, updates);

    QVkRenderTargetPrivate *rtD = RES_GET_D(QVkRenderTarget, rt);
    if (rtD->type == QVkRenderTargetPrivate::RtTexture)
        d->activateTextureRenderTarget(cb, static_cast<QRhiTextureRenderTarget *>(rt));

    cb->currentTarget = rt;

    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = RES_GET_D(QVkRenderPass, rtD->rp)->rp;
    rpBeginInfo.framebuffer = rtD->fb;
    rpBeginInfo.renderArea.extent.width = rtD->pixelSize.width();
    rpBeginInfo.renderArea.extent.height = rtD->pixelSize.height();
    rpBeginInfo.clearValueCount = rtD->attCount;
    QVarLengthArray<VkClearValue, 4> cvs;
    for (int i = 0; i < rtD->attCount; ++i) {
        VkClearValue cv;
        if (clearValues[i].isDepthStencil)
            cv.depthStencil = { clearValues[i].d, clearValues[i].s };
        else
            cv.color = { { clearValues[i].rgba.x(), clearValues[i].rgba.y(), clearValues[i].rgba.z(), clearValues[i].rgba.w() } };
        cvs.append(cv);
    }
    rpBeginInfo.pClearValues = cvs.constData();

    d->df->vkCmdBeginRenderPass(cb->cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    d->inPass = true;
}

void QRhi::endPass(QRhiCommandBuffer *cb)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);
    d->df->vkCmdEndRenderPass(cb->cb);
    d->inPass = false;

    if (RES_GET_D(QVkRenderTarget, cb->currentTarget)->type == QVkRenderTargetPrivate::RtTexture)
        d->deactivateTextureRenderTarget(cb, static_cast<QRhiTextureRenderTarget *>(cb->currentTarget));

    cb->currentTarget = nullptr;
}

void QRhi::setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);
    QVkGraphicsPipelinePrivate *psD = RES_GET_D(QVkGraphicsPipeline, ps);
    Q_ASSERT(psD->pipeline);

    if (!srb)
        srb = ps->shaderResourceBindings;

    bool hasDynamicBufferInSrb = false;
    for (const QRhiShaderResourceBindings::Binding &b : qAsConst(srb->bindings)) {
        switch (b.type) {
        case QRhiShaderResourceBindings::Binding::UniformBuffer:
            Q_ASSERT(b.ubuf.buf->usage.testFlag(QRhiBuffer::UniformBuffer));
            RES_GET_D(QVkBuffer, b.ubuf.buf)->lastActiveFrameSlot = d->currentFrameSlot;
            if (!b.ubuf.buf->isStatic())
                hasDynamicBufferInSrb = true;
            break;
        case QRhiShaderResourceBindings::Binding::SampledTexture:
            RES_GET_D(QVkTexture, b.stex.tex)->lastActiveFrameSlot = d->currentFrameSlot;
            RES_GET_D(QVkSampler, b.stex.sampler)->lastActiveFrameSlot = d->currentFrameSlot;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    // ensure the descriptor set we are going to bind refers to up-to-date Vk objects
    QVkShaderResourceBindingsPrivate *srbD = RES_GET_D(QVkShaderResourceBindings, srb);
    const int descSetIdx = hasDynamicBufferInSrb ? d->currentFrameSlot : 0;
    bool srbUpdate = false;
    for (int i = 0, ie = srb->bindings.count(); i != ie; ++i) {
        const QRhiShaderResourceBindings::Binding &b(srb->bindings[i]);
        QVkShaderResourceBindingsPrivate::BoundResourceData &bd(srbD->boundResourceData[descSetIdx][i]);
        switch (b.type) {
        case QRhiShaderResourceBindings::Binding::UniformBuffer:
            if (RES_GET_D(QVkBuffer, b.ubuf.buf)->generation != bd.ubuf.generation) {
                srbUpdate = true;
                bd.ubuf.generation = RES_GET_D(QVkBuffer, b.ubuf.buf)->generation;
            }
            break;
        case QRhiShaderResourceBindings::Binding::SampledTexture:
            if (RES_GET_D(QVkTexture, b.stex.tex)->generation != bd.stex.texGeneration
                    || RES_GET_D(QVkSampler, b.stex.sampler)->generation != bd.stex.samplerGeneration)
            {
                srbUpdate = true;
                bd.stex.texGeneration = RES_GET_D(QVkTexture, b.stex.tex)->generation;
                bd.stex.samplerGeneration = RES_GET_D(QVkSampler, b.stex.sampler)->generation;
            }
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
    if (srbUpdate)
        d->updateShaderResourceBindings(srb, descSetIdx);

    if (cb->currentPipeline != ps) {
        psD->lastActiveFrameSlot = d->currentFrameSlot;
        d->df->vkCmdBindPipeline(cb->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, psD->pipeline);
        cb->currentPipeline = ps;
    }

    if (hasDynamicBufferInSrb || srbUpdate || cb->currentSrb != srb) {
        srbD->lastActiveFrameSlot = d->currentFrameSlot;
        d->df->vkCmdBindDescriptorSets(cb->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, psD->layout, 0, 1,
                                       &srbD->descSets[descSetIdx], 0, nullptr);
        cb->currentSrb = srb;
    }
}

void QRhi::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<VertexInput> &bindings,
                          QRhiBuffer *indexBuf, quint32 indexOffset, IndexFormat indexFormat)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);

    QVarLengthArray<VkBuffer, 4> bufs;
    QVarLengthArray<VkDeviceSize, 4> ofs;
    for (int i = 0, ie = bindings.count(); i != ie; ++i) {
        QRhiBuffer *buf = bindings[i].first;
        QVkBufferPrivate *bufD = RES_GET_D(QVkBuffer, buf);
        Q_ASSERT(buf->usage.testFlag(QRhiBuffer::VertexBuffer));
        bufD->lastActiveFrameSlot = d->currentFrameSlot;
        const int idx = buf->isStatic() ? 0 : d->currentFrameSlot;
        bufs.append(bufD->buffers[idx]);
        ofs.append(bindings[i].second);
    }
    if (!bufs.isEmpty())
        d->df->vkCmdBindVertexBuffers(cb->cb, startBinding, bufs.count(), bufs.constData(), ofs.constData());

    if (indexBuf) {
        QVkBufferPrivate *bufD = RES_GET_D(QVkBuffer, indexBuf);
        Q_ASSERT(indexBuf->usage.testFlag(QRhiBuffer::IndexBuffer));
        bufD->lastActiveFrameSlot = d->currentFrameSlot;
        const int idx = indexBuf->isStatic() ? 0 : d->currentFrameSlot;
        const VkIndexType type = indexFormat == IndexUInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        d->df->vkCmdBindIndexBuffer(cb->cb, bufD->buffers[idx], indexOffset, type);
    }
}

static inline VkViewport toVkViewport(const QRhiViewport &viewport)
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

void QRhi::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);
    VkViewport vp = toVkViewport(viewport);
    d->df->vkCmdSetViewport(cb->cb, 0, 1, &vp);
}

static inline VkRect2D toVkScissor(const QRhiScissor &scissor)
{
    VkRect2D s;
    s.offset.x = scissor.r.x();
    s.offset.y = scissor.r.y();
    s.extent.width = scissor.r.width();
    s.extent.height = scissor.r.height();
    return s;
}

void QRhi::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);
    VkRect2D s = toVkScissor(scissor);
    d->df->vkCmdSetScissor(cb->cb, 0, 1, &s);
}

void QRhi::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);
    const float bc[4] = { c.x(), c.y(), c.z(), c.w() };
    d->df->vkCmdSetBlendConstants(cb->cb, bc);
}

void QRhi::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);
    d->df->vkCmdSetStencilReference(cb->cb, VK_STENCIL_FRONT_AND_BACK, refValue);
}

void QRhi::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);
    d->df->vkCmdDraw(cb->cb, vertexCount, instanceCount, firstVertex, firstInstance);
}

void QRhi::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                       quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
    RHI_D(QRhiVulkan);
    Q_ASSERT(d->inPass);
    d->df->vkCmdDrawIndexed(cb->cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void QRhiVulkan::prepareNewFrame(QRhiCommandBuffer *cb)
{
    Q_ASSERT(!inFrame);
    inFrame = true;

    executeDeferredReleases();

    cb->resetState();
}

void QRhiVulkan::finishFrame()
{
    Q_ASSERT(inFrame);
    inFrame = false;
    ++finishedFrameCount;
}

void QRhiVulkan::executeDeferredReleases(bool forced)
{
    for (int i = releaseQueue.count() - 1; i >= 0; --i) {
        const QRhiVulkan::DeferredReleaseEntry &e(releaseQueue[i]);
        if (forced || currentFrameSlot == e.lastActiveFrameSlot || e.lastActiveFrameSlot < 0) {
            switch (e.type) {
            case QRhiVulkan::DeferredReleaseEntry::Pipeline:
                df->vkDestroyPipeline(dev, e.pipelineState.pipeline, nullptr);
                df->vkDestroyPipelineLayout(dev, e.pipelineState.layout, nullptr);
                break;
            case QRhiVulkan::DeferredReleaseEntry::ShaderResourceBindings:
                df->vkDestroyDescriptorSetLayout(dev, e.shaderResourceBindings.layout, nullptr);
                if (e.shaderResourceBindings.poolIndex >= 0) {
                    descriptorPools[e.shaderResourceBindings.poolIndex].refCount -= 1;
                    Q_ASSERT(descriptorPools[e.shaderResourceBindings.poolIndex].refCount >= 0);
                }
                break;
            case QRhiVulkan::DeferredReleaseEntry::Buffer:
                for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
                    vmaDestroyBuffer(allocator, e.buffer.buffers[i], toVmaAllocation(e.buffer.allocations[i]));
                vmaDestroyBuffer(allocator, e.buffer.stagingBuffer, toVmaAllocation(e.buffer.stagingAlloc));
                break;
            case QRhiVulkan::DeferredReleaseEntry::RenderBuffer:
                df->vkDestroyImageView(dev, e.renderBuffer.imageView, nullptr);
                df->vkDestroyImage(dev, e.renderBuffer.image, nullptr);
                df->vkFreeMemory(dev, e.renderBuffer.memory, nullptr);
                break;
            case QRhiVulkan::DeferredReleaseEntry::Texture:
                df->vkDestroyImageView(dev, e.texture.imageView, nullptr);
                vmaDestroyImage(allocator, e.texture.image, toVmaAllocation(e.texture.allocation));
                vmaDestroyBuffer(allocator, e.texture.stagingBuffer, toVmaAllocation(e.texture.stagingAlloc));
                break;
            case QRhiVulkan::DeferredReleaseEntry::Sampler:
                df->vkDestroySampler(dev, e.sampler.sampler, nullptr);
                break;
            case QRhiVulkan::DeferredReleaseEntry::TextureRenderTarget:
                df->vkDestroyFramebuffer(dev, e.textureRenderTarget.fb, nullptr);
                break;
            case QRhiVulkan::DeferredReleaseEntry::RenderPass:
                df->vkDestroyRenderPass(dev, e.renderPass.rp, nullptr);
                break;
            default:
                break;
            }
            releaseQueue.removeAt(i);
        }
    }
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

QVector<int> QRhi::supportedSampleCounts() const
{
    RHI_D(QRhiVulkan);
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

VkSampleCountFlagBits QRhiVulkan::effectiveSampleCount(int sampleCount)
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

QMatrix4x4 QRhi::openGLCorrectionMatrix() const
{
    RHI_D(QRhiVulkan);
    if (d->clipCorrectMatrix.isIdentity()) {
        // NB the ctor takes row-major
        d->clipCorrectMatrix = QMatrix4x4(1.0f, 0.0f, 0.0f, 0.0f,
                                          0.0f, -1.0f, 0.0f, 0.0f,
                                          0.0f, 0.0f, 0.5f, 0.5f,
                                          0.0f, 0.0f, 0.0f, 1.0f);
    }
    return d->clipCorrectMatrix;
}

QRhiResourcePrivate::~QRhiResourcePrivate()
{
}

QRhiResource::QRhiResource(QRhi *rhi, QRhiResourcePrivate *d)
    : d_ptr(d)
{
    d_ptr->rhi = rhi;
}

QRhiResource::~QRhiResource()
{
    delete d_ptr;
}

QRhiBuffer::QRhiBuffer(QRhi *rhi, QRhiResourcePrivate *d,
                       Type type_, UsageFlags usage_, int size_)
    : QRhiResource(rhi, d),
      type(type_), usage(usage_), size(size_)
{
}

QRhiRenderBuffer::QRhiRenderBuffer(QRhi *rhi, QRhiResourcePrivate *d,
                                   Type type_, const QSize &pixelSize_, int sampleCount_)
    : QRhiResource(rhi, d),
      type(type_), pixelSize(pixelSize_), sampleCount(sampleCount_)
{
}

QRhiTexture::QRhiTexture(QRhi *rhi, QRhiResourcePrivate *d, Format format_, const QSize &pixelSize_, Flags flags_)
    : QRhiResource(rhi, d),
      format(format_), pixelSize(pixelSize_), flags(flags_)
{
}

QRhiSampler::QRhiSampler(QRhi *rhi, QRhiResourcePrivate *d,
                         Filter magFilter_, Filter minFilter_, Filter mipmapMode_, AddressMode u_, AddressMode v_)
    : QRhiResource(rhi, d),
      magFilter(magFilter_), minFilter(minFilter_), mipmapMode(mipmapMode_),
      addressU(u_), addressV(v_)
{
}

QRhiShaderResourceBindings::QRhiShaderResourceBindings(QRhi *rhi, QRhiResourcePrivate *d)
    : QRhiResource(rhi, d)
{
}

QRhiGraphicsPipeline::QRhiGraphicsPipeline(QRhi *rhi, QRhiResourcePrivate *d)
    : QRhiResource(rhi, d)
{
}

QRhiSwapChain::QRhiSwapChain(QRhi *rhi, QRhiResourcePrivate *d)
    : QRhiResource(rhi, d)
{
}

QVkBuffer::QVkBuffer(QRhi *rhi, Type type, UsageFlags usage, int size)
    : QRhiBuffer(rhi, new QVkBufferPrivate, type, usage, size)
{
    RES_D(QVkBuffer);
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        d->buffers[i] = VK_NULL_HANDLE;
        d->allocations[i] = nullptr;
    }
}

void QVkBuffer::release()
{
    RES_D(QVkBuffer);
    int nullBufferCount = 0;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        if (!d->buffers[i])
            ++nullBufferCount;
    }
    if (nullBufferCount == QVK_FRAMES_IN_FLIGHT)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::Buffer;
    e.lastActiveFrameSlot = d->lastActiveFrameSlot;

    e.buffer.stagingBuffer = d->stagingBuffer;
    e.buffer.stagingAlloc = d->stagingAlloc;

    d->stagingBuffer = VK_NULL_HANDLE;
    d->stagingAlloc = nullptr;

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        e.buffer.buffers[i] = d->buffers[i];
        e.buffer.allocations[i] = d->allocations[i];

        d->buffers[i] = VK_NULL_HANDLE;
        d->allocations[i] = nullptr;
    }

    RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkBuffer::build()
{
    RES_D(QVkBuffer);
    if (d->buffers[0])
        release();

    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = toVkBufferUsage(usage);

    VmaAllocationCreateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));

    if (isStatic()) {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    } else {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    }

    RES_RHI(QRhiVulkan);
    VkResult err = VK_SUCCESS;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        d->buffers[i] = VK_NULL_HANDLE;
        d->allocations[i] = nullptr;
        if (i == 0 || !isStatic()) {
            VmaAllocation allocation;
            err = vmaCreateBuffer(rhiD->allocator, &bufferInfo, &allocInfo, &d->buffers[i], &allocation, nullptr);
            if (err != VK_SUCCESS)
                break;
            d->allocations[i] = allocation;
        }
    }

    if (err == VK_SUCCESS && isStatic()) {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocation allocation;
        err = vmaCreateBuffer(rhiD->allocator, &bufferInfo, &allocInfo, &d->stagingBuffer, &allocation, nullptr);
        if (err == VK_SUCCESS)
            d->stagingAlloc = allocation;
    }

    if (err == VK_SUCCESS) {
        d->lastActiveFrameSlot = -1;
        d->generation += 1;
        return true;
    } else {
        qWarning("Failed to create buffer: %d", err);
        return false;
    }
}

QVkRenderBuffer::QVkRenderBuffer(QRhi *rhi, Type type, const QSize &pixelSize, int sampleCount)
    : QRhiRenderBuffer(rhi, new QVkRenderBufferPrivate, type, pixelSize, sampleCount)
{
}

void QVkRenderBuffer::release()
{
    RES_D(QVkRenderBuffer);
    if (!d->memory)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::RenderBuffer;
    e.lastActiveFrameSlot = d->lastActiveFrameSlot;

    e.renderBuffer.memory = d->memory;
    e.renderBuffer.image = d->image;
    e.renderBuffer.imageView = d->imageView;

    d->memory = VK_NULL_HANDLE;
    d->image = VK_NULL_HANDLE;
    d->imageView = VK_NULL_HANDLE;

    RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkRenderBuffer::build()
{
    RES_D(QVkRenderBuffer);
    if (d->memory)
        release();

    RES_RHI(QRhiVulkan);
    switch (type) {
    case QRhiRenderBuffer::DepthStencil:
        if (!rhiD->createTransientImage(rhiD->optimalDepthStencilFormat(),
                                        pixelSize,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                        rhiD->effectiveSampleCount(sampleCount),
                                        &d->memory,
                                        &d->image,
                                        &d->imageView,
                                        1))
        {
            return false;
        }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    d->lastActiveFrameSlot = -1;
    return true;
}

QVkTexture::QVkTexture(QRhi *rhi, Format format, const QSize &pixelSize, Flags flags)
    : QRhiTexture(rhi, new QVkTexturePrivate, format, pixelSize, flags)
{
}

void QVkTexture::release()
{
    RES_D(QVkTexture);
    if (!d->image)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::Texture;
    e.lastActiveFrameSlot = d->lastActiveFrameSlot;

    e.texture.image = d->image;
    e.texture.imageView = d->imageView;
    e.texture.allocation = d->allocation;
    e.texture.stagingBuffer = d->stagingBuffer;
    e.texture.stagingAlloc = d->stagingAlloc;

    d->image = VK_NULL_HANDLE;
    d->imageView = VK_NULL_HANDLE;
    d->allocation = nullptr;
    d->stagingBuffer = VK_NULL_HANDLE;
    d->stagingAlloc = nullptr;

    RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkTexture::build()
{
    RES_D(QVkTexture);
    if (d->image)
        release();

    RES_RHI(QRhiVulkan);
    VkFormat vkformat = toVkTextureFormat(format);
    VkFormatProperties props;
    rhiD->f->vkGetPhysicalDeviceFormatProperties(rhiD->physDev, vkformat, &props);
    const bool canSampleOptimal = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    if (!canSampleOptimal) {
        qWarning("Texture sampling not supported?!");
        return false;
    }

    const QSize size = safeSize(pixelSize);
    const bool isDepthStencil = isDepthStencilTextureFormat(format);

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
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (flags.testFlag(QRhiTexture::RenderTarget)) {
        if (isDepthStencil)
            imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    VmaAllocationCreateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VmaAllocation allocation;
    VkResult err = vmaCreateImage(rhiD->allocator, &imageInfo, &allocInfo, &d->image, &allocation, nullptr);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create image: %d", err);
        return false;
    }
    d->allocation = allocation;

    VkImageViewCreateInfo viewInfo;
    memset(&viewInfo, 0, sizeof(viewInfo));
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = d->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkformat;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    viewInfo.subresourceRange.aspectMask = isDepthStencil ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                                                          : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = viewInfo.subresourceRange.layerCount = 1;

    err = rhiD->df->vkCreateImageView(rhiD->dev, &viewInfo, nullptr, &d->imageView);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create image view: %d", err);
        return false;
    }

    d->layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    d->lastActiveFrameSlot = -1;
    d->generation += 1;
    return true;
}

QVkSampler::QVkSampler(QRhi *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode, AddressMode u, AddressMode v)
    : QRhiSampler(rhi, new QVkSamplerPrivate, magFilter, minFilter, mipmapMode, u, v)
{
}

void QVkSampler::release()
{
    RES_D(QVkSampler);
    if (!d->sampler)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::Sampler;
    e.lastActiveFrameSlot = d->lastActiveFrameSlot;

    e.sampler.sampler = d->sampler;
    d->sampler = VK_NULL_HANDLE;

    RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkSampler::build()
{
    RES_D(QVkSampler);
    if (d->sampler)
        release();

    VkSamplerCreateInfo samplerInfo;
    memset(&samplerInfo, 0, sizeof(samplerInfo));
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = toVkFilter(magFilter);
    samplerInfo.minFilter = toVkFilter(minFilter);
    samplerInfo.mipmapMode = toVkMipmapMode(mipmapMode);
    samplerInfo.addressModeU = toVkAddressMode(addressU);
    samplerInfo.addressModeV = toVkAddressMode(addressV);
    samplerInfo.maxAnisotropy = 1.0f;

    RES_RHI(QRhiVulkan);
    VkResult err = rhiD->df->vkCreateSampler(rhiD->dev, &samplerInfo, nullptr, &d->sampler);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create sampler: %d", err);
        return false;
    }

    d->lastActiveFrameSlot = -1;
    d->generation += 1;
    return true;
}

QVkRenderPass::QVkRenderPass(QRhi *rhi)
    : QRhiRenderPass(rhi, new QVkRenderPassPrivate)
{
}

void QVkRenderPass::release()
{
    RES_D(QVkRenderPass);
    if (!d->rp)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::RenderPass;
    e.lastActiveFrameSlot = d->lastActiveFrameSlot;

    e.renderPass.rp = d->rp;

    d->rp = VK_NULL_HANDLE;

    RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

QVkRenderTarget::QVkRenderTarget(QRhi *rhi)
    : QRhiRenderTarget(rhi, new QVkRenderTargetPrivate)
{
}

QVkRenderTarget::QVkRenderTarget(QRhi *rhi, QVkRenderTargetPrivate *d)
    : QRhiRenderTarget(rhi, d)
{
}

void QVkRenderTarget::release()
{
    RES_D(QVkRenderTarget);
    Q_ASSERT(d->type == QVkRenderTargetPrivate::RtRef);
    // nothing to do here
}

QSize QVkRenderTarget::sizeInPixels() const
{
    RES_D(const QVkRenderTarget);
    return d->pixelSize;
}

const QRhiRenderPass *QVkRenderTarget::renderPass() const
{
    RES_D(const QVkRenderTarget);
    return d->rp;
}

QVkTextureRenderTarget::QVkTextureRenderTarget(QRhi *rhi)
    : QRhiTextureRenderTarget(rhi, new QVkTextureRenderTargetPrivate)
{
}

void QVkTextureRenderTarget::release()
{
    RES_D(QVkTextureRenderTarget);
    Q_ASSERT(d->type == QVkRenderTargetPrivate::RtTexture);
    if (!d->fb)
        return;

    d->rp->release();

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::TextureRenderTarget;
    e.lastActiveFrameSlot = d->lastActiveFrameSlot;

    e.textureRenderTarget.fb = d->fb;

    d->fb = VK_NULL_HANDLE;

    RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkTextureRenderTarget::build()
{
    RES_D(QVkTextureRenderTarget);
    if (d->fb)
        release();

    Q_ASSERT(texture);
    Q_ASSERT(!depthStencilBuffer || !depthTexture);
    const bool hasDepthStencil = depthStencilBuffer || depthTexture;
    const bool preserved = flags.testFlag(QRhiTextureRenderTarget::PreserveColorContents);

    VkAttachmentDescription attDesc[2];
    memset(attDesc, 0, sizeof(attDesc));

    // ### what about depth-only passes?

    attDesc[0].format = toVkTextureFormat(texture->format);
    attDesc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc[0].loadOp = preserved ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[0].initialLayout = preserved ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    RES_RHI(QRhiVulkan);
    if (hasDepthStencil) {
        attDesc[1].format = depthTexture ? toVkTextureFormat(depthTexture->format) : rhiD->optimalDepthStencilFormat();
        attDesc[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attDesc[1].storeOp = depthTexture ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attDesc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attDesc[1].stencilStoreOp = depthTexture ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attDesc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attDesc[1].finalLayout = depthTexture ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
    }

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
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

    VkResult err = rhiD->df->vkCreateRenderPass(rhiD->dev, &rpInfo, nullptr, &RES_GET_D(QVkRenderPass, d->rp)->rp);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create renderpass: %d", err);
        return false;
    }

    const VkImageView views[] = {
        RES_GET_D(QVkTexture, texture)->imageView,
        hasDepthStencil ? (depthTexture ? RES_GET_D(QVkTexture, depthTexture)->imageView
                : RES_GET_D(QVkRenderBuffer, depthStencilBuffer)->imageView)
            : VK_NULL_HANDLE
    };
    const int attCount = hasDepthStencil ? 2 : 1;

    VkFramebufferCreateInfo fbInfo;
    memset(&fbInfo, 0, sizeof(fbInfo));
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = RES_GET_D(QVkRenderPass, d->rp)->rp;
    fbInfo.attachmentCount = attCount;
    fbInfo.pAttachments = views;
    fbInfo.width = texture->pixelSize.width();
    fbInfo.height = texture->pixelSize.height();
    fbInfo.layers = 1;

    err = rhiD->df->vkCreateFramebuffer(rhiD->dev, &fbInfo, nullptr, &d->fb);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create framebuffer: %d", err);
        return false;
    }

    d->pixelSize = texture->pixelSize;
    d->attCount = attCount;

    d->lastActiveFrameSlot = -1;
    return true;
}

QSize QVkTextureRenderTarget::sizeInPixels() const
{
    RES_D(const QVkTextureRenderTarget);
    return d->pixelSize;
}

const QRhiRenderPass *QVkTextureRenderTarget::renderPass() const
{
    RES_D(const QVkTextureRenderTarget);
    return d->rp;
}

QVkShaderResourceBindings::QVkShaderResourceBindings(QRhi *rhi)
    : QRhiShaderResourceBindings(rhi, new QVkShaderResourceBindingsPrivate)
{
}

void QVkShaderResourceBindings::release()
{
    RES_D(QVkShaderResourceBindings);
    if (!d->layout)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::ShaderResourceBindings;
    e.lastActiveFrameSlot = d->lastActiveFrameSlot;

    e.shaderResourceBindings.poolIndex = d->poolIndex;
    e.shaderResourceBindings.layout = d->layout;

    d->poolIndex = -1;
    d->layout = VK_NULL_HANDLE;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        d->descSets[i] = VK_NULL_HANDLE;

    RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkShaderResourceBindings::build()
{
    RES_D(QVkShaderResourceBindings);
    if (d->layout)
        release();

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        d->descSets[i] = VK_NULL_HANDLE;

    QVarLengthArray<VkDescriptorSetLayoutBinding, 4> vkbindings;
    for (const QRhiShaderResourceBindings::Binding &b : qAsConst(bindings)) {
        VkDescriptorSetLayoutBinding binding;
        memset(&binding, 0, sizeof(binding));
        binding.binding = b.binding;
        binding.descriptorType = toVkDescriptorType(b.type);
        binding.descriptorCount = 1; // no array support yet
        binding.stageFlags = toVkShaderStageFlags(b.stage);
        vkbindings.append(binding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    memset(&layoutInfo, 0, sizeof(layoutInfo));
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = uint32_t(vkbindings.count());
    layoutInfo.pBindings = vkbindings.constData();

    RES_RHI(QRhiVulkan);
    VkResult err = rhiD->df->vkCreateDescriptorSetLayout(rhiD->dev, &layoutInfo, nullptr, &d->layout);
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
        layouts[i] = d->layout;
    allocInfo.pSetLayouts = layouts;
    if (!rhiD->allocateDescriptorSet(&allocInfo, d->descSets, &d->poolIndex))
        return false;

    rhiD->updateShaderResourceBindings(this);

    d->lastActiveFrameSlot = -1;
    return true;
}

QVkGraphicsPipeline::QVkGraphicsPipeline(QRhi *rhi)
    : QRhiGraphicsPipeline(rhi, new QVkGraphicsPipelinePrivate)
{
}

void QVkGraphicsPipeline::release()
{
    RES_D(QVkGraphicsPipeline);
    if (!d->pipeline && !d->layout)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::Pipeline;
    e.lastActiveFrameSlot = d->lastActiveFrameSlot;

    e.pipelineState.pipeline = d->pipeline;
    e.pipelineState.layout = d->layout;

    d->pipeline = VK_NULL_HANDLE;
    d->layout = VK_NULL_HANDLE;

    RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkGraphicsPipeline::build()
{
    RES_D(QVkGraphicsPipeline);
    if (d->pipeline)
        release();

    RES_RHI(QRhiVulkan);
    if (!rhiD->ensurePipelineCache())
        return false;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    QVkShaderResourceBindingsPrivate *srbD = RES_GET_D(QVkShaderResourceBindings, shaderResourceBindings);
    Q_ASSERT(shaderResourceBindings && srbD->layout);
    pipelineLayoutInfo.pSetLayouts = &srbD->layout;
    VkResult err = rhiD->df->vkCreatePipelineLayout(rhiD->dev, &pipelineLayoutInfo, nullptr, &d->layout);
    if (err != VK_SUCCESS)
        qWarning("Failed to create pipeline layout: %d", err);

    VkGraphicsPipelineCreateInfo pipelineInfo;
    memset(&pipelineInfo, 0, sizeof(pipelineInfo));
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    QVarLengthArray<VkShaderModule, 4> shaders;
    QVarLengthArray<VkPipelineShaderStageCreateInfo, 4> shaderStageCreateInfos;
    for (const QRhiGraphicsShaderStage &shaderStage : shaderStages) {
        VkShaderModule shader = rhiD->createShader(shaderStage.shader);
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
    for (int i = 0, ie = vertexInputLayout.bindings.count(); i != ie; ++i) {
        const QRhiVertexInputLayout::Binding &binding(vertexInputLayout.bindings[i]);
        VkVertexInputBindingDescription bindingInfo = {
            uint32_t(i),
            binding.stride,
            binding.classification == QRhiVertexInputLayout::Binding::PerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE
        };
        vertexBindings.append(bindingInfo);
    }
    QVarLengthArray<VkVertexInputAttributeDescription, 4> vertexAttributes;
    for (const QRhiVertexInputLayout::Attribute &attribute : vertexInputLayout.attributes) {
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
    if (flags.testFlag(QRhiGraphicsPipeline::UsesBlendConstants))
        dynEnable << VK_DYNAMIC_STATE_BLEND_CONSTANTS;
    if (flags.testFlag(QRhiGraphicsPipeline::UsesStencilRef))
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
    inputAsmInfo.topology = toVkTopology(topology);
    pipelineInfo.pInputAssemblyState = &inputAsmInfo;

    VkPipelineRasterizationStateCreateInfo rastInfo;
    memset(&rastInfo, 0, sizeof(rastInfo));
    rastInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastInfo.rasterizerDiscardEnable = rasterizerDiscard;
    rastInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rastInfo.cullMode = toVkCullMode(cullMode);
    rastInfo.frontFace = toVkFrontFace(frontFace);
    rastInfo.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rastInfo;

    VkPipelineMultisampleStateCreateInfo msInfo;
    memset(&msInfo, 0, sizeof(msInfo));
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = rhiD->effectiveSampleCount(sampleCount);
    pipelineInfo.pMultisampleState = &msInfo;

    VkPipelineDepthStencilStateCreateInfo dsInfo;
    memset(&dsInfo, 0, sizeof(dsInfo));
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.depthTestEnable = depthTest;
    dsInfo.depthWriteEnable = depthWrite;
    dsInfo.depthCompareOp = toVkCompareOp(depthOp);
    dsInfo.stencilTestEnable = stencilTest;
    fillVkStencilOpState(&dsInfo.front, stencilFront);
    dsInfo.front.compareMask = stencilReadMask;
    dsInfo.front.writeMask = stencilWriteMask;
    fillVkStencilOpState(&dsInfo.back, stencilBack);
    dsInfo.back.compareMask = stencilReadMask;
    dsInfo.back.writeMask = stencilWriteMask;
    pipelineInfo.pDepthStencilState = &dsInfo;

    VkPipelineColorBlendStateCreateInfo blendInfo;
    memset(&blendInfo, 0, sizeof(blendInfo));
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    QVarLengthArray<VkPipelineColorBlendAttachmentState, 4> vktargetBlends;
    for (const QRhiGraphicsPipeline::TargetBlend &b : qAsConst(targetBlends)) {
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
        vktargetBlends.append(blend);
    }
    blendInfo.attachmentCount = vktargetBlends.count();
    blendInfo.pAttachments = vktargetBlends.constData();
    pipelineInfo.pColorBlendState = &blendInfo;

    pipelineInfo.layout = d->layout;

    Q_ASSERT(renderPass && RES_GET_D(const QVkRenderPass, renderPass)->rp);
    pipelineInfo.renderPass = RES_GET_D(const QVkRenderPass, renderPass)->rp;

    err = rhiD->df->vkCreateGraphicsPipelines(rhiD->dev, rhiD->pipelineCache, 1, &pipelineInfo, nullptr, &d->pipeline);

    for (VkShaderModule shader : shaders)
        rhiD->df->vkDestroyShaderModule(rhiD->dev, shader, nullptr);

    if (err == VK_SUCCESS) {
        d->lastActiveFrameSlot = -1;
        return true;
    } else {
        qWarning("Failed to create graphics pipeline: %d", err);
        return false;
    }
}

QVkSwapChain::QVkSwapChain(QRhi *rhi)
    : QRhiSwapChain(rhi, new QVkSwapChainPrivate)
{
}

void QVkSwapChain::release()
{
    RES_D(QVkSwapChain);
    RES_RHI(QRhiVulkan);
    rhiD->releaseSwapChainResources(this);
}

QRhiCommandBuffer *QVkSwapChain::currentFrameCommandBuffer()
{
    RES_D(QVkSwapChain);
    return &d->imageRes[d->currentImage].cmdBuf;
}

QRhiRenderTarget *QVkSwapChain::currentFrameRenderTarget()
{
    RES_D(QVkSwapChain);
    return &d->rt;
}

const QRhiRenderPass *QVkSwapChain::defaultRenderPass() const
{
    RES_D(const QVkSwapChain);
    return d->rt.renderPass();
}

QSize QVkSwapChain::sizeInPixels() const
{
    RES_D(const QVkSwapChain);
    return d->pixelSize;
}

bool QVkSwapChain::build(QWindow *window, const QSize &pixelSize, SurfaceImportFlags flags,
                         QRhiRenderBuffer *depthStencil, int sampleCount)
{
    RES_D(QVkSwapChain);
    RES_RHI(QRhiVulkan);
    return rhiD->rebuildSwapChain(window, pixelSize, flags, depthStencil, sampleCount, this);
}

QT_END_NAMESPACE
