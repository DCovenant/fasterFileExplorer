#ifndef SEARCHWORKER_H
#define SEARCHWORKER_H

#include <QObject>
#include <QProcess>
#include <QStringList>

class SearchWorker : public QObject {
    Q_OBJECT
public:
    explicit SearchWorker(QObject *parent = nullptr);
    void startSearch(const QString& command);
    void stopSearch();

signals:
    void resultsReady(const QStringList& results);
    void finished();  // Signal when search completes

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess* process;
};

#endif // SEARCHWORKER_H