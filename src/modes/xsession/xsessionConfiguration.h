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

#ifndef _X_SESSION_CONFIGURATION_H_
#define _X_SESSION_CONFIGURATION_H_

#include <QStringList>

#include "logModeConfiguration.h"

#include "logging.h"
#include "defaults.h"

#include "xsessionLogMode.h"

#include "ksystemlogConfig.h"

class XSessionConfigurationPrivate
{
public:
    QString xsessionPath;

    bool ignoreXorgErrors;

    QStringList xorgErrorKeywords;

    QStringList warningKeywords;
    QStringList errorKeywords;
};

class XSessionConfiguration : public LogModeConfiguration
{
    Q_OBJECT

public:
    XSessionConfiguration()
        : d(new XSessionConfigurationPrivate())
    {
        configuration->setCurrentGroup(QStringLiteral("XSessionLogMode"));

        configuration->addItemString(QStringLiteral("LogFilePath"), d->xsessionPath,
                                     QStringLiteral("~/.xsession-errors"), QStringLiteral("LogFilePath"));

        configuration->addItemBool(QStringLiteral("IgnoreXorgErrors"), d->ignoreXorgErrors, false,
                                   QStringLiteral("IgnoreXorgErrors"));

        QStringList defaultXorgErrorKeywords;
        defaultXorgErrorKeywords.append(QStringLiteral("X Error"));
        defaultXorgErrorKeywords.append(QStringLiteral("  Major opcode"));
        defaultXorgErrorKeywords.append(QStringLiteral("  Minor opcode"));
        defaultXorgErrorKeywords.append(QStringLiteral("  Resource id"));
        configuration->addItemStringList(QStringLiteral("XorgErrorKeywords"), d->xorgErrorKeywords,
                                         defaultXorgErrorKeywords, QStringLiteral("XorgErrorKeywords"));

        QStringList defaultWarningKeywords;
        defaultWarningKeywords.append(QStringLiteral("WARNING"));
        configuration->addItemStringList(QStringLiteral("WarningKeywords"), d->warningKeywords,
                                         defaultWarningKeywords, QStringLiteral("WarningKeywords"));

        QStringList defaultErrorKeywords;
        defaultErrorKeywords.append(QStringLiteral("ERROR"));
        configuration->addItemStringList(QStringLiteral("ErrorKeywords"), d->errorKeywords,
                                         defaultErrorKeywords, QStringLiteral("ErrorKeywords"));
    }

    virtual ~XSessionConfiguration() { delete d; }

    QStringList xorgErrorKeywords() const { return d->xorgErrorKeywords; }

    bool isIgnoreXorgErrors() const { return d->ignoreXorgErrors; }

    void setIgnoreXorgErrors(bool ignore) { d->ignoreXorgErrors = ignore; }

    QString xsessionPath() const { return d->xsessionPath; }

    void setXSessionPath(const QString &xsessionPath) { d->xsessionPath = xsessionPath; }

    QStringList warningKeywords() const { return d->warningKeywords; }

    QStringList errorKeywords() const { return d->errorKeywords; }

private:
    XSessionConfigurationPrivate *const d;
};

#endif // _X_SESSION_CONFIGURATION_H_
