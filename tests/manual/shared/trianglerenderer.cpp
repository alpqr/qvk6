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
    static float vertexData[] = { // Y up, CCW
         0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  -0.5f,   0.0f, 0.0f, 1.0f
    };
    m_triBuf = new QVkBuffer;
    m_triBuf->type = QVkBuffer::StaticType;
    m_triBuf->usage = QVkBuffer::VertexBuffer;
    m_triBuf->size = sizeof(vertexData);
    m_r->createBuffer(m_triBuf, vertexData);

    m_mvpBuf = new QVkBuffer;
    m_mvpBuf->type = QVkBuffer::DynamicType;
    m_mvpBuf->usage = QVkBuffer::UniformBuffer;
    m_mvpBuf->size = 4 * 4 * sizeof(float);
    m_r->createBuffer(m_mvpBuf);

    m_srb = new QVkShaderResourceBindings;
    QVkShaderResourceBindings::Binding mvpBinding;
    mvpBinding.binding = 0;
    mvpBinding.stage = QVkGraphicsShaderStage::Vertex;
    mvpBinding.type = QVkShaderResourceBindings::Binding::UniformBuffer;
    mvpBinding.uniformBuffer.buf = m_mvpBuf;
    m_srb->bindings = { mvpBinding };
    m_r->createShaderResourceBindings(m_srb);
}

void TriangleRenderer::initRenderPassDependentResources(const QVkRenderPass *rp)
{
    m_ps = new QVkGraphicsPipelineState;

    QBakedShader vs = getShader(QLatin1String(":/color.vert.qsb"));
    Q_ASSERT(vs.isValid());
    QVkGraphicsShaderStage vsStage;
    vsStage.type = QVkGraphicsShaderStage::Vertex;
    vsStage.spirv = vs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader;
    QBakedShader fs = getShader(QLatin1String(":/color.frag.qsb"));
    Q_ASSERT(fs.isValid());
    QVkGraphicsShaderStage fsStage;
    fsStage.type = QVkGraphicsShaderStage::Fragment;
    fsStage.spirv = fs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader;

    m_ps->shaderStages = { vsStage, fsStage };

    QVkVertexInputLayout inputLayout;
    QVkVertexInputLayout::Binding inputBinding;
    inputBinding.stride = 5 * sizeof(float);
    inputLayout.bindings = { inputBinding };
    QVkVertexInputLayout::Attribute inputAttr1;
    inputAttr1.binding = 0;
    inputAttr1.location = 0;
    inputAttr1.format = QVkVertexInputLayout::Attribute::Float2;
    inputAttr1.offset = 0;
    inputAttr1.semanticName = "POSITION";
    QVkVertexInputLayout::Attribute inputAttr2;
    inputAttr2.binding = 0;
    inputAttr2.location = 1;
    inputAttr2.format = QVkVertexInputLayout::Attribute::Float3;
    inputAttr2.offset = 2 * sizeof(float);
    inputAttr1.semanticName = "COLOR";
    inputLayout.attributes = { inputAttr1, inputAttr2 };

    m_ps->vertexInputLayout = inputLayout;
    m_ps->renderPass = rp;
    m_ps->shaderResourceBindings = m_srb;

    m_r->createGraphicsPipelineState(m_ps);
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

void TriangleRenderer::releaseRenderPassDependentResources()
{
    if (m_ps) {
        m_r->scheduleRelease(m_ps);
        delete m_ps;
        m_ps = nullptr;
    }
}

void TriangleRenderer::queueDraw(QVkCommandBuffer *cb, const QSize &outputSizeInPixels)
{
    QMatrix4x4 m = m_r->openGLCorrectionMatrix();
    m.perspective(45.0f, outputSizeInPixels.width() / (float) outputSizeInPixels.height(), 0.01f, 100.0f);
    m.translate(0, 0, -4);
    m.rotate(m_rotation, 0, 1, 0);
    m_rotation += 1;
    m_r->updateBuffer(m_mvpBuf, 0, 4 * 4 * sizeof(float), m.constData());

    QVkViewport vp;
    vp.r = QRectF(QPointF(0, 0), outputSizeInPixels);
    m_r->cmdViewport(cb, vp);
    QVkScissor s;
    s.r = vp.r;
    m_r->cmdScissor(cb, s);

    m_r->cmdSetGraphicsPipelineState(cb, m_ps);
    m_r->cmdSetVertexBuffer(cb, 0, m_triBuf, 0);
    m_r->cmdDraw(cb, 3, 1, 0, 0);
}
