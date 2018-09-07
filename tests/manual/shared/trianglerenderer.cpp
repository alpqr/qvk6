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

#include "trianglerenderer.h"
#include <QFile>
#include <QBakedShader>

static float vertexData[] = { // Y up (note m_proj), CCW
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f
};

QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

void TriangleRenderer::initResources()
{
    m_vbuf = new QVkBuffer(QVkBuffer::StaticType, QVkBuffer::VertexBuffer, sizeof(vertexData));
    m_r->createBuffer(m_vbuf);
    m_vbufReady = false;

    m_ubuf = new QVkBuffer(QVkBuffer::DynamicType, QVkBuffer::UniformBuffer, 64 + 4);
    m_r->createBuffer(m_ubuf);

    m_image = QImage(QLatin1String(":/qt256.png"));
    m_tex = new QVkTexture(QVkTexture::RGBA8, QSize(m_image.width(), m_image.height()));
    m_r->createTexture(m_tex);
    m_texReady = false;

    m_srb = new QVkShaderResourceBindings;
    const auto ubufVisibility = QVkShaderResourceBindings::Binding::VertexStage | QVkShaderResourceBindings::Binding::FragmentStage;
    m_srb->bindings = {
        QVkShaderResourceBindings::Binding::uniformBuffer(0, ubufVisibility, m_ubuf)
    };
    m_r->createShaderResourceBindings(m_srb);
}

// the ps depends on the renderpass -> so it is tied to the swapchain.
// on the other hand, srb and buffers are referenced from the ps but can be reused.
void TriangleRenderer::initOutputDependentResources(const QVkRenderPass *rp, const QSize &pixelSize)
{
    m_ps = new QVkGraphicsPipelineState;

    //m_ps->targetBlends = { QVkGraphicsPipelineState::TargetBlend() }; // no blending

    QVkGraphicsPipelineState::TargetBlend premulAlphaBlend; // convenient defaults...
    premulAlphaBlend.enable = true;
    m_ps->targetBlends = { premulAlphaBlend };

    m_ps->depthTest = true;
    m_ps->depthWrite = true;
    m_ps->depthOp = QVkGraphicsPipelineState::LessOrEqual;

    m_ps->sampleCount = SAMPLES;

    QBakedShader vs = getShader(QLatin1String(":/color.vert.qsb"));
    Q_ASSERT(vs.isValid());
    QBakedShader fs = getShader(QLatin1String(":/color.frag.qsb"));
    Q_ASSERT(fs.isValid());
    m_ps->shaderStages = {
        QVkGraphicsShaderStage(QVkGraphicsShaderStage::Vertex,
                               vs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader),
        QVkGraphicsShaderStage(QVkGraphicsShaderStage::Fragment,
                               fs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader)
    };

    QVkVertexInputLayout inputLayout;
    inputLayout.bindings = {
        QVkVertexInputLayout::Binding(5 * sizeof(float))
    };
    inputLayout.attributes = {
        QVkVertexInputLayout::Attribute(0, 0, QVkVertexInputLayout::Attribute::Float2, 0, "POSITION"),
        QVkVertexInputLayout::Attribute(0, 1, QVkVertexInputLayout::Attribute::Float3, 2 * sizeof(float), "COLOR")
    };

    m_ps->vertexInputLayout = inputLayout;
    m_ps->shaderResourceBindings = m_srb;
    m_ps->renderPass = rp;

    m_r->createGraphicsPipelineState(m_ps);

    m_proj = m_r->openGLCorrectionMatrix();
    m_proj.perspective(45.0f, pixelSize.width() / (float) pixelSize.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);
}

void TriangleRenderer::releaseResources()
{
    if (m_srb) {
        m_r->scheduleRelease(m_srb);
        delete m_srb;
        m_srb = nullptr;
    }

    if (m_tex) {
        m_r->scheduleRelease(m_tex);
        delete m_tex;
        m_tex = nullptr;
    }

    if (m_ubuf) {
        m_r->scheduleRelease(m_ubuf);
        delete m_ubuf;
        m_ubuf = nullptr;
    }

    if (m_vbuf) {
        m_r->scheduleRelease(m_vbuf);
        delete m_vbuf;
        m_vbuf = nullptr;
    }
}

void TriangleRenderer::releaseOutputDependentResources()
{
    if (m_ps) {
        m_r->scheduleRelease(m_ps);
        delete m_ps;
        m_ps = nullptr;
    }
}

void TriangleRenderer::queueCopy(QVkCommandBuffer *cb)
{
    if (!m_vbufReady) {
        m_vbufReady = true;
        m_r->cmdUploadStaticBuffer(cb, m_vbuf, vertexData);
    }

    if (!m_texReady) {
        m_texReady = true;
        m_r->cmdUploadTexture(cb, m_tex, m_image);
    }
}

void TriangleRenderer::queueDraw(QVkCommandBuffer *cb, const QSize &outputSizeInPixels)
{
    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.rotate(m_rotation, 0, 1, 0);
    m_r->updateDynamicBuffer(m_ubuf, 0, 64, mvp.constData());
    m_opacity += m_opacityDir * 0.005f;
    if (m_opacity < 0.0f || m_opacity > 1.0f) {
        m_opacityDir *= -1;
        m_opacity = qBound(0.0f, m_opacity, 1.0f);
    }
    m_r->updateDynamicBuffer(m_ubuf, 64, 4, &m_opacity);

    m_r->cmdViewport(cb, QVkViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->cmdScissor(cb, QVkScissor(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));

    m_r->cmdSetGraphicsPipelineState(cb, m_ps);
    m_r->cmdSetVertexBuffer(cb, 0, m_vbuf, 0);
    m_r->cmdDraw(cb, 3, 1, 0, 0);
}
