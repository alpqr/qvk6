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
}

QRhiProfiler::~QRhiProfiler()
{
    delete d;
}

void QRhiProfiler::setOutputDevice(QIODevice *device)
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

void QRhiProfilerPrivate::newBuffer(QRhiBuffer *buf, quint32 realSize, int backingGpuBufCount, int backingCpuBufCount)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::NewBuffer);
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
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::ReleaseBuffer);
    WRITE_PAIR(QLatin1String("buffer"), quint64(quintptr(buf)));
    writer->endMap();
}

void QRhiProfilerPrivate::newBufferStagingArea(QRhiBuffer *buf, int slot, quint32 size)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::NewBufferStagingArea);
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
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::ReleaseBufferStagingArea);
    WRITE_PAIR(QLatin1String("buffer"), quint64(quintptr(buf)));
    WRITE_PAIR(QLatin1String("slot"), slot);
    writer->endMap();
}

void QRhiProfilerPrivate::newRenderBuffer(QRhiRenderBuffer *rb, bool transientBacking, bool winSysBacking)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::NewRenderBuffer);
    WRITE_PAIR(QLatin1String("renderbuffer"), quint64(quintptr(rb)));
    const QRhiRenderBuffer::Type type = rb->type();
    const QSize sz = rb->pixelSize();
    // just make up something, ds is likely D24S8 while color is RGBA8 or similar
    const QRhiTexture::Format assumedFormat = type == QRhiRenderBuffer::DepthStencil ? QRhiTexture::D32 : QRhiTexture::RGBA8;
    const quint32 byteSize = rhiD->approxByteSizeForTexture(assumedFormat, sz, 1, 1);
    WRITE_PAIR(QLatin1String("type"), type);
    WRITE_PAIR(QLatin1String("width"), sz.width());
    WRITE_PAIR(QLatin1String("height"), sz.height());
    WRITE_PAIR(QLatin1String("sample_count"), rb->sampleCount());
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
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::ReleaseRenderBuffer);
    WRITE_PAIR(QLatin1String("renderbuffer"), quint64(quintptr(rb)));
    writer->endMap();
}

void QRhiProfilerPrivate::newTexture(QRhiTexture *tex, bool owns, int mipCount, int layerCount, int sampleCount)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::NewTexture);
    WRITE_PAIR(QLatin1String("texture"), quint64(quintptr(tex)));
    const QRhiTexture::Format format = tex->format();
    const QSize sz = tex->pixelSize();
    const quint32 byteSize = rhiD->approxByteSizeForTexture(format, sz, mipCount, layerCount);
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
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::ReleaseTexture);
    WRITE_PAIR(QLatin1String("texture"), quint64(quintptr(tex)));
    writer->endMap();
}

void QRhiProfilerPrivate::newTextureStagingArea(QRhiTexture *tex, int slot, quint32 size)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::NewTextureStagingArea);
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
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::ReleaseTextureStagingArea);
    WRITE_PAIR(QLatin1String("texture"), quint64(quintptr(tex)));
    WRITE_PAIR(QLatin1String("slot"), slot);
    writer->endMap();
}

void QRhiProfilerPrivate::resizeSwapChain(QRhiSwapChain *sc, int bufferCount, int sampleCount)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::ResizeSwapChain);
    WRITE_PAIR(QLatin1String("swapchain"), quint64(quintptr(sc)));
    const QSize sz = sc->currentPixelSize();
    const quint32 byteSize = rhiD->approxByteSizeForTexture(QRhiTexture::BGRA8, sz, 1, 1) * bufferCount;
    WRITE_PAIR(QLatin1String("width"), sz.width());
    WRITE_PAIR(QLatin1String("height"), sz.height());
    WRITE_PAIR(QLatin1String("buffer_count"), bufferCount);
    WRITE_PAIR(QLatin1String("effective_sample_count"), sampleCount);
    WRITE_PAIR(QLatin1String("approx_total_byte_size"), byteSize);
    writer->endMap();
}

void QRhiProfilerPrivate::releaseSwapChain(QRhiSwapChain *sc)
{
    if (!ensureStream())
        return;

    writer->startMap();
    WRITE_PAIR(QLatin1String("op"), QRhiProfiler::ReleaseSwapChain);
    WRITE_PAIR(QLatin1String("swapchain"), quint64(quintptr(sc)));
    writer->endMap();
}

QT_END_NAMESPACE
