/****************************************************************************
 **
 ** Copyright (C) 2018 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the test suite of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
 ** General Public License version 3 as published by the Free Software
 ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
 ** included in the packaging of this file. Please review the following
 ** information to ensure the GNU General Public License requirements will
 ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#include <QGuiApplication>
#include <QRhiGles2InitParams>
#include "examplewindow.h"

class GlWindow : public ExampleWindow
{
public:
    GlWindow() { setSurfaceType(OpenGLSurface); }
    ~GlWindow() { releaseResources(); }

private:
    void init() override;
    void releaseResources() override;
    void prepareRender() override;

    QOpenGLContext *ctx = nullptr;
};

void GlWindow::init()
{
    ctx = new QOpenGLContext;
    ctx->create();

    QRhiGles2InitParams params;
    params.context = ctx;
    m_r = QRhi::create(QRhi::OpenGLES2, &params);

    ctx->makeCurrent(this);
    ExampleWindow::init();
}

void GlWindow::releaseResources()
{
    if (!ctx)
        return;

    ctx->makeCurrent(this);
    ExampleWindow::releaseResources();
    ctx->doneCurrent();

    delete ctx;
    ctx = nullptr;
}

void GlWindow::prepareRender()
{
    ctx->makeCurrent(this);
}

int main(int argc, char **argv)
{
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
