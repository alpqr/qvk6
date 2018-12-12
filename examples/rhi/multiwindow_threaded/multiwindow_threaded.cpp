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

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QEvent>
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

#if QT_CONFIG(vulkan)
QVulkanInstance *instance = nullptr;
#endif

// Window (main thread) emit signals -> Renderer::send* (main thread) -> event queue (add on main, process on render thread) -> Renderer::renderEvent (render thread)

// event queue is taken from the Qt Quick scenegraph as-is
// all this below is conceptually the same as the QSG threaded render loop
class RenderThreadEventQueue : public QQueue<QEvent *>
{
public:
    RenderThreadEventQueue()
        : waiting(false)
    {
    }

    void addEvent(QEvent *e) {
        mutex.lock();
        enqueue(e);
        if (waiting)
            condition.wakeOne();
        mutex.unlock();
    }

    QEvent *takeEvent(bool wait) {
        mutex.lock();
        if (isEmpty() && wait) {
            waiting = true;
            condition.wait(&mutex);
            waiting = false;
        }
        QEvent *e = dequeue();
        mutex.unlock();
        return e;
    }

    bool hasMoreEvents() {
        mutex.lock();
        bool has = !isEmpty();
        mutex.unlock();
        return has;
    }

private:
    QMutex mutex;
    QWaitCondition condition;
    bool waiting;
};

struct Renderer;

struct Thread : public QThread
{
    Thread(Renderer *renderer_)
        : renderer(renderer_)
    {
        active = true;
        start();
    }
    void run() override;

    Renderer *renderer;
    bool active;
    RenderThreadEventQueue eventQueue;
    bool sleeping = false;
    bool stopEventProcessing = false;
    bool pendingRender = false;
    bool pendingRenderIsNewExpose = false;
    // mutex and cond used to allow the main thread waiting until something completes on the render thread
    QMutex mutex;
    QWaitCondition cond;
};

class RenderThreadEvent : public QEvent
{
public:
    RenderThreadEvent(QEvent::Type type) : QEvent(type) { }
};

class InitEvent : public RenderThreadEvent
{
public:
    static const QEvent::Type TYPE = QEvent::Type(QEvent::User + 1);
    InitEvent() : RenderThreadEvent(TYPE)
    { }
};

class RenderEvent : public RenderThreadEvent
{
public:
    static const QEvent::Type TYPE = QEvent::Type(QEvent::User + 2);
    RenderEvent(bool newlyExposed_) : RenderThreadEvent(TYPE), newlyExposed(newlyExposed_)
    { }
    bool newlyExposed;
};

class SurfaceCleanupEvent : public RenderThreadEvent
{
public:
    static const QEvent::Type TYPE = QEvent::Type(QEvent::User + 3);
    SurfaceCleanupEvent() : RenderThreadEvent(TYPE)
    { }
};

class CloseEvent : public RenderThreadEvent
{
public:
    static const QEvent::Type TYPE = QEvent::Type(QEvent::User + 4);
    CloseEvent() : RenderThreadEvent(TYPE)
    { }
};

struct Renderer
{
    // ctor and dtor and send* are called main thread, rest on the render thread

    Renderer(QWindow *w);
    ~Renderer();

    void sendInit();
    void sendRender(bool newlyExposed);
    void sendSurfaceGoingAway();

    QWindow *window;
    Thread *thread;
    QRhi *r = nullptr;
#ifndef QT_NO_OPENGL
    QOpenGLContext *context = nullptr;
    QOffscreenSurface *fallbackSurface = nullptr;
#endif

    void createRhi();
    void destroyRhi();
    void renderEvent(QEvent *e);
    void init();
    void releaseSwapChain();
    void releaseResources();
    void render(bool newlyExposed);

    QVector<QRhiResource *> m_releasePool;
    bool m_hasSwapChain = false;
    QRhiSwapChain *m_sc = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;
    QRhiRenderPassDescriptor *m_rp = nullptr;
};

void Thread::run()
{
    while (active) {
        if (pendingRender) {
            pendingRender = false;
            renderer->render(pendingRenderIsNewExpose);
        }

        while (eventQueue.hasMoreEvents()) {
            QEvent *e = eventQueue.takeEvent(false);
            renderer->renderEvent(e);
            delete e;
        }

        if (active && !pendingRender) {
            sleeping = true;
            stopEventProcessing = false;
            while (!stopEventProcessing) {
                QEvent *e = eventQueue.takeEvent(true);
                renderer->renderEvent(e);
                delete e;
            }
            sleeping = false;
        }
    }

#ifndef QT_NO_OPENGL
    if (renderer->context)
        renderer->context->moveToThread(qGuiApp->thread());
#endif
}

Renderer::Renderer(QWindow *w)
    : window(w)
{ // main thread
    thread = new Thread(this);

#ifndef QT_NO_OPENGL
    if (graphicsApi == OpenGL) {
        context = new QOpenGLContext;
        if (!context->create())
            qFatal("Failed to get OpenGL context");

        fallbackSurface = new QOffscreenSurface;
        fallbackSurface->setFormat(context->format());
        fallbackSurface->create();

        context->moveToThread(thread);
    }
#endif
}

Renderer::~Renderer()
{ // main thread
    thread->eventQueue.addEvent(new CloseEvent);
    thread->wait();
    delete thread;

#ifndef QT_NO_OPENGL
    delete context;
    delete fallbackSurface;
#endif
}

void Renderer::createRhi()
{
    if (r)
        return;

#ifndef QT_NO_OPENGL
    if (graphicsApi == OpenGL) {
        QRhiGles2InitParams params;
        params.context = context;
        params.window = window;
        params.fallbackSurface = fallbackSurface;
        r = QRhi::create(QRhi::OpenGLES2, &params);
    }
#endif

#if QT_CONFIG(vulkan)
    if (graphicsApi == Vulkan) {
        QRhiVulkanInitParams params;
        params.inst = instance;
        params.window = window;
        r = QRhi::create(QRhi::Vulkan, &params);
    }
#endif

#ifdef Q_OS_WIN
    if (graphicsApi == D3D11) {
        QRhiD3D11InitParams params;
        r = QRhi::create(QRhi::D3D11, &params);
    }
#endif

#ifdef Q_OS_DARWIN
    if (graphicsApi == Metal) {
        QRhiMetalInitParams params;
        r = QRhi::create(QRhi::Metal, &params);
    }
#endif

    if (!r)
        qFatal("Failed to create RHI backend");
}

void Renderer::destroyRhi()
{
    delete r;
    r = nullptr;
}

void Renderer::renderEvent(QEvent *e)
{
    Q_ASSERT(QThread::currentThread() == thread);

    if (thread->sleeping)
        thread->stopEventProcessing = true;

    switch (e->type()) {
    case InitEvent::TYPE:
        qDebug() << "renderer" << this << "for window" << window << "is initializing";
        createRhi();
        init();
        break;
    case RenderEvent::TYPE:
        thread->pendingRender = true;
        thread->pendingRenderIsNewExpose = static_cast<RenderEvent *>(e)->newlyExposed;
        break;
    case SurfaceCleanupEvent::TYPE: // when the QWindow is closed, before QPlatformWindow goes away
        thread->mutex.lock();
        qDebug() << "renderer" << this << "for window" << window << "is destroying swapchain";
        releaseSwapChain();
        thread->cond.wakeOne();
        thread->mutex.unlock();
        break;
    case CloseEvent::TYPE: // when destroying the window+renderer (NB not the same as hitting X on the window, that's just QWindow close)
        qDebug() << "renderer" << this << "for window" << window << "is shutting down";
        thread->active = false;
        thread->stopEventProcessing = true;
        releaseResources();
        destroyRhi();
        break;
    default:
        break;
    }
}

void Renderer::init()
{
    m_sc = r->newSwapChain();
    m_ds = r->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                              QSize(), // no need to set the size yet
                              1,
                              QRhiRenderBuffer::ToBeUsedWithSwapChainOnly);
    m_releasePool << m_ds;
    m_sc->setWindow(window);
    m_sc->setDepthStencil(m_ds);
    m_rp = m_sc->newCompatibleRenderPassDescriptor();
    m_releasePool << m_rp;
    m_sc->setRenderPassDescriptor(m_rp);
}

void Renderer::releaseSwapChain()
{
    if (m_hasSwapChain) {
        m_hasSwapChain = false;
        m_sc->release();
    }
}

void Renderer::releaseResources()
{
    for (QRhiResource *res : m_releasePool)
        res->releaseAndDestroy();

    m_releasePool.clear();

    if (m_sc) {
        m_sc->releaseAndDestroy();
        m_sc = nullptr;
    }
}

void Renderer::render(bool newlyExposed)
{
    auto buildOrResizeSwapChain = [this] {
        qDebug() << "renderer" << this << "build or resize swapchain for window" << window;
        const QSize outputSize = m_sc->surfacePixelSize();
        qDebug() << "  size is" << outputSize;
        m_ds->setPixelSize(outputSize);
        m_ds->build();
        m_hasSwapChain = m_sc->buildOrResize();
    };

    if (newlyExposed || m_sc->currentPixelSize() != m_sc->surfacePixelSize())
        buildOrResizeSwapChain();

    if (!m_hasSwapChain)
        return;

    QRhi::FrameOpResult result = r->beginFrame(m_sc);
    if (result == QRhi::FrameOpSwapChainOutOfDate) {
        buildOrResizeSwapChain();
        if (!m_hasSwapChain)
            return;
        result = r->beginFrame(m_sc);
    }
    if (result != QRhi::FrameOpSuccess)
        return;

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();

    cb->beginPass(m_sc->currentFrameRenderTarget(), { 0.4f, 0.7f, 0.0f, 1.0f }, { 1.0f, 0 });
    cb->endPass();

    r->endFrame(m_sc);
}

void Renderer::sendInit()
{ // main thread
    InitEvent *e = new InitEvent;
    thread->eventQueue.addEvent(e);
}

void Renderer::sendRender(bool newlyExposed)
{ // main thread
    RenderEvent *e = new RenderEvent(newlyExposed);
    thread->eventQueue.addEvent(e);
}

void Renderer::sendSurfaceGoingAway()
{ // main thread
    SurfaceCleanupEvent *e = new SurfaceCleanupEvent;

    // cannot let this thread to proceed with tearing down the native window
    // before the render thread completes the swapchain release
    thread->mutex.lock();

    thread->eventQueue.addEvent(e);

    thread->cond.wait(&thread->mutex);
    thread->mutex.unlock();
}

class Window : public QWindow
{
    Q_OBJECT

public:
    Window(const QString &title, const QColor &bgColor, int axis);
    ~Window();

    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;

signals:
    void initRequested();
    void renderRequested(bool newlyExposed);
    void surfaceGoingAway();

protected:
    QColor m_bgColor;
    int m_rotationAxis = 0;

    bool m_running = false;
    bool m_notExposed = true;
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
        setVulkanInstance(instance);
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
}

void Window::exposeEvent(QExposeEvent *)
{
    // initialize and start rendering when the window becomes usable for graphics purposes
    if (isExposed() && !m_running) {
        m_running = true;
        m_notExposed = false;
        emit initRequested();
        emit renderRequested(true);
    }

    // stop pushing frames when not exposed (on some platforms this is essential, optional on others)
    if (!isExposed() && m_running)
        m_notExposed = true;

    // continue when exposed again
    if (isExposed() && m_running && m_notExposed) {
        m_notExposed = false;
        emit renderRequested(true);
    }
}

bool Window::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::UpdateRequest:
        if (!m_notExposed)
            emit renderRequested(false);
        break;

    case QEvent::PlatformSurface:
        // this is the proper time to tear down the swapchain (while the native window and surface are still around)
        if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            emit surfaceGoingAway();
        break;

    default:
        break;
    }

    return QWindow::event(e);
}

struct WindowAndRenderer
{
    QWindow *window;
    Renderer *renderer;
};

QVector<WindowAndRenderer> windows;

void createWindow()
{
    static QColor colors[] = { Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::cyan, Qt::gray };
    const int n = windows.count();
    Window *w = new Window(QString::asprintf("Window #%d", n), colors[n % 6], n % 3);
    Renderer *renderer = new Renderer(w);
    QObject::connect(w, &Window::initRequested, w, [renderer] {
        renderer->sendInit();
    });
    QObject::connect(w, &Window::renderRequested, w, [w, renderer](bool newlyExposed) {
        renderer->sendRender(newlyExposed);
        w->requestUpdate();
    });
    QObject::connect(w, &Window::surfaceGoingAway, w, [renderer] {
        renderer->sendSurfaceGoingAway();
    });
    windows.append({ w, renderer });
    w->show();
}

void closeWindow()
{
    WindowAndRenderer wr = windows.takeLast();
    delete wr.renderer;
    delete wr.window;
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
    instance = new QVulkanInstance;
    if (graphicsApi == Vulkan) {
#ifndef Q_OS_ANDROID
        instance->setLayers({ "VK_LAYER_LUNARG_standard_validation" });
#else
        instance->setLayers(QByteArrayList()
                       << "VK_LAYER_GOOGLE_threading"
                       << "VK_LAYER_LUNARG_parameter_validation"
                       << "VK_LAYER_LUNARG_object_tracker"
                       << "VK_LAYER_LUNARG_core_validation"
                       << "VK_LAYER_LUNARG_image"
                       << "VK_LAYER_LUNARG_swapchain"
                       << "VK_LAYER_GOOGLE_unique_objects");
#endif
        if (!instance->create()) {
            qWarning("Failed to create Vulkan instance, switching to OpenGL");
            graphicsApi = OpenGL;
        }
    }
#endif

    int winCount = 0;
    QWidget w;
    w.resize(800, 600);
    w.setWindowTitle(QCoreApplication::applicationName() + QLatin1String(" - ") + graphicsApiName());
    QVBoxLayout *layout = new QVBoxLayout(&w);

    QPlainTextEdit *info = new QPlainTextEdit(
                QLatin1String("This application tests rendering on a separate thread per window, with dedicated QRhi instances. " // ### still sharing the same graphics device where applicable
                              "No resources are shared across windows here. (so no synchronization mess) "
                              "\n\nNote that this is only safe with D3D/DXGI if the main (gui) thread is not blocked when issuing the Present."
                              "\n\nThis is the same concept as the Qt Quick Scenegraph's threaded render loop. This should allow rendering to the different windows "
                              "without unintentionally throttling each other's threads."
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

    for (const WindowAndRenderer &wr : windows) {
        delete wr.renderer;
        delete wr.window;
    }

#if QT_CONFIG(vulkan)
    delete instance;
#endif

    return result;
}

#include "multiwindow_threaded.moc"
