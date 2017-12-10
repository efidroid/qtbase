/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QEVENTDISPATCHER_UEFI_P_H
#define QEVENTDISPATCHER_UEFI_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "QtCore/qabstracteventdispatcher.h"
#include "QtCore/qlist.h"
#include "QtCore/quefieventnotifier.h"
#include "private/qabstracteventdispatcher_p.h"
#include "QtCore/qvarlengtharray.h"
#include "private/qtimerinfo_unix_p.h"

QT_BEGIN_NAMESPACE

class QEventDispatcherUEFIPrivate;

class Q_CORE_EXPORT QEventDispatcherUEFI : public QAbstractEventDispatcher
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QEventDispatcherUEFI)

public:
    explicit QEventDispatcherUEFI(QObject *parent = 0);
    ~QEventDispatcherUEFI();

    bool processEvents(QEventLoop::ProcessEventsFlags flags) Q_DECL_OVERRIDE;
    bool hasPendingEvents() Q_DECL_OVERRIDE;

    bool registerEventNotifier(QUefiEventNotifier *notifier);
    void unregisterEventNotifier(QUefiEventNotifier *notifier);

    void registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *object) Q_DECL_FINAL;
    bool unregisterTimer(int timerId) Q_DECL_FINAL;
    bool unregisterTimers(QObject *object) Q_DECL_FINAL;
    QList<TimerInfo> registeredTimers(QObject *object) const Q_DECL_FINAL;

    int remainingTime(int timerId) Q_DECL_FINAL;

    void wakeUp() Q_DECL_FINAL;
    void interrupt() Q_DECL_FINAL;
    void flush() Q_DECL_OVERRIDE;

protected:
    QEventDispatcherUEFI(QEventDispatcherUEFIPrivate &dd, QObject *parent = 0);
};

class Q_CORE_EXPORT QEventDispatcherUEFIPrivate : public QAbstractEventDispatcherPrivate
{
    Q_DECLARE_PUBLIC(QEventDispatcherUEFI)

public:
    QEventDispatcherUEFIPrivate();
    ~QEventDispatcherUEFIPrivate();

    int activateTimers();

    QVector<Q_EFI_EVENT> waitevents;
    Q_EFI_EVENT uefiTimeoutEvent;
    Q_EFI_EVENT uefiWakeupEvent;

    QList<QUefiEventNotifier *> uefiEventNotifierList;

    QTimerInfoList timerList;
    QAtomicInt interrupt; // bool
};

QT_END_NAMESPACE

#endif // QEVENTDISPATCHER_UEFI_P_H
