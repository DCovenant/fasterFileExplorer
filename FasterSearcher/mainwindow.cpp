#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QListWidget>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>


QString selectedFolder;
QString searchText;
QProcess *process = nullptr;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->searchTerm->setEnabled(false);
    ui->lineFolderFilter->setEnabled(false);  // Disabled by default
    ui->searchButton->setEnabled(false);
    ui->finderStatus->setText("waiting...");  // Initial status

    process = new QProcess(this);

    // Connect to read output in real-time
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessReadyRead);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::onProcessFinished);
    
    // Connect list item click to copy path
    connect(ui->listTable, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        QApplication::clipboard()->setText(item->text());
        qDebug() << "Copied to clipboard:" << item->text();
    });
    
    // Connect scrollbar to detect manual scrolling
    connect(ui->listTable->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        // If user scrolls away from top, disable auto-scroll
        QScrollBar *scrollBar = ui->listTable->verticalScrollBar();
        if (value > 10) {  // More than 10px from top
            autoScroll = false;
        } else {
            autoScroll = true;  // Re-enable if user scrolls back to top
        }
    });
}
MainWindow::~MainWindow(){
    if (process && process->state() == QProcess::Running) {
        process->kill();  // Clean up
    }
    delete ui;
}

void MainWindow::on_actionButton_clicked(){
    qDebug() << "Action button clicked";
    // Open folder selection dialog
    selectedFolder = QFileDialog::getExistingDirectory(
        this,
        tr("Select Folder"),  // Dialog title
        QDir::homePath(),     // Default directory (user's home)
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if(!selectedFolder.isEmpty()){
        ui->folderName->setText(selectedFolder);
        ui->searchTerm->setEnabled(true);
        ui->searchButton->setEnabled(true);
    }

}

void MainWindow::on_searchTerm_returnPressed(){
    searchText = ui->searchTerm->text();
    qDebug() << "Searching for:" << searchText;

    if (selectedFolder.isEmpty() || searchText.isEmpty()) return;

    // Clear previous results
    ui->listTable->clear();

    // Kill any running process
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished();
    }
    
    // Build the command arguments
    QStringList args;
    args << "-i";  // Case-insensitive
    
    // If folder filter is enabled, use --full-path to filter by folder name
    if (ui->folderFilter->isChecked() && !ui->lineFolderFilter->text().isEmpty()) {
        QString folderPattern = ui->lineFolderFilter->text();
        args << "-p";  // Match against full path
        args << (".*" + folderPattern + ".*" + searchText);  // Regex: any path containing folderPattern, then searchText
        qDebug() << "Folder filter active. Pattern:" << folderPattern;
    } else {
        args << searchText;  // What to search for (normal mode)
    }
    
    args << selectedFolder;  // Base directory (no wildcards)
    
    // Log the command for comparison
    qDebug() << "Running command: fdfind" << args.join(" ");
    qDebug() << "Arguments passed to fdfind:";
    for (int i = 0; i < args.size(); ++i) {
        qDebug() << "  arg[" << i << "]:" << args[i];
    }
    
    // Re-enable auto-scroll for new search
    autoScroll = true;
    
    // Start fd-find command
    process->start("fdfind", args);
    
    // Update status
    ui->finderStatus->setText("running");
}

void MainWindow::on_searchButton_clicked(){
    on_searchTerm_returnPressed();
}

void MainWindow::on_stopButton_clicked(){
    qDebug() << "Stop button clicked";
    
    // Kill the running process
    if (process && process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished();
        ui->finderStatus->setText("stopped");
        qDebug() << "Process stopped";
    }
}

void MainWindow::on_folderFilter_toggled(bool checked){
    // Enable/disable the folder filter line edit based on checkbox
    ui->lineFolderFilter->setEnabled(checked);
    qDebug() << "Folder filter checkbox toggled:" << checked;
}

void MainWindow::onProcessReadyRead(){
    // Read new output as it arrives
    QString newOutput = process->readAllStandardOutput();
    QStringList lines = newOutput.split('\n', Qt::SkipEmptyParts);
    
    // Add each new path to the top of the list
    for (const QString &line : lines) {
        if (!line.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(line);
            ui->listTable->insertItem(0, item);
        }
    }
    
    // Auto-scroll to top only if user hasn't manually scrolled away
    if (autoScroll) {
        ui->listTable->scrollToTop();
    }
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus){
    if (exitCode != 0) {
        qDebug() << "fd-find error:" << process->readAllStandardError();
    }
    // Process is done; any remaining output is handled in onProcessReadyRead
    
    // Update status
    ui->finderStatus->setText("waiting...");
}