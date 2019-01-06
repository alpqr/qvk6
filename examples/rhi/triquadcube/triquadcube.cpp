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

// An example exercising more than a single feature. Enables profiling
// (resource logging to a CBOR file) and inserts debug markers and sets some
// object names. Can also be used to test MSAA swapchains, swapchain image
// readback, requesting an sRGB swapchain, and some texture features.

#define EXAMPLEFW_PREINIT
#include "../shared/examplefw.h"
#include "trianglerenderer.h"
#include "quadrenderer.h"
#include "texturedcuberenderer.h"
#include "triangleoncuberenderer.h"

#include <QFileInfo>
#include <QFile>
#include <QRhiProfiler>

#define PROFILE_TO_FILE
//#define USE_MSAA
//#define USE_SRGB_SWAPCHAIN
//#define READBACK_SWAPCHAIN

struct {
    TriangleRenderer triRenderer;
    QuadRenderer quadRenderer;
    TexturedCubeRenderer cubeRenderer;
    TriangleOnCubeRenderer liveTexCubeRenderer;
    int sampleCount = 1;
    bool onScreenOnly = false;
    bool triangleOnly = false;
    QSize lastOutputSize;
    int frameCount = 0;
    QFile profOut;
} d;

void preInit()
{
    rhiFlags |= QRhi::EnableDebugMarkers;

#ifdef PROFILE_TO_FILE
    rhiFlags |= QRhi::EnableProfiling;
    const QString profFn = QFileInfo(QLatin1String("rhiprof.cbor")).absoluteFilePath();
    qDebug("Writing profiling output to %s", qPrintable(profFn));
    d.profOut.setFileName(profFn);
    d.profOut.open(QIODevice::WriteOnly);
#endif

#ifdef USE_MSAA
    sampleCount = 4; // enable 4x MSAA (except for the render-to-texture pass)
#endif

#ifdef READBACK_SWAPCHAIN
    scFlags |= QRhiSwapChain::UsedAsTransferSource;
#endif

#ifdef USE_SRGB_SWAPCHAIN
    scFlags |= QRhiSwapChain::sRGB;
#endif
}

void Window::customInit()
{
#ifdef PROFILE_TO_FILE
    m_r->profiler()->setDevice(&d.profOut);
#endif

    d.triRenderer.setRhi(m_r);
    d.triRenderer.setSampleCount(d.sampleCount);
    d.triRenderer.initResources(m_rp);

    if (!d.triangleOnly) {
        d.triRenderer.setTranslation(QVector3D(0, 0.5f, 0));

        d.quadRenderer.setRhi(m_r);
        d.quadRenderer.setSampleCount(d.sampleCount);
        d.quadRenderer.setPipeline(d.triRenderer.pipeline());
        d.quadRenderer.initResources(m_rp);
        d.quadRenderer.setTranslation(QVector3D(1.5f, -0.5f, 0));

        d.cubeRenderer.setRhi(m_r);
        d.cubeRenderer.setSampleCount(d.sampleCount);
        d.cubeRenderer.initResources(m_rp);
        d.cubeRenderer.setTranslation(QVector3D(0, -0.5f, 0));
    }

    if (!d.onScreenOnly) {
        d.liveTexCubeRenderer.setRhi(m_r);
        d.liveTexCubeRenderer.setSampleCount(d.sampleCount);
        d.liveTexCubeRenderer.initResources(m_rp);
        d.liveTexCubeRenderer.setTranslation(QVector3D(-2.0f, 0, 0));
    }

    // Put the gpu mem allocator statistics to the profiling stream after doing
    // all the init. (where applicable)
    m_r->profiler()->addVMemAllocatorStats();

    // Check some features/limits.
    qDebug("isFeatureSupported(MultisampleTexture): %d", m_r->isFeatureSupported(QRhi::MultisampleTexture));
    qDebug("isFeatureSupported(MultisampleRenderBuffer): %d", m_r->isFeatureSupported(QRhi::MultisampleRenderBuffer));
    qDebug("isFeatureSupported(DebugMarkers): %d", m_r->isFeatureSupported(QRhi::DebugMarkers));
    qDebug("Min 2D texture width/height: %d", m_r->resourceSizeLimit(QRhi::TextureSizeMin));
    qDebug("Max 2D texture width/height: %d", m_r->resourceSizeLimit(QRhi::TextureSizeMax));
}

void Window::customRelease()
{
    d.triRenderer.releaseResources();

    if (!d.triangleOnly) {
        d.quadRenderer.releaseResources();
        d.cubeRenderer.releaseResources();
    }

    if (!d.onScreenOnly)
        d.liveTexCubeRenderer.releaseResources();
}

void Window::customRender()
{
    const QSize outputSize = m_sc->currentPixelSize();
    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();

    if (outputSize != d.lastOutputSize) {
        d.triRenderer.resize(outputSize);
        if (!d.triangleOnly) {
            d.quadRenderer.resize(outputSize);
            d.cubeRenderer.resize(outputSize);
        }
        if (!d.onScreenOnly)
            d.liveTexCubeRenderer.resize(outputSize);
        d.lastOutputSize = outputSize;
    }

    if (!d.onScreenOnly) {
        cb->debugMarkBegin("Offscreen triangle pass");
        d.liveTexCubeRenderer.queueOffscreenPass(cb);
        cb->debugMarkEnd();
    }

    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();
    d.triRenderer.queueResourceUpdates(u);
    if (!d.triangleOnly) {
        d.quadRenderer.queueResourceUpdates(u);
        d.cubeRenderer.queueResourceUpdates(u);
    }
    if (!d.onScreenOnly)
        d.liveTexCubeRenderer.queueResourceUpdates(u);

    cb->beginPass(m_sc->currentFrameRenderTarget(), { 0.4f, 0.7f, 0.0f, 1.0f }, { 1.0f, 0 }, u);
    cb->debugMarkBegin(QByteArrayLiteral("Triangle"));
    d.triRenderer.queueDraw(cb, outputSize);
    cb->debugMarkEnd();
    if (!d.triangleOnly) {
        cb->debugMarkMsg(QByteArrayLiteral("More stuff"));
        cb->debugMarkBegin(QByteArrayLiteral("Quad"));
        d.quadRenderer.queueDraw(cb, outputSize);
        cb->debugMarkEnd();
        cb->debugMarkBegin(QByteArrayLiteral("Cube"));
        d.cubeRenderer.queueDraw(cb, outputSize);
        cb->debugMarkEnd();
    }
    if (!d.onScreenOnly) {
        cb->debugMarkMsg(QByteArrayLiteral("Even more stuff"));
        cb->debugMarkBegin(QByteArrayLiteral("Cube with offscreen triangle"));
        d.liveTexCubeRenderer.queueDraw(cb, outputSize);
        cb->debugMarkEnd();
    }

    QRhiResourceUpdateBatch *passEndUpdates = nullptr;
#ifdef READBACK_SWAPCHAIN
    passEndUpdates = m_r->nextResourceUpdateBatch();
    QRhiReadbackDescription rb; // no texture given -> backbuffer
    QRhiReadbackResult *rbResult = new QRhiReadbackResult;
    int frameNo = d.frameCount;
    rbResult->completed = [this, rbResult, frameNo] {
        {
            QImage::Format fmt = rbResult->format == QRhiTexture::BGRA8 ? QImage::Format_ARGB32_Premultiplied
                                                                        : QImage::Format_RGBA8888_Premultiplied;
            const uchar *p = reinterpret_cast<const uchar *>(rbResult->data.constData());
            QImage image(p, rbResult->pixelSize.width(), rbResult->pixelSize.height(), fmt);
            QString fn = QString::asprintf("frame%d.png", frameNo);
            fn = QFileInfo(fn).absoluteFilePath();
            qDebug("Saving into %s", qPrintable(fn));
            if (m_r->isYUpInFramebuffer())
                image.mirrored().save(fn);
            else
                image.save(fn);
        }
        delete rbResult;
    };
    passEndUpdates->readBackTexture(rb, rbResult);
#endif

    cb->endPass(passEndUpdates);

    d.frameCount += 1;
}