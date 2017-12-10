TARGET = quefigop

DEFINES += QT_NO_FOREACH

QT += \
    core-private gui-private \
    eventdispatcher_support-private \
    fontdatabase_support-private fb_support-private

qtHaveModule(input_support-private): \
    QT += input_support-private

SOURCES = main.cpp \
          quefigopintegration.cpp \
          quefigopscreen.cpp

HEADERS = quefigopintegration.h \
          quefigopscreen.h

OTHER_FILES += uefigop.json

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QUefiGopIntegrationPlugin
!equals(TARGET, $$QT_DEFAULT_QPA_PLUGIN): PLUGIN_EXTENDS = -
load(qt_plugin)
