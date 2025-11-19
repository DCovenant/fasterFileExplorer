#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "IndexerDialog.h"
#include <QListWidget>
#include <QScrollBar>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QThread>
#include <QKeyEvent>
#include "SearchWorker.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->listTable->setStyleSheet("QListWidget { background-color: #c9c9c9; font-size: 11pt; } QListWidget::item { padding: 4px; }");
    ui->searchTerm->setEnabled(true);
    ui->lineFolderFilter->setEnabled(false);
    ui->searchButton->setEnabled(true);
    ui->showMore->setEnabled(false);
    ui->filterResultsButton->setEnabled(false);
    ui->filterResultsLineEdit->setEnabled(false);
    ui->finderStatus->setText("Stopped");
    ui->overallAppStatus->setText("Ready to search database. Folder filtering is optional.");
    
    connect(ui->listTable, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QString path = item->text();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        ui->overallAppStatus->setText("Opened: " + path);
    });

    connect(ui->listTable->verticalScrollBar(), &QScrollBar::valueChanged, 
            this, &MainWindow::onScrollChanged);
    
    batchTimer = new QTimer(this);
    batchTimer->setInterval(500);
    connect(batchTimer, &QTimer::timeout, this, &MainWindow::flushBatchedResults);
    
    connect(ui->fileFormatFilterDropdownbox, &QComboBox::currentTextChanged, 
            this, &MainWindow::on_fileFormatFilterDropdownbox_currentTextChanged);
    
    ui->foundItemsInfoLabel->setText("0");
    
    checkDatabaseStatus();
}

void MainWindow::checkDatabaseStatus() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString dbPath = QDir::cleanPath(appDir + "/../file_index.db");
    
    if (!QFile::exists(dbPath)) {
        QMessageBox::warning(this, "No Index Found",
            "There is no index database available.\n\n"
            "To create an index:\n"
            "1. Go to Index > Add Index\n"
            "2. Select a folder to index\n"
            "3. Wait for indexing to complete\n\n"
            "Searching can still be performed using the fallback file search (fd), "
            "but it will be significantly slower than using an indexed database.");
        ui->overallAppStatus->setText("WARNING: No index found. Fallback search will be used (slower).");
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionButton_clicked()
{
    selectedFolder = QFileDialog::getExistingDirectory(
        this, tr("Select Folder"), "C:/",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!selectedFolder.isEmpty()) {
        ui->actionButton->setText(selectedFolder);
        ui->searchTerm->setEnabled(true);
        ui->searchButton->setEnabled(true);
        ui->searchTerm->setFocus();
        
        int fileCount = 0;
        if (SearchWorker::isFolderIndexed(selectedFolder, &fileCount)) {
            ui->overallAppStatus->setText(QString("Base folder set: %1 files indexed. Ready to search.").arg(fileCount));
        } else {
            ui->overallAppStatus->setText("WARNING: This folder is not indexed! Please index it first using 'Index > Add Index'.");
        }
    }
}

void MainWindow::on_searchTerm_returnPressed()
{
    on_searchButton_clicked();
}

void MainWindow::on_stopButton_clicked()
{
    ui->showMore->setEnabled(false);
    
    if (currentWorker) {
        emit stopSearchRequested();
        batchTimer->stop();
        ui->finderStatus->setText("Stopped");
        ui->overallAppStatus->setText("Search stopped by user.");
        ui->filterResultsButton->setEnabled(true);
        ui->filterResultsLineEdit->setEnabled(true);
    }
}

void MainWindow::on_folderFilter_toggled(bool checked)
{
    ui->lineFolderFilter->setEnabled(checked);
    ui->overallAppStatus->setText("Folder filtering activated.");
}

void MainWindow::onScrollChanged(int value)
{
    QScrollBar* scrollBar = ui->listTable->verticalScrollBar();
    autoScroll = (value >= scrollBar->maximum() - 10);
}

MainWindow::SearchParams MainWindow::getSearchParameters() {
    SearchParams params;
    params.searchTerm = ui->searchTerm->text().trimmed();
    
    QString basePath = selectedFolder.isEmpty() ? "" : selectedFolder;
    QString subfolderFilter = ui->lineFolderFilter->text().trimmed();
    
    if (!basePath.isEmpty()) {
        params.folderFilter = basePath;
        params.subfolderPattern = (ui->folderFilter->isChecked() && !subfolderFilter.isEmpty()) ? subfolderFilter : "";
        params.useFolderFilter = true;
    } else {
        params.folderFilter = "";
        params.subfolderPattern = "";
        params.useFolderFilter = false;
    }
    
    return params;
}

void MainWindow::on_searchButton_clicked() {
    if (currentWorker) {
        emit stopSearchRequested();
        batchTimer->stop();
        QThread::msleep(100);
    }
    
    ui->overallAppStatus->setText("Search began.");
    ui->listTable->clear();
    allResults.clear();
    autoScroll = true;
    ui->finderStatus->setText("Running");
    ui->showMore->setEnabled(false);
    
    currentResultCount = 0;
    totalResultCount = 0;
    limitReached = false;
    searchFinished = false;
    
    batchedResults.clear();
    batchTimer->start();

    SearchParams params = getSearchParameters();
    if (params.searchTerm.isEmpty()) {
        ui->overallAppStatus->setText("Please enter a search term.");
        ui->finderStatus->setText("Stopped");
        batchTimer->stop();
        return;
    }
    
    if (!selectedFolder.isEmpty() && SearchWorker::databaseExists()) {
        int fileCount = 0;
        if (!SearchWorker::isFolderIndexed(selectedFolder, &fileCount)) {
            ui->overallAppStatus->setText("WARNING: Selected folder is not indexed. Using fallback search (slower).");
        } else {
            QString subfolderText = ui->lineFolderFilter->text().trimmed();
            if (!subfolderText.isEmpty() && ui->folderFilter->isChecked()) {
                ui->overallAppStatus->setText(QString("Searching folders matching regex '%1' within base folder (%2 total files)...").arg(subfolderText).arg(fileCount));
            } else {
                ui->overallAppStatus->setText(QString("Searching in base folder (%1 files)...").arg(fileCount));
            }
        }
    } else if (!selectedFolder.isEmpty()) {
        ui->overallAppStatus->setText(QString("Searching in %1 using fallback search...").arg(selectedFolder));
    } else {
        ui->overallAppStatus->setText("Searching entire database...");
    }

    QThread* thread = new QThread;
    SearchWorker* worker = new SearchWorker;
    currentWorker = worker;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [worker, params]() {
        worker->startSearch(params.searchTerm, params.folderFilter, params.useFolderFilter, params.subfolderPattern);
    });
    connect(worker, &SearchWorker::resultsReady, this, &MainWindow::updateList);
    connect(this, &MainWindow::stopSearchRequested, worker, &SearchWorker::stopSearch);
    connect(worker, &SearchWorker::finished, this, [this]() {
        batchTimer->stop();
        flushBatchedResults();
        searchFinished = true;
        currentWorker = nullptr;
        ui->finderStatus->setText("Stopped");
        
        if (limitReached) {
            ui->overallAppStatus->setText(QString("Search complete. Showing %1 of %2 total results.").arg(currentResultCount).arg(totalResultCount));
        } else {
            ui->overallAppStatus->setText(QString("Search complete. Found %1 results.").arg(totalResultCount));
        }
        
        ui->filterResultsButton->setEnabled(true);
        ui->filterResultsLineEdit->setEnabled(true);
    });
    connect(worker, &SearchWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &SearchWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}

void MainWindow::updateList(const QStringList& results) {
    totalResultCount += results.size();
    if (limitReached) return;
    
    batchedResults.append(results);
    
    if (batchedResults.size() > 500) {
        flushBatchedResults();
    }
}

void MainWindow::flushBatchedResults() {
    if (batchedResults.isEmpty()) return;
    
    QStringList filteredResults;
    QString fileFormatFilter = ui->fileFormatFilterDropdownbox->currentText();
    bool filterByFormat = (fileFormatFilter != "All Files");
    
    for (const QString& line : batchedResults) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            if (!filterByFormat || trimmed.toLower().endsWith(fileFormatFilter.toLower())) {
                filteredResults.append(trimmed);
                allResults.append(trimmed);
            }
        }
    }
    
    if (filteredResults.isEmpty()) {
        batchedResults.clear();
        return;
    }
    
    ui->listTable->setUpdatesEnabled(false);
    
    for (const QString& text : filteredResults) {
        QListWidgetItem* item = new QListWidgetItem(text);
        item->setBackground(Qt::transparent);
        item->setForeground(Qt::black);
        ui->listTable->addItem(item);
    }
    
    ui->listTable->setUpdatesEnabled(true);
    
    currentResultCount += filteredResults.size();
    updateFoundItemsLabel();
    
    if (currentResultCount >= MAX_RESULTS) {
        limitReached = true;
        ui->showMore->setEnabled(true);
        ui->filterResultsButton->setEnabled(true);
        ui->filterResultsLineEdit->setEnabled(true);
        ui->overallAppStatus->setText(QString("Showing %1 results. Total found: %2+ (click More to load more)").arg(currentResultCount).arg(totalResultCount));
        batchTimer->stop();
    }
    
    if (autoScroll) {
        ui->listTable->scrollToBottom();
    }
    
    batchedResults.clear();
}

void MainWindow::on_showMore_clicked()
{
    if (searchFinished) {
        ui->overallAppStatus->setText(QString("All %1 results are displayed.").arg(totalResultCount));
        ui->showMore->setEnabled(false);
        return;
    }
    
    limitReached = false;
    ui->showMore->setEnabled(false);
    ui->filterResultsLineEdit->clear();
    ui->filterResultsLineEdit->setEnabled(false);
    ui->filterResultsButton->setEnabled(false);
    ui->overallAppStatus->setText("Loading more results...");
    
    ui->listTable->setUpdatesEnabled(false);
    for (int i = 0; i < ui->listTable->count(); ++i) {
        QListWidgetItem* item = ui->listTable->item(i);
        item->setBackground(Qt::transparent);
        item->setForeground(Qt::black);
    }
    ui->listTable->setUpdatesEnabled(true);
    
    batchTimer->start();
}

void MainWindow::on_filterResultsButton_clicked()
{
    QString filterText = ui->filterResultsLineEdit->text().trimmed();
    
    if (filterText.isEmpty()) {
        for (int i = 0; i < ui->listTable->count(); ++i) {
            QListWidgetItem* item = ui->listTable->item(i);
            item->setBackground(Qt::transparent);
            item->setForeground(Qt::black);
        }
        ui->overallAppStatus->setText("Filter cleared.");
    } else {
        applyResultsFilter(filterText);
    }
}

void MainWindow::applyResultsFilter(const QString& filterText)
{
    int matchCount = 0;
    ui->listTable->setUpdatesEnabled(false);
    
    for (int i = 0; i < ui->listTable->count(); ++i) {
        QListWidgetItem* item = ui->listTable->item(i);
        
        if (item->text().contains(filterText, Qt::CaseInsensitive)) {
            item->setBackground(QColor(255, 255, 200));
            item->setForeground(Qt::black);
            matchCount++;
        } else {
            item->setBackground(Qt::transparent);
            item->setForeground(QColor(150, 150, 150));
        }
    }
    
    ui->listTable->setUpdatesEnabled(true);
    ui->overallAppStatus->setText(QString("Filter applied: %1 matches found").arg(matchCount));
}

void MainWindow::updateFoundItemsLabel() {
    ui->foundItemsInfoLabel->setText(QString::number(ui->listTable->count()));
}

void MainWindow::on_fileFormatFilterDropdownbox_currentTextChanged(const QString &text) {
    ui->listTable->clear();
    currentResultCount = 0;
    
    if (allResults.isEmpty()) {
        updateFoundItemsLabel();
        return;
    }
    
    bool filterByFormat = (text != "All Files");
    
    ui->listTable->setUpdatesEnabled(false);
    
    for (const QString& path : allResults) {
        if (!filterByFormat || path.toLower().endsWith(text.toLower())) {
            QListWidgetItem* item = new QListWidgetItem(path);
            item->setBackground(Qt::transparent);
            item->setForeground(Qt::black);
            ui->listTable->addItem(item);
            currentResultCount++;
            
            if (currentResultCount >= MAX_RESULTS) break;
        }
    }
    
    ui->listTable->setUpdatesEnabled(true);
    updateFoundItemsLabel();
    
    ui->overallAppStatus->setText(QString("File format filter applied: showing %1 results").arg(currentResultCount));
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // Check for Ctrl+Shift+Alt+T
    if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier) && 
        event->key() == Qt::Key_T) {
        toggleConsoleWindow();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::toggleConsoleWindow() {
#ifdef Q_OS_WIN
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow) {
        if (IsWindowVisible(consoleWindow)) {
            ShowWindow(consoleWindow, SW_HIDE);
            ui->overallAppStatus->setText("Debug console hidden");
        } else {
            ShowWindow(consoleWindow, SW_SHOW);
            ui->overallAppStatus->setText("Debug console shown");
        }
    }
#endif
}

void MainWindow::on_actionAdd_Index_triggered()
{
    IndexerDialog dialog(this);
    dialog.exec();
}