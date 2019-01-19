/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Shader Tools module
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

#ifndef QBAKEDSHADER_P_H
#define QBAKEDSHADER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "qtshadertoolsglobal_p.h"
#include "qbakedshader.h"
#include <QtCore/QAtomicInt>
#include <QtCore/QHash>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

struct Q_SHADERTOOLS_PRIVATE_EXPORT QBakedShaderPrivate
{
    QBakedShaderPrivate()
        : ref(1)
    {
    }

    QBakedShaderPrivate(const QBakedShaderPrivate *other)
        : ref(1),
          stage(other->stage),
          desc(other->desc),
          shaders(other->shaders)
    {
    }

    static QBakedShaderPrivate *get(QBakedShader *s) { return s->d; }
    static const QBakedShaderPrivate *get(const QBakedShader *s) { return s->d; }

    QAtomicInt ref;
    QBakedShader::ShaderStage stage = QBakedShader::VertexStage;
    QShaderDescription desc;
    QHash<QBakedShaderKey, QBakedShaderCode> shaders;
};

QT_END_NAMESPACE

#endif
