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
#include "qrhirsh_p.h"
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

    \badcode
        vbuf = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
        if (!vbuf->build()) { error }
        ...
        vbuf->releaseAndDestroy();
    \endcode

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
    native resource that may still be in use by an in-flight frame.

    \li Note that this does not mean that resources can freely be released
    within a frame (that is, in a
    \l{QRhiCommandBuffer::beginFrame()}{beginFrame()} -
    \l{QRhiCommandBuffer::endFrame()}{endFrame()} section). As a general rule,
    all referenced QRhiResource objects must stay unchanged until the frame is
    submitted by calling \l{QRhiCommandBuffer::endFrame()}{endFrame()}.

    \endlist

    \section3 Command buffers and defered command execution

    Regardless of the design and capabilities of the underlying graphics API,
    all QRhi backends implement some level of command buffers. No
    QRhiCommandBuffer function issues any native bind or draw command (such as,
    \c glDrawElements) directly. Commands are always recorded in a queue,
    either native or provided by the QRhi backend. The command buffer is
    submitted, and so execution starts only upon QRhi::endFrame() or
    QRhi::finish().

    The defered nature has consequences for some types of objects. For example,
    writing to a dynamic buffer multiple times within a frame, in case such
    buffers are backed by host-visible memory, will result in making the
    results of all writes are visible to all draw calls in the command buffer
    of the frame, regardless of when the dynamic buffer update was recorded
    relative to a draw call.

    Furthermore, instances of QRhiResource subclasses must be treated immutable
    within a frame in which they are referenced in any way. Create or rebuild
    all resources upfront, before starting to record commands for the next
    frame. Reusing a QRhiResource instance within a frame (by rebuilding it and
    then referencing it again in the same \c{beginFrame - endFrame} section)
    should be avoided as it may lead to unexpected results, depending on the
    backend. As a general rule, all referenced QRhiResource objects must stay
    unchanged until the frame is submitted by calling
    \l{QRhiCommandBuffer::endFrame()}{endFrame()}.

    Unlike APIs like OpenGL, upload and copy type of commands cannot be mixed
    with draw commands. The typical renderer will involve a sequence similar to
    the following: \c{(re)build resources} - \c{begin frame} - \c{record
    uploads and copies} - \c{start renderpass} - \c{record draw calls} - \c{end
    renderpass} - \c{end frame}. Recording copy type of operations happens via
    QRhiResourceUpdateBatch. Such operations are committed typically on
    \l{QRhiCommandBuffer::beginPass()}{beginPass()}.

    When working with legacy rendering engines designed for OpenGL, the
    migration to QRhi often involves redesigning from having a single \c render
    step (that performs copies and uploads, clears buffers, and issues draw
    calls, all mixed together) to a clearly separated, two phase \c prepare -
    \c render setup where the \c render step only starts a renderpass and
    records draw calls, while all resource creation and queuing of updates,
    uploads and copies happens beforehand, in the \c prepare step.

    \section3 Resource reuse

    From the user's point of view a QRhiResource is reusable immediately after
    calling QRhiResource::release(). With the exception of swapchains, calling
    \c build() on an already built object does an implicit \c release(). This
    provides a handy shortcut to reuse a QRhiResource instance with different
    parameters, with a new native graphics object underneath.

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

        srb = rhi->newShaderResourceBindings()
        srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, ubuf)
        });
        srb->build();

        ...

        // now in a later frame we need to grow the buffer to a larger size
        ubuf->setSize(512);
        ubuf->build(); // same as ubuf->release(); ubuf->build();

        // that's it, srb needs no changes whatsoever
    \endcode

    \section3 Pooled objects

    In addition to resources, there are pooled objects as well, such as,
    QRhiResourceUpdateBatch. An instance is retrieved via a \c next function,
    such as, nextResourceUpdateBatch(). The caller does not own the returned
    instance in this case. The only valid way of operating here is calling
    functions on the QRhiResourceUpdateBatch and then passing it to
    QRhiCommandBuffer::beginPass() or QRhiCommandBuffer::endPass(). These
    functions take care of returning the batch to the pool. Alternatively, a
    batch can be "canceled" and returned to the pool without processing by
    calling QRhiResourceUpdateBatch::release().

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

    \section3 Threading and resource sharing

    A QRhi instance can be created and used on any thread but all usage must be
    limited to that one single thread.

    When it comes to externally created native objects, such as OpenGL contexts
    passed in via QRhiGles2NativeHandles, it is up to the application to ensure
    they are not misused by other threads.

    By default any QRhiResource created from one QRhi must only be used with
    that one QRhi and must be released before the QRhi is destroyed. This rule
    is relaxed when a QRhi is created with an associated
    QRhiResourceSharingHost. From then on any QRhiResource that reports \c true
    from \l{QRhiResource::isShareable()}{isShareable()} is usable with other
    QRhi instances as well, as long as they are associated with the same
    QRhiResourceSharingHost. Resources can also outlive their creating QRhi in
    this case, as long as there is at least one QRhi left in the sharing group.

    \note Support for efficient concurrent access to shared resources from QRhi
    instances operating on different threads, especially when writing (for
    example, rendering to a texture) is involved, is limited for the time
    being. Neither GPU-GPU nor GPU-host synchronization facilities (such as,
    fences or events) are provided in the QRhi API at the moment. These may be
    introduced in the future.

    \sa {Qt Shader Tools}, QRhiResourceSharingHost
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

    \value MultisampleTexture Indicates that textures with a sample count larger
    than 1 are supported.

    \value MultisampleRenderBuffer Indicates that renderbuffers with a sample
    count larger than 1 are supported.

    \value DebugMarkers Indicates that debug marker groups (and so
    QRhiCommandBuffer::debugMarkBegin()) are supported.

    \value Timestamps Indicates that command buffer timestamps are supported.
    Relevant for QRhiProfiler::gpuFrameTimes().

    \value Instancing Indicates that instanced drawing is supported.

    \value CustomInstanceStepRate Indicates that instance step rates other than
    1 are supported.

    \value PrimitiveRestart Indicates that restarting the assembly of
    primitives when encountering an index value of 0xFFFF
    (\l{QRhiCommandBuffer::IndexUInt16}{IndexUInt16}) or 0xFFFFFFFF
    (\l{QRhiCommandBuffer::IndexUInt32}{IndexUInt32}) is enabled, for certain
    primitive topologies at least. QRhi will try to enable this with all
    backends, but in some cases it will not be supported. Dynamically
    controlling primitive restart is not possible since with some APIs
    primitive restart with a fixed index is always on. Applications must assume
    that whenever this feature is reported as supported, the above mentioned
    index values \c may be treated specially, depending on the topology. The
    only two topologies where primitive restart is guaranteed to behave
    identically across backends, as long as this feature is reported as
    supported, are \l{QRhiGraphicsPipeline::LineStrip}{LineStrip} and
    \l{QRhiGraphicsPipeline::TriangleStrip}{TriangleStrip}.

    \value GeometryShaders Indicates that geometry shaders are supported.

    \value TessellationShaders Indicates that tessellation control and
    evaluation shaders are supported.

    \value NonDynamicUniformBuffers Indicates that creating buffers with the
    usage \l{QRhiBuffer::UniformBuffer}{UniformBuffer} and the types
    \l{QRhiBuffer::Immutable}{Immutable} or \l{QRhiBuffer::Static}{Static} is
    supported. When reported as unsupported, uniform (constant) buffers must be
    created as \l{QRhiBuffer::Dynamic}{Dynamic}. (which is recommended
    regardless)

    \value NonFourAlignedEffectiveIndexBufferOffset Indicates that effective
    index buffer offsets (\c{indexOffset + firstIndex * indexComponentSize})
    that are not 4 byte aligned are supported. When not supported, attempting
    to issue a \l{QRhiCommandBuffer::drawIndexed()}{drawIndexed()} with a
    non-aligned effective offset may lead to unspecified behavior.

    \value NPOTTextureRepeat Indicates that the \l{QRhiSampler::Repeat}{Repeat}
    mode is supported for textures with a non-power-of-two size.
 */

/*!
    \enum QRhi::BeginFrameFlag
    Flag values for QRhi::beginFrame()
 */

/*!
    \enum QRhi::EndFrameFlag
    Flag values for QRhi::endFrame()

    \value SkipPresent Specifies that no present command is to be queued or no
    swapBuffers call is to be made. This way no image is presented. Generating
    multiple frames with all having this flag set is not recommended (except,
    for example, for benchmarking purposes - but keep in mind that backends may
    behave differently when it comes to waiting for command completion without
    presenting so the results are not comparable between them)
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

    Contains fields that are relevant to all backends.

    \l resourceSharingHost is set to null by default. Setting it to an actual
    QRhiResourceSharingHost instance is optional. When set, it causes the QRhi
    to share either the graphics device, or, in case of OpenGL, use a context
    that shares resources, with all other QRhi instances that have the same
    QRhiResourceSharingHost set in their QRhiInitParams. This makes the
    underlying graphics resources of QRhiResource subclasses such as
    QRhiTexture visible to all the QRhi instances that use the same
    QRhiResourceSharingHost.

    \sa QRhiResource::isShareable()
 */

/*!
    \class QRhiColorClearValue
    \inmodule QtRhi
    \brief Specifies a clear color for a color buffer.
 */

/*!
    Constructs a color clear value with \c{(0, 0, 0, 1)} (opaque black).
 */
QRhiColorClearValue::QRhiColorClearValue()
    : m_rgba(0, 0, 0, 1)
{
}

/*!
    Constructs a color clear value with the floating point color components
    (\c{0.0f - 1.0f}) specified in \a c.
  */
QRhiColorClearValue::QRhiColorClearValue(const QVector4D &c)
    : m_rgba(c)
{
}

/*!
    Constructs a color clear value with the floating point color components
    (\c{0.0f - 1.0f}) specified in \a r, \a g, \a b, and \a a.
 */
QRhiColorClearValue::QRhiColorClearValue(float r, float g, float b, float a)
    : m_rgba(r, g, b, a)
{
}

/*!
    \return \c true if the colors in the two QRhiColorClearValue objects \a a
    and \a b are equal.

    \relates QRhiColorClearValue
 */
bool operator==(const QRhiColorClearValue &a, const QRhiColorClearValue &b) Q_DECL_NOTHROW
{
    return a.rgba() == b.rgba();
}

/*!
    \return \c false if the colors in the two QRhiColorClearValue objects \a a
    and \a b are equal; otherwise returns \c true.

    \relates QRhiColorClearValue
*/
bool operator!=(const QRhiColorClearValue &a, const QRhiColorClearValue &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a v, using \a seed to seed the calculation.

    \relates QRhiColorClearValue
 */
uint qHash(const QRhiColorClearValue &v, uint seed) Q_DECL_NOTHROW
{
    const QVector4D c = v.rgba();
    return qFloor((c.x() + c.y() + c.z() + c.w()) * 1000) + seed;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiColorClearValue &v)
{
    QDebugStateSaver saver(dbg);
    const QVector4D c = v.rgba();
    dbg.nospace() << "QRhiColorClearValue(r=" << c.x()
                  << " g=" << c.y()
                  << " b=" << c.z()
                  << " a=" << c.w()
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiDepthStencilClearValue
    \inmodule QtRhi
    \brief Specifies clear values for a depth or stencil buffer.
 */

/*!
    Constructs a depth/stencil clear value with depth clear value 1.0f and
    stencil clear value 0.
 */
QRhiDepthStencilClearValue::QRhiDepthStencilClearValue()
    : m_d(1),
      m_s(0)
{
}

/*!
    Constructs a depth/stencil clear value with depth clear value \a d and
    stencil clear value \a s.
 */
QRhiDepthStencilClearValue::QRhiDepthStencilClearValue(float d, quint32 s)
    : m_d(d),
      m_s(s)
{
}

/*!
    \return \c true if the values in the two QRhiDepthStencilClearValue objects
    \a a and \a b are equal.

    \relates QRhiDepthStencilClearValue
 */
bool operator==(const QRhiDepthStencilClearValue &a, const QRhiDepthStencilClearValue &b) Q_DECL_NOTHROW
{
    return a.depthClearValue() == b.depthClearValue()
            && a.stencilClearValue() == b.stencilClearValue();
}

/*!
    \return \c false if the values in the two QRhiDepthStencilClearValue
    objects \a a and \a b are equal; otherwise returns \c true.

    \relates QRhiDepthStencilClearValue
*/
bool operator!=(const QRhiDepthStencilClearValue &a, const QRhiDepthStencilClearValue &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a v, using \a seed to seed the calculation.

    \relates QRhiDepthStencilClearValue
 */
uint qHash(const QRhiDepthStencilClearValue &v, uint seed) Q_DECL_NOTHROW
{
    return seed * (qFloor(v.depthClearValue() * 100) + v.stencilClearValue());
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiDepthStencilClearValue &v)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QRhiDepthStencilClearValue(depth-clear=" << v.depthClearValue()
                  << " stencil-clear=" << v.stencilClearValue()
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiViewport
    \inmodule QtRhi
    \brief Specifies a viewport rectangle.

    Used with QRhiCommandBuffer::setViewport().

    \note QRhi assumes OpenGL-style viewport coordinates, meaning x and y are
    bottom-left.

    Typical usage is like the following:

    \badcode
      const QSize outputSizeInPixels = swapchain->currentPixelSize();
      const QRhiViewport viewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height());
      cb->beginPass(swapchain->currentFrameRenderTarget(), { 0, 0, 0, 1 }, { 1, 0 });
      cb->setGraphicsPipeline(ps);
      cb->setViewport(viewport);
      ...
    \endcode

    \sa QRhiCommandBuffer::setViewport(), QRhi::clipSpaceCorrMatrix(), QRhiScissor
 */

/*!
    Constructs a viewport description with a default rectangle and depth range.
    The default depth range is 0.0f - 1.0f.

    \sa QRhi::clipSpaceCorrMatrix()
 */
QRhiViewport::QRhiViewport()
    : m_rect(0, 0, 1280, 720),
      m_minDepth(0),
      m_maxDepth(1)
{
}

/*!
    Constructs a viewport description with the rectangle specified by \a x, \a
    y, \a w, \a h and the depth range \a minDepth and \a maxDepth.

    \note x and y are assumed to be the bottom-left position.

    \sa QRhi::clipSpaceCorrMatrix()
 */
QRhiViewport::QRhiViewport(float x, float y, float w, float h, float minDepth, float maxDepth)
    : m_rect(x, y, w, h),
      m_minDepth(minDepth),
      m_maxDepth(maxDepth)
{
}

/*!
    \return \c true if the values in the two QRhiViewport objects
    \a a and \a b are equal.

    \relates QRhiViewport
 */
bool operator==(const QRhiViewport &a, const QRhiViewport &b) Q_DECL_NOTHROW
{
    return a.viewport() == b.viewport()
            && a.minDepth() == b.minDepth()
            && a.maxDepth() == b.maxDepth();
}

/*!
    \return \c false if the values in the two QRhiViewport
    objects \a a and \a b are equal; otherwise returns \c true.

    \relates QRhiViewport
*/
bool operator!=(const QRhiViewport &a, const QRhiViewport &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a v, using \a seed to seed the calculation.

    \relates QRhiViewport
 */
uint qHash(const QRhiViewport &v, uint seed) Q_DECL_NOTHROW
{
    const QVector4D r = v.viewport();
    return seed + r.x() + r.y() + r.z() + r.w() + qFloor(v.minDepth() * 100) + qFloor(v.maxDepth() * 100);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiViewport &v)
{
    QDebugStateSaver saver(dbg);
    const QVector4D r = v.viewport();
    dbg.nospace() << "QRhiViewport(bottom-left-x=" << r.x()
                  << " bottom-left-y=" << r.y()
                  << " width=" << r.z()
                  << " height=" << r.w()
                  << " minDepth=" << v.minDepth()
                  << " maxDepth=" << v.maxDepth()
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiScissor
    \inmodule QtRhi
    \brief Specifies a scissor rectangle.

    Used with QRhiCommandBuffer::setScissor(). Setting a scissor rectangle is
    only possible with a QRhiGraphicsPipeline that has
    QRhiGraphicsPipeline::UsesScissor set.

    \note QRhi assumes OpenGL-style scissor coordinates, meaning x and y are
    bottom-left.

    \sa QRhiCommandBuffer::setScissor(), QRhiViewport
 */

/*!
    Constructs an empty scissor.
 */
QRhiScissor::QRhiScissor()
{
}

/*!
    Constructs a scissor with the rectangle specified by \a x, \a y, \a w, and
    \a h.

    \note x and y are assumed to be the bottom-left position.
 */
QRhiScissor::QRhiScissor(int x, int y, int w, int h)
    : m_rect(x, y, w, h)
{
}

/*!
    \return \c true if the values in the two QRhiScissor objects
    \a a and \a b are equal.

    \relates QRhiScissor
 */
bool operator==(const QRhiScissor &a, const QRhiScissor &b) Q_DECL_NOTHROW
{
    return a.scissor() == b.scissor();
}

/*!
    \return \c false if the values in the two QRhiScissor
    objects \a a and \a b are equal; otherwise returns \c true.

    \relates QRhiScissor
*/
bool operator!=(const QRhiScissor &a, const QRhiScissor &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a v, using \a seed to seed the calculation.

    \relates QRhiScissor
 */
uint qHash(const QRhiScissor &v, uint seed) Q_DECL_NOTHROW
{
    const QVector4D r = v.scissor();
    return seed + r.x() + r.y() + r.z() + r.w();
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiScissor &s)
{
    QDebugStateSaver saver(dbg);
    const QVector4D r = s.scissor();
    dbg.nospace() << "QRhiScissor(bottom-left-x=" << r.x()
                  << " bottom-left-y=" << r.y()
                  << " width=" << r.z()
                  << " height=" << r.w()
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiVertexInputBinding
    \inmodule QtRhi
    \brief Describes a vertex input binding.

    Specifies the stride (in bytes, must be a multiple of 4), the
    classification and optionally the instance step rate.

    As an example, assume a vertex shader with the following inputs:

    \badcode
        layout(location = 0) in vec4 position;
        layout(location = 1) in vec2 texcoord;
    \endcode

    Now let's assume also that 3 component vertex positions \c{(x, y, z)} and 2
    component texture coordinates \c{(u, v)} are provided in a non-interleaved
    format in a buffer (or separate buffers even). Definining two bindings
    could then be done like this:

    \badcode
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { 3 * sizeof(float) },
            { 2 * sizeof(float) }
        });
    \endcode

    Only the stride is interesting here since instancing is not used. The
    binding number is given by the index of the QRhiVertexInputBinding
    element in the bindings vector of the QRhiVertexInputLayout.

    Once a graphics pipeline with this vertex input layout is bound, the vertex
    inputs could be set up like the following for drawing a cube with 36
    vertices, assuming we have a single buffer with first the positions and
    then the texture coordinates:

    \badcode
        cb->setVertexInput(0, { { cubeBuf, 0 }, { cubeBuf, 36 * 3 * sizeof(float) } });
    \endcode

    Note how the index defined by \c {startBinding + i}, where \c i is the
    index in the second argument of
    \l{QRhiCommandBuffer::setVertexInput()}{setVertexInput()}, matches the
    index of the corresponding entry in the \c bindings vector of the
    QRhiVertexInputLayout.

    \note the stride must always be a multiple of 4.

    \sa QRhiCommandBuffer::setVertexInput()
 */

/*!
    \enum QRhiVertexInputBinding::Classification
    Describes the input data classification.

    \value PerVertex Data is per-vertex
    \value PerInstance Data is per-instance
 */

/*!
    Constructs an empty vertex input binding description.
 */
QRhiVertexInputBinding::QRhiVertexInputBinding()
{
}

/*!
    Constructs a vertex input binding description with the specified \a stride,
    classification \a cls, and instance step rate \a stepRate.

    \note \a stepRate other than 1 is only supported when
    QRhi::CustomInstanceStepRate is reported to be supported.
 */
QRhiVertexInputBinding::QRhiVertexInputBinding(quint32 stride, Classification cls, int stepRate)
    : m_stride(stride),
      m_classification(cls),
      m_instanceStepRate(stepRate)
{
}

/*!
    \return \c true if the values in the two QRhiVertexInputBinding objects
    \a a and \a b are equal.

    \relates QRhiVertexInputBinding
 */
bool operator==(const QRhiVertexInputBinding &a, const QRhiVertexInputBinding &b) Q_DECL_NOTHROW
{
    return a.stride() == b.stride()
            && a.classification() == b.classification()
            && a.instanceStepRate() == b.instanceStepRate();
}

/*!
    \return \c false if the values in the two QRhiVertexInputBinding
    objects \a a and \a b are equal; otherwise returns \c true.

    \relates QRhiVertexInputBinding
*/
bool operator!=(const QRhiVertexInputBinding &a, const QRhiVertexInputBinding &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a v, using \a seed to seed the calculation.

    \relates QRhiVertexInputBinding
 */
uint qHash(const QRhiVertexInputBinding &v, uint seed) Q_DECL_NOTHROW
{
    return seed + v.stride() + v.classification();
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiVertexInputBinding &b)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QRhiVertexInputBinding(stride=" << b.stride()
                  << " cls=" << b.classification()
                  << " step-rate=" << b.instanceStepRate()
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiVertexInputAttribute
    \inmodule QtRhi
    \brief Describes a single vertex input element.

    The members specify the binding number, location, format, and offset for a
    single vertex input element.

    \note For HLSL it is assumed that the vertex shader uses
    \c{TEXCOORD<location>} as the semantic for each input. Hence no separate
    semantic name and index.

    As an example, assume a vertex shader with the following inputs:

    \badcode
        layout(location = 0) in vec4 position;
        layout(location = 1) in vec2 texcoord;
    \endcode

    Now let's assume that we have 3 component vertex positions \c{(x, y, z)}
    and 2 component texture coordinates \c{(u, v)} are provided in a
    non-interleaved format in a buffer (or separate buffers even). Once two
    bindings are defined, the attributes could be specified as:

    \badcode
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { 3 * sizeof(float) },
            { 2 * sizeof(float) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
            { 1, 1, QRhiVertexInputAttribute::Float2, 0 }
        });
    \endcode

    Once a graphics pipeline with this vertex input layout is bound, the vertex
    inputs could be set up like the following for drawing a cube with 36
    vertices, assuming we have a single buffer with first the positions and
    then the texture coordinates:

    \badcode
        cb->setVertexInput(0, { { cubeBuf, 0 }, { cubeBuf, 36 * 3 * sizeof(float) } });
    \endcode

    When working with interleaved data, there will typically be just one
    binding, with multiple attributes refering to that same buffer binding
    point:

    \badcode
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { 5 * sizeof(float) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float2, 3 * sizeof(float) }
        });
    \endcode

    and then:

    \badcode
        cb->setVertexInput(0, { { interleavedCubeBuf, 0 } });
    \endcode

    \sa QRhiCommandBuffer::setVertexInput()
 */

/*!
    \enum QRhiVertexInputAttribute::Format
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
    Constructs an empty vertex input attribute description.
 */
QRhiVertexInputAttribute::QRhiVertexInputAttribute()
{
}

/*!
    Constructs a vertex input attribute description with the specified \a
    binding number, \a location, \a format, and \a offset.
 */
QRhiVertexInputAttribute::QRhiVertexInputAttribute(int binding, int location, Format format, quint32 offset)
    : m_binding(binding),
      m_location(location),
      m_format(format),
      m_offset(offset)
{
}

/*!
    \return \c true if the values in the two QRhiVertexInputAttribute objects
    \a a and \a b are equal.

    \relates QRhiVertexInputAttribute
 */
bool operator==(const QRhiVertexInputAttribute &a, const QRhiVertexInputAttribute &b) Q_DECL_NOTHROW
{
    return a.binding() == b.binding()
            && a.location() == b.location()
            && a.format() == b.format()
            && a.offset() == b.offset();
}

/*!
    \return \c false if the values in the two QRhiVertexInputAttribute
    objects \a a and \a b are equal; otherwise returns \c true.

    \relates QRhiVertexInputAttribute
*/
bool operator!=(const QRhiVertexInputAttribute &a, const QRhiVertexInputAttribute &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a v, using \a seed to seed the calculation.

    \relates QRhiVertexInputAttribute
 */
uint qHash(const QRhiVertexInputAttribute &v, uint seed) Q_DECL_NOTHROW
{
    return seed + v.binding() + v.location() + v.format() + v.offset();
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiVertexInputAttribute &a)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QRhiVertexInputAttribute(binding=" << a.binding()
                  << " location=" << a.location()
                  << " format=" << a.format()
                  << " offset=" << a.offset()
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiVertexInputLayout
    \inmodule QtRhi
    \brief Describes the layout of vertex inputs consumed by a vertex shader.

    The vertex input layout is defined by the collections of
    QRhiVertexInputBinding and QRhiVertexInputAttribute.
 */

/*!
    Constructs an empty vertex input layout description.
 */
QRhiVertexInputLayout::QRhiVertexInputLayout()
{
}

/*!
    \return \c true if the values in the two QRhiVertexInputLayout objects
    \a a and \a b are equal.

    \relates QRhiVertexInputLayout
 */
bool operator==(const QRhiVertexInputLayout &a, const QRhiVertexInputLayout &b) Q_DECL_NOTHROW
{
    return a.bindings() == b.bindings()
            && a.attributes() == b.attributes();
}

/*!
    \return \c false if the values in the two QRhiVertexInputLayout
    objects \a a and \a b are equal; otherwise returns \c true.

    \relates QRhiVertexInputLayout
*/
bool operator!=(const QRhiVertexInputLayout &a, const QRhiVertexInputLayout &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a v, using \a seed to seed the calculation.

    \relates QRhiVertexInputLayout
 */
uint qHash(const QRhiVertexInputLayout &v, uint seed) Q_DECL_NOTHROW
{
    return qHash(v.bindings(), seed) + qHash(v.attributes(), seed);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiVertexInputLayout &v)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QRhiVertexInputLayout(bindings=" << v.bindings()
                  << " attributes=" << v.attributes()
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiGraphicsShaderStage
    \inmodule QtRhi
    \brief Specifies the type and the shader code for a shader stage in the graphics pipeline.

    \note Some backends only support a subset of the stages. Use
    QRhi::isFeatureSupported() to query for support at runtime. For example,
    Metal has no geometry shader support, while the OpenGL 2.x backend only
    supports the vertex and fragment stages.
 */

/*!
    \enum QRhiGraphicsShaderStage::Type
    Specifies the type of the shader stage.

    \value Vertex Vertex stage
    \value Fragment Fragment (pixel) stage
    \value Geometry Geometry stage
    \value TessellationControl Tessellation control (hull) stage
    \value TessellationEvaluation Tessellation evaluation (domain) stage
 */

/*!
    Constructs an empty shader stage description.
 */
QRhiGraphicsShaderStage::QRhiGraphicsShaderStage()
{
}

/*!
    Constructs a shader stage description with the \a type of the stage and the
    \a shader.

    The shader variant \a v defaults to QBakedShaderKey::StandardShader. A
    QBakedShader pack contains multiple source and binary versions of a shader.
    In addition, it can also contain variants of the shader with slightly
    modified code. \a v can then be used to select the desired variant.
 */
QRhiGraphicsShaderStage::QRhiGraphicsShaderStage(Type type, const QBakedShader &shader, QBakedShaderKey::ShaderVariant v)
    : m_type(type),
      m_shader(shader),
      m_shaderVariant(v)
{
}

/*!
    \return \c true if the values in the two QRhiGraphicsShaderStage objects
    \a a and \a b are equal.

    \relates QRhiGraphicsShaderStage
 */
bool operator==(const QRhiGraphicsShaderStage &a, const QRhiGraphicsShaderStage &b) Q_DECL_NOTHROW
{
    return a.type() == b.type()
            && a.shader() == b.shader()
            && a.shaderVariant() == b.shaderVariant();
}

/*!
    \return \c false if the values in the two QRhiGraphicsShaderStage
    objects \a a and \a b are equal; otherwise returns \c true.

    \relates QRhiGraphicsShaderStage
*/
bool operator!=(const QRhiGraphicsShaderStage &a, const QRhiGraphicsShaderStage &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a v, using \a seed to seed the calculation.

    \relates QRhiGraphicsShaderStage
 */
uint qHash(const QRhiGraphicsShaderStage &v, uint seed) Q_DECL_NOTHROW
{
    return v.type() + qHash(v.shader(), seed) + v.shaderVariant();
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiGraphicsShaderStage &s)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QRhiGraphicsShaderStage(type=" << s.type()
                  << " shader=" << s.shader()
                  << " variant=" << s.shaderVariant()
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiColorAttachment
    \inmodule QtRhi
    \brief Describes the a single color attachment of a render target.

    A color attachment is either a QRhiTexture or a QRhiRenderBuffer. The
    former, when texture() is set, is used in most cases.

    \note texture() and renderBuffer() cannot be both set (be non-null at the
    same time).

    Setting renderBuffer instead is recommended only when multisampling is
    needed. Relying on QRhi::MultisampleRenderBuffer is a better choice than
    QRhi::MultisampleTexture in practice since the former is available in more
    run time configurations (e.g. when running on OpenGL ES 3.0 which has no
    support for multisample textures, but does support multisample
    renderbuffers).

    When targeting a non-multisample texture, the layer() and level()
    indicate the targeted layer (face index \c{0-5} for cubemaps) and mip
    level.

    When texture() or renderBuffer() is multisample, resolveTexture() can be
    set optionally. When set, samples are resolved automatically into that
    (non-multisample) texture at the end of the render pass. When rendering
    into a multisample renderbuffers, this is the only way to get resolved,
    non-multisample content out of them. Multisample textures allow sampling in
    shaders so for them this is just one option.

    \note when resolving is enabled, the multisample data may not be written
    out at all. This means that the multisample texture() must not be used
    afterwards with shaders for sampling when resolveTexture() is set.
 */

/*!
    Constructs an empty color attachment description.
 */
QRhiColorAttachment::QRhiColorAttachment()
{
}

/*!
    Constructs a color attachment description that specifies \a texture as the
    associated color buffer.
 */
QRhiColorAttachment::QRhiColorAttachment(QRhiTexture *texture)
    : m_texture(texture)
{
}

/*!
    Constructs a color attachment description that specifies \a renderBuffer as
    the associated color buffer.
 */
QRhiColorAttachment::QRhiColorAttachment(QRhiRenderBuffer *renderBuffer)
    : m_renderBuffer(renderBuffer)
{
}

/*!
    \class QRhiTextureRenderTargetDescription
    \inmodule QtRhi
    \brief Describes the color and depth or depth/stencil attachments of a render target.

    A texture render target has zero or more textures as color attachments,
    zero or one renderbuffer as combined depth/stencil buffer or zero or one
    texture as depth buffer.

    \note depthStencilBuffer() and depthTexture() cannot be both set (cannot be
    non-null at the same time).
 */

/*!
    Constructs an empty texture render target description.
 */
QRhiTextureRenderTargetDescription::QRhiTextureRenderTargetDescription()
{
}

/*!
    Constructs a texture render target description with one attachment
    described by \a colorAttachment.
 */
QRhiTextureRenderTargetDescription::QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment)
{
    m_colorAttachments.append(colorAttachment);
}

/*!
    Constructs a texture render target description with two attachments, a
    color attachment described by \a colorAttachment, and a depth/stencil
    attachment with \a depthStencilBuffer.
 */
QRhiTextureRenderTargetDescription::QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment,
                                                                       QRhiRenderBuffer *depthStencilBuffer)
    : m_depthStencilBuffer(depthStencilBuffer)
{
    m_colorAttachments.append(colorAttachment);
}

/*!
    Constructs a texture render target description with two attachments, a
    color attachment described by \a colorAttachment, and a depth attachment
    with \a depthTexture.

    \note \a depthTexture must have a suitable format, such as QRhiTexture::D16
    or QRhiTexture::D32.
 */
QRhiTextureRenderTargetDescription::QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment,
                                                                       QRhiTexture *depthTexture)
    : m_depthTexture(depthTexture)
{
    m_colorAttachments.append(colorAttachment);
}

/*!
    \class QRhiTextureMipLevel
    \inmodule QtRhi
    \brief Describes one mip level in a layer in a texture upload operation.

    The source content is specified either as a QImage or as a raw blob. The
    former is only allowed for uncompressed textures, while the latter is only
    supported for compressed ones.

    \note image() and compressedData() cannot be both set at the same time.

    destinationTopLeft() specifies the top-left corner of the target
    rectangle. Defaults to (0, 0).

    An empty sourceSize() (the default) indicates that size is assumed to be
    the size of the subresource. For uncompressed textures this implies that
    the size of the source image() must match the subresource. For compressed
    textures sufficient amount of data must be provided in compressedData().

    \note With compressed textures the first upload must always match the
    subresource size due to graphics API limitations with some backends.

    sourceTopLeft() is is only supported for uncompressed textures, and
    specifies the top-left corner of the source rectangle.

    \note Setting sourceSize() or sourceTopLeft() may trigger a QImage copy
    internally, depending on the format and the backend.
 */

/*!
    Constructs an empty mip level description.
 */
QRhiTextureMipLevel::QRhiTextureMipLevel()
{
}

/*!
    Constructs a mip level description with a \a image.

    The \l{QImage::size()}{size} of \a image must match the size of the mip
    level. For level 0 that is the \l{QRhiTexture::pixelSize()}{texture size}.

    The bit depth of \a image must be compatible with the
    \l{QRhiTexture::Format}{texture format}.

    To describe a partial upload, call setSourceSize(), setSourceTopLeft(), or
    setDestinationTopLeft() afterwards.
 */
QRhiTextureMipLevel::QRhiTextureMipLevel(const QImage &image)
    : m_image(image)
{
}

/*!
    Constructs a mip level description suitable for compressed textures. The
    compressed data is specified in \a compressedData.
 */
QRhiTextureMipLevel::QRhiTextureMipLevel(const QByteArray &compressedData)
    : m_compressedData(compressedData)
{
}

/*!
    \class QRhiTextureLayer
    \inmodule QtRhi
    \brief Describes one layer (face for cubemaps) in a texture upload operation.
 */

/*!
    Constructs an empty texture layer description.
 */
QRhiTextureLayer::QRhiTextureLayer()
{
}

/*!
    Constructs a texture layer description with the specified list of \a
    mipImages.
 */
QRhiTextureLayer::QRhiTextureLayer(const QVector<QRhiTextureMipLevel> &mipImages)
    : m_mipImages(mipImages)
{
}

/*!
    \class QRhiTextureUploadDescription
    \inmodule QtRhi
    \brief Describes a texture upload operation.

    Used with QRhiResourceUpdateBatch::uploadTexture(). That function has two
    variants: one taking a QImage and one taking a
    QRhiTextureUploadDescription. The former is a convenience version,
    internally creating a QRhiTextureUploadDescription with a single layer and
    a single image in that layer. However, when cubemaps, pre-generated mip
    images, or compressed textures are involved, applications will have to work
    directly with this class instead.

    \note Cubemaps have one layer for each of the six faces in the order +X,
    -X, +Y, -Y, +Z, -Z.

    For example, specifying the faces of a cubemap could look like the following:

    \badcode
        QImage faces[6];
        ...
        QVector<QRhiTextureLayer> layers;
        for (int i = 0; i < 6; ++i)
          layers.append(QRhiTextureLayer({ QRhiTextureMipLevel(faces[i]) });
        QRhiTextureUploadDescription desc(layers);
        resourceUpdates->uploadTexture(texture, desc);
    \endcode

    Another example that specifies mip images for a compressed texture:

    \badcode
        QVector<QRhiTextureMipLevel> mipImages;
        const int mipCount = rhi->mipLevelsForSize(compressedTexture->pixelSize());
        for (int level = 0; level < mipCount; ++level) {
            const QByteArray compressedDataForLevel = ...
            mipImages.append(QRhiTextureMipLevel(compressedDataForLevel));
        }
        QRhiTextureLayer layer(mipImages);
        QRhiTextureUploadDescription desc({ layer });
        resourceUpdates->uploadTexture(compressedTexture, desc);
    \endcode
 */

/*!
    Constructs an empty texture upload description.
 */
QRhiTextureUploadDescription::QRhiTextureUploadDescription()
{
}

/*!
    Constructs a texture upload description with the specified list of \a
    layers.
 */
QRhiTextureUploadDescription::QRhiTextureUploadDescription(const QVector<QRhiTextureLayer> &layers)
    : m_layers(layers)
{
}

/*!
    \class QRhiTextureCopyDescription
    \inmodule QtRhi
    \brief Describes a texture-to-texture copy operation.

    An empty pixelSize() indicates that the entire subresource is to be copied.
    A default constructed copy description therefore leads to copying the
    entire subresource at level 0 of layer 0.

    \note The source texture must be created with
    QRhiTexture::UsedAsTransferSource.

    \note The source and destination rectangles defined by pixelSize(),
    sourceTopLeft(), and destinationTopLeft() must fit the source and
    destination textures, respectively. The behavior is undefined otherwise.
 */

/*!
    Constructs an empty texture copy description.
 */
QRhiTextureCopyDescription::QRhiTextureCopyDescription()
{
}

/*!
    \class QRhiReadbackDescription
    \inmodule QtRhi
    \brief Describes a readback (reading back texture contents from possibly GPU-only memory) operation.

    The source of the readback operation is either a QRhiTexture or the
    current backbuffer of the currently targeted QRhiSwapChain. When
    texture() is not set, the swapchain is used. Otherwise the specified
    QRhiTexture is treated as the source.

    \note Textures used in readbacks must be created with
    QRhiTexture::UsedAsTransferSource.

    \note Swapchains used in readbacks must be created with
    QRhiSwapChain::UsedAsTransferSource.

    layer() and level() are only applicable when the source is a QRhiTexture.

    \note Multisample textures cannot be read back. Readbacks are supported for
    multisample swapchain buffers however.
 */

/*!
    Constructs an empty texture readback description.

    \note The source texture is set to null by default, which is still a valid
    readback: it specifies that the backbuffer of the current swapchain is to
    be read back. (current meaning the frame's target swapchain at the time of
    committing the QRhiResourceUpdateBatch with the
    \l{QRhiResourceUpdateBatch::readBackTexture()}{texture readback} on it)
 */
QRhiReadbackDescription::QRhiReadbackDescription()
{
}

/*!
    Constructs an texture readback description that specifies that level 0 of
    layer 0 of \a texture is to be read back.

    \note \a texture can also be null in which case this constructor is
    identical to the argumentless variant.
 */
QRhiReadbackDescription::QRhiReadbackDescription(QRhiTexture *texture)
    : m_texture(texture)
{
}

/*!
    \class QRhiReadbackResult
    \inmodule QtRhi
    \brief Describes the results of a potentially asynchronous readback operation.

    When \l completed is set, the function is invoked when the \l data is
    available. \l format and \l pixelSize are set upon completion together with
    \l data.
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
    \internal
 */
QRhiResource::QRhiResource(QRhiImplementation *rhi_)
    : rhi(rhi_)
{
}

/*!
   Destructor.
 */
QRhiResource::~QRhiResource()
{
}

/*!
    \fn void QRhiResource::release()

    Releases the underlying native graphics resources. Safe to call multiple
    times, subsequent invocations will be a no-op then.

    Once release() is called, the QRhiResource instance can be reused, by
    calling the appropriate \c build() function again, or destroyed.
 */

/*!
    Releases native graphics resources, if there is any, and destroys the
    QRhiResource. Equivalent to \c{r->release(); delete r; }.
 */
void QRhiResource::releaseAndDestroy()
{
    release();
    delete this;
}

/*!
    \return the currently set object name. By default the name is empty.
 */
QByteArray QRhiResource::name() const
{
    return objectName;
}

/*!
    Sets a \a name for the object.

    This has two uses: to get descriptive names for the native graphics
    resources visible in graphics debugging tools, such as
    \l{https://renderdoc.org/}{RenderDoc} and
    \l{https://developer.apple.com/xcode/}{XCode}, and in the output stream of
    QRhiProfiler.

    When it comes to naming native objects by relaying the name via the
    appropriate graphics API, note that the name is ignored when
    QRhi::DebugMarkers are not supported, and may, depending on the backend,
    also be ignored when QRhi::EnableDebugMarkers is not set.

    \note The name may be ignored for objects other than buffers,
    renderbuffers, and textures, depending on the backend.

    \note The name may be modified. For slotted resources, such as a QRhiBuffer
    backed by multiple native buffers, QRhi will append a suffix to make the
    underlying native buffers easily distinguishable from each other.
 */
void QRhiResource::setName(const QByteArray &name)
{
    objectName = name;
    objectName.replace(',', '_'); // cannot contain comma for QRhiProfiler
}

/*!
    \return true if this resource is shareable between QRhi instances via a
    QRhiResourceSharingHost. It also means that such an QRhiResource can
    outlive the QRhi on which it was created.

    \note Once a shareable QRhiResource is \c orphaned, meaning that the QRhi
    from which it was created gets destroyed, \c{build()} is not a valid
    operation anymore. The QRhiResource can then only be used in graphics
    operations (as the underlying graphics objects are still there and valid)
    or can be released, as long as the QRhiResourceSharingHost,
    with which the creator QRhi was associated with, has at least one
    associated QRhi alive.

    \note Shareable resources typically include buffers, textures, samplers,
    and renderbuffers. Most backends will not support other type of shareable
    resources.

    \sa QRhiResourceSharingHost
 */
bool QRhiResource::isShareable() const
{
    return false;
}

/*!
    \class QRhiBuffer
    \inmodule QtRhi
    \brief Vertex, index, or uniform (constant) buffer resource.
 */

/*!
    \enum QRhiBuffer::Type
    Specifies storage type of buffer resource.

    \value Immutable Indicates that the data is not expected to change ever
    after the initial upload. Under the hood such buffer resources are
    typically placed in device local (GPU) memory (on systems where
    applicable). Uploading new data is possible, but may be expensive. The
    upload typically happens by copying to a separate, host visible staging
    buffer from which a GPU buffer-to-buffer copy is issued into the actual
    GPU-only buffer.

    \value Static Indicates that the data is expected to change only
    infrequently. Typically placed in device local (GPU) memory, where
    applicable. On backends where host visible staging buffers are used for
    uploading, the staging buffers are kept around for this type, unlike with
    Immutable, so subsequent uploads do not suffer in performance. Frequent
    updates, especially updates in consecutive frames, should be avoided.

    \value Dynamic Indicates that the data is expected to change frequently.
    Not recommended for large buffers. Typically backed by host visible memory
    in 2 copies in order to allow for changing without stalling the graphics
    pipeline. The double buffering is managed transparently to the applications
    and is not exposed in the API here in any form. This is the recommended,
    and, with some backends, the only possible, type for buffers with
    UniformBuffer usage.
 */

/*!
    \enum QRhiBuffer::UsageFlag
    Flag values to specify how the buffer is going to be used.

    \value VertexBuffer Vertex buffer
    \value IndexBuffer Index buffer
    \value UniformBuffer Uniform (constant) buffer
 */

/*!
    \fn void QRhiBuffer::setSize(int sz)

    Sets the size of the buffer in bytes. The size is normally specified in
    QRhi::newBuffer() so this function is only used when the size has to be
    changed. As with other setters, the size only takes effect when calling
    build(), and for already built buffers this involves releasing the previous
    native resource and creating new ones under the hood.

    Backends may choose to allocate buffers bigger than \a sz in order to
    fulfill alignment requirements. This is hidden from the applications and
    size() will always report the size requested in \a sz.
 */

/*!
    \internal
 */
QRhiBuffer::QRhiBuffer(QRhiImplementation *rhi, Type type_, UsageFlags usage_, int size_)
    : QRhiResource(rhi),
      m_type(type_), m_usage(usage_), m_size(size_)
{
}

/*!
    \fn bool QRhiBuffer::build()

    Creates the corresponding native graphics resources. If there are already
    resources present due to an earlier build() with no corresponding
    release(), then release() is called implicitly first.

    \return \c true when successful, \c false when a graphics operation failed.
    Regardless of the return value, calling release() is always safe.
 */

/*!
    \class QRhiRenderBuffer
    \inmodule QtRhi
    \brief Renderbuffer resource.

    Renderbuffers cannot be sampled or read but have some benefits over
    textures in some cases:

    A DepthStencil renderbuffer may be lazily allocated and be backed by
    transient memory with some APIs. On some platforms this may mean the
    depth/stencil buffer uses no physical backing at all.

    Color renderbuffers are useful since QRhi::MultisampleRenderBuffer may be
    supported even when QRhi::MultisampleTexture is not.

    How the renderbuffer is implemented by a backend is not exposed to the
    applications. In some cases it may be backed by ordinary textures, while in
    others there may be a different kind of native resource used.
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
    \internal
 */
QRhiRenderBuffer::QRhiRenderBuffer(QRhiImplementation *rhi, Type type_, const QSize &pixelSize_,
                                   int sampleCount_, Flags flags_)
    : QRhiResource(rhi),
      m_type(type_), m_pixelSize(pixelSize_), m_sampleCount(sampleCount_), m_flags(flags_)
{
}

/*!
    \fn bool QRhiRenderBuffer::build()

    Creates the corresponding native graphics resources. If there are already
    resources present due to an earlier build() with no corresponding
    release(), then release() is called implicitly first.

    \return \c true when successful, \c false when a graphics operation failed.
    Regardless of the return value, calling release() is always safe.
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
    \internal
 */
QRhiTexture::QRhiTexture(QRhiImplementation *rhi, Format format_, const QSize &pixelSize_,
                         int sampleCount_, Flags flags_)
    : QRhiResource(rhi),
      m_format(format_), m_pixelSize(pixelSize_), m_sampleCount(sampleCount_), m_flags(flags_)
{
}

/*!
    \fn bool QRhiTexture::build()

    Creates the corresponding native graphics resources. If there are already
    resources present due to an earlier build() with no corresponding
    release(), then release() is called implicitly first.

    \return \c true when successful, \c false when a graphics operation failed.
    Regardless of the return value, calling release() is always safe.
 */

/*!
    \return a pointer to a backend-specific QRhiNativeHandles subclass, such as
    QRhiVulkanTextureNativeHandles. The returned value is null when exposing
    the underlying native resources is not supported by the backend.

    \sa QRhiVulkanTextureNativeHandles, QRhiD3D11TextureNativeHandles,
    QRhiMetalTextureNativeHandles, QRhiGles2TextureNativeHandles
 */
const QRhiNativeHandles *QRhiTexture::nativeHandles()
{
    return nullptr;
}

/*!
    Similar to build() except that no new native textures are created. Instead,
    the texture from \a src is used.

    This allows importing an existing native texture object (which must belong
    to the same device or sharing context, depending on the graphics API) from
    an external graphics engine.

    \note format(), pixelSize(), sampleCount(), and flags() must still be set
    correctly. Passing incorrect sizes and other values to QRhi::newTexture()
    and then following it with a buildFrom() expecting that the native texture
    object alone is sufficient to deduce such values is \b wrong and will lead
    to problems.

    \note QRhiTexture does not take ownership of the texture object. release()
    does not free the object or any associated memory.

    The opposite of this operation, exposing a QRhiTexture-created native
    texture object to a foreign engine, is possible via nativeHandles().

    \sa QRhiVulkanTextureNativeHandles, QRhiD3D11TextureNativeHandles,
    QRhiMetalTextureNativeHandles, QRhiGles2TextureNativeHandles
 */
bool QRhiTexture::buildFrom(const QRhiNativeHandles *src)
{
    Q_UNUSED(src);
    return false;
}

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
    \internal
 */
QRhiSampler::QRhiSampler(QRhiImplementation *rhi,
                         Filter magFilter_, Filter minFilter_, Filter mipmapMode_,
                         AddressMode u_, AddressMode v_, AddressMode w_)
    : QRhiResource(rhi),
      m_magFilter(magFilter_), m_minFilter(minFilter_), m_mipmapMode(mipmapMode_),
      m_addressU(u_), m_addressV(v_), m_addressW(w_)
{
}

/*!
    \class QRhiRenderPassDescriptor
    \inmodule QtRhi
    \brief Render pass resource.
 */

/*!
    \internal
 */
QRhiRenderPassDescriptor::QRhiRenderPassDescriptor(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

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
    \internal
 */
QRhiRenderTarget::QRhiRenderTarget(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

/*!
    \fn QRhiRenderTarget::Type QRhiRenderTarget::type() const

    \return the type of the render target.
 */

/*!
    \fn QSize QRhiRenderTarget::sizeInPixels() const

    \return the size in pixels.
 */

/*!
    \fn float QRhiRenderTarget::devicePixelRatio() const

    \return the device pixel ratio. For QRhiTextureRenderTarget this is always
    1. For targets retrieved from a QRhiSwapChain the value reflects the
    \l{QWindow::devicePixelRatio()}{device pixel ratio} of the targeted
    QWindow.
 */

/*!
    \internal
 */
QRhiReferenceRenderTarget::QRhiReferenceRenderTarget(QRhiImplementation *rhi)
    : QRhiRenderTarget(rhi)
{
}

/*!
    \class QRhiTextureRenderTarget
    \inmodule QtRhi
    \brief Texture render target resource.

    A texture render target allows rendering into one or more textures,
    optionally with a depth texture or depth/stencil renderbuffer.

    \note Textures used in combination with QRhiTextureRenderTarget must be
    created with the QRhiTexture::RenderTarget flag.

    The simplest example of creating a render target with a texture as its
    single color attachment:

    \badcode
        texture = rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget);
        texture->build();
        rt = rhi->newTextureRenderTarget({ texture });
        rp = rt->newCompatibleRenderPassDescriptor();
        rt->setRenderPassDescriptor(rt);
        rt->build();
        // rt can now be used with beginPass()
    \endcode
 */

/*!
    \enum QRhiTextureRenderTarget::Flag

    Flag values describing the load/store behavior for the render target. The
    load/store behavior may be baked into native resources under the hood,
    depending on the backend, and therefore it needs to be known upfront and
    cannot be changed without rebuilding (and so releasing and creating new
    native resources).

    \value PreserveColorContents Indicates that the contents of the color
    attachments is to be loaded when starting a render pass, instead of
    clearing. This is potentially more expensive, especially on mobile (tiled)
    GPUs, but allows preserving the existing contents between passes.

    \value PreserveDepthStencilContents Indicates that the contents of the
    depth texture is to be loaded when starting a render pass, instead
    clearing. Only applicable when a texture is used as the depth buffer
    (QRhiTextureRenderTargetDescription::depthTexture() is set) because
    depth/stencil renderbuffers may not have any physical backing and data may
    not be written out in the first place.
 */

/*!
    \internal
 */
QRhiTextureRenderTarget::QRhiTextureRenderTarget(QRhiImplementation *rhi,
                                                 const QRhiTextureRenderTargetDescription &desc_,
                                                 Flags flags_)
    : QRhiRenderTarget(rhi),
      m_desc(desc_),
      m_flags(flags_)
{
}

/*!
    \fn QRhiRenderPassDescriptor *QRhiTextureRenderTarget::newCompatibleRenderPassDescriptor()

    \return a new QRhiRenderPassDescriptor that is compatible with this render
    target.

    The returned value is used in two ways: it can be passed to
    setRenderPassDescriptor() and
    QRhiGraphicsPipeline::setRenderPassDescriptor(). A render pass descriptor
    describes the attachments (color, depth/stencil) and the load/store
    behavior that can be affected by flags(). A QRhiGraphicsPipeline can only
    be used in combination with a render target that has the same
    QRhiRenderPassDescriptor set.

    Two QRhiTextureRenderTarget instances can share the same render pass
    descriptor as long as they have the same number and type of attachments.
    The associated QRhiTexture or QRhiRenderBuffer instances are not part of
    the render pass descriptor so those can differ in the two
    QRhiTextureRenderTarget intances.

    \note resources, such as QRhiTexture instances, referenced in description()
    must already be built

    \sa build()
 */

/*!
    \fn bool QRhiTextureRenderTarget::build()

    Creates the corresponding native graphics resources. If there are already
    resources present due to an earlier build() with no corresponding
    release(), then release() is called implicitly first.

    \note renderPassDescriptor() must be set before calling build(). To obtain
    a QRhiRenderPassDescriptor compatible with the render target, call
    newCompatibleRenderPassDescriptor() before build() but after setting all
    other parameters, such as description() and flags(). To save resources,
    reuse the same QRhiRenderPassDescriptor with multiple
    QRhiTextureRenderTarget instances, whenever possible. Sharing the same
    render pass descriptor is only possible when the render targets have the
    same number and type of attachments (the actual textures can differ) and
    the same flags.

    \note resources, such as QRhiTexture instances, referenced in description()
    must already be built

    \return \c true when successful, \c false when a graphics operation failed.
    Regardless of the return value, calling release() is always safe.
 */

/*!
    \class QRhiShaderResourceBindings
    \inmodule QtRhi
    \brief Encapsulates resources for making buffer, texture, sampler resources visible to shaders.

    A QRhiShaderResourceBindings is a collection of QRhiShaderResourceBinding
    objects, each of which describe a single binding.

    Take a fragment shader with the following interface:

    \badcode
        layout(std140, binding = 0) uniform buf {
            mat4 mvp;
            int flip;
        } ubuf;

        layout(binding = 1) uniform sampler2D tex;
    \endcode

    To make resources visible to the shader, the following
    QRhiShaderResourceBindings could be created and then passed to
    QRhiGraphicsPipeline::setShaderResourceBindings():

    \badcode
        srb = rhi->newShaderResourceBindings();
        srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, ubuf),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, texture, sampler)
        });
        srb->build();
        ...
        ps = rhi->newGraphicsPipeline();
        ...
        ps->setShaderResourceBindings(srb);
        ps->build();
        ...
        cb->setGraphicsPipeline(ps);
        cb->setShaderResources(); // binds srb
    \endcode

    This assumes that \c ubuf is a QRhiBuffer, \c texture is a QRhiTexture,
    while \a sampler is a QRhiSampler. The example also assumes that the
    uniform block is present in the vertex shader as well so the same buffer is
    made visible to the vertex stage too.

    \section3 Advanced usage

    Building on the above example, let's assume that a pass now needs to use
    the exact same pipeline and shaders with a different texture. Creating a
    whole separate QRhiGraphicsPipeline just for this would be an overkill.
    This is why QRhiCommandBuffer::setShaderResources() allows specifying a \a
    srb argument. As long as the layouts (so the number of bindings and the
    binding points) match between two QRhiShaderResourceBindings, they can both
    be used with the same pipeline, assuming the pipeline was built with one of
    them in the first place.

    Creating and then using a new \c srb2 that is very similar to \c srb with
    the exception of referencing another texture could be implemented like the
    following:

    \badcode
        srb2 = rhi->newShaderResourceBindings();
        QVector<QRhiShaderResourceBinding> bindings = srb->bindings();
        bindings[1] = QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, anotherTexture, sampler);
        srb2->setBindings(bindings);
        srb2->build();
        ...
        cb->setGraphicsPipeline(ps);
        cb->setShaderResources(srb2); // binds srb2
    \endcode
 */

/*!
    \internal
 */
QRhiShaderResourceBindings::QRhiShaderResourceBindings(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

/*!
    \return \c true if the layout is compatible with \a other. The layout does
    not include the actual resource (such as, buffer or texture) and related
    parameters (such as, offset or size). It does include the binding point,
    pipeline stage, and resource type, however. The number and order of the
    bindings must also match in order to be compatible.

    When there is a QRhiGraphicsPipeline created with this
    QRhiShaderResourceBindings, and the function returns \c true, \a other can
    then safely be passed to QRhiCommandBuffer::setShaderResources(), and so
    be used with the pipeline in place of this QRhiShaderResourceBindings.

    This function can be called before build() as well. The bindings must
    already be set via setBindings() however.
 */
bool QRhiShaderResourceBindings::isLayoutCompatible(const QRhiShaderResourceBindings *other) const
{
    const int count = m_bindings.count();
    if (count != other->m_bindings.count())
        return false;

    for (int i = 0; i < count; ++i) {
        if (!m_bindings[i].isLayoutCompatible(other->m_bindings.at(i)))
            return false;
    }

    return true;
}

/*!
    \class QRhiShaderResourceBinding
    \inmodule QtRhi
    \brief Describes the shader resource for a single binding point.

    A QRhiShaderResourceBinding cannot be constructed directly. Instead, use
    the static functions uniformBuffer(), sampledTexture() to get an instance.
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
    \value GeometryStage Geometry stage
    \value TessellationControlStage Tessellation control (hull) stage
    \value TessellationEvaluationStage Tessellation evaluation (domain) stage
 */

/*!
    \internal
 */
QRhiShaderResourceBinding::QRhiShaderResourceBinding()
    : d(new QRhiShaderResourceBindingPrivate)
{
}

/*!
    \internal
 */
void QRhiShaderResourceBinding::detach()
{
    qAtomicDetach(d);
}

/*!
    \internal
 */
QRhiShaderResourceBinding::QRhiShaderResourceBinding(const QRhiShaderResourceBinding &other)
    : d(other.d)
{
    d->ref.ref();
}

/*!
    \internal
 */
QRhiShaderResourceBinding &QRhiShaderResourceBinding::operator=(const QRhiShaderResourceBinding &other)
{
    qAtomicAssign(d, other.d);
    return *this;
}

/*!
    Destructor.
 */
QRhiShaderResourceBinding::~QRhiShaderResourceBinding()
{
    if (!d->ref.deref())
        delete d;
}

/*!
    \return \c true if the layout is compatible with \a other. The layout does not
    include the actual resource (such as, buffer or texture) and related
    parameters (such as, offset or size).

    For example, \c a and \c b below are not equal, but are compatible layout-wise:

    \badcode
        auto a = QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, buffer);
        auto b = QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, someOtherBuffer, 256);
    \endcode
 */
bool QRhiShaderResourceBinding::isLayoutCompatible(const QRhiShaderResourceBinding &other) const
{
    return (d == other.d)
            || (d->binding == other.d->binding && d->stage == other.d->stage && d->type == other.d->type);
}

/*!
    \return a shader resource binding for the given binding number, pipeline
    stages, and buffer specified by \a binding, \a stage, and \a buf.
 */
QRhiShaderResourceBinding QRhiShaderResourceBinding::uniformBuffer(
        int binding, StageFlags stage, QRhiBuffer *buf)
{
    QRhiShaderResourceBinding b;
    QRhiShaderResourceBindingPrivate *d = QRhiShaderResourceBindingPrivate::get(&b);
    Q_ASSERT(d->ref.load() == 1);
    d->binding = binding;
    d->stage = stage;
    d->type = UniformBuffer;
    d->u.ubuf.buf = buf;
    d->u.ubuf.offset = 0;
    d->u.ubuf.maybeSize = 0; // entire buffer
    d->u.ubuf.hasDynamicOffset = false;
    return b;
}

/*!
    \return a shader resource binding for the given binding number, pipeline
    stages, and buffer specified by \a binding, \a stage, and \a buf. This
    overload binds a region only, as specified by \a offset and \a size.

    \note It is up to the user to ensure the offset is aligned to
    QRhi::ubufAlignment().
 */
QRhiShaderResourceBinding QRhiShaderResourceBinding::uniformBuffer(
        int binding, StageFlags stage, QRhiBuffer *buf, int offset, int size)
{
    Q_ASSERT(size > 0);
    QRhiShaderResourceBinding b;
    QRhiShaderResourceBindingPrivate *d = QRhiShaderResourceBindingPrivate::get(&b);
    Q_ASSERT(d->ref.load() == 1);
    d->binding = binding;
    d->stage = stage;
    d->type = UniformBuffer;
    d->u.ubuf.buf = buf;
    d->u.ubuf.offset = offset;
    d->u.ubuf.maybeSize = size;
    d->u.ubuf.hasDynamicOffset = false;
    return b;
}

/*!
    \return a shader resource binding for the given binding number, pipeline
    stages, and buffer specified by \a binding, \a stage, and \a buf. The
    uniform buffer is assumed to have dynamic offset. The dynamic offset can be
    specified in QRhiCommandBuffer::setShaderResources(), thus allowing using
    varying offset values without creating new bindings for the buffer. The
    size of the bound region is specified by \a size. Like with non-dynamic
    offsets, \c{offset + size} cannot exceed the size of \a buf.
 */
QRhiShaderResourceBinding QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
        int binding, StageFlags stage, QRhiBuffer *buf, int size)
{
    QRhiShaderResourceBinding b;
    QRhiShaderResourceBindingPrivate *d = QRhiShaderResourceBindingPrivate::get(&b);
    Q_ASSERT(d->ref.load() == 1);
    d->binding = binding;
    d->stage = stage;
    d->type = UniformBuffer;
    d->u.ubuf.buf = buf;
    d->u.ubuf.offset = 0;
    d->u.ubuf.maybeSize = size;
    d->u.ubuf.hasDynamicOffset = true;
    return b;
}

/*!
    \return a shader resource binding for the given binding number, pipeline
    stages, texture, and sampler specified by \a binding, \a stage, \a tex,
    \a sampler.
 */
QRhiShaderResourceBinding QRhiShaderResourceBinding::sampledTexture(
        int binding, StageFlags stage, QRhiTexture *tex, QRhiSampler *sampler)
{
    QRhiShaderResourceBinding b;
    QRhiShaderResourceBindingPrivate *d = QRhiShaderResourceBindingPrivate::get(&b);
    Q_ASSERT(d->ref.load() == 1);
    d->binding = binding;
    d->stage = stage;
    d->type = SampledTexture;
    d->u.stex.tex = tex;
    d->u.stex.sampler = sampler;
    return b;
}

/*!
    \return \c true if the contents of the two QRhiShaderResourceBinding
    objects \a a and \a b are equal. This includes the resources (buffer,
    texture) and related parameters (offset, size) as well. To only compare
    layouts (binding point, pipeline stage, resource type), use
    \l{QRhiShaderResourceBinding::isLayoutCompatible()}{isLayoutCompatible()}
    instead.

    \relates QRhiShaderResourceBinding
 */
bool operator==(const QRhiShaderResourceBinding &a, const QRhiShaderResourceBinding &b) Q_DECL_NOTHROW
{
    if (a.d == b.d)
        return true;

    if (a.d->binding != b.d->binding
            || a.d->stage != b.d->stage
            || a.d->type != b.d->type)
    {
        return false;
    }

    switch (a.d->type) {
    case QRhiShaderResourceBinding::UniformBuffer:
        if (a.d->u.ubuf.buf != b.d->u.ubuf.buf
                || a.d->u.ubuf.offset != b.d->u.ubuf.offset
                || a.d->u.ubuf.maybeSize != b.d->u.ubuf.maybeSize)
        {
            return false;
        }
        break;
    case QRhiShaderResourceBinding::SampledTexture:
        if (a.d->u.stex.tex != b.d->u.stex.tex
                || a.d->u.stex.sampler != b.d->u.stex.sampler)
        {
            return false;
        }
        break;
    default:
        Q_UNREACHABLE();
        return false;
    }

    return true;
}

/*!
    \return \c false if all the bindings in the two QRhiShaderResourceBinding
    objects \a a and \a b are equal; otherwise returns \c true.

    \relates QRhiShaderResourceBinding
 */
bool operator!=(const QRhiShaderResourceBinding &a, const QRhiShaderResourceBinding &b) Q_DECL_NOTHROW
{
    return !(a == b);
}

/*!
    \return the hash value for \a b, using \a seed to seed the calculation.

    \relates QRhiShaderResourceBinding
 */
uint qHash(const QRhiShaderResourceBinding &b, uint seed) Q_DECL_NOTHROW
{
    const char *u = reinterpret_cast<const char *>(&b.d->u);
    return seed + b.d->binding + 10 * b.d->stage + 100 * b.d->type
            + qHash(QByteArray::fromRawData(u, sizeof(b.d->u)), seed);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiShaderResourceBinding &b)
{
    const QRhiShaderResourceBindingPrivate *d = b.d;
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QRhiShaderResourceBinding("
                  << "binding=" << d->binding
                  << " stage=" << d->stage
                  << " type=" << d->type;
    switch (d->type) {
    case QRhiShaderResourceBinding::UniformBuffer:
        dbg.nospace() << " UniformBuffer("
                      << "buffer=" << d->u.ubuf.buf
                      << " offset=" << d->u.ubuf.offset
                      << " maybeSize=" << d->u.ubuf.maybeSize
                      << ')';
        break;
    case QRhiShaderResourceBinding::SampledTexture:
        dbg.nospace() << " SampledTexture("
                      << "texture=" << d->u.stex.tex
                      << " sampler=" << d->u.stex.sampler
                      << ')';
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    dbg.nospace() << ')';
    return dbg;
}
#endif

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QRhiShaderResourceBindings &srb)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QRhiShaderResourceBindings("
                  << srb.m_bindings
                  << ')';
    return dbg;
}
#endif

/*!
    \class QRhiGraphicsPipeline
    \inmodule QtRhi
    \brief Graphics pipeline state resource.

    \note Setting the shader resource bindings is mandatory. The referenced
    QRhiShaderResourceBindings must already be built by the time build() is
    called.

    \note Setting the render pass descriptor is mandatory. To obtain a
    QRhiRenderPassDescriptor that can be passed to setRenderPassDescriptor(),
    use either QRhiTextureRenderTarget::newCompatibleRenderPassDescriptor() or
    QRhiSwapChain::newCompatibleRenderPassDescriptor().

    \note Setting the vertex input layout is mandatory.

    \note Setting the shader stages is mandatory.

    \note sampleCount() defaults to 1 and must match the sample count of the
    render target's color and depth stencil attachments.

    \note The depth test, depth write, and stencil test are disabled by
    default.

    \note stencilReadMask() and stencilWriteMask() apply to both faces. They
    both default to 0xFF.
 */

/*!
    \fn void QRhiGraphicsPipeline::setTargetBlends(const QVector<TargetBlend> &blends)

    Sets the blend specification for color attachments. Each element in \a
    blends corresponds to a color attachment of the render target.

    By default no blends are set, which is a shortcut to disabling blending and
    enabling color write for all four channels.
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
 */

/*!
    \class QRhiGraphicsPipeline::StencilOpState
    \inmodule QtRhi
    \brief Describes the stencil operation state.
 */

/*!
    \internal
 */
QRhiGraphicsPipeline::QRhiGraphicsPipeline(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

/*!
    \fn bool QRhiGraphicsPipeline::build()

    Creates the corresponding native graphics resources. If there are already
    resources present due to an earlier build() with no corresponding
    release(), then release() is called implicitly first.

    \return \c true when successful, \c false when a graphics operation failed.
    Regardless of the return value, calling release() is always safe.
 */

/*!
    \class QRhiSwapChain
    \inmodule QtRhi
    \brief Swapchain resource.

    A swapchain enables presenting rendering results to a surface. A swapchain
    is typically backed by a set of color buffers. Of these, one is displayed
    at a time.

    Below is a typical pattern for creating and managing a swapchain and some
    associated resources in order to render onto a QWindow:

    \badcode
      void init()
      {
          sc = rhi->newSwapChain();
          ds = rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                    QSize(), // no need to set the size yet
                                    1,
                                    QRhiRenderBuffer::UsedWithSwapChainOnly);
          sc->setWindow(window);
          sc->setDepthStencil(ds);
          rp = sc->newCompatibleRenderPassDescriptor();
          sc->setRenderPassDescriptor(rp);
          resizeSwapChain();
      }

      void resizeSwapChain()
      {
          const QSize outputSize = sc->surfacePixelSize();
          ds->setPixelSize(outputSize);
          ds->build();
          hasSwapChain = sc->buildOrResize();
      }

      void render()
      {
          if (!hasSwapChain || notExposed)
              return;

          if (sc->currentPixelSize() != sc->surfacePixelSize() || newlyExposed) {
              resizeSwapChain();
              if (!hasSwapChain)
                  return;
              newlyExposed = false;
          }

          rhi->beginFrame(sc);
          // ...
          rhi->endFrame(sc);
      }
    \endcode

    Avoid relying on QWindow resize events to resize swapchains, especially
    considering that surface sizes may not always fully match the QWindow
    reported dimensions. The safe, cross-platform approach is to do the check
    via surfacePixelSize() whenever starting a new frame.

    Releasing the swapchain must happen while the QWindow and the underlying
    native window is fully up and running. Building on the previous example:

    \badcode
        void releaseSwapChain()
        {
            sc->release();
            hasSwapChain = false;
        }

        // assuming Window is our QWindow subclass
        bool Window::event(QEvent *e)
        {
            switch (e->type()) {
            case QEvent::UpdateRequest: // for QWindow::requestUpdate()
                render();
                break;
            case QEvent::PlatformSurface:
                if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
                    releaseSwapChain();
                break;
            default:
                break;
            }
            return QWindow::event(e);
        }
    \endcode

    Initializing the swapchain and starting to render the first frame cannot
    start at any time. The safe, cross-platform approach is to rely on expose
    events. QExposeEvent is a loosely specified event that is sent whenever a
    window gets mapped, obscured, and resized, depending on the platform.

    \badcode
        void Window::exposeEvent(QExposeEvent *)
        {
            // initialize and start rendering when the window becomes usable for graphics purposes
            if (isExposed() && !running) {
                running = true;
                init();
                render();
            }

            // stop pushing frames when not exposed or size becomes 0
            if ((!isExposed() || (hasSwapChain && sc->surfacePixelSize().isEmpty())) && running)
                notExposed = true;

            // continue when exposed again and the surface has a valid size
            if (isExposed() && running && notExposed && !sc->surfacePixelSize().isEmpty()) {
                notExposed = false;
                newlyExposed = true;
                render();
            }
        }
    \endcode

    Once the rendering has started, a simple way to request a new frame is
    QWindow::requestUpdate(). While on some platforms this is merely a small
    timer, on others it has a specific implementation: for instance on macOS or
    iOS it may be backed by
    \l{https://developer.apple.com/documentation/corevideo/cvdisplaylink?language=objc}{CVDisplayLink}.
    The example above is already prepared for update requests by handling
    QEvent::UpdateRequest.

    While acting as a QRhiRenderTarget, QRhiSwapChain also manages a
    QRhiCommandBuffer. Calling QRhi::endFrame() submits the recorded commands
    and also enqueues a \c present request. The default behavior is to do this
    with a swap interval of 1, meaning synchronizing to the display's vertical
    refresh is enabled. Thus the rendering thread calling beginFrame() and
    endFrame() will get throttled to vsync. On some backends this can be
    disabled by passing QRhiSwapChain:NoVSync in flags().

    Multisampling (MSAA) is handled transparently to the applications when
    requested via setSampleCount(). Where applicable, QRhiSwapChain will take
    care of creating additional color buffers and issuing a multisample resolve
    command at the end of a frame. For OpenGL, it is necessary to request the
    appropriate sample count also via QSurfaceFormat, by calling
    QSurfaceFormat::setDefaultFormat() before initializing the QRhi.
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
    request altogether. For OpenGL, try instead setting the swap interval to 0
    on the QWindow via QSurfaceFormat::setSwapInterval().
 */

/*!
    \internal
 */
QRhiSwapChain::QRhiSwapChain(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

/*!
    \fn QObject *QRhiSwapChain::target() const
    \internal
 */

/*!
    \fn void QRhiSwapChain::setTarget(QObject *obj)
    \internal
 */

/*!
    \fn QSize QRhiSwapChain::currentPixelSize() const

    \return the size with which the swapchain was last successfully built. Use
    this to decide if buildOrResize() needs to be called again: if
    \c{currentPixelSize() != surfacePixelSize()} then the swapchain needs to be
    resized.

    \sa surfacePixelSize()
  */

/*!
    \fn QSize QRhiSwapChain::surfacePixelSize()

    \return The size of the window's associated surface or layer. Do not assume
    this is the same as QWindow::size() * QWindow::devicePixelRatio().

    Can be called before buildOrResize() (but with window() already set), which
    allows setting the correct size for the depth-stencil buffer that is then
    used together with the swapchain's color buffers. Also used in combination
    with currentPixelSize() to detect size changes.

    \sa currentPixelSize()
  */

/*!
    \fn QRhiCommandBuffer *QRhiSwapChain::currentFrameCommandBuffer()

    \return a command buffer on which rendering commands can be recorded. Only
    valid within a QRhi::beginFrame() - QRhi::endFrame() block where
    beginFrame() was called with this swapchain.

    \note the value must not be cached and reused between frames
*/

/*!
    \fn QRhiRenderTarget *QRhiSwapChain::currentFrameRenderTarget()

    \return a render target that can used with beginPass() in order to render
    the the swapchain's current backbuffer. Only valid within a
    QRhi::beginFrame() - QRhi::endFrame() block where beginFrame() was called
    with this swapchain.

    \note the value must not be cached and reused between frames
 */

/*!
    \fn bool QRhiSwapChain::buildOrResize()

    Creates the swapchain if not already done and resizes the swapchain buffers
    to match the current size of the targeted surface. Call this whenever the
    size of the target surface is different than before.

    \note call release() only when the swapchain needs to be released
    completely, typically upon
    QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed. To perform resizing, just
    call buildOrResize().

    \return \c true when successful, \c false when a graphics operation failed.
    Regardless of the return value, calling release() is always safe.
 */

/*!
    \class QRhiCommandBuffer
    \inmodule QtRhi
    \brief Command buffer resource.

    Not creatable by applications at the moment. The only ways to obtain a
    valid QRhiCommandBuffer are to get it from the targeted swapchain via
    QRhiSwapChain::currentFrameCommandBuffer(), or, in case of rendering
    completely offscreen, initializing one via QRhi::beginOffscreenFrame().
 */

/*!
    \enum QRhiCommandBuffer::IndexFormat
    Specifies the index data type

    \value IndexUInt16 Unsigned 16-bit (quint16)
    \value IndexUInt32 Unsigned 32-bit (quint32)
 */

/*!
    \typedef QRhiCommandBuffer::DynamicOffset

    Synonym for QPair<int, quint32>. The first entry is the binding, the second
    is the offset in the buffer.
*/

/*!
    \typedef QRhiCommandBuffer::VertexInput

    Synonym for QPair<QRhiBuffer *, quint32>. The second entry is an offset in
    the buffer specified by the first.
*/

/*!
    \internal
 */
QRhiCommandBuffer::QRhiCommandBuffer(QRhiImplementation *rhi)
    : QRhiResource(rhi)
{
}

QRhiImplementation::~QRhiImplementation()
{
    qDeleteAll(resUpdPool);

    if (rsh) {
        for (QRhiResource *res : qAsConst(resources)) {
            if (res->isShareable()) {
                res->orphanedWithRsh = rsh;
            } else {
                qWarning("QRhi %p going down orphaning an unreleased, non-shareable resource %p (%s). This is bad.",
                         q, res, res->objectName.constData());
            }
            res->rhi = nullptr;
        }
    } else {
        // Be nice and show something about leaked stuff. Though we may not get
        // this far with some backends where the allocator or the api may check
        // and freak out for unfreed graphics objects in the derived dtor already.
#ifndef QT_NO_DEBUG
        if (!resources.isEmpty()) {
            qWarning("QRhi %p going down with %d unreleased resources and no QRhiResourceSharingHost. This is bad.",
                     q, resources.count());
            for (QRhiResource *res : qAsConst(resources)) {
                qWarning("  Resource %p (%s)", res, res->objectName.constData());
                res->rhi = nullptr;
            }
        }
#endif
    }
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
    \class QRhiResourceSharingHost
    \inmodule QtRhi
    \brief Enables resource sharing and synchronization between QRhi instances.

    When a QRhi is created with QRhiInitParams::resourceSharingHost set, it
    causes the QRhi to share either the graphics device, or, in case of OpenGL,
    use a context that shares resources, with all other QRhi instances that
    have the same QRhiResourceSharingHost set in their QRhiInitParams.

    Sharing the same device means that when two or more QRhi instances have the
    same QhiResourceSharingHost set, there will only be one native graphics
    device object, such as, \c VkDevice, \c MTLDevice, or \c ID3D11Device,
    created, and that device will be available as long as any of the associated
    QRhi instances are alive, thus avoiding lifetime and ownership issues.

    This makes the underlying graphics resources of QRhiResource subclasses
    such as QRhiTexture available to all the QRhi instances that use the same
    QRhiResourceSharingHost. This applies only to QRhiResource instances that
    report \c true from \l{QRhiResource::isShareable()}{isShareable()}.

    In order to avoid lifetime management issues with shared resources, a
    sharaeble QRhiResource is allowed to be orphaned, meaning the QRhi the
    resource was created from can be destroyed while keeping the QRhiResource
    usable (although operations like \c build() are not allowed anymore then).
    Applications can thus postpone calling
    \l{QRhiResource::release()}{release()} on the resource as long as there is
    at least one QRhi associated with the QRhiResourceSharingHost that was
    associated with the resource's creator QRhi.

    \note The QRhiResourceSharingHost does not perform graphics resource
    operations on its own. Therefore it is important that all relevant
    QRhiResource instances are released before the last associated QRhi is
    destroyed. Attempting to release a QRhiResource after that will lead to
    graphics resource leaks (assuming the resource sharing host gets destroyed
    then; if there are new QRhi instances created for the same resource sharing
    host later on then there is no issue, as long as, again, those are alive).

    \note The QRhiResourceSharingHost can be created on a thread that is
    different than the threads on which the associated QRhi instances will be
    created. It is however up to the application to organize those threads in a
    way that the QRhiResourceSharingHost is only destroyed after all associated
    QRhi instances have been fully destroyed.

    \note Having a QRhiResourceSharingHost associated with a QRhi triggers
    mutex-based synchronization for native objects like the graphics queue or
    device context, with backends where applicable. This allows having two or
    more QRhi instances on two or more different threads with the same
    QRhiResourceSharingHost and so sharing the same device, queue, or device
    context.

    \note Resource sharing increases the chance of contention and inconsistent
    frame rate due to sharing the same device and possibly command queue, and
    due to the need for synchronization inside various QRhi calls. The results
    can also vary based on the implementation of the underlying graphics API,
    with potentially different results from different vendor's drivers or
    different driver versions. Applications and libraries are advised to
    consider offering resource sharing as an opt-in feature, combined with a
    design that allows operating with fully independent QRhi instances as well.

    To illustrate resource sharing, take the following code snippets.

    In many cases an application will use a single QRhi. Resources created from it
    must be always released before destroying the rhi:

    \badcode
        QRhi *rhi = QRhi::create(initParams);
        QRhiTexture *tex = rhi->newTexture(...);
        ... use tex with rhi
        tex->releaseAndDestroy();
        delete rhi;
    \endcode

    Let's introduce a second QRhi:

    \badcode
        QRhi *rhi = QRhi::create(initParams);
        QRhi *rhi2 = QRhi::create(initParams);
        QRhiTexture *tex = rhi->newTexture(...);
        ... use tex with rhi
        tex->releaseAndDestroy();
        delete rhi;
        delete rhi2;
    \endcode

    Here using \c tex with \c rhi2 is forbidden. (although it may work with some backends)
    Let's make it work:

    \badcode
        QRhiResourceSharingHost *rsh = new QRhiResourceSharingHost;
        initParams.resourceSharingHost = rsh;
        QRhi *rhi = QRhi::create(initParams);
        QRhi *rhi2 = QRhi::create(initParams);
        QRhiTexture *tex = rhi->newTexture(...);
        if (!tex->isShareable()) { error("not supported"); }
        ... use tex with rhi or rhi2
        tex->releaseAndDestroy();
        delete rhi;
        delete rhi2;
        delete rsh;
    \endcode

    Now \c tex is usable with \c rhi2 as well.

    What if \c rhi is destroyed before \c rhi2?

    \badcode
        QRhiResourceSharingHost *rsh = new QRhiResourceSharingHost;
        initParams.resourceSharingHost = rsh;
        QRhi *rhi = QRhi::create(initParams);
        QRhi *rhi2 = QRhi::create(initParams);
        QRhiTexture *tex = rhi->newTexture(...);
        if (!tex->isShareable()) { error("not supported"); }
        ... use tex with rhi or rhi2
        delete rhi;
        ... use tex with rhi2
        tex->releaseAndDestroy();
        delete rhi2;
        delete rsh;
    \endcode

    This is valid as well. After the \c{delete rhi} statement \c tex is in an
    orphaned state so calling \c build() on it for example would fail. On the
    other hand, it is still valid to use \c{tex} as it is for rendering, with
    its existing underlying native texture.

    \note Moving the \c{tex->releaseAndDestroy()} call between the \c{delete
    rhi2} and \c{delete rsh} statements would be incorrect.

    \sa QRhiResource::isShareable(), QRhiInitParams, QRhi::create()
 */

/*!
    Constructs a new QRhiResourceSharingHost.
 */
QRhiResourceSharingHost::QRhiResourceSharingHost()
    : d(new QRhiResourceSharingHostPrivate)
{
}

/*!
    Destructor.

    \note Destroying a QRhiResourceSharingHost before all the associated QRhi
    instances are destroyed is not allowed and will lead to undefined behavior.
 */
QRhiResourceSharingHost::~QRhiResourceSharingHost()
{
    delete d;
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
QRhi *QRhi::create(Implementation impl, QRhiInitParams *params, Flags flags, QRhiNativeHandles *importDevice)
{
    QScopedPointer<QRhi> r(new QRhi);

    switch (impl) {
    case Null:
        r->d = new QRhiNull(static_cast<QRhiNullInitParams *>(params));
        break;
    case Vulkan:
#if QT_CONFIG(vulkan)
        r->d = new QRhiVulkan(static_cast<QRhiVulkanInitParams *>(params),
                              static_cast<QRhiVulkanNativeHandles *>(importDevice));
        break;
#else
        qWarning("This build of Qt has no Vulkan support");
        break;
#endif
    case OpenGLES2:
#ifndef QT_NO_OPENGL
        r->d = new QRhiGles2(static_cast<QRhiGles2InitParams *>(params),
                             static_cast<QRhiGles2NativeHandles *>(importDevice));
        break;
#else
        qWarning("This build of Qt has no OpenGL support");
        break;
#endif
    case D3D11:
#ifdef Q_OS_WIN
        r->d = new QRhiD3D11(static_cast<QRhiD3D11InitParams *>(params),
                             static_cast<QRhiD3D11NativeHandles *>(importDevice));
        break;
#else
        qWarning("This platform has no Direct3D 11 support");
        break;
#endif
    case Metal:
#ifdef Q_OS_DARWIN
        r->d = new QRhiMetal(static_cast<QRhiMetalInitParams *>(params),
                             static_cast<QRhiMetalNativeHandles *>(importDevice));
        break;
#else
        qWarning("This platform has no Metal support");
        break;
#endif
    default:
        break;
    }

    if (r->d) {
        r->d->q = r.data();
        if (flags.testFlag(EnableProfiling)) {
            QRhiProfilerPrivate *profD = QRhiProfilerPrivate::get(&r->d->profiler);
            profD->rhiDWhenEnabled = r->d;
        }
        r->d->debugMarkers = flags.testFlag(EnableDebugMarkers);
        if (r->d->create(flags))
            return r.take();
    }

    return nullptr;
}

/*!
    \class QRhiResourceUpdateBatch
    \inmodule QtRhi
    \brief Records upload and copy type of operations.

    With QRhi it is no longer possible to perform copy type of operations at
    arbitrary times. Instead, all such operations are recorded into batches
    that are then passed, most commonly, to QRhiCommandBuffer::beginPass().
    What then happens under the hood is hidden from the application: the
    underlying implementations can defer and implement these operations in
    various different ways.

    A resource update batch owns no graphics resources and does not perform any
    actual operations on its own. It should rather be viewed as a command
    buffer for update, upload, and copy type of commands.

    To get an available, empty batch from the pool, call
    QRhi::nextResourceUpdateBatch().
 */

/*!
    \internal
 */
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

/*!
    \return the batch to the pool. This should only be used when the batch is
    not passed to one of QRhiCommandBuffer::beginPass(),
    QRhiCommandBuffer::endPass(), or QRhiCommandBuffer::resourceUpdate()
    because these implicitly call release().

    \note QRhiResourceUpdateBatch instances must never by \c deleted by
    applications.
 */
void QRhiResourceUpdateBatch::release()
{
    d->free();
}

/*!
    Copies all queued operations from the \a other batch into this one.

    \note \a other is not changed in any way, typically it will still need a
    release()

    This allows for a convenient pattern where resource updates that are
    already known during the initialization step are collected into a batch
    that is then merged into another when starting to first render pass later
    on:

    \badcode
    void init()
    {
        ...
        initialUpdates = rhi->nextResourceUpdateBatch();
        initialUpdates->uploadStaticBuffer(vbuf, vertexData);
        initialUpdates->uploadStaticBuffer(ibuf, indexData);
        ...
    }

    void render()
    {
        ...
        QRhiResourceUpdateBatch *resUpdates = rhi->nextResourceUpdateBatch();
        if (initialUpdates) {
            resUpdates->merge(initialUpdates);
            initialUpdates->release();
            initialUpdates = nullptr;
        }
        resUpdates->updateDynamicBuffer(...);
        ...
        cb->beginPass(rt, clearCol, clearDs, resUpdates);
    }
    \endcode
 */
void QRhiResourceUpdateBatch::merge(QRhiResourceUpdateBatch *other)
{
    d->merge(other->d);
}

/*!
    Enqueues updating a region of a QRhiBuffer \a buf created with the type
    QRhiBuffer::Dynamic.

    The region is specified \a offset and \a size. The actual bytes to write
    are specified by \a data which must have at least \a size bytes available.
    \a data can safely be destroyed or changed once this function returns since
    it takes a copy internally as necessary.

    \note If host writes are involved, which is the case with
    updateDynamicBuffer() typically as such buffers are backed by host visible
    memory with most backends, they may accumulate within a frame. Thus pass 1
    reading a region changed by a batch passed to pass 2 may see the changes
    specified in pass 2's update batch.

    \note QRhi transparently manages double buffering in order to prevent
    stalling the graphics pipeline. The fact that a QRhiBuffer may have
    multiple native underneath can be safely ignored when using the QRhi and
    QRhiResourceUpdateBatch.
 */
void QRhiResourceUpdateBatch::updateDynamicBuffer(QRhiBuffer *buf, int offset, int size, const void *data)
{
    if (size > 0)
        d->dynamicBufferUpdates.append({ buf, offset, size, data });
}

/*!
    Enqueues updating a region of a QRhiBuffer \a buf created with the type
    QRhiBuffer::Immutable or QRhiBuffer::Static.

    The region is specified \a offset and \a size. The actual bytes to write
    are specified by \a data which must have at least \a size bytes available.
    \a data can safely be destroyed or changed once this function returns since
    it takes a copy internally as necessary.
 */
void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, int offset, int size, const void *data)
{
    if (size > 0)
        d->staticBufferUploads.append({ buf, offset, size, data });
}

/*!
    Enqueues updating the entire QRhiBuffer \a buf created with the type
    QRhiBuffer::Immutable or QRhiBuffer::Static.
 */
void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, const void *data)
{
    if (buf->size() > 0)
        d->staticBufferUploads.append({ buf, 0, 0, data });
}

/*!
    Enqueues uploading the image data for one or more mip levels in one or more
    layers of the texture \a tex.

    The details of the copy (source QImage or compressed texture data, regions,
    target layers and levels) are described in \a desc.
 */
void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QRhiTextureUploadDescription &desc)
{
    d->textureUploads.append({ tex, desc });
}

/*!
    Enqueues uploading the image data for mip level 0 of layer 0 of the texture
    \a tex.

    \a tex must have an uncompressed format. Its format must also be compatible
    with the QImage::format() of \a image. The source data is given in \a
    image.
 */
void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QImage &image)
{
    uploadTexture(tex, {{{{{ image }}}}});
}

/*!
   Enqueues a texture-to-texture copy operation from \a src into \a dst as
   described by \a desc.

   \note The source texture \a src must be created with
   QRhiTexture::UsedAsTransferSource.
 */
void QRhiResourceUpdateBatch::copyTexture(QRhiTexture *dst, QRhiTexture *src, const QRhiTextureCopyDescription &desc)
{
    d->textureCopies.append({ dst, src, desc });
}

/*!
   Enqueues a texture-to-host copy operation as described by \a rb.

   Normally \a rb will specify a QRhiTexture as the source. However, when the
   swapchain in the current frame was created with
   QRhiSwapChain::UsedAsTransferSource, it can also be the source of the
   readback. For this, leave the texture set to null in \a rb.

   Unlike other operations, the results here need to be processed by the
   application. Therefore, \a result provides not just the data but also a
   callback as operations on the batch are asynchronous by nature:

   \badcode
      beginFrame(sc);
      beginPass
      ...
      QRhiReadbackResult *rbResult = new QRhiReadbackResult;
      rbResult->completed = [rbResult] {
          {
              QImage::Format fmt = rbResult->format == QRhiTexture::BGRA8 ? QImage::Format_ARGB32_Premultiplied
                                                                          : QImage::Format_RGBA8888_Premultiplied;
              const uchar *p = reinterpret_cast<const uchar *>(rbResult->data.constData());
              QImage image(p, rbResult->pixelSize.width(), rbResult->pixelSize.height(), fmt);
              image.save("result.png");
          }
          delete rbResult;
      };
      u = nextResourceUpdateBatch();
      QRhiReadbackDescription rb; // no texture -> uses the current backbuffer of sc
      u->readBackTexture(rb, rbResult);
      endPass(u);
      endFrame(sc);
   \endcode

   \note The texture must be created with QRhiTexture::UsedAsTransferSource.

   \note Multisample textures cannot be read back.
 */
void QRhiResourceUpdateBatch::readBackTexture(const QRhiReadbackDescription &rb, QRhiReadbackResult *result)
{
    d->textureReadbacks.append({ rb, result });
}

/*!
   Enqueues a mipmap generation operation for the texture \a tex.

   \note The texture must be created with QRhiTexture::MipMapped and
   QRhiTexture::UsedWithGenerateMips.
 */
void QRhiResourceUpdateBatch::generateMips(QRhiTexture *tex)
{
    d->textureMipGens.append(QRhiResourceUpdateBatchPrivate::TextureMipGen(tex));
}

/*!
   \return an available, empty batch to which copy type of operations can be
   recorded.

   \note the return value is not owned by the caller and must never be
   destroyed. Instead, the batch is returned the the pool for reuse by passing
   it to QRhiCommandBuffer::beginPass(), QRhiCommandBuffer::endPass(), or
   QRhiCommandBuffer::resourceUpdate(), or by calling
   QRhiResourceUpdateBatch::release() on it.

   \note Can be called outside beginFrame() - endFrame() as well since a batch
   instance just collects data on its own, it does not perform any operations.
 */
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
}

/*!
    Sometimes committing resource updates is necessary without starting a
    render pass. Not often needed, updates should typically be passed to
    beginPass (or endPass, in case of readbacks) instead.

    \note Cannot be called inside a pass.
 */
void QRhiCommandBuffer::resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->resourceUpdate(this, resourceUpdates);
}

/*!
    Records starting a new render pass targeting the render target \a rt.

    \a resourceUpdates, when not null, specifies a resource update batch that
    is to be committed and then released.

    The color and depth/stencil buffers of the render target are normally
    cleared. The clear values are specified in \a colorClearValue and \a
    depthStencilClearValue. The exception is when the render target was created
    with QRhiTextureRenderTarget::PreserveColorContents and/or
    QRhiTextureRenderTarget::PreserveDepthStencilContents. The clear values are
    ignored then.

    \note Enabling preserved color or depth contents leads to decreased
    performance depending on the underlying hardware. Mobile GPUs with tiled
    architecture benefit from not having to reload the previous contents into
    the tile buffer. Similarly, a QRhiTextureRenderTarget with a QRhiTexture as
    the depth buffer is less efficient than a QRhiRenderBuffer since using a
    depth texture triggers requiring writing the data out to it, while with
    renderbuffers this is not needed (as the API does not allow sampling or
    reading from a renderbuffer).

    \note Do not assume that any state or resource bindings persist between
    passes.

    \note The QRhiCommandBuffer's \c set and \c draw functions can only be
    called inside a pass. Also, with the exception of setGraphicsPipeline(),
    they expect to have a pipeline set already on the command buffer.
    Unspecified issues may arise otherwise, depending on the backend.
 */
void QRhiCommandBuffer::beginPass(QRhiRenderTarget *rt,
                                  const QRhiColorClearValue &colorClearValue,
                                  const QRhiDepthStencilClearValue &depthStencilClearValue,
                                  QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->beginPass(this, rt, colorClearValue, depthStencilClearValue, resourceUpdates);
}

/*!
    Records ending the current render pass.

    \a resourceUpdates, when not null, specifies a resource update batch that
    is to be committed and then released.
 */
void QRhiCommandBuffer::endPass(QRhiResourceUpdateBatch *resourceUpdates)
{
    rhi->endPass(this, resourceUpdates);
}

/*!
    Records setting a new graphics pipeline \a ps.

    \note This function must be called before recording other \c set or \c draw
    commands on the command buffer.

    \note QRhi will optimize out unneccessary invocations within a pass, so
    therefore overoptimizing to avoid calls to this function is not necessary
    on the applications' side.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.
 */
void QRhiCommandBuffer::setGraphicsPipeline(QRhiGraphicsPipeline *ps)
{
    rhi->setGraphicsPipeline(this, ps);
}

/*!
    Records binding a set of shader resources, such as, uniform buffers or
    textures, that are made visible to one or more shader stages.

    \a srb can be null in which case the current graphics pipeline's associated
    QRhiGraphicsPipeline::shaderResourceBindings() is used. When \a srb is
    non-null, it must be
    \l{QRhiShaderResourceBindings::isLayoutCompatible()}{layout-compatible},
    meaning the layout (number of bindings, the type and binding number of each
    binding) must fully match the QRhiShaderResourceBindings that was
    associated with the pipeline at the time of calling
    QRhiGraphicsPipeline::build().

    There are cases when a seemingly unnecessary setShaderResources() call is
    mandatory: when rebuilding a resource referenced from \a srb, for example
    changing the size of a QRhiBuffer followed by a QRhiBuffer::build(), this
    is the place where associated native objects (such as descriptor sets in
    case of Vulkan) are updated to refer to the current native resources that
    back the QRhiBuffer, QRhiTexture, QRhiSampler objects referenced from \a
    srb. In this case setShaderResources() must be called even if \a srb is
    the same as in the last call.

    \a dynamicOffsets allows specifying buffer offsets for uniform buffers that
    were associated with \a srb via
    QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(). This is
    different from providing the offset in the \a srb itself: dynamic offsets
    do not require building a new QRhiShaderResourceBindings for every
    different offset, can avoid writing the underlying descriptors (with
    backends where applicable), and so they may be more efficient. Each element
    of \a dynamicOffsets is a \c binding - \c offset pair.

    \note All offsets in \a dynamicOffsets must be byte aligned to the value
    returned from QRhi::ubufAlignment().

    \note QRhi will optimize out unneccessary invocations within a pass (taking
    the conditions described above into account), so therefore overoptimizing
    to avoid calls to this function is not necessary on the applications' side.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.
 */
void QRhiCommandBuffer::setShaderResources(QRhiShaderResourceBindings *srb,
                                           const QVector<DynamicOffset> &dynamicOffsets)
{
    rhi->setShaderResources(this, srb, dynamicOffsets);
}

/*!
    Records vertex input bindings.

    The index buffer used by subsequent drawIndexed() commands is specified by
    \a indexBuf, \a indexOffset, and \a indexFormat. \a indexBuf can be set to
    null when indexed drawing is not needed.

    Vertex buffer bindings are batched. \a startBinding specifies the first
    binding number. The recorded command then binds each buffer from \a
    bindings to the binding point \c{startBinding + i} where \c i is the index
    in \a bindings. Each element in \a bindings specifies a QRhiBuffer and an
    offset.

    Superfluous vertex input and index changes in the same pass are ignored
    automatically with most backends and therefore applications do not need to
    overoptimize to avoid calls to this function.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.

    As a simple example, take a vertex shader with two inputs:

    \badcode
        layout(location = 0) in vec4 position;
        layout(location = 1) in vec3 color;
    \endcode

    and assume we have the data available in interleaved format, using only 2
    floats for position (so 5 floats per vertex: x, y, r, g, b). A QRhiGraphicsPipeline for
    this shader can then be created using the input layout:

    \badcode
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { 5 * sizeof(float) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float3, 2 * sizeof(float) }
        });
    \endcode

    Here there is one buffer binding (binding number 0), with two inputs
    referencing it. When recording the pass, once the pipeline is set, the
    vertex bindings can be specified simply like the following (using C++11
    initializer syntax), assuming vbuf is the QRhiBuffer with all the
    interleaved position+color data:

    \badcode
        cb->setVertexInput(0, { { vbuf, 0 } });
    \endcode
 */
void QRhiCommandBuffer::setVertexInput(int startBinding, const QVector<VertexInput> &bindings,
                                       QRhiBuffer *indexBuf, quint32 indexOffset,
                                       IndexFormat indexFormat)
{
    rhi->setVertexInput(this, startBinding, bindings, indexBuf, indexOffset, indexFormat);
}

/*!
    Records setting the active viewport rectangle specified in \a viewport.

    With backends where the underlying graphics API has scissoring always
    enabled, this function also sets the scissor to match the viewport whenever
    the active QRhiGraphicsPipeline does not have
    \l{QRhiGraphicsPipeline::UsesScissor}{UsesScissor} set.

    \note QRhi assumes OpenGL-style viewport coordinates, meaning x and y are
    bottom-left.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.
 */
void QRhiCommandBuffer::setViewport(const QRhiViewport &viewport)
{
    rhi->setViewport(this, viewport);
}

/*!
    Records setting the active scissor rectangle specified in \a scissor.

    This can only be called when the bound pipeline has
    \l{QRhiGraphicsPipeline::UsesScissor}{UsesScissor} set. When the flag is
    set on the active pipeline, this function must be called because scissor
    testing will get enabled and so a scissor rectangle must be provided.

    \note QRhi assumes OpenGL-style viewport coordinates, meaning x and y are
    bottom-left.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.
 */
void QRhiCommandBuffer::setScissor(const QRhiScissor &scissor)
{
    rhi->setScissor(this, scissor);
}

/*!
    Records setting the active blend constants to \a c.

    This can only be called when the bound pipeline has
    QRhiGraphicsPipeline::UsesBlendConstants set.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.
 */
void QRhiCommandBuffer::setBlendConstants(const QVector4D &c)
{
    rhi->setBlendConstants(this, c);
}

/*!
    Records setting the active stencil reference value to \a refValue.

    This can only be called when the bound pipeline has
    QRhiGraphicsPipeline::UsesStencilRef set.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.
 */
void QRhiCommandBuffer::setStencilRef(quint32 refValue)
{
    rhi->setStencilRef(this, refValue);
}

/*!
    Records a non-indexed draw.

    The number of vertices is specified in \a vertexCount. For instanced
    drawing set \a instanceCount to a value other than 1. \a firstVertex is
    the index of the first vertex to draw. \a firstInstance is the instance ID
    of the first instance to draw.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.
 */
void QRhiCommandBuffer::draw(quint32 vertexCount,
                             quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)
{
    rhi->draw(this, vertexCount, instanceCount, firstVertex, firstInstance);
}

/*!
    Records an indexed draw.

    The number of vertices is specified in \a indexCount. \a firstIndex is the
    base index. The effective offset in the index buffer is given by
    \c{indexOffset + firstIndex * n} where \c n is 2 or 4 depending on the
    index element type. \c indexOffset is specified in setVertexInput().

    \note The effective offset in the index buffer must be 4 byte aligned with
    some backends (for example, Metal). With these backends the
    \l{QRhi::NonFourAlignedEffectiveIndexBufferOffset}{NonFourAlignedEffectiveIndexBufferOffset}
    feature will be reported as not-supported.

    For instanced drawing set \a instanceCount to a value other than 1. \a
    firstInstance is the instance ID of the first instance to draw.

    \a vertexOffset is added to the vertex index.

    \note This function can only be called inside a pass, meaning between a
    beginPass() end endPass() call.
 */
void QRhiCommandBuffer::drawIndexed(quint32 indexCount,
                                    quint32 instanceCount, quint32 firstIndex,
                                    qint32 vertexOffset, quint32 firstInstance)
{
    rhi->drawIndexed(this, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

/*!
    Records a named debug group on the command buffer. This is shown in
    graphics debugging tools such as \l{https://renderdoc.org/}{RenderDoc} and
    \l{https://developer.apple.com/xcode/}{XCode}. The end of the grouping is
    indicated by debugMarkEnd().

    \note Ignored when QRhi::DebugMarkers are not supported or
    QRhi::EnableDebugMarkers is not set.

    \note Can be called anywhere within the frame, both inside and outside of passes.
 */
void QRhiCommandBuffer::debugMarkBegin(const QByteArray &name)
{
    rhi->debugMarkBegin(this, name);
}

/*!
    Records the end of a debug group.

    \note Ignored when QRhi::DebugMarkers are not supported or
    QRhi::EnableDebugMarkers is not set.

    \note Can be called anywhere within the frame, both inside and outside of passes.
 */
void QRhiCommandBuffer::debugMarkEnd()
{
    rhi->debugMarkEnd(this);
}

/*!
    Inserts a debug message \a msg into the command stream.

    \note Ignored when QRhi::DebugMarkers are not supported or
    QRhi::EnableDebugMarkers is not set.

    \note With some backends debugMarkMsg() is only supported inside a pass and
    is ignored when called outside a pass. With others it is recorded anywhere
    within the frame.
 */
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
    \return \c true if the underlying graphics API has the Y axis pointing up
    in framebuffers and images.

    In practice this is \c true for OpenGL only.
 */
bool QRhi::isYUpInFramebuffer() const
{
    return d->isYUpInFramebuffer();
}

/*!
    \return \c true if the underlying graphics API has the Y axis pointing up
    in its normalized device coordinate system.

    In practice this is \c false for Vulkan only.

    \note clipSpaceCorrMatrix() includes the corresponding adjustment (to make
    Y point up) in its returned matrix.
 */
bool QRhi::isYUpInNDC() const
{
    return d->isYUpInNDC();
}

/*!
    \return a matrix that can be used to allow applications keep using
    OpenGL-targeted vertex data and perspective projection matrices (such as,
    the ones generated by QMatrix4x4::perspective()), regardless of the
    backend. Once \c{this_matrix * mvp} is used instead of just \c mvp, vertex
    data with Y up and viewports with depth range 0 - 1 can be used without
    considering what backend and so graphics API is going to be used at run
    time.

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

/*!
    \return a new graphics pipeline resource.

    \sa QRhiResource::release(), QRhiResource::releaseAndDestroy()
 */
QRhiGraphicsPipeline *QRhi::newGraphicsPipeline()
{
    return d->createGraphicsPipeline();
}

/*!
    \return a new shader resource binding collection resource.

    \sa QRhiResource::release(), QRhiResource::releaseAndDestroy()
 */
QRhiShaderResourceBindings *QRhi::newShaderResourceBindings()
{
    return d->createShaderResourceBindings();
}

/*!
    \return a new buffer with the specified \a type, \a usage, and \a size.

    \note Some \a usage and \a type combinations may not be supported by all
    backends. See \l{QRhi::NonDynamicUniformBuffers}{the feature flags}.

    \sa QRhiResource::release(), QRhiResource::releaseAndDestroy()
 */
QRhiBuffer *QRhi::newBuffer(QRhiBuffer::Type type,
                            QRhiBuffer::UsageFlags usage,
                            int size)
{
    return d->createBuffer(type, usage, size);
}

/*!
    \return a new renderbuffer with the specified \a type, \a pixelSize, \a
    sampleCount, and \a flags.

    \sa QRhiResource::release(), QRhiResource::releaseAndDestroy()
 */
QRhiRenderBuffer *QRhi::newRenderBuffer(QRhiRenderBuffer::Type type,
                                        const QSize &pixelSize,
                                        int sampleCount,
                                        QRhiRenderBuffer::Flags flags)
{
    return d->createRenderBuffer(type, pixelSize, sampleCount, flags);
}

/*!
    \return a new texture with the specified \a format, \a pixelSize, \a
    sampleCount, and \a flags.

    \sa QRhiResource::release(), QRhiResource::releaseAndDestroy()
 */
QRhiTexture *QRhi::newTexture(QRhiTexture::Format format,
                              const QSize &pixelSize,
                              int sampleCount,
                              QRhiTexture::Flags flags)
{
    return d->createTexture(format, pixelSize, sampleCount, flags);
}

/*!
    \return a new sampler with the specified magnification filter \a magFilter,
    minification filter \a minFilter, mipmapping mode \a mipmapMpde, and S/T/R
    addressing modes \a u, \a v, and \a w.

    \sa QRhiResource::release(), QRhiResource::releaseAndDestroy()
 */
QRhiSampler *QRhi::newSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                              QRhiSampler::Filter mipmapMode,
                              QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w)
{
    return d->createSampler(magFilter, minFilter, mipmapMode, u, v, w);
}

/*!
    \return a new texture render target with color and depth/stencil
    attachments given in \a desc, and with the specified \a flags.

    \sa QRhiResource::release(), QRhiResource::releaseAndDestroy()
 */

QRhiTextureRenderTarget *QRhi::newTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                      QRhiTextureRenderTarget::Flags flags)
{
    return d->createTextureRenderTarget(desc, flags);
}

/*!
    \return a new swapchain.

    \sa QRhiResource::release(), QRhiSwapChain::buildOrResize()
 */
QRhiSwapChain *QRhi::newSwapChain()
{
    return d->createSwapChain();
}

/*!
    Starts a new frame targeting the next available buffer of \a swapChain.

    The high level pattern of rendering into a QWindow using a swapchain:

    \list

    \li Create a swapchain.

    \li Call QRhiSwapChain::buildOrResize() whenever the surface size is
    different than before.

    \li Call QRhiSwapChain::release() on
    QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed.

    \li Then on every frame:
    \badcode
       beginFrame(sc);
       updates = nextResourceUpdateBatch();
       updates->...
       QRhiCommandBuffer *cb = sc->currentFrameCommandBuffer();
       cb->beginPass(sc->currentFrameRenderTarget(), colorClear, dsClear, updates);
       ...
       cb->endPass();
       ... // more passes as neccessary
       endFrame(sc);
    \endcode

    \endlist

    \a flags is currently unused.

    \sa endFrame()
 */
QRhi::FrameOpResult QRhi::beginFrame(QRhiSwapChain *swapChain, BeginFrameFlags flags)
{
    return d->beginFrame(swapChain, flags);
}

/*!
    Ends, commits, and presents a frame that was started in the last
    beginFrame() on \a swapChain.

    Double (or triple) buffering is managed internally by the QRhiSwapChain and
    QRhi.

    \a flags can optionally be used to change the behavior in certain ways.
    Passing QRhi::SkipPresent skips queuing the Present command or calling
    swapBuffers.

    \sa beginFrame()
 */
QRhi::FrameOpResult QRhi::endFrame(QRhiSwapChain *swapChain, EndFrameFlags flags)
{
    return d->endFrame(swapChain, flags);
}

/*!
    Starts a new offscreen frame. Provides a command buffer suitable for
    recording rendering commands in \a cb.

    \note The QRhiCommandBuffer stored to *cb is not owned by the caller.

    Rendering without a swapchain is possible as well. The typical use case is
    to use it in completely offscreen applications, e.g. to generate image
    sequences by rendering and reading back without ever showing a window.

    Usage in on-screen applications (so beginFrame, endFrame,
    beginOffscreenFrame, endOffscreenFrame, beginFrame, ...) is possible too
    but it does reduce parallelism so it should be done only infrequently.

    Offscreen frames do not let the CPU - potentially - generate another frame
    while the GPU is still processing the previous one. This has the side
    effect that if readbacks are scheduled, the results are guaranteed to be
    available once endOffscreenFrame() returns. That is not the case with
    frames targeting a swapchain.

    The skeleton of rendering a frame without a swapchain and then reading the
    frame contents back could look like the following:

    \badcode
          QRhiReadbackResult rbResult;
          QRhiCommandBuffer *cb;
          beginOffscreenFrame(&cb);
          beginPass
          ...
          u = nextResourceUpdateBatch();
          u->readBackTexture(rb, &rbResult);
          endPass(u);
          endOffscreenFrame();
          // image data available in rbResult
   \endcode

   \sa endOffscreenFrame()
 */
QRhi::FrameOpResult QRhi::beginOffscreenFrame(QRhiCommandBuffer **cb)
{
    return d->beginOffscreenFrame(cb);
}

/*!
    Ends and waits for the offscreen frame.

    \sa beginOffscreenFrame()
 */
QRhi::FrameOpResult QRhi::endOffscreenFrame()
{
    return d->endOffscreenFrame();
}

/*!
    Waits for any work on the graphics queue (where applicable) to complete,
    then executes all deferred operations, like completing readbacks and
    resource releases. Can be called inside and outside of a frame, but not
    inside a pass. Inside a frame it implies submitting any work on the
    command buffer.

    \note Avoid this function. One case where it may be needed is when the
    results of an enqueued readback in a swapchain-based frame are needed at a
    fixed given point and so waiting for the results is desired.
 */
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
