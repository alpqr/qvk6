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

#ifndef QVKRENDER_H
#define QVKRENDER_H

#include <QtVkRender/qtvkrglobal.h>
#include <QVector4D>
#include <QVector2D>
#include <QSize>

QT_BEGIN_NAMESPACE

class QVkRenderPrivate;
class QVulkanWindow;

struct QVkClearValue
{
    QVkClearValue(const QVector4D &rgba_) : rgba(rgba_) { }
    QVkClearValue(float d_, uint32_t s_) : d(d_), s(s_), isDepthStencil(true) { }
    QVector4D rgba;
    float d = 1;
    uint32_t s = 0;
    bool isDepthStencil = false;
};

typedef void * QVkAlloc;

struct QVkBuffer
{
    QVkAlloc allocation = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    // ### going to be a tad more complicated than this due to multiple frames in flight
};

struct QVkTexture
{
    QVkAlloc allocation = nullptr;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    // ### for now
};

struct QVkSampler
{
};

struct QVkCommandBuffer
{
    VkCommandBuffer cb = VK_NULL_HANDLE;
};

struct QVkSwapChain
{
    static const int DEFAULT_BUFFER_COUNT = 2;
    static const int MAX_BUFFER_COUNT = 3;
    static const int FRAME_LAG = 2;

    QSize pixelSize;
    bool supportsReadback = false;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    int bufferCount = 0;
    bool hasDepthStencil = false;

    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        QVkCommandBuffer cmdBuf;
        VkFence cmdFence = VK_NULL_HANDLE;
        bool cmdFenceWaitable = false;
        VkFramebuffer fb = VK_NULL_HANDLE;
    } imageRes[MAX_BUFFER_COUNT];

    struct FrameResources {
        VkFence fence = VK_NULL_HANDLE;
        bool fenceWaitable = false;
        VkSemaphore imageSem = VK_NULL_HANDLE;
        VkSemaphore drawSem = VK_NULL_HANDLE;
        bool imageAcquired = false;
        bool imageSemWaitable = false;
    } frameRes[FRAME_LAG];

    uint32_t currentImage = 0;
    uint32_t currentFrame = 0;
};

struct QVkRenderTarget
{
    VkFramebuffer fb = VK_NULL_HANDLE;
    VkRenderPass rp = VK_NULL_HANDLE;
    QSize pixelSize;
    int attCount = 0;
};

class Q_VKR_EXPORT QVkRender
{
public:
    struct InitParams {
        QVulkanInstance *inst = nullptr;
        VkPhysicalDevice physDev = VK_NULL_HANDLE;
        VkDevice dev = VK_NULL_HANDLE;
        VkCommandPool cmdPool = VK_NULL_HANDLE;
        VkQueue gfxQueue = VK_NULL_HANDLE;
    };

    enum SurfaceImportFlag {
        ImportWithAlpha = 0x01,
        ImportWithDepthStencil = 0x02
    };
    Q_DECLARE_FLAGS(SurfaceImportFlags, SurfaceImportFlag)

    enum FrameOpResult {
        FrameOpSuccess = 0,
        FrameOpError,
        FrameOpSwapChainOutOfDate,
        FrameOpDeviceLost
    };

    QVkRender(const InitParams &params);
    ~QVkRender();

    VkFormat optimalDepthStencilFormat() const;

    /* some basic use cases:

      1. render to a QVulkanWindow from a startNextFrame() implementation
         This is simple, repeat on every frame:
           importVulkanWindowCurrentFrame
           beginPass(rt, cb, ...)
           ...
           endPass(cb)

      2. render to a QWindow (must be VulkanSurface), VkDevice and friends must be provided as well
           call importSurface to create a swapchain + call it whenever the size is different than before
           call releaseSwapChain on QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed
           Then on every frame:
             beginFrame
             beginPass(sc, ...)
             ...
             endPass(sc)
             endFrame (this queues the Present, begin/endFrame manages double buffering internally)

      3. render to a texture:
        3.1 as part of normal frame:
          createTexture
          createRenderTarget
          ...
          cb = currentFrameCommandBuffer(sc)
          beginPass(rt, cb, ...)
          ...
          endPass(cb)

        TBD. everything is subject to change.
      */

    bool createTexture(int whatever, QVkTexture *outTex);
    void releaseTexture(QVkTexture *tex);
    bool createRenderTarget(QVkTexture *color, QVkTexture *ds, QVkRenderTarget *outRt);
    void releaseRenderTarget(QVkRenderTarget *rt);

    bool importSurface(VkSurfaceKHR surface, const QSize &pixelSize, SurfaceImportFlags flags, QVkTexture *depthStencil, QVkSwapChain *outSwapChain);
    void releaseSwapChain(QVkSwapChain *swapChain);
    QVkCommandBuffer *currentFrameCommandBuffer(QVkSwapChain *swapChain);
    FrameOpResult beginFrame(QVkSwapChain *sc);
    FrameOpResult endFrame(QVkSwapChain *sc);
    void beginPass(QVkSwapChain *sc, const QVkClearValue *clearValues);
    void endPass(QVkSwapChain *sc);

    void importVulkanWindowCurrentFrame(QVulkanWindow *window, QVkRenderTarget *outRt, QVkCommandBuffer *outCb);
    void beginPass(QVkRenderTarget *rt, QVkCommandBuffer *cb, const QVkClearValue *clearValues);
    void endPass(QVkCommandBuffer *cb);

private:
    QVkRenderPrivate *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVkRender::SurfaceImportFlags)

QT_END_NAMESPACE

#endif
