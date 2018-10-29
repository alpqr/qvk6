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
#include <QRhiGles2InitParams>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include "examplewindow.h"

class GlWindow : public ExampleWindow
{
public:
    GlWindow() { setSurfaceType(OpenGLSurface); }
    ~GlWindow() { releaseResources(); }

private:
    void init() override;
    void releaseResources() override;

    QOpenGLContext *ctx = nullptr;
    QOffscreenSurface *fallbackSurface = nullptr;
};

void GlWindow::init()
{
    ctx = new QOpenGLContext;
    if (!ctx->create())
        qFatal("Failed to get OpenGL context");

    fallbackSurface = new QOffscreenSurface;
    fallbackSurface->setFormat(ctx->format());
    fallbackSurface->create();

    QRhiGles2InitParams params;
    params.context = ctx;
    params.window = this;
    params.fallbackSurface = fallbackSurface;
    m_r = QRhi::create(QRhi::OpenGLES2, &params);

    ExampleWindow::init();
}

void GlWindow::releaseResources()
{
    ExampleWindow::releaseResources();
    delete ctx;
    ctx = nullptr;
    delete fallbackSurface;
    fallbackSurface = nullptr;
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    GlWindow w;
    w.resize(1280, 720);
    w.setTitle(QLatin1String("OpenGL"));
    w.show();
    return app.exec();
}
