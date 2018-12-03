/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QGuiApplication>
#include <QImage>
#include <QFileInfo>
#include <QFile>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QBakedShader>
#include <QRhiGles2InitParams>

static float vertexData[] = { // Y up (note m_proj), CCW
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f,
};

static QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    QOpenGLContext context;
    if (!context.create())
        qFatal("Failed to get OpenGL context");

    QOffscreenSurface offscreenSurface;
    offscreenSurface.setFormat(context.format());
    offscreenSurface.create();

    QRhiGles2InitParams params;
    params.context = &context;
    params.fallbackSurface = &offscreenSurface;

    QRhi *r = QRhi::create(QRhi::OpenGLES2, &params);

    if (!r) {
        qWarning("Failed to initialize RHI");
        return 1;
    }

    QRhiTexture *tex = r->newTexture(QRhiTexture::RGBA8, QSize(1280, 720), 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    tex->build();
    QRhiTextureRenderTarget *rt = r->newTextureRenderTarget({ tex });
    QRhiRenderPassDescriptor *rp = rt->newCompatibleRenderPassDescriptor();
    rt->setRenderPassDescriptor(rp);
    rt->build();

    QMatrix4x4 proj = r->clipSpaceCorrMatrix();
    proj.perspective(45.0f, 1280 / 720.f, 0.01f, 1000.0f);
    proj.translate(0, 0, -4);

    QRhiBuffer *vbuf = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
    vbuf->build();

    QRhiBuffer *ubuf = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    ubuf->build();

    QRhiShaderResourceBindings *srb = r->newShaderResourceBindings();
    srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, ubuf)
    });
    srb->build();

    QRhiGraphicsPipeline *ps = r->newGraphicsPipeline();

    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
    premulAlphaBlend.enable = true;
    ps->setTargetBlends({ premulAlphaBlend });

    const QBakedShader vs = getShader(QLatin1String(":/color.vert.qsb"));
    if (!vs.isValid())
        qFatal("Failed to load shader pack (vertex)");
    const QBakedShader fs = getShader(QLatin1String(":/color.frag.qsb"));
    if (!fs.isValid())
        qFatal("Failed to load shader pack (fragment)");

    ps->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, vs },
        { QRhiGraphicsShaderStage::Fragment, fs }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.bindings = {
        { 5 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float2, 0 },
        { 0, 1, QRhiVertexInputLayout::Attribute::Float3, 2 * sizeof(float) }
    };

    ps->setVertexInputLayout(inputLayout);
    ps->setShaderResourceBindings(srb);
    ps->setRenderPassDescriptor(rp);
    ps->build();

    for (int frame = 0; frame < 20; ++frame) {
        QRhiCommandBuffer *cb;
        if (r->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
            break;

        qDebug("Generating offscreen frame %d", frame);
        QRhiResourceUpdateBatch *u = r->nextResourceUpdateBatch();
        if (frame == 0)
            u->uploadStaticBuffer(vbuf, vertexData);

        static float rotation = 0.0f;
        QMatrix4x4 mvp = proj;
        mvp.rotate(rotation, 0, 1, 0);
        u->updateDynamicBuffer(ubuf, 0, 64, mvp.constData());
        rotation += 5.0f;
        static float opacity = 1.0f;
        static int opacityDir= 1;
        u->updateDynamicBuffer(ubuf, 64, 4, &opacity);
        opacity += opacityDir * 0.005f;
        if (opacity < 0.0f || opacity > 1.0f) {
            opacityDir *= -1;
            opacity = qBound(0.0f, opacity, 1.0f);
        }

        cb->beginPass(rt, { 0, 1, 0, 1 }, { 1, 0 }, u);
        cb->setGraphicsPipeline(ps);
        cb->setViewport({ 0, 0, 1280, 720 });
        cb->setVertexInput(0, { { vbuf, 0 } });
        cb->draw(3);

        u = r->nextResourceUpdateBatch();
        QRhiReadbackDescription rb(tex);
        QRhiReadbackResult rbResult;
        rbResult.completed = [frame] { qDebug("  - readback %d completed", frame); };
        u->readBackTexture(rb, &rbResult);

        cb->endPass(u);

        qDebug("Submit and wait");
        r->endOffscreenFrame();

        // No finish() or waiting for the completed callback is needed here
        // since the endOffscreenFrame() implies a wait for completion.
        if (!rbResult.data.isEmpty()) {
            const uchar *p = reinterpret_cast<const uchar *>(rbResult.data.constData());
            QImage image(p, rbResult.pixelSize.width(), rbResult.pixelSize.height(), QImage::Format_RGBA8888);
            QString fn = QString::asprintf("frame%d.png", frame);
            fn = QFileInfo(fn).absoluteFilePath();
            qDebug("Saving into %s", qPrintable(fn));
            image.mirrored().save(fn); // gl has isYUpInFramebuffer == true so mirror
        } else {
            qWarning("Readback failed!");
        }
    }

    ps->releaseAndDestroy();
    srb->releaseAndDestroy();
    ubuf->releaseAndDestroy();
    vbuf->releaseAndDestroy();

    rt->releaseAndDestroy();
    rp->releaseAndDestroy();
    tex->releaseAndDestroy();

    delete r;

    return 0;
}
