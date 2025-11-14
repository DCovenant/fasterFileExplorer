#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QListWidget>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , process(new QProcess(this))
{
    ui->setupUi(this);
    
    // Initial UI state
    ui->searchTerm->setEnabled(false);
    ui->lineFolderFilter->setEnabled(false);
    ui->searchButton->setEnabled(false);
    ui->finderStatus->setText("Stopped");

    // Process setup
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setReadChannel(QProcess::StandardOutput);

    // Connect signals
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessReadyRead);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &MainWindow::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &MainWindow::onProcessError);
    
    connect(ui->listTable, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QString path = item->text();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        ui->overallAppStatus->setText("Oppened: "+ path);
    });
        
    // Auto-scroll management
    connect(ui->listTable->verticalScrollBar(), &QScrollBar::valueChanged, 
            this, &MainWindow::onScrollChanged);
}

MainWindow::~MainWindow()
{
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished(1000);
    }
    delete ui;
}

void MainWindow::on_actionButton_clicked()
{
    selectedFolder = QFileDialog::getExistingDirectory(
        this,
        tr("Select Folder"),
        "L:/",  // Start at network drive
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!selectedFolder.isEmpty()) {
        ui->folderName->setText(selectedFolder);
        ui->searchTerm->setEnabled(true);
        ui->searchButton->setEnabled(true);
        ui->searchTerm->setFocus();  // Auto-focus for quick typing
    }
}

void MainWindow::on_searchTerm_returnPressed()
{
    ui->overallAppStatus->setText("Search began.");
    startSearch();
}

void MainWindow::on_searchButton_clicked()
{
    ui->overallAppStatus->setText("Search began.");
    startSearch();
}

void MainWindow::startSearch()
{
    QString searchText = ui->searchTerm->text().trimmed();
    
    if (selectedFolder.isEmpty() || searchText.isEmpty()) {
        return;
    }

    // Stop any running search
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished(500);
    }
    
    ui->listTable->clear();
    autoScroll = true;
    ui->finderStatus->setText("Running");

    // Build fd.exe arguments
    QStringList args;
    args << "-i";  // Case-insensitive
    
    // Folder filter (if enabled)
    if (ui->folderFilter->isChecked() && !ui->lineFolderFilter->text().isEmpty()) {
        args << "-p";  // Match against full path
        args << (".*" + ui->lineFolderFilter->text() + ".*" + searchText);
    } else {
        args << searchText;
    }
    
    args << selectedFolder;

    // Get fd.exe path (must be in same directory as executable)
    QString appDir = QCoreApplication::applicationDirPath();
    QString fdPath = QDir::cleanPath(appDir + "/fd.exe");
    
    qDebug() << "Application directory:" << appDir;
    qDebug() << "Looking for fd.exe at:" << fdPath;
    qDebug() << "fd.exe exists:" << QFile::exists(fdPath);
    qDebug() << "Search command:" << fdPath << args.join(" ");
    
    if (!QFile::exists(fdPath)) {
        QString errorMsg = QString("ERROR: fd.exe not found at:\n%1").arg(fdPath);
        ui->finderStatus->setText("fd.exe not found!");
        qDebug() << errorMsg;
        QMessageBox::critical(this, "Error", errorMsg);
        return;
    }
    
    // Start the process
    process->start(fdPath, args);
    
    // Wait a moment to see if it starts
    if (!process->waitForStarted(1000)) {
        QString errorMsg = QString("Failed to start fd.exe\nError: %1\nState: %2")
            .arg(process->errorString())
            .arg(process->state());
        ui->finderStatus->setText("Failed to start fd.exe");
        qDebug() << errorMsg;
        QMessageBox::critical(this, "Error", errorMsg);
        return;
    }
}

void MainWindow::on_stopButton_clicked()
{
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished(500);
        ui->finderStatus->setText("Stopped");
        ui->overallAppStatus->setText("Search stopped.");

    }
}

void MainWindow::on_folderFilter_toggled(bool checked)
{
    ui->lineFolderFilter->setEnabled(checked);
    ui->overallAppStatus->setText("Folder filtering activated.");
}

void MainWindow::onProcessReadyRead()
{
    QString output = process->readAllStandardOutput();
    QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    
    if (lines.isEmpty()) return;

    // Batch insert for performance
    ui->listTable->setUpdatesEnabled(false);
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            ui->listTable->insertItem(0, new QListWidgetItem(trimmed));
        }
    }
    
    ui->listTable->setUpdatesEnabled(true);
    
    if (autoScroll) {
        ui->listTable->scrollToTop();
    }
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    
    // Read any remaining output
    onProcessReadyRead();
    
    if (exitCode != 0) {
        ui->finderStatus->setText("error");
        qDebug() << "fd.exe exited with code:" << exitCode;
    } else {
        ui->finderStatus->setText("Stopped");
        ui->overallAppStatus->setText("Search finnished.");
    }
}

void MainWindow::onProcessError(QProcess::ProcessError error)
{
    ui->finderStatus->setText("ERROR");
    qDebug() << "Process error:" << error << process->errorString();
}

void MainWindow::onScrollChanged(int value)
{
    // Disable auto-scroll if user manually scrolls away from top
    autoScroll = (value <= 10);
}