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
#include <QBitArray>

QT_BEGIN_NAMESPACE

#define QRHI_RES(t, x) static_cast<t *>(x)
#define QRHI_RES_RHI(t) t *rhiD = static_cast<t *>(rhi)

class QRhiReferenceRenderTarget : public QRhiRenderTarget
{
protected:
    QRhiReferenceRenderTarget(QRhiImplementation *rhi);
};

class QRhiImplementation
{
public:
    virtual ~QRhiImplementation();

    virtual QRhiGraphicsPipeline *createGraphicsPipeline() = 0;
    virtual QRhiShaderResourceBindings *createShaderResourceBindings() = 0;
    virtual QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                                     QRhiBuffer::UsageFlags usage,
                                     int size) = 0;
    virtual QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                                 const QSize &pixelSize,
                                                 int sampleCount,
                                                 QRhiRenderBuffer::Hints hints) = 0;
    virtual QRhiTexture *createTexture(QRhiTexture::Format format,
                                       const QSize &pixelSize,
                                       QRhiTexture::Flags flags) = 0;
    virtual QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                       QRhiSampler::Filter mipmapMode,
                                       QRhiSampler:: AddressMode u, QRhiSampler::AddressMode v, QRhiSampler::AddressMode w) = 0;

    virtual QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                               QRhiTextureRenderTarget::Flags flags) = 0;

    virtual QRhiSwapChain *createSwapChain() = 0;
    virtual QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain) = 0;
    virtual QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain) = 0;

    virtual void beginPass(QRhiRenderTarget *rt,
                           QRhiCommandBuffer *cb,
                           const QRhiColorClearValue &colorClearValue,
                           const QRhiDepthStencilClearValue &depthStencilClearValue,
                           QRhiResourceUpdateBatch *resourceUpdates) = 0;
    virtual void endPass(QRhiCommandBuffer *cb) = 0;

    virtual void setGraphicsPipeline(QRhiCommandBuffer *cb,
                                     QRhiGraphicsPipeline *ps,
                                     QRhiShaderResourceBindings *srb = nullptr) = 0;

    virtual void setVertexInput(QRhiCommandBuffer *cb,
                                int startBinding, const QVector<QRhi::VertexInput> &bindings,
                                QRhiBuffer *indexBuf, quint32 indexOffset,
                                QRhi::IndexFormat indexFormat) = 0;

    virtual void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport) = 0;
    virtual void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor) = 0;
    virtual void setBlendConstants(QRhiCommandBuffer *cb, const QVector4D &c) = 0;
    virtual void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue) = 0;

    virtual void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
                      quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) = 0;
    virtual void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                             quint32 instanceCount, quint32 firstIndex,
                             qint32 vertexOffset, quint32 firstInstance) = 0;

    virtual QVector<int> supportedSampleCounts() const = 0;
    virtual int ubufAlignment() const = 0;
    virtual bool isYUpInFramebuffer() const = 0;
    virtual QMatrix4x4 clipSpaceCorrMatrix() const = 0;

    QVector<QRhiResourceUpdateBatch *> resUpdPool;
    QBitArray resUpdPoolMap;
};

struct QRhiResourceUpdateBatchPrivate
{
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
        StaticBufferUpload(QRhiBuffer *buf_, const void *data_)
            : buf(buf_), data(reinterpret_cast<const char *>(data_), buf_->size())
        { }

        QRhiBuffer *buf = nullptr;
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

    QVector<DynamicBufferUpdate> dynamicBufferUpdates;
    QVector<StaticBufferUpload> staticBufferUploads;
    QVector<TextureUpload> textureUploads;

    QRhiResourceUpdateBatch *q = nullptr;
    QRhiImplementation *rhi = nullptr;
    int poolIndex = -1;

    void free();

    static QRhiResourceUpdateBatchPrivate *get(QRhiResourceUpdateBatch *b) { return b->d; }
};

Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::DynamicBufferUpdate, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::StaticBufferUpload, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiResourceUpdateBatchPrivate::TextureUpload, Q_MOVABLE_TYPE);

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
    };

    QVarLengthArray<Batch, 4> batches; // sorted by startBinding

private:
    Batch curBatch;
    int curBinding = -1;
};

QT_END_NAMESPACE

#endif
