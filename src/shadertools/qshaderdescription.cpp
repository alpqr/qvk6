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

#include "qshaderdescription_p.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>

QT_BEGIN_NAMESPACE

/*!
    \class QShaderDescription
    \inmodule QtShaderTools
 */

QShaderDescription::QShaderDescription()
    : d(new QShaderDescriptionPrivate)
{
}

void QShaderDescription::detach()
{
    if (d->ref.load() != 1) {
        QShaderDescriptionPrivate *newd = new QShaderDescriptionPrivate(d);
        if (!d->ref.deref())
            delete d;
        d = newd;
    }
}

QShaderDescription::QShaderDescription(const QShaderDescription &other)
{
    d = other.d;
    d->ref.ref();
}

QShaderDescription &QShaderDescription::operator=(const QShaderDescription &other)
{
    if (d != other.d) {
        other.d->ref.ref();
        if (!d->ref.deref())
            delete d;
        d = other.d;
    }
    return *this;
}

QShaderDescription::~QShaderDescription()
{
    if (!d->ref.deref())
        delete d;
}

bool QShaderDescription::isValid() const
{
    return !d->inVars.isEmpty() || !d->outVars.isEmpty() || !d->uniformBlocks.isEmpty() || !d->pushConstantBlocks.isEmpty();
}

QByteArray QShaderDescription::toBinaryJson() const
{
    return d->makeDoc().toBinaryData();
}

QByteArray QShaderDescription::toJson() const
{
    return d->makeDoc().toJson();
}

QShaderDescription QShaderDescription::fromBinaryJson(const QByteArray &data)
{
    QShaderDescription desc;
    QShaderDescriptionPrivate::get(&desc)->loadDoc(QJsonDocument::fromBinaryData(data));
    return desc;
}

QVector<QShaderDescription::InOutVariable> QShaderDescription::inputVariables() const
{
    return d->inVars;
}

QVector<QShaderDescription::InOutVariable> QShaderDescription::outputVariables() const
{
    return d->outVars;
}

QVector<QShaderDescription::UniformBlock> QShaderDescription::uniformBlocks() const
{
    return d->uniformBlocks;
}

QVector<QShaderDescription::PushConstantBlock> QShaderDescription::pushConstantBlocks() const
{
    return d->pushConstantBlocks;
}

QVector<QShaderDescription::InOutVariable> QShaderDescription::combinedImageSamplers() const
{
    return d->combinedImageSamplers;
}

static struct TypeTab {
    QString k;
    QShaderDescription::VarType v;
} typeTab[] = {
    { QLatin1String("float"), QShaderDescription::Float },
    { QLatin1String("vec2"), QShaderDescription::Vec2 },
    { QLatin1String("vec3"), QShaderDescription::Vec3 },
    { QLatin1String("vec4"), QShaderDescription::Vec4 },
    { QLatin1String("mat2"), QShaderDescription::Mat2 },
    { QLatin1String("mat3"), QShaderDescription::Mat3 },
    { QLatin1String("mat4"), QShaderDescription::Mat4 },

    { QLatin1String("struct"), QShaderDescription::Struct },

    { QLatin1String("sampler1D"), QShaderDescription::Sampler1D },
    { QLatin1String("sampler2D"), QShaderDescription::Sampler2D },
    { QLatin1String("sampler2DMS"), QShaderDescription::Sampler2DMS },
    { QLatin1String("sampler3D"), QShaderDescription::Sampler3D },
    { QLatin1String("samplerCube"), QShaderDescription::SamplerCube },
    { QLatin1String("sampler1DArray"), QShaderDescription::Sampler1DArray },
    { QLatin1String("sampler2DArray"), QShaderDescription::Sampler2DArray },
    { QLatin1String("sampler2DMSArray"), QShaderDescription::Sampler2DMSArray },
    { QLatin1String("sampler3DArray"), QShaderDescription::Sampler3DArray },
    { QLatin1String("samplerCubeArray"), QShaderDescription::SamplerCubeArray },

    { QLatin1String("mat2x3"), QShaderDescription::Mat2x3 },
    { QLatin1String("mat2x4"), QShaderDescription::Mat2x4 },
    { QLatin1String("mat3x2"), QShaderDescription::Mat3x2 },
    { QLatin1String("mat3x4"), QShaderDescription::Mat3x4 },
    { QLatin1String("mat4x2"), QShaderDescription::Mat4x2 },
    { QLatin1String("mat4x3"), QShaderDescription::Mat4x3 },

    { QLatin1String("int"), QShaderDescription::Int },
    { QLatin1String("ivec2"), QShaderDescription::Int2 },
    { QLatin1String("ivec3"), QShaderDescription::Int3 },
    { QLatin1String("ivec4"), QShaderDescription::Int4 },

    { QLatin1String("uint"), QShaderDescription::Uint },
    { QLatin1String("uvec2"), QShaderDescription::Uint2 },
    { QLatin1String("uvec3"), QShaderDescription::Uint3 },
    { QLatin1String("uvec4"), QShaderDescription::Uint4 },

    { QLatin1String("bool"), QShaderDescription::Bool },
    { QLatin1String("bvec2"), QShaderDescription::Bool2 },
    { QLatin1String("bvec3"), QShaderDescription::Bool3 },
    { QLatin1String("bvec4"), QShaderDescription::Bool4 },

    { QLatin1String("double"), QShaderDescription::Double },
    { QLatin1String("dvec2"), QShaderDescription::Double2 },
    { QLatin1String("dvec3"), QShaderDescription::Double3 },
    { QLatin1String("dvec4"), QShaderDescription::Double4 },
    { QLatin1String("dmat2"), QShaderDescription::DMat2 },
    { QLatin1String("dmat3"), QShaderDescription::DMat3 },
    { QLatin1String("dmat4"), QShaderDescription::DMat4 },
    { QLatin1String("dmat2x3"), QShaderDescription::DMat2x3 },
    { QLatin1String("dmat2x4"), QShaderDescription::DMat2x4 },
    { QLatin1String("dmat3x2"), QShaderDescription::DMat3x2 },
    { QLatin1String("dmat3x4"), QShaderDescription::DMat3x4 },
    { QLatin1String("dmat4x2"), QShaderDescription::DMat4x2 },
    { QLatin1String("dmat4x3"), QShaderDescription::DMat4x3 },
};

static QString typeStr(const QShaderDescription::VarType &t)
{
    for (size_t i = 0; i < sizeof(typeTab) / sizeof(TypeTab); ++i) {
        if (typeTab[i].v == t)
            return typeTab[i].k;
    }
    return QString();
}

static QShaderDescription::VarType mapType(const QString &t)
{
    for (size_t i = 0; i < sizeof(typeTab) / sizeof(TypeTab); ++i) {
        if (typeTab[i].k == t)
            return typeTab[i].v;
    }
    return QShaderDescription::Unknown;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QShaderDescription &sd)
{
    const QShaderDescriptionPrivate *d = sd.d;
    QDebugStateSaver saver(dbg);

    if (sd.isValid()) {
        dbg.nospace() << "QShaderDescription("
                      << "inVars " << d->inVars
                      << " outVars " << d->outVars
                      << " uniformBlocks " << d->uniformBlocks
                      << " pcBlocks " << d->pushConstantBlocks
                      << " samplers " << d->combinedImageSamplers
                      << ')';
    } else {
        dbg.nospace() << "QShaderDescription(null)";
    }

    return dbg;
}

QDebug operator<<(QDebug dbg, const QShaderDescription::InOutVariable &var)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "InOutVariable(" << typeStr(var.type) << ' ' << var.name;
    if (var.location >= 0)
        dbg.nospace() << " location=" << var.location;
    if (var.binding >= 0)
        dbg.nospace() << " binding=" << var.binding;
    if (var.descriptorSet >= 0)
        dbg.nospace() << " set=" << var.descriptorSet;
    dbg.nospace() << ')';
    return dbg;
}

QDebug operator<<(QDebug dbg, const QShaderDescription::BlockVariable &var)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "BlockVariable(" << typeStr(var.type) << ' ' << var.name
                  << " offset=" << var.offset << " size=" << var.size;
    if (!var.arrayDims.isEmpty())
        dbg.nospace() << " array=" << var.arrayDims;
    if (var.arrayStride)
        dbg.nospace() << " arrayStride=" << var.arrayStride;
    if (var.matrixStride)
        dbg.nospace() << " matrixStride=" << var.matrixStride;
    if (var.matrixIsRowMajor)
        dbg.nospace() << " [rowmaj]";
    if (!var.structMembers.isEmpty())
        dbg.nospace() << " structMembers=" << var.structMembers;
    dbg.nospace() << ')';
    return dbg;
}

QDebug operator<<(QDebug dbg, const QShaderDescription::UniformBlock &blk)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "UniformBlock(" << blk.blockName << ' ' << blk.structName << " size=" << blk.size;
    if (blk.binding >= 0)
        dbg.nospace() << " binding=" << blk.binding;
    if (blk.descriptorSet >= 0)
        dbg.nospace() << " set=" << blk.descriptorSet;
    dbg.nospace() << ' ' << blk.members << ')';
    return dbg;
}

QDebug operator<<(QDebug dbg, const QShaderDescription::PushConstantBlock &blk)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "PushConstantBlock(" << blk.name << " size=" << blk.size << ' ' << blk.members << ')';
    return dbg;
}
#endif

static const QString nameKey = QLatin1String("name");
static const QString typeKey = QLatin1String("type");
static const QString locationKey = QLatin1String("location");
static const QString bindingKey = QLatin1String("binding");
static const QString setKey = QLatin1String("set");
static const QString offsetKey = QLatin1String("offset");
static const QString arrayDimsKey = QLatin1String("arrayDims");
static const QString arrayStrideKey = QLatin1String("arrayStride");
static const QString matrixStrideKey = QLatin1String("matrixStride");
static const QString matrixRowMajorKey = QLatin1String("matrixRowMajor");
static const QString structMembersKey = QLatin1String("structMembers");
static const QString membersKey = QLatin1String("members");
static const QString inputsKey = QLatin1String("inputs");
static const QString outputsKey = QLatin1String("outputs");
static const QString uniformBlocksKey = QLatin1String("uniformBlocks");
static const QString blockNameKey = QLatin1String("blockName");
static const QString structNameKey = QLatin1String("structName");
static const QString sizeKey = QLatin1String("size");
static const QString pushConstantBlocksKey = QLatin1String("pushConstantBlocks");
static const QString combinedImageSamplersKey = QLatin1String("combinedImageSamplers");

static void addDeco(QJsonObject *obj, const QShaderDescription::InOutVariable &v)
{
    if (v.location >= 0)
        (*obj)[locationKey] = v.location;
    if (v.binding >= 0)
        (*obj)[bindingKey] = v.binding;
    if (v.descriptorSet >= 0)
        (*obj)[setKey] = v.descriptorSet;
}

static QJsonObject inOutObject(const QShaderDescription::InOutVariable &v)
{
    QJsonObject obj;
    obj[nameKey] = v.name;
    obj[typeKey] = typeStr(v.type);
    addDeco(&obj, v);
    return obj;
}

static QJsonObject blockMemberObject(const QShaderDescription::BlockVariable &v)
{
    QJsonObject obj;
    obj[nameKey] = v.name;
    obj[typeKey] = typeStr(v.type);
    obj[offsetKey] = v.offset;
    obj[sizeKey] = v.size;
    if (!v.arrayDims.isEmpty()) {
        QJsonArray dimArr;
        for (int dim : v.arrayDims)
            dimArr.append(dim);
        obj[arrayDimsKey] = dimArr;
    }
    if (v.arrayStride)
        obj[arrayStrideKey] = v.arrayStride;
    if (v.matrixStride)
        obj[matrixStrideKey] = v.matrixStride;
    if (v.matrixIsRowMajor)
        obj[matrixRowMajorKey] = true;
    if (!v.structMembers.isEmpty()) {
        QJsonArray arr;
        for (const QShaderDescription::BlockVariable &sv : v.structMembers)
            arr.append(blockMemberObject(sv));
        obj[structMembersKey] = arr;
    }
    return obj;
}

QJsonDocument QShaderDescriptionPrivate::makeDoc()
{
    QJsonObject root;

    QJsonArray jinputs;
    for (const QShaderDescription::InOutVariable &v : qAsConst(inVars))
        jinputs.append(inOutObject(v));
    if (!jinputs.isEmpty())
        root[inputsKey] = jinputs;

    QJsonArray joutputs;
    for (const QShaderDescription::InOutVariable &v : qAsConst(outVars))
        joutputs.append(inOutObject(v));
    if (!joutputs.isEmpty())
        root[outputsKey] = joutputs;

    QJsonArray juniformBlocks;
    for (const QShaderDescription::UniformBlock &b : uniformBlocks) {
        QJsonObject juniformBlock;
        juniformBlock[blockNameKey] = b.blockName;
        juniformBlock[structNameKey] = b.structName;
        juniformBlock[sizeKey] = b.size;
        if (b.binding >= 0)
            juniformBlock[bindingKey] = b.binding;
        if (b.descriptorSet >= 0)
            juniformBlock[setKey] = b.descriptorSet;
        QJsonArray members;
        for (const QShaderDescription::BlockVariable &v : b.members)
            members.append(blockMemberObject(v));
        juniformBlock[membersKey] = members;
        juniformBlocks.append(juniformBlock);
    }
    if (!juniformBlocks.isEmpty())
        root[uniformBlocksKey] = juniformBlocks;

    QJsonArray jpushConstantBlocks;
    for (const QShaderDescription::PushConstantBlock &b : pushConstantBlocks) {
        QJsonObject jpushConstantBlock;
        jpushConstantBlock[nameKey] = b.name;
        jpushConstantBlock[sizeKey] = b.size;
        QJsonArray members;
        for (const QShaderDescription::BlockVariable &v : b.members)
            members.append(blockMemberObject(v));
        jpushConstantBlock[membersKey] = members;
        jpushConstantBlocks.append(jpushConstantBlock);
    }
    if (!jpushConstantBlocks.isEmpty())
        root[pushConstantBlocksKey] = jpushConstantBlocks;

    QJsonArray jcombinedSamplers;
    for (const QShaderDescription::InOutVariable &v : qAsConst(combinedImageSamplers)) {
        QJsonObject sampler;
        sampler[nameKey] = v.name;
        sampler[typeKey] = typeStr(v.type);
        addDeco(&sampler, v);
        jcombinedSamplers.append(sampler);
    }
    if (!jcombinedSamplers.isEmpty())
        root[combinedImageSamplersKey] = jcombinedSamplers;

    return QJsonDocument(root);
}

static QShaderDescription::InOutVariable inOutVar(const QJsonObject &obj)
{
    QShaderDescription::InOutVariable var;
    var.name = obj[nameKey].toString();
    var.type = mapType(obj[typeKey].toString());
    if (obj.contains(locationKey))
        var.location = obj[locationKey].toInt();
    if (obj.contains(bindingKey))
        var.binding = obj[bindingKey].toInt();
    if (obj.contains(setKey))
        var.descriptorSet = obj[setKey].toInt();
    return var;
}

static QShaderDescription::BlockVariable blockVar(const QJsonObject &obj)
{
    QShaderDescription::BlockVariable var;
    var.name = obj[nameKey].toString();
    var.type = mapType(obj[typeKey].toString());
    var.offset = obj[offsetKey].toInt();
    var.size = obj[sizeKey].toInt();
    if (obj.contains(arrayDimsKey)) {
        QJsonArray dimArr = obj[arrayDimsKey].toArray();
        for (int i = 0; i < dimArr.count(); ++i)
            var.arrayDims.append(dimArr.at(i).toInt());
    }
    if (obj.contains(arrayStrideKey))
        var.arrayStride = obj[arrayStrideKey].toInt();
    if (obj.contains(matrixStrideKey))
        var.matrixStride = obj[matrixStrideKey].toInt();
    if (obj.contains(matrixRowMajorKey))
        var.matrixIsRowMajor = obj[matrixRowMajorKey].toBool();
    if (obj.contains(structMembersKey)) {
        QJsonArray arr = obj[structMembersKey].toArray();
        for (int i = 0; i < arr.count(); ++i)
            var.structMembers.append(blockVar(arr.at(i).toObject()));
    }
    return var;
}

void QShaderDescriptionPrivate::loadDoc(const QJsonDocument &doc)
{
    if (doc.isNull()) {
        qWarning("QShaderDescription: JSON document is empty");
        return;
    }

    Q_ASSERT(ref.load() == 1); // must be detached

    inVars.clear();
    outVars.clear();
    uniformBlocks.clear();
    pushConstantBlocks.clear();
    combinedImageSamplers.clear();

    QJsonObject root = doc.object();

    if (root.contains(inputsKey)) {
        QJsonArray inputs = root[inputsKey].toArray();
        for (int i = 0; i < inputs.count(); ++i)
            inVars.append(inOutVar(inputs[i].toObject()));
    }

    if (root.contains(outputsKey)) {
        QJsonArray outputs = root[outputsKey].toArray();
        for (int i = 0; i < outputs.count(); ++i)
            outVars.append(inOutVar(outputs[i].toObject()));
    }

    if (root.contains(uniformBlocksKey)) {
        QJsonArray ubs = root[uniformBlocksKey].toArray();
        for (int i = 0; i < ubs.count(); ++i) {
            QJsonObject ubObj = ubs[i].toObject();
            QShaderDescription::UniformBlock ub;
            ub.blockName = ubObj[blockNameKey].toString();
            ub.structName = ubObj[structNameKey].toString();
            ub.size = ubObj[sizeKey].toInt();
            if (ubObj.contains(bindingKey))
                ub.binding = ubObj[bindingKey].toInt();
            if (ubObj.contains(setKey))
                ub.descriptorSet = ubObj[setKey].toInt();
            QJsonArray members = ubObj[membersKey].toArray();
            for (const QJsonValue &member : members)
                ub.members.append(blockVar(member.toObject()));
            uniformBlocks.append(ub);
        }
    }

    if (root.contains(pushConstantBlocksKey)) {
        QJsonArray pcs = root[pushConstantBlocksKey].toArray();
        for (int i = 0; i < pcs.count(); ++i) {
            QJsonObject pcObj = pcs[i].toObject();
            QShaderDescription::PushConstantBlock pc;
            pc.name = pcObj[nameKey].toString();
            pc.size = pcObj[sizeKey].toInt();
            QJsonArray members = pcObj[membersKey].toArray();
            for (const QJsonValue &member : members)
                pc.members.append(blockVar(member.toObject()));
            pushConstantBlocks.append(pc);
        }
    }

    if (root.contains(combinedImageSamplersKey)) {
        QJsonArray samplers = root[combinedImageSamplersKey].toArray();
        for (int i = 0; i < samplers.count(); ++i)
            combinedImageSamplers.append(inOutVar(samplers[i].toObject()));
    }
}

QT_END_NAMESPACE
