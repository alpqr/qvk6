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

#include "texturedcuberenderer.h"
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
    0.0f,0.0f, 1.0f,1.0f, 1.0f,0.0f,
    1.0f,1.0f, 0.0f,0.0f, 0.0f,1.0f,
    1.0f,1.0f, 1.0f,0.0f, 0.0f,1.0f,
    0.0f,0.0f, 0.0f,1.0f, 1.0f,0.0f,

    1.0f,1.0f, 1.0f,0.0f, 0.0f,1.0f,
    0.0f,0.0f, 0.0f,1.0f, 1.0f,0.0f,
    0.0f,0.0f, 1.0f,1.0f, 1.0f,0.0f,
    1.0f,1.0f, 0.0f,0.0f, 0.0f,1.0f,

    0.0f,1.0f, 1.0f,0.0f, 1.0f,1.0f,
    1.0f,0.0f, 0.0f,1.0f, 0.0f,0.0f,
    1.0f,0.0f, 1.0f,1.0f, 0.0f,0.0f,
    0.0f,1.0f, 0.0f,0.0f, 1.0f,1.0f
};

static QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

void TexturedCubeRenderer::initResources()
{
    m_vbuf = m_r->createBuffer(QRhiBuffer::StaticType, QRhiBuffer::VertexBuffer, sizeof(vertexData));
    m_vbuf->build();
    m_vbufReady = false;

    m_ubuf = m_r->createBuffer(QRhiBuffer::DynamicType, QRhiBuffer::UniformBuffer, 64);
    m_ubuf->build();

    m_image = QImage(QLatin1String(":/qt256.png")).mirrored().convertToFormat(QImage::Format_RGBA8888);
    m_tex = new QRhiTexture(QRhiTexture::RGBA8, QSize(m_image.width(), m_image.height()));
    m_r->createTexture(m_tex);

    m_sampler = m_r->createSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Repeat, QRhiSampler::Repeat);
    m_sampler->build();

    m_srb = new QRhiShaderResourceBindings;
    const auto ubufVisibility = QRhiShaderResourceBindings::Binding::VertexStage | QRhiShaderResourceBindings::Binding::FragmentStage;
    m_srb->bindings = {
        QRhiShaderResourceBindings::Binding::uniformBuffer(0, ubufVisibility, m_ubuf, 0, 64),
        QRhiShaderResourceBindings::Binding::sampledTexture(1, QRhiShaderResourceBindings::Binding::FragmentStage, m_tex, m_sampler)
    };
    m_r->createShaderResourceBindings(m_srb);
}

void TexturedCubeRenderer::initOutputDependentResources(const QRhiRenderPass *rp, const QSize &pixelSize)
{
    m_ps = new QRhiGraphicsPipeline;

    m_ps->targetBlends = { QRhiGraphicsPipeline::TargetBlend() };

    m_ps->depthTest = true;
    m_ps->depthWrite = true;
    m_ps->depthOp = QRhiGraphicsPipeline::Less;

    m_ps->cullMode = QRhiGraphicsPipeline::Back;
    m_ps->frontFace = QRhiGraphicsPipeline::CW;

    m_ps->sampleCount = SAMPLES;

    QBakedShader vs = getShader(QLatin1String(":/texture.vert.qsb"));
    Q_ASSERT(vs.isValid());
    QBakedShader fs = getShader(QLatin1String(":/texture.frag.qsb"));
    Q_ASSERT(fs.isValid());
    m_ps->shaderStages = {
        QRhiGraphicsShaderStage(QRhiGraphicsShaderStage::Vertex,
        vs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader),
        QRhiGraphicsShaderStage(QRhiGraphicsShaderStage::Fragment,
        fs.shader(QBakedShader::ShaderKey(QBakedShader::SpirvShader)).shader)
    };

    QRhiVertexInputLayout inputLayout;
    inputLayout.bindings = {
        QRhiVertexInputLayout::Binding(3 * sizeof(float)),
        QRhiVertexInputLayout::Binding(2 * sizeof(float))
    };
    inputLayout.attributes = {
        QRhiVertexInputLayout::Attribute(0, 0, QRhiVertexInputLayout::Attribute::Float3, 0, "POSITION"),
        QRhiVertexInputLayout::Attribute(1, 1, QRhiVertexInputLayout::Attribute::Float2, 0, "TEXCOORD")
    };

    m_ps->vertexInputLayout = inputLayout;
    m_ps->shaderResourceBindings = m_srb;
    m_ps->renderPass = rp;

    m_r->createGraphicsPipeline(m_ps);

    m_proj = m_r->openGLCorrectionMatrix();
    m_proj.perspective(45.0f, pixelSize.width() / (float) pixelSize.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);
}

void TexturedCubeRenderer::releaseResources()
{
    if (m_srb) {
        m_r->releaseLater(m_srb);
        delete m_srb;
        m_srb = nullptr;
    }

    if (m_sampler) {
        m_sampler->release();
        delete m_sampler;
        m_sampler = nullptr;
    }

    if (m_tex) {
        m_r->releaseLater(m_tex);
        delete m_tex;
        m_tex = nullptr;
    }

    if (m_ubuf) {
        m_ubuf->release();
        delete m_ubuf;
        m_ubuf = nullptr;
    }

    if (m_vbuf) {
        m_vbuf->release();
        delete m_vbuf;
        m_vbuf = nullptr;
    }
}

void TexturedCubeRenderer::releaseOutputDependentResources()
{
    if (m_ps) {
        m_r->releaseLater(m_ps);
        delete m_ps;
        m_ps = nullptr;
    }
}

QRhi::PassUpdates TexturedCubeRenderer::update()
{
    QRhi::PassUpdates u;

    if (!m_vbufReady) {
        m_vbufReady = true;
        u.staticBufferUploads.append({ m_vbuf, vertexData });
    }

    if (!m_image.isNull()) {
        u.textureUploads.append({ m_tex, m_image });
        m_image = QImage();
    }

    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.translate(m_translation);
    mvp.rotate(m_rotation, 0, 1, 0);
    u.dynamicBufferUpdates.append({ m_ubuf, 0, 64, mvp.constData() });

    return u;
}

void TexturedCubeRenderer::queueDraw(QRhiCommandBuffer *cb, const QSize &outputSizeInPixels)
{
    m_r->setViewport(cb, QRhiViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->setScissor(cb, QRhiScissor(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));

    m_r->setGraphicsPipeline(cb, m_ps);
    m_r->setVertexInput(cb, 0, { { m_vbuf, 0 }, { m_vbuf, 36 * 3 * sizeof(float) } });
    m_r->draw(cb, 36);
}
