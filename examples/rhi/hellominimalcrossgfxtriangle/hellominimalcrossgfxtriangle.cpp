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

// This is a compact, minimal, single-file demo of deciding the backend at
// runtime while using the exact same shaders and rendering code without any
// branching whatsoever once the QWindow is up and the RHI is initialized.

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QWindow>
#include <QPlatformSurfaceEvent>
#include <QElapsedTimer>

#include <QBakedShader>
#include <QFile>

#ifndef QT_NO_OPENGL
#include <QRhiGles2InitParams>
#include <QOpenGLContext>
#include <QOffscreenSurface>
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

static float vertexData[] = {
    // Y up (note clipSpaceCorrMatrix in m_proj), CCW
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f,
};

static QBakedShader getShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QBakedShader::fromSerialized(f.readAll());

    return QBakedShader();
}

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

class Window : public QWindow
{
public:
    Window();
    ~Window();

protected:
    void init();
    void releaseResources();
    void resizeSwapChain();
    void releaseSwapChain();
    void render();

    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

    bool m_running = false;
    bool m_notExposed = false;
    bool m_newlyExposed = false;

    QRhi *m_r = nullptr;
    bool m_hasSwapChain = false;
    QRhiSwapChain *m_sc = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;
    QRhiRenderPassDescriptor *m_rp = nullptr;
    QRhiBuffer *m_vbuf = nullptr;
    bool m_vbufReady = false;
    QRhiBuffer *m_ubuf = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiGraphicsPipeline *m_ps = nullptr;
    QVector<QRhiResource *> releasePool;

    QMatrix4x4 m_proj;
    float m_rotation = 0;
    float m_opacity = 1;
    int m_opacityDir = -1;

    QElapsedTimer m_timer;
    qint64 m_elapsedMs;
    int m_elapsedCount;

#ifndef QT_NO_OPENGL
    QOpenGLContext *m_context = nullptr;
    QOffscreenSurface *m_fallbackSurface = nullptr;
#endif
};

Window::Window()
{
    // Tell the platform plugin what we want.
    switch (graphicsApi) {
    case OpenGL:
        setSurfaceType(OpenGLSurface);
        break;
    case Vulkan:
        setSurfaceType(VulkanSurface);
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

    // stop pushing frames when not exposed (on some platforms this is essential, optional on others)
    if (!isExposed() && m_running)
        m_notExposed = true;

    // continue when exposed again
    if (isExposed() && m_running && m_notExposed) {
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

void Window::init()
{
#ifndef QT_NO_OPENGL
    if (graphicsApi == OpenGL) {
        m_context = new QOpenGLContext;
        if (!m_context->create())
            qFatal("Failed to get OpenGL context");

        m_fallbackSurface = new QOffscreenSurface;
        m_fallbackSurface->setFormat(m_context->format());
        m_fallbackSurface->create();

        QRhiGles2InitParams params;
        params.context = m_context;
        params.window = this;
        params.fallbackSurface = m_fallbackSurface;
        m_r = QRhi::create(QRhi::OpenGLES2, &params);
    }
#endif

#if QT_CONFIG(vulkan)
    if (graphicsApi == Vulkan) {
        QRhiVulkanInitParams params;
        params.inst = vulkanInstance();
        params.window = this;
        m_r = QRhi::create(QRhi::Vulkan, &params);
    }
#endif

#ifdef Q_OS_WIN
    if (graphicsApi == D3D11) {
        QRhiD3D11InitParams params;
        m_r = QRhi::create(QRhi::D3D11, &params);
    }
#endif

#ifdef Q_OS_DARWIN
    if (graphicsApi == Metal) {
        QRhiMetalInitParams params;
        m_r = QRhi::create(QRhi::Metal, &params);
    }
#endif

    if (!m_r)
        qFatal("Failed to create RHI backend");

    // now onto the backend-independent init

    m_sc = m_r->newSwapChain();
    // allow depth-stencil, although we do not actually enable depth test/write for the triangle
    m_ds = m_r->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                QSize(), // we don't know the size yet, this is fine
                                1,
                                QRhiRenderBuffer::ToBeUsedWithSwapChainOnly);
    releasePool << m_ds;
    m_sc->setWindow(this);
    m_sc->setDepthStencil(m_ds);
    m_rp = m_sc->newCompatibleRenderPassDescriptor();
    releasePool << m_rp;
    m_sc->setRenderPassDescriptor(m_rp);

    m_vbuf = m_r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
    releasePool << m_vbuf;
    m_vbuf->build();
    m_vbufReady = false;

    m_ubuf = m_r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    releasePool << m_ubuf;
    m_ubuf->build();

    m_srb = m_r->newShaderResourceBindings();
    releasePool << m_srb;
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_ubuf)
    });
    m_srb->build();

    m_ps = m_r->newGraphicsPipeline();
    releasePool << m_ps;

    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
    premulAlphaBlend.enable = true;
    m_ps->setTargetBlends({ premulAlphaBlend });

    const QBakedShader vs = getShader(QLatin1String(":/color.vert.qsb"));
    if (!vs.isValid())
        qFatal("Failed to load shader pack (vertex)");
    const QBakedShader fs = getShader(QLatin1String(":/color.frag.qsb"));
    if (!fs.isValid())
        qFatal("Failed to load shader pack (fragment)");

    m_ps->setShaderStages({
        { QRhiGraphicsShaderStage::Vertex, vs },
        { QRhiGraphicsShaderStage::Fragment, fs }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.bindings = {
        { 5 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float2, 0 },
        { 0, 1, QRhiVertexInputLayout::Attribute::Float3, 2 * sizeof(float) }
    };

    m_ps->setVertexInputLayout(inputLayout);
    m_ps->setShaderResourceBindings(m_srb);
    m_ps->setRenderPassDescriptor(m_rp);

    m_ps->build();
}

void Window::releaseResources()
{
    for (QRhiResource *r : releasePool)
        r->releaseAndDestroy();

    releasePool.clear();

    delete m_sc;
    delete m_r;

#ifndef QT_NO_OPENGL
    delete m_context;
    delete m_fallbackSurface;
#endif
}

void Window::resizeSwapChain()
{
    const QSize outputSize = size() * devicePixelRatio();

    m_ds->setPixelSize(outputSize);
    m_ds->build(); // == m_ds->release(); m_ds->build();

    m_sc->setRequestedPixelSize(outputSize);
    m_hasSwapChain = m_sc->buildOrResize();

    m_elapsedMs = 0;
    m_elapsedCount = 0;

    const QSize outputSizeInPixels = m_sc->effectivePixelSize();
    m_proj = m_r->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, outputSizeInPixels.width() / (float) outputSizeInPixels.height(), 0.01f, 100.0f);
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
    if (m_sc->requestedPixelSize() != size() * devicePixelRatio() || m_newlyExposed) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        m_newlyExposed = false;
    }

    // Start a new frame. This is where we block when too far ahead of
    // GPU/present, and that's what throttles the thread to the refresh rate.
    // (except for OpenGL where it happens either in endFrame or somewhere else
    // depending on the GL implementation)
    QRhi::FrameOpResult r = m_r->beginFrame(m_sc);
    if (r == QRhi::FrameOpSwapChainOutOfDate) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        r = m_r->beginFrame(m_sc);
    }
    if (r != QRhi::FrameOpSuccess) {
        requestUpdate();
        return;
    }

    if (m_elapsedCount)
        m_elapsedMs += m_timer.elapsed();
    m_timer.restart();
    m_elapsedCount += 1;
    if (m_elapsedMs >= 4000) {
        qDebug("%f", m_elapsedCount / 4.0f);
        m_elapsedMs = 0;
        m_elapsedCount = 0;
    }

    // Set up buffer updates.
    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();
    if (!m_vbufReady) {
        m_vbufReady = true;
        u->uploadStaticBuffer(m_vbuf, vertexData);
    }
    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.rotate(m_rotation, 0, 1, 0);
    u->updateDynamicBuffer(m_ubuf, 0, 64, mvp.constData());
    m_opacity += m_opacityDir * 0.005f;
    if (m_opacity < 0.0f || m_opacity > 1.0f) {
        m_opacityDir *= -1;
        m_opacity = qBound(0.0f, m_opacity, 1.0f);
    }
    u->updateDynamicBuffer(m_ubuf, 64, 4, &m_opacity);

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    const QSize outputSizeInPixels = m_sc->effectivePixelSize();

    // Apply buffer updates, clear, start the renderpass (where applicable).
    cb->beginPass(m_sc->currentFrameRenderTarget(), { 0.4f, 0.7f, 0.0f, 1.0f }, { 1.0f, 0 }, u);

    cb->setGraphicsPipeline(m_ps);
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setVertexInput(0, { { m_vbuf, 0 } });
    cb->draw(3);

    cb->endPass();

    // Submit.
    m_r->endFrame(m_sc);

    requestUpdate(); // render continuously, throttled by the presentation rate (due to beginFrame above)
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    // Defaults.
#if defined(Q_OS_WIN)
    graphicsApi = D3D11;
#elif defined(Q_OS_DARWIN)
    graphicsApi = Metal;
#elif QT_CONFIG(vulkan)
    graphicsApi = Vulkan;
#else
    graphicsApi = OpenGL;
#endif

    // Allow overriding via the command line.
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

    // OpenGL specifics.
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    // Vulkan setup.
#if QT_CONFIG(vulkan)
    QVulkanInstance inst;
    if (graphicsApi == Vulkan) {
        if (!inst.create()) {
            qWarning("Failed to create Vulkan instance, switching to OpenGL");
            graphicsApi = OpenGL;
        }
    }
#endif

    // Create and show the window.
    Window w;
#if QT_CONFIG(vulkan)
    if (graphicsApi == Vulkan)
        w.setVulkanInstance(&inst);
#endif
    w.resize(1280, 720);
    w.setTitle(graphicsApiName());
    w.show();

    return app.exec();
}
