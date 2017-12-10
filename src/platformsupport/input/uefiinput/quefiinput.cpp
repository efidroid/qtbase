/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins module of the Qt Toolkit.
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
#include <Protocol/AbsolutePointer.h>
#include <Protocol/GraphicsOutput.h>
}

#include "quefiinput_p.h"

#include <QSocketNotifier>
#include <QStringList>
#include <QPoint>
#include <QLoggingCategory>

#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

static EFI_GUID mEfiAbsolutePointerProtocolGuid = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;

QUefiInputHandler::QUefiInputHandler(void *gop, const QString &key,
                                       const QString &specification,
                                       QObject *parent)
    : QObject(parent),
    m_signalMapper(0),
    m_gop(gop)
{
    Q_UNUSED(key);
    Q_UNUSED(specification);

    EFI_STATUS status;
    UINTN handle_count;
    EFI_HANDLE *handle_buffer;
    EFI_ABSOLUTE_POINTER_PROTOCOL *abs_ptr;

    m_signalMapper = new QSignalMapper();

    setObjectName(QLatin1String("UEFI input Handler"));

    handle_count = 0;
    handle_buffer = NULL;
    status = gST->BootServices->LocateHandleBuffer(
                ByProtocol, &mEfiAbsolutePointerProtocolGuid, NULL, &handle_count, &handle_buffer
    );
    if (!EFI_ERROR(status)) {
        for (UINTN i=0; i<handle_count; i++) {
            status = gST->BootServices->HandleProtocol(
                        handle_buffer[i], &mEfiAbsolutePointerProtocolGuid, (void**)&abs_ptr
            );
            if (EFI_ERROR(status)) {
                qDebug() << "Can't handle protocol";
                continue;
            }

            auto notify = new QUefiEventNotifier(abs_ptr->WaitForInput);
            m_notifiers.append(notify);
            connect(notify, SIGNAL(activated(Q_EFI_EVENT)), m_signalMapper, SLOT(map()));
            m_signalMapper->setMapping(notify, (int)abs_ptr);
        }

        gST->BootServices->FreePool (handle_buffer);
    }

    connect(
        m_signalMapper, SIGNAL(mapped(int)),
        this, SLOT(readAbsolutePointerData(int))
    );
}

QUefiInputHandler::~QUefiInputHandler()
{
    delete m_signalMapper;

    while (!m_notifiers.isEmpty())
        delete m_notifiers.takeFirst();
}

void QUefiInputHandler::readAbsolutePointerData(int context)
{
    EFI_STATUS status;
    EFI_ABSOLUTE_POINTER_STATE pointer_state;
    EFI_ABSOLUTE_POINTER_PROTOCOL *abs_ptr = (EFI_ABSOLUTE_POINTER_PROTOCOL*)context;
    EFI_ABSOLUTE_POINTER_MODE *mode = abs_ptr->Mode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL*)m_gop;

    status = abs_ptr->GetState(abs_ptr, &pointer_state);
    if (EFI_ERROR(status)) {
        return;
    }

    QPoint pos(
        pointer_state.CurrentX * gop->Mode->Info->HorizontalResolution / (mode->AbsoluteMaxX - mode->AbsoluteMinX),
        pointer_state.CurrentY * gop->Mode->Info->VerticalResolution   / (mode->AbsoluteMaxY - mode->AbsoluteMinY)
    );
    bool pressed = pointer_state.CurrentZ > 0;

    QWindowSystemInterface::handleMouseEvent(0, pos, pos, pressed ? Qt::LeftButton : Qt::NoButton);
}

QT_END_NAMESPACE
