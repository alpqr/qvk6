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

    enum ShaderSource {
        SpirvShader = 0,
        GlslShader,
        HlslShader,
        DxbcShader, // fxc
        MslShader,
        DxilShader, // dxc
        MetalLibShader // xcrun metal + xcrun metallib
    };

    struct ShaderSourceVersion {
        enum Flag {
            GlslEs = 0x01
        };
        Q_DECLARE_FLAGS(Flags, Flag)

        ShaderSourceVersion() { }
        ShaderSourceVersion(int version_, Flags flags_ = Flags())
            : version(version_), flags(flags_)
        { }

        int version = 100;
        Flags flags;
    };

    enum ShaderVariant {
        NormalShader = 0,
        BatchableVertexShader
    };

    struct ShaderKey {
        ShaderKey() { }
        ShaderKey(ShaderSource source_,
                  const ShaderSourceVersion &sourceVersion_ = ShaderSourceVersion(),
                  ShaderVariant variant_ = QBakedShader::NormalShader)
            : source(source_),
              sourceVersion(sourceVersion_),
              variant(variant_)
        { }

        ShaderSource source = SpirvShader;
        ShaderSourceVersion sourceVersion;
        ShaderVariant variant = QBakedShader::NormalShader;
    };

    struct Shader {
        Shader() { }
        Shader(const QByteArray &shader_, const QByteArray &entryPoint_ = QByteArray())
            : shader(shader_), entryPoint(entryPoint_)
        { }

        QByteArray shader;
        QByteArray entryPoint;
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

    QList<ShaderKey> availableShaders() const;
    Shader shader(const ShaderKey &key) const;
    void setShader(const ShaderKey &key, const Shader &shader);
    void removeShader(const ShaderKey &key);

    QByteArray serialized() const;
    static QBakedShader fromSerialized(const QByteArray &data);

private:
    QBakedShaderPrivate *d;
    friend struct QBakedShaderPrivate;
#ifndef QT_NO_DEBUG_STREAM
    friend Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QBakedShader &);
#endif
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QBakedShader::ShaderSourceVersion::Flags)

Q_SHADERTOOLS_EXPORT bool operator==(const QBakedShader::ShaderSourceVersion &lhs, const QBakedShader::ShaderSourceVersion &rhs) Q_DECL_NOTHROW;
Q_SHADERTOOLS_EXPORT bool operator==(const QBakedShader::ShaderKey &lhs, const QBakedShader::ShaderKey &rhs) Q_DECL_NOTHROW;
Q_SHADERTOOLS_EXPORT bool operator==(const QBakedShader::Shader &lhs, const QBakedShader::Shader &rhs) Q_DECL_NOTHROW;

inline bool operator!=(const QBakedShader::ShaderSourceVersion &lhs, const QBakedShader::ShaderSourceVersion &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

inline bool operator!=(const QBakedShader::ShaderKey &lhs, const QBakedShader::ShaderKey &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

inline bool operator!=(const QBakedShader::Shader &lhs, const QBakedShader::Shader &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

Q_SHADERTOOLS_EXPORT uint qHash(const QBakedShader::ShaderKey &k, uint seed = 0);

#ifndef QT_NO_DEBUG_STREAM
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QBakedShader &);
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug dbg, const QBakedShader::ShaderKey &k);
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug dbg, const QBakedShader::ShaderSourceVersion &v);
#endif

Q_DECLARE_TYPEINFO(QBakedShader::ShaderSourceVersion, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QBakedShader::ShaderKey, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QBakedShader::Shader, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
