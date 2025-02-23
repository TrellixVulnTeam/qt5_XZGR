/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSvg module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSVGFUNCTIONS_WCE_H
#define QSVGFUNCTIONS_WCE_H

#ifdef Q_OS_WINCE

// File I/O ---------------------------------------------------------

#define _O_RDONLY       0x0001
#define _O_RDWR         0x0002
#define _O_WRONLY       0x0004
#define _O_CREAT        0x0008
#define _O_TRUNC        0x0010
#define _O_APPEND       0x0020
#define _O_EXCL         0x0040

#define O_RDONLY        _O_RDONLY
#define O_RDWR          _O_RDWR
#define O_WRONLY        _O_WRONLY
#define O_CREAT         _O_CREAT
#define O_TRUNC         _O_TRUNC
#define O_APPEND        _O_APPEND
#define O_EXCL          _O_EXCL

//For zlib we need these helper functions, but they break the build when
//set globally, so just set them for zlib use
#ifdef ZLIB_H
#define open qt_wince_open
#define _wopen(a,b,c) qt_wince__wopen(a,b,c)
#define close qt_wince__close
#define lseek qt_wince__lseek
#define read qt_wince__read
#define write qt_wince__write
#endif

int qt_wince__wopen(const wchar_t *filename, int oflag, int pmode);
int qt_wince_open(const char *filename, int oflag, int pmode);
int qt_wince__close(int handle);
long qt_wince__lseek(int handle, long offset, int origin);
int qt_wince__read(int handle, void *buffer, unsigned int count);
int qt_wince__write(int handle, const void *buffer, unsigned int count);

#endif // Q_OS_WINCE
#endif // QSVGFUNCTIONS_WCE_H
