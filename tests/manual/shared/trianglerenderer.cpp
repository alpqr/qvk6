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

QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

// note the dependencies:
//   ps depends on srb that depends on uniform buffer(s)
//   ps depends on vertex/index buffers
//   ps depends on the renderpass -> now it may be tied to the swapchain too

void TriangleRenderer::initResources()
{
    static float vertexData[] = { // Y up (note m_proj), CCW
         0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  -0.5f,   0.0f, 0.0f, 1.0f
    };
    m_triBuf = new QVkBuffer(QVkBuffer::StaticType, QVkBuffer::VertexBuffer, sizeof(vertexData));
    m_r->createBuffer(m_triBuf, vertexData);

    m_mvpBuf = new QVkBuffer(QVkBuffer::DynamicType, QVkBuffer::UniformBuffer, 4 * 4 * sizeof(float));
    m_r->createBuffer(m_mvpBuf);

    m_srb = new QVkShaderResourceBindings;
    m_srb->bindings = {
        QVkShaderResourceBindings::Binding::uniformBuffer(0, QVkShaderResourceBindings::Binding::VertexStage, m_mvpBuf)
    };
    m_r->createShaderResourceBindings(m_srb);
}

void TriangleRenderer::initOutputDependentResources(const QVkRenderPass *rp, const QSize &pixelSize)
{
    m_ps = new QVkGraphicsPipelineState;

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

    if (m_mvpBuf) {
        m_r->scheduleRelease(m_mvpBuf);
        delete m_mvpBuf;
        m_mvpBuf = nullptr;
    }

    if (m_triBuf) {
        m_r->scheduleRelease(m_triBuf);
        delete m_triBuf;
        m_triBuf = nullptr;
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

void TriangleRenderer::queueDraw(QVkCommandBuffer *cb, const QSize &outputSizeInPixels)
{
    m_rotation += 1;
    QMatrix4x4 mvp = m_proj;
    mvp.rotate(m_rotation, 0, 1, 0);
    m_r->updateBuffer(m_mvpBuf, 0, 4 * 4 * sizeof(float), mvp.constData());

    m_r->cmdViewport(cb, QVkViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->cmdScissor(cb, QVkScissor(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));

    m_r->cmdSetGraphicsPipelineState(cb, m_ps);
    m_r->cmdSetVertexBuffer(cb, 0, m_triBuf, 0);
    m_r->cmdDraw(cb, 3, 1, 0, 0);
}
