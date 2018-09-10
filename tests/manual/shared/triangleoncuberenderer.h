/****************************************************************************
 **
 ** Copyright (C) 2018 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the test suite of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
 ** General Public License version 3 as published by the Free Software
 ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
 ** included in the packaging of this file. Please review the following
 ** information to ensure the GNU General Public License requirements will
 ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#ifndef TRIANGLEONCUBERENDERER_H
#define TRIANGLEONCUBERENDERER_H

#include "trianglerenderer.h"

class TriangleOnCubeRenderer
{
public:
    void setVkRender(QVkRender *r) { m_r = r; }
    void setTranslation(const QVector3D &v) { m_translation = v; }
    bool isPipelineInitialized() const { return m_ps != nullptr; }
    void initResources();
    void releaseResources();
    void initOutputDependentResources(const QVkRenderPass *rp, const QSize &pixelSize, QVkRenderBuffer *ds);
    void releaseOutputDependentResources();
    void queueCopy(QVkCommandBuffer *cb);
    void queueOffscreenPass(QVkCommandBuffer *cb);
    void queueDraw(QVkCommandBuffer *cb, const QSize &outputSizeInPixels);

    static const int SAMPLES = 1; // 1 (or 0) = no MSAA; 2, 4, 8 = MSAA

private:
    QVkRender *m_r;

    QVkBuffer *m_vbuf = nullptr;
    bool m_vbufReady = false;
    QVkBuffer *m_ubuf = nullptr;
    QVkTexture *m_tex = nullptr;
    QVkSampler *m_sampler = nullptr;
    QVkTextureRenderTarget *m_rt = nullptr;
    QVkShaderResourceBindings *m_srb = nullptr;
    QVkGraphicsPipeline *m_ps = nullptr;

    QVector3D m_translation;
    QMatrix4x4 m_proj;
    float m_rotation = 0;

    TriangleRenderer m_offscreenTriangle;
};

#endif
