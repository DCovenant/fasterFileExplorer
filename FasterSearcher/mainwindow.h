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
    void on_folderFilter_toggled(bool checked);
    void on_stopButton_clicked();
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void on_searchButton_clicked();

private:
    Ui::MainWindow *ui;
    bool autoScroll = true;  // Track if we should auto-scroll
};
#endif // MAINWINDOW_H