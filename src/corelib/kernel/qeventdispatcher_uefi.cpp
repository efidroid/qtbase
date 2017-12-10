/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
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

extern "C" {
#include <Base.h>
#include <PiDxe.h>
#include <Library/UefiBootServicesTableLib.h>
}

#include "qplatformdefs.h"

#include "qcoreapplication.h"
#include "qpair.h"
#include "quefieventnotifier.h"
#include "qthread.h"
#include "qelapsedtimer.h"
#include <qdebug.h>

#include "qeventdispatcher_uefi_p.h"
#include <private/qthread_p.h>
#include <private/qcoreapplication_p.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

QT_BEGIN_NAMESPACE

QEventDispatcherUEFIPrivate::QEventDispatcherUEFIPrivate()
{
    EFI_STATUS status;

    status = gST->BootServices->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &uefiTimeoutEvent);
    Q_ASSERT(status == EFI_SUCCESS);

    status = gST->BootServices->CreateEvent(0, TPL_CALLBACK, NULL, NULL, &uefiWakeupEvent);
    Q_ASSERT(status == EFI_SUCCESS);
}

QEventDispatcherUEFIPrivate::~QEventDispatcherUEFIPrivate()
{
    // cleanup timers
    qDeleteAll(timerList);
}

int QEventDispatcherUEFIPrivate::activateTimers()
{
    return timerList.activateTimers();
}

QEventDispatcherUEFI::QEventDispatcherUEFI(QObject *parent)
    : QAbstractEventDispatcher(*new QEventDispatcherUEFIPrivate, parent)
{ }

QEventDispatcherUEFI::QEventDispatcherUEFI(QEventDispatcherUEFIPrivate &dd, QObject *parent)
    : QAbstractEventDispatcher(dd, parent)
{ }

QEventDispatcherUEFI::~QEventDispatcherUEFI()
{ }

/*!
    \internal
*/
void QEventDispatcherUEFI::registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *obj)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1 || interval < 0 || !obj) {
        qWarning("QEventDispatcherUEFI::registerTimer: invalid arguments");
        return;
    } else if (obj->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("QEventDispatcherUEFI::registerTimer: timers cannot be started from another thread");
        return;
    }
#endif

    Q_D(QEventDispatcherUEFI);
    d->timerList.registerTimer(timerId, interval, timerType, obj);
}

/*!
    \internal
*/
bool QEventDispatcherUEFI::unregisterTimer(int timerId)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1) {
        qWarning("QEventDispatcherUEFI::unregisterTimer: invalid argument");
        return false;
    } else if (thread() != QThread::currentThread()) {
        qWarning("QEventDispatcherUEFI::unregisterTimer: timers cannot be stopped from another thread");
        return false;
    }
#endif

    Q_D(QEventDispatcherUEFI);
    return d->timerList.unregisterTimer(timerId);
}

/*!
    \internal
*/
bool QEventDispatcherUEFI::unregisterTimers(QObject *object)
{
#ifndef QT_NO_DEBUG
    if (!object) {
        qWarning("QEventDispatcherUEFI::unregisterTimers: invalid argument");
        return false;
    } else if (object->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("QEventDispatcherUEFI::unregisterTimers: timers cannot be stopped from another thread");
        return false;
    }
#endif

    Q_D(QEventDispatcherUEFI);
    return d->timerList.unregisterTimers(object);
}

QList<QEventDispatcherUEFI::TimerInfo>
QEventDispatcherUEFI::registeredTimers(QObject *object) const
{
    if (!object) {
        qWarning("QEventDispatcherUEFI:registeredTimers: invalid argument");
        return QList<TimerInfo>();
    }

    Q_D(const QEventDispatcherUEFI);
    return d->timerList.registeredTimers(object);
}

/*****************************************************************************
 QEventDispatcher implementations for UEFI
 *****************************************************************************/

bool QEventDispatcherUEFI::registerEventNotifier(QUefiEventNotifier *notifier)
{
    Q_ASSERT(notifier);
#ifndef QT_NO_DEBUG
    if (notifier->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("QSocketNotifier: socket notifiers cannot be enabled from another thread");
        return false;
    }
#endif

    Q_D(QEventDispatcherUEFI);

    if (d->uefiEventNotifierList.contains(notifier))
        return true;

    d->uefiEventNotifierList.append(notifier);
    return true;
}

void QEventDispatcherUEFI::unregisterEventNotifier(QUefiEventNotifier *notifier)
{
    Q_ASSERT(notifier);
#ifndef QT_NO_DEBUG
    Q_EFI_EVENT event = notifier->event();
    if (notifier->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("QUefiEventNotifier: event notifier (event %p) cannot be disabled from another thread.\n"
                "(Notifier's thread is %s(%p), event dispatcher's thread is %s(%p), current thread is %s(%p))",
                event,
                notifier->thread() ? notifier->thread()->metaObject()->className() : "QThread", notifier->thread(),
                thread() ? thread()->metaObject()->className() : "QThread", thread(),
                QThread::currentThread() ? QThread::currentThread()->metaObject()->className() : "QThread", QThread::currentThread());
        return;
    }
#endif

    Q_D(QEventDispatcherUEFI);

    int i = d->uefiEventNotifierList.indexOf(notifier);
    if (i != -1)
        d->uefiEventNotifierList.takeAt(i);
}

bool QEventDispatcherUEFI::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    EFI_STATUS status;

    Q_D(QEventDispatcherUEFI);
    d->interrupt.store(0);

    // we are awake, broadcast it
    emit awake();
    QCoreApplicationPrivate::sendPostedEvents(0, 0, d->threadData);

    const bool include_timers = (flags & QEventLoop::X11ExcludeTimers) == 0;
    const bool include_notifiers = (flags & QEventLoop::ExcludeSocketNotifiers) == 0;
    const bool wait_for_events = flags & QEventLoop::WaitForMoreEvents;

    const bool canWait = (d->threadData->canWaitLocked()
                          && !d->interrupt.load()
                          && wait_for_events);

    if (canWait)
        emit aboutToBlock();

    if (d->interrupt.load())
        return false;

    d->waitevents.clear();
    d->waitevents.reserve(1 + (include_notifiers ? d->uefiEventNotifierList.count() : 0));

    timespec wait_tm = { 0, 0 };

    size_t notifiers_start = 0;
    size_t notifiers_end = 0;
    if (include_notifiers) {
        notifiers_start = d->waitevents.count();
        for (int i=0; i<d->uefiEventNotifierList.count(); i++) {
            d->waitevents.append(d->uefiEventNotifierList.at(i)->event());
        }
    }
    notifiers_end = d->waitevents.count();

    // This must come after the notifiers so we can receive user input during animations
    if (!canWait || (include_timers && d->timerList.timerWait(wait_tm))) {
        UINT64 trigger_time = 0;

        trigger_time += wait_tm.tv_sec * 10000000;
        trigger_time += wait_tm.tv_nsec / 100;

        status = gST->BootServices->SetTimer(d->uefiTimeoutEvent, TimerRelative, trigger_time);
        Q_ASSERT(status == EFI_SUCCESS);

        d->waitevents.append(d->uefiTimeoutEvent);
    }

    // This must be last, as it's popped off the end below
    d->waitevents.append(d->uefiWakeupEvent);

    int nevents = 0;
    UINTN event_index;

    status = gST->BootServices->WaitForEvent(d->waitevents.size(), d->waitevents.data(), &event_index);
    if (EFI_ERROR(status)) {
        qDebug("WaitForEvent: %08x", status);
        return false;
    }

    if (event_index == (UINTN)d->waitevents.size()-1) {
        nevents++;
    }
    d->waitevents.takeLast();

    if (include_notifiers) {
        if (event_index>=notifiers_start && event_index < notifiers_end) {
            QEvent event(QEvent::UefiEventAct);
            QCoreApplication::sendEvent(d->uefiEventNotifierList.at(event_index-notifiers_start), &event);
            nevents++;
        }
    }

    if (include_timers) {
        nevents += (d->activateTimers() > 0);
    }

    // return true if we handled events, false otherwise
    return (nevents > 0);
}

bool QEventDispatcherUEFI::hasPendingEvents()
{
    extern uint qGlobalPostedEventsCount(); // from qapplication.cpp
    return qGlobalPostedEventsCount();
}

int QEventDispatcherUEFI::remainingTime(int timerId)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1) {
        qWarning("QEventDispatcherUEFI::remainingTime: invalid argument");
        return -1;
    }
#endif

    Q_D(QEventDispatcherUEFI);
    return d->timerList.timerRemainingTime(timerId);
}

void QEventDispatcherUEFI::wakeUp()
{
    Q_D(QEventDispatcherUEFI);
    EFI_STATUS status = gST->BootServices->SignalEvent(d->uefiWakeupEvent);
    Q_ASSERT(status == EFI_SUCCESS);
}

void QEventDispatcherUEFI::interrupt()
{
    Q_D(QEventDispatcherUEFI);
    d->interrupt.store(1);
    wakeUp();
}

void QEventDispatcherUEFI::flush()
{ }

QT_END_NAMESPACE

#include "moc_qeventdispatcher_uefi_p.cpp"
