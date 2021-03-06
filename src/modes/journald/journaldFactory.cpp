/***************************************************************************
 *   KSystemLog, a system log viewer tool                                  *
 *   Copyright (C) 2007 by Nicolas Ternisien                               *
 *   nicolas.ternisien@gmail.com                                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#include "journaldFactory.h"
#include "journaldLocalAnalyzer.h"
#include "journaldLogMode.h"
#include "journaldConfiguration.h"
#include "logMode.h"
#include "logging.h"
#include "multipleActions.h"

#include <KLocalizedString>

QList<LogMode *> JournaldModeFactory::createLogModes() const
{
    QList<LogMode *> logModes;
    logModes.append(new JournaldLogMode());
    return logModes;
}

LogModeAction *JournaldModeFactory::createLogModeAction() const
{
    JournaldLogMode *logMode = dynamic_cast<JournaldLogMode *>(
        Globals::instance().findLogMode(QLatin1String(JOURNALD_LOG_MODE_ID)));

    MultipleActions *multipleActions
        = new MultipleActions(QIcon::fromTheme(QLatin1String(JOURNALD_MODE_ICON)), i18n("Journald"), logMode);

    QIcon filterIcon = QIcon::fromTheme(QLatin1String("view-filter"));
    QIcon remoteIcon = QIcon::fromTheme(QLatin1String("preferences-system-network-sharing"));

    // Do not add journald submenu actions into action collection.
    // There are too many of them and submenu contents sometimes gets changed.
    ActionData actionData;
    actionData.id = logMode->id();
    actionData.addToActionCollection = false;

    JournaldAnalyzerOptions analyzerOptions;
    analyzerOptions.analyzerType = JournaldAnalyzerType::Local;

    actionData.analyzerOptions = QVariant::fromValue(analyzerOptions);

    KActionMenu *actionMenu = new KActionMenu(QIcon::fromTheme(QLatin1String("drive-harddisk")),
                                              i18n("Local journal"), multipleActions);

    // Add "All messages" action.
    QAction *action = new QAction(filterIcon, i18n("All messages"), actionMenu);
    action->setData(QVariant::fromValue(actionData));
    actionMenu->addAction(action);
    multipleActions->addInnerAction(action, false, true);

    // Add separator.
    action = new QAction(actionMenu);
    action->setSeparator(true);
    actionMenu->addAction(action);

    // Add filtering by systemd unit.
    KActionMenu *filterActionMenu = new KActionMenu(filterIcon, i18n("Filter by systemd unit"), actionMenu);
    QStringList units = JournaldLocalAnalyzer::unitsStatic();
    for (const QString &unit : units) {
        action = new QAction(unit, filterActionMenu);

        analyzerOptions.filter = QString("_SYSTEMD_UNIT=%1").arg(unit);
        actionData.analyzerOptions = QVariant::fromValue(analyzerOptions);
        action->setData(QVariant::fromValue(actionData));

        filterActionMenu->addAction(action);
        multipleActions->addInnerAction(action, false, true);
    }
    actionMenu->addAction(filterActionMenu);

    // Add filtering by syslog identifier.
    filterActionMenu = new KActionMenu(filterIcon, i18n("Filter by syslog identifier"), actionMenu);
    QStringList syslogIDs = JournaldLocalAnalyzer::syslogIdentifiersStatic();
    for (const QString &id : syslogIDs) {
        action = new QAction(id, filterActionMenu);

        analyzerOptions.filter = QString("SYSLOG_IDENTIFIER=%1").arg(id);
        actionData.analyzerOptions = QVariant::fromValue(analyzerOptions);
        action->setData(QVariant::fromValue(actionData));

        filterActionMenu->addAction(action);
        multipleActions->addInnerAction(action, false, true);
    }
    actionMenu->addAction(filterActionMenu);

    multipleActions->addInnerAction(actionMenu, true, false);

    analyzerOptions.analyzerType = JournaldAnalyzerType::Network;
    analyzerOptions.filter.clear();

    action = new QAction(multipleActions);
    action->setSeparator(true);
    multipleActions->addInnerAction(action, true, false);

    // Create remote journal submenus.
    JournaldConfiguration *configuration = logMode->logModeConfiguration<JournaldConfiguration *>();
    auto remoteJournals = configuration->remoteJournals();
    for (const auto &addressInfo : remoteJournals) {
        QString menuText = QString("%1:%2").arg(addressInfo.address).arg(addressInfo.port);
        actionMenu = new KActionMenu(remoteIcon, menuText, multipleActions);

        action = new QAction(QIcon::fromTheme(QLatin1String("network-connect")), i18n("Connect"), actionMenu);
        analyzerOptions.address = addressInfo;
        actionData.analyzerOptions = QVariant::fromValue(analyzerOptions);
        action->setData(QVariant::fromValue(actionData));
        actionMenu->addAction(action);
        multipleActions->addInnerAction(action, false, true);

        // Add separator.
        action = new QAction(actionMenu);
        action->setSeparator(true);
        actionMenu->addAction(action);

        // Add filtering by systemd unit.
        JournalFilters filters = logMode->filters(addressInfo);
        if (!filters.systemdUnits.isEmpty()) {
            KActionMenu *filterActionMenu
                = new KActionMenu(filterIcon, i18n("Filter by systemd unit"), actionMenu);

            for (const QString &unit : filters.systemdUnits) {
                action = new QAction(unit, filterActionMenu);

                analyzerOptions.filter = QString("_SYSTEMD_UNIT=%1").arg(unit);
                actionData.analyzerOptions = QVariant::fromValue(analyzerOptions);
                action->setData(QVariant::fromValue(actionData));

                filterActionMenu->addAction(action);
                multipleActions->addInnerAction(action, false, true);
            }
            actionMenu->addAction(filterActionMenu);
        }

        // Add filtering by syslog identifier.
        if (!filters.syslogIdentifiers.isEmpty()) {
            KActionMenu *filterActionMenu
                = new KActionMenu(filterIcon, i18n("Filter by syslog identifier"), actionMenu);

            for (const QString &id : filters.syslogIdentifiers) {
                action = new QAction(id, filterActionMenu);

                analyzerOptions.filter = QString("SYSLOG_IDENTIFIER=%1").arg(id);
                actionData.analyzerOptions = QVariant::fromValue(analyzerOptions);
                action->setData(QVariant::fromValue(actionData));

                filterActionMenu->addAction(action);
                multipleActions->addInnerAction(action, false, true);
            }
            actionMenu->addAction(filterActionMenu);
        }

        multipleActions->addInnerAction(actionMenu, true, false);
    }

    // Add default log action with icon.
    // Don't put in into the menu, but allow it to be added into action collection and placed on the toolbar.
    multipleActions->addInnerAction(logMode->action(), false, true);

    return multipleActions;
}
