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

QRhiProfiler::QRhiProfiler()
    : d(new QRhiProfilerPrivate)
{
}

QRhiProfiler::~QRhiProfiler()
{
    delete d;
}

const QRhiProfilerStream *QRhiProfiler::stream() const
{
    return nullptr; // ###
}

void QRhiProfiler::addVMemAllocatorStats()
{
    if (d->rhiD)
        d->rhiD->sendVMemStatsToProfiler();
}

void QRhiProfilerPrivate::newBuffer(QRhiBuffer *buf, size_t realSize, int backingGpuBufCount, int backingCpuBufCount)
{
}

void QRhiProfilerPrivate::releaseBuffer(QRhiBuffer *buf)
{
}

void QRhiProfilerPrivate::newBufferStagingArea(QRhiBuffer *buf, int slot, size_t size)
{
}

void QRhiProfilerPrivate::releaseBufferStagingArea(QRhiBuffer *buf, int slot)
{
}

void QRhiProfilerPrivate::newRenderBuffer(QRhiRenderBuffer *rb, bool transientBacking, bool winSysBacking)
{
    // calc approx size
}

void QRhiProfilerPrivate::releaseRenderBuffer(QRhiRenderBuffer *rb)
{
}

void QRhiProfilerPrivate::newTexture(QRhiTexture *tex, bool owns, int mipCount, int layerCount, int sampleCount)
{
//    size_t approxSize = 0;
//    for (int i = 0; i < mipLevelCount; ++i) {
//        quint32 byteSize = 0;
//        rhiD->textureFormatInfo(m_format, size, nullptr, &byteSize);
//        approxSize += byteSize;
//    }
//    const int layerCount = isCube ? 6 : 1;
//    approxSize *= layerCount;

}

void QRhiProfilerPrivate::releaseTexture(QRhiTexture *tex)
{
}

void QRhiProfilerPrivate::newTextureStagingArea(QRhiTexture *tex, int slot, size_t size)
{
}

void QRhiProfilerPrivate::releaseTextureStagingArea(QRhiTexture *tex, int slot)
{
}

void QRhiProfilerPrivate::resizeSwapChain(QRhiSwapChain *sc, int bufferCount, int sampleCount)
{
    // calc approx size
}

void QRhiProfilerPrivate::releaseSwapChain(QRhiSwapChain *sc)
{
}

QT_END_NAMESPACE
