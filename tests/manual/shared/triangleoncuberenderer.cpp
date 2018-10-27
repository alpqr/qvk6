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

// toggle to test the preserved content (no clear) path
const bool IMAGE_UNDER_OFFSCREEN_RENDERING = false;
const bool UPLOAD_UNDERLAY_ON_EVERY_FRAME = false;

const bool DEPTH_TEXTURE = false; // offscreen pass uses a depth texture (verify with renderdoc etc.)
const bool MRT = false; // two textures, the second is just cleared as the shader does not write anything (vk valid.layer may warn but for testing that's ok)

#include "cube.h"

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
    m_vbuf = m_r->createBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(cube));
    m_vbuf->build();
    m_vbufReady = false;

    m_ubuf = m_r->createBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 4);
    m_ubuf->build();

    if (IMAGE_UNDER_OFFSCREEN_RENDERING) {
        m_image = QImage(QLatin1String(":/qt256.png")).scaled(OFFSCREEN_SIZE).convertToFormat(QImage::Format_RGBA8888);
        if (m_r->isYUpInFramebuffer())
            m_image = m_image.mirrored(); // just cause we'll flip texcoord Y when y up so accomodate our static background image as well
    }

    m_tex = m_r->createTexture(QRhiTexture::RGBA8, OFFSCREEN_SIZE, QRhiTexture::RenderTarget);
    m_tex->build();

    if (MRT) {
        m_tex2 = m_r->createTexture(QRhiTexture::RGBA8, OFFSCREEN_SIZE, QRhiTexture::RenderTarget);
        m_tex2->build();
    }

    m_sampler = m_r->createSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->build();

    m_srb = m_r->createShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_ubuf),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_tex, m_sampler)
    });
    m_srb->build();

    if (DEPTH_TEXTURE) {
        m_offscreenTriangle.setDepthWrite(true);
        m_depthTex = m_r->createTexture(QRhiTexture::D32, OFFSCREEN_SIZE, QRhiTexture::RenderTarget);
        m_depthTex->build();
    }

    QRhiTextureRenderTarget::Flags rtFlags = 0;
    if (IMAGE_UNDER_OFFSCREEN_RENDERING)
        rtFlags |= QRhiTextureRenderTarget::PreserveColorContents;

    if (DEPTH_TEXTURE) {
        m_rt = m_r->createTextureRenderTarget({ nullptr, m_depthTex }, rtFlags);
    } else {
        QRhiTextureRenderTargetDescription desc { m_tex };
        if (MRT) {
            m_offscreenTriangle.setColorAttCount(2);
            desc.colorAttachments.append(m_tex2);
        }
        m_rt = m_r->createTextureRenderTarget(desc, rtFlags);
    }

    m_rt->build();

    m_offscreenTriangle.setRhi(m_r);
    m_offscreenTriangle.initResources();
    m_offscreenTriangle.setScale(2);
    // m_tex and the offscreen triangle are never multisample
}

void TriangleOnCubeRenderer::initOutputDependentResources(const QRhiRenderPass *rp, const QSize &pixelSize)
{
    m_ps = m_r->createGraphicsPipeline();

    m_ps->setDepthTest(true);
    m_ps->setDepthWrite(true);
    m_ps->setDepthOp(QRhiGraphicsPipeline::Less);

    m_ps->setCullMode(QRhiGraphicsPipeline::Back);
    m_ps->setFrontFace(QRhiGraphicsPipeline::CCW);

    m_ps->setSampleCount(m_sampleCount);

    QBakedShader vs = getShader(QLatin1String(":/texture.vert.qsb"));
    Q_ASSERT(vs.isValid());
    QBakedShader fs = getShader(QLatin1String(":/texture.frag.qsb"));
    Q_ASSERT(fs.isValid());
    m_ps->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, vs },
        { QRhiGraphicsShaderStage::Fragment, fs }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.bindings = {
        { 3 * sizeof(float) },
        { 2 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float3, 0 },
        { 1, 1, QRhiVertexInputLayout::Attribute::Float2, 0 }
    };

    m_ps->setVertexInputLayout(inputLayout);
    m_ps->setShaderResourceBindings(m_srb);
    m_ps->setRenderPass(rp);

    m_ps->build();

    m_proj = m_r->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, pixelSize.width() / (float) pixelSize.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);

    m_offscreenTriangle.initOutputDependentResources(m_rt->renderPass(), pixelSize);
}

void TriangleOnCubeRenderer::releaseResources()
{
    m_offscreenTriangle.releaseResources();

    if (m_srb) {
        m_srb->releaseAndDestroy();
        m_srb = nullptr;
    }

    if (m_rt) {
        m_rt->releaseAndDestroy();
        m_rt = nullptr;
    }

    if (m_sampler) {
        m_sampler->releaseAndDestroy();
        m_sampler = nullptr;
    }

    if (m_depthTex) {
        m_depthTex->releaseAndDestroy();
        m_depthTex = nullptr;
    }

    if (m_tex2) {
        m_tex2->releaseAndDestroy();
        m_tex2 = nullptr;
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

void TriangleOnCubeRenderer::releaseOutputDependentResources()
{
    m_offscreenTriangle.releaseOutputDependentResources();

    if (m_ps) {
        m_ps->releaseAndDestroy();
        m_ps = nullptr;
    }
}

void TriangleOnCubeRenderer::queueResourceUpdates(QRhiResourceUpdateBatch *resourceUpdates)
{
    if (!m_vbufReady) {
        m_vbufReady = true;
        resourceUpdates->uploadStaticBuffer(m_vbuf, cube);
        qint32 flip = m_r->isYUpInFramebuffer() ? 1 : 0;
        resourceUpdates->updateDynamicBuffer(m_ubuf, 64, 4, &flip);
    }

    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.translate(m_translation);
    mvp.scale(0.5f);
    mvp.rotate(m_rotation, 1, 0, 0);
    resourceUpdates->updateDynamicBuffer(m_ubuf, 0, 64, mvp.constData());
}

void TriangleOnCubeRenderer::queueOffscreenPass(QRhiCommandBuffer *cb)
{
    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();
    m_offscreenTriangle.queueResourceUpdates(u);

    if (IMAGE_UNDER_OFFSCREEN_RENDERING && !m_image.isNull()) {
        u->uploadTexture(m_tex, m_image);
        if (!UPLOAD_UNDERLAY_ON_EVERY_FRAME)
            m_image = QImage();
    }

    m_r->beginPass(m_rt, cb, { 0.0f, 0.4f, 0.7f, 1.0f }, { 1.0f, 0 }, u);
    m_offscreenTriangle.queueDraw(cb, OFFSCREEN_SIZE);
    m_r->endPass(cb);
}

void TriangleOnCubeRenderer::queueDraw(QRhiCommandBuffer *cb, const QSize &outputSizeInPixels)
{
    m_r->setGraphicsPipeline(cb, m_ps);
    m_r->setViewport(cb, QRhiViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->setVertexInput(cb, 0, { { m_vbuf, 0 }, { m_vbuf, 36 * 3 * sizeof(float) } });
    m_r->draw(cb, 36);
}
