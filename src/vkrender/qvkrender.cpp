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
}

void QVkRenderPrivate::destroy()
{
    vmaDestroyAllocator(allocator);
}

bool QVkRender::createTexture(int whatever, QVkTexture *outTex)
{
    // ###
    return true;
}

void QVkRender::releaseTexture(QVkTexture *tex)
{
    tex;
}

bool QVkRender::createRenderTarget(QVkTexture *color, QVkTexture *ds, QVkRenderTarget *outRt)
{
    color; ds; outRt;
    return true;
}

void QVkRender::releaseRenderTarget(QVkRenderTarget *rt)
{
    rt;
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

bool QVkRenderPrivate::createDefaultRenderPass(VkRenderPass *rp, bool hasDepthStencil)
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
    rpInfo.attachmentCount = hasDepthStencil ? 2 : 1;
    rpInfo.pAttachments = attDesc;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subPassDesc;

//    if (msaa) {
//        colorRef.attachment = 2;
//        subPassDesc.pResolveAttachments = &resolveRef;
//        rpInfo.attachmentCount = 3; // or 2
//    }

    VkResult err = df->vkCreateRenderPass(dev, &rpInfo, nullptr, rp);
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

    uint32_t formatCount = 0;
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
                                 outSwapChain->hasDepthStencil ? depthStencil->imageView : VK_NULL_HANDLE,
                                 VK_NULL_HANDLE }; // ### will be 3 with MSAA
        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = outSwapChain->rp;
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
    uint32_t reqBufferCount = QVkSwapChain::DEFAULT_BUFFER_COUNT;
    if (surfaceCaps.maxImageCount)
        reqBufferCount = qBound(surfaceCaps.minImageCount, reqBufferCount, surfaceCaps.maxImageCount);

    VkExtent2D bufferSize = surfaceCaps.currentExtent;
    if (bufferSize.width == uint32_t(-1)) {
        Q_ASSERT(bufferSize.height == uint32_t(-1));
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

    uint32_t actualSwapChainBufferCount = 0;
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

    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };

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

    VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
    for (int i = 0; i < QVkSwapChain::FRAME_LAG; ++i) {
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

QVkCommandBuffer *QVkRender::currentFrameCommandBuffer(QVkSwapChain *swapChain)
{
    return &swapChain->imageRes[swapChain->currentImage].cmdBuf;
}

void QVkRenderPrivate::releaseSwapChain(QVkSwapChain *swapChain)
{
    if (swapChain->sc == VK_NULL_HANDLE)
        return;

    df->vkDeviceWaitIdle(dev);

    for (int i = 0; i < QVkSwapChain::FRAME_LAG; ++i) {
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

    if (swapChain->rp) {
        df->vkDestroyRenderPass(dev, swapChain->rp, nullptr);
        swapChain->rp = VK_NULL_HANDLE;
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

    VkCommandBufferAllocateInfo cmdBufInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, d->cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    VkResult err = d->df->vkAllocateCommandBuffers(d->dev, &cmdBufInfo, &image.cmdBuf.cb);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return FrameOpDeviceLost;
        else
            qWarning("Failed to allocate frame command buffer: %d", err);
        return FrameOpError;
    }

    VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr };
    err = d->df->vkBeginCommandBuffer(image.cmdBuf.cb, &cmdBufBeginInfo);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return FrameOpDeviceLost;
        else
            qWarning("Failed to begin frame command buffer: %d", err);
        return FrameOpError;
    }

    return FrameOpSuccess;
}

QVkRender::FrameOpResult QVkRender::endFrame(QVkSwapChain *sc)
{
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
    if (1) {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &frame.drawSem;
    }
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

    sc->currentFrame = (sc->currentFrame + 1) % QVkSwapChain::FRAME_LAG;

    return FrameOpSuccess;
}

void QVkRender::beginPass(QVkSwapChain *sc, const QVkClearValue *clearValues)
{
    QVkRenderTarget rt;
    rt.fb = sc->imageRes[sc->currentImage].fb;
    rt.rp = sc->rp;
    rt.pixelSize = sc->pixelSize;
    rt.attCount = sc->hasDepthStencil ? 2 : 1; // 3 or 2 with msaa

    beginPass(&rt, &sc->imageRes[sc->currentImage].cmdBuf, clearValues);
}

void QVkRender::endPass(QVkSwapChain *sc)
{
    endPass(&sc->imageRes[sc->currentImage].cmdBuf);
}

void QVkRender::importVulkanWindowCurrentFrame(QVulkanWindow *window, QVkRenderTarget *outRt, QVkCommandBuffer *outCb)
{
    outRt->fb = window->currentFramebuffer();
    outRt->rp = window->defaultRenderPass();
    outRt->pixelSize = window->swapChainImageSize();
    outRt->attCount = window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;

    outCb->cb = window->currentCommandBuffer();
}

void QVkRender::beginPass(QVkRenderTarget *rt, QVkCommandBuffer *cb, const QVkClearValue *clearValues)
{
    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = rt->rp;
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
            cv.color = { clearValues[i].rgba.x(), clearValues[i].rgba.y(), clearValues[i].rgba.z(), clearValues[i].rgba.w() };
        cvs.append(cv);
    }
    rpBeginInfo.pClearValues = cvs.data();

    d->df->vkCmdBeginRenderPass(cb->cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void QVkRender::endPass(QVkCommandBuffer *cb)
{
    d->df->vkCmdEndRenderPass(cb->cb);
}

QT_END_NAMESPACE
