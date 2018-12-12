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
#include <QWaitCondition>
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
    QMutex mutex;
    QWaitCondition cond;
};

struct Renderer
{
    Renderer(QWindow *w);
    ~Renderer();

    QWindow *window;
    Thread *thread;
    QRhi *r = nullptr;
#ifndef QT_NO_OPENGL
    QOpenGLContext *context = nullptr;
    QOffscreenSurface *fallbackSurface = nullptr;
#endif

    void createRhi();
    void destroyRhi();
};

void Thread::run()
{
    while (active) {
        qDebug() << QThread::currentThread();
    }

#ifndef QT_NO_OPENGL
    if (renderer->context)
        renderer->context->moveToThread(qGuiApp->thread());
#endif
}

Renderer::Renderer(QWindow *w)
    : window(w)
{
    thread = new Thread(this);

    // ctor and dtor are called main thread, rest on the render thread
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
{
    thread->active = false;
    thread->wait();
    delete thread;

#ifndef QT_NO_OPENGL
    delete context;
    delete fallbackSurface;
#endif
}

void Renderer::createRhi()
{
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
}

class Window : public QWindow
{
public:
    Window(const QString &title, const QColor &bgColor, int axis);
    ~Window();

protected:
    QColor m_bgColor;
    int m_rotationAxis = 0;
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
    QWindow *w = new Window(QString::asprintf("Window #%d", n), colors[n % 6], n % 3);
    windows.append({ w, new Renderer(w) });
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
                QLatin1String("This application tests rendering on a separate thread per window, with dedicated QRhi instances still sharing the same graphics device. "
                              "No resources are shared across windows here. (so no synchronization mess) "
                              "Note that this is only safe with D3D/DXGI if the main (gui) thread is not blocked when issuing the Present."
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
