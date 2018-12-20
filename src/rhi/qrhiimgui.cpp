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

bool QRhiImgui::imguiPass(QRhiCommandBuffer *cb, QRhiRenderTarget *rt)
{
    ImGuiIO &io(ImGui::GetIO());

    if (d->textures.isEmpty()) {
        unsigned char *pixels;
        int w, h;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
        const QImage wrapperImg((const uchar *) pixels, w, h, QImage::Format_RGBA8888);
        d->textures.append(wrapperImg.copy());
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

    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList *cmdList = draw->CmdLists[n];
        const ImDrawIdx *indexBufOffset = nullptr;

//        e.vbuf = QByteArray((const char *) cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
//        e.ibuf = QByteArray((const char *) cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
            const ImDrawCmd *cmd = &cmdList->CmdBuffer[i];
            if (!cmd->UserCallback) {
//                ImGuiRenderer::FrameDesc::Cmd qcmd;
//                qcmd.elemCount = cmd->ElemCount;
//                qcmd.indexOffset = indexBufOffset;

//                qcmd.scissorPixelBottomLeft = QPointF(cmd->ClipRect.x, io.DisplaySize.y * m_dpr - cmd->ClipRect.w);
//                qcmd.scissorPixelSize = QSizeF(cmd->ClipRect.z - cmd->ClipRect.x, cmd->ClipRect.w - cmd->ClipRect.y);

//                qcmd.textureIndex = uint(reinterpret_cast<quintptr>(cmd->TextureId));

//                e.cmds.append(qcmd);
            } else {
                cmd->UserCallback(cmdList, cmd);
            }
            indexBufOffset += cmd->ElemCount;
        }
    }

    return true;
}

void QRhiImgui::releaseResources()
{
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
