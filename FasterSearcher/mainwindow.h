#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QProcess>
#include <QListWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionButton_clicked();
    void on_searchTerm_returnPressed();
    void on_searchButton_clicked();
    void on_stopButton_clicked();
    void on_folderFilter_toggled(bool checked);
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onScrollChanged(int value);

private:
    void startSearch();  // Consolidated search logic
    
    Ui::MainWindow *ui;
    QProcess *process;
    QString selectedFolder;
    bool autoScroll = true;
};

#endif // MAINWINDOW_H