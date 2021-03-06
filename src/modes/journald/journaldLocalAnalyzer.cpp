/***************************************************************************
 *   KSystemLog, a system log viewer tool                                  *
 *   Copyright (C) 2007 by Nicolas Ternisien                               *
 *   nicolas.ternisien@gmail.com                                           *
 *   Copyright (C) 2015 by Vyacheslav Matyushin                            *
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

#include "journaldLocalAnalyzer.h"
#include "journaldConfiguration.h"
#include "ksystemlogConfig.h"
#include "logging.h"
#include "logViewModel.h"

#include <KLocalizedString>

#include <QtConcurrent>

JournaldLocalAnalyzer::JournaldLocalAnalyzer(LogMode *logMode, QString filter)
    : JournaldAnalyzer(logMode)
{
    m_cursor = nullptr;
    m_forgetWatchers = true;

    // Initialize journal access flags and open the journal.
    m_journalFlags = 0;
    JournaldConfiguration *configuration = logMode->logModeConfiguration<JournaldConfiguration *>();
    switch (configuration->entriesType()) {
    case JournaldConfiguration::EntriesAll:
        break;
    case JournaldConfiguration::EntriesCurrentUser:
        m_journalFlags |= SD_JOURNAL_CURRENT_USER;
        break;
    case JournaldConfiguration::EntriesSystem:
        m_journalFlags |= SD_JOURNAL_SYSTEM;
        break;
    default:
        break;
    }
    sd_journal_open(&m_journal, m_journalFlags);

    qintptr fd = sd_journal_get_fd(m_journal);
    m_journalNotifier = new QSocketNotifier(fd, QSocketNotifier::Read);
    m_journalNotifier->setEnabled(false);
    connect(m_journalNotifier, &QSocketNotifier::activated, this,
            &JournaldLocalAnalyzer::journalDescriptorUpdated);

    if (configuration->displayCurrentBootOnly()) {
        QFile file(QLatin1String("/proc/sys/kernel/random/boot_id"));
        if (file.open(QIODevice::ReadOnly)) {
            m_currentBootID = file.readAll().trimmed();
            m_currentBootID.remove(QChar('-'));
            m_filters << QString("_BOOT_ID=%1").arg(m_currentBootID);
        } else {
            logWarning() << "Journald analyzer failed to open /proc/sys/kernel/random/boot_id";
        }
    }

    if (!filter.isEmpty()) {
        m_filters << filter;
        m_filterName = filter.section('=', 1);
    }
}

JournaldLocalAnalyzer::~JournaldLocalAnalyzer()
{
    watchLogFiles(false);
    sd_journal_close(m_journal);
    delete m_journalNotifier;
}

void JournaldLocalAnalyzer::watchLogFiles(bool enabled)
{
    m_journalNotifier->setEnabled(enabled);

    m_workerMutex.lock();
    m_forgetWatchers = enabled;
    m_workerMutex.unlock();

    if (enabled) {
        JournalWatcher *watcher = new JournalWatcher();
        m_workerMutex.lock();
        m_journalWatchers.append(watcher);
        m_workerMutex.unlock();
        connect(watcher, &JournalWatcher::finished, this, &JournaldLocalAnalyzer::readJournalInitialFinished);
        watcher->setFuture(QtConcurrent::run(this, &JournaldLocalAnalyzer::readJournal, m_filters));
    } else {
        for (JournalWatcher *watcher : m_journalWatchers) {
            watcher->waitForFinished();
        }
        qDeleteAll(m_journalWatchers);
        m_journalWatchers.clear();

        if (m_cursor) {
            free(m_cursor);
            m_cursor = nullptr;
        }
    }
}

QStringList JournaldLocalAnalyzer::units() const
{
    return JournaldLocalAnalyzer::unitsStatic();
}

QStringList JournaldLocalAnalyzer::unitsStatic()
{
    return getUniqueFieldValues("_SYSTEMD_UNIT");
}

QStringList JournaldLocalAnalyzer::syslogIdentifiers() const
{
    return JournaldLocalAnalyzer::syslogIdentifiersStatic();
}

QStringList JournaldLocalAnalyzer::syslogIdentifiersStatic()
{
    return getUniqueFieldValues("SYSLOG_IDENTIFIER");
}

void JournaldLocalAnalyzer::readJournalInitialFinished()
{
    readJournalFinished(FullRead);
}

void JournaldLocalAnalyzer::readJournalUpdateFinished()
{
    readJournalFinished(UpdatingRead);
}

void JournaldLocalAnalyzer::readJournalFinished(ReadingMode readingMode)
{
    JournalWatcher *watcher = static_cast<JournalWatcher *>(sender());
    if (!watcher)
        return;

    QList<JournalEntry> entries = watcher->result();

    if (parsingPaused) {
        logDebug() << "Parsing is paused, discarding journald entries.";
    } else if (entries.size() == 0) {
        logDebug() << "Received no entries.";
    } else {
        insertionLocking.lock();
        logViewModel->startingMultipleInsertions();

        if (FullRead == readingMode) {
            emit statusBarChanged(i18n("Reading journald entries..."));
            // Start displaying the loading bar.
            emit readFileStarted(*logMode, LogFile(), 0, 1);
        }

        // Add journald entries to the model.
        int entriesInserted = updateModel(entries, readingMode);

        logViewModel->endingMultipleInsertions(readingMode, entriesInserted);

        if (FullRead == readingMode) {
            emit statusBarChanged(i18n("Journald entries loaded successfully."));

            // Stop displaying the loading bar.
            emit readEnded();
        }

        // Inform LogManager that new lines have been added.
        emit logUpdated(entriesInserted);

        insertionLocking.unlock();
    }

    m_workerMutex.lock();
    if (m_forgetWatchers) {
        m_journalWatchers.removeAll(watcher);
        watcher->deleteLater();
    }
    m_workerMutex.unlock();
}

void JournaldLocalAnalyzer::journalDescriptorUpdated(int fd)
{
    logDebug() << "Journal was updated.";
    QFile file;
    file.open(fd, QIODevice::ReadOnly);
    file.readAll();
    file.close();

    if (parsingPaused) {
        logDebug() << "Parsing is paused, will not fetch new journald entries.";
        return;
    }

    JournalWatcher *watcher = new JournalWatcher();
    m_workerMutex.lock();
    m_journalWatchers.append(watcher);
    m_workerMutex.unlock();
    connect(watcher, &JournalWatcher::finished, this, &JournaldLocalAnalyzer::readJournalUpdateFinished);
    watcher->setFuture(QtConcurrent::run(this, &JournaldLocalAnalyzer::readJournal, m_filters));
}

QList<JournaldLocalAnalyzer::JournalEntry> JournaldLocalAnalyzer::readJournal(const QStringList &filters)
{
    QMutexLocker mutexLocker(&m_workerMutex);
    QList<JournalEntry> entryList;
    sd_journal *journal;

    if (!m_filterName.isEmpty()) {
        emit statusChanged(m_filterName);
    }

    int res = sd_journal_open(&journal, m_journalFlags);
    if (res < 0) {
        logWarning() << "Failed to access the journal.";
        return QList<JournalEntry>();
    }

    if (prepareJournalReading(journal, filters)) {
        // Iterate over filtered entries.
        forever {
            JournalEntry entry;
            res = sd_journal_next(journal);
            if (res < 0) {
                logWarning() << "Failed to access next journal entry.";
                break;
            }
            if (res == 0) {
                // Reached last journal entry.
                break;
            }
            entry = readJournalEntry(journal);
            entryList.append(entry);
        }

        free(m_cursor);
        sd_journal_get_cursor(journal, &m_cursor);
    }

    sd_journal_close(journal);
    if (entryList.size() > 0)
        logDebug() << "Read" << entryList.size() << "journal entries.";
    return entryList;
}

bool JournaldLocalAnalyzer::prepareJournalReading(sd_journal *journal, const QStringList &filters)
{
    int res;

    // Set entries filter.
    for (const QString &filter : filters) {
        res = sd_journal_add_match(journal, filter.toUtf8(), 0);
        if (res < 0) {
            logWarning() << "Failed to set journal filter.";
            return false;
        }
    }

    // Go to the latest journal entry.
    res = sd_journal_seek_tail(journal);
    if (res < 0) {
        logWarning() << "Failed to seek journal tail.";
        return false;
    }

    // Read number of entries allowed by KSystemLog configuration.
    int maxEntriesNum = KSystemLogConfig::maxLines();

    // Seek to cursor.
    if (m_cursor) {
        int entriesNum = 0;
        // Continue searching for the oldest entry until
        // either cursor is found or maximum number of entries is traversed.
        while (entriesNum < maxEntriesNum) {
            entriesNum++;

            res = sd_journal_previous(journal);
            if (res < 0) {
                logWarning() << "Failed to seek previous journal entry.";
                return false;
            }

            res = sd_journal_test_cursor(journal, m_cursor);
            if (res > 0) {
                if (entriesNum == 1) {
                    // No new entries are found.
                    return false;
                }
                // Latest journal entry before journal update is found.
                break;
            }
        }
    } else {
        // Jump over maxEntriesNum entries backwards from the end of the journal.
        res = sd_journal_previous_skip(journal, maxEntriesNum + 1);
        if (res < 0) {
            // Seek failed. Read entries from the beginning.
            res = sd_journal_seek_head(journal);
            if (res < 0) {
                logWarning() << "Failed to seek journal head.";
                return false;
            }
        }
    }
    return true;
}

JournaldLocalAnalyzer::JournalEntry JournaldLocalAnalyzer::readJournalEntry(sd_journal *journal) const
{
    // Reads a single journal entry into JournalEntry structure.
    JournalEntry entry;
    const void *data;
    size_t length;
    uint64_t time;
    int res;

    res = sd_journal_get_realtime_usec(journal, &time);
    if (res == 0) {
        entry.date.setMSecsSinceEpoch(time / 1000);
    }

    res = sd_journal_get_data(journal, "SYSLOG_IDENTIFIER", &data, &length);
    if (res == 0) {
        entry.unit = QString::fromUtf8((const char *)data, length).section('=', 1);
    } else {
        res = sd_journal_get_data(journal, "_SYSTEMD_UNIT", &data, &length);
        if (res == 0) {
            entry.unit = QString::fromUtf8((const char *)data, length).section('=', 1);
        }
    }

    res = sd_journal_get_data(journal, "MESSAGE", &data, &length);
    if (res == 0) {
        entry.message = QString::fromUtf8((const char *)data, length).section('=', 1);
        entry.message.remove(QRegularExpression(ConsoleColorEscapeSequence));
    }

    res = sd_journal_get_data(journal, "PRIORITY", &data, &length);
    if (res == 0) {
        entry.priority = QString::fromUtf8((const char *)data, length).section('=', 1).toInt();
    }

    res = sd_journal_get_data(journal, "_BOOT_ID", &data, &length);
    if (res == 0) {
        entry.bootID = QString::fromUtf8((const char *)data, length).section('=', 1);
    }

    return entry;
}

QStringList JournaldLocalAnalyzer::getUniqueFieldValues(const QString id, int flags)
{
    QStringList units;
    sd_journal *journal;
    int res = sd_journal_open(&journal, flags);
    if (res == 0) {
        const void *data;
        size_t length;

        // Get all unique field values. The order is not defined.
        res = sd_journal_query_unique(journal, id.toUtf8());
        if (res == 0) {
            SD_JOURNAL_FOREACH_UNIQUE(journal, data, length)
            {
                units.append(QString::fromUtf8((const char *)data, length).section('=', 1));
            }
        }

        units.removeDuplicates();
        units.sort();
        sd_journal_close(journal);
    } else {
        logWarning() << "Failed to open the journal and extract unique values for field" << id;
    }
    return units;
}
