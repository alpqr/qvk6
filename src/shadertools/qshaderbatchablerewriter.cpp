/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Shader Tools module
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qshaderbatchablerewriter_p.h"

// This is a slightly modified version of qsgshaderrewriter.cpp from qtdeclarative/src/quick/scenegraph/coreapi.
//
// It generates a push constant block (_qt) with a single member (zRange) and a vertex attribute (_qt_order) at location 7.
//

QT_BEGIN_NAMESPACE

namespace QShaderBatchableRewriter {

struct Tokenizer {

    enum Token {
        Token_Void,
        Token_OpenBrace,
        Token_CloseBrace,
        Token_SemiColon,
        Token_Identifier,
        Token_Macro,
        Token_Unspecified,

        Token_EOF
    };

    static const char *NAMES[];

    void initialize(const QByteArray &input);
    Token next();

    const char *stream;
    const char *pos;
    const char *identifier;
};

const char *Tokenizer::NAMES[] = {
    "Void",
    "OpenBrace",
    "CloseBrace",
    "SemiColon",
    "Identifier",
    "Macro",
    "Unspecified",
    "EOF"
};

void Tokenizer::initialize(const QByteArray &input)
{
    stream = input.constData();
    pos = input;
    identifier = input;
}

Tokenizer::Token Tokenizer::next()
{
    while (*pos != 0) {
        char c = *pos++;
        switch (c) {
        case '/':

            if (*pos == '/') {
                // '//' comment
                ++pos;
                while (*pos != 0 && *pos != '\n') ++pos;
                if (*pos != 0) ++pos; // skip the newline

            } else if (*pos == '*') {
                // /* */ comment
                ++pos;
                while (*pos != 0 && *pos != '*' && pos[1] != '/') ++pos;
                if (*pos != 0) pos += 2;
            }
            break;

        case '#': {
            while (*pos != 0) {
                if (*pos == '\n') {
                    ++pos;
                    break;
                } else if (*pos == '\\') {
                    ++pos;
                    while (*pos != 0 && (*pos == ' ' || *pos == '\t'))
                        ++pos;
                    if (*pos != 0 && (*pos == '\n' || (*pos == '\r' && pos[1] == '\n')))
                        pos+=2;
                } else {
                    ++pos;
                }
            }
            break;
        }

        case 'v': {
            if (*pos == 'o' && pos[1] == 'i' && pos[2] == 'd') {
                pos += 3;
                return Token_Void;
            }
            Q_FALLTHROUGH();
        }

        case ';': return Token_SemiColon;
        case 0: return Token_EOF;
        case '{': return Token_OpenBrace;
        case '}': return Token_CloseBrace;

        case ' ':
        case '\n':
        case '\r': break;
        default:
            // Identifier...
            if ((c >= 'a' && c <= 'z' ) || (c >= 'A' && c <= 'Z' ) || c == '_') {
                identifier = pos - 1;
                while (*pos != 0 && ((*pos >= 'a' && *pos <= 'z')
                                     || (*pos >= 'A' && *pos <= 'Z')
                                     || *pos == '_'
                                     || (*pos >= '0' && *pos <= '9'))) {
                    ++pos;
                }
                return Token_Identifier;
            } else {
                return Token_Unspecified;
            }
        }
    }

    return Token_EOF;
}

QByteArray addZAdjustment(const QByteArray &input)
{
    Tokenizer tok;
    tok.initialize(input);

    Tokenizer::Token lt = tok.next();
    Tokenizer::Token t = tok.next();

    // First find "void main() { ... "
    const char* voidPos = input.constData();
    while (t != Tokenizer::Token_EOF) {
        if (lt == Tokenizer::Token_Void && t == Tokenizer::Token_Identifier) {
            if (qstrncmp("main", tok.identifier, 4) == 0)
                break;
        }
        voidPos = tok.pos - 4;
        lt = t;
        t = tok.next();
    }

    QByteArray result;
    result.reserve(1024);
    result += QByteArray::fromRawData(input.constData(), voidPos - input.constData());

    result += QByteArrayLiteral("layout(location = 7) in float _qt_order;\n");
    result += QByteArrayLiteral("layout(push_constant) uniform _Qt { float zRange; } _qt;\n");

    // Find first brace '{'
    while (t != Tokenizer::Token_EOF && t != Tokenizer::Token_OpenBrace) t = tok.next();
    int braceDepth = 1;
    t = tok.next();

    // Find matching brace and insert our code there...
    while (t != Tokenizer::Token_EOF) {
        switch (t) {
        case Tokenizer::Token_CloseBrace:
            braceDepth--;
            if (braceDepth == 0) {
                result += QByteArray::fromRawData(voidPos, tok.pos - 1 - voidPos);
                result += QByteArrayLiteral("    gl_Position.z = (gl_Position.z * _qt.zRange + _qt_order) * gl_Position.w;\n");
                result += QByteArray(tok.pos - 1);
                return result;
            }
            break;
        case Tokenizer::Token_OpenBrace:
            ++braceDepth;
            break;
        default:
            break;
        }
        t = tok.next();
    }
    return QByteArray();
}

} // namespace

QT_END_NAMESPACE
