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

class QVkRenderPrivate
{
public:
    void create();
    void destroy();

    bool recreateSwapChain(VkSurfaceKHR surface, const QSize &pixelSize, QVkRender::SurfaceImportFlags flags, QVkSwapChain *swapChain);
    void releaseSwapChain(QVkSwapChain *swapChain);

    VkFormat optimalDepthStencilFormat();
    bool createDefaultRenderPass(VkRenderPass *rp, bool hasDepthStencil);
    bool ensurePipelineCache();
    VkShaderModule createShader(const QByteArray &spirv);

    void prepareNewFrame();
    void finishFrame();
    void executeDeferredReleases(bool goingDown = false);

    QVulkanInstance *inst;
    VkPhysicalDevice physDev;
    VkDevice dev;
    VkCommandPool cmdPool;
    VkQueue gfxQueue;
    VmaAllocator allocator;
    QVulkanFunctions *f = nullptr;
    QVulkanDeviceFunctions *df = nullptr;

    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;

    VkFormat dsFormat = VK_FORMAT_UNDEFINED;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VkColorSpaceKHR(0); // this is in fact VK_COLOR_SPACE_SRGB_NONLINEAR_KHR

    VkPipelineCache pipelineCache = VK_NULL_HANDLE;

    QMatrix4x4 clipCorrectMatrix;

    int currentFrameSlot = 0; // 0..FRAMES_IN_FLIGHT-1
    bool inFrame = false;
    int finishedFrameCount = 0;

    struct DeferredReleaseEntry {
        enum Type {
            PipelineState,
            Buffer
        };
        Type type;
        int lastActiveFrameSlot; // -1 if not used otherwise 0..FRAMES_IN_FLIGHT-1
        union {
            struct {
                VkPipeline pipeline;
                VkPipelineLayout layout;
            } pipelineState;
            struct {
                VkBuffer buffer;
                QVkAlloc allocation;
            } buffer[QVK_FRAMES_IN_FLIGHT];
        };
    };
    QVector<DeferredReleaseEntry> releaseQueue;
};

QT_END_NAMESPACE

#endif
