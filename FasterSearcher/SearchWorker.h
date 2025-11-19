#ifndef SEARCHWORKER_H
#define SEARCHWORKER_H

#include <QObject>
#include <QSqlDatabase>
#include <QStringList>
#include <QThread>
#include <QAtomicInt>
#include <QMutex>

class SearchWorker : public QObject {
    Q_OBJECT
public:
    explicit SearchWorker(QObject *parent = nullptr);
    void startSearch(const QString& term, const QString& filter, bool useFilter, const QString& subfolderPattern = "");
    void stopSearch();
    static bool isFolderIndexed(const QString& folderPath, int* fileCount = nullptr);
    static bool databaseExists();

signals:
    void resultsReady(const QStringList& results);
    void finished();  // Signal when search completes

private slots:
    void performDatabaseSearch();
    void performFdSearch();

private:
    QSqlDatabase db;
    class QProcess* fdProcess;
    QString searchTerm;
    QString folderFilter;
    QString subfolderFilterPattern;
    bool useFolderFilter;
    QAtomicInt stopRequested;
    QMutex processMutex;
};

#endif // SEARCHWORKER_H