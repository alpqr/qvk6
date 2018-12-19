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

#ifndef QSHADERDESCRIPTION_H
#define QSHADERDESCRIPTION_H

#include <QtShaderTools/qtshadertoolsglobal.h>
#include <QtCore/QString>
#include <QtCore/QVector>

QT_BEGIN_NAMESPACE

struct QShaderDescriptionPrivate;

class Q_SHADERTOOLS_EXPORT QShaderDescription
{
public:
    QShaderDescription();
    QShaderDescription(const QShaderDescription &other);
    QShaderDescription &operator=(const QShaderDescription &other);
    ~QShaderDescription();
    void detach();

    bool isValid() const;

    QByteArray toBinaryJson() const;
    QByteArray toJson() const;

    static QShaderDescription fromBinaryJson(const QByteArray &data);

    enum VarType {
        Unknown = 0,

        // do not reorder
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat2,
        Mat2x3,
        Mat2x4,
        Mat3,
        Mat3x2,
        Mat3x4,
        Mat4,
        Mat4x2,
        Mat4x3,

        Int,
        Int2,
        Int3,
        Int4,

        Uint,
        Uint2,
        Uint3,
        Uint4,

        Bool,
        Bool2,
        Bool3,
        Bool4,

        Double,
        Double2,
        Double3,
        Double4,
        DMat2,
        DMat2x3,
        DMat2x4,
        DMat3,
        DMat3x2,
        DMat3x4,
        DMat4,
        DMat4x2,
        DMat4x3,

        Sampler1D,
        Sampler2D,
        Sampler2DMS,
        Sampler3D,
        SamplerCube,
        Sampler1DArray,
        Sampler2DArray,
        Sampler2DMSArray,
        Sampler3DArray,
        SamplerCubeArray,

        Struct
    };

    // Optional data (like decorations) usually default to an otherwise invalid value (-1 or 0). This is intentional.

    struct InOutVariable {
        QString name;
        VarType type = Unknown;
        int location = -1;
        int binding = -1;
        int descriptorSet = -1;
    };

    struct BlockVariable {
        QString name;
        VarType type = Unknown;
        int offset = 0;
        int size = 0;
        QVector<int> arrayDims;
        int arrayStride = 0;
        int matrixStride = 0;
        bool matrixIsRowMajor = false;
        QVector<BlockVariable> structMembers;
    };

    struct UniformBlock {
        QString blockName;
        QString structName;
        int size = 0;
        int binding = -1;
        int descriptorSet = -1;
        QVector<BlockVariable> members;
    };

    struct PushConstantBlock {
        QString name;
        int size = 0;
        QVector<BlockVariable> members;
    };

    QVector<InOutVariable> inputVariables() const;
    QVector<InOutVariable> outputVariables() const;
    QVector<UniformBlock> uniformBlocks() const;
    QVector<PushConstantBlock> pushConstantBlocks() const;
    QVector<InOutVariable> combinedImageSamplers() const;

private:
    QShaderDescriptionPrivate *d;
    friend struct QShaderDescriptionPrivate;
#ifndef QT_NO_DEBUG_STREAM
    friend Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QShaderDescription &);
#endif
};

#ifndef QT_NO_DEBUG_STREAM
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QShaderDescription &);
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QShaderDescription::InOutVariable &);
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QShaderDescription::BlockVariable &);
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QShaderDescription::UniformBlock &);
Q_SHADERTOOLS_EXPORT QDebug operator<<(QDebug, const QShaderDescription::PushConstantBlock &);
#endif

QT_END_NAMESPACE

#endif
