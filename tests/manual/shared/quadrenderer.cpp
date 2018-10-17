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

#include "quadrenderer.h"
#include <QFile>
#include <QBakedShader>

// Renders a quad using indexed drawing. No QRhiGraphicsPipeline is created, it
// expects to reuse the one created by TriangleRenderer. A separate
// QRhiShaderResourceBindings is still needed, this will override the one the
// QRhiGraphicsPipeline references.

static float vertexData[] =
{ // Y up (note m_proj), CCW
  -0.5f,   0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
  -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
  0.5f,   -0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
  0.5f,   0.5f,    1.0f, 0.0f, 0.0f,   1.0f, 1.0f
};

static quint16 indexData[] =
{
    0, 1, 2, 0, 2, 3
};

void QuadRenderer::initResources()
{
    m_vbuf = m_r->createBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
    m_vbuf->build();
    m_vbufReady = false;

    m_ibuf = m_r->createBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, sizeof(indexData));
    m_ibuf->build();

    m_ubuf = m_r->createBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    m_ubuf->build();

    m_srb = m_r->createShaderResourceBindings();
    const auto ubufVisibility = QRhiShaderResourceBindings::Binding::VertexStage | QRhiShaderResourceBindings::Binding::FragmentStage;
    m_srb->bindings = {
        QRhiShaderResourceBindings::Binding::uniformBuffer(0, ubufVisibility, m_ubuf)
    };
    m_srb->build();
}

void QuadRenderer::setPipeline(QRhiGraphicsPipeline *ps, const QSize &pixelSize)
{
    m_ps = ps;

    m_proj = m_r->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, pixelSize.width() / (float) pixelSize.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);
}

void QuadRenderer::releaseResources()
{
    if (m_srb) {
        m_srb->releaseAndDestroy();
        m_srb = nullptr;
    }

    if (m_ubuf) {
        m_ubuf->releaseAndDestroy();
        m_ubuf = nullptr;
    }

    if (m_ibuf) {
        m_ibuf->releaseAndDestroy();
        m_ibuf = nullptr;
    }

    if (m_vbuf) {
        m_vbuf->releaseAndDestroy();
        m_vbuf = nullptr;
    }
}

void QuadRenderer::queueResourceUpdates(QRhiResourceUpdateBatch *resourceUpdates)
{
    if (!m_vbufReady) {
        m_vbufReady = true;
        resourceUpdates->uploadStaticBuffer(m_vbuf, vertexData);
        resourceUpdates->uploadStaticBuffer(m_ibuf, indexData);
    }

    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.translate(m_translation);
    mvp.rotate(m_rotation, 0, 1, 0);
    resourceUpdates->updateDynamicBuffer(m_ubuf, 0, 64, mvp.constData());

    if (!m_opacityReady) {
        m_opacityReady = true;
        const float opacity = 1.0f;
        resourceUpdates->updateDynamicBuffer(m_ubuf, 64, 4, &opacity);
    }
}

void QuadRenderer::queueDraw(QRhiCommandBuffer *cb, const QSize &/*outputSizeInPixels*/)
{
    m_r->setGraphicsPipeline(cb, m_ps, m_srb);
    //m_r->setViewport(cb, QRhiViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->setVertexInput(cb, 0, { { m_vbuf, 0 } }, m_ibuf, 0, QRhi::IndexUInt16);
    m_r->drawIndexed(cb, 6);
}
