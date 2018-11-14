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

QT_BEGIN_NAMESPACE

class QWindow;
class QRhiImplementation;
class QRhiBuffer;
class QRhiRenderBuffer;
class QRhiTexture;
class QRhiSampler;
struct QRhiResourceUpdateBatchPrivate;

// C++ object ownership rules:
//   1. new*() and create() return value owned by the caller.
//   2. next*() return value not owned by the caller.
//   3. Passing a pointer via set*() or the structs does not transfer ownership.
//   4. release() does not destroy the C++ object. releaseAndDestroy() does, and is equivalent to o->release(); delete o;
//
// Graphics resource ownership rules:
//   1. new*() does not create underlying graphics resources. build() does.
//   2. Except: new*Descriptor() implicitly "builds". (no build() for QRhiRenderPassDescriptor f.ex.)
//   3. release() schedules graphics resources for destruction. The C++ object is reusable immediately via build(), or can be destroyed.
//   4. build() on an already built object calls release() implicitly. Except swapchains. (buildOrResize - special semantics)
//   5. Ownership of resources imported via QRhi*InitParams is not taken.
//
// Other:
//   1. QRhiResourceUpdateBatch manages no graphics resources underneath. beginPass() implicitly calls release() on the batch.

struct Q_RHI_EXPORT QRhiColorClearValue
{
    QRhiColorClearValue() : rgba(0, 0, 0, 1) { }
    explicit QRhiColorClearValue(const QVector4D &rgba_) : rgba(rgba_) { }
    QRhiColorClearValue(float r, float g, float b, float a) : rgba(r, g, b, a) { }
    QVector4D rgba;
};

Q_DECLARE_TYPEINFO(QRhiColorClearValue, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiDepthStencilClearValue
{
    QRhiDepthStencilClearValue() : d(1), s(0) { }
    QRhiDepthStencilClearValue(float d_, quint32 s_) : d(d_), s(s_) { }
    float d;
    quint32 s;
};

Q_DECLARE_TYPEINFO(QRhiDepthStencilClearValue, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiViewport
{
    QRhiViewport() { }
    // x,y is bottom-left, like in OpenGL, regardless of what isYUpInFramebuffer() says
    QRhiViewport(float x, float y, float w, float h, float minDepth_ = 0.0f, float maxDepth_ = 1.0f)
        : r(x, y, w, h), minDepth(minDepth_), maxDepth(maxDepth_)
    { }
    QVector4D r;
    float minDepth;
    float maxDepth;
};

Q_DECLARE_TYPEINFO(QRhiViewport, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiScissor
{
    QRhiScissor() { }
    // x,y is bottom-left, like in OpenGL, regardless of what isYUpInFramebuffer() says
    QRhiScissor(int x, int y, int w, int h)
        : r(x, y, w, h)
    { }
    QVector4D r;
};

Q_DECLARE_TYPEINFO(QRhiScissor, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiVertexInputLayout
{
    struct Q_RHI_EXPORT Binding {
        enum Classification {
            PerVertex,
            PerInstance
        };
        Binding() { }
        Binding(quint32 stride_, Classification cls = PerVertex, int stepRate = 1)
            : stride(stride_), classification(cls), instanceStepRate(stepRate)
        { }
        quint32 stride; // must be a multiple of 4
        Classification classification;
        int instanceStepRate;
    };

    struct Q_RHI_EXPORT Attribute {
        enum Format {
            Float4,
            Float3,
            Float2,
            Float,
            UNormByte4,
            UNormByte2,
            UNormByte
        };
        Attribute() { }
        Attribute(int binding_, int location_, Format format_, quint32 offset_)
            : binding(binding_), location(location_), format(format_), offset(offset_)
        { }
        int binding;
        // With HLSL we assume the vertex shader uses TEXCOORD<location> as the
        // semantic for each input. Hence no separate semantic name and index.
        int location;
        Format format;
        quint32 offset;
    };

    QVector<Binding> bindings; // slots
    QVector<Attribute> attributes;
};

Q_DECLARE_TYPEINFO(QRhiVertexInputLayout::Binding, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiVertexInputLayout::Attribute, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiVertexInputLayout, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiGraphicsShaderStage
{
    enum Type {
        Vertex,
        Fragment,
        TessellationControl, // Hull
        TessellationEvaluation // Domain
        // yes, no geometry shaders (Metal does not have them)
    };

    QRhiGraphicsShaderStage() { }
    QRhiGraphicsShaderStage(Type type_, const QBakedShader &shader_)
        : type(type_), shader(shader_)
    { }

    Type type;
    QBakedShader shader;
};

Q_DECLARE_TYPEINFO(QRhiGraphicsShaderStage, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiShaderResourceBinding
{
    enum Type {
        UniformBuffer,
        SampledTexture
    };

    enum StageFlag {
        VertexStage = 1 << 0,
        FragmentStage = 1 << 1,
        TessellationControlStage = 1 << 2,
        TessellationEvaluationStage = 1 << 3
    };
    Q_DECLARE_FLAGS(StageFlags, StageFlag)

    static QRhiShaderResourceBinding uniformBuffer(int binding_, StageFlags stage_, QRhiBuffer *buf_);

    // Bind a region only. Up to the user to ensure offset is aligned to ubufAlignment.
    static QRhiShaderResourceBinding uniformBuffer(int binding_, StageFlags stage_, QRhiBuffer *buf_, int offset_, int size_);

    static QRhiShaderResourceBinding sampledTexture(int binding_, StageFlags stage_, QRhiTexture *tex_, QRhiSampler *sampler_);

    int binding;
    StageFlags stage;
    Type type;
    struct UniformBufferData {
        QRhiBuffer *buf;
        int offset;
        int maybeSize;
    };
    struct SampledTextureData {
        QRhiTexture *tex;
        QRhiSampler *sampler;
    };
    union {
        UniformBufferData ubuf;
        SampledTextureData stex;
    };
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiShaderResourceBinding::StageFlags)
Q_DECLARE_TYPEINFO(QRhiShaderResourceBinding, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiTextureRenderTargetDescription
{
    struct Q_RHI_EXPORT ColorAttachment {
        ColorAttachment() { }
        ColorAttachment(QRhiTexture *texture_) : texture(texture_) { }
        QRhiTexture *texture = nullptr;
        int layer = 0; // face (0..5) for cubemaps
    };

    QRhiTextureRenderTargetDescription()
    { }
    QRhiTextureRenderTargetDescription(const ColorAttachment &colorAttachment)
    { colorAttachments.append(colorAttachment); }
    QRhiTextureRenderTargetDescription(const ColorAttachment &colorAttachment, QRhiRenderBuffer *depthStencilBuffer_)
        : depthStencilBuffer(depthStencilBuffer_)
    { colorAttachments.append(colorAttachment); }
    QRhiTextureRenderTargetDescription(const ColorAttachment &colorAttachment, QRhiTexture *depthTexture_)
        : depthTexture(depthTexture_)
    { colorAttachments.append(colorAttachment); }

    QVector<ColorAttachment> colorAttachments;
    QRhiRenderBuffer *depthStencilBuffer = nullptr;
    QRhiTexture *depthTexture = nullptr;
};

Q_DECLARE_TYPEINFO(QRhiTextureRenderTargetDescription::ColorAttachment, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiTextureRenderTargetDescription, Q_MOVABLE_TYPE);

struct Q_RHI_EXPORT QRhiTextureUploadDescription
{
    struct Q_RHI_EXPORT Layer {
        struct Q_RHI_EXPORT MipLevel {
            MipLevel() { }
            MipLevel(const QImage &image_) : image(image_) { }
            QImage image;
        };
        Layer() { }
        Layer(const QVector<MipLevel> &mipImages_) : mipImages(mipImages_) { }
        QVector<MipLevel> mipImages;
    };
    QRhiTextureUploadDescription() { }
    QRhiTextureUploadDescription(const QVector<Layer> &layers_) : layers(layers_) { }
    QVector<Layer> layers; // 6 layers for cubemaps, 1 otherwise
};

Q_DECLARE_TYPEINFO(QRhiTextureUploadDescription::Layer::MipLevel, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiTextureUploadDescription::Layer, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiTextureUploadDescription, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiResource
{
public:
    virtual ~QRhiResource();
    virtual void release() = 0;
    void releaseAndDestroy();

protected:
    QRhiImplementation *rhi = nullptr;
    QRhiResource(QRhiImplementation *rhi_);
    Q_DISABLE_COPY(QRhiResource)
};

class Q_RHI_EXPORT QRhiBuffer : public QRhiResource
{
public:
    enum Type {
        Immutable, // data never changes after initial upload - under the hood typically in device local (GPU) memory
        Static,    // data changes infrequently - under the hood typically device local and updated via a separate, host visible staging buffer
        Dynamic    // data changes frequently - under the hood typically host visible
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

    // no restrictions here, up to the backend to round up if needed (that
    // won't be visible in the user-provided size reported here)
    int size() const { return m_size; }
    void setSize(int sz) { m_size = sz; }

    virtual bool build() = 0;

protected:
    QRhiBuffer(QRhiImplementation *rhi, Type type_, UsageFlags usage_, int size_);
    Type m_type;
    UsageFlags m_usage;
    int m_size;
    void *m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiBuffer::UsageFlags)

class Q_RHI_EXPORT QRhiRenderBuffer : public QRhiResource
{
public:
    enum Type {
        DepthStencil
    };

    enum Hint {
        ToBeUsedWithSwapChainOnly = 1 << 0 // use implicit winsys buffers, don't create anything (GL)
    };
    Q_DECLARE_FLAGS(Hints, Hint)

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    QSize pixelSize() const { return m_pixelSize; }
    void setPixelSize(const QSize &sz) { m_pixelSize = sz; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int s) { m_sampleCount = s; }

    Hints hints() const { return m_hints; }
    void setHints(Hints h) { m_hints = h; }

    virtual bool build() = 0;

protected:
    QRhiRenderBuffer(QRhiImplementation *rhi, Type type_, const QSize &pixelSize_,
                     int sampleCount_, Hints hints_);
    Type m_type;
    QSize m_pixelSize;
    int m_sampleCount;
    Hints m_hints;
    void *m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiRenderBuffer::Hints)

class Q_RHI_EXPORT QRhiTexture : public QRhiResource
{
public:
    enum Flag {
        RenderTarget = 1 << 0,
        ChangesFrequently = 1 << 1, // hint for backend to keep staging resources around
        CubeMap = 1 << 2,
        MipMapped = 1 << 3
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Format {
        RGBA8,
        BGRA8,
        R8,
        R16,

        D16,
        D32
    };

    Format format() const { return m_format; }
    void setFormat(Format fmt) { m_format = fmt; }

    QSize pixelSize() const { return m_pixelSize; }
    void setPixelSize(const QSize &sz) { m_pixelSize = sz; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    virtual bool build() = 0;

protected:
    QRhiTexture(QRhiImplementation *rhi, Format format_, const QSize &pixelSize_, Flags flags_);
    Format m_format;
    QSize m_pixelSize;
    Flags m_flags;
    void *m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTexture::Flags)

class Q_RHI_EXPORT QRhiSampler : public QRhiResource
{
public:
    enum Filter {
        None, // for mipmapMode only
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
    void *m_reserved;
};

class Q_RHI_EXPORT QRhiRenderPassDescriptor : public QRhiResource
{
protected:
    QRhiRenderPassDescriptor(QRhiImplementation *rhi);
    void *m_reserved;
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

    QRhiRenderPassDescriptor *renderPassDescriptor() const { return m_renderPassDesc; }
    void setRenderPassDescriptor(QRhiRenderPassDescriptor *desc) { m_renderPassDesc = desc; }

protected:
    QRhiRenderTarget(QRhiImplementation *rhi);
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
    void *m_reserved;
};

class Q_RHI_EXPORT QRhiTextureRenderTarget : public QRhiRenderTarget
{
public:
    enum Flag {
        PreserveColorContents = 1 << 0
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QRhiTextureRenderTargetDescription description() const { return m_desc; }
    void setDescription(const QRhiTextureRenderTargetDescription &desc) { m_desc = desc; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    // To be called before build() with description and flags set.
    // Textures in desc must already be built.
    // Note setRenderPassDescriptor() in the base class, that must still be called afterwards (but before build()).
    virtual QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() = 0;

    // as usual, textures in desc must be built before calling build() on the rt
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

    virtual bool build() = 0;

protected:
    QRhiShaderResourceBindings(QRhiImplementation *rhi);
    QVector<QRhiShaderResourceBinding> m_bindings;
    void *m_reserved;
};

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

    enum CullMode { // not a bitmask since some apis use a mask, some don't
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
    QVector<TargetBlend> m_targetBlends; // no blend when empty
    bool m_depthTest = false;
    bool m_depthWrite = false;
    CompareOp m_depthOp = Less;
    bool m_stencilTest = false;
    StencilOpState m_stencilFront;
    StencilOpState m_stencilBack;
    quint32 m_stencilReadMask = 0xFF; // applies to both faces
    quint32 m_stencilWriteMask = 0xFF; // applies to both faces
    int m_sampleCount = 1; // MSAA, swapchain+depthstencil must match
    QVector<QRhiGraphicsShaderStage> m_shaderStages;
    QRhiVertexInputLayout m_vertexInputLayout;
    QRhiShaderResourceBindings *m_shaderResourceBindings = nullptr; // must be built by the time ps' build() is called
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
    void *m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::ColorMask)
Q_DECLARE_TYPEINFO(QRhiGraphicsPipeline::TargetBlend, Q_MOVABLE_TYPE);

class Q_RHI_EXPORT QRhiCommandBuffer : public QRhiResource
{
protected:
    QRhiCommandBuffer(QRhiImplementation *rhi);
    void *m_reserved;
};

class Q_RHI_EXPORT QRhiSwapChain : public QRhiResource
{
public:
    enum SurfaceImportFlag {
        SurfaceHasPreMulAlpha = 1 << 0,
        SurfaceHasNonPreMulAlpha = 1 << 1
    };
    Q_DECLARE_FLAGS(SurfaceImportFlags, SurfaceImportFlag)

    QWindow *window() const { return m_window; }
    void setWindow(QWindow *window) { m_window = window; }

    QSize requestedPixelSize() const { return m_requestedPixelSize; }
    void setRequestedPixelSize(const QSize &size) { m_requestedPixelSize = size; }

    SurfaceImportFlags flags() const { return m_flags; }
    void setFlags(SurfaceImportFlags f) { m_flags = f; }

    QRhiRenderBuffer *depthStencil() const { return m_depthStencil; }
    void setDepthStencil(QRhiRenderBuffer *ds) { m_depthStencil = ds; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int samples) { m_sampleCount = samples; }

    QRhiRenderPassDescriptor *renderPassDescriptor() const { return m_renderPassDesc; }
    void setRenderPassDescriptor(QRhiRenderPassDescriptor *desc) { m_renderPassDesc = desc; }

    // Alternatively, integrate with an existing swapchain, f.ex.
    // QVulkanWindow. Other settings have no effect when this is set.
    QObject *target() const { return m_target; }
    void setTarget(QObject *obj) { m_target = obj; }

    virtual QRhiCommandBuffer *currentFrameCommandBuffer() = 0;
    virtual QRhiRenderTarget *currentFrameRenderTarget() = 0;

    // Applications are expected to use requestedPixelSize() for logic like "if
    // qwindow->size() * qwindow->dpr() != requestedPixelSize() then
    // resize_swapchain", and effectivePixelSize() for all graphics
    // calculations, like the viewport. On some platforms they may not be the
    // same, e.g. some Vulkan implementations on Windows were observed to make
    // the swapchain buffers' height off by one on high dpi screens, this is
    // then reflected in the effective size.
    virtual QSize effectivePixelSize() const = 0;

    // To be called before build() with relevant parameters like depthStencil and sampleCount set.
    // As an exception to the common rules, m_depthStencil is not required to be built yet.
    // Note setRenderPassDescriptor(), that must still be called afterwards (but before buildOrResize()).
    virtual QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() = 0;

    // As the name suggests, this is slightly different from the typical
    // build+release pattern: buildOrResize - buildOrResize is not the same as
    // buildOrResize - release - buildOrResize. A swapchain is often able to,
    // depending on the underlying APIs, accomodate changed output sizes in a
    // manner that is more efficient than a full destroy - create. So use the
    // former when a window is resized.
    virtual bool buildOrResize() = 0;

protected:
    QRhiSwapChain(QRhiImplementation *rhi);
    QWindow *m_window = nullptr;
    QSize m_requestedPixelSize;
    SurfaceImportFlags m_flags;
    QRhiRenderBuffer *m_depthStencil = nullptr;
    int m_sampleCount = 1;
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
    QObject *m_target = nullptr;
    void *m_reserved;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiSwapChain::SurfaceImportFlags)

class Q_RHI_EXPORT QRhiResourceUpdateBatch // sort of a command buffer for copy type of operations
{
public:
    ~QRhiResourceUpdateBatch();
    // Puts the batch back to the pool without any processing.
    void release();

    // None of these execute anything. Deferred to beginPass. What exactly then
    // happens underneath is hidden from the applications.
    void updateDynamicBuffer(QRhiBuffer *buf, int offset, int size, const void *data);
    void uploadStaticBuffer(QRhiBuffer *buf, const void *data);
    void uploadTexture(QRhiTexture *tex, const QRhiTextureUploadDescription &desc);
    void uploadTexture(QRhiTexture *tex, const QImage &image);

private:
    QRhiResourceUpdateBatch(QRhiImplementation *rhi);
    Q_DISABLE_COPY(QRhiResourceUpdateBatch)
    QRhiResourceUpdateBatchPrivate *d;
    friend struct QRhiResourceUpdateBatchPrivate;
    friend class QRhi;
};

struct Q_RHI_EXPORT QRhiInitParams
{
};

class Q_RHI_EXPORT QRhi
{
public:
    enum Implementation {
        Vulkan,
        OpenGLES2,
        D3D11,
        Metal
    };

    enum FrameOpResult {
        FrameOpSuccess = 0,
        FrameOpError,
        FrameOpSwapChainOutOfDate,
        FrameOpDeviceLost
    };

    enum IndexFormat {
        IndexUInt16,
        IndexUInt32
    };

    ~QRhi();

    static QRhi *create(Implementation impl, QRhiInitParams *params);

    /*
       The underlying graphics resources are created when calling build() and
       put on the release queue by release() (so this is safe even when the
       resource is used by the still executing/pending frame(s)).

       The QRhi* instance itself is not destroyed by the release and it is safe
       to destroy it right away after calling release().

       Changing any value needs explicit release and rebuilding of the
       underlying resource before it can take effect.

       res->build(); <change something>; res->release(); res->build(); ...
       is therefore perfectly valid and can be used to recreate things (when
       buffer or texture size changes f.ex.)

       In addition, just doing res->build(); ...; res->build() is valid too and
       has the same effect due to an implicit release() call made by build()
       when invoked on an object with valid resources underneath.
     */

    QRhiGraphicsPipeline *newGraphicsPipeline();
    QRhiShaderResourceBindings *newShaderResourceBindings();

    // Buffers are immutable like other resources but the underlying data can
    // change. (its size cannot) Having multiple frames in flight is handled
    // transparently, with multiple allocations, recording updates, etc.
    // internally. The underlying memory type may differ for static and dynamic
    // buffers. For best performance, static buffers may be copied to device
    // local (not necessarily host visible) memory via a staging (host visible)
    // buffer. Hence separate update-dynamic and upload-static operations.
    QRhiBuffer *newBuffer(QRhiBuffer::Type type,
                          QRhiBuffer::UsageFlags usage,
                          int size);

    // To be used for depth-stencil when no access is needed afterwards.
    // Transient image, backed by lazily allocated memory (on Vulkan at least,
    // ideal for tiled GPUs). May also be a dummy internally depending on the
    // backend and the hints (OpenGL, where the winsys interface provides the
    // depth-stencil buffer via the window surface).
    QRhiRenderBuffer *newRenderBuffer(QRhiRenderBuffer::Type type,
                                      const QSize &pixelSize,
                                      int sampleCount = 1,
                                      QRhiRenderBuffer::Hints hints = QRhiRenderBuffer::Hints());

    QRhiTexture *newTexture(QRhiTexture::Format format,
                            const QSize &pixelSize,
                            QRhiTexture::Flags flags = QRhiTexture::Flags());

    QRhiSampler *newSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                            QRhiSampler::Filter mipmapMode,
                            QRhiSampler::AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w = QRhiSampler::ClampToEdge);

    QRhiTextureRenderTarget *newTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                    QRhiTextureRenderTarget::Flags flags = QRhiTextureRenderTarget::Flags());

    /*
      Render to a QWindow (must be Vulkan/Metal/OpenGLSurface as appropriate):
        Create a swapchain.
        Call build() on the swapchain whenever the size is different than before.
        Call release() on QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed.
        Then on every frame:
           beginFrame(sc);
           updates = nextResourceUpdateBatch();
           updates->...
           beginPass(sc->currentFrameRenderTarget(), sc->currentFrameCommandBuffer(), clearValues, updates);
           ...
           endPass(sc->currentFrameCommandBuffer());
           endFrame(sc); // this queues the Present, begin/endFrame manages double buffering internally

      Also works with a QVulkanWindow from startNextFrame(). Use the overload of build() in initSwapChainResources().
     */
    QRhiSwapChain *newSwapChain();
    FrameOpResult beginFrame(QRhiSwapChain *swapChain);
    FrameOpResult endFrame(QRhiSwapChain *swapChain);

    // Returns an instance to which updates can be queued. Batch instances are
    // pooled and never owned by the application. An instance is returned to
    // the pool after a beginPass() processes it or when it is "canceled" by
    // calling release(). Can be called outside begin-endFrame as well since
    // a batch instance just collects data on its own.
    QRhiResourceUpdateBatch *nextResourceUpdateBatch();

    void beginPass(QRhiRenderTarget *rt,
                   QRhiCommandBuffer *cb,
                   const QRhiColorClearValue &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates = nullptr);
    void endPass(QRhiCommandBuffer *cb);

    // When specified, srb can be different from ps' srb but the layouts must
    // match. Basic tracking is included: no command is added to the cb when
    // the pipeline or desc.set are the same as in the last call in the same
    // frame; srb is updated automatically at this point whenever a referenced
    // buffer, texture, etc. is out of date internally (due to rebuilding since
    // the creation of the srb) - hence no need to manually recreate the srb in
    // case a QRhiBuffer is "resized" etc.
    void setGraphicsPipeline(QRhiCommandBuffer *cb,
                             QRhiGraphicsPipeline *ps,
                             QRhiShaderResourceBindings *srb = nullptr);

    // The following functions (taking a command buffer) expect to have the
    // pipeline set already on the command buffer. Otherwise, unspecified
    // issues may arise depending on the backend.
    using VertexInput = QPair<QRhiBuffer *, quint32>; // buffer, offset
    void setVertexInput(QRhiCommandBuffer *cb,
                        int startBinding, const QVector<VertexInput> &bindings,
                        QRhiBuffer *indexBuf = nullptr, quint32 indexOffset = 0,
                        IndexFormat indexFormat = IndexUInt16);

    void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport);
    void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor);
    void setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c);
    void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue);

    void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
              quint32 instanceCount = 1, quint32 firstVertex = 0, quint32 firstInstance = 0);

    // final offset (indexOffset + firstIndex * n) must be 4 byte aligned with some backends
    void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                     quint32 instanceCount = 1, quint32 firstIndex = 0,
                     qint32 vertexOffset = 0, quint32 firstInstance = 0);

    QVector<int> supportedSampleCounts() const;

    int ubufAlignment() const;
    int ubufAligned(int v) const;

    int mipLevelsForSize(const QSize &size) const;
    QSize sizeForMipLevel(int mipLevel, const QSize &baseLevelSize) const;

    bool isYUpInFramebuffer() const;

    // Make Y up and allow using 0..1 as the depth range. This lets
    // applications keep using OpenGL-targeted vertex data and perspective
    // matrices regardless of the backend. (by passing this_matrix * mvp,
    // instead of just mvp, to their vertex shaders)
    QMatrix4x4 clipSpaceCorrMatrix() const;

protected:
    QRhi();

private:
    Q_DISABLE_COPY(QRhi)
    QRhiImplementation *d = nullptr;
};

QT_END_NAMESPACE

#endif
