/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt RHI module
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QRHI_P_H
#define QRHI_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qtrhiglobal_p.h"
#include "qrhi.h"
#include "qrhiprofiler_p.h"
#include <QBitArray>
#include <QAtomicInt>
#include <QAtomicInteger>

QT_BEGIN_NAMESPACE

#define QRHI_RES(t, x) static_cast<t *>(x)
#define QRHI_RES_RHI(t) t *rhiD = static_cast<t *>(m_rhi)
#define QRHI_PROF QRhiProfilerPrivate *rhiP = m_rhi->profilerPrivateOrNull()
#define QRHI_PROF_F(f) for (bool qrhip_enabled = rhiP != nullptr; qrhip_enabled; qrhip_enabled = false) rhiP->f

class QRhiReferenceRenderTarget : public QRhiRenderTarget
{
protected:
    QRhiReferenceRenderTarget(QRhiImplementation *rhi);
};

class QRhiImplementation
{
public:
    virtual ~QRhiImplementation();

    virtual bool create(QRhi::Flags flags) = 0;
    virtual void destroy() = 0;

    virtual QRhiGraphicsPipeline *createGraphicsPipeline() = 0;
    virtual QRhiShaderResourceBindings *createShaderResourceBindings() = 0;
    virtual QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                                     QRhiBuffer::UsageFlags usage,
                                     int size) = 0;
    virtual QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                                 const QSize &pixelSize,
                                                 int sampleCount,
                                                 QRhiRenderBuffer::Flags flags) = 0;
    virtual QRhiTexture *createTexture(QRhiTexture::Format format,
                                       const QSize &pixelSize,
                                       int sampleCount,
                                       QRhiTexture::Flags flags) = 0;
    virtual QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                       QRhiSampler::Filter mipmapMode,
                                       QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w) = 0;

    virtual QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                               QRhiTextureRenderTarget::Flags flags) = 0;

    virtual QRhiSwapChain *createSwapChain() = 0;
    virtual QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain, QRhi::BeginFrameFlags flags) = 0;
    virtual QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain, QRhi::EndFrameFlags flags) = 0;
    virtual QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb) = 0;
    virtual QRhi::FrameOpResult endOffscreenFrame() = 0;
    virtual QRhi::FrameOpResult finish() = 0;

    virtual void resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) = 0;

    virtual void beginPass(QRhiCommandBuffer *cb,
                           QRhiRenderTarget *rt,
                           const QRhiColorClearValue &colorClearValue,
                           const QRhiDepthStencilClearValue &depthStencilClearValue,
                           QRhiResourceUpdateBatch *resourceUpdates) = 0;
    virtual void endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) = 0;

    virtual void setGraphicsPipeline(QRhiCommandBuffer *cb,
                                     QRhiGraphicsPipeline *ps) = 0;

    virtual void setShaderResources(QRhiCommandBuffer *cb,
                                    QRhiShaderResourceBindings *srb,
                                    const QVector<QRhiCommandBuffer::DynamicOffset> &dynamicOffsets) = 0;

    virtual void setVertexInput(QRhiCommandBuffer *cb,
                                int startBinding, const QVector<QRhiCommandBuffer::VertexInput> &bindings,
                                QRhiBuffer *indexBuf, quint32 indexOffset,
                                QRhiCommandBuffer::IndexFormat indexFormat) = 0;

    virtual void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport) = 0;
    virtual void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor) = 0;
    virtual void setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c) = 0;
    virtual void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue) = 0;

    virtual void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                      quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) = 0;
    virtual void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                             quint32 instanceCount, quint32 firstIndex,
                             qint32 vertexOffset, quint32 firstInstance) = 0;

    virtual void debugMarkBegin(QRhiCommandBuffer *cb, const QByteArray &name) = 0;
    virtual void debugMarkEnd(QRhiCommandBuffer *cb) = 0;
    virtual void debugMarkMsg(QRhiCommandBuffer *cb, const QByteArray &msg) = 0;

    virtual QVector<int> supportedSampleCounts() const = 0;
    virtual int ubufAlignment() const = 0;
    virtual bool isYUpInFramebuffer() const = 0;
    virtual bool isYUpInNDC() const = 0;
    virtual QMatrix4x4 clipSpaceCorrMatrix() const = 0;
    virtual bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const = 0;
    virtual bool isFeatureSupported(QRhi::Feature feature) const = 0;
    virtual int resourceSizeLimit(QRhi::ResourceSizeLimit limit) const = 0;
    virtual const QRhiNativeHandles *nativeHandles() = 0;

    virtual void sendVMemStatsToProfiler();

    bool isCompressedFormat(QRhiTexture::Format format) const;
    void compressedFormatInfo(QRhiTexture::Format format, const QSize &size,
                              quint32 *bpl, quint32 *byteSize,
                              QSize *blockDim) const;
    void textureFormatInfo(QRhiTexture::Format format, const QSize &size,
                           quint32 *bpl, quint32 *byteSize) const;
    quint32 approxByteSizeForTexture(QRhiTexture::Format format, const QSize &baseSize,
                                     int mipCount, int layerCount);

    QRhiProfilerPrivate *profilerPrivateOrNull()
    {
        // return null when QRhi::EnableProfiling was not set
        QRhiProfilerPrivate *p = QRhiProfilerPrivate::get(&profiler);
        return p->rhiDWhenEnabled ? p : nullptr;
    }

    // only really care about resources that own native graphics resources underneath
    void registerResource(QRhiResource *res)
    {
        res->m_orphanedWithRsh = nullptr;
        resources.insert(res);
    }

    void unregisterResource(QRhiResource *res)
    {
        resources.remove(res);
    }

    QSet<QRhiResource *> activeResources() const
    {
        return resources;
    }

    static bool orphanCheck(QRhiResource *res)
    {
        if (res->m_orphanedWithRsh) {
            qWarning("Attempted to perform something on an orphaned QRhiResource %p (%s). This is invalid.",
                     (void *) res, res->m_objectName.constData());
            return false;
        }
        return true;
    }

    void addReleaseAndDestroyLater(QRhiResource *res)
    {
        if (inFrame)
            pendingReleaseAndDestroyResources.insert(res);
        else
            res->releaseAndDestroy();
    }

    QRhi *q;

protected:
    QRhiResourceSharingHostPrivate *rsh = nullptr;
    bool debugMarkers = false;

private:
    QRhi::Implementation implType;
    QThread *implThread;
    QRhiProfiler profiler;
    QVector<QRhiResourceUpdateBatch *> resUpdPool;
    QBitArray resUpdPoolMap;
    QSet<QRhiResource *> resources;
    QSet<QRhiResource *> pendingReleaseAndDestroyResources;
    bool inFrame = false;

    friend class QRhi;
    friend class QRhiResourceUpdateBatchPrivate;
};

class QRhiResourceUpdateBatchPrivate
{
public:
    struct DynamicBufferUpdate {
        DynamicBufferUpdate() { }
        DynamicBufferUpdate(QRhiBuffer *buf_, int offset_, int size_, const void *data_)
            : buf(buf_), offset(offset_), data(reinterpret_cast<const char *>(data_), size_)
        { }

        QRhiBuffer *buf = nullptr;
        int offset = 0;
        QByteArray data;
    };

    struct StaticBufferUpload {
        StaticBufferUpload() { }
        StaticBufferUpload(QRhiBuffer *buf_, int offset_, int size_, const void *data_)
            : buf(buf_), offset(offset_), data(reinterpret_cast<const char *>(data_), size_ ? size_ : buf_->size())
        { }

        QRhiBuffer *buf = nullptr;
        int offset = 0;
        QByteArray data;
    };

    struct TextureUpload {
        TextureUpload() { }
        TextureUpload(QRhiTexture *tex_, const QRhiTextureUploadDescription &desc_)
            : tex(tex_), desc(desc_)
        { }

        QRhiTexture *tex = nullptr;
        QRhiTextureUploadDescription desc;
    };

    struct TextureCopy {
        TextureCopy() { }
        TextureCopy(QRhiTexture *dst_, QRhiTexture *src_, const QRhiTextureCopyDescription &desc_)
            : dst(dst_), src(src_), desc(desc_)
        { }

        QRhiTexture *dst = nullptr;
        QRhiTexture *src = nullptr;
        QRhiTextureCopyDescription desc;
    };

    struct TextureRead {
        TextureRead() { }
        TextureRead(const QRhiReadbackDescription &rb_, QRhiReadbackResult *result_)
            : rb(rb_), result(result_)
        { }

        QRhiReadbackDescription rb;
        QRhiReadbackResult *result;
    };

    struct TextureMipGen {
        TextureMipGen() { }
        TextureMipGen(QRhiTexture *tex_) : tex(tex_)
        { }

        QRhiTexture *tex = nullptr;
    };

    QVector<DynamicBufferUpdate> dynamicBufferUpdates;
    QVector<StaticBufferUpload> staticBufferUploads;
    QVector<TextureUpload> textureUploads;
    QVector<TextureCopy> textureCopies;
    QVector<TextureRead> textureReadbacks;
    QVector<TextureMipGen> textureMipGens;

    QRhiResourceUpdateBatch *q = nullptr;
    QRhiImplementation *rhi = nullptr;
    int poolIndex = -1;

    void free();
    void merge(QRhiResourceUpdateBatchPrivate *other);

    static QRhiResourceUpdateBatchPrivate *get(QRhiResourceUpdateBatch *b) { return b->d; }
};

Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::StaticBufferUpload, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::TextureUpload, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::TextureCopy, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::TextureRead, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::TextureMipGen, Q_MOVABLE_TYPE);

class Q_RHI_PRIVATE_EXPORT QRhiShaderResourceBindingPrivate
{
public:
    QRhiShaderResourceBindingPrivate()
        : ref(1)
    {
    }

    QRhiShaderResourceBindingPrivate(const QRhiShaderResourceBindingPrivate *other)
        : ref(1),
          binding(other->binding),
          stage(other->stage),
          type(other->type),
          u(other->u)
    {
    }

    static QRhiShaderResourceBindingPrivate *get(QRhiShaderResourceBinding *s) { return s->d; }
    static const QRhiShaderResourceBindingPrivate *get(const QRhiShaderResourceBinding *s) { return s->d; }

    QAtomicInt ref;
    int binding;
    QRhiShaderResourceBinding::StageFlags stage;
    QRhiShaderResourceBinding::Type type;
    struct UniformBufferData {
        QRhiBuffer *buf;
        int offset;
        int maybeSize;
        bool hasDynamicOffset;
    };
    struct SampledTextureData {
        QRhiTexture *tex;
        QRhiSampler *sampler;
    };
    union {
        UniformBufferData ubuf;
        SampledTextureData stex;
    } u;
};

template<typename T>
struct QRhiBatchedBindings
{
    void feed(int binding, T resource) { // binding must be strictly increasing
        if (curBinding == -1 || binding > curBinding + 1) {
            finish();
            curBatch.startBinding = binding;
            curBatch.resources.clear();
            curBatch.resources.append(resource);
        } else {
            Q_ASSERT(binding == curBinding + 1);
            curBatch.resources.append(resource);
        }
        curBinding = binding;
    }

    void finish() {
        if (!curBatch.resources.isEmpty())
            batches.append(curBatch);
    }

    void clear() {
        batches.clear();
        curBatch.resources.clear();
        curBinding = -1;
    }

    struct Batch {
        uint startBinding;
        QVarLengthArray<T, 4> resources;

        bool operator==(const Batch &other) const
        {
            return startBinding == other.startBinding && resources == other.resources;
        }

        bool operator!=(const Batch &other) const
        {
            return !operator==(other);
        }
    };

    QVarLengthArray<Batch, 4> batches; // sorted by startBinding

    bool operator==(const QRhiBatchedBindings<T> &other) const
    {
        return batches == other.batches;
    }

    bool operator!=(const QRhiBatchedBindings<T> &other) const
    {
        return !operator==(other);
    }

private:
    Batch curBatch;
    int curBinding = -1;
};

class QRhiGlobalObjectIdGenerator
{
public:
#ifdef Q_ATOMIC_INT64_IS_SUPPORTED
    using Type = quint64;
#else
    using Type = quint32;
#endif
    static Type newId();

private:
    QAtomicInteger<Type> counter;
};

QT_END_NAMESPACE

#endif
