/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt RHI module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qrhiprofiler_p.h"
#include "qrhi_p.h"
#include <QCborStreamWriter>

QT_BEGIN_NAMESPACE

QRhiProfiler::QRhiProfiler()
    : d(new QRhiProfilerPrivate)
{
    d->ts.start();
}

QRhiProfiler::~QRhiProfiler()
{
    delete d;
}

void QRhiProfiler::setDevice(QIODevice *device)
{
    d->outputDevice = device;
}

void QRhiProfiler::flush()
{
    d->flushStream();
}

void QRhiProfiler::addVMemAllocatorStats()
{
    if (d->rhiD)
        d->rhiD->sendVMemStatsToProfiler();
}

QRhiProfilerPrivate::~QRhiProfilerPrivate()
{
    flushStream();
    delete writer;
}

bool QRhiProfilerPrivate::ensureStream()
{
    if (!outputDevice)
        return false;

    if (!writer) {
        writer = new QCborStreamWriter(outputDevice);
        active = false;
    }

    if (!active) {
        writer->startArray();
        active = true;
    }

    return true;
}

void QRhiProfilerPrivate::flushStream()
{
    if (!active || !writer)
        return;

    // CborWriter is unbuffered so all we need to do here is to end the top-level array
    writer->endArray();

    active = false;
}

#define WRITE_PAIR(a, b) writer->append(a); writer->append(b)
#define WRITE_OP(op) WRITE_PAIR(QLatin1String("op"), QRhiProfiler::op)
#define WRITE_TIMESTAMP WRITE_PAIR(QLatin1String("timestamp"), ts.elapsed())

void QRhiProfilerPrivate::newBuffer(QRhiBuffer *buf, quint32 realSize, int backingGpuBufCount, int backingCpuBufCount)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(NewBuffer);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("buffer"), quint64(quintptr(buf)));
    WRITE_PAIR(QLatin1String("type"), buf->type());
    WRITE_PAIR(QLatin1String("usage"), buf->usage());
    WRITE_PAIR(QLatin1String("logical_size"), buf->size());
    WRITE_PAIR(QLatin1String("effective_size"), realSize);
    WRITE_PAIR(QLatin1String("backing_gpu_buf_count"), backingGpuBufCount);
    WRITE_PAIR(QLatin1String("backing_cpu_buf_count"), backingCpuBufCount);
    writer->endMap();
}

void QRhiProfilerPrivate::releaseBuffer(QRhiBuffer *buf)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(ReleaseBuffer);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("buffer"), quint64(quintptr(buf)));
    writer->endMap();
}

void QRhiProfilerPrivate::newBufferStagingArea(QRhiBuffer *buf, int slot, quint32 size)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(NewBufferStagingArea);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("buffer"), quint64(quintptr(buf)));
    WRITE_PAIR(QLatin1String("slot"), slot);
    WRITE_PAIR(QLatin1String("size"), size);
    writer->endMap();
}

void QRhiProfilerPrivate::releaseBufferStagingArea(QRhiBuffer *buf, int slot)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(ReleaseBufferStagingArea);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("buffer"), quint64(quintptr(buf)));
    WRITE_PAIR(QLatin1String("slot"), slot);
    writer->endMap();
}

void QRhiProfilerPrivate::newRenderBuffer(QRhiRenderBuffer *rb, bool transientBacking, bool winSysBacking, int sampleCount)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(NewRenderBuffer);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("renderbuffer"), quint64(quintptr(rb)));

    const QRhiRenderBuffer::Type type = rb->type();
    const QSize sz = rb->pixelSize();
    // just make up something, ds is likely D24S8 while color is RGBA8 or similar
    const QRhiTexture::Format assumedFormat = type == QRhiRenderBuffer::DepthStencil ? QRhiTexture::D32 : QRhiTexture::RGBA8;
    quint32 byteSize = rhiD->approxByteSizeForTexture(assumedFormat, sz, 1, 1);
    if (sampleCount > 1)
        byteSize *= sampleCount;

    WRITE_PAIR(QLatin1String("type"), type);
    WRITE_PAIR(QLatin1String("width"), sz.width());
    WRITE_PAIR(QLatin1String("height"), sz.height());
    WRITE_PAIR(QLatin1String("effective_sample_count"), sampleCount);
    WRITE_PAIR(QLatin1String("transient_backing"), transientBacking);
    WRITE_PAIR(QLatin1String("winsys_backing"), winSysBacking);
    WRITE_PAIR(QLatin1String("approx_byte_size"), byteSize);
    writer->endMap();
}

void QRhiProfilerPrivate::releaseRenderBuffer(QRhiRenderBuffer *rb)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(ReleaseRenderBuffer);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("renderbuffer"), quint64(quintptr(rb)));
    writer->endMap();
}

void QRhiProfilerPrivate::newTexture(QRhiTexture *tex, bool owns, int mipCount, int layerCount, int sampleCount)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(NewTexture);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("texture"), quint64(quintptr(tex)));

    const QRhiTexture::Format format = tex->format();
    const QSize sz = tex->pixelSize();
    quint32 byteSize = rhiD->approxByteSizeForTexture(format, sz, mipCount, layerCount);
    if (sampleCount > 1)
        byteSize *= sampleCount;

    WRITE_PAIR(QLatin1String("width"), sz.width());
    WRITE_PAIR(QLatin1String("height"), sz.height());
    WRITE_PAIR(QLatin1String("format"), format);
    WRITE_PAIR(QLatin1String("owns_native_resource"), owns);
    WRITE_PAIR(QLatin1String("mip_count"), mipCount);
    WRITE_PAIR(QLatin1String("layer_count"), layerCount);
    WRITE_PAIR(QLatin1String("effective_sample_count"), sampleCount);
    WRITE_PAIR(QLatin1String("approx_byte_size"), byteSize);
    writer->endMap();
}

void QRhiProfilerPrivate::releaseTexture(QRhiTexture *tex)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(ReleaseTexture);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("texture"), quint64(quintptr(tex)));
    writer->endMap();
}

void QRhiProfilerPrivate::newTextureStagingArea(QRhiTexture *tex, int slot, quint32 size)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(NewTextureStagingArea);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("texture"), quint64(quintptr(tex)));
    WRITE_PAIR(QLatin1String("slot"), slot);
    WRITE_PAIR(QLatin1String("size"), size);
    writer->endMap();
}

void QRhiProfilerPrivate::releaseTextureStagingArea(QRhiTexture *tex, int slot)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(ReleaseTextureStagingArea);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("texture"), quint64(quintptr(tex)));
    WRITE_PAIR(QLatin1String("slot"), slot);
    writer->endMap();
}

void QRhiProfilerPrivate::resizeSwapChain(QRhiSwapChain *sc, int bufferCount, int msaaBufferCount, int sampleCount)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(ResizeSwapChain);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("swapchain"), quint64(quintptr(sc)));

    const QSize sz = sc->currentPixelSize();
    quint32 byteSize = rhiD->approxByteSizeForTexture(QRhiTexture::BGRA8, sz, 1, 1);
    byteSize = byteSize * bufferCount + byteSize * msaaBufferCount * sampleCount;

    WRITE_PAIR(QLatin1String("width"), sz.width());
    WRITE_PAIR(QLatin1String("height"), sz.height());
    WRITE_PAIR(QLatin1String("buffer_count"), bufferCount);
    WRITE_PAIR(QLatin1String("msaa_buffer_count"), msaaBufferCount);
    WRITE_PAIR(QLatin1String("effective_sample_count"), sampleCount);
    WRITE_PAIR(QLatin1String("approx_total_byte_size"), byteSize);
    writer->endMap();
}

void QRhiProfilerPrivate::releaseSwapChain(QRhiSwapChain *sc)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(ReleaseSwapChain);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("swapchain"), quint64(quintptr(sc)));
    writer->endMap();
}

void QRhiProfilerPrivate::endSwapChainFrame(QRhiSwapChain *sc, int frameCount)
{
    if (!swapchains.contains(sc)) {
        swapchains[sc].t.start();
        return;
    }

    Sc &scd(swapchains[sc]);
    scd.frameDelta[scd.n++] = scd.t.restart();
    if (scd.n == Sc::FRAME_SAMPLE_SIZE) {
        scd.n = 0;
        float totalDelta = 0;
        for (int i = 0; i < Sc::FRAME_SAMPLE_SIZE; ++i)
            totalDelta += scd.frameDelta[i];
        const float avgDelta = totalDelta / Sc::FRAME_SAMPLE_SIZE;
        if (ensureStream()) {
            writer->startMap();
            WRITE_OP(FrameTime);
            WRITE_TIMESTAMP;
            WRITE_PAIR(QLatin1String("swapchain"), quint64(quintptr(sc)));
            WRITE_PAIR(QLatin1String("frames_since_resize"), frameCount);
            WRITE_PAIR(QLatin1String("avg_ms_between_frames"), avgDelta);
            writer->endMap();
        }
    }
}

void QRhiProfilerPrivate::vmemStat(int realAllocCount, int subAllocCount, quint32 totalSize, quint32 unusedSize)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_OP(VMemAllocStats);
    WRITE_TIMESTAMP;
    WRITE_PAIR(QLatin1String("realAllocCount"), realAllocCount);
    WRITE_PAIR(QLatin1String("subAllocCount"), subAllocCount);
    WRITE_PAIR(QLatin1String("totalSize"), totalSize);
    WRITE_PAIR(QLatin1String("unusedSize"), unusedSize);
    writer->endMap();
}

QT_END_NAMESPACE
