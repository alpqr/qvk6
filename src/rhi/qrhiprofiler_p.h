/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt RHI module
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QRHIPROFILER_P_H
#define QRHIPROFILER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qtrhiglobal_p.h"
#include "qrhiprofiler.h"
#include <QElapsedTimer>
#include <QHash>

QT_BEGIN_NAMESPACE

class QRhiProfilerPrivate
{
public:
    static QRhiProfilerPrivate *get(QRhiProfiler *p) { return p->d; }

    void newBuffer(QRhiBuffer *buf, quint32 realSize, int backingGpuBufCount, int backingCpuBufCount);
    void releaseBuffer(QRhiBuffer *buf);
    void newBufferStagingArea(QRhiBuffer *buf, int slot, quint32 size);
    void releaseBufferStagingArea(QRhiBuffer *buf, int slot);

    void newRenderBuffer(QRhiRenderBuffer *rb, bool transientBacking, bool winSysBacking, int sampleCount);
    void releaseRenderBuffer(QRhiRenderBuffer *rb);

    void newTexture(QRhiTexture *tex, bool owns, int mipCount, int layerCount, int sampleCount);
    void releaseTexture(QRhiTexture *tex);
    void newTextureStagingArea(QRhiTexture *tex, int slot, quint32 size);
    void releaseTextureStagingArea(QRhiTexture *tex, int slot);

    void resizeSwapChain(QRhiSwapChain *sc, int bufferCount, int msaaBufferCount, int sampleCount);
    void releaseSwapChain(QRhiSwapChain *sc);

    void beginSwapChainFrame(QRhiSwapChain *sc);
    void endSwapChainFrame(QRhiSwapChain *sc, int frameCount);
    void swapChainFrameGpuTime(QRhiSwapChain *sc, float gpuTimeMs);

    void newReadbackBuffer(quint64 id, QRhiResource *src, quint32 size);
    void releaseReadbackBuffer(quint64 id);

    void vmemStat(int realAllocCount, int subAllocCount, quint32 totalSize, quint32 unusedSize);

    void startEntry(QRhiProfiler::StreamOp op, qint64 timestamp, QRhiResource *res);
    void writeInt(const char *key, qint64 v);
    void writeFloat(const char *key, float f);
    void endEntry();

    QRhi *rhi = nullptr;
    QRhiImplementation *rhiD = nullptr;
    QIODevice *outputDevice = nullptr;
    QElapsedTimer ts;
    QByteArray buf;
    static const int DEFAULT_FRAME_TIMING_WRITE_INTERVAL = 120; // frames
    int frameTimingWriteInterval = DEFAULT_FRAME_TIMING_WRITE_INTERVAL;
    struct Sc {
        Sc() {
            frameToFrameDelta.reserve(DEFAULT_FRAME_TIMING_WRITE_INTERVAL);
            beginToEndDelta.reserve(DEFAULT_FRAME_TIMING_WRITE_INTERVAL);
            gpuFrameTime.reserve(DEFAULT_FRAME_TIMING_WRITE_INTERVAL);
        }
        QElapsedTimer frameToFrameTimer;
        bool frameToFrameRunning = false;
        QElapsedTimer beginToEndTimer;
        QVector<qint64> frameToFrameDelta;
        QVector<qint64> beginToEndDelta;
        QVector<float> gpuFrameTime;
    };
    QHash<QRhiSwapChain *, Sc> swapchains;
};

Q_DECLARE_TYPEINFO(QRhiProfilerPrivate::Sc, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
