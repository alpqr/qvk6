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

#include "../shared/examplefw.h"
#include "../shared/cube.h"

struct {
    QRhiBuffer *vbuf = nullptr;
    bool vbufReady = false;
    QRhiBuffer *ubuf = nullptr;
    QRhiTexture *tex = nullptr;
    QRhiSampler *sampler = nullptr;
    QRhiShaderResourceBindings *srb = nullptr;
    QRhiGraphicsPipeline *ps = nullptr;

    float rotation = 0;

    QByteArrayList compressedData;
} d;

static const quint32 DDS_MAGIC = 0x20534444; // 'DDS '
static const quint32 DDS_FOURCC = 4;

#define FOURCC(c0, c1, c2, c3) ((c0) | ((c1) << 8) | ((c2) << 16) | ((c3 << 24)))

struct DDS_PIXELFORMAT {
    quint32 size;
    quint32 flags;
    quint32 fourCC;
    quint32 rgbBitCount;
    quint32 rBitMask;
    quint32 gBitMask;
    quint32 bBitMask;
    quint32 aBitMask;
};

struct DDS_HEADER {
    quint32 size;
    quint32 flags;
    quint32 height;
    quint32 width;
    quint32 pitch;
    quint32 depth;
    quint32 mipMapCount;
    quint32 reserved1[11];
    DDS_PIXELFORMAT pixelFormat;
    quint32 caps;
    quint32 caps2;
    quint32 caps3;
    quint32 caps4;
    quint32 reserved2;
};

static quint32 bc1size(const QSize &size)
{
    static const quint32 blockSize = 8; // 8 bytes for BC1
    const quint32 bytesPerLine = qMax<quint32>(1, (size.width() + 3) / 4) * blockSize;
    const quint32 ySize = qMax<quint32>(1, (size.height() + 3) / 4);
    return bytesPerLine * ySize;
}

static QByteArrayList loadBC1(const QString &filename, QSize *size)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("Failed to open %s", qPrintable(filename));
        return QByteArrayList();
    }

    quint32 magic = 0;
    f.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    if (magic != DDS_MAGIC) {
        qWarning("%s is not a DDS file", qPrintable(filename));
        return QByteArrayList();
    }
    DDS_HEADER header;
    f.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (header.size != sizeof(DDS_HEADER)) {
        qWarning("Invalid DDS header size");
        return QByteArrayList();
    }
    if (header.pixelFormat.size != sizeof(DDS_PIXELFORMAT)) {
        qWarning("Invalid DDS pixel format size");
        return QByteArrayList();
    }
    if (!(header.pixelFormat.flags & DDS_FOURCC)) {
        qWarning("Invalid DDS pixel format");
        return QByteArrayList();
    }
    if (header.pixelFormat.fourCC != FOURCC('D', 'X', 'T', '1')) {
        qWarning("Only DXT1 (BC1) is supported");
        return QByteArrayList();
    }

    QByteArrayList data;
    QSize sz(header.width, header.height);
    for (quint32 level = 0; level < header.mipMapCount; ++level) {
        data.append(f.read(bc1size(sz)));
        sz.setWidth(qMax(1, sz.width() / 2));
        sz.setHeight(qMax(1, sz.height() / 2));
    }

    if (size)
        *size = QSize(header.width, header.height);

    return data;
}

void Window::customInit()
{
    if (!m_r->isTextureFormatSupported(QRhiTexture::BC1))
        qFatal("This backend does not support BC1");

    d.vbuf = m_r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(cube));
    d.vbuf->build();
    d.vbufReady = false;

    d.ubuf = m_r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    d.ubuf->build();

    QSize imageSize;
    d.compressedData = loadBC1(QLatin1String(":/qt256_bc1_9mips.dds"), &imageSize);
    qDebug() << d.compressedData.count() << imageSize << m_r->mipLevelsForSize(imageSize);

    d.tex = m_r->newTexture(QRhiTexture::BC1, imageSize, QRhiTexture::MipMapped);
    d.tex->build();

    d.sampler = m_r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    d.sampler->build();

    d.srb = m_r->newShaderResourceBindings();
    d.srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d.ubuf),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, d.tex, d.sampler)
    });
    d.srb->build();

    d.ps = m_r->newGraphicsPipeline();

    d.ps->setDepthTest(true);
    d.ps->setDepthWrite(true);
    d.ps->setDepthOp(QRhiGraphicsPipeline::Less);

    d.ps->setCullMode(QRhiGraphicsPipeline::Back);
    d.ps->setFrontFace(QRhiGraphicsPipeline::CCW);

    const QBakedShader vs = getShader(QLatin1String(":/texture.vert.qsb"));
    if (!vs.isValid())
        qFatal("Failed to load shader pack (vertex)");
    const QBakedShader fs = getShader(QLatin1String(":/texture.frag.qsb"));
    if (!fs.isValid())
        qFatal("Failed to load shader pack (fragment)");

    d.ps->setShaderStages({
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

    d.ps->setVertexInputLayout(inputLayout);
    d.ps->setShaderResourceBindings(d.srb);
    d.ps->setRenderPassDescriptor(m_rp);

    d.ps->build();
}

void Window::customRelease()
{
    if (d.ps) {
        d.ps->releaseAndDestroy();
        d.ps = nullptr;
    }

    if (d.srb) {
        d.srb->releaseAndDestroy();
        d.srb = nullptr;
    }

    if (d.ubuf) {
        d.ubuf->releaseAndDestroy();
        d.ubuf = nullptr;
    }

    if (d.vbuf) {
        d.vbuf->releaseAndDestroy();
        d.vbuf = nullptr;
    }

    if (d.sampler) {
        d.sampler->releaseAndDestroy();
        d.sampler = nullptr;
    }

    if (d.tex) {
        d.tex->releaseAndDestroy();
        d.tex = nullptr;
    }
}

void Window::customRender()
{
    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();
    if (!d.vbufReady) {
        d.vbufReady = true;
        u->uploadStaticBuffer(d.vbuf, cube);
        qint32 flip = 0;
        u->updateDynamicBuffer(d.ubuf, 64, 4, &flip);
    }
    if (!d.compressedData.isEmpty()) {
        QRhiTextureUploadDescription desc;
        QRhiTextureUploadDescription::Layer layer;
        for (int i = 0; i < d.compressedData.count(); ++i) {
            QRhiTextureUploadDescription::Layer::MipLevel image(d.compressedData[i]);
            layer.mipImages.append(image);
        }
        desc.layers.append(layer);
        u->uploadTexture(d.tex, desc);
        d.compressedData.clear();
    }
    d.rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.scale(0.5f);
    mvp.rotate(d.rotation, 0, 1, 0);
    u->updateDynamicBuffer(d.ubuf, 0, 64, mvp.constData());

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    const QSize outputSizeInPixels = m_sc->effectivePixelSize();

    cb->beginPass(m_sc->currentFrameRenderTarget(), { 0.4f, 0.7f, 0.0f, 1.0f }, { 1.0f, 0 }, u);

    cb->setGraphicsPipeline(d.ps);
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setVertexInput(0, { { d.vbuf, 0 }, { d.vbuf, 36 * 3 * sizeof(float) } });
    cb->draw(36);

    cb->endPass();
}
