#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QProcess>
#include <QListWidgetItem>
#include <QTimer>

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

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void on_actionButton_clicked();
    void on_searchTerm_returnPressed();
    void on_searchButton_clicked();
    void on_stopButton_clicked();
    void on_folderFilter_toggled(bool checked);
    void on_showMore_clicked();
    void on_filterResultsButton_clicked();
    void on_fileFormatFilterDropdownbox_currentTextChanged(const QString &text);
    void on_actionAdd_Index_triggered();
    void updateList(const QStringList& results);
    void onScrollChanged(int value);
    void flushBatchedResults();  // Process accumulated results
    void updateFoundItemsLabel();  // Update the found items count label

signals:
    void stopSearchRequested();  // Signal to stop the search

private:
    struct SearchParams {
        QString searchTerm;
        QString folderFilter;
        QString subfolderPattern;
        bool useFolderFilter;
    };
    SearchParams getSearchParameters();
    void applyResultsFilter(const QString& filterText);
    void checkDatabaseStatus();
    void toggleConsoleWindow();

    Ui::MainWindow *ui;
    QString selectedFolder;
    bool autoScroll = true;
    
    // Batch processing for UI updates
    QStringList batchedResults;
    QStringList allResults;  // Store all results for filtering
    QTimer* batchTimer;
    
    // Search worker management
    class SearchWorker* currentWorker = nullptr;
    
    // Result limiting
    static const int MAX_RESULTS = 5000;  // Balance between performance and usability
    int currentResultCount = 0;
    int totalResultCount = 0;  // Track total results found
    bool limitReached = false;
    bool searchFinished = false;
};

#endif // MAINWINDOW_H