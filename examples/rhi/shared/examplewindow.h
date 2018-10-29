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

#ifndef EXAMPLEWINDOW_H
#define EXAMPLEWINDOW_H

#include <QRhi>
#include <QWindow>
#include <QPlatformSurfaceEvent>
#include "trianglerenderer.h"
#include "quadrenderer.h"
#include "texturedcuberenderer.h"
#include "triangleoncuberenderer.h"

class ExampleWindow : public QWindow
{
public:
    virtual void init();
    virtual void releaseResources();
    virtual void recreateSwapChain();
    virtual void releaseSwapChain();

    void setSampleCount(int sampleCount) { m_sampleCount = sampleCount; }
    void setOnScreenOnly(bool v) { m_onScreenOnly = v; }
    void setTriangleOnly(bool v) { m_triangleOnly = v; }

protected:
    void render();
    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

    bool m_running = false;
    bool m_notExposed = false;
    bool m_newlyExposed = false;

    QRhi *m_r = nullptr;
    bool m_hasSwapChain = false;
    bool m_resizedSwapChain = false;
    QRhiSwapChain *m_sc = nullptr;
    QRhiRenderPassDescriptor *m_scrp = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;

    TriangleRenderer m_triRenderer;
    QuadRenderer m_quadRenderer;
    TexturedCubeRenderer m_cubeRenderer;
    TriangleOnCubeRenderer m_liveTexCubeRenderer;

    int m_sampleCount = 1;
    bool m_onScreenOnly = false;
    bool m_triangleOnly = false;
};

#endif
