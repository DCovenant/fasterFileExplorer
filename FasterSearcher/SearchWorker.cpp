#include "SearchWorker.h"
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QThread>
#include <QRegularExpression>
#include <QProcess>
#include <QMutexLocker>

SearchWorker::SearchWorker(QObject *parent) 
    : QObject(parent), fdProcess(nullptr), stopRequested(0) {
}

bool SearchWorker::databaseExists() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString dbPath = QDir::cleanPath(appDir + "/file_index.db");
    return QFile::exists(dbPath);
}

bool SearchWorker::isFolderIndexed(const QString& folderPath, int* fileCount) {
    QString appDir = QCoreApplication::applicationDirPath();
    QString dbPath = QDir::cleanPath(appDir + "/file_index.db");
    
    if (!QFile::exists(dbPath)) return false;
    
    QString connectionName = QString("check_connection_%1").arg((quintptr)QThread::currentThreadId());
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
    
    if (!db.open()) {
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }
    
    bool hasFiles = false;
    int count = 0;
    
    QSqlQuery query(db);
    QString normalizedPath = QDir::toNativeSeparators(folderPath);
    if (!normalizedPath.endsWith(QDir::separator())) {
        normalizedPath += QDir::separator();
    }
    
    query.prepare("SELECT COUNT(*) FROM files WHERE path LIKE ?");
    query.bindValue(0, normalizedPath + "%");
    
    if (query.exec() && query.next()) {
        count = query.value(0).toInt();
        hasFiles = (count > 0);
    }
    
    if (fileCount) *fileCount = count;
    
    if (db.isOpen()) db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    
    return hasFiles;
}

void SearchWorker::startSearch(const QString& term, const QString& filter, bool useFilter, const QString& subfolderPattern) {
    searchTerm = term;
    folderFilter = filter;
    useFolderFilter = useFilter;
    subfolderFilterPattern = subfolderPattern;
    stopRequested.storeRelaxed(0);
    
    bool useDatabase = false;
    
    if (databaseExists()) {
        if (useFilter && !filter.isEmpty()) {
            int fileCount = 0;
            useDatabase = isFolderIndexed(filter, &fileCount);
        } else {
            useDatabase = true;
        }
    }
    
    if (useDatabase) {
        performDatabaseSearch();
    } else {
        performFdSearch();
    }
}

void SearchWorker::stopSearch() {
    stopRequested.storeRelaxed(1);
    
    QMutexLocker locker(&processMutex);
    if (fdProcess && fdProcess->state() == QProcess::Running) {
        fdProcess->kill();
        fdProcess->waitForFinished(100);
    }
}

void SearchWorker::performDatabaseSearch() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString dbPath = QDir::cleanPath(appDir + "/file_index.db");
    
    if (!QFile::exists(dbPath)) {
        emit finished();
        return;
    }
    
    QString connectionName = QString("search_connection_%1").arg((quintptr)QThread::currentThreadId());
    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
    
    if (!db.open()) {
        emit finished();
        return;
    }
    
    int resultCount = 0;
    
    QSqlQuery query(db);
    
    if (useFolderFilter && !folderFilter.isEmpty()) {
        QString normalizedFolder = QDir::toNativeSeparators(folderFilter);
        if (!normalizedFolder.endsWith(QDir::separator())) {
            normalizedFolder += QDir::separator();
        }
        
        query.prepare("SELECT path FROM files WHERE path LIKE ? AND path LIKE ? COLLATE NOCASE");
        query.bindValue(0, normalizedFolder + "%");
        query.bindValue(1, QString("%%%1%").arg(searchTerm));
    } else {
        query.prepare("SELECT path FROM files WHERE path LIKE ? COLLATE NOCASE");
        query.bindValue(0, QString("%%%1%").arg(searchTerm));
    }
    
    if (!query.exec()) {
        // Query failed
    }
    
    QStringList batch;
    const int BATCH_SIZE = 100;
    
    QRegularExpression subfolderRegex;
    bool useSubfolderRegex = !subfolderFilterPattern.isEmpty();
    if (useSubfolderRegex) {
        subfolderRegex = QRegularExpression(subfolderFilterPattern, QRegularExpression::CaseInsensitiveOption);
        if (!subfolderRegex.isValid()) {
            useSubfolderRegex = false;
        }
    }
    
    while (query.isActive() && query.next() && !stopRequested.loadRelaxed()) {
        QString path = query.value(0).toString();
        if (!path.isEmpty()) {
            if (useSubfolderRegex && useFolderFilter) {
                QString normalizedBase = QDir::toNativeSeparators(folderFilter);
                if (!normalizedBase.endsWith(QDir::separator())) {
                    normalizedBase += QDir::separator();
                }
                
                if (path.startsWith(normalizedBase, Qt::CaseInsensitive)) {
                    QString relativePath = path.mid(normalizedBase.length());
                    
                    QStringList pathParts = relativePath.split(QDir::separator(), Qt::SkipEmptyParts);
                    bool matches = false;
                    for (const QString& part : pathParts) {
                        if (subfolderRegex.match(part).hasMatch()) {
                            matches = true;
                            break;
                        }
                    }
                    
                    if (!matches) continue;
                }
            }
            
            batch.append(path);
            resultCount++;
            
            if (batch.size() >= BATCH_SIZE) {
                emit resultsReady(batch);
                batch.clear();
                QThread::msleep(10);
            }
        }
    }
    
    if (!batch.isEmpty()) {
        emit resultsReady(batch);
    }
    
    QString connName = connectionName;
    if (db.isOpen()) db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    
    emit finished();
}

void SearchWorker::performFdSearch() {
    {
        QMutexLocker locker(&processMutex);
        if (fdProcess) {
            if (fdProcess->state() == QProcess::Running) {
                fdProcess->kill();
                fdProcess->waitForFinished(1000);
            }
            delete fdProcess;
            fdProcess = nullptr;
        }
    }
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString fdPath = QDir::cleanPath(appDir + "/fd.exe");
    
    if (!QFile::exists(fdPath)) {
        emit finished();
        return;
    }
    
    QString searchPath = (useFolderFilter && !folderFilter.isEmpty()) ? folderFilter : "C:/";
    
    QStringList arguments;
    arguments << "--hidden" << "--no-ignore" << "--type" << "f" << searchTerm << searchPath;
    
    {
        QMutexLocker locker(&processMutex);
        fdProcess = new QProcess(this);
        fdProcess->setProgram(fdPath);
        fdProcess->setArguments(arguments);
        fdProcess->setProcessChannelMode(QProcess::MergedChannels);
    }
    
    fdProcess->start();
    
    if (!fdProcess->waitForStarted(5000)) {
        QMutexLocker locker(&processMutex);
        delete fdProcess;
        fdProcess = nullptr;
        emit finished();
        return;
    }
    
    int timeoutCounter = 0;
    const int MAX_TIMEOUT = 3000;
    int totalEmitted = 0;
    
    while (fdProcess->state() == QProcess::Running && !stopRequested.loadRelaxed() && timeoutCounter < MAX_TIMEOUT) {
        if (stopRequested.loadRelaxed()) break;
        
        if (fdProcess->waitForReadyRead(50)) {
            if (stopRequested.loadRelaxed()) break;
            
            QByteArray output = fdProcess->readAllStandardOutput();
            if (!output.isEmpty()) {
                QString text = QString::fromUtf8(output);
                QStringList lines = text.split('\n', Qt::SkipEmptyParts);
                
                QStringList batch;
                for (const QString& line : lines) {
                    if (stopRequested.loadRelaxed()) break;
                    
                    QString trimmed = line.trimmed();
                    if (!trimmed.isEmpty()) {
                        batch.append(trimmed);
                        
                        if (batch.size() >= 100) {
                            if (stopRequested.loadRelaxed()) break;
                            if (!stopRequested.loadRelaxed()) {
                                emit resultsReady(batch);
                                totalEmitted += batch.size();
                            }
                            batch.clear();
                        }
                    }
                }
                
                if (stopRequested.loadRelaxed()) break;
                
                if (!batch.isEmpty() && !stopRequested.loadRelaxed()) {
                    emit resultsReady(batch);
                    totalEmitted += batch.size();
                }
            }
            timeoutCounter = 0;
        } else {
            timeoutCounter++;
        }
    }
    
    if (timeoutCounter >= MAX_TIMEOUT) {
        fdProcess->kill();
        fdProcess->waitForFinished(1000);
    }
    
    if (stopRequested.loadRelaxed()) {
        if (fdProcess->state() == QProcess::Running) {
            fdProcess->kill();
            fdProcess->waitForFinished(1000);
        }
        delete fdProcess;
        fdProcess = nullptr;
        emit finished();
        return;
    }
    
    if (fdProcess->state() == QProcess::NotRunning) {
        QByteArray output = fdProcess->readAllStandardOutput();
        if (!output.isEmpty()) {
            QString text = QString::fromUtf8(output);
            QStringList lines = text.split('\n', Qt::SkipEmptyParts);
            
            QStringList batch;
            for (const QString& line : lines) {
                QString trimmed = line.trimmed();
                if (!trimmed.isEmpty()) batch.append(trimmed);
            }
            
            if (!batch.isEmpty()) {
                emit resultsReady(batch);
                totalEmitted += batch.size();
            }
        }
    }
    
    {
        QMutexLocker locker(&processMutex);
        if (fdProcess) {
            delete fdProcess;
            fdProcess = nullptr;
        }
    }
    
    emit finished();
}