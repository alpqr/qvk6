/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt RHI module
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

#include "qrhivulkan_p.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_RECORDING_ENABLED 0
#ifdef QT_DEBUG
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#endif
#include "vk_mem_alloc.h"

#include <qmath.h>
#include <QVulkanFunctions>
#include <QVulkanWindow>

QT_BEGIN_NAMESPACE

/*
  Vulkan 1.0 backend. Provides a double-buffered swapchain that throttles the
  rendering thread to vsync. Textures and "static" buffers are device local,
  and a separate, host visible staging buffer is used to upload data to them.
  "Dynamic" buffers are in host visible memory and are duplicated (since there
  can be 2 frames in flight). This is handled transparently to the application.
*/

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

static inline VmaAllocation toVmaAllocation(QVkAlloc a)
{
    return reinterpret_cast<VmaAllocation>(a);
}

static inline VmaAllocator toVmaAllocator(QVkAllocator a)
{
    return reinterpret_cast<VmaAllocator>(a);
}

QRhiVulkan::QRhiVulkan(QRhiInitParams *params)
    : ofr(this)
{
    QRhiVulkanInitParams *vkparams = static_cast<QRhiVulkanInitParams *>(params);
    inst = vkparams->inst;
    importedDevPoolQueue = vkparams->importExistingDevice;
    if (importedDevPoolQueue) {
        physDev = vkparams->physDev;
        dev = vkparams->dev;
        gfxQueueFamilyIdx = vkparams->gfxQueueFamilyIdx;
        gfxQueue = vkparams->gfxQueue;
        cmdPool = vkparams->cmdPool;
        allocator = vkparams->vmemAllocator;
    }
    maybeWindow = vkparams->window; // may be null
}

bool QRhiVulkan::create(QRhi::Flags flags)
{
    Q_UNUSED(flags);
    Q_ASSERT(inst);

    globalVulkanInstance = inst; // assume this will not change during the lifetime of the entire application

    f = inst->functions();

    if (!importedDevPoolQueue) {
        uint32_t devCount = 0;
        f->vkEnumeratePhysicalDevices(inst->vkInstance(), &devCount, nullptr);
        qDebug("%d physical devices", devCount);
        if (!devCount) {
            qWarning("No physical devices");
            return false;
        }

        // Just pick the first physical device for now.
        devCount = 1;
        VkResult err = f->vkEnumeratePhysicalDevices(inst->vkInstance(), &devCount, &physDev);
        if (err != VK_SUCCESS) {
            qWarning("Failed to enumerate physical devices: %d", err);
            return false;
        }

        uint32_t queueCount = 0;
        f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueCount, nullptr);
        QVector<VkQueueFamilyProperties> queueFamilyProps(queueCount);
        f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueCount, queueFamilyProps.data());

        gfxQueue = VK_NULL_HANDLE;
        gfxQueueFamilyIdx = -1;
        int presQueueFamilyIdx = -1;
        for (int i = 0; i < queueFamilyProps.count(); ++i) {
            qDebug("queue family %d: flags=0x%x count=%d", i, queueFamilyProps[i].queueFlags, queueFamilyProps[i].queueCount);
            if (gfxQueueFamilyIdx == -1
                    && (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    && (!maybeWindow || inst->supportsPresent(physDev, i, maybeWindow)))
            {
                gfxQueueFamilyIdx = i;
            }
        }
        if (gfxQueueFamilyIdx != -1) {
            presQueueFamilyIdx = gfxQueueFamilyIdx;
        } else {
            // ###
            qWarning("No graphics queue that can present. This is not supported atm.");
        }
        if (gfxQueueFamilyIdx == -1) {
            qWarning("No graphics queue family found");
            return false;
        }
        if (presQueueFamilyIdx == -1) {
            qWarning("No present queue family found");
            return false;
        }

        VkDeviceQueueCreateInfo queueInfo[2];
        const float prio[] = { 0 };
        memset(queueInfo, 0, sizeof(queueInfo));
        queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo[0].queueFamilyIndex = gfxQueueFamilyIdx;
        queueInfo[0].queueCount = 1;
        queueInfo[0].pQueuePriorities = prio;
        if (gfxQueueFamilyIdx != presQueueFamilyIdx) {
            queueInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo[1].queueFamilyIndex = presQueueFamilyIdx;
            queueInfo[1].queueCount = 1;
            queueInfo[1].pQueuePriorities = prio;
        }

        QVector<const char *> devLayers;
        if (inst->layers().contains("VK_LAYER_LUNARG_standard_validation"))
            devLayers.append("VK_LAYER_LUNARG_standard_validation");

        QVector<const char *> devExts;
        devExts.append("VK_KHR_swapchain");

        VkDeviceCreateInfo devInfo;
        memset(&devInfo, 0, sizeof(devInfo));
        devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devInfo.queueCreateInfoCount = gfxQueueFamilyIdx == presQueueFamilyIdx ? 1 : 2;
        devInfo.pQueueCreateInfos = queueInfo;
        devInfo.enabledLayerCount = devLayers.count();
        devInfo.ppEnabledLayerNames = devLayers.constData();
        devInfo.enabledExtensionCount = devExts.count();
        devInfo.ppEnabledExtensionNames = devExts.constData();

        err = f->vkCreateDevice(physDev, &devInfo, nullptr, &dev);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create device: %d", err);
            return false;
        }
    }

    df = inst->deviceFunctions(dev);
    if (!cmdPool) {
        VkCommandPoolCreateInfo poolInfo;
        memset(&poolInfo, 0, sizeof(poolInfo));
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = gfxQueueFamilyIdx;
        VkResult err = df->vkCreateCommandPool(dev, &poolInfo, nullptr, &cmdPool);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create command pool: %d", err);
            return false;
        }
    }
    if (gfxQueueFamilyIdx != -1 && !gfxQueue)
        df->vkGetDeviceQueue(dev, gfxQueueFamilyIdx, 0, &gfxQueue);

    f->vkGetPhysicalDeviceProperties(physDev, &physDevProperties);
    ubufAlign = physDevProperties.limits.minUniformBufferOffsetAlignment;
    texbufAlign = physDevProperties.limits.optimalBufferCopyOffsetAlignment;

    qDebug("Device name: %s Driver version: %d.%d.%d", physDevProperties.deviceName,
           VK_VERSION_MAJOR(physDevProperties.driverVersion),
           VK_VERSION_MINOR(physDevProperties.driverVersion),
           VK_VERSION_PATCH(physDevProperties.driverVersion));

    if (!allocator) {
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
        VmaAllocator vmaallocator;
        VkResult err = vmaCreateAllocator(&allocatorInfo, &vmaallocator);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create allocator: %d", err);
            return false;
        }
        allocator = vmaallocator;
    }

    VkDescriptorPool pool;
    VkResult err = createDescriptorPool(&pool);
    if (err == VK_SUCCESS)
        descriptorPools.append(pool);
    else
        qWarning("Failed to create initial descriptor pool: %d", err);

    nativeHandlesStruct.physDev = physDev;
    nativeHandlesStruct.dev = dev;
    nativeHandlesStruct.gfxQueueFamilyIdx = gfxQueueFamilyIdx;
    nativeHandlesStruct.gfxQueue = gfxQueue;
    nativeHandlesStruct.cmdPool = cmdPool;
    nativeHandlesStruct.vmemAllocator = allocator;

    return true;
}

void QRhiVulkan::destroy()
{
    if (!df)
        return;

    df->vkDeviceWaitIdle(dev);

    executeDeferredReleases(true);
    finishActiveReadbacks(true);

    if (ofr.cmdFence) {
        df->vkDestroyFence(dev, ofr.cmdFence, nullptr);
        ofr.cmdFence = VK_NULL_HANDLE;
    }

    if (ofr.cbWrapper.cb) {
        df->vkFreeCommandBuffers(dev, cmdPool, 1, &ofr.cbWrapper.cb);
        ofr.cbWrapper.cb = VK_NULL_HANDLE;
    }

    if (pipelineCache) {
        df->vkDestroyPipelineCache(dev, pipelineCache, nullptr);
        pipelineCache = VK_NULL_HANDLE;
    }

    for (const DescriptorPoolData &pool : descriptorPools)
        df->vkDestroyDescriptorPool(dev, pool.pool, nullptr);

    descriptorPools.clear();

    if (!importedDevPoolQueue) {
        vmaDestroyAllocator(toVmaAllocator(allocator));
        if (cmdPool) {
            df->vkDestroyCommandPool(dev, cmdPool, nullptr);
            cmdPool = VK_NULL_HANDLE;
        }
        if (dev) {
            df->vkDestroyDevice(dev, nullptr);
            inst->resetDeviceFunctions(dev);
            dev = VK_NULL_HANDLE;
        }
    }

    f = nullptr;
    df = nullptr;
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

static inline VkFormat toVkTextureFormat(QRhiTexture::Format format, QRhiTexture::Flags flags)
{
    const bool srgb = flags.testFlag(QRhiTexture::sRGB);
    switch (format) {
    case QRhiTexture::RGBA8:
        return srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    case QRhiTexture::BGRA8:
        return srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
    case QRhiTexture::R8:
        return srgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
    case QRhiTexture::R16:
        return VK_FORMAT_R16_UNORM;

    case QRhiTexture::D16:
        return VK_FORMAT_D16_UNORM;
    case QRhiTexture::D32:
        return VK_FORMAT_D32_SFLOAT;

    case QRhiTexture::BC1:
        return srgb ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case QRhiTexture::BC2:
        return srgb ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
    case QRhiTexture::BC3:
        return srgb ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
    case QRhiTexture::BC4:
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case QRhiTexture::BC5:
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case QRhiTexture::BC6H:
        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case QRhiTexture::BC7:
        return srgb ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;

    case QRhiTexture::ETC2_RGB8:
        return srgb ? VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK : VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case QRhiTexture::ETC2_RGB8A1:
        return srgb ? VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK : VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
    case QRhiTexture::ETC2_RGBA8:
        return srgb ? VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK : VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;

    case QRhiTexture::ASTC_4x4:
        return srgb ? VK_FORMAT_ASTC_4x4_SRGB_BLOCK : VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case QRhiTexture::ASTC_5x4:
        return srgb ? VK_FORMAT_ASTC_5x4_SRGB_BLOCK : VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
    case QRhiTexture::ASTC_5x5:
        return srgb ? VK_FORMAT_ASTC_5x5_SRGB_BLOCK : VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
    case QRhiTexture::ASTC_6x5:
        return srgb ? VK_FORMAT_ASTC_6x5_SRGB_BLOCK : VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
    case QRhiTexture::ASTC_6x6:
        return srgb ? VK_FORMAT_ASTC_6x6_SRGB_BLOCK : VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case QRhiTexture::ASTC_8x5:
        return srgb ? VK_FORMAT_ASTC_8x5_SRGB_BLOCK : VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
    case QRhiTexture::ASTC_8x6:
        return srgb ? VK_FORMAT_ASTC_8x6_SRGB_BLOCK : VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
    case QRhiTexture::ASTC_8x8:
        return srgb ? VK_FORMAT_ASTC_8x8_SRGB_BLOCK : VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case QRhiTexture::ASTC_10x5:
        return srgb ? VK_FORMAT_ASTC_10x5_SRGB_BLOCK : VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
    case QRhiTexture::ASTC_10x6:
        return srgb ? VK_FORMAT_ASTC_10x6_SRGB_BLOCK : VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
    case QRhiTexture::ASTC_10x8:
        return srgb ? VK_FORMAT_ASTC_10x8_SRGB_BLOCK : VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
    case QRhiTexture::ASTC_10x10:
        return srgb ? VK_FORMAT_ASTC_10x10_SRGB_BLOCK : VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
    case QRhiTexture::ASTC_12x10:
        return srgb ? VK_FORMAT_ASTC_12x10_SRGB_BLOCK : VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
    case QRhiTexture::ASTC_12x12:
        return srgb ? VK_FORMAT_ASTC_12x12_SRGB_BLOCK : VK_FORMAT_ASTC_12x12_UNORM_BLOCK;

    default:
        Q_UNREACHABLE();
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

static inline QRhiTexture::Format colorTextureFormatFromVkFormat(VkFormat format, QRhiTexture::Flags *flags)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
        return QRhiTexture::RGBA8;
    case VK_FORMAT_R8G8B8A8_SRGB:
        if (flags)
            (*flags) |= QRhiTexture::sRGB;
        return QRhiTexture::RGBA8;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return QRhiTexture::BGRA8;
    case VK_FORMAT_B8G8R8A8_SRGB:
        if (flags)
            (*flags) |= QRhiTexture::sRGB;
        return QRhiTexture::BGRA8;
    case VK_FORMAT_R8_UNORM:
        return QRhiTexture::R8;
    case VK_FORMAT_R8_SRGB:
        if (flags)
            (*flags) |= QRhiTexture::sRGB;
        return QRhiTexture::R8;
    case VK_FORMAT_R16_UNORM:
        return QRhiTexture::R16;
    default: // this cannot assert, must warn and return unknown
        qWarning("VkFormat %d is not a recognized uncompressed color format", format);
        break;
    }
    return QRhiTexture::UnknownFormat;
}

static inline bool isDepthTextureFormat(QRhiTexture::Format format)
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
                                      VkSampleCountFlagBits samples,
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
        imgInfo.samples = samples;
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

bool QRhiVulkan::createDefaultRenderPass(VkRenderPass *rp, bool hasDepthStencil, VkSampleCountFlagBits samples, VkFormat colorFormat)
{
    VkAttachmentDescription attDesc[3];
    memset(attDesc, 0, sizeof(attDesc));

    // attachment list layout is color (1), ds (0-1), resolve (0-1)

    attDesc[0].format = colorFormat;
    attDesc[0].samples = samples;
    attDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[0].storeOp = samples > VK_SAMPLE_COUNT_1_BIT ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    attDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[0].finalLayout = samples > VK_SAMPLE_COUNT_1_BIT ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // clear on load + no store + lazy alloc + transient image should play
    // nicely with tiled GPUs (no physical backing necessary for ds buffer)
    attDesc[1].format = optimalDepthStencilFormat();
    attDesc[1].samples = samples;
    attDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    if (samples > VK_SAMPLE_COUNT_1_BIT) {
        attDesc[2].format = colorFormat;
        attDesc[2].samples = VK_SAMPLE_COUNT_1_BIT;
        attDesc[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attDesc[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attDesc[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attDesc[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attDesc[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attDesc[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference dsRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkAttachmentReference resolveRef = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

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

    if (samples > VK_SAMPLE_COUNT_1_BIT) {
        rpInfo.attachmentCount += 1;
        subPassDesc.pResolveAttachments = &resolveRef;
    }

    VkResult err = df->vkCreateRenderPass(dev, &rpInfo, nullptr, rp);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create renderpass: %d", err);
        return false;
    }

    return true;
}

bool QRhiVulkan::createOffscreenRenderPass(VkRenderPass *rp,
                                           const QVector<QRhiTextureRenderTargetDescription::ColorAttachment> &colorAttachments,
                                           bool preserveColor,
                                           bool preserveDs,
                                           QRhiRenderBuffer *depthStencilBuffer,
                                           QRhiTexture *depthTexture)
{
    QVarLengthArray<VkAttachmentDescription, 8> attDescs;
    QVarLengthArray<VkAttachmentReference, 8> colorRefs;
    QVarLengthArray<VkAttachmentReference, 8> resolveRefs;
    const int colorAttCount = colorAttachments.count();

    // attachment list layout is color (0-8), ds (0-1), resolve (0-8)

    for (int i = 0; i < colorAttCount; ++i) {
        QVkTexture *texD = QRHI_RES(QVkTexture, colorAttachments[i].texture);
        QVkRenderBuffer *rbD = QRHI_RES(QVkRenderBuffer, colorAttachments[i].renderBuffer);
        Q_ASSERT(texD || rbD);
        const VkFormat vkformat = texD ? texD->vkformat : rbD->vkformat;
        const VkSampleCountFlagBits samples = texD ? texD->samples : rbD->samples;

        VkAttachmentDescription attDesc;
        memset(&attDesc, 0, sizeof(attDesc));
        attDesc.format = vkformat;
        attDesc.samples = samples;
        attDesc.loadOp = preserveColor ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        attDesc.storeOp = colorAttachments[i].resolveTexture ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
        attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attDesc.initialLayout = preserveColor ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        attDesc.finalLayout = colorAttachments[i].resolveTexture ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attDescs.append(attDesc);

        const VkAttachmentReference ref = { uint32_t(attDescs.count() - 1), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        colorRefs.append(ref);
    }

    const bool hasDepthStencil = depthStencilBuffer || depthTexture;
    if (hasDepthStencil) {
        const VkFormat dsFormat = depthTexture ? QRHI_RES(QVkTexture, depthTexture)->vkformat
                                               : QRHI_RES(QVkRenderBuffer, depthStencilBuffer)->vkformat;
        const VkSampleCountFlagBits samples = depthTexture ? QRHI_RES(QVkTexture, depthTexture)->samples
                                                           : QRHI_RES(QVkRenderBuffer, depthStencilBuffer)->samples;
        const VkAttachmentLoadOp loadOp = preserveDs ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        const VkAttachmentStoreOp storeOp = depthTexture ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentDescription attDesc;
        memset(&attDesc, 0, sizeof(attDesc));
        attDesc.format = dsFormat;
        attDesc.samples = samples;
        attDesc.loadOp = loadOp;
        attDesc.storeOp = storeOp;
        attDesc.stencilLoadOp = loadOp;
        attDesc.stencilStoreOp = storeOp;
        attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attDesc.finalLayout = depthTexture ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attDescs.append(attDesc);
    }
    VkAttachmentReference dsRef = { uint32_t(attDescs.count() - 1), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    for (int i = 0; i < colorAttCount; ++i) {
        if (colorAttachments[i].resolveTexture) {
            QVkTexture *rtexD = QRHI_RES(QVkTexture, colorAttachments[i].resolveTexture);
            if (rtexD->samples > VK_SAMPLE_COUNT_1_BIT)
                qWarning("Resolving into a multisample texture is not supported");

            VkAttachmentDescription attDesc;
            memset(&attDesc, 0, sizeof(attDesc));
            attDesc.format = rtexD->vkformat;
            attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
            attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // ignored
            attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attDescs.append(attDesc);

            const VkAttachmentReference ref = { uint32_t(attDescs.count() - 1), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
            resolveRefs.append(ref);
        } else {
            const VkAttachmentReference ref = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
            resolveRefs.append(ref);
        }
    }

    VkSubpassDescription subPassDesc;
    memset(&subPassDesc, 0, sizeof(subPassDesc));
    subPassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPassDesc.colorAttachmentCount = colorRefs.count();
    Q_ASSERT(colorRefs.count() == resolveRefs.count());
    subPassDesc.pColorAttachments = !colorRefs.isEmpty() ? colorRefs.constData() : nullptr;
    subPassDesc.pDepthStencilAttachment = hasDepthStencil ? &dsRef : nullptr;
    subPassDesc.pResolveAttachments = !resolveRefs.isEmpty() ? resolveRefs.constData() : nullptr;

    VkRenderPassCreateInfo rpInfo;
    memset(&rpInfo, 0, sizeof(rpInfo));
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = attDescs.count();
    rpInfo.pAttachments = attDescs.constData();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subPassDesc;

    VkResult err = df->vkCreateRenderPass(dev, &rpInfo, nullptr, rp);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create renderpass: %d", err);
        return false;
    }

    return true;
}

bool QRhiVulkan::recreateSwapChain(QRhiSwapChain *swapChain)
{
    QVkSwapChain *swapChainD = QRHI_RES(QVkSwapChain, swapChain);
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, swapChainD->surface, &surfaceCaps);
    quint32 reqBufferCount = QVkSwapChain::DEFAULT_BUFFER_COUNT;
    if (surfaceCaps.maxImageCount)
        reqBufferCount = qBound(surfaceCaps.minImageCount, reqBufferCount, surfaceCaps.maxImageCount);

    VkSurfaceTransformFlagBitsKHR preTransform =
        (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
        : surfaceCaps.currentTransform;

    VkCompositeAlphaFlagBitsKHR compositeAlpha =
        (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
        : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    if (swapChainD->m_flags.testFlag(QRhiSwapChain::SurfaceHasPreMulAlpha)
            && (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR))
    {
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }

    if (swapChainD->m_flags.testFlag(QRhiSwapChain::SurfaceHasNonPreMulAlpha)
            && (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR))
    {
        compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainD->supportsReadback = (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (swapChainD->supportsReadback && swapChainD->m_flags.testFlag(QRhiSwapChain::UsedAsTransferSource))
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    qDebug("Creating new swapchain of %d buffers, size %dx%d",
           reqBufferCount, swapChainD->pixelSize.width(), swapChainD->pixelSize.height());

    VkSwapchainKHR oldSwapChain = swapChainD->sc;
    VkSwapchainCreateInfoKHR swapChainInfo;
    memset(&swapChainInfo, 0, sizeof(swapChainInfo));
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = swapChainD->surface;
    swapChainInfo.minImageCount = reqBufferCount;
    swapChainInfo.imageFormat = swapChainD->colorFormat;
    swapChainInfo.imageColorSpace = swapChainD->colorSpace;
    swapChainInfo.imageExtent = VkExtent2D { uint32_t(swapChainD->pixelSize.width()), uint32_t(swapChainD->pixelSize.height()) };
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

    if (actualSwapChainBufferCount > QVkSwapChain::MAX_BUFFER_COUNT) {
        qWarning("Too many swapchain buffers (%d)", actualSwapChainBufferCount);
        return false;
    }
    swapChainD->bufferCount = actualSwapChainBufferCount;

    VkImage swapChainImages[QVkSwapChain::MAX_BUFFER_COUNT];
    err = vkGetSwapchainImagesKHR(dev, swapChainD->sc, &actualSwapChainBufferCount, swapChainImages);
    if (err != VK_SUCCESS) {
        qWarning("Failed to get swapchain images: %d", err);
        return false;
    }

    VkImage msaaImages[QVkSwapChain::MAX_BUFFER_COUNT];
    VkImageView msaaViews[QVkSwapChain::MAX_BUFFER_COUNT];
    if (swapChainD->samples > VK_SAMPLE_COUNT_1_BIT) {
        if (!createTransientImage(swapChainD->colorFormat,
                                  swapChainD->pixelSize,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  swapChainD->samples,
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
        QVkSwapChain::ImageResources &image(swapChainD->imageRes[i]);
        image.image = swapChainImages[i];
        if (swapChainD->samples > VK_SAMPLE_COUNT_1_BIT) {
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
        QVkSwapChain::FrameResources &frame(swapChainD->frameRes[i]);

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
    QVkSwapChain *swapChainD = QRHI_RES(QVkSwapChain, swapChain);

    if (swapChainD->sc == VK_NULL_HANDLE)
        return;

    df->vkDeviceWaitIdle(dev);

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        QVkSwapChain::FrameResources &frame(swapChainD->frameRes[i]);
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
        QVkSwapChain::ImageResources &image(swapChainD->imageRes[i]);
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
        if (image.cmdBuf) {
            df->vkFreeCommandBuffers(dev, cmdPool, 1, &image.cmdBuf);
            image.cmdBuf = VK_NULL_HANDLE;
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

QRhi::FrameOpResult QRhiVulkan::beginFrame(QRhiSwapChain *swapChain)
{
    if (QRHI_RES(QVkSwapChain, swapChain)->wrapWindow)
        return beginWrapperFrame(swapChain);
    else
        return beginNonWrapperFrame(swapChain);
}

QRhi::FrameOpResult QRhiVulkan::endFrame(QRhiSwapChain *swapChain)
{
    if (QRHI_RES(QVkSwapChain, swapChain)->wrapWindow)
        return endWrapperFrame(swapChain);
    else
        return endNonWrapperFrame(swapChain);
}

QRhi::FrameOpResult QRhiVulkan::beginWrapperFrame(QRhiSwapChain *swapChain)
{
    QVkSwapChain *swapChainD = QRHI_RES(QVkSwapChain, swapChain);
    QVulkanWindow *w = swapChainD->wrapWindow;

    swapChainD->cbWrapper.cb = w->currentCommandBuffer();

    swapChainD->rtWrapper.d.fb = w->currentFramebuffer();
    swapChainD->m_currentPixelSize = swapChainD->pixelSize = swapChainD->rtWrapper.d.pixelSize = w->swapChainImageSize();
    swapChainD->rtWrapper.d.dpr = w->devicePixelRatio();

    currentFrameSlot = w->currentFrame();
    currentSwapChain = swapChainD;

    prepareNewFrame(&swapChainD->cbWrapper);

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiVulkan::endWrapperFrame(QRhiSwapChain *swapChain)
{
    Q_UNUSED(swapChain);

    prepareFrameEnd();
    currentSwapChain = nullptr;

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiVulkan::startCommandBuffer(VkCommandBuffer *cb)
{
    if (*cb) {
        df->vkFreeCommandBuffers(dev, cmdPool, 1, cb);
        *cb = VK_NULL_HANDLE;
    }

    VkCommandBufferAllocateInfo cmdBufInfo;
    memset(&cmdBufInfo, 0, sizeof(cmdBufInfo));
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufInfo.commandPool = cmdPool;
    cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufInfo.commandBufferCount = 1;

    VkResult err = df->vkAllocateCommandBuffers(dev, &cmdBufInfo, cb);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return QRhi::FrameOpDeviceLost;
        else
            qWarning("Failed to allocate frame command buffer: %d", err);
        return QRhi::FrameOpError;
    }

    VkCommandBufferBeginInfo cmdBufBeginInfo;
    memset(&cmdBufBeginInfo, 0, sizeof(cmdBufBeginInfo));
    cmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    err = df->vkBeginCommandBuffer(*cb, &cmdBufBeginInfo);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return QRhi::FrameOpDeviceLost;
        else
            qWarning("Failed to begin frame command buffer: %d", err);
        return QRhi::FrameOpError;
    }

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiVulkan::endAndSubmitCommandBuffer(VkCommandBuffer cb, VkFence cmdFence,
                                                          VkSemaphore *waitSem, VkSemaphore *signalSem)
{
    VkResult err = df->vkEndCommandBuffer(cb);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return QRhi::FrameOpDeviceLost;
        else
            qWarning("Failed to end frame command buffer: %d", err);
        return QRhi::FrameOpError;
    }

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;
    if (waitSem) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSem;
    }
    if (signalSem) {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSem;
    }
    VkPipelineStageFlags psf = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.pWaitDstStageMask = &psf;

    err = df->vkQueueSubmit(gfxQueue, 1, &submitInfo, cmdFence);
    if (err != VK_SUCCESS) {
        if (checkDeviceLost(err))
            return QRhi::FrameOpDeviceLost;
        else
            qWarning("Failed to submit to graphics queue: %d", err);
        return QRhi::FrameOpError;
    }

    return QRhi::FrameOpSuccess;
}

void QRhiVulkan::waitCommandCompletion(int frameSlot)
{
    for (QVkSwapChain *sc : qAsConst(swapchains)) {
        QVkSwapChain::ImageResources &image(sc->imageRes[sc->frameRes[frameSlot].imageIndex]);
        if (image.cmdFenceWaitable) {
            df->vkWaitForFences(dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
            df->vkResetFences(dev, 1, &image.cmdFence);
            image.cmdFenceWaitable = false;
        }
    }
}

QRhi::FrameOpResult QRhiVulkan::beginNonWrapperFrame(QRhiSwapChain *swapChain)
{
    QVkSwapChain *swapChainD = QRHI_RES(QVkSwapChain, swapChain);
    QVkSwapChain::FrameResources &frame(swapChainD->frameRes[swapChainD->currentFrame]);

    if (!frame.imageAcquired) {
        // Wait if we are too far ahead, i.e. the thread gets throttled based on the presentation rate
        // (note that we are using FIFO mode -> vsync)
        if (frame.fenceWaitable) {
            df->vkWaitForFences(dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
            df->vkResetFences(dev, 1, &frame.fence);
            frame.fenceWaitable = false;
        }

        // move on to next swapchain image
        VkResult err = vkAcquireNextImageKHR(dev, swapChainD->sc, UINT64_MAX,
                                             frame.imageSem, frame.fence, &frame.imageIndex);
        if (err == VK_SUCCESS || err == VK_SUBOPTIMAL_KHR) {
            swapChainD->currentImage = frame.imageIndex;
            frame.imageSemWaitable = true;
            frame.imageAcquired = true;
            frame.fenceWaitable = true;
        } else if (err == VK_ERROR_OUT_OF_DATE_KHR) {
            return QRhi::FrameOpSwapChainOutOfDate;
        } else {
            if (checkDeviceLost(err))
                return QRhi::FrameOpDeviceLost;
            else
                qWarning("Failed to acquire next swapchain image: %d", err);
            return QRhi::FrameOpError;
        }
    }

    // Make sure the previous commands for the same image have finished. (note
    // that this is based on the fence from the command buffer submit, nothing
    // to do with the Present)
    //
    // Do this also for any other swapchain's commands with the same frame slot
    // While this reduces concurrency, it keeps resource usage safe: swapchain
    // A starting its frame 0, followed by swapchain B starting its own frame 0
    // will make B wait for A's frame 0 commands, so if a resource is written
    // in B's frame or when B checks for pending resource releases, that won't
    // mess up A's in-flight commands (as they are not in flight anymore).
    waitCommandCompletion(swapChainD->currentFrame);

    // build new draw command buffer
    QVkSwapChain::ImageResources &image(swapChainD->imageRes[swapChainD->currentImage]);
    QRhi::FrameOpResult cbres = startCommandBuffer(&image.cmdBuf);
    if (cbres != QRhi::FrameOpSuccess)
        return cbres;

    swapChainD->cbWrapper.cb = image.cmdBuf;

    swapChainD->rtWrapper.d.fb = image.fb;

    currentFrameSlot = swapChainD->currentFrame;
    currentSwapChain = swapChainD;
    if (swapChainD->ds)
        swapChainD->ds->lastActiveFrameSlot = currentFrameSlot;

    prepareNewFrame(&swapChainD->cbWrapper);

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiVulkan::endNonWrapperFrame(QRhiSwapChain *swapChain)
{
    QVkSwapChain *swapChainD = QRHI_RES(QVkSwapChain, swapChain);

    prepareFrameEnd();

    QVkSwapChain::FrameResources &frame(swapChainD->frameRes[swapChainD->currentFrame]);
    QVkSwapChain::ImageResources &image(swapChainD->imageRes[swapChainD->currentImage]);

    if (!image.presentableLayout) {
        // was used in a readback as transfer source, go back to presentable layout
        VkImageMemoryBarrier presTrans;
        memset(&presTrans, 0, sizeof(presTrans));
        presTrans.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        presTrans.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        presTrans.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        presTrans.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        presTrans.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presTrans.image = image.image;
        presTrans.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        presTrans.subresourceRange.levelCount = presTrans.subresourceRange.layerCount = 1;
        df->vkCmdPipelineBarrier(image.cmdBuf,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 0, 0, nullptr, 0, nullptr,
                                 1, &presTrans);
        image.presentableLayout = true;
    }

    // stop recording and submit to the queue
    Q_ASSERT(!image.cmdFenceWaitable);
    QRhi::FrameOpResult submitres = endAndSubmitCommandBuffer(image.cmdBuf,
                                                              image.cmdFence,
                                                              frame.imageSemWaitable ? &frame.imageSem : nullptr,
                                                              &frame.drawSem);
    if (submitres != QRhi::FrameOpSuccess)
        return submitres;

    frame.imageSemWaitable = false;
    image.cmdFenceWaitable = true;

    // add the Present to the queue
    VkPresentInfoKHR presInfo;
    memset(&presInfo, 0, sizeof(presInfo));
    presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presInfo.swapchainCount = 1;
    presInfo.pSwapchains = &swapChainD->sc;
    presInfo.pImageIndices = &swapChainD->currentImage;
    presInfo.waitSemaphoreCount = 1;
    presInfo.pWaitSemaphores = &frame.drawSem; // gfxQueueFamilyIdx == presQueueFamilyIdx ? &frame.drawSem : &frame.presTransSem;

    VkResult err = vkQueuePresentKHR(gfxQueue, &presInfo);
    if (err != VK_SUCCESS) {
        if (err == VK_ERROR_OUT_OF_DATE_KHR) {
            return QRhi::FrameOpSwapChainOutOfDate;
        } else if (err != VK_SUBOPTIMAL_KHR) {
            if (checkDeviceLost(err))
                return QRhi::FrameOpDeviceLost;
            else
                qWarning("Failed to present: %d", err);
            return QRhi::FrameOpError;
        }
    }

    frame.imageAcquired = false;

    swapChainD->currentFrame = (swapChainD->currentFrame + 1) % QVK_FRAMES_IN_FLIGHT;

    currentSwapChain = nullptr;

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiVulkan::beginOffscreenFrame(QRhiCommandBuffer **cb)
{
    QRhi::FrameOpResult cbres = startCommandBuffer(&ofr.cbWrapper.cb);
    if (cbres != QRhi::FrameOpSuccess)
        return cbres;

    // Switch to the next slot manually. Swapchains do not know about this
    // which is good. So for example a - unusual but possible - onscreen,
    // onscreen, offscreen, onscreen, onscreen, onscreen sequence of
    // begin/endFrame leads to 0, 1, 0, 0, 1, 0. This works because the
    // offscreen frame is synchronous in the sense that we wait for execution
    // to complete in endFrame, and so no resources used in that frame are busy
    // anymore in the next frame.
    currentFrameSlot = (currentFrameSlot + 1) % QVK_FRAMES_IN_FLIGHT;
    // except that this gets complicated with multiple swapchains so make sure
    // any pending commands have finished for the frame slot we are going to use
    if (swapchains.count() > 1)
        waitCommandCompletion(currentFrameSlot);

    prepareNewFrame(&ofr.cbWrapper);
    ofr.active = true;

    *cb = &ofr.cbWrapper;
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiVulkan::endOffscreenFrame()
{
    Q_ASSERT(ofr.active);

    ofr.active = false;
    prepareFrameEnd();

    if (!ofr.cmdFence) {
        VkFenceCreateInfo fenceInfo;
        memset(&fenceInfo, 0, sizeof(fenceInfo));
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkResult err = df->vkCreateFence(dev, &fenceInfo, nullptr, &ofr.cmdFence);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create command buffer fence: %d", err);
            return QRhi::FrameOpError;
        }
    }

    QRhi::FrameOpResult submitres = endAndSubmitCommandBuffer(ofr.cbWrapper.cb, ofr.cmdFence, nullptr, nullptr);
    if (submitres != QRhi::FrameOpSuccess)
        return submitres;

    // wait for completion
    df->vkWaitForFences(dev, 1, &ofr.cmdFence, VK_TRUE, UINT64_MAX);
    df->vkResetFences(dev, 1, &ofr.cmdFence);

    // Here we know that executing the host-side reads for this (or any
    // previous) frame is safe since we waited for completion above.
    finishActiveReadbacks(true);

    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult QRhiVulkan::finish()
{
    Q_ASSERT(!inPass);

    QVkSwapChain *swapChainD = nullptr;
    if (inFrame) {
        // There is either a swapchain or an offscreen frame on-going.
        // End command recording and submit what we have.
        VkCommandBuffer cb;
        if (ofr.active) {
            Q_ASSERT(!currentSwapChain);
            cb = ofr.cbWrapper.cb;
        } else {
            Q_ASSERT(currentSwapChain);
            swapChainD = currentSwapChain;
            if (swapChainD->wrapWindow) {
                // QVulkanWindow's command buffer cannot be submitted and then recreated by us
                qWarning("finish() within a frame is not supported in combination with QVulkanWindow");
                return QRhi::FrameOpError;
            }
            cb = swapChainD->imageRes[swapChainD->currentImage].cmdBuf;
        }
        QRhi::FrameOpResult submitres = endAndSubmitCommandBuffer(cb, VK_NULL_HANDLE, nullptr, nullptr);
        if (submitres != QRhi::FrameOpSuccess)
            return submitres;
    }

    df->vkQueueWaitIdle(gfxQueue);

    if (inFrame) {
        // Allocate and begin recording on a new command buffer.
        if (ofr.active)
            startCommandBuffer(&ofr.cbWrapper.cb);
        else
            startCommandBuffer(&swapChainD->imageRes[swapChainD->currentImage].cmdBuf);
    }

    executeDeferredReleases(true);

    finishActiveReadbacks(true);

    return QRhi::FrameOpSuccess;
}

void QRhiVulkan::activateTextureRenderTarget(QRhiCommandBuffer *, QRhiTextureRenderTarget *rt)
{
    QVkTextureRenderTarget *rtD = QRHI_RES(QVkTextureRenderTarget, rt);
    rtD->lastActiveFrameSlot = currentFrameSlot;
    rtD->d.rp->lastActiveFrameSlot = currentFrameSlot;
    // the renderpass will implicitly transition so no barrier needed here
    for (const QRhiTextureRenderTargetDescription::ColorAttachment &colorAttachment : qAsConst(rtD->m_desc.colorAttachments)) {
        QVkTexture *texD = QRHI_RES(QVkTexture, colorAttachment.texture);
        QVkRenderBuffer *rbD = QRHI_RES(QVkRenderBuffer, colorAttachment.renderBuffer);
        if (texD) {
            texD->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            texD->lastActiveFrameSlot = currentFrameSlot;
        } else if (rbD) {
            rbD->backingTexture->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            rbD->lastActiveFrameSlot = currentFrameSlot;
        }
    }
    if (rtD->m_desc.depthTexture) {
        QVkTexture *depthTexD = QRHI_RES(QVkTexture, rtD->m_desc.depthTexture);
        depthTexD->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthTexD->lastActiveFrameSlot = currentFrameSlot;
    }
}

void QRhiVulkan::deactivateTextureRenderTarget(QRhiCommandBuffer *, QRhiTextureRenderTarget *rt)
{
    QVkTextureRenderTarget *rtD = QRHI_RES(QVkTextureRenderTarget, rt);
    // already in the right layout when the renderpass ends
    for (const QRhiTextureRenderTargetDescription::ColorAttachment &colorAttachment : qAsConst(rtD->m_desc.colorAttachments)) {
        QVkTexture *texD = QRHI_RES(QVkTexture, colorAttachment.texture);
        QVkRenderBuffer *rbD = QRHI_RES(QVkRenderBuffer, colorAttachment.renderBuffer);
        if (texD)
            texD->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        else if (rbD)
            rbD->backingTexture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (rtD->m_desc.depthTexture)
        QRHI_RES(QVkTexture, rtD->m_desc.depthTexture)->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void QRhiVulkan::prepareNewFrame(QRhiCommandBuffer *cb)
{
    Q_ASSERT(!inFrame);
    inFrame = true;

    // Now is the time to do things for frame N-F, where N is the current one,
    // F is QVK_FRAMES_IN_FLIGHT, because only here it is guaranteed that that
    // frame has completed on the GPU (due to the fence wait in beginFrame). To
    // decide if something is safe to handle now a simple "lastActiveFrameSlot
    // == currentFrameSlot" is sufficient (remember that e.g. with F==2
    // currentFrameSlot goes 0, 1, 0, 1, 0, ...)
    //
    // With multiple swapchains on the same QRhi things get more convoluted
    // (and currentFrameSlot strictly alternating is not true anymore) but
    // beginNonWrapperFrame() solves that by blocking as necessary so the rest
    // here is safe regardless.

    executeDeferredReleases();

    QRHI_RES(QVkCommandBuffer, cb)->resetState();

    finishActiveReadbacks(); // last, in case the readback-completed callback issues rhi calls
}

void QRhiVulkan::prepareFrameEnd()
{
    Q_ASSERT(inFrame);
    inFrame = false;
    ++finishedFrameCount;
}

void QRhiVulkan::resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(inFrame && !inPass);

    enqueueResourceUpdates(cb, resourceUpdates);
}

void QRhiVulkan::beginPass(QRhiCommandBuffer *cb,
                           QRhiRenderTarget *rt,
                           const QRhiColorClearValue &colorClearValue,
                           const QRhiDepthStencilClearValue &depthStencilClearValue,
                           QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(!inPass);

    if (resourceUpdates)
        enqueueResourceUpdates(cb, resourceUpdates);

    QVkRenderTargetData *rtD = nullptr;
    switch (rt->type()) {
    case QRhiRenderTarget::RtRef:
        rtD = &QRHI_RES(QVkReferenceRenderTarget, rt)->d;
        rtD->rp->lastActiveFrameSlot = currentFrameSlot;
        break;
    case QRhiRenderTarget::RtTexture:
    {
        QVkTextureRenderTarget *rtTex = QRHI_RES(QVkTextureRenderTarget, rt);
        rtD = &rtTex->d;
        activateTextureRenderTarget(cb, rtTex);
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    // No copies or image layout transitions allowed after this point (up until
    // endPass) as we are going to begin the renderpass.

    QVkCommandBuffer *cbD = QRHI_RES(QVkCommandBuffer, cb);
    cbD->currentTarget = rt;

    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = rtD->rp->rp;
    rpBeginInfo.framebuffer = rtD->fb;
    rpBeginInfo.renderArea.extent.width = rtD->pixelSize.width();
    rpBeginInfo.renderArea.extent.height = rtD->pixelSize.height();

    QVarLengthArray<VkClearValue, 4> cvs;
    for (int i = 0; i < rtD->colorAttCount; ++i) {
        VkClearValue cv;
        cv.color = { { colorClearValue.rgba.x(), colorClearValue.rgba.y(), colorClearValue.rgba.z(), colorClearValue.rgba.w() } };
        cvs.append(cv);
    }
    for (int i = 0; i < rtD->dsAttCount; ++i) {
        VkClearValue cv;
        cv.depthStencil = { depthStencilClearValue.d, depthStencilClearValue.s };
        cvs.append(cv);
    }
    for (int i = 0; i < rtD->resolveAttCount; ++i) {
        VkClearValue cv;
        cv.color = { { colorClearValue.rgba.x(), colorClearValue.rgba.y(), colorClearValue.rgba.z(), colorClearValue.rgba.w() } };
        cvs.append(cv);
    }
    rpBeginInfo.clearValueCount = cvs.count();
    rpBeginInfo.pClearValues = cvs.constData();

    df->vkCmdBeginRenderPass(cbD->cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    inPass = true;
}

void QRhiVulkan::endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    Q_ASSERT(inPass);
    QVkCommandBuffer *cbD = QRHI_RES(QVkCommandBuffer, cb);
    df->vkCmdEndRenderPass(cbD->cb);
    inPass = false;

    if (cbD->currentTarget->type() == QRhiRenderTarget::RtTexture)
        deactivateTextureRenderTarget(cb, static_cast<QRhiTextureRenderTarget *>(cbD->currentTarget));

    cbD->currentTarget = nullptr;

    if (resourceUpdates)
        enqueueResourceUpdates(cb, resourceUpdates);
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

void QRhiVulkan::updateShaderResourceBindings(QRhiShaderResourceBindings *srb, int descSetIdx)
{
    QVkShaderResourceBindings *srbD = QRHI_RES(QVkShaderResourceBindings, srb);

    QVarLengthArray<VkDescriptorBufferInfo, 4> bufferInfos;
    QVarLengthArray<VkDescriptorImageInfo, 4> imageInfos;
    QVarLengthArray<VkWriteDescriptorSet, 8> writeInfos;

    const bool updateAll = descSetIdx < 0;
    int frameSlot = updateAll ? 0 : descSetIdx;
    while (frameSlot < (updateAll ? QVK_FRAMES_IN_FLIGHT : descSetIdx + 1)) {
        srbD->boundResourceData[frameSlot].resize(srbD->m_bindings.count());
        for (int i = 0, ie = srbD->m_bindings.count(); i != ie; ++i) {
            const QRhiShaderResourceBinding &b(srbD->m_bindings[i]);
            QVkShaderResourceBindings::BoundResourceData &bd(srbD->boundResourceData[frameSlot][i]);

            VkWriteDescriptorSet writeInfo;
            memset(&writeInfo, 0, sizeof(writeInfo));
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.dstSet = srbD->descSets[frameSlot];
            writeInfo.dstBinding = b.binding;
            writeInfo.descriptorCount = 1;

            switch (b.type) {
            case QRhiShaderResourceBinding::UniformBuffer:
            {
                writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                QRhiBuffer *buf = b.ubuf.buf;
                QVkBuffer *bufD = QRHI_RES(QVkBuffer, buf);
                bd.ubuf.generation = bufD->generation;
                VkDescriptorBufferInfo bufInfo;
                bufInfo.buffer = bufD->m_type == QRhiBuffer::Dynamic ? bufD->buffers[frameSlot] : bufD->buffers[0];
                bufInfo.offset = b.ubuf.offset;
                bufInfo.range = b.ubuf.maybeSize ? b.ubuf.maybeSize : bufD->m_size;
                // be nice and assert when we know the vulkan device would die a horrible death due to non-aligned reads
                Q_ASSERT(aligned(bufInfo.offset, ubufAlign) == bufInfo.offset);
                bufferInfos.append(bufInfo);
                writeInfo.pBufferInfo = &bufferInfos.last();
            }
                break;
            case QRhiShaderResourceBinding::SampledTexture:
            {
                writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bd.stex.texGeneration = QRHI_RES(QVkTexture, b.stex.tex)->generation;
                bd.stex.samplerGeneration = QRHI_RES(QVkSampler, b.stex.sampler)->generation;
                VkDescriptorImageInfo imageInfo;
                imageInfo.sampler = QRHI_RES(QVkSampler, b.stex.sampler)->sampler;
                imageInfo.imageView = QRHI_RES(QVkTexture, b.stex.tex)->imageView;
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

    QVkBuffer *bufD = QRHI_RES(QVkBuffer, buf);
    int dstAccess = 0;
    VkPipelineStageFlagBits dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

    if (bufD->m_usage.testFlag(QRhiBuffer::VertexBuffer))
        dstAccess |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    if (bufD->m_usage.testFlag(QRhiBuffer::IndexBuffer))
        dstAccess |= VK_ACCESS_INDEX_READ_BIT;
    if (bufD->m_usage.testFlag(QRhiBuffer::UniformBuffer)) {
        dstAccess |= VK_ACCESS_UNIFORM_READ_BIT;
        dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; // don't know where it's used, assume vertex to be safe
    }

    bufMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufMemBarrier.dstAccessMask = dstAccess;
    bufMemBarrier.buffer = QRHI_RES(QVkBuffer, buf)->buffers[0];
    bufMemBarrier.size = bufD->m_size;

    df->vkCmdPipelineBarrier(QRHI_RES(QVkCommandBuffer, cb)->cb, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage,
                             0, 0, nullptr, 1, &bufMemBarrier, 0, nullptr);
}

void QRhiVulkan::imageSubResBarrier(QRhiCommandBuffer *cb, QRhiTexture *tex,
                                    VkImageLayout oldLayout, VkImageLayout newLayout,
                                    VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                    int startLayer, int layerCount,
                                    int startLevel, int levelCount)
{
    QVkTexture *texD = QRHI_RES(QVkTexture, tex);

    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = startLevel;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.subresourceRange.baseArrayLayer = startLayer;
    barrier.subresourceRange.layerCount = layerCount;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.image = texD->image;

    df->vkCmdPipelineBarrier(QRHI_RES(QVkCommandBuffer, cb)->cb,
                             srcStage,
                             dstStage,
                             0, 0, nullptr, 0, nullptr,
                             1, &barrier);
}

void QRhiVulkan::imageBarrier(QRhiCommandBuffer *cb, QRhiTexture *tex,
                              VkImageLayout newLayout,
                              VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                              VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    QVkTexture *texD = QRHI_RES(QVkTexture, tex);

    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = texD->mipLevelCount;
    barrier.subresourceRange.layerCount = texD->m_flags.testFlag(QRhiTexture::CubeMap) ? 6 : 1;

    barrier.oldLayout = texD->layout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.image = texD->image;

    df->vkCmdPipelineBarrier(QRHI_RES(QVkCommandBuffer, cb)->cb,
                             srcStage,
                             dstStage,
                             0, 0, nullptr, 0, nullptr,
                             1, &barrier);

    texD->layout = newLayout;
}

void QRhiVulkan::prepareForTransferDest(QRhiCommandBuffer *cb, QVkTexture *texD)
{
    if (texD->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        if (texD->layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
            imageBarrier(cb, texD,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         0, VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        } else {
            imageBarrier(cb, texD,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        }
    }
}

void QRhiVulkan::prepareForTransferSrc(QRhiCommandBuffer *cb, QVkTexture *texD)
{
    if (texD->layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        Q_ASSERT(texD->m_flags.testFlag(QRhiTexture::UsedAsTransferSource)
                 || texD->m_flags.testFlag(QRhiTexture::UsedWithGenerateMips));
        // assume the texture was written (so block up to color output, not just fragment)
        imageBarrier(cb, texD,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
}

void QRhiVulkan::finishTransferDest(QRhiCommandBuffer *cb, QVkTexture *texD)
{
    imageBarrier(cb, texD,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void QRhiVulkan::finishTransferSrc(QRhiCommandBuffer *cb, QVkTexture *texD)
{
    imageBarrier(cb, texD,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void QRhiVulkan::enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates)
{
    QVkCommandBuffer *cbD = QRHI_RES(QVkCommandBuffer, cb);
    QRhiResourceUpdateBatchPrivate *ud = QRhiResourceUpdateBatchPrivate::get(resourceUpdates);
    QRhiProfilerPrivate *rhiP = profilerPrivateOrNull();

    for (const QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate &u : ud->dynamicBufferUpdates) {
        QVkBuffer *bufD = QRHI_RES(QVkBuffer, u.buf);
        Q_ASSERT(bufD->m_type == QRhiBuffer::Dynamic);
        for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
            bufD->pendingDynamicUpdates[i].append(u);
    }

    for (const QRhiResourceUpdateBatchPrivate::StaticBufferUpload &u : ud->staticBufferUploads) {
        QVkBuffer *bufD = QRHI_RES(QVkBuffer, u.buf);
        Q_ASSERT(bufD->m_type != QRhiBuffer::Dynamic);
        Q_ASSERT(u.offset + u.data.size() <= bufD->m_size);

        if (!bufD->stagingBuffers[currentFrameSlot]) {
            VkBufferCreateInfo bufferInfo;
            memset(&bufferInfo, 0, sizeof(bufferInfo));
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufD->m_size;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo allocInfo;
            memset(&allocInfo, 0, sizeof(allocInfo));
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

            VmaAllocation allocation;
            VkResult err = vmaCreateBuffer(toVmaAllocator(allocator), &bufferInfo, &allocInfo,
                                           &bufD->stagingBuffers[currentFrameSlot], &allocation, nullptr);
            if (err == VK_SUCCESS) {
                bufD->stagingAllocations[currentFrameSlot] = allocation;
                QRHI_PROF_F(newBufferStagingArea(bufD, currentFrameSlot, bufD->m_size));
            } else {
                qWarning("Failed to create staging buffer of size %d: %d", bufD->m_size, err);
                continue;
            }
        }

        void *p = nullptr;
        VmaAllocation a = toVmaAllocation(bufD->stagingAllocations[currentFrameSlot]);
        VkResult err = vmaMapMemory(toVmaAllocator(allocator), a, &p);
        if (err != VK_SUCCESS) {
            qWarning("Failed to map buffer: %d", err);
            continue;
        }
        memcpy(static_cast<uchar *>(p) + u.offset, u.data.constData(), u.data.size());
        vmaUnmapMemory(toVmaAllocator(allocator), a);
        vmaFlushAllocation(toVmaAllocator(allocator), a, u.offset, u.data.size());

        VkBufferCopy copyInfo;
        memset(&copyInfo, 0, sizeof(copyInfo));
        copyInfo.srcOffset = u.offset;
        copyInfo.dstOffset = u.offset;
        copyInfo.size = u.data.size();

        df->vkCmdCopyBuffer(cbD->cb, bufD->stagingBuffers[currentFrameSlot], bufD->buffers[0], 1, &copyInfo);
        bufferBarrier(cb, u.buf);
        bufD->lastActiveFrameSlot = currentFrameSlot;

        if (bufD->m_type == QRhiBuffer::Immutable) {
            QRhiVulkan::DeferredReleaseEntry e;
            e.type = QRhiVulkan::DeferredReleaseEntry::StagingBuffer;
            e.lastActiveFrameSlot = currentFrameSlot;
            e.stagingBuffer.stagingBuffer = bufD->stagingBuffers[currentFrameSlot];
            e.stagingBuffer.stagingAllocation = bufD->stagingAllocations[currentFrameSlot];
            bufD->stagingBuffers[currentFrameSlot] = VK_NULL_HANDLE;
            bufD->stagingAllocations[currentFrameSlot] = nullptr;
            releaseQueue.append(e);
            QRHI_PROF_F(releaseBufferStagingArea(bufD, currentFrameSlot));
        }
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureUpload &u : ud->textureUploads) {
        if (u.desc.layers.isEmpty() || u.desc.layers[0].mipImages.isEmpty())
            continue;

        QVkTexture *utexD = QRHI_RES(QVkTexture, u.tex);
        VkDeviceSize stagingSize = 0;

        for (int layer = 0, layerCount = u.desc.layers.count(); layer != layerCount; ++layer) {
            const QRhiTextureUploadDescription::Layer &layerDesc(u.desc.layers[layer]);
            Q_ASSERT(layerDesc.mipImages.count() == 1 || utexD->m_flags.testFlag(QRhiTexture::MipMapped));
            for (int level = 0, levelCount = layerDesc.mipImages.count(); level != levelCount; ++level) {
                const QRhiTextureUploadDescription::Layer::MipLevel mipDesc(layerDesc.mipImages[level]);
                const qsizetype imageSizeBytes = mipDesc.image.isNull() ?
                            mipDesc.compressedData.size() : mipDesc.image.sizeInBytes();
                if (imageSizeBytes > 0)
                    stagingSize += aligned(imageSizeBytes, texbufAlign);
            }
        }

        if (!utexD->stagingBuffers[currentFrameSlot]) {
            VkBufferCreateInfo bufferInfo;
            memset(&bufferInfo, 0, sizeof(bufferInfo));
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = stagingSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo allocInfo;
            memset(&allocInfo, 0, sizeof(allocInfo));
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            VmaAllocation allocation;
            VkResult err = vmaCreateBuffer(toVmaAllocator(allocator), &bufferInfo, &allocInfo,
                                           &utexD->stagingBuffers[currentFrameSlot], &allocation, nullptr);
            if (err != VK_SUCCESS) {
                qWarning("Failed to create image staging buffer of size %d: %d", int(stagingSize), err);
                continue;
            }
            utexD->stagingAllocations[currentFrameSlot] = allocation;
        }

        QVarLengthArray<VkBufferImageCopy, 4> copyInfos;
        size_t curOfs = 0;
        void *mp = nullptr;
        VmaAllocation a = toVmaAllocation(utexD->stagingAllocations[currentFrameSlot]);
        VkResult err = vmaMapMemory(toVmaAllocator(allocator), a, &mp);
        if (err != VK_SUCCESS) {
            qWarning("Failed to map image data: %d", err);
            continue;
        }
        QVector<QImage> tempImages; // yes, we rely heavily on implicit sharing in QImage
        for (int layer = 0, layerCount = u.desc.layers.count(); layer != layerCount; ++layer) {
            const QRhiTextureUploadDescription::Layer &layerDesc(u.desc.layers[layer]);
            for (int level = 0, levelCount = layerDesc.mipImages.count(); level != levelCount; ++level) {
                const QRhiTextureUploadDescription::Layer::MipLevel mipDesc(layerDesc.mipImages[level]);
                qsizetype imageSizeBytes = 0;
                const void *src = nullptr;
                VkBufferImageCopy copyInfo;
                memset(&copyInfo, 0, sizeof(copyInfo));
                copyInfo.bufferOffset = curOfs;
                copyInfo.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyInfo.imageSubresource.mipLevel = level;
                copyInfo.imageSubresource.baseArrayLayer = layer;
                copyInfo.imageSubresource.layerCount = 1;
                copyInfo.imageExtent.depth = 1;

                const int dx = mipDesc.destinationTopLeft.x();
                const int dy = mipDesc.destinationTopLeft.y();
                if (!mipDesc.image.isNull()) {
                    imageSizeBytes = mipDesc.image.sizeInBytes();
                    if (imageSizeBytes > 0) {
                        QImage img = mipDesc.image;
                        int w = img.width();
                        int h = img.height();
                        src = img.constBits();
                        copyInfo.bufferRowLength = w; // this is in pixels, not bytes
                        if (!mipDesc.sourceSize.isEmpty() || !mipDesc.sourceTopLeft.isNull()) {
                            const int sx = mipDesc.sourceTopLeft.x();
                            const int sy = mipDesc.sourceTopLeft.y();
                            if (!mipDesc.sourceSize.isEmpty()) {
                                w = mipDesc.sourceSize.width();
                                h = mipDesc.sourceSize.height();
                            }
                            if (img.depth() == 32) {
                                src = img.constBits() + sy * img.bytesPerLine() + sx * 4;
                                // bufferRowLength remains set to the original image's width
                            } else {
                                img = img.copy(sx, sy, w, h);
                                src = img.constBits();
                                copyInfo.bufferRowLength = w;
                                tempImages.append(img); // keep the new, temporary image alive until the vkCmdCopy
                            }
                        }
                        copyInfo.imageOffset.x = dx;
                        copyInfo.imageOffset.y = dy;
                        copyInfo.imageExtent.width = w;
                        copyInfo.imageExtent.height = h;
                        copyInfos.append(copyInfo);
                    }
                } else {
                    imageSizeBytes = mipDesc.compressedData.size();
                    if (imageSizeBytes > 0) {
                        src = mipDesc.compressedData.constData();
                        const int subresw = qFloor(float(qMax(1, utexD->m_pixelSize.width() >> level)));
                        const int subresh = qFloor(float(qMax(1, utexD->m_pixelSize.height() >> level)));
                        int w, h;
                        if (mipDesc.sourceSize.isEmpty()) {
                            w = subresw;
                            h = subresh;
                        } else {
                            w = mipDesc.sourceSize.width();
                            h = mipDesc.sourceSize.height();
                        }
                        QSize blockDim;
                        compressedFormatInfo(utexD->m_format, QSize(w, h), nullptr, nullptr, &blockDim);
                        // x and y must be multiples of the block width and height
                        copyInfo.imageOffset.x = aligned(dx, blockDim.width());
                        copyInfo.imageOffset.y = aligned(dy, blockDim.height());
                        // width and height must be multiples of the block width and height
                        // or x + width and y + height must equal the subresource width and height
                        copyInfo.imageExtent.width = dx + w == subresw ? w : aligned(w, blockDim.width());
                        copyInfo.imageExtent.height = dy + h == subresh ? h : aligned(h, blockDim.height());
                        copyInfos.append(copyInfo);
                    }
                }

                if (imageSizeBytes > 0) {
                    memcpy(reinterpret_cast<char *>(mp) + curOfs, src, imageSizeBytes);
                    curOfs += aligned(imageSizeBytes, texbufAlign);
                }
            }
        }
        vmaUnmapMemory(toVmaAllocator(allocator), a);
        vmaFlushAllocation(toVmaAllocator(allocator), a, 0, stagingSize);

        prepareForTransferDest(cb, utexD);

        df->vkCmdCopyBufferToImage(cbD->cb, utexD->stagingBuffers[currentFrameSlot],
                                   utexD->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   copyInfos.count(), copyInfos.constData());
        utexD->lastActiveFrameSlot = currentFrameSlot;

        if (!utexD->m_flags.testFlag(QRhiTexture::ChangesFrequently)) {
            QRhiVulkan::DeferredReleaseEntry e;
            e.type = QRhiVulkan::DeferredReleaseEntry::StagingBuffer;
            e.lastActiveFrameSlot = currentFrameSlot;
            e.stagingBuffer.stagingBuffer = utexD->stagingBuffers[currentFrameSlot];
            e.stagingBuffer.stagingAllocation = utexD->stagingAllocations[currentFrameSlot];
            utexD->stagingBuffers[currentFrameSlot] = VK_NULL_HANDLE;
            utexD->stagingAllocations[currentFrameSlot] = nullptr;
            releaseQueue.append(e);
        }

        finishTransferDest(cb, utexD);
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureCopy &u : ud->textureCopies) {
        Q_ASSERT(u.src && u.dst);
        QVkTexture *srcD = QRHI_RES(QVkTexture, u.src);
        QVkTexture *dstD = QRHI_RES(QVkTexture, u.dst);

        VkImageCopy region;
        memset(&region, 0, sizeof(region));

        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.mipLevel = u.desc.sourceLevel;
        region.srcSubresource.baseArrayLayer = u.desc.sourceLayer;
        region.srcSubresource.layerCount = 1;

        region.srcOffset.x = u.desc.sourceTopLeft.x();
        region.srcOffset.y = u.desc.sourceTopLeft.y();

        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.mipLevel = u.desc.destinationLevel;
        region.dstSubresource.baseArrayLayer = u.desc.destinationLayer;
        region.dstSubresource.layerCount = 1;

        region.dstOffset.x = u.desc.destinationTopLeft.x();
        region.dstOffset.y = u.desc.destinationTopLeft.y();

        const QSize size = u.desc.pixelSize.isEmpty() ? srcD->m_pixelSize : u.desc.pixelSize;
        region.extent.width = size.width();
        region.extent.height = size.height();
        region.extent.depth = 1;

        prepareForTransferSrc(cb, srcD);
        prepareForTransferDest(cb, dstD);

        df->vkCmdCopyImage(QRHI_RES(QVkCommandBuffer, cb)->cb,
                           srcD->image, srcD->layout,
                           dstD->image, dstD->layout,
                           1, &region);

        finishTransferSrc(cb, srcD);
        finishTransferDest(cb, dstD);
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureRead &u : ud->textureReadbacks) {
        ActiveReadback aRb;
        aRb.activeFrameSlot = currentFrameSlot;
        aRb.desc = u.rb;
        aRb.result = u.result;

        QVkTexture *texD = QRHI_RES(QVkTexture, aRb.desc.texture);
        QVkSwapChain *swapChainD = nullptr;
        if (texD) {
            if (texD->samples > VK_SAMPLE_COUNT_1_BIT) {
                qWarning("Multisample texture cannot be read back");
                continue;
            }
            aRb.pixelSize = texD->m_pixelSize;
            if (u.rb.level > 0) {
                aRb.pixelSize.setWidth(qFloor(float(qMax(1, aRb.pixelSize.width() >> u.rb.level))));
                aRb.pixelSize.setHeight(qFloor(float(qMax(1, aRb.pixelSize.height() >> u.rb.level))));
            }
            aRb.format = texD->m_format;
        } else {
            Q_ASSERT(currentSwapChain);
            swapChainD = QRHI_RES(QVkSwapChain, currentSwapChain);
            if (!swapChainD->supportsReadback) {
                qWarning("Swapchain does not support readback");
                continue;
            }
            aRb.pixelSize = swapChainD->pixelSize;
            aRb.format = colorTextureFormatFromVkFormat(swapChainD->colorFormat, nullptr);
            if (aRb.format == QRhiTexture::UnknownFormat)
                continue;

            // Multisample swapchains need nothing special since resolving
            // happens when ending a renderpass.
        }
        textureFormatInfo(aRb.format, aRb.pixelSize, nullptr, &aRb.bufSize);

        // Create a host visible buffer.
        VkBufferCreateInfo bufferInfo;
        memset(&bufferInfo, 0, sizeof(bufferInfo));
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = aRb.bufSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo;
        memset(&allocInfo, 0, sizeof(allocInfo));
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;

        VmaAllocation allocation;
        VkResult err = vmaCreateBuffer(toVmaAllocator(allocator), &bufferInfo, &allocInfo, &aRb.buf, &allocation, nullptr);
        if (err == VK_SUCCESS) {
            aRb.bufAlloc = allocation;
        } else {
            qWarning("Failed to create readback buffer of size %u: %d", aRb.bufSize, err);
            continue;
        }

        // Copy from the (optimal and not host visible) image into the buffer.
        VkBufferImageCopy copyDesc;
        memset(&copyDesc, 0, sizeof(copyDesc));
        copyDesc.bufferOffset = 0;
        copyDesc.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyDesc.imageSubresource.mipLevel = aRb.desc.level;
        copyDesc.imageSubresource.baseArrayLayer = aRb.desc.layer;
        copyDesc.imageSubresource.layerCount = 1;
        copyDesc.imageExtent.width = aRb.pixelSize.width();
        copyDesc.imageExtent.height = aRb.pixelSize.height();
        copyDesc.imageExtent.depth = 1;

        if (texD) {
            prepareForTransferSrc(cb, texD);
            df->vkCmdCopyImageToBuffer(cbD->cb, texD->image, texD->layout, aRb.buf, 1, &copyDesc);
            finishTransferSrc(cb, texD);
        } else {
            // use the swapchain image
            VkImage image = swapChainD->imageRes[swapChainD->currentImage].image;
            VkImageMemoryBarrier barrier;
            memset(&barrier, 0, sizeof(barrier));
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.image = image;
            df->vkCmdPipelineBarrier(cbD->cb,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0, 0, nullptr, 0, nullptr,
                                     1, &barrier);
            swapChainD->imageRes[swapChainD->currentImage].presentableLayout = false;

            df->vkCmdCopyImageToBuffer(cbD->cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, aRb.buf, 1, &copyDesc);
        }

        activeReadbacks.append(aRb);
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureMipGen &u : ud->textureMipGens) {
        QVkTexture *utexD = QRHI_RES(QVkTexture, u.tex);
        Q_ASSERT(utexD->m_flags.testFlag(QRhiTexture::UsedWithGenerateMips));
        int w = utexD->m_pixelSize.width();
        int h = utexD->m_pixelSize.height();

        prepareForTransferSrc(cb, utexD);

        const uint layerCount = utexD->m_flags.testFlag(QRhiTexture::CubeMap) ? 6 : 1;
        for (uint level = 1; level < utexD->mipLevelCount; ++level) {
            if (level > 1) {
                imageSubResBarrier(cb, utexD,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   0, layerCount, level - 1, 1);
            }

            imageSubResBarrier(cb, utexD,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               0, layerCount, level, 1);

            VkImageBlit region;
            memset(&region, 0, sizeof(region));

            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.mipLevel = level - 1;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = layerCount;

            region.srcOffsets[1].x = w;
            region.srcOffsets[1].y = h;
            region.srcOffsets[1].z = 1;

            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.mipLevel = level;
            region.dstSubresource.baseArrayLayer = 0;
            region.dstSubresource.layerCount = layerCount;

            region.dstOffsets[1].x = w >> 1;
            region.dstOffsets[1].y = h >> 1;
            region.dstOffsets[1].z = 1;

            df->vkCmdBlitImage(cbD->cb,
                               utexD->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               utexD->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &region,
                               VK_FILTER_LINEAR);

            w >>= 1;
            h >>= 1;

            if (level == utexD->mipLevelCount - 1) {
                imageSubResBarrier(cb, utexD,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   0, layerCount, level, 1);
            }
        }

        finishTransferDest(cb, utexD);
    }

    for (const QRhiResourceUpdateBatchPrivate::TexturePrepare &u : ud->texturePrepares) {
        if (u.flags.testFlag(QRhiResourceUpdateBatch::TextureRead)) {
            QVkTexture *utexD = QRHI_RES(QVkTexture, u.tex);
            if (utexD->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                finishTransferDest(cb, utexD);
        }
    }

    ud->free();
}

void QRhiVulkan::executeBufferHostWritesForCurrentFrame(QVkBuffer *bufD)
{
    QVector<QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate> &updates(bufD->pendingDynamicUpdates[currentFrameSlot]);
    if (updates.isEmpty())
        return;

    Q_ASSERT(bufD->m_type == QRhiBuffer::Dynamic);
    void *p = nullptr;
    VmaAllocation a = toVmaAllocation(bufD->allocations[currentFrameSlot]);
    VkResult err = vmaMapMemory(toVmaAllocator(allocator), a, &p);
    if (err != VK_SUCCESS) {
        qWarning("Failed to map buffer: %d", err);
        return;
    }
    int changeBegin = -1;
    int changeEnd = -1;
    for (const QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate &u : updates) {
        Q_ASSERT(bufD == QRHI_RES(QVkBuffer, u.buf));
        memcpy(static_cast<char *>(p) + u.offset, u.data.constData(), u.data.size());
        if (changeBegin == -1 || u.offset < changeBegin)
            changeBegin = u.offset;
        if (changeEnd == -1 || u.offset + u.data.size() > changeEnd)
            changeEnd = u.offset + u.data.size();
    }
    vmaUnmapMemory(toVmaAllocator(allocator), a);
    if (changeBegin >= 0)
        vmaFlushAllocation(toVmaAllocator(allocator), a, changeBegin, changeEnd - changeBegin);

    updates.clear();
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
                for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
                    vmaDestroyBuffer(toVmaAllocator(allocator), e.buffer.buffers[i], toVmaAllocation(e.buffer.allocations[i]));
                    vmaDestroyBuffer(toVmaAllocator(allocator), e.buffer.stagingBuffers[i], toVmaAllocation(e.buffer.stagingAllocations[i]));
                }
                break;
            case QRhiVulkan::DeferredReleaseEntry::RenderBuffer:
                df->vkDestroyImageView(dev, e.renderBuffer.imageView, nullptr);
                df->vkDestroyImage(dev, e.renderBuffer.image, nullptr);
                df->vkFreeMemory(dev, e.renderBuffer.memory, nullptr);
                break;
            case QRhiVulkan::DeferredReleaseEntry::Texture:
                df->vkDestroyImageView(dev, e.texture.imageView, nullptr);
                vmaDestroyImage(toVmaAllocator(allocator), e.texture.image, toVmaAllocation(e.texture.allocation));
                for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
                    vmaDestroyBuffer(toVmaAllocator(allocator), e.texture.stagingBuffers[i], toVmaAllocation(e.texture.stagingAllocations[i]));
                break;
            case QRhiVulkan::DeferredReleaseEntry::Sampler:
                df->vkDestroySampler(dev, e.sampler.sampler, nullptr);
                break;
            case QRhiVulkan::DeferredReleaseEntry::TextureRenderTarget:
                df->vkDestroyFramebuffer(dev, e.textureRenderTarget.fb, nullptr);
                for (int att = 0; att < QVkRenderTargetData::MAX_COLOR_ATTACHMENTS; ++att) {
                    df->vkDestroyImageView(dev, e.textureRenderTarget.rtv[att], nullptr);
                    df->vkDestroyImageView(dev, e.textureRenderTarget.resrtv[att], nullptr);
                }
                break;
            case QRhiVulkan::DeferredReleaseEntry::RenderPass:
                df->vkDestroyRenderPass(dev, e.renderPass.rp, nullptr);
                break;
            case QRhiVulkan::DeferredReleaseEntry::StagingBuffer:
                vmaDestroyBuffer(toVmaAllocator(allocator), e.stagingBuffer.stagingBuffer, toVmaAllocation(e.stagingBuffer.stagingAllocation));
                break;
            default:
                break;
            }
            releaseQueue.removeAt(i);
        }
    }
}

void QRhiVulkan::finishActiveReadbacks(bool forced)
{
    QVarLengthArray<std::function<void()>, 4> completedCallbacks;

    for (int i = activeReadbacks.count() - 1; i >= 0; --i) {
        const QRhiVulkan::ActiveReadback &aRb(activeReadbacks[i]);
        if (forced || currentFrameSlot == aRb.activeFrameSlot || aRb.activeFrameSlot < 0) {
            aRb.result->format = aRb.format;
            aRb.result->pixelSize = aRb.pixelSize;
            aRb.result->data.resize(aRb.bufSize);
            void *p = nullptr;
            VmaAllocation a = toVmaAllocation(aRb.bufAlloc);
            VkResult err = vmaMapMemory(toVmaAllocator(allocator), a, &p);
            if (err != VK_SUCCESS) {
                qWarning("Failed to map readback buffer: %d", err);
                continue;
            }
            memcpy(aRb.result->data.data(), p, aRb.bufSize);
            vmaUnmapMemory(toVmaAllocator(allocator), a);

            vmaDestroyBuffer(toVmaAllocator(allocator), aRb.buf, a);

            if (aRb.result->completed)
                completedCallbacks.append(aRb.result->completed);

            activeReadbacks.removeAt(i);
        }
    }

    for (auto f : completedCallbacks)
        f();
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

QVector<int> QRhiVulkan::supportedSampleCounts() const
{
    const VkPhysicalDeviceLimits *limits = &physDevProperties.limits;
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

    if (!supportedSampleCounts().contains(sampleCount)) {
        qWarning("Attempted to set unsupported sample count %d", sampleCount);
        return VK_SAMPLE_COUNT_1_BIT;
    }

    for (size_t i = 0; i < sizeof(qvk_sampleCounts) / sizeof(qvk_sampleCounts[0]); ++i) {
        if (qvk_sampleCounts[i].count == sampleCount)
            return qvk_sampleCounts[i].mask;
    }

    Q_UNREACHABLE();
    return VK_SAMPLE_COUNT_1_BIT;
}

QRhiSwapChain *QRhiVulkan::createSwapChain()
{
    return new QVkSwapChain(this);
}

QRhiBuffer *QRhiVulkan::createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, int size)
{
    return new QVkBuffer(this, type, usage, size);
}

int QRhiVulkan::ubufAlignment() const
{
    return ubufAlign; // typically 256 (bytes)
}

bool QRhiVulkan::isYUpInFramebuffer() const
{
    return false;
}

QMatrix4x4 QRhiVulkan::clipSpaceCorrMatrix() const
{
    // See e.g. https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/

    static QMatrix4x4 m;
    if (m.isIdentity()) {
        // NB the ctor takes row-major
        m = QMatrix4x4(1.0f, 0.0f, 0.0f, 0.0f,
                       0.0f, -1.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 0.5f, 0.5f,
                       0.0f, 0.0f, 0.0f, 1.0f);
    }
    return m;
}

bool QRhiVulkan::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const
{
    VkPhysicalDeviceFeatures features;
    f->vkGetPhysicalDeviceFeatures(physDev, &features);

    // Note that with some SDKs the validation layer gives an odd warning about
    // BC not being supported, even when our check here succeeds. Not much we
    // can do about that.
    if (format >= QRhiTexture::BC1 && format <= QRhiTexture::BC7) {
        if (!features.textureCompressionBC)
            return false;
    }

    if (format >= QRhiTexture::ETC2_RGB8 && format <= QRhiTexture::ETC2_RGBA8) {
        if (!features.textureCompressionETC2)
            return false;
    }

    if (format >= QRhiTexture::ASTC_4x4 && format <= QRhiTexture::ASTC_12x12) {
        if (!features.textureCompressionASTC_LDR)
            return false;
    }

    VkFormat vkformat = toVkTextureFormat(format, flags);
    VkFormatProperties props;
    f->vkGetPhysicalDeviceFormatProperties(physDev, vkformat, &props);
    return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
}

bool QRhiVulkan::isFeatureSupported(QRhi::Feature feature) const
{
    switch (feature) {
    case QRhi::MultisampleTexture:
        Q_FALLTHROUGH();
    case QRhi::MultisampleRenderBuffer:
        return true;
    default:
        Q_UNREACHABLE();
        return false;
    }
}

const QRhiNativeHandles *QRhiVulkan::nativeHandles()
{
    return &nativeHandlesStruct;
}

void QRhiVulkan::sendVMemStatsToProfiler()
{
    VmaStats stats;
    vmaCalculateStats(toVmaAllocator(allocator), &stats);
    // ###
}

QRhiRenderBuffer *QRhiVulkan::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize,
                                                 int sampleCount, QRhiRenderBuffer::Flags flags)
{
    return new QVkRenderBuffer(this, type, pixelSize, sampleCount, flags);
}

QRhiTexture *QRhiVulkan::createTexture(QRhiTexture::Format format, const QSize &pixelSize,
                                       int sampleCount, QRhiTexture::Flags flags)
{
    return new QVkTexture(this, format, pixelSize, sampleCount, flags);
}

QRhiSampler *QRhiVulkan::createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                       QRhiSampler::Filter mipmapMode,
                                       QRhiSampler::AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w)
{
    return new QVkSampler(this, magFilter, minFilter, mipmapMode, u, v, w);
}

QRhiTextureRenderTarget *QRhiVulkan::createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                               QRhiTextureRenderTarget::Flags flags)
{
    return new QVkTextureRenderTarget(this, desc, flags);
}

QRhiGraphicsPipeline *QRhiVulkan::createGraphicsPipeline()
{
    return new QVkGraphicsPipeline(this);
}

QRhiShaderResourceBindings *QRhiVulkan::createShaderResourceBindings()
{
    return new QVkShaderResourceBindings(this);
}

void QRhiVulkan::setGraphicsPipeline(QRhiCommandBuffer *cb, QRhiGraphicsPipeline *ps, QRhiShaderResourceBindings *srb)
{
    Q_ASSERT(inPass);
    QVkGraphicsPipeline *psD = QRHI_RES(QVkGraphicsPipeline, ps);
    Q_ASSERT(psD->pipeline);

    if (!srb)
        srb = psD->m_shaderResourceBindings;

    QVkShaderResourceBindings *srbD = QRHI_RES(QVkShaderResourceBindings, srb);
    bool hasDynamicBufferInSrb = false;
    for (const QRhiShaderResourceBinding &b : qAsConst(srbD->m_bindings)) {
        switch (b.type) {
        case QRhiShaderResourceBinding::UniformBuffer:
        {
            QVkBuffer *bufD = QRHI_RES(QVkBuffer, b.ubuf.buf);
            Q_ASSERT(bufD->m_usage.testFlag(QRhiBuffer::UniformBuffer));
            bufD->lastActiveFrameSlot = currentFrameSlot;
            if (bufD->m_type == QRhiBuffer::Dynamic) {
                hasDynamicBufferInSrb = true;
                executeBufferHostWritesForCurrentFrame(bufD);
            }
        }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
            QRHI_RES(QVkTexture, b.stex.tex)->lastActiveFrameSlot = currentFrameSlot;
            QRHI_RES(QVkSampler, b.stex.sampler)->lastActiveFrameSlot = currentFrameSlot;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    // ensure the descriptor set we are going to bind refers to up-to-date Vk objects
    const int descSetIdx = hasDynamicBufferInSrb ? currentFrameSlot : 0;
    bool srbUpdate = false;
    for (int i = 0, ie = srbD->m_bindings.count(); i != ie; ++i) {
        const QRhiShaderResourceBinding &b(srbD->m_bindings[i]);
        QVkShaderResourceBindings::BoundResourceData &bd(srbD->boundResourceData[descSetIdx][i]);
        switch (b.type) {
        case QRhiShaderResourceBinding::UniformBuffer:
            if (QRHI_RES(QVkBuffer, b.ubuf.buf)->generation != bd.ubuf.generation) {
                srbUpdate = true;
                bd.ubuf.generation = QRHI_RES(QVkBuffer, b.ubuf.buf)->generation;
            }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
            if (QRHI_RES(QVkTexture, b.stex.tex)->generation != bd.stex.texGeneration
                    || QRHI_RES(QVkSampler, b.stex.sampler)->generation != bd.stex.samplerGeneration)
            {
                srbUpdate = true;
                bd.stex.texGeneration = QRHI_RES(QVkTexture, b.stex.tex)->generation;
                bd.stex.samplerGeneration = QRHI_RES(QVkSampler, b.stex.sampler)->generation;
            }
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
    if (srbUpdate)
        updateShaderResourceBindings(srb, descSetIdx);

    QVkCommandBuffer *cbD = QRHI_RES(QVkCommandBuffer, cb);
    if (cbD->currentPipeline != ps || cbD->currentPipelineGeneration != psD->generation) {
        df->vkCmdBindPipeline(cbD->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, psD->pipeline);
        cbD->currentPipeline = ps;
        cbD->currentPipelineGeneration = psD->generation;
    }
    psD->lastActiveFrameSlot = currentFrameSlot;

    if (hasDynamicBufferInSrb || srbUpdate || cbD->currentSrb != srb || cbD->currentSrbGeneration != srbD->generation) {
        df->vkCmdBindDescriptorSets(cbD->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, psD->layout, 0, 1,
                                       &srbD->descSets[descSetIdx], 0, nullptr);
        cbD->currentSrb = srb;
        cbD->currentSrbGeneration = srbD->generation;
    }
    srbD->lastActiveFrameSlot = currentFrameSlot;
}

void QRhiVulkan::setVertexInput(QRhiCommandBuffer *cb, int startBinding, const QVector<QRhiCommandBuffer::VertexInput> &bindings,
                                QRhiBuffer *indexBuf, quint32 indexOffset, QRhiCommandBuffer::IndexFormat indexFormat)
{
    Q_ASSERT(inPass);

    QVarLengthArray<VkBuffer, 4> bufs;
    QVarLengthArray<VkDeviceSize, 4> ofs;
    for (int i = 0, ie = bindings.count(); i != ie; ++i) {
        QRhiBuffer *buf = bindings[i].first;
        QVkBuffer *bufD = QRHI_RES(QVkBuffer, buf);
        Q_ASSERT(bufD->m_usage.testFlag(QRhiBuffer::VertexBuffer));
        bufD->lastActiveFrameSlot = currentFrameSlot;
        if (bufD->m_type == QRhiBuffer::Dynamic)
            executeBufferHostWritesForCurrentFrame(bufD);

        const int idx = bufD->m_type == QRhiBuffer::Dynamic ? currentFrameSlot : 0;
        bufs.append(bufD->buffers[idx]);
        ofs.append(bindings[i].second);
    }
    QVkCommandBuffer *cbD = QRHI_RES(QVkCommandBuffer, cb);
    if (!bufs.isEmpty())
        df->vkCmdBindVertexBuffers(cbD->cb, startBinding, bufs.count(), bufs.constData(), ofs.constData());

    if (indexBuf) {
        QVkBuffer *ibufD = QRHI_RES(QVkBuffer, indexBuf);
        Q_ASSERT(ibufD->m_usage.testFlag(QRhiBuffer::IndexBuffer));
        ibufD->lastActiveFrameSlot = currentFrameSlot;
        if (ibufD->m_type == QRhiBuffer::Dynamic)
            executeBufferHostWritesForCurrentFrame(ibufD);

        const int idx = ibufD->m_type == QRhiBuffer::Dynamic ? currentFrameSlot : 0;
        const VkIndexType type = indexFormat == QRhiCommandBuffer::IndexUInt16 ? VK_INDEX_TYPE_UINT16
                                                                               : VK_INDEX_TYPE_UINT32;
        df->vkCmdBindIndexBuffer(cbD->cb, ibufD->buffers[idx], indexOffset, type);
    }
}

static inline VkViewport toVkViewport(const QRhiViewport &viewport, const QSize &outputSize)
{
    // x,y is top-left in VkViewport but bottom-left in QRhiViewport
    VkViewport vp;
    vp.x = viewport.r.x();
    vp.y = outputSize.height() - (viewport.r.y() + viewport.r.w());
    vp.width = viewport.r.z();
    vp.height = viewport.r.w();
    vp.minDepth = viewport.minDepth;
    vp.maxDepth = viewport.maxDepth;
    return vp;
}

static inline VkRect2D toVkScissor(const QRhiScissor &scissor, const QSize &outputSize)
{
    // x,y is top-left in VkRect2D but bottom-left in QRhiScissor
    VkRect2D s;
    s.offset.x = scissor.r.x();
    s.offset.y = outputSize.height() - (scissor.r.y() + scissor.r.w());
    s.extent.width = scissor.r.z();
    s.extent.height = scissor.r.w();
    return s;
}

void QRhiVulkan::setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport)
{
    Q_ASSERT(inPass);
    QVkCommandBuffer *cbD = QRHI_RES(QVkCommandBuffer, cb);
    Q_ASSERT(cbD->currentPipeline && cbD->currentTarget);
    const QSize outputSize = cbD->currentTarget->sizeInPixels();
    const VkViewport vp = toVkViewport(viewport, outputSize);
    df->vkCmdSetViewport(cbD->cb, 0, 1, &vp);

    if (!QRHI_RES(QVkGraphicsPipeline, cbD->currentPipeline)->m_flags.testFlag(QRhiGraphicsPipeline::UsesScissor)) {
        const VkRect2D s = toVkScissor(QRhiScissor(viewport.r.x(), viewport.r.y(), viewport.r.z(), viewport.r.w()), outputSize);
        df->vkCmdSetScissor(cbD->cb, 0, 1, &s);
    }
}

void QRhiVulkan::setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor)
{
    Q_ASSERT(inPass);
    QVkCommandBuffer *cbD = QRHI_RES(QVkCommandBuffer, cb);
    Q_ASSERT(cbD->currentPipeline && cbD->currentTarget);
    Q_ASSERT(QRHI_RES(QVkGraphicsPipeline, cbD->currentPipeline)->m_flags.testFlag(QRhiGraphicsPipeline::UsesScissor));
    const VkRect2D s = toVkScissor(scissor, cbD->currentTarget->sizeInPixels());
    df->vkCmdSetScissor(cbD->cb, 0, 1, &s);
}

void QRhiVulkan::setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c)
{
    Q_ASSERT(inPass);
    const float bc[4] = { c.x(), c.y(), c.z(), c.w() };
    df->vkCmdSetBlendConstants(QRHI_RES(QVkCommandBuffer, cb)->cb, bc);
}

void QRhiVulkan::setStencilRef(QRhiCommandBuffer *cb, quint32 refValue)
{
    Q_ASSERT(inPass);
    df->vkCmdSetStencilReference(QRHI_RES(QVkCommandBuffer, cb)->cb, VK_STENCIL_FRONT_AND_BACK, refValue);
}

void QRhiVulkan::draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    Q_ASSERT(inPass);
    df->vkCmdDraw(QRHI_RES(QVkCommandBuffer, cb)->cb, vertexCount, instanceCount, firstVertex, firstInstance);
}

void QRhiVulkan::drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                       quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)
{
    Q_ASSERT(inPass);
    df->vkCmdDrawIndexed(QRHI_RES(QVkCommandBuffer, cb)->cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
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
    case QRhiSampler::None:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
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

static inline VkShaderStageFlagBits toVkShaderStage(QRhiGraphicsShaderStage::Type type)
{
    switch (type) {
    case QRhiGraphicsShaderStage::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case QRhiGraphicsShaderStage::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
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
    switch (c) {
    case QRhiGraphicsPipeline::None:
        return VK_CULL_MODE_NONE;
    case QRhiGraphicsPipeline::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case QRhiGraphicsPipeline::Back:
        return VK_CULL_MODE_BACK_BIT;
    default:
        Q_UNREACHABLE();
        return VK_CULL_MODE_NONE;
    }
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

static inline VkDescriptorType toVkDescriptorType(QRhiShaderResourceBinding::Type type)
{
    switch (type) {
    case QRhiShaderResourceBinding::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case QRhiShaderResourceBinding::SampledTexture:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    default:
        Q_UNREACHABLE();
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

static inline VkShaderStageFlags toVkShaderStageFlags(QRhiShaderResourceBinding::StageFlags stage)
{
    int s = 0;
    if (stage.testFlag(QRhiShaderResourceBinding::VertexStage))
        s |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stage.testFlag(QRhiShaderResourceBinding::FragmentStage))
        s |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stage.testFlag(QRhiShaderResourceBinding::TessellationControlStage))
        s |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stage.testFlag(QRhiShaderResourceBinding::TessellationEvaluationStage))
        s |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return VkShaderStageFlags(s);
}

QVkBuffer::QVkBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size)
    : QRhiBuffer(rhi, type, usage, size)
{
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        buffers[i] = stagingBuffers[i] = VK_NULL_HANDLE;
        allocations[i] = stagingAllocations[i] = nullptr;
    }
}

void QVkBuffer::release()
{
    int nullBufferCount = 0;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        if (!buffers[i])
            ++nullBufferCount;
    }
    if (nullBufferCount == QVK_FRAMES_IN_FLIGHT)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::Buffer;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        e.buffer.buffers[i] = buffers[i];
        e.buffer.allocations[i] = allocations[i];
        e.buffer.stagingBuffers[i] = stagingBuffers[i];
        e.buffer.stagingAllocations[i] = stagingAllocations[i];

        buffers[i] = VK_NULL_HANDLE;
        allocations[i] = nullptr;
        stagingBuffers[i] = VK_NULL_HANDLE;
        stagingAllocations[i] = nullptr;
        pendingDynamicUpdates[i].clear();
    }

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);

    QRHI_PROF;
    QRHI_PROF_F(releaseBuffer(this));
}

bool QVkBuffer::build()
{
    if (buffers[0])
        release();

    const int nonZeroSize = m_size <= 0 ? 256 : m_size;

    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = nonZeroSize;
    bufferInfo.usage = toVkBufferUsage(m_usage);

    VmaAllocationCreateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));

    if (m_type == Dynamic) {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    } else {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    QRHI_RES_RHI(QRhiVulkan);
    VkResult err = VK_SUCCESS;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        buffers[i] = VK_NULL_HANDLE;
        allocations[i] = nullptr;
        if (i == 0 || m_type == Dynamic) {
            VmaAllocation allocation;
            err = vmaCreateBuffer(toVmaAllocator(rhiD->allocator), &bufferInfo, &allocInfo, &buffers[i], &allocation, nullptr);
            if (err != VK_SUCCESS)
                break;
            allocations[i] = allocation;
            if (m_type == Dynamic)
                pendingDynamicUpdates[i].reserve(16);
        }
    }

    if (err != VK_SUCCESS) {
        qWarning("Failed to create buffer: %d", err);
        return false;
    }

    QRHI_PROF;
    QRHI_PROF_F(newBuffer(this, nonZeroSize, m_type != Dynamic ? 1 : QVK_FRAMES_IN_FLIGHT, 0));

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QVkRenderBuffer::QVkRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                                 int sampleCount, Flags flags)
    : QRhiRenderBuffer(rhi, type, pixelSize, sampleCount, flags)
{
}

QVkRenderBuffer::~QVkRenderBuffer()
{
    delete backingTexture;
}

void QVkRenderBuffer::release()
{
    if (!memory && !backingTexture)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::RenderBuffer;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.renderBuffer.memory = memory;
    e.renderBuffer.image = image;
    e.renderBuffer.imageView = imageView;

    memory = VK_NULL_HANDLE;
    image = VK_NULL_HANDLE;
    imageView = VK_NULL_HANDLE;

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);

    if (backingTexture) {
        Q_ASSERT(backingTexture->lastActiveFrameSlot == -1);
        backingTexture->release();
    }
}

bool QVkRenderBuffer::build()
{
    if (memory || backingTexture)
        release();

    QRHI_RES_RHI(QRhiVulkan);
    samples = rhiD->effectiveSampleCount(m_sampleCount);

    switch (m_type) {
    case QRhiRenderBuffer::Color:
    {
        if (!backingTexture) {
            backingTexture = QRHI_RES(QVkTexture, rhiD->createTexture(QRhiTexture::RGBA8,
                                                                      m_pixelSize,
                                                                      m_sampleCount,
                                                                      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        }
        if (!backingTexture->build())
            return false;
        vkformat = backingTexture->vkformat;
    }
        break;
    case QRhiRenderBuffer::DepthStencil:
        vkformat = rhiD->optimalDepthStencilFormat();
        if (!rhiD->createTransientImage(vkformat,
                                        m_pixelSize,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                        samples,
                                        &memory,
                                        &image,
                                        &imageView,
                                        1))
        {
            return false;
        }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    lastActiveFrameSlot = -1;
    return true;
}

QRhiTexture::Format QVkRenderBuffer::backingFormat() const
{
    return m_type == Color ? QRhiTexture::RGBA8 : QRhiTexture::UnknownFormat;
}

QVkTexture::QVkTexture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                       int sampleCount, Flags flags)
    : QRhiTexture(rhi, format, pixelSize, sampleCount, flags)
{
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        stagingBuffers[i] = VK_NULL_HANDLE;
        stagingAllocations[i] = nullptr;
    }
}

void QVkTexture::release()
{
    if (!image)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::Texture;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.texture.image = owns ? image : VK_NULL_HANDLE;
    e.texture.imageView = imageView;
    e.texture.allocation = owns ? imageAlloc : nullptr;

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i) {
        e.texture.stagingBuffers[i] = stagingBuffers[i];
        e.texture.stagingAllocations[i] = stagingAllocations[i];

        stagingBuffers[i] = VK_NULL_HANDLE;
        stagingAllocations[i] = nullptr;
    }

    image = VK_NULL_HANDLE;
    imageView = VK_NULL_HANDLE;
    imageAlloc = nullptr;
    nativeHandlesStruct.image = VK_NULL_HANDLE;

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkTexture::prepareBuild(QSize *adjustedSize)
{
    if (image)
        release();

    QRHI_RES_RHI(QRhiVulkan);
    vkformat = toVkTextureFormat(m_format, m_flags);
    VkFormatProperties props;
    rhiD->f->vkGetPhysicalDeviceFormatProperties(rhiD->physDev, vkformat, &props);
    const bool canSampleOptimal = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    if (!canSampleOptimal) {
        qWarning("Texture sampling with optimal tiling for format %d not supported", vkformat);
        return false;
    }

    const QSize size = m_pixelSize.isEmpty() ? QSize(16, 16) : m_pixelSize;
    const bool isCube = m_flags.testFlag(CubeMap);
    const bool hasMipMaps = m_flags.testFlag(MipMapped);

    mipLevelCount = hasMipMaps ? qCeil(log2(qMax(size.width(), size.height()))) + 1 : 1;
    samples = rhiD->effectiveSampleCount(m_sampleCount);
    if (samples > VK_SAMPLE_COUNT_1_BIT) {
        if (isCube) {
            qWarning("Cubemap texture cannot be multisample");
            return false;
        }
        if (hasMipMaps) {
            qWarning("Multisample texture cannot have mipmaps");
            return false;
        }
    }

    if (adjustedSize)
        *adjustedSize = size;

    return true;
}

bool QVkTexture::finishBuild()
{
    QRHI_RES_RHI(QRhiVulkan);

    const bool isDepth = isDepthTextureFormat(m_format);
    const bool isCube = m_flags.testFlag(CubeMap);

    VkImageViewCreateInfo viewInfo;
    memset(&viewInfo, 0, sizeof(viewInfo));
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = isCube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkformat;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    viewInfo.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = mipLevelCount;
    viewInfo.subresourceRange.layerCount = isCube ? 6 : 1;

    VkResult err = rhiD->df->vkCreateImageView(rhiD->dev, &viewInfo, nullptr, &imageView);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create image view: %d", err);
        return false;
    }

    nativeHandlesStruct.image = image;

    lastActiveFrameSlot = -1;
    generation += 1;

    return true;
}

bool QVkTexture::build()
{
    QRHI_RES_RHI(QRhiVulkan);

    QSize size;
    if (!prepareBuild(&size))
        return false;

    const bool isRenderTarget = m_flags.testFlag(QRhiTexture::RenderTarget);
    const bool isDepth = isDepthTextureFormat(m_format);
    const bool isCube = m_flags.testFlag(CubeMap);

    VkImageCreateInfo imageInfo;
    memset(&imageInfo, 0, sizeof(imageInfo));
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = isCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkformat;
    imageInfo.extent.width = size.width();
    imageInfo.extent.height = size.height();
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevelCount;
    imageInfo.arrayLayers = isCube ? 6 : 1;
    imageInfo.samples = samples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (isRenderTarget) {
        if (isDepth)
            imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (m_flags.testFlag(QRhiTexture::UsedAsTransferSource))
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (m_flags.testFlag(QRhiTexture::UsedWithGenerateMips))
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VmaAllocation allocation;
    VkResult err = vmaCreateImage(toVmaAllocator(rhiD->allocator), &imageInfo, &allocInfo, &image, &allocation, nullptr);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create image: %d", err);
        return false;
    }
    imageAlloc = allocation;

    if (!finishBuild())
        return false;

    owns = true;
    layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    return true;
}

bool QVkTexture::buildFrom(const QRhiNativeHandles *src)
{
    const QRhiVulkanTextureNativeHandles *h = static_cast<const QRhiVulkanTextureNativeHandles *>(src);
    if (!h || !h->image)
        return false;

    if (!prepareBuild())
        return false;

    image = h->image;

    if (!finishBuild())
        return false;

    owns = false;
    layout = h->layout;
    return true;
}

const QRhiNativeHandles *QVkTexture::nativeHandles()
{
    nativeHandlesStruct.layout = layout;
    return &nativeHandlesStruct;
}

QVkSampler::QVkSampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                       AddressMode u, AddressMode v, AddressMode w)
    : QRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v, w)
{
}

void QVkSampler::release()
{
    if (!sampler)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::Sampler;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.sampler.sampler = sampler;
    sampler = VK_NULL_HANDLE;

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkSampler::build()
{
    if (sampler)
        release();

    VkSamplerCreateInfo samplerInfo;
    memset(&samplerInfo, 0, sizeof(samplerInfo));
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = toVkFilter(m_magFilter);
    samplerInfo.minFilter = toVkFilter(m_minFilter);
    samplerInfo.mipmapMode = toVkMipmapMode(m_mipmapMode);
    samplerInfo.addressModeU = toVkAddressMode(m_addressU);
    samplerInfo.addressModeV = toVkAddressMode(m_addressV);
    samplerInfo.addressModeW = toVkAddressMode(m_addressW);
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.maxLod = m_mipmapMode == None ? 0.25f : 1000.0f;

    QRHI_RES_RHI(QRhiVulkan);
    VkResult err = rhiD->df->vkCreateSampler(rhiD->dev, &samplerInfo, nullptr, &sampler);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create sampler: %d", err);
        return false;
    }

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QVkRenderPassDescriptor::QVkRenderPassDescriptor(QRhiImplementation *rhi)
    : QRhiRenderPassDescriptor(rhi)
{
}

void QVkRenderPassDescriptor::release()
{
    if (!rp)
        return;

    if (!ownsRp) {
        rp = VK_NULL_HANDLE;
        return;
    }

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::RenderPass;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.renderPass.rp = rp;

    rp = VK_NULL_HANDLE;

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

QVkReferenceRenderTarget::QVkReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiReferenceRenderTarget(rhi)
{
}

void QVkReferenceRenderTarget::release()
{
    // nothing to do here
}

QRhiRenderTarget::Type QVkReferenceRenderTarget::type() const
{
    return RtRef; // no Vk* are owned directly by the object
}

QSize QVkReferenceRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

float QVkReferenceRenderTarget::devicePixelRatio() const
{
    return d.dpr;
}

QVkTextureRenderTarget::QVkTextureRenderTarget(QRhiImplementation *rhi,
                                               const QRhiTextureRenderTargetDescription &desc,
                                               Flags flags)
    : QRhiTextureRenderTarget(rhi, desc, flags)
{
    for (int att = 0; att < QVkRenderTargetData::MAX_COLOR_ATTACHMENTS; ++att) {
        rtv[att] = VK_NULL_HANDLE;
        resrtv[att] = VK_NULL_HANDLE;
    }
}

void QVkTextureRenderTarget::release()
{
    if (!d.fb)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::TextureRenderTarget;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.textureRenderTarget.fb = d.fb;
    d.fb = VK_NULL_HANDLE;

    for (int att = 0; att < QVkRenderTargetData::MAX_COLOR_ATTACHMENTS; ++att) {
        e.textureRenderTarget.rtv[att] = rtv[att];
        e.textureRenderTarget.resrtv[att] = resrtv[att];
        rtv[att] = VK_NULL_HANDLE;
        resrtv[att] = VK_NULL_HANDLE;
    }

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

QRhiRenderPassDescriptor *QVkTextureRenderTarget::newCompatibleRenderPassDescriptor()
{
    // not yet built so cannot rely on data computed in build()

    QRHI_RES_RHI(QRhiVulkan);
    QVkRenderPassDescriptor *rp = new QVkRenderPassDescriptor(rhi);
    if (!rhiD->createOffscreenRenderPass(&rp->rp,
                                         m_desc.colorAttachments,
                                         m_flags.testFlag(QRhiTextureRenderTarget::PreserveColorContents),
                                         m_flags.testFlag(QRhiTextureRenderTarget::PreserveDepthStencilContents),
                                         m_desc.depthStencilBuffer,
                                         m_desc.depthTexture))
    {
        delete rp;
        return nullptr;
    }

    rp->ownsRp = true;
    return rp;
}

bool QVkTextureRenderTarget::build()
{
    if (d.fb)
        release();

    Q_ASSERT(!m_desc.colorAttachments.isEmpty() || m_desc.depthTexture);
    Q_ASSERT(!m_desc.depthStencilBuffer || !m_desc.depthTexture);
    const bool hasDepthStencil = m_desc.depthStencilBuffer || m_desc.depthTexture;

    QRHI_RES_RHI(QRhiVulkan);
    QVarLengthArray<VkImageView, 8> views;

    d.colorAttCount = m_desc.colorAttachments.count();
    for (int i = 0; i < d.colorAttCount; ++i) {
        QVkTexture *texD = QRHI_RES(QVkTexture, m_desc.colorAttachments[i].texture);
        QVkRenderBuffer *rbD = QRHI_RES(QVkRenderBuffer, m_desc.colorAttachments[i].renderBuffer);
        Q_ASSERT(texD || rbD);
        if (texD) {
            Q_ASSERT(texD->flags().testFlag(QRhiTexture::RenderTarget));
            VkImageViewCreateInfo viewInfo;
            memset(&viewInfo, 0, sizeof(viewInfo));
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = texD->image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = texD->vkformat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = m_desc.colorAttachments[i].level;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = m_desc.colorAttachments[i].layer;
            viewInfo.subresourceRange.layerCount = 1;
            VkResult err = rhiD->df->vkCreateImageView(rhiD->dev, &viewInfo, nullptr, &rtv[i]);
            if (err != VK_SUCCESS) {
                qWarning("Failed to create render target image view: %d", err);
                return false;
            }
            views.append(rtv[i]);
            if (i == 0)
                d.pixelSize = texD->pixelSize();
        } else if (rbD) {
            Q_ASSERT(rbD->backingTexture);
            views.append(rbD->backingTexture->imageView);
            if (i == 0)
                d.pixelSize = rbD->pixelSize();
        } else {
            Q_UNREACHABLE();
        }
    }
    d.dpr = 1;

    if (hasDepthStencil) {
        d.dsAttCount = 1;
        views.append(m_desc.depthTexture ? QRHI_RES(QVkTexture, m_desc.depthTexture)->imageView
                                         : QRHI_RES(QVkRenderBuffer, m_desc.depthStencilBuffer)->imageView);

        if (d.colorAttCount == 0)
            d.pixelSize = m_desc.depthTexture ? m_desc.depthTexture->pixelSize() : m_desc.depthStencilBuffer->pixelSize();
    } else {
        d.dsAttCount = 0;
    }

    d.resolveAttCount = 0;
    for (int i = 0; i < d.colorAttCount; ++i) {
        if (m_desc.colorAttachments[i].resolveTexture) {
            d.resolveAttCount += 1;
            QVkTexture *resTexD = QRHI_RES(QVkTexture, m_desc.colorAttachments[i].resolveTexture);

            VkImageViewCreateInfo viewInfo;
            memset(&viewInfo, 0, sizeof(viewInfo));
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = resTexD->image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = resTexD->vkformat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = m_desc.colorAttachments[i].resolveLevel;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = m_desc.colorAttachments[i].resolveLayer;
            viewInfo.subresourceRange.layerCount = 1;
            VkResult err = rhiD->df->vkCreateImageView(rhiD->dev, &viewInfo, nullptr, &resrtv[i]);
            if (err != VK_SUCCESS) {
                qWarning("Failed to create render target resolve image view: %d", err);
                return false;
            }
            views.append(resrtv[i]);
        }
    }

    if (!m_renderPassDesc)
        qWarning("QVkTextureRenderTarget: No renderpass descriptor set. See newCompatibleRenderPassDescriptor() and setRenderPassDescriptor().");

    d.rp = QRHI_RES(QVkRenderPassDescriptor, m_renderPassDesc);
    Q_ASSERT(d.rp && d.rp->rp);

    VkFramebufferCreateInfo fbInfo;
    memset(&fbInfo, 0, sizeof(fbInfo));
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = d.rp->rp;
    fbInfo.attachmentCount = d.colorAttCount + d.dsAttCount + d.resolveAttCount;
    fbInfo.pAttachments = views.constData();
    fbInfo.width = d.pixelSize.width();
    fbInfo.height = d.pixelSize.height();
    fbInfo.layers = 1;

    VkResult err = rhiD->df->vkCreateFramebuffer(rhiD->dev, &fbInfo, nullptr, &d.fb);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create framebuffer: %d", err);
        return false;
    }

    lastActiveFrameSlot = -1;
    return true;
}

QRhiRenderTarget::Type QVkTextureRenderTarget::type() const
{
    return RtTexture;
}

QSize QVkTextureRenderTarget::sizeInPixels() const
{
    return d.pixelSize;
}

float QVkTextureRenderTarget::devicePixelRatio() const
{
    return d.dpr;
}

QVkShaderResourceBindings::QVkShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiShaderResourceBindings(rhi)
{
}

void QVkShaderResourceBindings::release()
{
    if (!layout)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::ShaderResourceBindings;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.shaderResourceBindings.poolIndex = poolIndex;
    e.shaderResourceBindings.layout = layout;

    poolIndex = -1;
    layout = VK_NULL_HANDLE;
    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        descSets[i] = VK_NULL_HANDLE;

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkShaderResourceBindings::build()
{
    if (layout)
        release();

    for (int i = 0; i < QVK_FRAMES_IN_FLIGHT; ++i)
        descSets[i] = VK_NULL_HANDLE;

    QVarLengthArray<VkDescriptorSetLayoutBinding, 4> vkbindings;
    for (const QRhiShaderResourceBinding &b : qAsConst(m_bindings)) {
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

    QRHI_RES_RHI(QRhiVulkan);
    VkResult err = rhiD->df->vkCreateDescriptorSetLayout(rhiD->dev, &layoutInfo, nullptr, &layout);
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
        layouts[i] = layout;
    allocInfo.pSetLayouts = layouts;
    if (!rhiD->allocateDescriptorSet(&allocInfo, descSets, &poolIndex))
        return false;

    rhiD->updateShaderResourceBindings(this);

    lastActiveFrameSlot = -1;
    generation += 1;
    return true;
}

QVkGraphicsPipeline::QVkGraphicsPipeline(QRhiImplementation *rhi)
    : QRhiGraphicsPipeline(rhi)
{
}

void QVkGraphicsPipeline::release()
{
    if (!pipeline && !layout)
        return;

    QRhiVulkan::DeferredReleaseEntry e;
    e.type = QRhiVulkan::DeferredReleaseEntry::Pipeline;
    e.lastActiveFrameSlot = lastActiveFrameSlot;

    e.pipelineState.pipeline = pipeline;
    e.pipelineState.layout = layout;

    pipeline = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->releaseQueue.append(e);
}

bool QVkGraphicsPipeline::build()
{
    if (pipeline)
        release();

    QRHI_RES_RHI(QRhiVulkan);
    if (!rhiD->ensurePipelineCache())
        return false;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    QVkShaderResourceBindings *srbD = QRHI_RES(QVkShaderResourceBindings, m_shaderResourceBindings);
    Q_ASSERT(m_shaderResourceBindings && srbD->layout);
    pipelineLayoutInfo.pSetLayouts = &srbD->layout;
    VkResult err = rhiD->df->vkCreatePipelineLayout(rhiD->dev, &pipelineLayoutInfo, nullptr, &layout);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create pipeline layout: %d", err);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo;
    memset(&pipelineInfo, 0, sizeof(pipelineInfo));
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    QVarLengthArray<VkShaderModule, 4> shaders;
    QVarLengthArray<VkPipelineShaderStageCreateInfo, 4> shaderStageCreateInfos;
    for (const QRhiGraphicsShaderStage &shaderStage : m_shaderStages) {
        const QBakedShader::Shader spirv = shaderStage.shader.shader(QBakedShader::SpirvShader);
        if (spirv.shader.isEmpty()) {
            qWarning() << "No SPIR-V shader code found in baked shader" << shaderStage.shader;
            return false;
        }
        VkShaderModule shader = rhiD->createShader(spirv.shader);
        if (shader) {
            shaders.append(shader);
            VkPipelineShaderStageCreateInfo shaderInfo;
            memset(&shaderInfo, 0, sizeof(shaderInfo));
            shaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderInfo.stage = toVkShaderStage(shaderStage.type);
            shaderInfo.module = shader;
            shaderInfo.pName = spirv.entryPoint.constData();
            shaderStageCreateInfos.append(shaderInfo);
        }
    }
    pipelineInfo.stageCount = shaderStageCreateInfos.count();
    pipelineInfo.pStages = shaderStageCreateInfos.constData();

    QVarLengthArray<VkVertexInputBindingDescription, 4> vertexBindings;
    for (int i = 0, ie = m_vertexInputLayout.bindings.count(); i != ie; ++i) {
        const QRhiVertexInputLayout::Binding &binding(m_vertexInputLayout.bindings[i]);
        VkVertexInputBindingDescription bindingInfo = {
            uint32_t(i),
            binding.stride,
            binding.classification == QRhiVertexInputLayout::Binding::PerVertex
                ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE
        };
        if (binding.classification == QRhiVertexInputLayout::Binding::PerInstance
                && binding.instanceStepRate != 1)
        {
            // ### could be supported with VK_EXT_vertex_attribute_divisor (Vulkan 1.1)
            qWarning("QRhiVulkan: Instance step rates other than 1 not currently supported");
        }
        vertexBindings.append(bindingInfo);
    }
    QVarLengthArray<VkVertexInputAttributeDescription, 4> vertexAttributes;
    for (const QRhiVertexInputLayout::Attribute &attribute : m_vertexInputLayout.attributes) {
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
    dynEnable << VK_DYNAMIC_STATE_SCISSOR; // ignore UsesScissor - Vulkan requires a scissor for the viewport always
    if (m_flags.testFlag(QRhiGraphicsPipeline::UsesBlendConstants))
        dynEnable << VK_DYNAMIC_STATE_BLEND_CONSTANTS;
    if (m_flags.testFlag(QRhiGraphicsPipeline::UsesStencilRef))
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
    viewportInfo.viewportCount = viewportInfo.scissorCount = 1;
    pipelineInfo.pViewportState = &viewportInfo;

    VkPipelineInputAssemblyStateCreateInfo inputAsmInfo;
    memset(&inputAsmInfo, 0, sizeof(inputAsmInfo));
    inputAsmInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAsmInfo.topology = toVkTopology(m_topology);
    pipelineInfo.pInputAssemblyState = &inputAsmInfo;

    VkPipelineRasterizationStateCreateInfo rastInfo;
    memset(&rastInfo, 0, sizeof(rastInfo));
    rastInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastInfo.cullMode = toVkCullMode(m_cullMode);
    rastInfo.frontFace = toVkFrontFace(m_frontFace);
    rastInfo.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rastInfo;

    VkPipelineMultisampleStateCreateInfo msInfo;
    memset(&msInfo, 0, sizeof(msInfo));
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = rhiD->effectiveSampleCount(m_sampleCount);
    pipelineInfo.pMultisampleState = &msInfo;

    VkPipelineDepthStencilStateCreateInfo dsInfo;
    memset(&dsInfo, 0, sizeof(dsInfo));
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.depthTestEnable = m_depthTest;
    dsInfo.depthWriteEnable = m_depthWrite;
    dsInfo.depthCompareOp = toVkCompareOp(m_depthOp);
    dsInfo.stencilTestEnable = m_stencilTest;
    if (m_stencilTest) {
        fillVkStencilOpState(&dsInfo.front, m_stencilFront);
        dsInfo.front.compareMask = m_stencilReadMask;
        dsInfo.front.writeMask = m_stencilWriteMask;
        fillVkStencilOpState(&dsInfo.back, m_stencilBack);
        dsInfo.back.compareMask = m_stencilReadMask;
        dsInfo.back.writeMask = m_stencilWriteMask;
    }
    pipelineInfo.pDepthStencilState = &dsInfo;

    VkPipelineColorBlendStateCreateInfo blendInfo;
    memset(&blendInfo, 0, sizeof(blendInfo));
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    QVarLengthArray<VkPipelineColorBlendAttachmentState, 4> vktargetBlends;
    for (const QRhiGraphicsPipeline::TargetBlend &b : qAsConst(m_targetBlends)) {
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
    if (vktargetBlends.isEmpty()) {
        VkPipelineColorBlendAttachmentState blend;
        memset(&blend, 0, sizeof(blend));
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        vktargetBlends.append(blend);
    }
    blendInfo.attachmentCount = vktargetBlends.count();
    blendInfo.pAttachments = vktargetBlends.constData();
    pipelineInfo.pColorBlendState = &blendInfo;

    pipelineInfo.layout = layout;

    Q_ASSERT(m_renderPassDesc && QRHI_RES(const QVkRenderPassDescriptor, m_renderPassDesc)->rp);
    pipelineInfo.renderPass = QRHI_RES(const QVkRenderPassDescriptor, m_renderPassDesc)->rp;

    err = rhiD->df->vkCreateGraphicsPipelines(rhiD->dev, rhiD->pipelineCache, 1, &pipelineInfo, nullptr, &pipeline);

    for (VkShaderModule shader : shaders)
        rhiD->df->vkDestroyShaderModule(rhiD->dev, shader, nullptr);

    if (err == VK_SUCCESS) {
        lastActiveFrameSlot = -1;
        generation += 1;
        return true;
    } else {
        qWarning("Failed to create graphics pipeline: %d", err);
        return false;
    }
}

QVkCommandBuffer::QVkCommandBuffer(QRhiImplementation *rhi)
    : QRhiCommandBuffer(rhi)
{
    resetState();
}

void QVkCommandBuffer::release()
{
    Q_UNREACHABLE();
}

QVkSwapChain::QVkSwapChain(QRhiImplementation *rhi)
    : QRhiSwapChain(rhi),
      rtWrapper(rhi),
      cbWrapper(rhi)
{
}

void QVkSwapChain::release()
{
    if (wrapWindow)
        return;

    QRHI_RES_RHI(QRhiVulkan);
    rhiD->swapchains.remove(this);
    rhiD->releaseSwapChainResources(this);
}

QRhiCommandBuffer *QVkSwapChain::currentFrameCommandBuffer()
{
    return &cbWrapper;
}

QRhiRenderTarget *QVkSwapChain::currentFrameRenderTarget()
{
    return &rtWrapper;
}

QSize QVkSwapChain::surfacePixelSize()
{
    if (m_target) {
        QVulkanWindow *vkw = qobject_cast<QVulkanWindow *>(m_target);
        return vkw ? vkw->swapChainImageSize() : QSize();
    }

    if (!ensureSurface())
        return QSize();

    // The size from the QWindow may not exactly match the surface... so if a
    // size is reported from the surface, use that.
    VkSurfaceCapabilitiesKHR surfaceCaps;
    memset(&surfaceCaps, 0, sizeof(surfaceCaps));
    QRHI_RES_RHI(QRhiVulkan);
    rhiD->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rhiD->physDev, surface, &surfaceCaps);
    VkExtent2D bufferSize = surfaceCaps.currentExtent;
    if (bufferSize.width == quint32(-1)) {
        Q_ASSERT(bufferSize.height == quint32(-1));
        return m_window->size() * m_window->devicePixelRatio();
    }
    return QSize(bufferSize.width, bufferSize.height);
}

QRhiRenderPassDescriptor *QVkSwapChain::newCompatibleRenderPassDescriptor()
{
    // not yet built so cannot rely on data computed in buildOrResize()

    if (m_target) {
        QVulkanWindow *vkw = qobject_cast<QVulkanWindow *>(m_target);
        if (vkw) {
            QVkRenderPassDescriptor *rp = new QVkRenderPassDescriptor(rhi);
            rp->rp = vkw->defaultRenderPass();
            rp->ownsRp = false;
            return rp;
        }
        return nullptr;
    }

    if (!ensureSurface()) // make sure sampleCount and colorFormat reflect what was requested
        return nullptr;

    QRHI_RES_RHI(QRhiVulkan);
    QVkRenderPassDescriptor *rp = new QVkRenderPassDescriptor(rhi);
    if (!rhiD->createDefaultRenderPass(&rp->rp,
                                       m_depthStencil != nullptr,
                                       samples,
                                       colorFormat))
    {
        delete rp;
        return nullptr;
    }

    rp->ownsRp = true;
    return rp;
}

bool QVkSwapChain::ensureSurface()
{
    // Do nothing when already done, however window may change so check the
    // surface is still the same. Some of the queries below are very expensive
    // with some implementations so it is important to do the rest only once
    // per surface.

    Q_ASSERT(m_window);
    VkSurfaceKHR surf = QVulkanInstance::surfaceForWindow(m_window);
    if (!surf) {
        qWarning("Failed to get surface for window");
        return false;
    }
    if (surface == surf)
        return true;

    surface = surf;

    QRHI_RES_RHI(QRhiVulkan);
    if (rhiD->gfxQueueFamilyIdx != -1) {
        if (!rhiD->inst->supportsPresent(rhiD->physDev, rhiD->gfxQueueFamilyIdx, m_window)) {
            qWarning("Presenting not supported on this window");
            return false;
        }
    }

    if (!rhiD->vkGetPhysicalDeviceSurfaceCapabilitiesKHR) {
        rhiD->vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
                    rhiD->inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
        rhiD->vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
                    rhiD->inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
        if (!rhiD->vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !rhiD->vkGetPhysicalDeviceSurfaceFormatsKHR) {
            qWarning("Physical device surface queries not available");
            return false;
        }
    }

    quint32 formatCount = 0;
    rhiD->vkGetPhysicalDeviceSurfaceFormatsKHR(rhiD->physDev, surface, &formatCount, nullptr);
    QVector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount)
        rhiD->vkGetPhysicalDeviceSurfaceFormatsKHR(rhiD->physDev, surface, &formatCount, formats.data());

    // Pick the preferred format, if there is one.
    if (!formats.isEmpty() && formats[0].format != VK_FORMAT_UNDEFINED) {
        colorFormat = formats[0].format;
        colorSpace = formats[0].colorSpace;
    }

    // When sRGB is requested just try a hardcoded format for now.
    if (m_flags.testFlag(sRGB)) {
        const VkFormat reqFmt = VK_FORMAT_B8G8R8A8_SRGB;
        auto r = std::find_if(formats.cbegin(), formats.cend(),
                              [reqFmt](const VkSurfaceFormatKHR &sfmt) { return sfmt.format == reqFmt; });
        if (r != formats.cend()) {
            colorFormat = r->format;
            colorSpace = r->colorSpace;
        }
    }

    samples = rhiD->effectiveSampleCount(m_sampleCount);

    return true;
}

bool QVkSwapChain::buildOrResize()
{
    if (m_target) {
        if (sc)
            release();
        QVulkanWindow *vkw = qobject_cast<QVulkanWindow *>(m_target);
        if (vkw) {
            rtWrapper.d.rp = QRHI_RES(QVkRenderPassDescriptor, m_renderPassDesc);
            Q_ASSERT(rtWrapper.d.rp && rtWrapper.d.rp->rp);
            m_currentPixelSize = pixelSize = rtWrapper.d.pixelSize = vkw->swapChainImageSize();
            rtWrapper.d.dpr = vkw->devicePixelRatio();
            rtWrapper.d.colorAttCount = 1;
            rtWrapper.d.dsAttCount = 1;
            rtWrapper.d.resolveAttCount = vkw->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 1 : 0;
            wrapWindow = vkw;
            return true;
        }
        return false;
    }

    QRHI_RES_RHI(QRhiVulkan);
    if (!window)
        rhiD->swapchains.insert(this);

    // Can be called multiple times due to window resizes - that is not the
    // same as a simple release+build (as with other resources). Thus no
    // release() here. See recreateSwapChain().

    // except if the window actually changes
    if (window && window != m_window)
        release();

    window = m_window;
    m_currentPixelSize = surfacePixelSize();
    pixelSize = m_currentPixelSize;

    if (!rhiD->recreateSwapChain(this))
        return false;

    if (m_depthStencil && m_depthStencil->sampleCount() != m_sampleCount) {
        qWarning("Depth-stencil buffer's sampleCount (%d) does not match color buffers' sample count (%d). Expect problems.",
                 m_depthStencil->sampleCount(), m_sampleCount);
    }
    if (m_depthStencil && m_depthStencil->pixelSize() != pixelSize) {
        qWarning("Depth-stencil buffer's size (%dx%d) does not match the surface size (%dx%d). Expect problems.",
                 m_depthStencil->pixelSize().width(), m_depthStencil->pixelSize().height(),
                 pixelSize.width(), pixelSize.height());
    }

    if (!m_renderPassDesc)
        qWarning("QVkSwapChain: No renderpass descriptor set. See newCompatibleRenderPassDescriptor() and setRenderPassDescriptor().");

    rtWrapper.d.rp = QRHI_RES(QVkRenderPassDescriptor, m_renderPassDesc);
    Q_ASSERT(rtWrapper.d.rp && rtWrapper.d.rp->rp);

    rtWrapper.d.pixelSize = pixelSize;
    rtWrapper.d.dpr = window->devicePixelRatio();
    rtWrapper.d.colorAttCount = 1;
    if (m_depthStencil) {
        rtWrapper.d.dsAttCount = 1;
        ds = QRHI_RES(QVkRenderBuffer, m_depthStencil);
    } else {
        rtWrapper.d.dsAttCount = 0;
        ds = nullptr;
    }
    if (samples > VK_SAMPLE_COUNT_1_BIT)
        rtWrapper.d.resolveAttCount = 1;
    else
        rtWrapper.d.resolveAttCount = 0;

    for (int i = 0; i < bufferCount; ++i) {
        QVkSwapChain::ImageResources &image(imageRes[i]);
        VkImageView views[3] = { // color, ds, resolve
            samples > VK_SAMPLE_COUNT_1_BIT ? image.msaaImageView : image.imageView,
            ds ? ds->imageView : VK_NULL_HANDLE,
            samples > VK_SAMPLE_COUNT_1_BIT ? image.imageView : VK_NULL_HANDLE
        };

        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = rtWrapper.d.rp->rp;
        fbInfo.attachmentCount = rtWrapper.d.colorAttCount + rtWrapper.d.dsAttCount + rtWrapper.d.resolveAttCount;
        fbInfo.pAttachments = views;
        fbInfo.width = pixelSize.width();
        fbInfo.height = pixelSize.height();
        fbInfo.layers = 1;

        VkResult err = rhiD->df->vkCreateFramebuffer(rhiD->dev, &fbInfo, nullptr, &image.fb);
        if (err != VK_SUCCESS) {
            qWarning("Failed to create framebuffer: %d", err);
            return false;
        }
    }

    wrapWindow = nullptr;
    return true;
}

QT_END_NAMESPACE
