/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

extern "C" {
#include <Base.h>
#include <PiDxe.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
}

#include "quefigopscreen.h"
#include <QtFbSupport/private/qfbcursor_p.h>
#include <QtFbSupport/private/qfbwindow_p.h>
#include <QtGui/QPainter>

#include <qimage.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

QUefiGopScreen::QUefiGopScreen(const QStringList &args, void *gop)
    : mArgs(args), mGop(gop), mBlitter(0)
{
}

QUefiGopScreen::~QUefiGopScreen()
{
}

bool QUefiGopScreen::initialize()
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL*)mGop;

    mDepth = 32;
    mGeometry = QRect(
        0, 0,
        gop->Mode->Info->HorizontalResolution,
        gop->Mode->Info->VerticalResolution
    );
    mFormat = QImage::Format_RGBX8888;

    const int dpi = 100;
    mPhysicalSize = QSizeF(
        qRound(mGeometry.width() * 25.4 / dpi),
        qRound(mGeometry.height() * 25.4 / dpi)
    );

    QFbScreen::initializeCompositor();
    mFbScreenImage = QImage(mGeometry.width(), mGeometry.height(), mFormat);

    mCursor = new QFbCursor(this);

    return true;
}

QRegion QUefiGopScreen::doRedraw()
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL*)mGop;
    QRegion touched = QFbScreen::doRedraw();
    EFI_STATUS status;

    if (touched.isEmpty())
        return touched;
    if (!mBlitter)
        mBlitter = new QPainter(&mFbScreenImage);

    mBlitter->setCompositionMode(QPainter::CompositionMode_Source);

    for (const QRect &rect : touched) {
        status = gop->Blt(
            gop,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)mScreenImage/*.rgbSwapped()*/.constBits(),
            EfiBltBufferToVideo,
            rect.x(), rect.y(),
            rect.x(), rect.y(),
            rect.width(), rect.height(),
            mScreenImage.width() * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
        );
        if (EFI_ERROR(status)) {
            qWarning("Can't blit image to screen: %u", status);
        }
    }

    return touched;
}

// grabWindow() grabs "from the screen" not from the backingstores.
// In linuxfb's case it will also include the mouse cursor.
QPixmap QUefiGopScreen::grabWindow(WId wid, int x, int y, int width, int height) const
{
    if (!wid) {
        if (width < 0)
            width = mFbScreenImage.width() - x;
        if (height < 0)
            height = mFbScreenImage.height() - y;
        return QPixmap::fromImage(mFbScreenImage).copy(x, y, width, height);
    }

    QFbWindow *window = windowForId(wid);
    if (window) {
        const QRect geom = window->geometry();
        if (width < 0)
            width = geom.width() - x;
        if (height < 0)
            height = geom.height() - y;
        QRect rect(geom.topLeft() + QPoint(x, y), QSize(width, height));
        rect &= window->geometry();
        return QPixmap::fromImage(mFbScreenImage).copy(rect);
    }

    return QPixmap();
}

QT_END_NAMESPACE

