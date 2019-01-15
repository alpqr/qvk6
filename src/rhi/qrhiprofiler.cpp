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

QT_BEGIN_NAMESPACE

/*!
    \class QRhiProfiler
    \inmodule QtRhi

    \brief Collects resource and timing information from an active QRhi.
 */

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

void QRhiProfiler::addVMemAllocatorStats()
{
    if (d->rhiD)
        d->rhiD->sendVMemStatsToProfiler();
}

int QRhiProfiler::frameTimingWriteInterval() const
{
    return d->frameTimingWriteInterval;
}

void QRhiProfiler::setFrameTimingWriteInterval(int frameCount)
{
    if (frameCount > 0)
        d->frameTimingWriteInterval = frameCount;
}

QRhiProfiler::CpuTime QRhiProfiler::frameToFrameTimes(QRhiSwapChain *sc) const
{
    auto it = d->swapchains.constFind(sc);
    if (it != d->swapchains.constEnd())
        return it->frameToFrameTime;

    return QRhiProfiler::CpuTime();
}

QRhiProfiler::CpuTime QRhiProfiler::frameBuildTimes(QRhiSwapChain *sc) const
{
    auto it = d->swapchains.constFind(sc);
    if (it != d->swapchains.constEnd())
        return it->beginToEndFrameTime;

    return QRhiProfiler::CpuTime();
}

QRhiProfiler::GpuTime QRhiProfiler::gpuFrameTimes(QRhiSwapChain *sc) const
{
    auto it = d->swapchains.constFind(sc);
    if (it != d->swapchains.constEnd())
        return it->gpuFrameTime;

    return QRhiProfiler::GpuTime();
}

void QRhiProfilerPrivate::startEntry(QRhiProfiler::StreamOp op, qint64 timestamp, QRhiResource *res)
{
    buf.clear();
    buf.append(QByteArray::number(op));
    buf.append(',');
    buf.append(QByteArray::number(timestamp));
    buf.append(',');
    buf.append(QByteArray::number(quint64(quintptr(res))));
    buf.append(',');
}

void QRhiProfilerPrivate::writeInt(const char *key, qint64 v)
{
    Q_ASSERT(key[0] != 'F');
    buf.append(key);
    buf.append(',');
    buf.append(QByteArray::number(v));
    buf.append(',');
}

void QRhiProfilerPrivate::writeFloat(const char *key, float f)
{
    Q_ASSERT(key[0] == 'F');
    buf.append(key);
    buf.append(',');
    buf.append(QByteArray::number(f));
    buf.append(',');
}

void QRhiProfilerPrivate::endEntry()
{
    buf.append('\n');
    outputDevice->write(buf);
}

void QRhiProfilerPrivate::newBuffer(QRhiBuffer *buf, quint32 realSize, int backingGpuBufCount, int backingCpuBufCount)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::NewBuffer, ts.elapsed(), buf);
    writeInt("type", buf->type());
    writeInt("usage", buf->usage());
    writeInt("logical_size", buf->size());
    writeInt("effective_size", realSize);
    writeInt("backing_gpu_buf_count", backingGpuBufCount);
    writeInt("backing_cpu_buf_count", backingCpuBufCount);
    endEntry();
}

void QRhiProfilerPrivate::releaseBuffer(QRhiBuffer *buf)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::ReleaseBuffer, ts.elapsed(), buf);
    endEntry();
}

void QRhiProfilerPrivate::newBufferStagingArea(QRhiBuffer *buf, int slot, quint32 size)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::NewBufferStagingArea, ts.elapsed(), buf);
    writeInt("slot", slot);
    writeInt("size", size);
    endEntry();
}

void QRhiProfilerPrivate::releaseBufferStagingArea(QRhiBuffer *buf, int slot)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::ReleaseBufferStagingArea, ts.elapsed(), buf);
    writeInt("slot", slot);
    endEntry();
}

void QRhiProfilerPrivate::newRenderBuffer(QRhiRenderBuffer *rb, bool transientBacking, bool winSysBacking, int sampleCount)
{
    if (!outputDevice)
        return;

    const QRhiRenderBuffer::Type type = rb->type();
    const QSize sz = rb->pixelSize();
    // just make up something, ds is likely D24S8 while color is RGBA8 or similar
    const QRhiTexture::Format assumedFormat = type == QRhiRenderBuffer::DepthStencil ? QRhiTexture::D32 : QRhiTexture::RGBA8;
    quint32 byteSize = rhiD->approxByteSizeForTexture(assumedFormat, sz, 1, 1);
    if (sampleCount > 1)
        byteSize *= sampleCount;

    startEntry(QRhiProfiler::NewRenderBuffer, ts.elapsed(), rb);
    writeInt("type", type);
    writeInt("width", sz.width());
    writeInt("height", sz.height());
    writeInt("effective_sample_count", sampleCount);
    writeInt("transient_backing", transientBacking);
    writeInt("winsys_backing", winSysBacking);
    writeInt("approx_byte_size", byteSize);
    endEntry();
}

void QRhiProfilerPrivate::releaseRenderBuffer(QRhiRenderBuffer *rb)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::ReleaseRenderBuffer, ts.elapsed(), rb);
    endEntry();
}

void QRhiProfilerPrivate::newTexture(QRhiTexture *tex, bool owns, int mipCount, int layerCount, int sampleCount)
{
    if (!outputDevice)
        return;

    const QRhiTexture::Format format = tex->format();
    const QSize sz = tex->pixelSize();
    quint32 byteSize = rhiD->approxByteSizeForTexture(format, sz, mipCount, layerCount);
    if (sampleCount > 1)
        byteSize *= sampleCount;

    startEntry(QRhiProfiler::NewTexture, ts.elapsed(), tex);
    writeInt("width", sz.width());
    writeInt("height", sz.height());
    writeInt("format", format);
    writeInt("owns_native_resource", owns);
    writeInt("mip_count", mipCount);
    writeInt("layer_count", layerCount);
    writeInt("effective_sample_count", sampleCount);
    writeInt("approx_byte_size", byteSize);
    endEntry();
}

void QRhiProfilerPrivate::releaseTexture(QRhiTexture *tex)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::ReleaseTexture, ts.elapsed(), tex);
    endEntry();
}

void QRhiProfilerPrivate::newTextureStagingArea(QRhiTexture *tex, int slot, quint32 size)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::NewTextureStagingArea, ts.elapsed(), tex);
    writeInt("slot", slot);
    writeInt("size", size);
    endEntry();
}

void QRhiProfilerPrivate::releaseTextureStagingArea(QRhiTexture *tex, int slot)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::ReleaseTextureStagingArea, ts.elapsed(), tex);
    writeInt("slot", slot);
    endEntry();
}

void QRhiProfilerPrivate::resizeSwapChain(QRhiSwapChain *sc, int bufferCount, int msaaBufferCount, int sampleCount)
{
    if (!outputDevice)
        return;

    const QSize sz = sc->currentPixelSize();
    quint32 byteSize = rhiD->approxByteSizeForTexture(QRhiTexture::BGRA8, sz, 1, 1);
    byteSize = byteSize * bufferCount + byteSize * msaaBufferCount * sampleCount;

    startEntry(QRhiProfiler::ResizeSwapChain, ts.elapsed(), sc);
    writeInt("width", sz.width());
    writeInt("height", sz.height());
    writeInt("buffer_count", bufferCount);
    writeInt("msaa_buffer_count", msaaBufferCount);
    writeInt("effective_sample_count", sampleCount);
    writeInt("approx_total_byte_size", byteSize);
    endEntry();
}

void QRhiProfilerPrivate::releaseSwapChain(QRhiSwapChain *sc)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::ReleaseSwapChain, ts.elapsed(), sc);
    endEntry();
}

template<typename T>
void calcTiming(QVector<T> *vec, T *minDelta, T *maxDelta, float *avgDelta)
{
    if (vec->isEmpty())
        return;

    *minDelta = *maxDelta = 0;
    float totalDelta = 0;
    for (T delta : qAsConst(*vec)) {
        totalDelta += float(delta);
        if (*minDelta == 0 || delta < *minDelta)
            *minDelta = delta;
        if (*maxDelta == 0 || delta > *maxDelta)
            *maxDelta = delta;
    }
    *avgDelta = totalDelta / vec->count();

    vec->clear();
}

void QRhiProfilerPrivate::beginSwapChainFrame(QRhiSwapChain *sc)
{
    Sc &scd(swapchains[sc]);
    scd.beginToEndTimer.start();
}

void QRhiProfilerPrivate::endSwapChainFrame(QRhiSwapChain *sc, int frameCount)
{
    Sc &scd(swapchains[sc]);
    if (!scd.frameToFrameRunning) {
        scd.frameToFrameTimer.start();
        scd.frameToFrameRunning = true;
        return;
    }

    scd.frameToFrameSamples.append(scd.frameToFrameTimer.restart());
    if (scd.frameToFrameSamples.count() >= frameTimingWriteInterval) {
        calcTiming(&scd.frameToFrameSamples,
                   &scd.frameToFrameTime.minTime, &scd.frameToFrameTime.maxTime, &scd.frameToFrameTime.avgTime);
        if (outputDevice) {
            startEntry(QRhiProfiler::FrameToFrameTime, ts.elapsed(), sc);
            writeInt("frames_since_resize", frameCount);
            writeInt("min_ms_frame_delta", scd.frameToFrameTime.minTime);
            writeInt("max_ms_frame_delta", scd.frameToFrameTime.maxTime);
            writeFloat("Favg_ms_frame_delta", scd.frameToFrameTime.avgTime);
            endEntry();
        }
    }

    scd.beginToEndSamples.append(scd.beginToEndTimer.elapsed());
    if (scd.beginToEndSamples.count() >= frameTimingWriteInterval) {
        calcTiming(&scd.beginToEndSamples,
                   &scd.beginToEndFrameTime.minTime, &scd.beginToEndFrameTime.maxTime, &scd.beginToEndFrameTime.avgTime);
        if (outputDevice) {
            startEntry(QRhiProfiler::FrameBuildTime, ts.elapsed(), sc);
            writeInt("frames_since_resize", frameCount);
            writeInt("min_ms_frame_build", scd.beginToEndFrameTime.minTime);
            writeInt("max_ms_frame_build", scd.beginToEndFrameTime.maxTime);
            writeFloat("Favg_ms_frame_build", scd.beginToEndFrameTime.avgTime);
            endEntry();
        }
    }
}

void QRhiProfilerPrivate::swapChainFrameGpuTime(QRhiSwapChain *sc, float gpuTime)
{
    Sc &scd(swapchains[sc]);
    scd.gpuFrameSamples.append(gpuTime);
    if (scd.gpuFrameSamples.count() >= frameTimingWriteInterval) {
        calcTiming(&scd.gpuFrameSamples,
                   &scd.gpuFrameTime.minTime, &scd.gpuFrameTime.maxTime, &scd.gpuFrameTime.avgTime);
        if (outputDevice) {
            startEntry(QRhiProfiler::GpuFrameTime, ts.elapsed(), sc);
            writeFloat("Fmin_ms_gpu_frame_time", scd.gpuFrameTime.minTime);
            writeFloat("Fmax_ms_gpu_frame_time", scd.gpuFrameTime.maxTime);
            writeFloat("Favg_ms_gpu_frame_time", scd.gpuFrameTime.avgTime);
            endEntry();
        }
    }
}

void QRhiProfilerPrivate::newReadbackBuffer(quint64 id, QRhiResource *src, quint32 size)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::NewReadbackBuffer, ts.elapsed(), src);
    writeInt("id", id);
    writeInt("size", size);
    endEntry();
}

void QRhiProfilerPrivate::releaseReadbackBuffer(quint64 id)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::ReleaseReadbackBuffer, ts.elapsed(), nullptr);
    writeInt("id", id);
    endEntry();
}

void QRhiProfilerPrivate::vmemStat(int realAllocCount, int subAllocCount, quint32 totalSize, quint32 unusedSize)
{
    if (!outputDevice)
        return;

    startEntry(QRhiProfiler::VMemAllocStats, ts.elapsed(), nullptr);
    writeInt("realAllocCount", realAllocCount);
    writeInt("subAllocCount", subAllocCount);
    writeInt("totalSize", totalSize);
    writeInt("unusedSize", unusedSize);
    endEntry();
}

QT_END_NAMESPACE
