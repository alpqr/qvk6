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

#ifndef QRHID3D11_H
#define QRHID3D11_H

#include <QtRhi/qrhi.h>

#ifndef Q_OS_MAC
#include <d3d11_1.h>

QT_BEGIN_NAMESPACE

struct Q_RHI_EXPORT QRhiD3D11InitParams : public QRhiInitParams
{
    bool enableDebugLayer = false;

    bool importExistingDevice = false;
    // both must be given when importExistingDevice is true. ownership not taken.
    // leave them unset otherwise.
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *context = nullptr;
};

QT_END_NAMESPACE

#endif // Q_OS_MAC

#endif
