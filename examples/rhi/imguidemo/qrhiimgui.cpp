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

#include "qrhiimgui_p.h"
#include <QFile>

QT_BEGIN_NAMESPACE

QRhiImgui::QRhiImgui()
    : d(new QRhiImguiPrivate)
{
}

QRhiImgui::~QRhiImgui()
{
    releaseResources();
    delete d;
}

void QRhiImgui::setFrameFunc(FrameFunc f)
{
    d->frame = f;
}

void QRhiImgui::demoWindow()
{
    ImGui::ShowDemoWindow(&d->showDemoWindow);
}

static QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

bool QRhiImgui::imguiPass(QRhiCommandBuffer *cb, QRhiRenderTarget *rt, QRhiRenderPassDescriptor *rp)
{
    ImGuiIO &io(ImGui::GetIO());

    if (d->textures.isEmpty()) {
        unsigned char *pixels;
        int w, h;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
        const QImage wrapperImg((const uchar *) pixels, w, h, QImage::Format_RGBA8888);
        QRhiImguiPrivate::Texture t;
        t.image = wrapperImg.copy();
        d->textures.append(t);
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(quintptr(d->textures.count() - 1)));
    }

    const QSize outputSize = rt->sizeInPixels();
    const float dpr = rt->devicePixelRatio();
    io.DisplaySize.x = outputSize.width() / dpr;
    io.DisplaySize.y = outputSize.height() / dpr;
    io.DisplayFramebufferScale = ImVec2(dpr, dpr);

    ImGui::NewFrame();
    if (d->frame)
        d->frame();
    ImGui::Render();

    ImDrawData *draw = ImGui::GetDrawData();
    draw->ScaleClipRects(ImVec2(dpr, dpr));

    QRhiResourceUpdateBatch *resUpd = d->rhi->nextResourceUpdateBatch();
    if (!d->ubuf) {
        d->ubuf = d->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 4);
        d->releasePool << d->ubuf;
        if (!d->ubuf->build())
            return false;

        float opacity = 1.0f;
        resUpd->updateDynamicBuffer(d->ubuf, 64, 4, &opacity);
    }

    QMatrix4x4 mvp = d->rhi->clipSpaceCorrMatrix();
    mvp.ortho(0, io.DisplaySize.x, io.DisplaySize.y, 0, 1, -1);
    resUpd->updateDynamicBuffer(d->ubuf, 0, 64, mvp.constData());

    if (!d->sampler) {
        d->sampler = d->rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                        QRhiSampler::Repeat, QRhiSampler::Repeat);
        d->releasePool << d->sampler;
        if (!d->sampler->build())
            return false;
    }

    for (QRhiImguiPrivate::Texture &t : d->textures) {
        if (!t.tex) {
            t.tex = d->rhi->newTexture(QRhiTexture::RGBA8, t.image.size());
            if (!t.tex->build())
                return false;
            resUpd->uploadTexture(t.tex, t.image);
        }
        if (!t.srb) {
            t.srb = d->rhi->newShaderResourceBindings();
            t.srb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d->ubuf),
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, t.tex, d->sampler)
            });
            if (!t.srb->build())
                return false;
        }
    }

    if (!d->ps) {
        d->ps = d->rhi->newGraphicsPipeline();
        d->releasePool << d->ps;
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::Zero;
        blend.colorWrite = QRhiGraphicsPipeline::R | QRhiGraphicsPipeline::G | QRhiGraphicsPipeline::B;
        d->ps->setTargetBlends({ blend });
        d->ps->setCullMode(QRhiGraphicsPipeline::None);
        d->ps->setDepthTest(true);
        d->ps->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        d->ps->setDepthWrite(false);
        d->ps->setFlags(QRhiGraphicsPipeline::UsesScissor);

        QBakedShader vs = getShader(QLatin1String(":/imgui.vert.qsb"));
        Q_ASSERT(vs.isValid());
        QBakedShader fs = getShader(QLatin1String(":/imgui.frag.qsb"));
        Q_ASSERT(fs.isValid());
        d->ps->setShaderStages({
            { QRhiGraphicsShaderStage::Vertex, vs },
            { QRhiGraphicsShaderStage::Fragment, fs }
        });

        QRhiVertexInputLayout inputLayout;
        inputLayout.bindings = {
            { 4 * sizeof(float) + sizeof(quint32) }
        };
        inputLayout.attributes = {
            { 0, 0, QRhiVertexInputLayout::Attribute::Float2, 0 },
            { 0, 1, QRhiVertexInputLayout::Attribute::Float2, 2 * sizeof(float) },
            { 0, 2, QRhiVertexInputLayout::Attribute::UNormByte4, 4 * sizeof(float) }
        };

        d->ps->setVertexInputLayout(inputLayout);
        d->ps->setShaderResourceBindings(d->textures[0].srb);
        d->ps->setRenderPassDescriptor(rp);

        if (!d->ps->build())
            return false;
    }

    // the imgui default
    Q_ASSERT(sizeof(ImDrawVert) == 20);
    // switched to uint in imconfig.h to avoid trouble with 4 byte offset alignment reqs
    Q_ASSERT(sizeof(ImDrawIdx) == 4);

    QVarLengthArray<quint32, 4> vbufOffsets(draw->CmdListsCount);
    QVarLengthArray<quint32, 4> ibufOffsets(draw->CmdListsCount);
    int totalVbufSize = 0;
    int totalIbufSize = 0;
    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        const int vbufSize = cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
        vbufOffsets[n] = totalVbufSize;
        totalVbufSize += vbufSize;
        const int ibufSize = cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
        ibufOffsets[n] = totalIbufSize;
        totalIbufSize += ibufSize;
    }

    if (!d->vbuf) {
        d->vbuf = d->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, totalVbufSize);
        d->releasePool << d->vbuf;
        if (!d->vbuf->build())
            return false;
    } else {
        if (totalVbufSize > d->vbuf->size()) {
            d->vbuf->setSize(totalVbufSize);
            if (!d->vbuf->build())
                return false;
        }
    }
    if (!d->ibuf) {
        d->ibuf = d->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, totalIbufSize);
        d->releasePool << d->ibuf;
        if (!d->ibuf->build())
            return false;
    } else {
        if (totalIbufSize > d->ibuf->size()) {
            d->ibuf->setSize(totalIbufSize);
            if (!d->ibuf->build())
                return false;
        }
    }

    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        const int vbufSize = cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
        resUpd->updateDynamicBuffer(d->vbuf, vbufOffsets[n], vbufSize, cmdList->VtxBuffer.Data);
        const int ibufSize = cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
        resUpd->updateDynamicBuffer(d->ibuf, ibufOffsets[n], ibufSize, cmdList->IdxBuffer.Data);
    }

    cb->beginPass(rt, { 0, 0, 0, 1 }, { 1, 0 }, resUpd);
    cb->setViewport({ 0, 0, float(outputSize.width()), float(outputSize.height()) });

    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        const ImDrawIdx *indexBufOffset = nullptr;

        for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
            const ImDrawCmd *cmd = &cmdList->CmdBuffer[i];
            if (!cmd->UserCallback) {
                const QPointF scissorPixelBottomLeft = QPointF(cmd->ClipRect.x, outputSize.height() - cmd->ClipRect.w);
                const QSizeF scissorPixelSize = QSizeF(cmd->ClipRect.z - cmd->ClipRect.x, cmd->ClipRect.w - cmd->ClipRect.y);
                const int textureIndex = int(reinterpret_cast<qintptr>(cmd->TextureId));
                cb->setGraphicsPipeline(d->ps, d->textures[textureIndex].srb);
                cb->setScissor({ int(scissorPixelBottomLeft.x()), int(scissorPixelBottomLeft.y()),
                                 int(scissorPixelSize.width()), int(scissorPixelSize.height()) });
                cb->setVertexInput(0, { { d->vbuf, vbufOffsets[n] } },
                                   d->ibuf, ibufOffsets[n] + quintptr(indexBufOffset), QRhiCommandBuffer::IndexUInt32);
                cb->drawIndexed(cmd->ElemCount);
            } else {
                cmd->UserCallback(cmdList, cmd);
            }
            indexBufOffset += cmd->ElemCount;
        }
    }

    cb->endPass();

    return true;
}

void QRhiImgui::initialize(QRhi *rhi)
{
    d->rhi = rhi;
}

void QRhiImgui::releaseResources()
{
    for (QRhiImguiPrivate::Texture &t : d->textures) {
        if (t.tex)
            t.tex->releaseAndDestroy();
        if (t.srb)
            t.srb->releaseAndDestroy();
    }
    d->textures.clear();

    for (QRhiResource *r : d->releasePool)
        r->releaseAndDestroy();
    d->releasePool.clear();

    d->vbuf = d->ibuf = d->ubuf = nullptr;
    d->ps = nullptr;
    d->sampler = nullptr;
}

QRhiImgui::FrameFunc QRhiImgui::frameFunc() const
{
    return d->frame;
}

QRhiImguiPrivate::QRhiImguiPrivate()
{
    ImGui::CreateContext();
}

QRhiImguiPrivate::~QRhiImguiPrivate()
{
    ImGui::DestroyContext();
}

QT_END_NAMESPACE
