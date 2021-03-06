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

#ifndef _SIMPLE_ACTIONS_H_
#define _SIMPLE_ACTIONS_H_

#include <QString>
#include <QList>
#include <QIcon>

#include "globals.h"

#include "logModeAction.h"

class QAction;

class SimpleAction : public LogModeAction
{
    Q_OBJECT

public:
    SimpleAction(const QIcon &icon, const QString &text, QObject *parent);

    SimpleAction(QAction *action, QObject *parent);

    virtual ~SimpleAction();

    QList<QAction *> innerActions() Q_DECL_OVERRIDE;

    QAction *actionMenu() Q_DECL_OVERRIDE;

private:
    QAction *action;
};

#endif // _SIMPLE_ACTIONS_H_
