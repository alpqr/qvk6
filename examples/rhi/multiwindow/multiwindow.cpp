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

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

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

static struct {
#if QT_CONFIG(vulkan)
    QVulkanInstance *instance = nullptr;
#endif
    QRhi *r = nullptr;
#ifndef QT_NO_OPENGL
    QOpenGLContext *context = nullptr;
    QOffscreenSurface *fallbackSurface = nullptr;
#endif
} r;

void createRhi()
{
#ifndef QT_NO_OPENGL
    if (graphicsApi == OpenGL) {
        r.context = new QOpenGLContext;
        if (!r.context->create())
            qFatal("Failed to get OpenGL context");

        r.fallbackSurface = new QOffscreenSurface;
        r.fallbackSurface->setFormat(r.context->format());
        r.fallbackSurface->create();

        QRhiGles2InitParams params;
        params.context = r.context;
        //params.window = this;
        params.fallbackSurface = r.fallbackSurface;
        r.r = QRhi::create(QRhi::OpenGLES2, &params);
    }
#endif

#if QT_CONFIG(vulkan)
    if (graphicsApi == Vulkan) {
        QRhiVulkanInitParams params;
        params.inst = r.instance;
        //params.window = this;
        r.r = QRhi::create(QRhi::Vulkan, &params);
    }
#endif

#ifdef Q_OS_WIN
    if (graphicsApi == D3D11) {
        QRhiD3D11InitParams params;
        r.r = QRhi::create(QRhi::D3D11, &params);
    }
#endif

#ifdef Q_OS_DARWIN
    if (graphicsApi == Metal) {
        QRhiMetalInitParams params;
        r.r = QRhi::create(QRhi::Metal, &params);
    }
#endif

    if (!r.r)
        qFatal("Failed to create RHI backend");
}

void destroyRhi()
{
    delete r.r;

#ifndef QT_NO_OPENGL
    delete r.context;
    delete r.fallbackSurface;
#endif
}

struct {
    QVector<QWindow *> windows;

    QRhiBuffer *vbuf = nullptr;
    QRhiBuffer *ubuf = nullptr;
    QRhiShaderResourceBindings *srb = nullptr;
    QRhiGraphicsPipeline *ps = nullptr;
    QRhiResourceUpdateBatch *initialUpdates = nullptr;
} d;

static float vertexData[] = {
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

// can use just one rpd from whichever window comes first since they are
// actually compatible due to all windows using the same config (have
// depth-stencil, sample count 1, same format). this means the same pso can be
// reused too.
void ensureSharedResources(QRhiRenderPassDescriptor *rp)
{
    if (!d.vbuf) {
        d.vbuf = r.r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
        d.vbuf->build();
        d.initialUpdates = r.r->nextResourceUpdateBatch();
        d.initialUpdates->uploadStaticBuffer(d.vbuf, vertexData);
    }

    if (!d.ubuf) {
        d.ubuf = r.r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
        d.ubuf->build();
    }

    if (!d.srb) {
        d.srb = r.r->newShaderResourceBindings();
        d.srb->setBindings({
                               QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d.ubuf)
                           });
        d.srb->build();
    }

    if (!d.ps) {
        d.ps = r.r->newGraphicsPipeline();

        QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
        premulAlphaBlend.enable = true;
        d.ps->setTargetBlends({ premulAlphaBlend });

        const QBakedShader vs = getShader(QLatin1String(":/color.vert.qsb"));
        if (!vs.isValid())
            qFatal("Failed to load shader pack (vertex)");
        const QBakedShader fs = getShader(QLatin1String(":/color.frag.qsb"));
        if (!fs.isValid())
            qFatal("Failed to load shader pack (fragment)");

        d.ps->setShaderStages({
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

        d.ps->setVertexInputLayout(inputLayout);
        d.ps->setShaderResourceBindings(d.srb);
        d.ps->setRenderPassDescriptor(rp);

        d.ps->build();
    }
}

void destroySharedResources()
{
    if (d.ps) {
        d.ps->releaseAndDestroy();
        d.ps = nullptr;
    }
    if (d.srb) {
        d.srb->releaseAndDestroy();
        d.srb = nullptr;
    }
    if (d.vbuf) {
        d.vbuf->releaseAndDestroy();
        d.vbuf = nullptr;
    }
    if (d.ubuf) {
        d.ubuf->releaseAndDestroy();
        d.ubuf = nullptr;
    }
}

class Window : public QWindow
{
public:
    Window(const QString &title, const QColor &bgColor, int axis);
    ~Window();

protected:
    void init();
    void releaseResources();
    void resizeSwapChain();
    void releaseSwapChain();
    void render();

    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

    QColor m_bgColor;
    int m_rotationAxis = 0;

    bool m_running = false;
    bool m_notExposed = false;
    bool m_newlyExposed = false;

    QMatrix4x4 m_proj;
    QVector<QRhiResource *> m_releasePool;

    bool m_hasSwapChain = false;
    QRhiSwapChain *m_sc = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;
    QRhiRenderPassDescriptor *m_rp = nullptr;

    float m_rotation = 0;
    float m_opacity = 1;
    int m_opacityDir = -1;
};

Window::Window(const QString &title, const QColor &bgColor, int axis)
    : m_bgColor(bgColor),
      m_rotationAxis(axis)
{
    switch (graphicsApi) {
    case OpenGL:
        setSurfaceType(OpenGLSurface);
        break;
    case Vulkan:
        setSurfaceType(VulkanSurface);
        setVulkanInstance(r.instance);
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
    m_sc = r.r->newSwapChain();
    m_ds = r.r->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                QSize(), // no need to set the size yet
                                1,
                                QRhiRenderBuffer::ToBeUsedWithSwapChainOnly);
    m_releasePool << m_ds;
    m_sc->setWindow(this);
    m_sc->setDepthStencil(m_ds);
    m_rp = m_sc->newCompatibleRenderPassDescriptor();
    m_releasePool << m_rp;
    m_sc->setRenderPassDescriptor(m_rp);

    ensureSharedResources(m_rp);
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
}

void Window::resizeSwapChain()
{
    const QSize outputSize = m_sc->surfacePixelSize();

    m_ds->setPixelSize(outputSize);
    m_ds->build();

    m_hasSwapChain = m_sc->buildOrResize();

    m_proj = r.r->clipSpaceCorrMatrix();
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

    QRhi::FrameOpResult result = r.r->beginFrame(m_sc);
    if (result == QRhi::FrameOpSwapChainOutOfDate) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        result = r.r->beginFrame(m_sc);
    }
    if (result != QRhi::FrameOpSuccess) {
        requestUpdate();
        return;
    }

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    const QSize outputSizeInPixels = m_sc->currentPixelSize();

    QRhiResourceUpdateBatch *u = r.r->nextResourceUpdateBatch();
    if (d.initialUpdates) {
        u->merge(d.initialUpdates);
        d.initialUpdates->release();
        d.initialUpdates = nullptr;
    }

    m_rotation += 1.0f;
    QMatrix4x4 mvp = m_proj;
    mvp.rotate(m_rotation, m_rotationAxis == 0 ? 1 : 0, m_rotationAxis == 1 ? 1 : 0, m_rotationAxis == 2 ? 1 : 0);
    u->updateDynamicBuffer(d.ubuf, 0, 64, mvp.constData());
    m_opacity += m_opacityDir * 0.005f;
    if (m_opacity < 0.0f || m_opacity > 1.0f) {
        m_opacityDir *= -1;
        m_opacity = qBound(0.0f, m_opacity, 1.0f);
    }
    u->updateDynamicBuffer(d.ubuf, 64, 4, &m_opacity);

    cb->beginPass(m_sc->currentFrameRenderTarget(),
                  { float(m_bgColor.redF()), float(m_bgColor.greenF()), float(m_bgColor.blueF()), 1.0f },
                  { 1.0f, 0 },
                  u);

    cb->setGraphicsPipeline(d.ps);
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setVertexInput(0, { { d.vbuf, 0 } });
    cb->draw(3);

    cb->endPass();

    r.r->endFrame(m_sc);

    requestUpdate();
}

void createWindow()
{
    static QColor colors[] = { Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::cyan, Qt::gray };
    const int n = d.windows.count();
    d.windows.append(new Window(QString::asprintf("Window #%d", n), colors[n % 6], n % 3));
    d.windows.last()->show();
}

void closeWindow()
{
    delete d.windows.takeLast();
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

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

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

#if QT_CONFIG(vulkan)
    r.instance = new QVulkanInstance;
    if (graphicsApi == Vulkan) {
#ifndef Q_OS_ANDROID
        r.instance->setLayers({ "VK_LAYER_LUNARG_standard_validation" });
#else
        r.instance->setLayers(QByteArrayList()
                       << "VK_LAYER_GOOGLE_threading"
                       << "VK_LAYER_LUNARG_parameter_validation"
                       << "VK_LAYER_LUNARG_object_tracker"
                       << "VK_LAYER_LUNARG_core_validation"
                       << "VK_LAYER_LUNARG_image"
                       << "VK_LAYER_LUNARG_swapchain"
                       << "VK_LAYER_GOOGLE_unique_objects");
#endif
        if (!r.instance->create()) {
            qWarning("Failed to create Vulkan instance, switching to OpenGL");
            graphicsApi = OpenGL;
        }
    }
#endif

    createRhi();

    int winCount = 0;
    QWidget w;
    w.resize(800, 600);
    w.setWindowTitle(QCoreApplication::applicationName() + QLatin1String(" - ") + graphicsApiName());
    QVBoxLayout *layout = new QVBoxLayout(&w);

    QPlainTextEdit *info = new QPlainTextEdit(
                QLatin1String("This application tests rendering with the same QRhi instance (and so the same Vulkan/Metal/D3D device or OpenGL context) "
                              "to multiple windows via multiple QRhiSwapChain objects, from the same one thread. Some resources are reused across all windows."
                              "\n\nUsing API: ") + graphicsApiName());
    info->setReadOnly(true);
    layout->addWidget(info);
    QLabel *label = new QLabel(QLatin1String("Window count: 0"));
    layout->addWidget(label);
    QPushButton *btn = new QPushButton(QLatin1String("New window"));
    QObject::connect(btn, &QPushButton::clicked, btn, [label, &winCount] {
        winCount += 1;
        label->setText(QString::asprintf("Window count: %d", winCount));
        createWindow();
    });
    layout->addWidget(btn);
    btn = new QPushButton(QLatin1String("Close window"));
    QObject::connect(btn, &QPushButton::clicked, btn, [label, &winCount] {
        if (winCount > 0) {
            winCount -= 1;
            label->setText(QString::asprintf("Window count: %d", winCount));
            closeWindow();
        }
    });
    layout->addWidget(btn);
    w.show();

    int result = app.exec();

    qDeleteAll(d.windows);

    destroySharedResources();
    destroyRhi();

#if QT_CONFIG(vulkan)
    delete r.instance;
#endif

    return result;
}
