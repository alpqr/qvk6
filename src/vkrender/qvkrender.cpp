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
    : d(new QVkRenderPrivate)
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

    VmaAllocatorCreateInfo allocatorInfo;
    memset(&allocatorInfo, 0, sizeof(allocatorInfo));
    allocatorInfo.physicalDevice = physDev;
    allocatorInfo.device = dev;
    allocatorInfo.pVulkanFunctions = &afuncs;
    vmaCreateAllocator(&allocatorInfo, &allocator);

    VkDescriptorPoolSize descPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, QVK_MAX_UNIFORM_BUFFERS_PER_SRB * QVK_MAX_SHADER_RESOURCE_BINDINGS * QVK_FRAMES_IN_FLIGHT }
    };
    VkDescriptorPoolCreateInfo descPoolInfo;
    memset(&descPoolInfo, 0, sizeof(descPoolInfo));
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descPoolInfo.maxSets = QVK_MAX_SHADER_RESOURCE_BINDINGS * QVK_FRAMES_IN_FLIGHT;
    descPoolInfo.poolSizeCount = sizeof(descPoolSizes) / sizeof(descPoolSizes[0]);
    descPoolInfo.pPoolSizes = descPoolSizes;
    VkResult err = df->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &descriptorPool);
    if (err != VK_SUCCESS)
        qFatal("Failed to create descriptor pool: %d", err);
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

    if (descriptorPool) {
        df->vkDestroyDescriptorPool(dev, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }

    vmaDestroyAllocator(allocator);

    f = nullptr;
    df = nullptr;
}

static inline VmaAllocation toVmaAllocation(QVkAlloc a)
{
    return reinterpret_cast<VmaAllocation>(a);
}

static const VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

VkFormat QVkRender::optimalDepthStencilFormat() const
{
    return d->optimalDepthStencilFormat();
}

VkFormat QVkRenderPrivate::optimalDepthStencilFormat()
{
    if (dsFormat != VK_FORMAT_UNDEFINED)
        return dsFormat;

    const VkFormat dsFormatCandidates[] = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT
    };
    VkFormat dsFormat;
    const int dsFormatCandidateCount = sizeof(dsFormatCandidates) / sizeof(VkFormat);
    int dsFormatIdx = 0;
    while (dsFormatIdx < dsFormatCandidateCount) {
        dsFormat = dsFormatCandidates[dsFormatIdx];
        VkFormatProperties fmtProp;
        f->vkGetPhysicalDeviceFormatProperties(physDev, dsFormat, &fmtProp);
        if (fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            break;
        ++dsFormatIdx;
    }
    if (dsFormatIdx == dsFormatCandidateCount)
        qWarning("Failed to find an optimal depth-stencil format");

    return dsFormat;
}

bool QVkRenderPrivate::createDefaultRenderPass(QVkRenderPass *rp, bool hasDepthStencil)
{
    VkAttachmentDescription attDesc[3];
    memset(attDesc, 0, sizeof(attDesc));

    // This is either the non-msaa render target or the resolve target.
    attDesc[0].format = colorFormat;
    attDesc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // ignored when msaa
    attDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attDesc[1].format = optimalDepthStencilFormat();
    attDesc[1].samples = VK_SAMPLE_COUNT_1_BIT; // ###
    attDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

//    if (msaa) {
//        // msaa render target
//        attDesc[2].format = colorFormat;
//        attDesc[2].samples = sampleCount;
//        attDesc[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
//        attDesc[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//        attDesc[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//        attDesc[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//        attDesc[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//        attDesc[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//    }

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
//    VkAttachmentReference resolveRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
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
    rpInfo.attachmentCount = hasDepthStencil ? 2 : 1;
    rpInfo.pAttachments = attDesc;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subPassDesc;

//    if (msaa) {
//        colorRef.attachment = 2;
//        subPassDesc.pResolveAttachments = &resolveRef;
//        rpInfo.attachmentCount = 3; // or 2
//    }

    VkResult err = df->vkCreateRenderPass(dev, &rpInfo, nullptr, &rp->rp);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create renderpass: %d", err);
        return false;
    }

    return true;
}

bool QVkRender::importSurface(VkSurfaceKHR surface, const QSize &pixelSize,
                              SurfaceImportFlags flags, QVkTexture *depthStencil,
                              QVkSwapChain *outSwapChain)
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
        d->colorFormat = formats[0].format;
        d->colorSpace = formats[0].colorSpace;
    }

    if (!d->recreateSwapChain(surface, pixelSize, flags, outSwapChain))
        return false;

    outSwapChain->hasDepthStencil = flags.testFlag(ImportWithDepthStencil);

    d->createDefaultRenderPass(&outSwapChain->rp, outSwapChain->hasDepthStencil);

    for (int i = 0; i < outSwapChain->bufferCount; ++i) {
        QVkSwapChain::ImageResources &image(outSwapChain->imageRes[i]);

        VkImageView views[3] = { image.imageView,
                                 outSwapChain->hasDepthStencil ? depthStencil->d[0].imageView : VK_NULL_HANDLE,
                                 VK_NULL_HANDLE }; // ### will be 3 with MSAA
        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = outSwapChain->rp.rp;
        fbInfo.attachmentCount = flags.testFlag(ImportWithDepthStencil) ? 2 : 1;
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

bool QVkRenderPrivate::recreateSwapChain(VkSurfaceKHR surface, const QSize &pixelSize, QVkRender::SurfaceImportFlags flags, QVkSwapChain *swapChain)
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

    if (flags.testFlag(QVkRender::ImportWithAlpha)) {
        if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        else if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
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
    swapChainInfo.imageFormat = colorFormat;
    swapChainInfo.imageColorSpace = colorSpace;
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

    VkFenceCreateInfo fenceInfo;
    memset(&fenceInfo, 0, sizeof(fenceInfo));
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < swapChain->bufferCount; ++i) {
        QVkSwapChain::ImageResources &image(swapChain->imageRes[i]);
        image.image = swapChainImages[i];

        VkImageViewCreateInfo imgViewInfo;
        memset(&imgViewInfo, 0, sizeof(imgViewInfo));
        imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imgViewInfo.image = swapChainImages[i];
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = colorFormat;
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
    d->prepareNewFrame();

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
    rt.attCount = sc->hasDepthStencil ? 2 : 1; // 3 or 2 with msaa

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

void QVkRender::importVulkanWindowCurrentFrame(QVulkanWindow *window, QVkRenderTarget *outRt, QVkCommandBuffer *outCb)
{
    outRt->fb = window->currentFramebuffer();
    outRt->rp.rp = window->defaultRenderPass();
    outRt->pixelSize = window->swapChainImageSize();
    outRt->attCount = window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;

    outCb->cb = window->currentCommandBuffer();
}

void QVkRender::beginFrame(QVulkanWindow *window)
{
    Q_UNUSED(window);

    d->currentFrameSlot = window->currentFrame();
    d->prepareNewFrame();
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

VkBufferUsageFlagBits toVkBufferUsage(QVkBuffer::UsageFlags usage)
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

bool QVkRender::createBuffer(QVkBuffer *buf, const void *data)
{
    if (buf->d[0].buffer) // no repeated create without a scheduleRelease first
        return false;

    Q_ASSERT(!buf->isStatic() || data);

    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = buf->size;
    bufferInfo.usage = toVkBufferUsage(buf->usage);

    VmaAllocationCreateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));

    // Some day we may consider using GPU_ONLY for static buffers and
    // issue a transfer from a temporary CPU_ONLY staging buffer. But
    // for now treat everything as "often changing".
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

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
            if (data) {
                void *p = nullptr;
                err = vmaMapMemory(d->allocator, allocation, &p);
                if (err != VK_SUCCESS)
                    break;
                memcpy(p, data, buf->size);
                vmaUnmapMemory(d->allocator, allocation);
                vmaFlushAllocation(d->allocator, allocation, 0, buf->size);
            }
        }
    }

    if (err == VK_SUCCESS) {
        buf->lastActiveFrameSlot = -1;
        return true;
    } else {
        qWarning("Failed to create buffer: %d", err);
        return false;
    }
}

void QVkRender::updateBuffer(QVkBuffer *buf, int offset, int size, const void *data)
{
    Q_ASSERT(!buf->isStatic());
    Q_ASSERT(offset + size <= buf->size);

    if (size < 1)
        return;

    const QByteArray u(static_cast<const char *>(data), size);
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        buf->d[i].pendingUpdates.append(QVkBuffer::PendingUpdate(offset, u));
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

static VkShaderStageFlagBits toVkShaderStage(QVkGraphicsShaderStage::Type type)
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

static VkFormat toVkAttributeFormat(QVkVertexInputLayout::Attribute::Format format)
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

bool QVkRender::createGraphicsPipelineState(QVkGraphicsPipelineState *ps)
{
    if (ps->pipeline) // no repeated create without a scheduleRelease first
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

    // ### revise this later
    VkDynamicState dynEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo;
    memset(&dynamicInfo, 0, sizeof(dynamicInfo));
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
    dynamicInfo.pDynamicStates = dynEnable;
    pipelineInfo.pDynamicState = &dynamicInfo;

    VkPipelineViewportStateCreateInfo viewportInfo;
    memset(&viewportInfo, 0, sizeof(viewportInfo));
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;
    pipelineInfo.pViewportState = &viewportInfo;

    VkPipelineInputAssemblyStateCreateInfo inputAsmInfo;
    memset(&inputAsmInfo, 0, sizeof(inputAsmInfo));
    inputAsmInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;;
    inputAsmInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // ###
    pipelineInfo.pInputAssemblyState = &inputAsmInfo;

    VkPipelineRasterizationStateCreateInfo rastInfo;
    memset(&rastInfo, 0, sizeof(rastInfo));
    rastInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rastInfo.cullMode = VK_CULL_MODE_NONE; // ###
    rastInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rastInfo.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rastInfo;

    VkPipelineMultisampleStateCreateInfo msInfo;
    memset(&msInfo, 0, sizeof(msInfo));
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.pMultisampleState = &msInfo;

    VkPipelineDepthStencilStateCreateInfo dsInfo;
    memset(&dsInfo, 0, sizeof(dsInfo));
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.depthTestEnable = VK_FALSE; // ###
    dsInfo.depthWriteEnable = VK_FALSE;
    dsInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineInfo.pDepthStencilState = &dsInfo;

    VkPipelineColorBlendStateCreateInfo blendInfo;
    memset(&blendInfo, 0, sizeof(blendInfo));
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    VkPipelineColorBlendAttachmentState blendAtt;
    memset(&blendAtt, 0, sizeof(blendAtt));
    blendAtt.colorWriteMask = 0xF;
    blendInfo.attachmentCount = 1;
    blendInfo.pAttachments = &blendAtt;
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
    // ### others
    default:
        Q_UNREACHABLE();
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

static VkShaderStageFlags toVkShaderStageFlags(QVkShaderResourceBindings::Binding::StageFlags stage)
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
    if (srb->layout) // no repeated create without a scheduleRelease first
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
    allocInfo.descriptorPool = d->descriptorPool;
    allocInfo.descriptorSetCount = QVK_FRAMES_IN_FLIGHT;
    VkDescriptorSetLayout layouts[QVK_FRAMES_IN_FLIGHT];
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        layouts[i] = srb->layout;
    allocInfo.pSetLayouts = layouts;
    err = d->df->vkAllocateDescriptorSets(d->dev, &allocInfo, srb->descSets);
    if (err != VK_SUCCESS) {
        qWarning("Failed to allocate descriptor sets: %d", err);
        return false;
    }

    QVarLengthArray<VkDescriptorBufferInfo, 4> bufferInfos;
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
                bufInfo.offset = 0;
                bufInfo.range = buf->size;
                bufferInfos.append(bufInfo);
                writeInfo.pBufferInfo = &bufferInfos.last();
            }
                break;
            // ### others
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

void QVkRender::cmdSetVertexBuffer(QVkCommandBuffer *cb, int binding, QVkBuffer *vb, quint32 offset)
{
    d->prepareBufferForUse(vb);
    const int idx = vb->isStatic() ? 0 : d->currentFrameSlot;
    const VkDeviceSize ofs = offset;
    d->df->vkCmdBindVertexBuffers(cb->cb, binding, 1, &vb->d[idx].buffer, &ofs);
}

void QVkRender::cmdSetVertexBuffers(QVkCommandBuffer *cb, int startBinding, const QVector<QVkBuffer *> &vb, const QVector<quint32> &offset)
{
    QVarLengthArray<VkBuffer, 4> bufs;
    QVarLengthArray<VkDeviceSize, 4> ofs;
    for (int i = 0, ie = vb.count(); i != ie; ++i) {
        d->prepareBufferForUse(vb[i]);
        const int idx = vb[i]->isStatic() ? 0 : d->currentFrameSlot;
        bufs.append(vb[i]->d[idx].buffer);
        ofs.append(offset[i]);
    }
    d->df->vkCmdBindVertexBuffers(cb->cb, startBinding, bufs.count(), bufs.constData(), ofs.constData());
}

void QVkRender::cmdSetIndexBuffer(QVkCommandBuffer *cb, QVkBuffer *ib, quint32 offset, IndexFormat format)
{
    d->prepareBufferForUse(ib);
    const int idx = ib->isStatic() ? 0 : d->currentFrameSlot;
    const VkIndexType type = format == IndexUInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    d->df->vkCmdBindIndexBuffer(cb->cb, ib->d[idx].buffer, offset, type);
}

void QVkRender::cmdSetGraphicsPipelineState(QVkCommandBuffer *cb, QVkGraphicsPipelineState *ps)
{
    Q_ASSERT(ps->pipeline);

    for (const QVkShaderResourceBindings::Binding &b : qAsConst(ps->shaderResourceBindings->bindings)) {
        switch (b.type) {
        case QVkShaderResourceBindings::Binding::UniformBuffer:
            d->prepareBufferForUse(b.ubuf.buf);
            break;
        // ### others
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    ps->lastActiveFrameSlot = d->currentFrameSlot;
    ps->shaderResourceBindings->lastActiveFrameSlot = d->currentFrameSlot;

    d->df->vkCmdBindPipeline(cb->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, ps->pipeline);

    d->df->vkCmdBindDescriptorSets(cb->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, ps->layout, 0, 1,
                                   &ps->shaderResourceBindings->descSets[d->currentFrameSlot], 0, nullptr);
}

static VkViewport toVkViewport(const QVkViewport &viewport)
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

void QVkRender::cmdViewport(QVkCommandBuffer *cb, const QVkViewport &viewport)
{
    VkViewport vp = toVkViewport(viewport);
    d->df->vkCmdSetViewport(cb->cb, 0, 1, &vp);
}

static VkRect2D toVkScissor(const QVkScissor &scissor)
{
    VkRect2D s;
    s.offset.x = scissor.r.x();
    s.offset.y = scissor.r.y();
    s.extent.width = scissor.r.width();
    s.extent.height = scissor.r.height();
    return s;
}

void QVkRender::cmdScissor(QVkCommandBuffer *cb, const QVkScissor &scissor)
{
    VkRect2D s = toVkScissor(scissor);
    d->df->vkCmdSetScissor(cb->cb, 0, 1, &s);
}

void QVkRender::cmdDraw(QVkCommandBuffer *cb, quint32 vertexCount, quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    d->df->vkCmdDraw(cb->cb, vertexCount, instanceCount, firstVertex, firstInstance);
}

void QVkRenderPrivate::prepareNewFrame()
{
    Q_ASSERT(!inFrame);
    inFrame = true;

    executeDeferredReleases();
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
            case QVkRenderPrivate::DeferredReleaseEntry::PipelineState:
                if (e.pipelineState.pipeline)
                    df->vkDestroyPipeline(dev, e.pipelineState.pipeline, nullptr);
                if (e.pipelineState.layout)
                    df->vkDestroyPipelineLayout(dev, e.pipelineState.layout, nullptr);
                break;
            case QVkRenderPrivate::DeferredReleaseEntry::ShaderResourceBindings:
                if (e.shaderResourceBindings.layout)
                    df->vkDestroyDescriptorSetLayout(dev, e.shaderResourceBindings.layout, nullptr);
                if (descriptorPool)
                    df->vkFreeDescriptorSets(dev, descriptorPool, QVK_FRAMES_IN_FLIGHT, e.shaderResourceBindings.sets);
                break;
            case QVkRenderPrivate::DeferredReleaseEntry::Buffer:
                for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
                    vmaDestroyBuffer(allocator, e.buffer[i].buffer, toVmaAllocation(e.buffer[i].allocation));
            default:
                break;
            }
            releaseQueue.removeAt(i);
        }
    }
}

void QVkRender::scheduleRelease(QVkGraphicsPipelineState *ps)
{
    if (!ps->pipeline && !ps->layout)
        return;

    QVkRenderPrivate::DeferredReleaseEntry e;
    e.type = QVkRenderPrivate::DeferredReleaseEntry::PipelineState;
    e.lastActiveFrameSlot = ps->lastActiveFrameSlot;

    e.pipelineState.pipeline = ps->pipeline;
    e.pipelineState.layout = ps->layout;

    ps->pipeline = VK_NULL_HANDLE;
    ps->layout = VK_NULL_HANDLE;

    d->releaseQueue.append(e);
}

void QVkRender::scheduleRelease(QVkShaderResourceBindings *srb)
{
    if (!srb->layout)
        return;

    QVkRenderPrivate::DeferredReleaseEntry e;
    e.type = QVkRenderPrivate::DeferredReleaseEntry::ShaderResourceBindings;
    e.lastActiveFrameSlot = srb->lastActiveFrameSlot;

    e.shaderResourceBindings.layout = srb->layout;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        e.shaderResourceBindings.sets[i] = srb->descSets[i];

    srb->layout = VK_NULL_HANDLE;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        srb->descSets[i] = VK_NULL_HANDLE;

    d->releaseQueue.append(e);
}

void QVkRender::scheduleRelease(QVkBuffer *buf)
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

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        e.buffer[i].buffer = buf->d[i].buffer;
        e.buffer[i].allocation = buf->d[i].allocation;

        buf->d[i].buffer = VK_NULL_HANDLE;
        buf->d[i].allocation = nullptr;
    }

    d->releaseQueue.append(e);
}

QT_END_NAMESPACE
