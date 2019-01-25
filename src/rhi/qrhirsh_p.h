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

#ifndef QRHIRSH_P_H
#define QRHIRSH_P_H

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

#include "qtrhiglobal_p.h"
#include "qrhi_p.h"

#ifndef QT_NO_OPENGL
#include "qrhigles2.h"
#endif
#if QT_CONFIG(vulkan)
#include "qrhivulkan.h"
#endif
#ifdef Q_OS_WIN
#include "qrhid3d11.h"
#endif
#ifdef Q_OS_DARWIN
#include "qrhimetal.h"
#endif

#include <QMutex>

QT_BEGIN_NAMESPACE

class Q_RHI_PRIVATE_EXPORT QRhiResourceSharingHostPrivate
{
public:
    static QRhiResourceSharingHostPrivate *get(QRhiResourceSharingHost *h) { return h->d; }

    QMutex mtx;
    int rhiCount = 0;

#ifndef QT_NO_OPENGL
    struct {
        QOpenGLContext *dummyShareContext = nullptr;
        void *releaseQueue = nullptr;
    } d_gles2;
#endif
#if QT_CONFIG(vulkan)
    struct {
        VkPhysicalDevice physDev = VK_NULL_HANDLE;
        VkDevice dev = VK_NULL_HANDLE;
        void *allocator = nullptr;
        int gfxQueueFamilyIdx = -1;
        QVulkanDeviceFunctions *df = nullptr;
        void *releaseQueue = nullptr;
    } d_vulkan;
#endif
#ifdef Q_OS_WIN
    QRhiD3D11NativeHandles d_d3d11;
#endif
#ifdef Q_OS_DARWIN
    struct {
        void *dev = nullptr;
    } d_metal;
#endif
};

QT_END_NAMESPACE

#endif
