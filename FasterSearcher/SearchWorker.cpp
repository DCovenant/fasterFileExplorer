#include "SearchWorker.h"
#include <QDebug>
#include <QFile>

SearchWorker::SearchWorker(QObject *parent) : QObject(parent), process(new QProcess(this)) {
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, &SearchWorker::onProcessReadyRead);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &SearchWorker::onProcessFinished);
}

void SearchWorker::startSearch(const QString& command) {
    QStringList parts = command.split('|', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;
    QString program = parts.takeFirst();
    if (!QFile::exists(program)) {
        qDebug() << "fd.exe not found at:" << program;
        return;
    }
    process->start(program, parts);  // parts now contains proper arguments
}

void SearchWorker::stopSearch() {
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished(500);
    }
}

void SearchWorker::onProcessReadyRead() {
    QString output = process->readAllStandardOutput();
    QStringList results = output.split('\n', Qt::SkipEmptyParts);
    
    // Filter results here in worker thread to reduce main thread load
    QStringList filtered;
    for (const QString& line : results) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            filtered.append(trimmed);
        }
    }
    
    if (!filtered.isEmpty()) {
        emit resultsReady(filtered);
    }
}

void SearchWorker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    // Emit finished signal to notify that search is complete
    emit finished();
}