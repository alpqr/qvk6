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

struct QVkClearValue
{
    QVkClearValue(const QVector4D &rgba_) : rgba(rgba_) { }
    QVkClearValue(float d_, uint32_t s_) : d(d_), s(s_), isDepthStencil(true) { }
    QVector4D rgba;
    float d = 1;
    uint32_t s = 0;
    bool isDepthStencil = false;
};

class Q_VKR_EXPORT QVkRender
{
public:
    struct InitParams {
        QVulkanInstance *inst;
        VkPhysicalDevice physDev;
        VkDevice dev;
    };

    QVkRender(const InitParams &params);
    ~QVkRender();

    void beginPass(VkCommandBuffer cb, VkRenderPass rp, VkFramebuffer fb, const QSize &size, const QVkClearValue *clearValues, int n);
    void endPass(VkCommandBuffer cb);

private:
    QVkRenderPrivate *d;
};

QT_END_NAMESPACE

#endif
