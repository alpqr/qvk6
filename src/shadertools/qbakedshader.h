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

#ifndef QBAKEDSHADER_H
#define QBAKEDSHADER_H

#include <QtShaderTools/qtshadertoolsglobal.h>
#include <QtShaderTools/qshaderdescription.h>

QT_BEGIN_NAMESPACE

struct QBakedShaderPrivate;

class Q_SHADERTOOLS_EXPORT QBakedShaderVersion
{
public:
    enum Flag {
        GlslEs = 0x01
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QBakedShaderVersion() { }
    QBakedShaderVersion(int v, Flags f = Flags())
        : m_version(v), m_flags(f)
    { }

    int version() const { return m_version; }
    void setVersion(int v) { m_version = v; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

private:
    int m_version = 100;
    Flags m_flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QBakedShaderVersion::Flags)
Q_DECLARE_TYPEINFO(QBakedShaderVersion, Q_MOVABLE_TYPE);

class Q_SHADERTOOLS_EXPORT QBakedShaderKey
{
public:
    enum ShaderSource {
        SpirvShader = 0,
        GlslShader,
        HlslShader,
        DxbcShader, // fxc
        MslShader,
        DxilShader, // dxc
        MetalLibShader // xcrun metal + xcrun metallib
    };

    enum ShaderVariant {
        StandardShader = 0,
        BatchableVertexShader
    };

    QBakedShaderKey() { }
    QBakedShaderKey(ShaderSource s,
                    const QBakedShaderVersion &sver,
                    ShaderVariant svar = StandardShader)
        : m_source(s),
          m_sourceVersion(sver),
          m_sourceVariant(svar)
    { }

    ShaderSource source() const { return m_source; }
    void setSource(ShaderSource s) { m_source = s; }

    QBakedShaderVersion sourceVersion() const { return m_sourceVersion; }
    void setSourceVersion(const QBakedShaderVersion &sver) { m_sourceVersion = sver; }

    ShaderVariant sourceVariant() const { return m_sourceVariant; }
    void setSourceVariant(ShaderVariant svar) { m_sourceVariant = svar; }

private:
    ShaderSource m_source = SpirvShader;
    QBakedShaderVersion m_sourceVersion;
    ShaderVariant m_sourceVariant = StandardShader;
};

Q_DECLARE_TYPEINFO(QBakedShaderKey, Q_MOVABLE_TYPE);

class Q_SHADERTOOLS_EXPORT QBakedShaderCode
{
public:
    QBakedShaderCode() { }
    QBakedShaderCode(const QByteArray &code, const QByteArray &entry = QByteArray())
        : m_shader(code), m_entryPoint(entry)
    { }

    QByteArray shader() const { return m_shader; }
    void setShader(const QByteArray &code) { m_shader = code; }

    QByteArray entryPoint() const { return m_entryPoint; }
    void setEntryPoint(const QByteArray &entry) { m_entryPoint = entry; }

private:
    QByteArray m_shader;
    QByteArray m_entryPoint;
};

Q_DECLARE_TYPEINFO(QBakedShaderCode, Q_MOVABLE_TYPE);

class Q_SHADERTOOLS_EXPORT QBakedShader
{
public:
    enum ShaderStage {
        VertexStage = 0,
        TessControlStage,
        TessEvaluationStage,
        GeometryStage,
        FragmentStage,
        ComputeStage
    };

    QBakedShader();
    QBakedShader(const QBakedShader &other);
    QBakedShader &operator=(const QBakedShader &other);
    ~QBakedShader();
    void detach();

    bool isValid() const;

    ShaderStage stage() const;
    void setStage(ShaderStage stage);

    QShaderDescription description() const;
    void setDescription(const QShaderDescription &desc);

    QList<QBakedShaderKey> availableShaders() const;
    QBakedShaderCode shader(const QBakedShaderKey &key) const;
    void setShader(const QBakedShaderKey &key, const QBakedShaderCode &shader);
    void removeShader(const QBakedShaderKey &key);

    QByteArray serialized() const;
    static QBakedShader fromSerialized(const QByteArray &data);

private:
    QBakedShaderPrivate *d;
    friend struct QBakedShaderPrivate;
#ifndef QT_NO_DEBUG_STREAM
    friend Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QBakedShader &);
#endif
};

Q_SHADERTOOLS_EXPORT bool operator==(const QBakedShaderVersion &lhs, const QBakedShaderVersion &rhs) Q_DECL_NOTHROW;
Q_SHADERTOOLS_EXPORT bool operator==(const QBakedShaderKey &lhs, const QBakedShaderKey &rhs) Q_DECL_NOTHROW;
Q_SHADERTOOLS_EXPORT bool operator==(const QBakedShaderCode &lhs, const QBakedShaderCode &rhs) Q_DECL_NOTHROW;

inline bool operator!=(const QBakedShaderVersion &lhs, const QBakedShaderVersion &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

inline bool operator!=(const QBakedShaderKey &lhs, const QBakedShaderKey &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

inline bool operator!=(const QBakedShaderCode &lhs, const QBakedShaderCode &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

Q_SHADERTOOLS_EXPORT uint qHash(const QBakedShaderKey &k, uint seed = 0);

#ifndef QT_NO_DEBUG_STREAM
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QBakedShader &);
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug dbg, const QBakedShaderKey &k);
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug dbg, const QBakedShaderVersion &v);
#endif

QT_END_NAMESPACE

#endif
