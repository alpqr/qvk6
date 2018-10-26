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

#include "qbakedshader_p.h"
#include <QDataStream>
#include <QBuffer>

QT_BEGIN_NAMESPACE

QBakedShader::QBakedShader()
    : d(new QBakedShaderPrivate)
{
}

void QBakedShader::detach()
{
    if (d->ref.load() != 1) {
        QBakedShaderPrivate *newd = new QBakedShaderPrivate(d);
        if (!d->ref.deref())
            delete d;
        d = newd;
    }
}

QBakedShader::QBakedShader(const QBakedShader &other)
{
    d = other.d;
    d->ref.ref();
}

QBakedShader &QBakedShader::operator=(const QBakedShader &other)
{
    if (d != other.d) {
        other.d->ref.ref();
        if (!d->ref.deref())
            delete d;
        d = other.d;
    }
    return *this;
}

QBakedShader::~QBakedShader()
{
    if (!d->ref.deref())
        delete d;
}

bool QBakedShader::isValid() const
{
    return !d->shaders.isEmpty();
}

QBakedShader::ShaderStage QBakedShader::stage() const
{
    return d->stage;
}

void QBakedShader::setStage(ShaderStage stage)
{
    if (stage != d->stage) {
        detach();
        d->stage = stage;
    }
}

QShaderDescription QBakedShader::description() const
{
    return d->desc;
}

void QBakedShader::setDescription(const QShaderDescription &desc)
{
    detach();
    d->desc = desc;
}

QList<QBakedShader::ShaderKey> QBakedShader::availableShaders() const
{
    return d->shaders.keys();
}

QBakedShader::Shader QBakedShader::shader(const ShaderKey &key) const
{
    return d->shaders.value(key);
}

void QBakedShader::setShader(const ShaderKey &key, const Shader &shader)
{
    if (d->shaders.value(key) == shader)
        return;

    detach();
    d->shaders[key] = shader;
}

QByteArray QBakedShader::serialized() const
{
    QBuffer buf;
    QDataStream ds(&buf);
    ds.setVersion(QDataStream::Qt_5_10);
    if (!buf.open(QIODevice::WriteOnly))
        return QByteArray();

    ds << d->stage;
    ds << d->desc.toBinaryJson();
    ds << d->shaders.count();
    for (auto it = d->shaders.cbegin(), itEnd = d->shaders.cend(); it != itEnd; ++it) {
        const ShaderKey &k(it.key());
        ds << k.source;
        ds << k.sourceVersion.version;
        ds << k.sourceVersion.flags;
        ds << k.variant;
        const Shader &shader(d->shaders.value(k));
        ds << shader.shader;
        ds << shader.entryPoint;
    }

    return qCompress(buf.buffer());
}

QBakedShader QBakedShader::fromSerialized(const QByteArray &data)
{
    QByteArray udata = qUncompress(data);
    QBuffer buf(&udata);
    QDataStream ds(&buf);
    ds.setVersion(QDataStream::Qt_5_10);
    if (!buf.open(QIODevice::ReadOnly))
        return QBakedShader();

    QBakedShader bs;
    QBakedShaderPrivate *d = QBakedShaderPrivate::get(&bs);
    Q_ASSERT(d->ref.load() == 1); // must be detached
    int intVal;
    ds >> intVal;
    d->stage = ShaderStage(intVal);
    QByteArray descBin;
    ds >> descBin;
    d->desc = QShaderDescription::fromBinaryJson(descBin);
    int count;
    ds >> count;
    for (int i = 0; i < count; ++i) {
        ShaderKey k;
        ds >> intVal;
        k.source = ShaderSource(intVal);
        ds >> intVal;
        k.sourceVersion.version = intVal;
        ds >> intVal;
        k.sourceVersion.flags = ShaderSourceVersion::Flags(intVal);
        ds >> intVal;
        k.variant = ShaderVariant(intVal);
        Shader shader;
        QByteArray s;
        ds >> s;
        shader.shader = s;
        ds >> s;
        shader.entryPoint = s;
        d->shaders[k] = shader;
    }

    return bs;
}

bool operator==(const QBakedShader::ShaderSourceVersion &lhs, const QBakedShader::ShaderSourceVersion &rhs) Q_DECL_NOTHROW
{
    return lhs.version == rhs.version && lhs.flags == rhs.flags;
}

bool operator==(const QBakedShader::ShaderKey &lhs, const QBakedShader::ShaderKey &rhs) Q_DECL_NOTHROW
{
    return lhs.source == rhs.source && lhs.sourceVersion == rhs.sourceVersion
            && lhs.variant == rhs.variant;
}

bool operator==(const QBakedShader::Shader &lhs, const QBakedShader::Shader &rhs) Q_DECL_NOTHROW
{
    return lhs.shader == rhs.shader && lhs.entryPoint == rhs.entryPoint;
}

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug dbg, const QBakedShader &bs)
{
    const QBakedShaderPrivate *d = bs.d;
    QDebugStateSaver saver(dbg);

    dbg.nospace() << "QBakedShader("
                  << "stage=" << d->stage
                  << " shaders=" << d->shaders.keys()
                  << " desc.isValid=" << d->desc.isValid()
                  << ')';

    return dbg;
}

uint qHash(const QBakedShader::ShaderKey &k, uint seed)
{
    return seed + 10 * k.source + k.sourceVersion.version + k.sourceVersion.flags + k.variant;
}

QDebug operator<<(QDebug dbg, const QBakedShader::ShaderKey &k)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "ShaderKey(" << k.source
                  << " " << k.sourceVersion
                  << " " << k.variant << ")";
    return dbg;
}

QDebug operator<<(QDebug dbg, const QBakedShader::ShaderSourceVersion &v)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "Version(" << v.version << " " << v.flags << ")";
    return dbg;
}

#endif // QT_NO_DEBUG_STREAM

QT_END_NAMESPACE
