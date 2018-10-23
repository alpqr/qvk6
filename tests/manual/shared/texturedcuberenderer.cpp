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

#include "cube.h"

const bool MIPMAP = true;

static QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

void TexturedCubeRenderer::initResources()
{
    m_vbuf = m_r->createBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(cube));
    m_vbuf->build();
    m_vbufReady = false;

    m_ubuf = m_r->createBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 4);
    m_ubuf->build();

    m_image = QImage(QLatin1String(":/qt256.png")).convertToFormat(QImage::Format_RGBA8888);
    QRhiTexture::Flags texFlags = 0;
    if (MIPMAP)
        texFlags |= QRhiTexture::MipMapped;
    m_tex = m_r->createTexture(QRhiTexture::RGBA8, QSize(m_image.width(), m_image.height()), texFlags);
    m_tex->build();

    m_sampler = m_r->createSampler(QRhiSampler::Linear, QRhiSampler::Linear, MIPMAP ? QRhiSampler::Linear : QRhiSampler::None,
                                   QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->build();

    m_srb = m_r->createShaderResourceBindings();
    const auto ubufVisibility = QRhiShaderResourceBindings::Binding::VertexStage | QRhiShaderResourceBindings::Binding::FragmentStage;
    m_srb->bindings = {
        QRhiShaderResourceBindings::Binding::uniformBuffer(0, ubufVisibility, m_ubuf),
        QRhiShaderResourceBindings::Binding::sampledTexture(1, QRhiShaderResourceBindings::Binding::FragmentStage, m_tex, m_sampler)
    };
    m_srb->build();
}

void TexturedCubeRenderer::initOutputDependentResources(const QRhiRenderPass *rp, const QSize &pixelSize)
{
    m_ps = m_r->createGraphicsPipeline();

    m_ps->depthTest = true;
    m_ps->depthWrite = true;
    m_ps->depthOp = QRhiGraphicsPipeline::Less;

    m_ps->cullMode = QRhiGraphicsPipeline::Back;
    m_ps->frontFace = QRhiGraphicsPipeline::CCW;

    m_ps->sampleCount = m_sampleCount;

    QBakedShader vs = getShader(QLatin1String(":/texture.vert.qsb"));
    Q_ASSERT(vs.isValid());
    QBakedShader fs = getShader(QLatin1String(":/texture.frag.qsb"));
    Q_ASSERT(fs.isValid());
    m_ps->shaderStages = {
        { QRhiGraphicsShaderStage::Vertex, vs },
        { QRhiGraphicsShaderStage::Fragment, fs }
    };

    QRhiVertexInputLayout inputLayout;
    inputLayout.bindings = {
        { 3 * sizeof(float) },
        { 2 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float3, 0 },
        { 1, 1, QRhiVertexInputLayout::Attribute::Float2, 0 }
    };

    m_ps->vertexInputLayout = inputLayout;
    m_ps->shaderResourceBindings = m_srb;
    m_ps->renderPass = rp;

    m_ps->build();

    m_proj = m_r->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, pixelSize.width() / (float) pixelSize.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);
}

void TexturedCubeRenderer::releaseResources()
{
    if (m_srb) {
        m_srb->releaseAndDestroy();
        m_srb = nullptr;
    }

    if (m_sampler) {
        m_sampler->releaseAndDestroy();
        m_sampler = nullptr;
    }

    if (m_tex) {
        m_tex->releaseAndDestroy();
        m_tex = nullptr;
    }

    if (m_ubuf) {
        m_ubuf->releaseAndDestroy();
        m_ubuf = nullptr;
    }

    if (m_vbuf) {
        m_vbuf->releaseAndDestroy();
        m_vbuf = nullptr;
    }
}

void TexturedCubeRenderer::releaseOutputDependentResources()
{
    if (m_ps) {
        m_ps->releaseAndDestroy();
        m_ps = nullptr;
    }
}

void TexturedCubeRenderer::queueResourceUpdates(QRhiResourceUpdateBatch *resourceUpdates)
{
    if (!m_vbufReady) {
        m_vbufReady = true;
        resourceUpdates->uploadStaticBuffer(m_vbuf, cube);
        qint32 flip = 0;
        resourceUpdates->updateDynamicBuffer(m_ubuf, 64, 4, &flip);
    }

    if (!m_image.isNull()) {
        if (MIPMAP) {
            QRhiResourceUpdateBatch::TextureUploadDescription desc;
            desc.layers.append(QRhiResourceUpdateBatch::TextureUploadDescription::Layer());
            // the ghetto mipmap generator...
            for (int i = 0, ie = m_r->mipLevelsForSize(m_image.size()); i != ie; ++i) {
                QImage image = m_image.scaled(m_r->sizeForMipLevel(i, m_image.size()));
                desc.layers[0].mipImages.append({ image });
            }
            resourceUpdates->uploadTexture(m_tex, desc);
        } else {
            resourceUpdates->uploadTexture(m_tex, m_image);
        }
        m_image = QImage();
    }

    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.translate(m_translation);
    mvp.scale(0.5f);
    mvp.rotate(m_rotation, 0, 1, 0);
    resourceUpdates->updateDynamicBuffer(m_ubuf, 0, 64, mvp.constData());
}

void TexturedCubeRenderer::queueDraw(QRhiCommandBuffer *cb, const QSize &outputSizeInPixels)
{
    m_r->setGraphicsPipeline(cb, m_ps);
    m_r->setViewport(cb, QRhiViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->setVertexInput(cb, 0, { { m_vbuf, 0 }, { m_vbuf, 36 * 3 * sizeof(float) } });
    m_r->draw(cb, 36);
}
