/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Demonstrates using the same QRhiTexture with two QRhi instances.

#include <QGuiApplication>

#include <QCommandLineParser>
#include <QWindow>
#include <QPlatformSurfaceEvent>
#include <QElapsedTimer>

#include <QBakedShader>
#include <QFile>

#include <QOffscreenSurface>
#ifndef QT_NO_OPENGL
#include <QRhiGles2InitParams>
#endif

#if QT_CONFIG(vulkan)
#include <QLoggingCategory>
#include <QRhiVulkanInitParams>
#endif

#ifdef Q_OS_WIN
#include <QRhiD3D11InitParams>
#endif

#ifdef Q_OS_DARWIN
#include <QRhiMetalInitParams>
#endif

enum GraphicsApi
{
    OpenGL,
    Vulkan,
    D3D11,
    Metal
};

static GraphicsApi graphicsApi;

static QString graphicsApiName()
{
    switch (graphicsApi) {
    case OpenGL:
        return QLatin1String("OpenGL 2.x");
    case Vulkan:
        return QLatin1String("Vulkan");
    case D3D11:
        return QLatin1String("Direct3D 11");
    case Metal:
        return QLatin1String("Metal");
    default:
        break;
    }
    return QString();
}

#if QT_CONFIG(vulkan)
QVulkanInstance *vkinst = nullptr;
int activeRhiCount = 0;
QRhiResourceSharingHost *rsh = nullptr;
QRhiTexture *tex = nullptr;
#endif

void createRhi(QWindow *window, QRhi **rhi, QOffscreenSurface **fallbackSurface)
{
    // This is what makes the difference here - create a single
    // QRhiResourceSharingHost and associate all QRhis with it.
    if (!rsh)
        rsh = new QRhiResourceSharingHost;

#ifndef QT_NO_OPENGL
    if (graphicsApi == OpenGL) {
        *fallbackSurface = QRhiGles2InitParams::newFallbackSurface();
        QRhiGles2InitParams params;
        params.resourceSharingHost = rsh;
        params.fallbackSurface = *fallbackSurface;
        params.window = window;
        *rhi = QRhi::create(QRhi::OpenGLES2, &params);
    }
#endif

#if QT_CONFIG(vulkan)
    if (graphicsApi == Vulkan) {
        QRhiVulkanInitParams params;
        params.resourceSharingHost = rsh;
        params.inst = vkinst;
        params.window = window;
        *rhi = QRhi::create(QRhi::Vulkan, &params);
    }
#endif

#ifdef Q_OS_WIN
    if (graphicsApi == D3D11) {
        QRhiD3D11InitParams params;
        params.resourceSharingHost = rsh;
        params.enableDebugLayer = true;
        *rhi = QRhi::create(QRhi::D3D11, &params);
    }
#endif

#ifdef Q_OS_DARWIN
    if (graphicsApi == Metal) {
        QRhiMetalInitParams params;
        params.resourceSharingHost = rsh;
        *rhi = QRhi::create(QRhi::Metal, &params);
    }
#endif

    if (!*rhi)
        qFatal("Failed to create RHI backend");
}

class Window : public QWindow
{
public:
    Window(const QString &title, const QColor &bgColor, int windowNumber);
    ~Window();

protected:
    void init();
    void releaseResources();
    void resizeSwapChain();
    void releaseSwapChain();
    void render();

    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

    QRhi *m_rhi = nullptr;
    QOffscreenSurface *m_fallbackSurface = nullptr;
    QColor m_bgColor;
    int m_windowNumber;

    bool m_running = false;
    bool m_notExposed = false;
    bool m_newlyExposed = false;

    QMatrix4x4 m_proj;
    float m_rotation = 0;
    QVector<QRhiResource *> m_releasePool;

    bool m_hasSwapChain = false;
    QRhiSwapChain *m_sc = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;
    QRhiRenderPassDescriptor *m_rp = nullptr;

    QRhiResourceUpdateBatch *initialUpdates = nullptr;
    QRhiBuffer *vbuf = nullptr;
    QRhiBuffer *ibuf = nullptr;
    QRhiBuffer *ubuf = nullptr;
    QRhiSampler *sampler = nullptr;
    QRhiShaderResourceBindings *srb = nullptr;
    QRhiGraphicsPipeline *ps = nullptr;
};

Window::Window(const QString &title, const QColor &bgColor, int windowNumber)
    : m_bgColor(bgColor),
      m_windowNumber(windowNumber)
{
    switch (graphicsApi) {
    case OpenGL:
        setSurfaceType(OpenGLSurface);
        break;
    case Vulkan:
        setSurfaceType(VulkanSurface);
        setVulkanInstance(vkinst);
        break;
    case D3D11:
        setSurfaceType(OpenGLSurface); // not a typo
        break;
    case Metal:
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
        setSurfaceType(MetalSurface);
#endif
        break;
    default:
        break;
    }

    resize(800, 600);
    setTitle(title);
}

Window::~Window()
{
    releaseResources();
}

void Window::exposeEvent(QExposeEvent *)
{
    // initialize and start rendering when the window becomes usable for graphics purposes
    if (isExposed() && !m_running) {
        m_running = true;
        init();
        resizeSwapChain();
        render();
    }

    // stop pushing frames when not exposed (or size is 0)
    if ((!isExposed() || (m_hasSwapChain && m_sc->surfacePixelSize().isEmpty())) && m_running)
        m_notExposed = true;

    // continue when exposed again and the surface has a valid size.
    // note that the surface size can be (0, 0) even though size() reports a valid one...
    if (isExposed() && m_running && m_notExposed && !m_sc->surfacePixelSize().isEmpty()) {
        m_notExposed = false;
        m_newlyExposed = true;
        render();
    }
}

bool Window::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::UpdateRequest:
        render();
        break;

    case QEvent::PlatformSurface:
        // this is the proper time to tear down the swapchain (while the native window and surface are still around)
        if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            releaseSwapChain();
        break;

    default:
        break;
    }

    return QWindow::event(e);
}

static float quadVert[] =
{
  -0.5f,   0.5f,   0.0f, 0.0f,
  -0.5f,  -0.5f,   0.0f, 1.0f,
  0.5f,   -0.5f,   1.0f, 1.0f,
  0.5f,   0.5f,    1.0f, 0.0f
};

static quint16 quadIndex[] =
{
    0, 1, 2, 0, 2, 3
};

QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

void Window::init()
{
    createRhi(this, &m_rhi, &m_fallbackSurface);
    ++activeRhiCount;

    m_sc = m_rhi->newSwapChain();
    m_ds = m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                  QSize(), // no need to set the size yet
                                  1,
                                  QRhiRenderBuffer::UsedWithSwapChainOnly);
    m_releasePool << m_ds;
    m_sc->setWindow(this);
    m_sc->setDepthStencil(m_ds);
    m_rp = m_sc->newCompatibleRenderPassDescriptor();
    m_releasePool << m_rp;
    m_sc->setRenderPassDescriptor(m_rp);

    vbuf = m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(quadVert));
    m_releasePool << vbuf;
    vbuf->build();

    ibuf = m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, sizeof(quadIndex));
    m_releasePool << ibuf;
    ibuf->build();

    ubuf = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    ubuf->build();
    m_releasePool << ubuf;

    QImage image;
    bool newTex = false;
    if (!tex) {
        newTex = true;
        image.load(QLatin1String(":/qt256.png"));
        tex = m_rhi->newTexture(QRhiTexture::RGBA8, image.size());
        Q_ASSERT(tex->isSharable());
        tex->build();
    }

    sampler = m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_releasePool << sampler;
    sampler->build();

    srb = m_rhi->newShaderResourceBindings();
    m_releasePool << srb;
    srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, ubuf),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, tex, sampler)
    });
    srb->build();

    ps = m_rhi->newGraphicsPipeline();
    m_releasePool << ps;
    ps->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, getShader(QLatin1String(":/texture.vert.qsb")) },
        { QRhiGraphicsShaderStage::Fragment, getShader(QLatin1String(":/texture.frag.qsb")) }
    });
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 4 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) }
    });
    ps->setVertexInputLayout(inputLayout);
    ps->setShaderResourceBindings(srb);
    ps->setRenderPassDescriptor(m_rp);
    ps->build();

    initialUpdates = m_rhi->nextResourceUpdateBatch();
    initialUpdates->uploadStaticBuffer(vbuf, 0, sizeof(quadVert), quadVert);
    initialUpdates->uploadStaticBuffer(ibuf, quadIndex);
    quint32 flip = 0;
    initialUpdates->updateDynamicBuffer(ubuf, 64, 4, &flip);

    if (newTex)
        initialUpdates->uploadTexture(tex, image);
}

void Window::releaseResources()
{
    for (QRhiResource *res : m_releasePool)
        res->releaseAndDestroy();

    m_releasePool.clear();

    if (m_sc) {
        m_sc->releaseAndDestroy();
        m_sc = nullptr;
    }

    // tex may outlive its creating QRhi, that's fine since it's isSharable()==true
    if (activeRhiCount == 1) {
        tex->releaseAndDestroy();
        tex = nullptr;
    }
    delete m_rhi;
    m_rhi = nullptr;
    --activeRhiCount;

    delete m_fallbackSurface;
    m_fallbackSurface = nullptr;
}

void Window::resizeSwapChain()
{
    const QSize outputSize = m_sc->surfacePixelSize();

    m_ds->setPixelSize(outputSize);
    m_ds->build();

    m_hasSwapChain = m_sc->buildOrResize();

    m_proj = m_rhi->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, outputSize.width() / (float) outputSize.height(), 0.01f, 1000.0f);
    m_proj.translate(0, 0, -4);
}

void Window::releaseSwapChain()
{
    if (m_hasSwapChain) {
        m_hasSwapChain = false;
        m_sc->release();
    }
}

void Window::render()
{
    if (!m_hasSwapChain || m_notExposed)
        return;

    // If the window got resized or got newly exposed, resize the swapchain.
    // (the newly-exposed case is not actually required by some
    // platforms/backends, but f.ex. Vulkan on Windows seems to need it)
    if (m_sc->currentPixelSize() != m_sc->surfacePixelSize() || m_newlyExposed) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        m_newlyExposed = false;
    }

    QRhi::FrameOpResult result = m_rhi->beginFrame(m_sc);
    if (result == QRhi::FrameOpSwapChainOutOfDate) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        result = m_rhi->beginFrame(m_sc);
    }
    if (result != QRhi::FrameOpSuccess) {
        requestUpdate();
        return;
    }

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    const QSize outputSizeInPixels = m_sc->currentPixelSize();

    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
    if (initialUpdates) {
        u->merge(initialUpdates);
        initialUpdates->release();
        initialUpdates = nullptr;
    }

    QMatrix4x4 mvp = m_proj;
    mvp.scale(2.5f);
    mvp.rotate(m_rotation, m_windowNumber == 2, m_windowNumber == 1, m_windowNumber == 0);
    m_rotation += 0.5f;
    u->updateDynamicBuffer(ubuf, 0, 64, mvp.constData());


    cb->beginPass(m_sc->currentFrameRenderTarget(),
                  { float(m_bgColor.redF()), float(m_bgColor.greenF()), float(m_bgColor.blueF()), 1.0f },
                  { 1.0f, 0 },
                  u);

    cb->setGraphicsPipeline(ps);
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setVertexInput(0, { { vbuf, 0 } }, ibuf, 0, QRhiCommandBuffer::IndexUInt16);
    cb->drawIndexed(6);

    cb->endPass();

    m_rhi->endFrame(m_sc);

    requestUpdate();
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

#if defined(Q_OS_WIN)
    graphicsApi = D3D11;
#elif defined(Q_OS_DARWIN)
    graphicsApi = Metal;
#elif QT_CONFIG(vulkan)
    graphicsApi = Vulkan;
#else
    graphicsApi = OpenGL;
#endif

    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    QCommandLineOption glOption({ "g", "opengl" }, QLatin1String("OpenGL (2.x)"));
    cmdLineParser.addOption(glOption);
    QCommandLineOption vkOption({ "v", "vulkan" }, QLatin1String("Vulkan"));
    cmdLineParser.addOption(vkOption);
    QCommandLineOption d3dOption({ "d", "d3d11" }, QLatin1String("Direct3D 11"));
    cmdLineParser.addOption(d3dOption);
    QCommandLineOption mtlOption({ "m", "metal" }, QLatin1String("Metal"));
    cmdLineParser.addOption(mtlOption);
    cmdLineParser.process(app);
    if (cmdLineParser.isSet(glOption))
        graphicsApi = OpenGL;
    if (cmdLineParser.isSet(vkOption))
        graphicsApi = Vulkan;
    if (cmdLineParser.isSet(d3dOption))
        graphicsApi = D3D11;
    if (cmdLineParser.isSet(mtlOption))
        graphicsApi = Metal;

    qDebug("Selected graphics API is %s", qPrintable(graphicsApiName()));
    qDebug("This is a multi-api example, use command line arguments to override:\n%s", qPrintable(cmdLineParser.helpText()));

#if QT_CONFIG(vulkan)
    vkinst = new QVulkanInstance;
    if (graphicsApi == Vulkan) {
#ifndef Q_OS_ANDROID
        vkinst->setLayers({ "VK_LAYER_LUNARG_standard_validation" });
#else
        vkinst->setLayers(QByteArrayList()
                          << "VK_LAYER_GOOGLE_threading"
                          << "VK_LAYER_LUNARG_parameter_validation"
                          << "VK_LAYER_LUNARG_object_tracker"
                          << "VK_LAYER_LUNARG_core_validation"
                          << "VK_LAYER_LUNARG_image"
                          << "VK_LAYER_LUNARG_swapchain"
                          << "VK_LAYER_GOOGLE_unique_objects");
#endif
        if (!vkinst->create()) {
            qWarning("Failed to create Vulkan instance, switching to OpenGL");
            graphicsApi = OpenGL;
        }
    }
#endif

    int result;
    // lifetime: make sure the QWindows are gone when we move on to destroying
    // the Vulkan instance and such.
    {
        Window windowA(QLatin1String("QRhi #1"), Qt::green, 0);
        Window windowB(QLatin1String("QRhi #2"), Qt::blue, 1);

        windowA.show();
        windowB.show();

        windowA.setPosition(windowA.position() - QPoint(200, 200));
        windowB.setPosition(windowB.position() + QPoint(200, 200));

        result = app.exec();
    }

    delete rsh;
#if QT_CONFIG(vulkan)
    delete vkinst;
#endif

    return result;
}
