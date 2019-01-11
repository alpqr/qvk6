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

#include "renderwindow.h"
#include <QTimer>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QOpenGLExtraFunctions>
#include <private/qspirvshader_p.h>

RenderWindow::RenderWindow(const QSpirvShader &vs, const QSpirvShader &fs, const QSurfaceFormat &format)
    : m_context(0),
      m_initialized(false),
      m_angle(0.0f),
      m_vs(vs),
      m_fs(fs)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setFormat(format);
    m_context = new QOpenGLContext(this);
    m_context->setFormat(requestedFormat());
    if (!m_context->create())
        qWarning("Failed to create context");
}

void RenderWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed())
        render();
}

static GLfloat vertices[] = {
    0.0f, 0.5f,
    -0.5f, -0.5f,
    0.5f, -0.5f
};

static GLfloat colors[] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
};

void RenderWindow::init()
{
    m_program = new QOpenGLShaderProgram(this);

    QSurfaceFormat format = m_context->format();

    // Now the fun part:

    int version = 120;
    const bool es = format.renderableType() == QSurfaceFormat::OpenGLES;
    if (!es && format.profile() == QSurfaceFormat::CoreProfile)
        version = 330;

    QSpirvShader::GlslFlags flags = 0;
    if (es) {
        version = 100;
        flags |= QSpirvShader::GlslEs;
    }

    const QByteArray vertexShaderSource = m_vs.translateToGLSL(version, flags);
    const QByteArray fragmentShaderSource = m_fs.translateToGLSL(version, flags);

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        emit error(m_program->log());
        return;
    }
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        emit error(m_program->log());
        return;
    }
    if (!m_program->link()) {
        emit error(m_program->log());
        return;
    }

    m_posAttr = m_program->attributeLocation("position");
    m_colAttr = m_program->attributeLocation("color");

    const QShaderDescription vsDesc = m_vs.shaderDescription();

    // ### maybe move this to a helper api?
    if (version == 330) {
        // use a true uniform buffer
        QOpenGLExtraFunctions *f = m_context->extraFunctions();
        const QString bname = vsDesc.uniformBlocks()[0].blockName;
        GLuint blockIdx = f->glGetUniformBlockIndex(m_program->programId(), bname.toUtf8().constData());
        if (blockIdx == GL_INVALID_INDEX)
            qWarning("uniform block %s not found?!", qPrintable(bname));
        f->glUniformBlockBinding(m_program->programId(), blockIdx, 0);
        f->glGenBuffers(1, &m_ubo);
        f->glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_ubo);
    } else {
        // without uniform buffer support we have a mere struct
        const QString uniName = vsDesc.uniformBlocks()[0].structName + QLatin1String(".mvp");
        m_matrixUniform = m_program->uniformLocation(uniName);
    }

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices) + sizeof(colors));
    m_vbo.write(sizeof(vertices), colors, sizeof(colors));
    m_vbo.release();

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
    if (m_vao.isCreated())
        setupVertexAttribs();
}

void RenderWindow::setupVertexAttribs()
{
    m_vbo.bind();
    m_program->setAttributeBuffer(m_posAttr, GL_FLOAT, 0, 2);
    m_program->setAttributeBuffer(m_colAttr, GL_FLOAT, sizeof(vertices), 3);
    m_program->enableAttributeArray(m_posAttr);
    m_program->enableAttributeArray(m_colAttr);
    m_vbo.release();
}

void RenderWindow::render()
{
    if (!m_context->makeCurrent(this)) {
        emit error(tr("makeCurrent() failed"));
        return;
    }

    QOpenGLExtraFunctions *f = m_context->extraFunctions();
    if (!m_initialized) {
        m_initialized = true;
        f->glEnable(GL_DEPTH_TEST);
        f->glClearColor(0, 0, 0, 1);
        init();
        emit ready();
    }

    if (!m_vbo.isCreated())
        return;

    const qreal retinaScale = devicePixelRatio();
    f->glViewport(0, 0, width() * retinaScale, height() * retinaScale);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_program->bind();
    QMatrix4x4 matrix;
    matrix.perspective(45.0f, width() / float(height()), 0.01f, 100.0f);
    matrix.translate(0.0f, 0.0f, -4.0f);
    matrix.rotate(m_angle, 0.0f, 1.0f, 0.0f);

    if (m_vao.isCreated())
        m_vao.bind();
    else
        setupVertexAttribs();

    if (m_ubo) {
        f->glBufferData(GL_UNIFORM_BUFFER, 64, matrix.constData(), GL_DYNAMIC_DRAW);
    } else {
        m_program->setUniformValue(m_matrixUniform, matrix);
    }

    f->glDrawArrays(GL_TRIANGLES, 0, 3);

    m_vao.release();
    m_program->release();

    m_context->swapBuffers(this);

    m_angle += 1.0f;

    const int interval = 5;
    QTimer::singleShot(interval, this, &RenderWindow::render);
}
