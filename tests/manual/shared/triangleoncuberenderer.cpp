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

#include "triangleoncuberenderer.h"
#include <QFile>
#include <QBakedShader>

// ugliness borrowed from qtdeclarative/examples/quick/rendercontrol/cuberenderer.cpp
static float vertexData[] = { // Y up, CW
    -0.5, 0.5, 0.5, 0.5,-0.5,0.5,-0.5,-0.5,0.5,
    0.5, -0.5, 0.5, -0.5,0.5,0.5,0.5,0.5,0.5,
    -0.5, -0.5, -0.5, 0.5,-0.5,-0.5,-0.5,0.5,-0.5,
    0.5, 0.5, -0.5, -0.5,0.5,-0.5,0.5,-0.5,-0.5,

    0.5, -0.5, -0.5, 0.5,-0.5,0.5,0.5,0.5,-0.5,
    0.5, 0.5, 0.5, 0.5,0.5,-0.5,0.5,-0.5,0.5,
    -0.5, 0.5, -0.5, -0.5,-0.5,0.5,-0.5,-0.5,-0.5,
    -0.5, -0.5, 0.5, -0.5,0.5,-0.5,-0.5,0.5,0.5,

    0.5, 0.5,  -0.5, -0.5, 0.5,  0.5,  -0.5,  0.5,  -0.5,
    -0.5,  0.5,  0.5,  0.5,  0.5,  -0.5, 0.5, 0.5,  0.5,
    -0.5,  -0.5, -0.5, -0.5, -0.5, 0.5,  0.5, -0.5, -0.5,
    0.5, -0.5, 0.5,  0.5,  -0.5, -0.5, -0.5,  -0.5, 0.5,

    // texcoords
    0.0f,1.0f, 1.0f,0.0f, 1.0f,1.0f,
    1.0f,0.0f, 0.0f,1.0f, 0.0f,0.0f,
    1.0f,0.0f, 1.0f,1.0f, 0.0f,0.0f,
    0.0f,1.0f, 0.0f,0.0f, 1.0f,1.0f,

    1.0f,0.0f, 1.0f,1.0f, 0.0f,0.0f,
    0.0f,1.0f, 0.0f,0.0f, 1.0f,1.0f,
    0.0f,1.0f, 1.0f,0.0f, 1.0f,1.0f,
    1.0f,0.0f, 0.0f,1.0f, 0.0f,0.0f,

    0.0f,0.0f, 1.0f,1.0f, 1.0f,0.0f,
    1.0f,1.0f, 0.0f,0.0f, 0.0f,1.0f,
    1.0f,1.0f, 1.0f,0.0f, 0.0f,1.0f,
    0.0f,0.0f, 0.0f,1.0f, 1.0f,0.0f
};

static QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

static const QSize OFFSCREEN_SIZE(512, 512);

void TriangleOnCubeRenderer::initResources()
{
    m_vbuf = new QVkBuffer(QVkBuffer::StaticType, QVkBuffer::VertexBuffer, sizeof(vertexData));
    m_r->createBuffer(m_vbuf);
    m_vbufReady = false;

    m_ubuf = new QVkBuffer(QVkBuffer::DynamicType, QVkBuffer::UniformBuffer, 64);
    m_r->createBuffer(m_ubuf);

    m_tex = new QVkTexture(QVkTexture::RGBA8, OFFSCREEN_SIZE, QVkTexture::NoUploadContents | QVkTexture::RenderTarget);
    m_r->createTexture(m_tex);

    m_sampler = new QVkSampler(QVkSampler::Linear, QVkSampler::Linear, QVkSampler::Linear, QVkSampler::Repeat, QVkSampler::Repeat);
    m_r->createSampler(m_sampler);

    m_srb = new QVkShaderResourceBindings;
    const auto ubufVisibility = QVkShaderResourceBindings::Binding::VertexStage | QVkShaderResourceBindings::Binding::FragmentStage;
    m_srb->bindings = {
        QVkShaderResourceBindings::Binding::uniformBuffer(0, ubufVisibility, m_ubuf, 0, 64),
        QVkShaderResourceBindings::Binding::sampledTexture(1, QVkShaderResourceBindings::Binding::FragmentStage, m_tex, m_sampler)
    };
    m_r->createShaderResourceBindings(m_srb);

    m_rt = new QVkTextureRenderTarget(m_tex);
    m_r->createTextureRenderTarget(m_rt);

    m_offscreenTriangle.setVkRender(m_r);
    m_offscreenTriangle.initResources();
    m_offscreenTriangle.setScale(2);
}

void TriangleOnCubeRenderer::initOutputDependentResources(const QVkRenderPass *rp, const QSize &pixelSize, QVkRenderBuffer *ds)
{
    m_ps = new QVkGraphicsPipeline;

    m_ps->targetBlends = { QVkGraphicsPipeline::TargetBlend() };

    m_ps->depthTest = true;
    m_ps->depthWrite = true;
    m_ps->depthOp = QVkGraphicsPipeline::Less;

    m_ps->cullMode = QVkGraphicsPipeline::Back;
    m_ps->frontFace = QVkGraphicsPipeline::CW;

    m_ps->sampleCount = SAMPLES;

    QBakedShader vs = getShader(QLatin1String(":/texture.vert.qsb"));
    Q_ASSERT(vs.isValid());
    QBakedShader fs = getShader(QLatin1String(":/texture.frag.qsb"));
    Q_ASSERT(fs.isValid());
    m_ps->shaderStages = {
        QVkGraphicsShaderStage(QVkGraphicsShaderStage::Vertex,
        vs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader),
        QVkGraphicsShaderStage(QVkGraphicsShaderStage::Fragment,
        fs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader)
    };

    QVkVertexInputLayout inputLayout;
    inputLayout.bindings = {
        QVkVertexInputLayout::Binding(3 * sizeof(float)),
        QVkVertexInputLayout::Binding(2 * sizeof(float))
    };
    inputLayout.attributes = {
        QVkVertexInputLayout::Attribute(0, 0, QVkVertexInputLayout::Attribute::Float3, 0, "POSITION"),
        QVkVertexInputLayout::Attribute(1, 1, QVkVertexInputLayout::Attribute::Float2, 0, "TEXCOORD")
    };

    m_ps->vertexInputLayout = inputLayout;
    m_ps->shaderResourceBindings = m_srb;
    m_ps->renderPass = rp;

    m_r->createGraphicsPipeline(m_ps);

    m_proj = m_r->openGLCorrectionMatrix();
    m_proj.perspective(45.0f, pixelSize.width() / (float) pixelSize.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);

    m_offscreenTriangle.initOutputDependentResources(m_rt->renderPass(), pixelSize);
}

void TriangleOnCubeRenderer::releaseResources()
{
    m_offscreenTriangle.releaseResources();

    if (m_srb) {
        m_r->releaseLater(m_srb);
        delete m_srb;
        m_srb = nullptr;
    }

    if (m_rt) {
        m_r->releaseLater(m_rt);
        delete m_rt;
        m_rt = nullptr;
    }

    if (m_sampler) {
        m_r->releaseLater(m_sampler);
        delete m_sampler;
        m_sampler = nullptr;
    }

    if (m_tex) {
        m_r->releaseLater(m_tex);
        delete m_tex;
        m_tex = nullptr;
    }

    if (m_ubuf) {
        m_r->releaseLater(m_ubuf);
        delete m_ubuf;
        m_ubuf = nullptr;
    }

    if (m_vbuf) {
        m_r->releaseLater(m_vbuf);
        delete m_vbuf;
        m_vbuf = nullptr;
    }
}

void TriangleOnCubeRenderer::releaseOutputDependentResources()
{
    m_offscreenTriangle.releaseOutputDependentResources();

    if (m_ps) {
        m_r->releaseLater(m_ps);
        delete m_ps;
        m_ps = nullptr;
    }
}

void TriangleOnCubeRenderer::queueCopy(QVkCommandBuffer *cb)
{
    if (!m_vbufReady) {
        m_vbufReady = true;
        m_r->uploadStaticBuffer(cb, m_vbuf, vertexData);
    }

    m_offscreenTriangle.queueCopy(cb);
}

void TriangleOnCubeRenderer::queueOffscreenPass(QVkCommandBuffer *cb)
{
    const QVector4D clearColor(0.0f, 0.4f, 0.7f, 1.0f);
    const QVkClearValue clearValues[] = {
        clearColor,
        QVkClearValue(1.0f, 0)
    };
    m_r->beginPass(m_rt, cb, clearValues);
    m_offscreenTriangle.queueDraw(cb, OFFSCREEN_SIZE);
    m_r->endPass(cb);
}

void TriangleOnCubeRenderer::queueDraw(QVkCommandBuffer *cb, const QSize &outputSizeInPixels)
{
    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.translate(m_translation);
    mvp.rotate(m_rotation, 1, 0, 0);
    m_r->updateDynamicBuffer(m_ubuf, 0, 64, mvp.constData());

    m_r->setViewport(cb, QVkViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->setScissor(cb, QVkScissor(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));

    m_r->setGraphicsPipeline(cb, m_ps);
    m_r->setVertexInput(cb, 0, { { m_vbuf, 0 }, { m_vbuf, 36 * 3 * sizeof(float) } });
    m_r->draw(cb, 36);
}
