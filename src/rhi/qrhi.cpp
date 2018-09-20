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

#include "qrhi.h"
#include "qrhivulkan_p.h"

QT_BEGIN_NAMESPACE

QRhiResource::QRhiResource(QRhi *rhi_)
    : rhi(rhi_)
{
}

QRhiResource::~QRhiResource()
{
}

QRhiBuffer::QRhiBuffer(QRhi *rhi, Type type_, UsageFlags usage_, int size_)
    : QRhiResource(rhi),
      type(type_), usage(usage_), size(size_)
{
}

QRhiRenderBuffer::QRhiRenderBuffer(QRhi *rhi, Type type_, const QSize &pixelSize_, int sampleCount_)
    : QRhiResource(rhi),
      type(type_), pixelSize(pixelSize_), sampleCount(sampleCount_)
{
}

QRhiTexture::QRhiTexture(QRhi *rhi, Format format_, const QSize &pixelSize_, Flags flags_)
    : QRhiResource(rhi),
      format(format_), pixelSize(pixelSize_), flags(flags_)
{
}

QRhiSampler::QRhiSampler(QRhi *rhi,
                         Filter magFilter_, Filter minFilter_, Filter mipmapMode_, AddressMode u_, AddressMode v_)
    : QRhiResource(rhi),
      magFilter(magFilter_), minFilter(minFilter_), mipmapMode(mipmapMode_),
      addressU(u_), addressV(v_)
{
}

QRhiRenderPass::QRhiRenderPass(QRhi *rhi)
    : QRhiResource(rhi)
{
}

QRhiRenderTarget::QRhiRenderTarget(QRhi *rhi)
    : QRhiResource(rhi)
{
}

QRhiReferenceRenderTarget::QRhiReferenceRenderTarget(QRhi *rhi)
    : QRhiRenderTarget(rhi)
{
}

QRhiTextureRenderTarget::QRhiTextureRenderTarget(QRhi *rhi,
                                                 QRhiTexture *texture_, Flags flags_)
    : QRhiRenderTarget(rhi),
      texture(texture_), depthTexture(nullptr), depthStencilBuffer(nullptr), flags(flags_)
{
}

QRhiTextureRenderTarget::QRhiTextureRenderTarget(QRhi *rhi,
                                                 QRhiTexture *texture_, QRhiRenderBuffer *depthStencilBuffer_, Flags flags_)
    : QRhiRenderTarget(rhi),
      texture(texture_), depthTexture(nullptr), depthStencilBuffer(depthStencilBuffer_), flags(flags_)
{
}

QRhiTextureRenderTarget::QRhiTextureRenderTarget(QRhi *rhi,
                                                 QRhiTexture *texture_, QRhiTexture *depthTexture_, Flags flags_)
    : QRhiRenderTarget(rhi),
      texture(texture_), depthTexture(depthTexture_), depthStencilBuffer(nullptr), flags(flags_)
{
}

QRhiShaderResourceBindings::QRhiShaderResourceBindings(QRhi *rhi)
    : QRhiResource(rhi)
{
}

QRhiGraphicsPipeline::QRhiGraphicsPipeline(QRhi *rhi)
    : QRhiResource(rhi)
{
}

QRhiSwapChain::QRhiSwapChain(QRhi *rhi)
    : QRhiResource(rhi)
{
}

QRhiCommandBuffer::QRhiCommandBuffer(QRhi *rhi)
    : QRhiResource(rhi)
{
}

QRhi::QRhi()
{
}

QRhi::~QRhi()
{
}

QRhi *QRhi::create(Implementation impl, QRhiInitParams *params)
{
    switch (impl) {
    case Vulkan:
        return new QRhiVulkan(params);
    default:
        break;
    }
    return nullptr;
}

int QRhi::ubufAligned(int v) const
{
    const int byteAlign = ubufAlignment();
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

QMatrix4x4 QRhi::openGLCorrectionMatrix() const
{
    static QMatrix4x4 m;
    if (m.isIdentity()) {
        // NB the ctor takes row-major
        m = QMatrix4x4(1.0f, 0.0f, 0.0f, 0.0f,
                       0.0f, -1.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 0.5f, 0.5f,
                       0.0f, 0.0f, 0.0f, 1.0f);
    }
    return m;
}

QT_END_NAMESPACE
