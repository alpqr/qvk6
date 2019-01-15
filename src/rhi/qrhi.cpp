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

#include "qrhi_p.h"
#include <qmath.h>

#include "qrhinull_p.h"
#ifndef QT_NO_OPENGL
#include "qrhigles2_p.h"
#endif
#if QT_CONFIG(vulkan)
#include "qrhivulkan_p.h"
#endif
#ifdef Q_OS_WIN
#include "qrhid3d11_p.h"
#endif
#ifdef Q_OS_DARWIN
#include "qrhimetal_p.h"
#endif

QT_BEGIN_NAMESPACE

/*!
    \class QRhi
    \inmodule QtRhi

    \brief Accelerated 2D/3D graphics API abstraction.

     QRhi is an abstraction for hardware accelerated graphics APIs, such as,
     \l{https://www.khronos.org/opengl/}{OpenGL},
     \l{https://www.khronos.org/opengles/}{OpenGL ES},
     \l{https://docs.microsoft.com/en-us/windows/desktop/direct3d}{Direct3D},
     \l{https://developer.apple.com/metal/}{Metal}, and
     \l{https://www.khronos.org/vulkan/}{Vulkan}.

    Each QRhi instance is backed by a backend for a specific graphics API. The
    selection of the backend is a run time choice and is up to the application
    or library that creates the QRhi instance. Some backends are available on
    multiple platforms (OpenGL, Vulkan, Null), while APIs specific to a given
    platform are only available when running on the platform in question (Metal
    on macOS/iOS/tvOS, Direct3D on Windows).

    \section2 Design Fundamentals

    A QRhi cannot be instantiated directly. Instead, use the create()
    function. Delete the QRhi instance normally to release the graphics device.

    \section3 Resources

    Instances of classes deriving from QRhiResource, such as, QRhiBuffer,
    QRhiTexture, etc., encapsulate zero, one, or more native graphics
    resources. Instances of such classes are always created via the \c new
    functions of the QRhi, such as, newBuffer(), newTexture(),
    newTextureRenderTarget(), newSwapChain().

    \list

    \li The returned value from both create() and functions like newBuffer() is
    owned by the caller.

    \li Unlike QRhi, subclasses of QRhiResource should not be destroyed
    directly via delete without calling QRhiResource::release(). The typical
    approach is to call QRhiResource::releaseAndDestroy(). This is equivalent
    to QRhiResource::release() followed by \c delete.

    \li Just creating a QRhiResource subclass never allocates or initalizes any
    native resources. That is only done when calling the \c build function of a
    subclass, for example, QRhiBuffer::build() or QRhiTexture::build().

    \li The exception is
    QRhiTextureRenderTarget::newCompatibleRenderPassDescriptor() and
    QRhiSwapChain::newCompatibleRenderPassDescriptor(). There is no \c build
    operation for these and the returned object is immediately active.

    \li The resource objects themselves are treated as immutable: once a
    resource is built, changing any parameters via the setters, such as,
    QRhiTexture::setPixelSize(), has no effect, unless the underlying native
    resource is released and \c build is called again. See more about resource
    reuse in the sections below.

    \li The underlying native resources are scheduled for releasing by calling
    QRhiResource::release(). Backends often queue release requests and defer
    executing them to an unspecified time, this is hidden from the
    applications. This way applications do not have to worry about releasing a
    native resource that may still be in use by an in flight frame.

    \endlist

    \badcode
        vbuf = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
        if (!vbuf->build()) { error }
        ...
        vbuf->releaseAndDestroy();
    \endcode

    \section3 Resource reuse

    From the user's point of view the QRhiResource is reusable immediately
    after calling QRhiResource::release(). With the exception of swapchains,
    calling \c build on an already built object does an implicit release. This
    provides a handy shortcut to reuse a QRhiResource instance with different
    parameters, with a new native graphics resource underneath.

    The importance of reusing the same object lies in the fact that some
    objects reference other objects: for example, a QRhiShaderResourceBindings
    can reference QRhiBuffer, QRhiTexture, and QRhiSampler instances. If now
    one of these buffers need to be resized or a sampler parameter needs
    changing, destroying and creating a whole new QRhiBuffer or QRhiSampler
    would invalidate all references to the old instance. By just changing the
    appropriate parameters via QRhiBuffer::setSize() or similar and then
    calling QRhiBuffer::build(), everything works as expected and there is no
    need to touch the QRhiShaderResourceBindings at all, even though there is a
    good chance that under the hood the QRhiBuffer is now backed by a whole new
    native buffer.

    \badcode
        ubuf = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256);
        ubuf->build();

        srb = m_r->newShaderResourceBindings()
        srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, ubuf)
        });
        srb->build();

        ...

        // now suddenly we need buffer with a different size
        ubuf->setSize(512);
        ubuf->build(); // same as ubuf->release(); ubuf->build();

        // that's it, srb needs no changes whatsoever
    \endcode

    \section3 Pooled objects

    There are pooled objects too, like QRhiResourceUpdateBatch. An instance is
    retrieved via a \c next function, such as, nextResourceUpdateBatch(). The
    caller does not own the returned instance in this case. The only valid way
    of operating here is calling functions on the QRhiResourceUpdateBatch and
    then passing it to QRhiCommandBuffer::beginPass() or
    QRhiCommandBuffer::endPass(). These functions take care of returning the
    batch to the pool. Alternatively, a batch can be "canceled" and returned to
    the pool without processing by calling QRhiResourceUpdateBatch::release().

    A typical pattern is thus:

    \badcode
        QRhiResourceUpdateBatch *resUpdates = rhi->nextResourceUpdateBatch();
        ...
        resUpdates->updateDynamicBuffer(ubuf, 0, 64, mvp.constData());
        if (!image.isNull()) {
            resUpdates->uploadTexture(texture, image);
            image = QImage();
        }
        ...
        QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
        cb->beginPass(swapchain->currentFrameRenderTarget(), clearCol, clearDs, resUpdates);
    \endcode

    \section3 Swapchain specifics

    QRhiSwapChain features some special semantics due to the peculiar nature of
    swapchains.

    \list

    \li It has no \c build but rather a QRhiSwapChain::buildOrResize().
    Repeatedly calling this function is \b not the same as calling
    QRhiSwapChain::release() followed by QRhiSwapChain::buildOrResize(). This
    is because swapchains often have ways to handle the case where buffers need
    to be resized in a manner that is more efficient than a brute force
    destroying and recreating from scratch.

    \li An active QRhiSwapChain must be released by calling
    QRhiSwapChain::release() whenever the targeted QWindow sends the
    QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed event. It should not be
    postponed since releasing the swapchain may become problematic when the
    native window is not around anymore (e.g. because the QPlatformWindow got
    destroyed already when getting a QWindow::close())

    \endlist

    \section3 Ownership

    The general rule is no ownership transfer. Creating a QRhi with an already
    existing graphics device does not mean the QRhi takes ownership of the
    device object. Similarly, ownership is not given away when a device or
    texture object is "exported" via QRhi::nativeHandles() or
    QRhiTexture::nativeHandles(). Most importantly, passing pointers in structs
    and via setters does not transfer ownership.

    \sa {Qt Shader Tools}
 */

/*!
    \enum QRhi::Implementation
    Describes which graphics API-specific backend gets used by a QRhi instance.

    \value Null
    \value Vulkan
    \value OpenGLES2
    \value D3D11
    \value Metal
 */

/*!
    \enum QRhi::Flag
    Describes what special features to enable.

    \value EnableProfiling Enables gathering timing (CPU, GPU) and resource
    (QRhiBuffer, QRhiTexture, etc.) information and additional metadata. See
    QRhiProfiler. Avoid enabling in production builds as it may involve a
    performance penalty.

    \value EnableDebugMarkers Enables debug marker groups. Without this frame
    debugging features like making debug groups and custom resource name
    visible in external GPU debugging tools will not be available and functions
    like QRhiCommandBuffer::debugMarkBegin() will become a no-op. Avoid
    enabling in production builds as it may involve a performance penalty.
 */

/*!
    \enum QRhi::FrameOpResult
    Describes the result of operations that can have a soft failure.

    \value FrameOpSuccess Success

    \value FrameOpError Unspecified error

    \value FrameOpSwapChainOutOfDate The swapchain is in an inconsistent state
    internally. This can be recoverable by attempting to repeat the operation
    (such as, beginFrame()) later.

    \value FrameOpDeviceLost The graphics device was lost. This can be
    recoverable by attempting to repeat the operation (such as, beginFrame())
    and releasing and reinitializing all objects backed by native graphics
    resources.
 */

/*!
    \enum QRhi::Feature
    Flag values to indicate what features are supported by the backend currently in use.

    \value MultisampleTexture Textures with sample count larger than 1 are supported.
    \value MultisampleRenderBuffer Renderbuffers with sample count larger than 1 are supported.
    \value DebugMarkers Debug marker groups (and so QRhiCommandBuffer::debugMarkBegin()) are supported.
    \value Timestamps Command buffer timestamps are supported. Relevant for QRhiProfiler::gpuFrameTimes().
    \value Instancing Instanced drawing is supported.
    \value CustomInstanceStepRate Instance step rate other than 1 is supported.
 */

/*!
    \enum QRhi::ResourceSizeLimit
    Describes the resource limit to query.

    \value TextureSizeMin Minimum texture width and height. This is typically
    1. The minimum texture size is handled gracefully, meaning attempting to
    create a texture with an empty size will instead create a texture with the
    minimum size.

    \value TextureSizeMax Maximum texture width and height. This depends on the
    graphics API and sometimes the platform or implementation as well.
    Typically the value is in the range 4096 - 16384. Attempting to create
    textures larger than this is expected to fail.
 */

/*!
    \class QRhiInitParams
    \inmodule QtRhi
    \brief Base class for backend-specific initialization parameters.
 */

/*!
    \class QRhiColorClearValue
    \inmodule QtRhi
    \brief Specifies a clear color for a color buffer.
 */

/*!
    \class QRhiDepthStencilClearValue
    \inmodule QtRhi
    \brief Specifies clear values for a depth or stencil buffer.
 */

/*!
    \class QRhiViewport
    \inmodule QtRhi
    \brief Specifies a viewport rectangle.
 */

/*!
    \class QRhiScissor
    \inmodule QtRhi
    \brief Specifies a scissor rectangle.
 */

/*!
    \class QRhiVertexInputLayout
    \inmodule QtRhi
    \brief Describes the layout of vertex inputs consumed by a vertex shader.
 */

/*!
    \class QRhiVertexInputLayout::Binding
    \inmodule QtRhi
    \brief Describes a vertex input binding.
 */

/*!
    \enum QRhiVertexInputLayout::Binding::Classification
    Describes the input data classification.

    \value PerVertex Data is per-vertex
    \value PerInstance Data is per-instance
 */

/*!
    \class QRhiVertexInputLayout::Attribute
    \inmodule QtRhi
    \brief Describes a single vertex input element.
 */

/*!
    \enum QRhiVertexInputLayout::Attribute::Format
    Specifies the type of the element data.

    \value Float4 Four component float vector
    \value Float3 Three component float vector
    \value Float2 Two component float vector
    \value Float Float
    \value UNormByte4 Four component normalized unsigned byte vector
    \value UNormByte2 Two component normalized unsigned byte vector
    \value UNormByte Normalized unsigned byte
 */

/*!
    \class QRhiGraphicsShaderStage
    \inmodule QtRhi
    \brief Specifies the type and the shader code for a shader stage in the graphics pipeline.
 */

/*!
    \enum QRhiGraphicsShaderStage::Type
    Specifies the type of the shader stage.

    \value Vertex Vertex stage
    \value Fragment Fragment (pixel) stage
    \value TessellationControl Tessellation control (hull) stage
    \value TessellationEvaluation Tessellation evaluation (domain) stage
 */

/*!
    \class QRhiShaderResourceBinding
    \inmodule QtRhi
    \brief Specifies the shader resources that are made visible to one or more shader stages.
 */

/*!
    \enum QRhiShaderResourceBinding::Type
    Specifies type of the shader resource bound to a binding point

    \value UniformBuffer Uniform buffer
    \value SampledTexture Combined image sampler
 */

/*!
    \enum QRhiShaderResourceBinding::StageFlag
    Flag values to indicate which stages the shader resource is visible in

    \value VertexStage Vertex stage
    \value FragmentStage Fragment (pixel) stage
    \value TessellationControlStage Tessellation control (hull) stage
    \value TessellationEvaluationStage Tessellation evaluation (domain) stage
 */

/*!
    \class QRhiTextureRenderTargetDescription
    \inmodule QtRhi
    \brief Describes the color and depth or depth/stencil attachments of a render target.
 */

/*!
    \class QRhiTextureRenderTargetDescription::ColorAttachment
    \inmodule QtRhi
    \brief Describes the color attachments of a render target.
 */

/*!
    \class QRhiTextureUploadDescription
    \inmodule QtRhi
    \brief Describes a texture upload operation.
 */

/*!
    \class QRhiTextureUploadDescription::Layer
    \inmodule QtRhi
    \brief Describes one layer (face for cubemaps) in a texture upload operation.
 */

/*!
    \class QRhiTextureUploadDescription::Layer::MipLevel
    \inmodule QtRhi
    \brief Describes one mip level in a layer in a texture upload operation.
 */

/*!
    \class QRhiTextureCopyDescription
    \inmodule QtRhi
    \brief Describes a texture-to-texture copy operation.
 */

/*!
    \class QRhiReadbackDescription
    \inmodule QtRhi
    \brief Describes a readback (reading back texture contents from possibly GPU-only memory) operation.
 */

/*!
    \class QRhiReadbackResult
    \inmodule QtRhi
    \brief Describes the results of a potentially asynchronous readback operation.
 */

/*!
    \class QRhiNativeHandles
    \inmodule QtRhi
    \brief Base class for classes exposing backend-specific collections of native resource objects.
 */

/*!
    \class QRhiResource
    \inmodule QtRhi
    \brief Base class for classes encapsulating native resource objects.
 */

/*!
    \class QRhiBuffer
    \inmodule QtRhi
    \brief Vertex, index, or uniform (constant) buffer resource.
 */

/*!
    \enum QRhiBuffer::Type
    Specifies type of buffer resource.

    \value Immutable Indicates that the data is not expected to change ever
    after the initial upload. Under the hood such buffer resources are
    typically placed in device local (GPU) memory (on systems where
    applicable). Uploading new data is possible, but frequent changes can be
    expensive. Upload typically happens by copying to a separate, host visible
    staging buffer from which a GPU buffer-to-buffer copy is issued into the
    actual GPU-only buffer.

    \value Static Indicates that the data is expected to change only
    infrequently. Typically placed in device local (GPU) memory, where
    applicable. On backends where host visible staging buffers are used for
    uploading, the staging buffers are kept around for this type, unlike with
    Immutable, so subsequent uploads do not suffer in performance. Frequent
    updates should be avoided.

    \value Dynamic Indicates that the data is expected to change frequently.
    Not recommended for large buffers. Typically backed by host visible memory
    in 2 copies in order to allow for changing without stalling the graphics
    pipeline. The double buffering is managed transparently to the applications
    and is not exposed in the API here in any form.
 */

/*!
    \enum QRhiBuffer::UsageFlag
    Flag values to specify how the buffer is going to be used.

    \value VertexBuffer Vertex buffer
    \value IndexBuffer Index buffer
    \value UniformBuffer Uniform (constant) buffer
 */

/*!
    \class QRhiTexture
    \inmodule QtRhi
    \brief Texture resource.
 */

/*!
    \enum QRhiTexture::Flag

    Flag values to specify how the texture is going to be used. Not honoring
    the flags set before build() and attempting to use the texture in ways that
    was not declared upfront can lead to unspecified behavior or decreased
    performance depending on the backend and the underlying graphics API.

    \value RenderTarget The texture going to be used in combination with
    QRhiTextureRenderTarget

    \value ChangesFrequently Performance hint to indicate that the texture
    contents will change frequently and so staging buffers, if any, are to be
    kept alive to avoid performance hits

    \value CubeMap The texture is a cubemap. Such textures have 6 layers, one
    for each face in the order of +X, -X, +Y, -Y, +Z, -Z. Cubemap textures
    cannot be multisample.

     \value MipMapped The texture has mipmaps. The appropriate mip count is
     calculated automatically and can also be retrieved via
     QRhi::mipLevelsForSize(). The images for the mip levels have to be
     provided in the texture uploaded or generated via
     QRhiResourceUpdateBatch::generateMips(). Multisample textures cannot have
     mipmaps.

    \value sRGB Use an sRGB format

    \value UsedAsTransferSource The texture is used as the source of a texture
    copy or readback, meaning the texture is given as the source in
    QRhiResourceUpdateBatch::copyTexture() or
    QRhiResourceUpdateBatch::readBackTexture().

     \value UsedWithGenerateMips The texture is going to be used with
     QRhiResourceUpdateBatch::generateMips().
 */

/*!
    \enum QRhiTexture::Format

    Specifies the texture format. See also QRhi::isTextureFormatSupported() and
    note that flags() can modify the format when QRhiTexture::sRGB is set.

    \value UnknownFormat Not a valid format. This cannot be passed to setFormat().
    \value RGBA8
    \value BGRA8
    \value R8
    \value R16
    \value D16
    \value D32
    \value BC1
    \value BC2
    \value BC3
    \value BC4
    \value BC5
    \value BC6H
    \value BC7
    \value ETC2_RGB8
    \value ETC2_RGB8A1
    \value ETC2_RGBA8
    \value ASTC_4x4
    \value ASTC_5x4
    \value ASTC_5x5
    \value ASTC_6x5
    \value ASTC_6x6
    \value ASTC_8x5
    \value ASTC_8x6
    \value ASTC_8x8
    \value ASTC_10x5
    \value ASTC_10x6
    \value ASTC_10x8
    \value ASTC_10x10
    \value ASTC_12x10
    \value ASTC_12x12
 */

/*!
    \class QRhiSampler
    \inmodule QtRhi
    \brief Sampler resource.
 */

/*!
    \enum QRhiSampler::Filter
    Specifies the minification, magnification, or mipmap filtering

    \value None Applicable only for mipmapMode(), indicates no mipmaps to be used
    \value Nearest
    \value Linear
 */

/*!
    \enum QRhiSampler::AddressMode
    Specifies the addressing mode

    \value Repeat
    \value ClampToEdge
    \value Border
    \value Mirror
    \value MirrorOnce
 */

/*!
    \class QRhiRenderBuffer
    \inmodule QtRhi
    \brief Renderbuffer resource.
 */

/*!
    \enum QRhiRenderBuffer::Type
    Specifies the type of the renderbuffer

    \value DepthStencil Combined depth/stencil
    \value Color Color
 */

/*!
    \enum QRhiRenderBuffer::Flag
    Flag values for flags() and setFlags()

    \value UsedWithSwapChainOnly For DepthStencil renderbuffers this indicates
    that the renderbuffer is only used in combination with a QRhiSwapChain and
    never in other ways. Relevant with some backends, while others ignore it.
    With OpenGL where a separate windowing system interface API is in use (EGL,
    GLX, etc.), the flag is important since it avoids creating any actual
    resource as there is already a windowing system provided depth/stencil
    buffer as requested by QSurfaceFormat.
 */

/*!
    \class QRhiRenderPassDescriptor
    \inmodule QtRhi
    \brief Render pass resource.
 */

/*!
    \class QRhiRenderTarget
    \inmodule QtRhi
    \brief Represents an onscreen (swapchain) or offscreen (texture) render target.
 */

/*!
    \enum QRhiRenderTarget::Type
    Specifies the type of the render target

    \value RtRef This is a reference to another resource's buffer(s). Used by
    targets returned from QRhiSwapChain::currentFrameRenderTarget().

    \value RtTexture This is a QRhiTextureRenderTarget.
 */

/*!
    \class QRhiTextureRenderTarget
    \inmodule QtRhi
    \brief Texture render target resource.
 */

/*!
    \enum QRhiTextureRenderTarget::Flag
    Flag values describing the load/store behavior for the render target

    \value PreserveColorContents Indicates that the contents of the color
    attachments is to be loaded when starting a render pass, instead of
    clearing. This is potentially more expensive, especially on mobile (tiled)
    GPUs, but allows preserving the existing contents between passes.

    \value PreserveDepthStencilContents Indicates that the contents of the
    depth texture is to be loaded when starting a render pass, instead
    clearing. Only applicable when a texture is used as the depth buffer
    (QRhiTextureRenderTargetDescription::depthTexture is set) because
    depth/stencil renderbuffers may not have any physical backing and data may
    not be written out in the first place.
 */

/*!
    \class QRhiShaderResourceBindings
    \inmodule QtRhi
    \brief Encapsulates resources for making buffer, texture, sampler resources visible to shaders.
 */

/*!
    \class QRhiGraphicsPipeline
    \inmodule QtRhi
    \brief Graphics pipeline state resource.
 */

/*!
    \enum QRhiGraphicsPipeline::Flag

    Flag values for describing the dynamic state of the pipeline. The viewport is always dynamic.

    \value UsesBlendConstants Indicates that a blend color constant will be set
    via QRhiCommandBuffer::setBlendConstants()

    \value UsesStencilRef Indicates that a stencil reference value will be set
    via QRhiCommandBuffer::setStencilRef()

    \value UsesScissor Indicates that a scissor rectangle will be set via
    QRhiCommandBuffer::setScissor()
 */

/*!
    \enum QRhiGraphicsPipeline::Topology
    Specifies the primitive topology

    \value Triangles (default)
    \value TriangleStrip
    \value Lines
    \value LineStrip
    \value Points
 */

/*!
    \enum QRhiGraphicsPipeline::CullMode
    Specifies the culling mode

    \value None No culling (default)
    \value Front Cull front faces
    \value Back Cull back faces
 */

/*!
    \enum QRhiGraphicsPipeline::FrontFace
    Specifies the front face winding order

    \value CCW Counter clockwise (default)
    \value CW Clockwise
 */

/*!
    \enum QRhiGraphicsPipeline::ColorMaskComponent
    Flag values for specifying the color write mask

    \value R
    \value G
    \value B
    \value A
 */

/*!
    \enum QRhiGraphicsPipeline::BlendFactor
    Specifies the blend factor

    \value Zero
    \value One
    \value SrcColor
    \value OneMinusSrcColor
    \value DstColor
    \value OneMinusDstColor
    \value SrcAlpha
    \value OneMinusSrcAlpha
    \value DstAlpha
    \value OneMinusDstAlpha
    \value ConstantColor
    \value OneMinusConstantColor
    \value ConstantAlpha
    \value OneMinusConstantAlpha
    \value SrcAlphaSaturate
    \value Src1Color
    \value OneMinusSrc1Color
    \value Src1Alpha
    \value OneMinusSrc1Alpha
 */

/*!
    \enum QRhiGraphicsPipeline::BlendOp
    Specifies the blend operation

    \value Add
    \value Subtract
    \value ReverseSubtract
    \value Min
    \value Max
 */

/*!
    \enum QRhiGraphicsPipeline::CompareOp
    Specifies the depth or stencil comparison function

    \value Never
    \value Less (default for depth)
    \value Equal
    \value LessOrEqual
    \value Greater
    \value NotEqual
    \value GreaterOrEqual
    \value Always (default for stencil)
 */

/*!
    \enum QRhiGraphicsPipeline::StencilOp
    Specifies the stencil operation

    \value StencilZero
    \value Keep (default)
    \value Replace
    \value IncrementAndClamp
    \value DecrementAndClamp
    \value Invert
    \value IncrementAndWrap
    \value DecrementAndWrap
 */

/*!
    \class QRhiGraphicsPipeline::TargetBlend
    \inmodule QtRhi
    \brief Describes the blend state for one color attachment.

    Defaults to color write enabled, blending disabled. The blend values are
    set up for pre-multiplied alpha (One, OneMinusSrcAlpha, One,
    OneMinusSrcAlpha) by default.

    Not having any TargetBlend specified in
    QRhiGraphicsPipeline::setTargetBlends() disables blending and is a
    convenient shortcut when having only one color attachment and no blending
    is desired. Otherwise a TargetBlend for each color attachment is expected.
 */

/*!
    \class QRhiGraphicsPipeline::StencilOpState
    \inmodule QtRhi
    \brief Describes the stencil operation state.
 */

/*!
    \class QRhiSwapChain
    \inmodule QtRhi
    \brief Swapchain resource.
 */

/*!
    \enum QRhiSwapChain::Flag
    Flag values to describe swapchain properties

    \value SurfaceHasPreMulAlpha Indicates that the target surface has
    transparency with premultiplied alpha.

    \value SurfaceHasNonPreMulAlpha Indicates the target surface has
    transparencyt with non-premultiplied alpha.

    \value sRGB Requests to pick an sRGB format.

    \value UsedAsTransferSource Indicates the the swapchain will be used as the
    source of a readback in QRhiResourceUpdateBatch::readBackTexture().

    \value NoVSync Requests disabling waiting for vertical sync, also avoiding
    throttling the rendering thread. The behavior is backend specific and
    applicable only where it is possible to control this. Some may ignore the
    request altogether. For OpenGL, use QSurfaceFormat::setSwapInterval().
 */

/*!
    \class QRhiCommandBuffer
    \inmodule QtRhi
    \brief Command buffer resource.

    Not creatable by applications at the moment. The only ways to obtain a
    valid QRhiCommandBuffer are to get it from the targeted swapchain via
    QRhiSwapChain::currentFrameCommandBuffer(), or, in case of rendering
    compeletely offscreen, initializing one via QRhi::beginOffscreenFrame().
 */

/*!
    \enum QRhiCommandBuffer::IndexFormat
    Specifies the index data type

    \value IndexUInt16 Unsigned 16-bit (quint16)
    \value IndexUInt32 Unsigned 32-bit (quint32)
 */

/*!
    \class QRhiResourceUpdateBatch
    \inmodule QtRhi
    \brief Records upload and copy type of operations.
 */

/*!
    \enum QRhiResourceUpdateBatch::TexturePrepareFlag
    \internal
 */

QRhiResource::QRhiResource(QRhiImplementation *rhi_)
    : rhi(rhi_)
{
}

QRhiResource::~QRhiResource()
{
}

void QRhiResource::releaseAndDestroy()
{
    release();
    delete this;
}

QByteArray QRhiResource::name() const
{
    return objectName;
}

void QRhiResource::setName(const QByteArray &name)
{
    objectName = name;
}

QRhiBuffer::QRhiBuffer(QRhiImplementation *rhi, Type type_, UsageFlags usage_, int size_)
    : QRhiResource(rhi),
      m_type(type_), m_usage(usage_), m_size(size_)
{
}

QRhiRenderBuffer::QRhiRenderBuffer(QRhiImplementation *rhi, Type type_, const QSize &pixelSize_,
                                   int sampleCount_, Flags flags_)
    : QRhiResource(rhi),
      m_type(type_), m_pixelSize(pixelSize_), m_sampleCount(sampleCount_), m_flags(flags_)
{
}

QRhiTexture::QRhiTexture(QRhiImplementation *rhi, Format format_, const QSize &pixelSize_,
                         int sampleCount_, Flags flags_)
    : QRhiResource(rhi),
      m_format(format_), m_pixelSize(pixelSize_), m_sampleCount(sampleCount_), m_flags(flags_)
{
}

const QRhiNativeHandles *QRhiTexture::nativeHandles()
{
    return nullptr;
}

bool QRhiTexture::buildFrom(const QRhiNativeHandles *src)
{
    Q_UNUSED(src);
    return false;
}

QRhiSampler::QRhiSampler(QRhiImplementation *rhi,
                         Filter magFilter_, Filter minFilter_, Filter mipmapMode_,
                         AddressMode u_, AddressMode v_, AddressMode w_)
    : QRhiResource(rhi),
      m_magFilter(magFilter_), m_minFilter(minFilter_), m_mipmapMode(mipmapMode_),
      m_addressU(u_), m_addressV(v_), m_addressW(w_)
{
}

QRhiRenderPassDescriptor::QRhiRenderPassDescriptor(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiRenderTarget::QRhiRenderTarget(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiReferenceRenderTarget::QRhiReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiRenderTarget(rhi)
{
}

QRhiTextureRenderTarget::QRhiTextureRenderTarget(QRhiImplementation *rhi,
                                                 const QRhiTextureRenderTargetDescription &desc_,
                                                 Flags flags_)
    : QRhiRenderTarget(rhi),
      m_desc(desc_),
      m_flags(flags_)
{
}

QRhiShaderResourceBindings::QRhiShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

/*!
    Returns a shader resource binding for the given binding number, pipeline
    stage, and buffer specified by \a binding_, \a stage_, and \a buf_.
 */
QRhiShaderResourceBinding QRhiShaderResourceBinding::uniformBuffer(
        int binding_, StageFlags stage_, QRhiBuffer *buf_)
{
    QRhiShaderResourceBinding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = UniformBuffer;
    b.ubuf.buf = buf_;
    b.ubuf.offset = 0;
    b.ubuf.maybeSize = 0; // entire buffer
    return b;
}

/*!
    Returns a shader resource binding for the given binding number, pipeline
    stage, and buffer specified by \a binding_, \a stage_, and \a buf_. This
    overload binds a region only, as specified by \a offset_ and \a size_.

    \note It is up to the user to ensure the offset is aligned to
    QRhi::ubufAlignment().
 */
QRhiShaderResourceBinding QRhiShaderResourceBinding::uniformBuffer(
        int binding_, StageFlags stage_, QRhiBuffer *buf_, int offset_, int size_)
{
    Q_ASSERT(size_ > 0);
    QRhiShaderResourceBinding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = UniformBuffer;
    b.ubuf.buf = buf_;
    b.ubuf.offset = offset_;
    b.ubuf.maybeSize = size_;
    return b;
}

/*!
    Returns a shader resource binding for the given binding number, pipeline
    stage, texture, and sampler specified by \a binding_, \a stage_, \a tex_,
    \a sampler_.
 */
QRhiShaderResourceBinding QRhiShaderResourceBinding::sampledTexture(
        int binding_, StageFlags stage_, QRhiTexture *tex_, QRhiSampler *sampler_)
{
    QRhiShaderResourceBinding b;
    b.binding = binding_;
    b.stage = stage_;
    b.type = SampledTexture;
    b.stex.tex = tex_;
    b.stex.sampler = sampler_;
    return b;
}

QRhiGraphicsPipeline::QRhiGraphicsPipeline(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiSwapChain::QRhiSwapChain(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiCommandBuffer::QRhiCommandBuffer(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiImplementation::~QRhiImplementation()
{
    qDeleteAll(resUpdPool);
}

void QRhiImplementation::sendVMemStatsToProfiler()
{
    // nothing to do in the default implementation
}

bool QRhiImplementation::isCompressedFormat(QRhiTexture::Format format) const
{
    return (format >= QRhiTexture::BC1 && format <= QRhiTexture::BC7)
            || (format >= QRhiTexture::ETC2_RGB8 && format <= QRhiTexture::ETC2_RGBA8)
            || (format >= QRhiTexture::ASTC_4x4 && format <= QRhiTexture::ASTC_12x12);
}

void QRhiImplementation::compressedFormatInfo(QRhiTexture::Format format, const QSize &size,
                                              quint32 *bpl, quint32 *byteSize,
                                              QSize *blockDim) const
{
    int xdim = 4;
    int ydim = 4;
    quint32 blockSize = 0;

    switch (format) {
    case QRhiTexture::BC1:
        blockSize = 8;
        break;
    case QRhiTexture::BC2:
        blockSize = 16;
        break;
    case QRhiTexture::BC3:
        blockSize = 16;
        break;
    case QRhiTexture::BC4:
        blockSize = 8;
        break;
    case QRhiTexture::BC5:
        blockSize = 16;
        break;
    case QRhiTexture::BC6H:
        blockSize = 16;
        break;
    case QRhiTexture::BC7:
        blockSize = 16;
        break;

    case QRhiTexture::ETC2_RGB8:
        blockSize = 8;
        break;
    case QRhiTexture::ETC2_RGB8A1:
        blockSize = 8;
        break;
    case QRhiTexture::ETC2_RGBA8:
        blockSize = 16;
        break;

    case QRhiTexture::ASTC_4x4:
        blockSize = 16;
        break;
    case QRhiTexture::ASTC_5x4:
        blockSize = 16;
        xdim = 5;
        break;
    case QRhiTexture::ASTC_5x5:
        blockSize = 16;
        xdim = ydim = 5;
        break;
    case QRhiTexture::ASTC_6x5:
        blockSize = 16;
        xdim = 6;
        ydim = 5;
        break;
    case QRhiTexture::ASTC_6x6:
        blockSize = 16;
        xdim = ydim = 6;
        break;
    case QRhiTexture::ASTC_8x5:
        blockSize = 16;
        xdim = 8;
        ydim = 5;
        break;
    case QRhiTexture::ASTC_8x6:
        blockSize = 16;
        xdim = 8;
        ydim = 6;
        break;
    case QRhiTexture::ASTC_8x8:
        blockSize = 16;
        xdim = ydim = 8;
        break;
    case QRhiTexture::ASTC_10x5:
        blockSize = 16;
        xdim = 10;
        ydim = 5;
        break;
    case QRhiTexture::ASTC_10x6:
        blockSize = 16;
        xdim = 10;
        ydim = 6;
        break;
    case QRhiTexture::ASTC_10x8:
        blockSize = 16;
        xdim = 10;
        ydim = 8;
        break;
    case QRhiTexture::ASTC_10x10:
        blockSize = 16;
        xdim = ydim = 10;
        break;
    case QRhiTexture::ASTC_12x10:
        blockSize = 16;
        xdim = 12;
        ydim = 10;
        break;
    case QRhiTexture::ASTC_12x12:
        blockSize = 16;
        xdim = ydim = 12;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    const quint32 wblocks = (size.width() + xdim - 1) / xdim;
    const quint32 hblocks = (size.height() + ydim - 1) / ydim;

    if (bpl)
        *bpl = wblocks * blockSize;
    if (byteSize)
        *byteSize = wblocks * hblocks * blockSize;
    if (blockDim)
        *blockDim = QSize(xdim, ydim);
}

void QRhiImplementation::textureFormatInfo(QRhiTexture::Format format, const QSize &size,
                                           quint32 *bpl, quint32 *byteSize) const
{
    if (isCompressedFormat(format)) {
        compressedFormatInfo(format, size, bpl, byteSize, nullptr);
        return;
    }

    quint32 bpc = 0;
    switch (format) {
    case QRhiTexture::RGBA8:
        bpc = 4;
        break;
    case QRhiTexture::BGRA8:
        bpc = 4;
        break;
    case QRhiTexture::R8:
        bpc = 1;
        break;
    case QRhiTexture::R16:
        bpc = 2;
        break;

    case QRhiTexture::D16:
        bpc = 2;
        break;
    case QRhiTexture::D32:
        bpc = 4;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    if (bpl)
        *bpl = size.width() * bpc;
    if (byteSize)
        *byteSize = size.width() * size.height() * bpc;
}

// Approximate because it excludes subresource alignment or multisampling.
quint32 QRhiImplementation::approxByteSizeForTexture(QRhiTexture::Format format, const QSize &baseSize,
                                                     int mipCount, int layerCount)
{
    quint32 approxSize = 0;
    for (int level = 0; level < mipCount; ++level) {
        quint32 byteSize = 0;
        const QSize size(qFloor(float(qMax(1, baseSize.width() >> level))),
                         qFloor(float(qMax(1, baseSize.height() >> level))));
        textureFormatInfo(format, size, nullptr, &byteSize);
        approxSize += byteSize;
    }
    approxSize *= layerCount;
    return approxSize;
}

/*!
    \internal
 */
QRhi::QRhi()
{
}

/*!
    Destructor. Destroys the backend and releases resources.
 */
QRhi::~QRhi()
{
    if (d) {
        d->destroy();
        delete d;
    }
}

/*!
    \return a new QRhi instance with a backend for the graphics API specified by \a impl.

    \a params must point to an instance of one of the backend-specific
    subclasses of QRhiInitParams, such as, QRhiVulkanInitParams,
    QRhiMetalInitParams, QRhiD3D11InitParams, QRhiGles2InitParams. See these
    classes for examples on creating a QRhi.

    \a flags is optional. It is used to enable profile and debug related
    features that are potentially expensive and should only be used during
    development.
 */
QRhi *QRhi::create(Implementation impl, QRhiInitParams *params, Flags flags)
{
    QScopedPointer<QRhi> r(new QRhi);

    switch (impl) {
    case Null:
        r->d = new QRhiNull(params);
        break;
    case Vulkan:
#if QT_CONFIG(vulkan)
        r->d = new QRhiVulkan(params);
        break;
#else
        qWarning("This build of Qt has no Vulkan support");
        break;
#endif
    case OpenGLES2:
#ifndef QT_NO_OPENGL
        r->d = new QRhiGles2(params);
        break;
#else
        qWarning("This build of Qt has no OpenGL support");
        break;
#endif
    case D3D11:
#ifdef Q_OS_WIN
        r->d = new QRhiD3D11(params);
        break;
#else
        qWarning("This platform has no Direct3D 11 support");
        break;
#endif
    case Metal:
#ifdef Q_OS_DARWIN
        r->d = new QRhiMetal(params);
        break;
#else
        qWarning("This platform has no Metal support");
        break;
#endif
    default:
        break;
    }

    if (r->d) {
        if (flags.testFlag(EnableProfiling)) {
            QRhiProfilerPrivate *profD = QRhiProfilerPrivate::get(&r->d->profiler);
            profD->rhi = r.data();
            profD->rhiD = r->d;
        }
        r->d->debugMarkers = flags.testFlag(EnableDebugMarkers);
        if (r->d->create(flags))
            return r.take();
    }

    return nullptr;
}

QRhiResourceUpdateBatch::QRhiResourceUpdateBatch(QRhiImplementation *rhi)
    : d(new QRhiResourceUpdateBatchPrivate)
{
    d->q = this;
    d->rhi = rhi;
}

QRhiResourceUpdateBatch::~QRhiResourceUpdateBatch()
{
    delete d;
}

void QRhiResourceUpdateBatch::release()
{
    d->free();
}

void QRhiResourceUpdateBatch::merge(QRhiResourceUpdateBatch *other)
{
    d->merge(other->d);
}

void QRhiResourceUpdateBatch::updateDynamicBuffer(QRhiBuffer *buf, int offset, int size, const void *data)
{
    d->dynamicBufferUpdates.append({ buf, offset, size, data });
}

void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, int offset, int size, const void *data)
{
    d->staticBufferUploads.append({ buf, offset, size, data });
}

void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, const void *data)
{
    d->staticBufferUploads.append({ buf, 0, 0, data });
}

void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QRhiTextureUploadDescription &desc)
{
    d->textureUploads.append({ tex, desc });
}

void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QImage &image)
{
    uploadTexture(tex, {{{{{ image }}}}});
}

void QRhiResourceUpdateBatch::copyTexture(QRhiTexture *dst, QRhiTexture *src, const QRhiTextureCopyDescription &desc)
{
    d->textureCopies.append({ dst, src, desc });
}

void QRhiResourceUpdateBatch::readBackTexture(const QRhiReadbackDescription &rb, QRhiReadbackResult *result)
{
    d->textureReadbacks.append({ rb, result });
}

void QRhiResourceUpdateBatch::generateMips(QRhiTexture *tex)
{
    d->textureMipGens.append(QRhiResourceUpdateBatchPrivate::TextureMipGen(tex));
}

/*!
    \internal
 */
void QRhiResourceUpdateBatch::prepareTextureForUse(QRhiTexture *tex, TexturePrepareFlags flags)
{
    d->texturePrepares.append({ tex, flags });
}

QRhiResourceUpdateBatch *QRhi::nextResourceUpdateBatch()
{
    auto nextFreeBatch = [this]() -> QRhiResourceUpdateBatch * {
        for (int i = 0, ie = d->resUpdPoolMap.count(); i != ie; ++i) {
            if (!d->resUpdPoolMap.testBit(i)) {
                d->resUpdPoolMap.setBit(i);
                QRhiResourceUpdateBatch *u = d->resUpdPool[i];
                QRhiResourceUpdateBatchPrivate::get(u)->poolIndex = i;
                return u;
            }
        }
        return nullptr;
    };

    QRhiResourceUpdateBatch *u = nextFreeBatch();
    if (!u) {
        const int oldSize = d->resUpdPool.count();
        const int newSize = oldSize + 4;
        d->resUpdPool.resize(newSize);
        d->resUpdPoolMap.resize(newSize);
        for (int i = oldSize; i < newSize; ++i)
            d->resUpdPool[i] = new QRhiResourceUpdateBatch(d);
        u = nextFreeBatch();
        Q_ASSERT(u);
    }

    return u;
}

void QRhiResourceUpdateBatchPrivate::free()
{
    Q_ASSERT(poolIndex >= 0 && rhi->resUpdPool[poolIndex] == q);

    dynamicBufferUpdates.clear();
    staticBufferUploads.clear();
    textureUploads.clear();
    textureCopies.clear();
    textureReadbacks.clear();
    textureMipGens.clear();
    texturePrepares.clear();

    rhi->resUpdPoolMap.clearBit(poolIndex);
    poolIndex = -1;
}

void QRhiResourceUpdateBatchPrivate::merge(QRhiResourceUpdateBatchPrivate *other)
{
    dynamicBufferUpdates += other->dynamicBufferUpdates;
    staticBufferUploads += other->staticBufferUploads;
    textureUploads += other->textureUploads;
    textureCopies += other->textureCopies;
    textureReadbacks += other->textureReadbacks;
    textureMipGens += other->textureMipGens;
    texturePrepares += other->texturePrepares;
}

void QRhiCommandBuffer::resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->resourceUpdate(this, resourceUpdates);
}

void QRhiCommandBuffer::beginPass(QRhiRenderTarget *rt,
                                  const QRhiColorClearValue &colorClearValue,
                                  const QRhiDepthStencilClearValue &depthStencilClearValue,
                                  QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->beginPass(this, rt, colorClearValue, depthStencilClearValue, resourceUpdates);
}

void QRhiCommandBuffer::endPass(QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->endPass(this, resourceUpdates);
}

void QRhiCommandBuffer::setGraphicsPipeline(QRhiGraphicsPipeline *ps,
                                            QRhiShaderResourceBindings *srb)
{
    rhi->setGraphicsPipeline(this, ps, srb);
}

void QRhiCommandBuffer::setVertexInput(int startBinding, const QVector<VertexInput> &bindings,
                                       QRhiBuffer *indexBuf, quint32 indexOffset,
                                       IndexFormat indexFormat)
{
    rhi->setVertexInput(this, startBinding, bindings, indexBuf, indexOffset, indexFormat);
}

void QRhiCommandBuffer::setViewport(const QRhiViewport &viewport)
{
    rhi->setViewport(this, viewport);
}

void QRhiCommandBuffer::setScissor(const QRhiScissor &scissor)
{
    rhi->setScissor(this, scissor);
}

void QRhiCommandBuffer::setBlendConstants(const QVector4D &c)
{
    rhi->setBlendConstants(this, c);
}

void QRhiCommandBuffer::setStencilRef(quint32 refValue)
{
    rhi->setStencilRef(this, refValue);
}

void QRhiCommandBuffer::draw(quint32 vertexCount,
                             quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    rhi->draw(this, vertexCount, instanceCount, firstVertex, firstInstance);
}

void QRhiCommandBuffer::drawIndexed(quint32 indexCount,
                                    quint32 instanceCount, quint32 firstIndex,
                                    qint32 vertexOffset, quint32 firstInstance)
{
    rhi->drawIndexed(this, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void QRhiCommandBuffer::debugMarkBegin(const QByteArray &name)
{
    rhi->debugMarkBegin(this, name);
}

void QRhiCommandBuffer::debugMarkEnd()
{
    rhi->debugMarkEnd(this);
}

void QRhiCommandBuffer::debugMarkMsg(const QByteArray &msg)
{
    rhi->debugMarkMsg(this, msg);
}

/*!
    \return the value (typically an offset) \a v aligned to the uniform buffer
    alignment given by by ubufAlignment().
 */
int QRhi::ubufAligned(int v) const
{
    const int byteAlign = ubufAlignment();
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

/*!
    \return the number of mip levels for a given \a size.
 */
int QRhi::mipLevelsForSize(const QSize &size) const
{
    return qCeil(std::log2(qMax(size.width(), size.height()))) + 1;
}

/*!
    \return the texture image size for a given \a mipLevel, calculated based on
    the level 0 size given in \a baseLevelSize.
 */
QSize QRhi::sizeForMipLevel(int mipLevel, const QSize &baseLevelSize) const
{
    const int w = qFloor(float(qMax(1, baseLevelSize.width() >> mipLevel)));
    const int h = qFloor(float(qMax(1, baseLevelSize.height() >> mipLevel)));
    return QSize(w, h);
}

/*!
    \return \c true if the underlying graphics API has Y up in the framebuffer.

    In practice this is \c true for OpenGL only.
 */
bool QRhi::isYUpInFramebuffer() const
{
    return d->isYUpInFramebuffer();
}

/*!
    Returns a matrix that can be used allow applications keep using
    OpenGL-targeted vertex data and projection matrices (for example, the ones
    generated by QMatrix4x4::perspective()) regardless of the backed. Once
    \c{this_matrix * mvp} is used instead of just \c mvp, vertex data with Y up
    and viewports with depth range 0 - 1 can be used without considering what
    backend and so graphics API is going to be used at run time.

    See
    \l{https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/}{this
    page} for a discussion of the topic from Vulkan perspective.
 */
QMatrix4x4 QRhi::clipSpaceCorrMatrix() const
{
    return d->clipSpaceCorrMatrix();
}

/*!
    \return \c true if the specified texture \a format modified by \a flags is
    supported.

    The query is supported both for uncompressed and compressed formats.
 */
bool QRhi::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const
{
    return d->isTextureFormatSupported(format, flags);
}

/*!
    \return \c true if the specified \a feature is supported
 */
bool QRhi::isFeatureSupported(QRhi::Feature feature) const
{
    return d->isFeatureSupported(feature);
}

/*!
    \return the value for the specified resource \a limit.

    The values are expected to be queried by the backends upon initialization,
    meaning calling this function is a light operation.
 */
int QRhi::resourceSizeLimit(ResourceSizeLimit limit) const
{
    return d->resourceSizeLimit(limit);
}

/*!
    \return a pointer to the backend-specific collection of native objects
    for the device, context, and similar concepts used by the backend.

    Cast to QRhiVulkanNativeHandles, QRhiD3D11NativeHandles,
    QRhiGles2NativeHandles, QRhiMetalNativeHandles as appropriate.

    \note No ownership is transfered, neither for the returned pointer nor for
    any native objects.
 */
const QRhiNativeHandles *QRhi::nativeHandles()
{
    return d->nativeHandles();
}

/*!
    \return the associated QRhiProfiler instance.

    An instance is always available for each QRhi, but it is not very useful
    without EnableProfiling because no data is collected without setting the
    flag upon creation.
  */
QRhiProfiler *QRhi::profiler()
{
    return &d->profiler;
}

QRhiGraphicsPipeline *QRhi::newGraphicsPipeline()
{
    return d->createGraphicsPipeline();
}

QRhiShaderResourceBindings *QRhi::newShaderResourceBindings()
{
    return d->createShaderResourceBindings();
}

QRhiBuffer *QRhi::newBuffer(QRhiBuffer::Type type,
                            QRhiBuffer::UsageFlags usage,
                            int size)
{
    return d->createBuffer(type, usage, size);
}

QRhiRenderBuffer *QRhi::newRenderBuffer(QRhiRenderBuffer::Type type,
                                        const QSize &pixelSize,
                                        int sampleCount,
                                        QRhiRenderBuffer::Flags flags)
{
    return d->createRenderBuffer(type, pixelSize, sampleCount, flags);
}

QRhiTexture *QRhi::newTexture(QRhiTexture::Format format,
                              const QSize &pixelSize,
                              int sampleCount,
                              QRhiTexture::Flags flags)
{
    return d->createTexture(format, pixelSize, sampleCount, flags);
}

QRhiSampler *QRhi::newSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                              QRhiSampler::Filter mipmapMode,
                              QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w)
{
    return d->createSampler(magFilter, minFilter, mipmapMode, u, v, w);
}

QRhiTextureRenderTarget *QRhi::newTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                      QRhiTextureRenderTarget::Flags flags)
{
    return d->createTextureRenderTarget(desc, flags);
}

QRhiSwapChain *QRhi::newSwapChain()
{
    return d->createSwapChain();
}

QRhi::FrameOpResult QRhi::beginFrame(QRhiSwapChain *swapChain)
{
    return d->beginFrame(swapChain);
}

QRhi::FrameOpResult QRhi::endFrame(QRhiSwapChain *swapChain)
{
    return d->endFrame(swapChain);
}

QRhi::FrameOpResult QRhi::beginOffscreenFrame(QRhiCommandBuffer **cb)
{
    return d->beginOffscreenFrame(cb);
}

QRhi::FrameOpResult QRhi::endOffscreenFrame()
{
    return d->endOffscreenFrame();
}

QRhi::FrameOpResult QRhi::finish()
{
    return d->finish();
}

/*!
    \return the list of supported sample counts.

    A typical example would be (1, 2, 4, 8).

    With some backend this list of supported values is fixed in advance, while
    with some others the (physical) device properties indicate what is
    supported at run time.
 */
QVector<int> QRhi::supportedSampleCounts() const
{
    return d->supportedSampleCounts();
}

/*!
    \return the minimum uniform buffer offset alignment in bytes. This is
    typically 256.

    Attempting to bind a uniform buffer region with an offset not aligned to
    this value will lead to failures depending on the backend and the
    underlying graphics API.

    \sa ubufAligned()
 */
int QRhi::ubufAlignment() const
{
    return d->ubufAlignment();
}

QT_END_NAMESPACE
