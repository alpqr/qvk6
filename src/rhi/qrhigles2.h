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

#ifndef QRHIGLES2_H
#define QRHIGLES2_H

#include <QtRhi/qrhi.h>

QT_BEGIN_NAMESPACE

class QOpenGLContext;
class QWindow;
class QOffscreenSurface;

struct Q_RHI_EXPORT QRhiGles2InitParams : public QRhiInitParams
{
    QOpenGLContext *context = nullptr;

    // Why do we need a window here? Because it turns out weird things will
    // happen if the first makeCurrent is with a QOffscreenSurface (which may
    // be an invisible window, which then triggers something on multi-adapter
    // systems especially). So while this is optional, apps are encouraged to
    // pass in the window for which the "swapchain" will be built.
    QWindow *window = nullptr;

    // Why doesn't the RHI create a QOffscreenSurface on its own? Because that
    // must be done on the gui/main thread while the RHI in principle can operate
    // on any (one) thread. Ownership not taken.
    QOffscreenSurface *fallbackSurface = nullptr;
};

QT_END_NAMESPACE

#endif
