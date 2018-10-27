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

static float vertexData[] = { // Y up (note m_proj), CCW
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f
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
    void recreateSwapChain();
    void releaseSwapChain();
    void render();

    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

    bool m_running = false;
    bool m_notExposed = false;
    bool m_newlyExposed = false;

    QRhi *m_r = nullptr;
    bool m_hasSwapChain = false;
    bool m_swapChainChanged = false;
    QRhiSwapChain *m_sc = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;

    QRhiBuffer *m_vbuf = nullptr;
    bool m_vbufReady = false;
    QRhiBuffer *m_ubuf = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiGraphicsPipeline *m_ps = nullptr;

    QMatrix4x4 m_proj;
    float m_rotation = 0;
    float m_opacity = 1;
    int m_opacityDir = -1;

    QElapsedTimer m_timer;
    qint64 m_elapsedMs;
    int m_elapsedCount;

#ifndef QT_NO_OPENGL
    QOpenGLContext *m_context = nullptr;
#endif
};

Window::Window()
{
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
    if (!isExposed() && m_running)
        m_notExposed = true;

    if (isExposed() && m_running && m_notExposed) {
        m_notExposed = false;
        m_newlyExposed = true;
        render();
    }

    if (isExposed() && !m_running) {
        m_running = true;
        init();
        recreateSwapChain();
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

        QRhiGles2InitParams params;
        params.context = m_context;
        params.window = this;
        params.fallbackSurface = new QOffscreenSurface;
        params.fallbackSurface->setFormat(m_context->format());
        params.fallbackSurface->create();
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

    m_sc = m_r->createSwapChain();

    m_vbuf = m_r->createBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
    m_vbuf->build();
    m_vbufReady = false;

    m_ubuf = m_r->createBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    m_ubuf->build();

    m_srb = m_r->createShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_ubuf)
    });
    m_srb->build();
}

void Window::releaseResources()
{
    if (m_srb) {
        m_srb->releaseAndDestroy();
        m_srb = nullptr;
    }

    if (m_ubuf) {
        m_ubuf->releaseAndDestroy();
        m_ubuf = nullptr;
    }

    if (m_vbuf) {
        m_vbuf->releaseAndDestroy();
        m_vbuf = nullptr;
    }

    delete m_sc;
    m_sc = nullptr;

    delete m_r;
    m_r = nullptr;

#ifndef QT_NO_OPENGL
    delete m_context;
    m_context = nullptr;
#endif
}

void Window::recreateSwapChain()
{
    if (!m_sc)
        return;

    const QSize outputSize = size() * devicePixelRatio();

    if (!m_ds) {
        m_ds = m_r->createRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                       outputSize,
                                       1,
                                       QRhiRenderBuffer::ToBeUsedWithSwapChainOnly);
    } else {
        m_ds->release();
        m_ds->setPixelSize(outputSize);
    }
    m_ds->build();

    m_hasSwapChain = m_sc->build(this, outputSize, 0, m_ds, 1);
    m_swapChainChanged = true;

    m_elapsedMs = 0;
    m_elapsedCount = 0;

    m_ps = m_r->createGraphicsPipeline();

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
        { 7 * sizeof(float) }
    };
    inputLayout.attributes = {
        { 0, 0, QRhiVertexInputLayout::Attribute::Float2, 0 },
        { 0, 1, QRhiVertexInputLayout::Attribute::Float3, 2 * sizeof(float) }
    };

    m_ps->setVertexInputLayout(inputLayout);
    m_ps->setShaderResourceBindings(m_srb);
    m_ps->setRenderPass(m_sc->defaultRenderPass());

    m_ps->build();

    const QSize outputSizeInPixels = m_sc->effectiveSizeInPixels();
    m_proj = m_r->clipSpaceCorrMatrix();
    m_proj.perspective(45.0f, outputSizeInPixels.width() / (float) outputSizeInPixels.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);
}

void Window::releaseSwapChain()
{
    if (m_ps) {
        m_ps->releaseAndDestroy();
        m_ps = nullptr;
    }
    if (m_hasSwapChain) {
        m_hasSwapChain = false;
        m_sc->release();
    }
    if (m_ds) {
        m_ds->releaseAndDestroy();
        m_ds = nullptr;
    }
}

void Window::render()
{
    if (!m_hasSwapChain || m_notExposed)
        return;

    if (m_sc->requestedSizeInPixels() != size() * devicePixelRatio() || m_newlyExposed) {
        recreateSwapChain();
        if (!m_hasSwapChain)
            return;
        m_newlyExposed = false;
    }

    QRhi::FrameOpResult r = m_r->beginFrame(m_sc);
    if (r == QRhi::FrameOpSwapChainOutOfDate) {
        recreateSwapChain();
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
    const QSize outputSizeInPixels = m_sc->effectiveSizeInPixels();

    m_r->beginPass(m_sc->currentFrameRenderTarget(), cb, { 0.4f, 0.7f, 0.0f, 1.0f }, { 1.0f, 0 }, u);

    m_r->setGraphicsPipeline(cb, m_ps);
    m_r->setViewport(cb, QRhiViewport(0, 0, outputSizeInPixels.width(), outputSizeInPixels.height()));
    m_r->setVertexInput(cb, 0, { { m_vbuf, 0 } });
    m_r->draw(cb, 3);

    m_r->endPass(cb);

    m_r->endFrame(m_sc);

    requestUpdate(); // render continuously, throttled by the presentation rate
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
