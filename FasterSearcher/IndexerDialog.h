#ifndef INDEXERDIALOG_H
#define INDEXERDIALOG_H

#include <QDialog>
#include <QProcess>
#include <QSqlDatabase>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class IndexerDialog;
}
QT_END_NAMESPACE

class IndexerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IndexerDialog(QWidget *parent = nullptr);
    ~IndexerDialog();

private slots:
    void on_openFolderButton_clicked();
    void on_startIndexingButton_clicked();
    void handleProcessOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    Ui::IndexerDialog *ui;
    QProcess *indexerProcess;
    QTimer *outputTimer;
    QString selectedFolderPath;
    QString dbPath;
    
    void appendOutput(const QString &text, const QString &color = "black");
    bool checkFolderInDatabase(const QString &folderPath);
    int countFilesNotInDatabase(const QString &folderPath);
    QString getRustExecutablePath();
    void checkForOutput();
    void processOutput(const QString &output);
};

#endif // INDEXERDIALOG_H
