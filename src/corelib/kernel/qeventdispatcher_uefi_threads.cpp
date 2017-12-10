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

#include "qplatformdefs.h"

#include "qcoreapplication.h"
#include "qpair.h"
#include "quefieventnotifier.h"
#include "qthread.h"
#include "qelapsedtimer.h"
#include <qdebug.h>

#include "qeventdispatcher_uefi_threads_p.h"
#include <private/qthread_p.h>
#include <private/qcore_unix_p.h>
#include <private/qcoreapplication_p.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

QT_BEGIN_NAMESPACE

QEventDispatcherUEFIThreadsPrivate::QEventDispatcherUEFIThreadsPrivate()
{
    pthread_cond_init(&condition, NULL);
    pthread_mutex_init(&mutex, NULL);
}

QEventDispatcherUEFIThreadsPrivate::~QEventDispatcherUEFIThreadsPrivate()
{
}

int QEventDispatcherUEFIThreadsPrivate::activateTimers()
{
    return timerList.activateTimers();
}

QEventDispatcherUEFIThreads::QEventDispatcherUEFIThreads(QObject *parent)
    : QAbstractEventDispatcher(*new QEventDispatcherUEFIThreadsPrivate, parent)
{ }

QEventDispatcherUEFIThreads::QEventDispatcherUEFIThreads(QEventDispatcherUEFIThreadsPrivate &dd, QObject *parent)
    : QAbstractEventDispatcher(dd, parent)
{ }

QEventDispatcherUEFIThreads::~QEventDispatcherUEFIThreads()
{ }

/*!
    \internal
*/
void QEventDispatcherUEFIThreads::registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *obj)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1 || interval < 0 || !obj) {
        qWarning("QEventDispatcherUEFIThreads::registerTimer: invalid arguments");
        return;
    } else if (obj->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("QEventDispatcherUEFIThreads::registerTimer: timers cannot be started from another thread");
        return;
    }
#endif

    Q_D(QEventDispatcherUEFIThreads);
    d->timerList.registerTimer(timerId, interval, timerType, obj);
}

/*!
    \internal
*/
bool QEventDispatcherUEFIThreads::unregisterTimer(int timerId)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1) {
        qWarning("QEventDispatcherUEFIThreads::unregisterTimer: invalid argument");
        return false;
    } else if (thread() != QThread::currentThread()) {
        qWarning("QEventDispatcherUEFIThreads::unregisterTimer: timers cannot be stopped from another thread");
        return false;
    }
#endif

    Q_D(QEventDispatcherUEFIThreads);
    return d->timerList.unregisterTimer(timerId);
}

/*!
    \internal
*/
bool QEventDispatcherUEFIThreads::unregisterTimers(QObject *object)
{
#ifndef QT_NO_DEBUG
    if (!object) {
        qWarning("QEventDispatcherUEFIThreads::unregisterTimers: invalid argument");
        return false;
    } else if (object->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("QEventDispatcherUEFIThreads::unregisterTimers: timers cannot be stopped from another thread");
        return false;
    }
#endif

    Q_D(QEventDispatcherUEFIThreads);
    return d->timerList.unregisterTimers(object);
}

QList<QEventDispatcherUEFIThreads::TimerInfo>
QEventDispatcherUEFIThreads::registeredTimers(QObject *object) const
{
    if (!object) {
        qWarning("QEventDispatcherUEFIThreads:registeredTimers: invalid argument");
        return QList<TimerInfo>();
    }

    Q_D(const QEventDispatcherUEFIThreads);
    return d->timerList.registeredTimers(object);
}

/*****************************************************************************
 QEventDispatcher implementations for UEFI
 *****************************************************************************/

bool QEventDispatcherUEFIThreads::registerEventNotifier(QUefiEventNotifier *notifier)
{
    Q_ASSERT(notifier);
    qWarning("QEventDispatcherUEFIThreads: event notifiers are not supported");
    return false;
}

void QEventDispatcherUEFIThreads::unregisterEventNotifier(QUefiEventNotifier *notifier)
{
    Q_ASSERT(notifier);
    qWarning("QEventDispatcherUEFIThreads: event notifiers are not supported");
}

bool QEventDispatcherUEFIThreads::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    Q_D(QEventDispatcherUEFIThreads);
    d->interrupt.store(0);

    //qDebug("QEventDispatcherUEFIThreads::processEvents");

    // we are awake, broadcast it
    emit awake();
    QCoreApplicationPrivate::sendPostedEvents(0, 0, d->threadData);

    const bool include_timers = (flags & QEventLoop::X11ExcludeTimers) == 0;

    const bool canWait = (d->threadData->canWaitLocked()
                          && !d->interrupt.load());

    if (canWait)
        emit aboutToBlock();

    if (d->interrupt.load())
        return false;

    timespec *tm = nullptr;
    timespec wait_tm = { 0, 0 };

    if (!canWait || (include_timers && d->timerList.timerWait(wait_tm))) {
        timespec now = qt_gettime();
        wait_tm.tv_sec += now.tv_sec;
        wait_tm.tv_nsec += now.tv_nsec;
        normalizedTimespec(wait_tm);
        tm = &wait_tm;
    }

    int nevents = 0;

    int rc;
    pthread_mutex_lock(&d->mutex);
    if (tm)
        rc = pthread_cond_timedwait(&d->condition, &d->mutex, tm);
    else
        rc = pthread_cond_wait(&d->condition, &d->mutex);
    pthread_mutex_unlock(&d->mutex);

    if (include_timers) {
        nevents += (d->activateTimers() > 0);
    }

    // return true if we handled events, false otherwise
    return (nevents > 0);
}

bool QEventDispatcherUEFIThreads::hasPendingEvents()
{
    extern uint qGlobalPostedEventsCount(); // from qapplication.cpp
    return qGlobalPostedEventsCount();
}

int QEventDispatcherUEFIThreads::remainingTime(int timerId)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1) {
        qWarning("QEventDispatcherUEFIThreads::remainingTime: invalid argument");
        return -1;
    }
#endif

    Q_D(QEventDispatcherUEFIThreads);
    return d->timerList.timerRemainingTime(timerId);
}

void QEventDispatcherUEFIThreads::wakeUp()
{
    Q_D(QEventDispatcherUEFIThreads);

    pthread_mutex_lock(&d->mutex);
    pthread_cond_signal(&d->condition);
    pthread_mutex_unlock(&d->mutex);
}

void QEventDispatcherUEFIThreads::interrupt()
{
    Q_D(QEventDispatcherUEFIThreads);
    d->interrupt.store(1);
    wakeUp();
}

void QEventDispatcherUEFIThreads::flush()
{ }

QT_END_NAMESPACE

#include "moc_qeventdispatcher_uefi_threads_p.cpp"
