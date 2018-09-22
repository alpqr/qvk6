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

#ifndef EXAMPLEWINDOW_H
#define EXAMPLEWINDOW_H

#include <QRhi>
#include <QWindow>
#include <QPlatformSurfaceEvent>
#include "trianglerenderer.h"
#include "texturedcuberenderer.h"
#include "triangleoncuberenderer.h"

class ExampleWindow : public QWindow
{
public:
    virtual void init();
    virtual void releaseResources();
    virtual void recreateSwapChain();
    virtual void releaseSwapChain();

protected:
    void render();
    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

    bool m_inited = false;

    QRhi *m_r = nullptr;
    bool m_hasSwapChain = false;
    bool m_swapChainChanged = false;
    QRhiSwapChain *m_sc = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;

    TriangleRenderer m_triRenderer;
    TexturedCubeRenderer m_cubeRenderer;
    TriangleOnCubeRenderer m_liveTexCubeRenderer;
};

#endif
