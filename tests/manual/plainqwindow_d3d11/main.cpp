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
#include <QRhiD3D11InitParams>
#include "examplewindow.h"

class D3D11Window : public ExampleWindow
{
public:
    D3D11Window() { setSurfaceType(OpenGLSurface); }
    ~D3D11Window() { releaseResources(); }

private:
    void init() override;
};

void D3D11Window::init()
{
    QRhiD3D11InitParams params;
    params.enableDebugLayer = true;
    m_r = QRhi::create(QRhi::D3D11, &params);

    setOnScreenOnly(true); // ###
    ExampleWindow::init();
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    D3D11Window w;
    w.resize(1280, 720);
    w.setTitle(QLatin1String("D3D11"));
    w.show();

    return app.exec();
}
