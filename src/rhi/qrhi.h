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

#ifndef QRHI_H
#define QRHI_H

#include <QtRhi/qtrhiglobal.h>
#include <QVector4D>
#include <QVector2D>
#include <QSize>
#include <QMatrix4x4>
#include <QVector>
#include <QImage>
#include <QtShaderTools/QBakedShader>
#include <functional>

QT_BEGIN_NAMESPACE

class QWindow;
class QRhiImplementation;
class QRhiBuffer;
class QRhiRenderBuffer;
class QRhiTexture;
class QRhiSampler;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;
class QRhiResourceUpdateBatchPrivate;
class QRhiProfiler;
class QRhiShaderResourceBindingPrivate;
class QRhiResourceSharingHostPrivate;

class Q_RHI_EXPORT QRhiColorClearValue
{
public:
    QRhiColorClearValue();
    explicit QRhiColorClearValue(const QVector4D &c);
    QRhiColorClearValue(float r, float g, float b, float a);

    QVector4D rgba() const { return m_rgba; }
    void setRgba(const QVector4D &c) { m_rgba = c; }

private:
    QVector4D m_rgba;
};

Q_DECLARE_TYPEINFO(QRhiColorClearValue, Q_MOVABLE_TYPE);

Q_RHI_EXPORT bool operator==(const QRhiColorClearValue &a, const QRhiColorClearValue &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiColorClearValue &a, const QRhiColorClearValue &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiColorClearValue &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiColorClearValue &);
#endif

class Q_RHI_EXPORT QRhiDepthStencilClearValue
{
public:
    QRhiDepthStencilClearValue();
    QRhiDepthStencilClearValue(float d, quint32 s);

    float depthClearValue() const { return m_d; }
    void setDepthClearValue(float d) { m_d = d; }

    quint32 stencilClearValue() const { return m_s; }
    void setStencilClearValue(quint32 s) { m_s = s; }

private:
    float m_d;
    quint32 m_s;
};

Q_DECLARE_TYPEINFO(QRhiDepthStencilClearValue, Q_MOVABLE_TYPE);

Q_RHI_EXPORT bool operator==(const QRhiDepthStencilClearValue &a, const QRhiDepthStencilClearValue &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiDepthStencilClearValue &a, const QRhiDepthStencilClearValue &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiDepthStencilClearValue &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiDepthStencilClearValue &);
#endif

class Q_RHI_EXPORT QRhiViewport
{
public:
    QRhiViewport();
    QRhiViewport(float x, float y, float w, float h, float minDepth = 0.0f, float maxDepth = 1.0f);

    QVector4D viewport() const { return m_rect; }
    void setViewport(const QVector4D &v) { m_rect = v; }

    float minDepth() const { return m_minDepth; }
    void setMinDepth(float minDepth) { m_minDepth = minDepth; }

    float maxDepth() const { return m_maxDepth; }
    void setMaxDepth(float maxDepth) { m_maxDepth = maxDepth; }

private:
    QVector4D m_rect;
    float m_minDepth;
    float m_maxDepth;
};

Q_DECLARE_TYPEINFO(QRhiViewport, Q_MOVABLE_TYPE);

Q_RHI_EXPORT bool operator==(const QRhiViewport &a, const QRhiViewport &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiViewport &a, const QRhiViewport &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiViewport &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiViewport &);
#endif

class Q_RHI_EXPORT QRhiScissor
{
public:
    QRhiScissor();
    QRhiScissor(int x, int y, int w, int h);

    QVector4D scissor() const { return m_rect; }
    void setScissor(const QVector4D &v) { m_rect = v; }

private:
    QVector4D m_rect;
};

Q_DECLARE_TYPEINFO(QRhiScissor, Q_MOVABLE_TYPE);

Q_RHI_EXPORT bool operator==(const QRhiScissor &a, const QRhiScissor &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiScissor &a, const QRhiScissor &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiScissor &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiScissor &);
#endif

class Q_RHI_EXPORT QRhiVertexInputBinding
{
public:
    enum Classification {
        PerVertex,
        PerInstance
    };

    QRhiVertexInputBinding();
    QRhiVertexInputBinding(quint32 stride, Classification cls = PerVertex, int stepRate = 1);

    quint32 stride() const { return m_stride; }
    void setStride(quint32 s) { m_stride = s; }

    Classification classification() const { return m_classification; }
    void setClassification(Classification c) { m_classification = c; }

    int instanceStepRate() const { return m_instanceStepRate; }
    void setInstanceStepRate(int rate) { m_instanceStepRate = rate; }

private:
    quint32 m_stride;
    Classification m_classification;
    int m_instanceStepRate;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiVertexInputBinding, Q_MOVABLE_TYPE);

Q_RHI_EXPORT bool operator==(const QRhiVertexInputBinding &a, const QRhiVertexInputBinding &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiVertexInputBinding &a, const QRhiVertexInputBinding &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiVertexInputBinding &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiVertexInputBinding &);
#endif

class Q_RHI_EXPORT QRhiVertexInputAttribute
{
public:
    enum Format {
        Float4,
        Float3,
        Float2,
        Float,
        UNormByte4,
        UNormByte2,
        UNormByte
    };

    QRhiVertexInputAttribute();
    QRhiVertexInputAttribute(int binding, int location, Format format, quint32 offset);

    int binding() const { return m_binding; }
    void setBinding(int b) { m_binding = b; }

    int location() const { return m_location; }
    void setLocation(int loc) { m_location = loc; }

    Format format() const { return m_format; }
    void setFormt(Format f) { m_format = f; }

    quint32 offset() const { return m_offset; }
    void setOffset(quint32 ofs) { m_offset = ofs; }

private:
    int m_binding;
    int m_location;
    Format m_format;
    quint32 m_offset;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiVertexInputAttribute, Q_MOVABLE_TYPE);

Q_RHI_EXPORT bool operator==(const QRhiVertexInputAttribute &a, const QRhiVertexInputAttribute &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiVertexInputAttribute &a, const QRhiVertexInputAttribute &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiVertexInputAttribute &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiVertexInputAttribute &);
#endif

class Q_RHI_EXPORT QRhiVertexInputLayout
{
public:
    QRhiVertexInputLayout();

    QVector<QRhiVertexInputBinding> bindings() const { return m_bindings; }
    void setBindings(const QVector<QRhiVertexInputBinding> &v) { m_bindings = v; }

    QVector<QRhiVertexInputAttribute> attributes() const { return m_attributes; }
    void setAttributes(const QVector<QRhiVertexInputAttribute> &v) { m_attributes = v; }

private:
    QVector<QRhiVertexInputBinding> m_bindings;
    QVector<QRhiVertexInputAttribute> m_attributes;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiVertexInputLayout, Q_MOVABLE_TYPE);

Q_RHI_EXPORT bool operator==(const QRhiVertexInputLayout &a, const QRhiVertexInputLayout &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiVertexInputLayout &a, const QRhiVertexInputLayout &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiVertexInputLayout &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiVertexInputLayout &);
#endif

class Q_RHI_EXPORT QRhiGraphicsShaderStage
{
public:
    enum Type {
        Vertex,
        Fragment,
        Geometry,
        TessellationControl,
        TessellationEvaluation
    };

    QRhiGraphicsShaderStage();
    QRhiGraphicsShaderStage(Type type, const QBakedShader &shader,
                            QBakedShaderKey::ShaderVariant v = QBakedShaderKey::StandardShader);

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    QBakedShader shader() const { return m_shader; }
    void setShader(const QBakedShader &s) { m_shader = s; }

    QBakedShaderKey::ShaderVariant shaderVariant() const { return m_shaderVariant; }
    void setShaderVariant(QBakedShaderKey::ShaderVariant v) { m_shaderVariant = v; }

private:
    Type m_type;
    QBakedShader m_shader;
    QBakedShaderKey::ShaderVariant m_shaderVariant = QBakedShaderKey::StandardShader;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiGraphicsShaderStage, Q_MOVABLE_TYPE);

Q_RHI_EXPORT bool operator==(const QRhiGraphicsShaderStage &a, const QRhiGraphicsShaderStage &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiGraphicsShaderStage &a, const QRhiGraphicsShaderStage &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiGraphicsShaderStage &s, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiGraphicsShaderStage &);
#endif

class Q_RHI_EXPORT QRhiShaderResourceBinding
{
public:
    enum Type {
        UniformBuffer,
        SampledTexture
    };

    enum StageFlag {
        VertexStage = 1 << 0,
        FragmentStage = 1 << 1,
        GeometryStage = 1 << 2,
        TessellationControlStage = 1 << 3,
        TessellationEvaluationStage = 1 << 4
    };
    Q_DECLARE_FLAGS(StageFlags, StageFlag)

    QRhiShaderResourceBinding();
    QRhiShaderResourceBinding(const QRhiShaderResourceBinding &other);
    QRhiShaderResourceBinding &operator=(const QRhiShaderResourceBinding &other);
    ~QRhiShaderResourceBinding();
    void detach();

    bool isLayoutCompatible(const QRhiShaderResourceBinding &other) const;

    static QRhiShaderResourceBinding uniformBuffer(int binding, StageFlags stage, QRhiBuffer *buf);
    static QRhiShaderResourceBinding uniformBuffer(int binding, StageFlags stage, QRhiBuffer *buf, int offset, int size);
    static QRhiShaderResourceBinding uniformBufferWithDynamicOffset(int binding, StageFlags stage, QRhiBuffer *buf, int size);
    static QRhiShaderResourceBinding sampledTexture(int binding, StageFlags stage, QRhiTexture *tex, QRhiSampler *sampler);

private:
    QRhiShaderResourceBindingPrivate *d;
    friend class QRhiShaderResourceBindingPrivate;
    friend Q_RHI_EXPORT bool operator==(const QRhiShaderResourceBinding &, const QRhiShaderResourceBinding &) Q_DECL_NOTHROW;
    friend Q_RHI_EXPORT bool operator!=(const QRhiShaderResourceBinding &, const QRhiShaderResourceBinding &) Q_DECL_NOTHROW;
    friend Q_RHI_EXPORT uint qHash(const QRhiShaderResourceBinding &, uint) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
    friend Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiShaderResourceBinding &);
#endif
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiShaderResourceBinding::StageFlags)

Q_RHI_EXPORT bool operator==(const QRhiShaderResourceBinding &a, const QRhiShaderResourceBinding &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT bool operator!=(const QRhiShaderResourceBinding &a, const QRhiShaderResourceBinding &b) Q_DECL_NOTHROW;
Q_RHI_EXPORT uint qHash(const QRhiShaderResourceBinding &b, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiShaderResourceBinding &);
#endif

class Q_RHI_EXPORT QRhiColorAttachment
{
public:
    QRhiColorAttachment();
    QRhiColorAttachment(QRhiTexture *texture);
    QRhiColorAttachment(QRhiRenderBuffer *renderBuffer);

    QRhiTexture *texture() const { return m_texture; }
    void setTexture(QRhiTexture *tex) { m_texture = tex; }

    QRhiRenderBuffer *renderBuffer() const { return m_renderBuffer; }
    void setRenderBuffer(QRhiRenderBuffer *rb) { m_renderBuffer = rb; }

    int layer() const { return m_layer; }
    void setLayer(int layer) { m_layer = layer; }

    int level() const { return m_level; }
    void setLevel(int level) { m_level = level; }

    QRhiTexture *resolveTexture() const { return m_resolveTexture; }
    void setResolveTexture(QRhiTexture *tex) { m_resolveTexture = tex; }

    int resolveLayer() const { return m_resolveLayer; }
    void setResolveLayer(int layer) { m_resolveLayer = layer; }

    int resolveLevel() const { return m_resolveLevel; }
    void setResolveLevel(int level) { m_resolveLevel = level; }

private:
    QRhiTexture *m_texture = nullptr;
    QRhiRenderBuffer *m_renderBuffer = nullptr;
    int m_layer = 0;
    int m_level = 0;
    QRhiTexture *m_resolveTexture = nullptr;
    int m_resolveLayer = 0;
    int m_resolveLevel = 0;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiColorAttachment, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiTextureRenderTargetDescription
{
public:
    QRhiTextureRenderTargetDescription();
    QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment);
    QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment, QRhiRenderBuffer *depthStencilBuffer);
    QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment, QRhiTexture *depthTexture);

    QVector<QRhiColorAttachment> colorAttachments() const { return m_colorAttachments; }
    void setColorAttachments(const QVector<QRhiColorAttachment> &att) { m_colorAttachments = att; }

    QRhiRenderBuffer *depthStencilBuffer() const { return m_depthStencilBuffer; }
    void setDepthStencilBuffer(QRhiRenderBuffer *renderBuffer) { m_depthStencilBuffer = renderBuffer; }

    QRhiTexture *depthTexture() const { return m_depthTexture; }
    void setDepthTexture(QRhiTexture *texture) { m_depthTexture = texture; }

private:
    QVector<QRhiColorAttachment> m_colorAttachments;
    QRhiRenderBuffer *m_depthStencilBuffer = nullptr;
    QRhiTexture *m_depthTexture = nullptr;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiTextureRenderTargetDescription, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiTextureMipLevel
{
public:
    QRhiTextureMipLevel();
    QRhiTextureMipLevel(const QImage &image);
    QRhiTextureMipLevel(const QByteArray &compressedData);

    QImage image() const { return m_image; }
    void setImage(const QImage &image) { m_image = image; }

    QByteArray compressedData() const { return m_compressedData; }
    void setCompressedData(const QByteArray &data) { m_compressedData = data; }

    QPoint destinationTopLeft() const { return m_destinationTopLeft; }
    void setDestinationTopLeft(const QPoint &p) { m_destinationTopLeft = p; }

    QSize sourceSize() const { return m_sourceSize; }
    void setSourceSize(const QSize &size) { m_sourceSize = size; }

    QPoint sourceTopLeft() const { return m_sourceTopLeft; }
    void setSourceTopLeft(const QPoint &p) { m_sourceTopLeft = p; }

private:
    QImage m_image;
    QByteArray m_compressedData;
    QPoint m_destinationTopLeft;
    QSize m_sourceSize;
    QPoint m_sourceTopLeft;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiTextureMipLevel, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiTextureLayer
{
public:
    QRhiTextureLayer();
    QRhiTextureLayer(const QVector<QRhiTextureMipLevel> &mipImages);

    QVector<QRhiTextureMipLevel> mipImages() const { return m_mipImages; }
    void setMipImages(const QVector<QRhiTextureMipLevel> &images) { m_mipImages = images; }

private:
    QVector<QRhiTextureMipLevel> m_mipImages;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiTextureLayer, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiTextureUploadDescription
{
public:
    QRhiTextureUploadDescription();
    QRhiTextureUploadDescription(const QVector<QRhiTextureLayer> &layers);

    QVector<QRhiTextureLayer> layers() const { return m_layers; }
    void setLayers(const QVector<QRhiTextureLayer> &layers) { m_layers = layers; }

private:
    QVector<QRhiTextureLayer> m_layers;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiTextureUploadDescription, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiTextureCopyDescription
{
public:
    QRhiTextureCopyDescription();

    QSize pixelSize() const { return m_pixelSize; }
    void setPixelSize(const QSize &sz) { m_pixelSize = sz; }

    int sourceLayer() const { return m_sourceLayer; }
    void setSourceLayer(int layer) { m_sourceLayer = layer; }

    int sourceLevel() const { return m_sourceLevel; }
    void setSourceLevel(int level) { m_sourceLevel = level; }

    QPoint sourceTopLeft() const { return m_sourceTopLeft; }
    void setSourceTopLeft(const QPoint &p) { m_sourceTopLeft = p; }

    int destinationLayer() const { return m_destinationLayer; }
    void setDestinationLayer(int layer) { m_destinationLayer = layer; }

    int destinationLevel() const { return m_destinationLevel; }
    void setDestinationLevel(int level) { m_destinationLevel = level; }

    QPoint destinationTopLeft() const { return m_destinationTopLeft; }
    void setDestinationTopLeft(const QPoint &p) { m_destinationTopLeft = p; }

private:
    QSize m_pixelSize;
    int m_sourceLayer = 0;
    int m_sourceLevel = 0;
    QPoint m_sourceTopLeft;
    int m_destinationLayer = 0;
    int m_destinationLevel = 0;
    QPoint m_destinationTopLeft;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiTextureCopyDescription, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiReadbackDescription
{
public:
    QRhiReadbackDescription();
    QRhiReadbackDescription(QRhiTexture *texture);

    QRhiTexture *texture() const { return m_texture; }
    void setTexture(QRhiTexture *tex) { m_texture = tex; }

    int layer() const { return m_layer; }
    void setLayer(int layer) { m_layer = layer; }

    int level() const { return m_level; }
    void setLevel(int level) { m_level = level; }

private:
    QRhiTexture *m_texture = nullptr;
    int m_layer = 0;
    int m_level = 0;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_TYPEINFO(QRhiReadbackDescription, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiNativeHandles
{
};

class Q_RHI_EXPORT QRhiResource
{
public:
    virtual ~QRhiResource();

    virtual void release() = 0;
    void releaseAndDestroy();
    void releaseAndDestroyLater();

    QByteArray name() const;
    void setName(const QByteArray &name);

    virtual bool isShareable() const;

    quint64 globalResourceId() const;

protected:
    QRhiResource(QRhiImplementation *rhi);
    Q_DISABLE_COPY(QRhiResource)
    friend class QRhiImplementation;
    QRhiImplementation *m_rhi = nullptr;
    quint64 m_id;
    QByteArray m_objectName;
    QRhiResourceSharingHostPrivate *m_orphanedWithRsh = nullptr;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

class Q_RHI_EXPORT QRhiBuffer : public QRhiResource
{
public:
    enum Type {
        Immutable,
        Static,
        Dynamic
    };

    enum UsageFlag {
        VertexBuffer = 1 << 0,
        IndexBuffer = 1 << 1,
        UniformBuffer = 1 << 2
    };
    Q_DECLARE_FLAGS(UsageFlags, UsageFlag)

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    UsageFlags usage() const { return m_usage; }
    void setUsage(UsageFlags u) { m_usage = u; }

    int size() const { return m_size; }
    void setSize(int sz) { m_size = sz; }

    virtual bool build() = 0;

protected:
    QRhiBuffer(QRhiImplementation *rhi, Type type_, UsageFlags usage_, int size_);
    Type m_type;
    UsageFlags m_usage;
    int m_size;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiBuffer::UsageFlags)

class Q_RHI_EXPORT QRhiTexture : public QRhiResource
{
public:
    enum Flag {
        RenderTarget = 1 << 0,
        ChangesFrequently = 1 << 1,
        CubeMap = 1 << 2,
        MipMapped = 1 << 3,
        sRGB = 1 << 4,
        UsedAsTransferSource = 1 << 5,
        UsedWithGenerateMips = 1 << 6
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Format {
        UnknownFormat,

        RGBA8,
        BGRA8,
        R8,
        R16,
        RED_OR_ALPHA8,

        D16,
        D32,

        BC1,
        BC2,
        BC3,
        BC4,
        BC5,
        BC6H,
        BC7,

        ETC2_RGB8,
        ETC2_RGB8A1,
        ETC2_RGBA8,

        ASTC_4x4,
        ASTC_5x4,
        ASTC_5x5,
        ASTC_6x5,
        ASTC_6x6,
        ASTC_8x5,
        ASTC_8x6,
        ASTC_8x8,
        ASTC_10x5,
        ASTC_10x6,
        ASTC_10x8,
        ASTC_10x10,
        ASTC_12x10,
        ASTC_12x12
    };

    Format format() const { return m_format; }
    void setFormat(Format fmt) { m_format = fmt; }

    QSize pixelSize() const { return m_pixelSize; }
    void setPixelSize(const QSize &sz) { m_pixelSize = sz; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int s) { m_sampleCount = s; }

    virtual bool build() = 0;
    virtual const QRhiNativeHandles *nativeHandles();
    virtual bool buildFrom(const QRhiNativeHandles *src);

protected:
    QRhiTexture(QRhiImplementation *rhi, Format format_, const QSize &pixelSize_,
                int sampleCount_, Flags flags_);
    Format m_format;
    QSize m_pixelSize;
    int m_sampleCount;
    Flags m_flags;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTexture::Flags)

class Q_RHI_EXPORT QRhiSampler : public QRhiResource
{
public:
    enum Filter {
        None,
        Nearest,
        Linear
    };

    enum AddressMode {
        Repeat,
        ClampToEdge,
        Border,
        Mirror,
        MirrorOnce
    };

    Filter magFilter() const { return m_magFilter; }
    void setMagFilter(Filter f) { m_magFilter = f; }

    Filter minFilter() const { return m_minFilter; }
    void setMinFilter(Filter f) { m_minFilter = f; }

    Filter mipmapMode() const { return m_mipmapMode; }
    void setMipmapMode(Filter f) { m_mipmapMode = f; }

    AddressMode addressU() const { return m_addressU; }
    void setAddressU(AddressMode mode) { m_addressU = mode; }

    AddressMode addressV() const { return m_addressV; }
    void setAddressV(AddressMode mode) { m_addressV = mode; }

    AddressMode addressW() const { return m_addressW; }
    void setAddressW(AddressMode mode) { m_addressW = mode; }

    virtual bool build() = 0;

protected:
    QRhiSampler(QRhiImplementation *rhi,
                Filter magFilter_, Filter minFilter_, Filter mipmapMode_,
                AddressMode u_, AddressMode v_, AddressMode w_);
    Filter m_magFilter;
    Filter m_minFilter;
    Filter m_mipmapMode;
    AddressMode m_addressU;
    AddressMode m_addressV;
    AddressMode m_addressW;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

class Q_RHI_EXPORT QRhiRenderBuffer : public QRhiResource
{
public:
    enum Type {
        DepthStencil,
        Color
    };

    enum Flag {
        UsedWithSwapChainOnly = 1 << 0
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    QSize pixelSize() const { return m_pixelSize; }
    void setPixelSize(const QSize &sz) { m_pixelSize = sz; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int s) { m_sampleCount = s; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags h) { m_flags = h; }

    virtual bool build() = 0;

    virtual QRhiTexture::Format backingFormat() const = 0;

protected:
    QRhiRenderBuffer(QRhiImplementation *rhi, Type type_, const QSize &pixelSize_,
                     int sampleCount_, Flags flags_);
    Type m_type;
    QSize m_pixelSize;
    int m_sampleCount;
    Flags m_flags;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiRenderBuffer::Flags)

class Q_RHI_EXPORT QRhiRenderPassDescriptor : public QRhiResource
{
protected:
    QRhiRenderPassDescriptor(QRhiImplementation *rhi);
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

class Q_RHI_EXPORT QRhiRenderTarget : public QRhiResource
{
public:
    enum Type {
        RtRef,
        RtTexture
    };

    virtual Type type() const = 0;
    virtual QSize sizeInPixels() const = 0;
    virtual float devicePixelRatio() const = 0;

    QRhiRenderPassDescriptor *renderPassDescriptor() const { return m_renderPassDesc; }
    void setRenderPassDescriptor(QRhiRenderPassDescriptor *desc) { m_renderPassDesc = desc; }

protected:
    QRhiRenderTarget(QRhiImplementation *rhi);
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

class Q_RHI_EXPORT QRhiTextureRenderTarget : public QRhiRenderTarget
{
public:
    enum Flag {
        PreserveColorContents = 1 << 0,
        PreserveDepthStencilContents = 1 << 1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QRhiTextureRenderTargetDescription description() const { return m_desc; }
    void setDescription(const QRhiTextureRenderTargetDescription &desc) { m_desc = desc; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    virtual QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() = 0;

    virtual bool build() = 0;

protected:
    QRhiTextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc_, Flags flags_);
    QRhiTextureRenderTargetDescription m_desc;
    Flags m_flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTextureRenderTarget::Flags)

class Q_RHI_EXPORT QRhiShaderResourceBindings : public QRhiResource
{
public:
    QVector<QRhiShaderResourceBinding> bindings() const { return m_bindings; }
    void setBindings(const QVector<QRhiShaderResourceBinding> &b) { m_bindings = b; }

    bool isLayoutCompatible(const QRhiShaderResourceBindings *other) const;

    virtual bool build() = 0;

protected:
    QRhiShaderResourceBindings(QRhiImplementation *rhi);
    QVector<QRhiShaderResourceBinding> m_bindings;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
#ifndef QT_NO_DEBUG_STREAM
    friend Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiShaderResourceBindings &);
#endif
};

#ifndef QT_NO_DEBUG_STREAM
Q_RHI_EXPORT QDebug operator<<(QDebug, const QRhiShaderResourceBindings &);
#endif

class Q_RHI_EXPORT QRhiGraphicsPipeline : public QRhiResource
{
public:
    enum Flag {
        UsesBlendConstants = 1 << 0,
        UsesStencilRef = 1 << 1,
        UsesScissor = 1 << 2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Topology {
        Triangles,
        TriangleStrip,
        Lines,
        LineStrip,
        Points
    };

    enum CullMode {
        None,
        Front,
        Back
    };

    enum FrontFace {
        CCW,
        CW
    };

    enum ColorMaskComponent {
        R = 1 << 0,
        G = 1 << 1,
        B = 1 << 2,
        A = 1 << 3
    };
    Q_DECLARE_FLAGS(ColorMask, ColorMaskComponent)

    enum BlendFactor {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        SrcAlphaSaturate,
        Src1Color,
        OneMinusSrc1Color,
        Src1Alpha,
        OneMinusSrc1Alpha
    };

    enum BlendOp {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    struct TargetBlend {
        ColorMask colorWrite = ColorMask(0xF); // R | G | B | A
        bool enable = false;
        BlendFactor srcColor = One;
        BlendFactor dstColor = OneMinusSrcAlpha;
        BlendOp opColor = Add;
        BlendFactor srcAlpha = One;
        BlendFactor dstAlpha = OneMinusSrcAlpha;
        BlendOp opAlpha = Add;
    };

    enum CompareOp {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum StencilOp {
        StencilZero,
        Keep,
        Replace,
        IncrementAndClamp,
        DecrementAndClamp,
        Invert,
        IncrementAndWrap,
        DecrementAndWrap
    };

    struct StencilOpState {
        StencilOp failOp = Keep;
        StencilOp depthFailOp = Keep;
        StencilOp passOp = Keep;
        CompareOp compareOp = Always;
    };

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    Topology topology() const { return m_topology; }
    void setTopology(Topology t) { m_topology = t; }

    CullMode cullMode() const { return m_cullMode; }
    void setCullMode(CullMode mode) { m_cullMode = mode; }

    FrontFace frontFace() const { return m_frontFace; }
    void setFrontFace(FrontFace f) { m_frontFace = f; }

    QVector<TargetBlend> targetBlends() const { return m_targetBlends; }
    void setTargetBlends(const QVector<TargetBlend> &blends) { m_targetBlends = blends; }

    bool hasDepthTest() const { return m_depthTest; }
    void setDepthTest(bool enable) { m_depthTest = enable; }

    bool hasDepthWrite() const { return m_depthWrite; }
    void setDepthWrite(bool enable) { m_depthWrite = enable; }

    CompareOp depthOp() const { return m_depthOp; }
    void setDepthOp(CompareOp op) { m_depthOp = op; }

    bool hasStencilTest() const { return m_stencilTest; }
    void setStencilTest(bool enable) { m_stencilTest = enable; }

    StencilOpState stencilFront() const { return m_stencilFront; }
    void setStencilFront(const StencilOpState &state) { m_stencilFront = state; }

    StencilOpState stencilBack() const { return m_stencilBack; }
    void setStencilBack(const StencilOpState &state) { m_stencilBack = state; }

    quint32 stencilReadMask() const { return m_stencilReadMask; }
    void setStencilReadMask(quint32 mask) { m_stencilReadMask = mask; }

    quint32 stencilWriteMask() const { return m_stencilWriteMask; }
    void setStencilWriteMask(quint32 mask) { m_stencilWriteMask = mask; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int s) { m_sampleCount = s; }

    QVector<QRhiGraphicsShaderStage> shaderStages() const { return m_shaderStages; }
    void setShaderStages(const QVector<QRhiGraphicsShaderStage> &stages) { m_shaderStages = stages; }

    QRhiVertexInputLayout vertexInputLayout() const { return m_vertexInputLayout; }
    void setVertexInputLayout(const QRhiVertexInputLayout &layout) { m_vertexInputLayout = layout; }

    QRhiShaderResourceBindings *shaderResourceBindings() const { return m_shaderResourceBindings; }
    void setShaderResourceBindings(QRhiShaderResourceBindings *srb) { m_shaderResourceBindings = srb; }

    QRhiRenderPassDescriptor *renderPassDescriptor() const { return m_renderPassDesc; }
    void setRenderPassDescriptor(QRhiRenderPassDescriptor *desc) { m_renderPassDesc = desc; }

    virtual bool build() = 0;

protected:
    QRhiGraphicsPipeline(QRhiImplementation *rhi);
    Flags m_flags;
    Topology m_topology = Triangles;
    CullMode m_cullMode = None;
    FrontFace m_frontFace = CCW;
    QVector<TargetBlend> m_targetBlends;
    bool m_depthTest = false;
    bool m_depthWrite = false;
    CompareOp m_depthOp = Less;
    bool m_stencilTest = false;
    StencilOpState m_stencilFront;
    StencilOpState m_stencilBack;
    quint32 m_stencilReadMask = 0xFF;
    quint32 m_stencilWriteMask = 0xFF;
    int m_sampleCount = 1;
    QVector<QRhiGraphicsShaderStage> m_shaderStages;
    QRhiVertexInputLayout m_vertexInputLayout;
    QRhiShaderResourceBindings *m_shaderResourceBindings = nullptr;
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::ColorMask)
Q_DECLARE_TYPEINFO(QRhiGraphicsPipeline::TargetBlend, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiSwapChain : public QRhiResource
{
public:
    enum Flag {
        SurfaceHasPreMulAlpha = 1 << 0,
        SurfaceHasNonPreMulAlpha = 1 << 1,
        sRGB = 1 << 2,
        UsedAsTransferSource = 1 << 3,
        NoVSync = 1 << 4
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QWindow *window() const { return m_window; }
    void setWindow(QWindow *window) { m_window = window; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    QRhiRenderBuffer *depthStencil() const { return m_depthStencil; }
    void setDepthStencil(QRhiRenderBuffer *ds) { m_depthStencil = ds; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int samples) { m_sampleCount = samples; }

    QRhiRenderPassDescriptor *renderPassDescriptor() const { return m_renderPassDesc; }
    void setRenderPassDescriptor(QRhiRenderPassDescriptor *desc) { m_renderPassDesc = desc; }

    QObject *target() const { return m_target; }
    void setTarget(QObject *obj) { m_target = obj; }

    QSize currentPixelSize() const { return m_currentPixelSize; }

    virtual QRhiCommandBuffer *currentFrameCommandBuffer() = 0;
    virtual QRhiRenderTarget *currentFrameRenderTarget() = 0;
    virtual QSize surfacePixelSize() = 0;
    virtual QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() = 0;
    virtual bool buildOrResize() = 0;

protected:
    QRhiSwapChain(QRhiImplementation *rhi);
    QWindow *m_window = nullptr;
    Flags m_flags;
    QRhiRenderBuffer *m_depthStencil = nullptr;
    int m_sampleCount = 1;
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
    QObject *m_target = nullptr;
    QSize m_currentPixelSize;
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiSwapChain::Flags)

class Q_RHI_EXPORT QRhiCommandBuffer : public QRhiResource
{
public:
    enum IndexFormat {
        IndexUInt16,
        IndexUInt32
    };

    void resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates);

    void beginPass(QRhiRenderTarget *rt,
                   const QRhiColorClearValue &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates = nullptr);
    void endPass(QRhiResourceUpdateBatch *resourceUpdates = nullptr);

    void setGraphicsPipeline(QRhiGraphicsPipeline *ps);
    using DynamicOffset = QPair<int, quint32>; // binding, offset
    void setShaderResources(QRhiShaderResourceBindings *srb = nullptr,
                            const QVector<DynamicOffset> &dynamicOffsets = QVector<DynamicOffset>());
    using VertexInput = QPair<QRhiBuffer *, quint32>; // buffer, offset
    void setVertexInput(int startBinding, const QVector<VertexInput> &bindings,
                        QRhiBuffer *indexBuf = nullptr, quint32 indexOffset = 0,
                        IndexFormat indexFormat = IndexUInt16);

    void setViewport(const QRhiViewport &viewport);
    void setScissor(const QRhiScissor &scissor);
    void setBlendConstants(const QVector4D &c);
    void setStencilRef(quint32 refValue);

    void draw(quint32 vertexCount,
              quint32 instanceCount = 1,
              quint32 firstVertex = 0,
              quint32 firstInstance = 0);

    void drawIndexed(quint32 indexCount,
                     quint32 instanceCount = 1,
                     quint32 firstIndex = 0,
                     qint32 vertexOffset = 0,
                     quint32 firstInstance = 0);

    void debugMarkBegin(const QByteArray &name);
    void debugMarkEnd();
    void debugMarkMsg(const QByteArray &msg);

protected:
    QRhiCommandBuffer(QRhiImplementation *rhi);
    Q_DECL_UNUSED_MEMBER quint64 m_reserved;
};

struct Q_RHI_EXPORT QRhiReadbackResult
{
    std::function<void()> completed = nullptr;
    QRhiTexture::Format format;
    QSize pixelSize;
    QByteArray data;
}; // non-movable due to the std::function

class Q_RHI_EXPORT QRhiResourceUpdateBatch
{
public:
    ~QRhiResourceUpdateBatch();

    void release();

    void merge(QRhiResourceUpdateBatch *other);

    void updateDynamicBuffer(QRhiBuffer *buf, int offset, int size, const void *data);
    void uploadStaticBuffer(QRhiBuffer *buf, int offset, int size, const void *data);
    void uploadStaticBuffer(QRhiBuffer *buf, const void *data);
    void uploadTexture(QRhiTexture *tex, const QRhiTextureUploadDescription &desc);
    void uploadTexture(QRhiTexture *tex, const QImage &image);
    void copyTexture(QRhiTexture *dst, QRhiTexture *src, const QRhiTextureCopyDescription &desc = QRhiTextureCopyDescription());
    void readBackTexture(const QRhiReadbackDescription &rb, QRhiReadbackResult *result);
    void generateMips(QRhiTexture *tex);

private:
    QRhiResourceUpdateBatch(QRhiImplementation *rhi);
    Q_DISABLE_COPY(QRhiResourceUpdateBatch)
    QRhiResourceUpdateBatchPrivate *d;
    friend class QRhiResourceUpdateBatchPrivate;
    friend class QRhi;
};

class Q_RHI_EXPORT QRhiResourceSharingHost
{
public:
    QRhiResourceSharingHost();
    ~QRhiResourceSharingHost();

private:
    Q_DISABLE_COPY(QRhiResourceSharingHost)
    QRhiResourceSharingHostPrivate *d;
    friend class QRhiResourceSharingHostPrivate;
};

struct Q_RHI_EXPORT QRhiInitParams
{
    QRhiResourceSharingHost *resourceSharingHost = nullptr;
};

class Q_RHI_EXPORT QRhi
{
public:
    enum Implementation {
        Null,
        Vulkan,
        OpenGLES2,
        D3D11,
        Metal
    };

    enum Flag {
        EnableProfiling = 1 << 0,
        EnableDebugMarkers = 1 << 1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum FrameOpResult {
        FrameOpSuccess = 0,
        FrameOpError,
        FrameOpSwapChainOutOfDate,
        FrameOpDeviceLost
    };

    enum Feature {
        MultisampleTexture = 1,
        MultisampleRenderBuffer,
        DebugMarkers,
        Timestamps,
        Instancing,
        CustomInstanceStepRate,
        PrimitiveRestart,
        GeometryShaders,
        TessellationShaders,
        NonDynamicUniformBuffers,
        NonFourAlignedEffectiveIndexBufferOffset,
        NPOTTextureRepeat,
        RedOrAlpha8IsRed
    };

    enum BeginFrameFlag {
    };
    Q_DECLARE_FLAGS(BeginFrameFlags, BeginFrameFlag)

    enum EndFrameFlag {
        SkipPresent = 1 << 0
    };
    Q_DECLARE_FLAGS(EndFrameFlags, EndFrameFlag)

    enum ResourceSizeLimit {
        TextureSizeMin = 1,
        TextureSizeMax
    };

    ~QRhi();

    static QRhi *create(Implementation impl,
                        QRhiInitParams *params,
                        Flags flags = Flags(),
                        QRhiNativeHandles *importDevice = nullptr);

    Implementation backend() const;

    QRhiGraphicsPipeline *newGraphicsPipeline();
    QRhiShaderResourceBindings *newShaderResourceBindings();

    QRhiBuffer *newBuffer(QRhiBuffer::Type type,
                          QRhiBuffer::UsageFlags usage,
                          int size);

    QRhiRenderBuffer *newRenderBuffer(QRhiRenderBuffer::Type type,
                                      const QSize &pixelSize,
                                      int sampleCount = 1,
                                      QRhiRenderBuffer::Flags flags = QRhiRenderBuffer::Flags());

    QRhiTexture *newTexture(QRhiTexture::Format format,
                            const QSize &pixelSize,
                            int sampleCount = 1,
                            QRhiTexture::Flags flags = QRhiTexture::Flags());

    QRhiSampler *newSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                            QRhiSampler::Filter mipmapMode,
                            QRhiSampler::AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w = QRhiSampler::ClampToEdge);

    QRhiTextureRenderTarget *newTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                    QRhiTextureRenderTarget::Flags flags = QRhiTextureRenderTarget::Flags());

    QRhiSwapChain *newSwapChain();
    FrameOpResult beginFrame(QRhiSwapChain *swapChain, BeginFrameFlags flags = BeginFrameFlags());
    FrameOpResult endFrame(QRhiSwapChain *swapChain, EndFrameFlags flags = EndFrameFlags());

    FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb);
    FrameOpResult endOffscreenFrame();

    QRhi::FrameOpResult finish();

    QRhiResourceUpdateBatch *nextResourceUpdateBatch();

    QVector<int> supportedSampleCounts() const;

    int ubufAlignment() const;
    int ubufAligned(int v) const;

    int mipLevelsForSize(const QSize &size) const;
    QSize sizeForMipLevel(int mipLevel, const QSize &baseLevelSize) const;

    bool isYUpInFramebuffer() const;
    bool isYUpInNDC() const;

    QMatrix4x4 clipSpaceCorrMatrix() const;

    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags = QRhiTexture::Flags()) const;
    bool isFeatureSupported(QRhi::Feature feature) const;
    int resourceSizeLimit(ResourceSizeLimit limit) const;

    const QRhiNativeHandles *nativeHandles();

    QRhiProfiler *profiler();

protected:
    QRhi();

private:
    Q_DISABLE_COPY(QRhi)
    QRhiImplementation *d = nullptr;
    QRhi::Implementation dtype;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhi::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhi::BeginFrameFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhi::EndFrameFlags)

QT_END_NAMESPACE

#endif
