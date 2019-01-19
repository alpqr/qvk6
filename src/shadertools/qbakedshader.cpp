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

/*!
    \class QBakedShader
    \inmodule QtShaderTools

    \brief Contains multiple versions of a shader translated to multiple shading languages,
    together with reflection metadata.

    QBakedShader is the entry point to shader code in the graphics API agnostic
    Qt world. Instead of using GLSL shader sources, as was the custom with Qt
    5.x, new graphics systems with backends for multiple graphics APIs, such
    as, Vulkan, Metal, Direct3D, and OpenGL, take QBakedShader as their input
    whenever a shader needs to be specified.

    A QBakedShader instance is empty and thus invalid by default. To get a useful
    instance, the two typical methods are:

    \list

    \li Generate the contents offline, during build time or earlier, using the
    \c qsb command line tool. The result is a binary file that is shipped with
    the application, read via QIODevice::readAll(), and then deserialized via
    fromSerialized(). For more information, see QShaderBaker.

    \li Generate at run time via QShaderBaker. This is an expensive operation,
    but allows applications to use user-provided or dynamically generated
    shader source strings.

    \endlist

    When used together with the Qt Rendering Hardware Interface and its
    classes, like QRhiGraphicsPipeline, no further action is needed from the
    application's side as these classes are prepared to consume a QBakedShader
    whenever a shader needs to be specified for a given stage of the graphics
    pipeline.

    Alternatively, applications can access

    \list

    \li the source or byte code for any of the shading language versions that
    are included in the QBakedShader,

    \li the name of the entry point for the shader,

    \li the reflection metadata containing a description of the shader's
    inputs, outputs and resources like uniform blocks. This is essential when
    an application or framework needs to discover the inputs of a shader at
    runtime due to not having advance knowledge of the vertex attributes or the
    layout of the uniform buffers used by the shader.

    \endlist

    QBakedShader makes no assumption about the shading language that was used
    as the source for generating the various versions and variants that are
    included in it.

    QBakedShader uses implicit sharing similarly to many core Qt types, and so
    can be returned or passed by value. Detach happens implicitly when calling
    a setter.

    For reference, QRhi expects that a QBakedShader suitable for all its
    backends contains at least the following:

    \list

    \li SPIR-V 1.0 bytecode suitable for Vulkan 1.0 or newer

    \li GLSL/ES 100 source code suitable for OpenGL ES 2.0 or newer

    \li GLSL 120 source code suitable for OpenGL 2.1

    \li HLSL Shader Model 5.0 source code or the corresponding DXBC bytecode suitable for Direct3D 11

    \li Metal Shading Language 1.2 source code or the corresponding bytecode suitable for Metal

    \endlist

    \sa QShaderBaker
 */

/*!
    \enum QBakedShader::ShaderStage
    Describes the stage of the graphics pipeline the shader is suitable for.

    \value VertexStage Vertex shader
    \value TessControlStage Tessellation control (hull) shader
    \value TessEvaluationStage Tessellation evaluation (domain) shader
    \value GeometryStage Geometry shader
    \value FragmentStage Fragment (pixel) shader
    \value ComputeStage Compute shader
 */

/*!
    \class QBakedShaderVersion
    \inmodule QtShaderTools

    \brief Specifies the shading language version.

    While languages like SPIR-V or the Metal Shading Language use traditional
    version numbers, shaders for other APIs can use slightly different
    versioning schemes. All those are mapped to a single version number in
    here, however. For HLSL, the version refers to the Shader Model version,
    like 5.0, 5.1, or 6.0. For GLSL an additional flag is needed to choose
    between GLSL and GLSL/ES.

    Below is a list with the most common examples of shader versions for
    different graphics APIs:

    \list

    \li Vulkan (SPIR-V): 100
    \li OpenGL: 120, 330, 440, etc.
    \li OpenGL ES: 100 with GlslEs, 300 with GlslEs, etc.
    \li Direct3D: 50, 51, 60
    \li Metal: 12, 20
    \endlist

    A default constructed QBakedShaderVersion contains a version of 100 and no
    flags set.
 */

/*!
    \enum QBakedShaderVersion::Flag

    Describes the flags that can be set.

    \value GlslEs Indicates that GLSL/ES is meant in combination with GlslShader
 */

/*!
    \class QBakedShaderKey
    \inmodule QtShaderTools

    \brief Specifies the shading language, the version with flags, and the variant.

    A default constructed QBakedShaderKey has source set to SpirvShader and
    sourceVersion set to 100. sourceVariant defaults to StandardShader.
 */

/*!
    \enum QBakedShaderKey::ShaderSource
    Describes what kind of shader code an entry contains.

    \value SpirvShader SPIR-V
    \value GlslShader GLSL
    \value HlslShader HLSL
    \value DxbcShader Direct3D bytecode (HLSL compiled by \c fxc)
    \value MslShader Metal Shading Language
    \value DxilShader Direct3D bytecode (HLSL compiled by \c dxc)
    \value MetalLibShader Pre-compiled Metal bytecode
 */

/*!
    \enum QBakedShaderKey::ShaderVariant
    Describes what kind of shader code an entry contains.

    \value StandardShader The normal, unmodified version of the shader code.
    \value BatchableVertexShader Vertex shader rewritten to be suitable for Qt Quick scenegraph batching.
 */

/*!
    \class QBakedShaderCode
    \inmodule QtShaderTools

    \brief Contains source or binary code for a shader and additional metadata.

    When shader() is empty after retrieving a QBakedShaderCode instance from
    QBakedShader, it indicates no shader code was found for the requested key.
 */

static const int QSB_VERSION = 1;

/*!
    Constructs a new, empty (and thus invalid) QBakedShader instance.
 */
QBakedShader::QBakedShader()
    : d(new QBakedShaderPrivate)
{
}

/*!
    \internal
 */
void QBakedShader::detach()
{
    if (d->ref.load() != 1) {
        QBakedShaderPrivate *newd = new QBakedShaderPrivate(d);
        if (!d->ref.deref())
            delete d;
        d = newd;
    }
}

/*!
    \internal
 */
QBakedShader::QBakedShader(const QBakedShader &other)
{
    d = other.d;
    d->ref.ref();
}

/*!
    \internal
 */
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

/*!
    Destructor.
 */
QBakedShader::~QBakedShader()
{
    if (!d->ref.deref())
        delete d;
}

/*!
    \return true if the QBakedShader contains at least one shader version.
 */
bool QBakedShader::isValid() const
{
    return !d->shaders.isEmpty();
}

/*!
    \return the pipeline stage the shader is meant for.
 */
QBakedShader::ShaderStage QBakedShader::stage() const
{
    return d->stage;
}

/*!
    Sets the pipeline \a stage.
 */
void QBakedShader::setStage(ShaderStage stage)
{
    if (stage != d->stage) {
        detach();
        d->stage = stage;
    }
}

/*!
    \return the reflection metadata for the shader.
 */
QShaderDescription QBakedShader::description() const
{
    return d->desc;
}

/*!
    Sets the reflection metadata to \a desc.
 */
void QBakedShader::setDescription(const QShaderDescription &desc)
{
    detach();
    d->desc = desc;
}

/*!
    \return the list of available shader versions
 */
QList<QBakedShaderKey> QBakedShader::availableShaders() const
{
    return d->shaders.keys();
}

/*!
    \return the source or binary code for a given shader version specified by \a key.
 */
QBakedShaderCode QBakedShader::shader(const QBakedShaderKey &key) const
{
    return d->shaders.value(key);
}

/*!
    Stores the source or binary \a shader code for a given shader version specified by \a key.
 */
void QBakedShader::setShader(const QBakedShaderKey &key, const QBakedShaderCode &shader)
{
    if (d->shaders.value(key) == shader)
        return;

    detach();
    d->shaders[key] = shader;
}

/*!
    Removes the source or binary shader code for a given \a key.
    Does nothing when not found.
 */
void QBakedShader::removeShader(const QBakedShaderKey &key)
{
    auto it = d->shaders.find(key);
    if (it == d->shaders.end())
        return;

    detach();
    d->shaders.erase(it);
}

/*!
    \return a serialized binary version of all the data held by the
    QBakedShader, suitable for writing to files or other I/O devices.

    \sa fromSerialized()
 */
QByteArray QBakedShader::serialized() const
{
    QBuffer buf;
    QDataStream ds(&buf);
    ds.setVersion(QDataStream::Qt_5_10);
    if (!buf.open(QIODevice::WriteOnly))
        return QByteArray();

    ds << QSB_VERSION;
    ds << d->stage;
    ds << d->desc.toBinaryJson();
    ds << d->shaders.count();
    for (auto it = d->shaders.cbegin(), itEnd = d->shaders.cend(); it != itEnd; ++it) {
        const QBakedShaderKey &k(it.key());
        ds << k.source();
        ds << k.sourceVersion().version();
        ds << k.sourceVersion().flags();
        ds << k.sourceVariant();
        const QBakedShaderCode &shader(d->shaders.value(k));
        ds << shader.shader();
        ds << shader.entryPoint();
    }

    return qCompress(buf.buffer());
}

/*!
    Creates a new QBakedShader instance from the given \a data.

    \sa serialized()
  */
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
    if (intVal != QSB_VERSION)
        return QBakedShader();

    ds >> intVal;
    d->stage = ShaderStage(intVal);
    QByteArray descBin;
    ds >> descBin;
    d->desc = QShaderDescription::fromBinaryJson(descBin);
    int count;
    ds >> count;
    for (int i = 0; i < count; ++i) {
        QBakedShaderKey k;
        ds >> intVal;
        k.setSource(QBakedShaderKey::ShaderSource(intVal));
        QBakedShaderVersion ver;
        ds >> intVal;
        ver.setVersion(intVal);
        ds >> intVal;
        ver.setFlags(QBakedShaderVersion::Flags(intVal));
        k.setSourceVersion(ver);
        ds >> intVal;
        k.setSourceVariant(QBakedShaderKey::ShaderVariant(intVal));
        QBakedShaderCode shader;
        QByteArray s;
        ds >> s;
        shader.setShader(s);
        ds >> s;
        shader.setEntryPoint(s);
        d->shaders[k] = shader;
    }

    return bs;
}

bool operator==(const QBakedShaderVersion &lhs, const QBakedShaderVersion &rhs) Q_DECL_NOTHROW
{
    return lhs.version() == rhs.version() && lhs.flags() == rhs.flags();
}

bool operator==(const QBakedShaderKey &lhs, const QBakedShaderKey &rhs) Q_DECL_NOTHROW
{
    return lhs.source() == rhs.source() && lhs.sourceVersion() == rhs.sourceVersion()
            && lhs.sourceVariant() == rhs.sourceVariant();
}

bool operator==(const QBakedShaderCode &lhs, const QBakedShaderCode &rhs) Q_DECL_NOTHROW
{
    return lhs.shader() == rhs.shader() && lhs.entryPoint() == rhs.entryPoint();
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

uint qHash(const QBakedShaderKey &k, uint seed)
{
    return seed + 10 * k.source() + k.sourceVersion().version() + k.sourceVersion().flags() + k.sourceVariant();
}

QDebug operator<<(QDebug dbg, const QBakedShaderKey &k)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "ShaderKey(" << k.source()
                  << " " << k.sourceVersion()
                  << " " << k.sourceVariant() << ")";
    return dbg;
}

QDebug operator<<(QDebug dbg, const QBakedShaderVersion &v)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "Version(" << v.version() << " " << v.flags() << ")";
    return dbg;
}

#endif // QT_NO_DEBUG_STREAM

QT_END_NAMESPACE
