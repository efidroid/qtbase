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

#include "quefigopintegration.h"
#include "quefigopscreen.h"

#include <QtFontDatabaseSupport/private/qfreetypefontdatabase_p.h>
#include <QtEventDispatcherSupport/private/quefieventdispatcher_p.h>

#include <QtFbSupport/private/qfbvthandler_p.h>
#include <QtFbSupport/private/qfbbackingstore_p.h>
#include <QtFbSupport/private/qfbwindow_p.h>
#include <QtFbSupport/private/qfbcursor_p.h>

#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatforminputcontextfactory_p.h>

#include <QtInputSupport/private/quefiinput_p.h>

QT_BEGIN_NAMESPACE

static EFI_GUID mGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

QUefiGopIntegration::QUefiGopIntegration(const QStringList &paramList)
    : m_primaryScreen(nullptr),
      m_fontDb(new QFreeTypeFontDatabase)
{
    EFI_STATUS status;

    status = gST->BootServices->LocateProtocol (&mGraphicsOutputProtocolGuid, NULL, (void **)&m_gop);
    if (status != EFI_SUCCESS) {
        qDebug() << "Can't find mGraphicsOutputProtocolGuid: " << status;
        Q_ASSERT(0);
    }

    if (!m_primaryScreen)
        m_primaryScreen = new QUefiGopScreen(paramList, m_gop);
}

QUefiGopIntegration::~QUefiGopIntegration()
{
    destroyScreen(m_primaryScreen);
}

void QUefiGopIntegration::initialize()
{
    if (m_primaryScreen->initialize())
        screenAdded(m_primaryScreen);
    else
        qWarning("uefigop: Failed to initialize screen");

    m_inputContext = QPlatformInputContextFactory::create();

    m_nativeInterface.reset(new QPlatformNativeInterface);

    m_vtHandler.reset(new QFbVtHandler);

    if (!qEnvironmentVariableIntValue("QT_QPA_FB_DISABLE_INPUT"))
        createInputHandlers();
}

bool QUefiGopIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case WindowManagement: return false;
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformBackingStore *QUefiGopIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QFbBackingStore(window);
}

QPlatformWindow *QUefiGopIntegration::createPlatformWindow(QWindow *window) const
{
    return new QFbWindow(window);
}

QAbstractEventDispatcher *QUefiGopIntegration::createEventDispatcher() const
{
    return new QUefiEventDispatcher();
}

QList<QPlatformScreen *> QUefiGopIntegration::screens() const
{
    QList<QPlatformScreen *> list;
    list.append(m_primaryScreen);
    return list;
}

QPlatformFontDatabase *QUefiGopIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

void QUefiGopIntegration::createInputHandlers()
{
    new QUefiInputHandler(m_gop, QLatin1String("UefiInput"), QString());
}

QPlatformNativeInterface *QUefiGopIntegration::nativeInterface() const
{
    return m_nativeInterface.data();
}

QT_END_NAMESPACE
