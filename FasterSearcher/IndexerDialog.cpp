#include "IndexerDialog.h"
#include "ui_IndexerDialog.h"
#include <QFileDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QTextCursor>
#include <QScrollBar>
#include <QThread>
#include <QFile>
#include <QRegularExpression>
#include <QTimer>

IndexerDialog::IndexerDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::IndexerDialog), indexerProcess(nullptr), outputTimer(nullptr)
{
    ui->setupUi(this);
    
    QString appDir = QCoreApplication::applicationDirPath();
    dbPath = QDir::cleanPath(appDir + "/file_index.db");
    
    appendOutput("=== File Index Manager ===", "white");
    appendOutput(QString("Database location: %1").arg(dbPath), "gray");
    appendOutput("Click 'Open Folder' to select a folder to index.\n", "gray");
}

IndexerDialog::~IndexerDialog()
{
    if (outputTimer) {
        outputTimer->stop();
        delete outputTimer;
    }
    if (indexerProcess) {
        indexerProcess->kill();
        indexerProcess->waitForFinished();
        delete indexerProcess;
    }
    delete ui;
}

void IndexerDialog::appendOutput(const QString &text, const QString &color)
{
    QTextCursor cursor = ui->outputDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->outputDisplay->setTextCursor(cursor);
    
    QString coloredText = QString("<span style='color:%1;'>%2</span><br>")
                              .arg(color, text.toHtmlEscaped());
    ui->outputDisplay->insertHtml(coloredText);
    
    ui->outputDisplay->verticalScrollBar()->setValue(
        ui->outputDisplay->verticalScrollBar()->maximum()
    );
    
    ui->outputDisplay->repaint();
    QCoreApplication::processEvents();
}

bool IndexerDialog::checkFolderInDatabase(const QString &folderPath)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "check_connection");
    db.setDatabaseName(dbPath);
    
    if (!db.open()) {
        appendOutput(QString("Error: Could not open database: %1").arg(db.lastError().text()), "red");
        return false;
    }
    
    bool hasFiles = false;
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM files WHERE path LIKE ?");
    query.bindValue(0, folderPath + "%");
    
    if (query.exec() && query.next()) {
        hasFiles = (query.value(0).toInt() > 0);
    }
    
    QString connName = "check_connection";
    if (db.isOpen()) db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    
    return hasFiles;
}

QString IndexerDialog::getRustExecutablePath()
{
    QString appDir = QCoreApplication::applicationDirPath();
    
    QStringList paths = {
        QDir::cleanPath(appDir + "/rust_version/target/release/rustIndexP2Sql.exe"),
        QDir::cleanPath(appDir + "/../rust_version/target/release/rustIndexP2Sql.exe"),
        QDir::cleanPath(appDir + "/../../rust_version/target/release/rustIndexP2Sql.exe")
    };
    
    for (const QString& path : paths) {
        if (QFile::exists(path)) return path;
    }
    
    return "";
}

void IndexerDialog::on_openFolderButton_clicked()
{
    QString folder = QFileDialog::getExistingDirectory(
        this, tr("Select Folder to Index"), "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (folder.isEmpty()) return;
    
    selectedFolderPath = QDir::toNativeSeparators(folder);
    if (!selectedFolderPath.endsWith(QDir::separator())) {
        selectedFolderPath += QDir::separator();
    }
    
    appendOutput(QString("\nSelected folder: %1").arg(selectedFolderPath), "lightblue");
    
    if (checkFolderInDatabase(selectedFolderPath)) {
        appendOutput("[OK] This folder is already in the database.", "green");
        appendOutput("Indexing will update existing entries and add new files.", "gray");
    } else {
        appendOutput("[!] This folder has not been indexed yet.", "orange");
    }
    
    ui->startIndexingButton->setEnabled(true);
}

void IndexerDialog::on_startIndexingButton_clicked()
{
    if (selectedFolderPath.isEmpty()) {
        appendOutput("Error: No folder selected!", "red");
        return;
    }
    
    QString rustExe = getRustExecutablePath();
    
    if (rustExe.isEmpty() || !QFile::exists(rustExe)) {
        appendOutput("Error: Rust indexer not found!", "red");
        appendOutput(QString("Expected location: %1").arg(getRustExecutablePath()), "red");
        return;
    }
    
    ui->openFolderButton->setEnabled(false);
    ui->startIndexingButton->setEnabled(false);
    
    appendOutput(QString("\nStarting indexer: %1").arg(rustExe), "lightblue");
    appendOutput(QString("Target: %1").arg(selectedFolderPath), "lightblue");
    appendOutput("============================================================\n", "gray");
    
    if (indexerProcess) delete indexerProcess;
    indexerProcess = new QProcess(this);
    
    indexerProcess->setProcessChannelMode(QProcess::SeparateChannels);
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("RUST_LOG", "info");
    indexerProcess->setProcessEnvironment(env);
    
    connect(indexerProcess, &QProcess::readyReadStandardOutput, 
            this, &IndexerDialog::handleProcessOutput);
    connect(indexerProcess, &QProcess::readyReadStandardError, 
            this, &IndexerDialog::handleProcessOutput);
    connect(indexerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &IndexerDialog::handleProcessFinished);
    
    if (!outputTimer) {
        outputTimer = new QTimer(this);
        connect(outputTimer, &QTimer::timeout, this, &IndexerDialog::checkForOutput);
    }
    outputTimer->start(100);
    
    QString workingDir = QCoreApplication::applicationDirPath();
    indexerProcess->setWorkingDirectory(workingDir);
    
    int threadCount = QThread::idealThreadCount();
    if (threadCount < 1) threadCount = 4;
    
    QStringList arguments;
    arguments << selectedFolderPath << QString::number(threadCount) << dbPath;
    
    indexerProcess->start(rustExe, arguments);
    
    if (!indexerProcess->waitForStarted(5000)) {
        appendOutput("Error: Failed to start indexer process!", "red");
        appendOutput(QString("Error details: %1").arg(indexerProcess->errorString()), "red");
        ui->openFolderButton->setEnabled(true);
        ui->startIndexingButton->setEnabled(true);
    } else {
        appendOutput("Process started successfully...", "green");
    }
}

void IndexerDialog::handleProcessOutput()
{
    if (!indexerProcess) return;
    
    QByteArray data = indexerProcess->readAllStandardOutput();
    QString output = QString::fromUtf8(data);
    
    QStringList lines = output.split(QRegularExpression("[\r\n]"));
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.length() < 3) continue;
        
        QString cleanLine = trimmed;
        cleanLine.replace(QRegularExpression("[\U0001F300-\U0001F9FF]"), "");
        
        QString color = "lightblue";
        
        if (cleanLine.contains("complete", Qt::CaseInsensitive) || 
            cleanLine.contains("success", Qt::CaseInsensitive) ||
            cleanLine.contains("Found")) {
            color = "green";
        } else if (cleanLine.contains("Error", Qt::CaseInsensitive) || 
                   cleanLine.contains("failed", Qt::CaseInsensitive)) {
            color = "red";
        } else if (cleanLine.contains("Target path") || 
                   cleanLine.contains("Database") ||
                   cleanLine.contains("INDEXER")) {
            color = "white";
        }
        
        if (!cleanLine.isEmpty()) {
            appendOutput(cleanLine, color);
        }
    }
}

void IndexerDialog::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (indexerProcess) {
        QString remainingOutput = QString::fromUtf8(indexerProcess->readAllStandardOutput());
        QString remainingError = QString::fromUtf8(indexerProcess->readAllStandardError());
        
        if (!remainingOutput.isEmpty()) appendOutput(remainingOutput, "lightblue");
        if (!remainingError.isEmpty()) appendOutput(remainingError, "red");
    }
    
    appendOutput("\n============================================================", "gray");
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        appendOutput("[OK] Indexing completed successfully!", "green");
    } else if (exitStatus == QProcess::CrashExit) {
        appendOutput("[ERROR] Process crashed!", "red");
    } else {
        appendOutput(QString("[WARNING] Process finished with exit code: %1").arg(exitCode), "red");
    }
    
    ui->openFolderButton->setEnabled(true);
    ui->startIndexingButton->setEnabled(false);
    
    if (outputTimer) outputTimer->stop();
}

void IndexerDialog::checkForOutput()
{
    if (!indexerProcess) {
        if (outputTimer && outputTimer->isActive()) outputTimer->stop();
        return;
    }
    
    QProcess::ProcessState state = indexerProcess->state();
    if (state == QProcess::NotRunning) {
        handleProcessFinished(indexerProcess->exitCode(), indexerProcess->exitStatus());
        if (outputTimer && outputTimer->isActive()) outputTimer->stop();
        return;
    }
    
    if (state != QProcess::Running) return;
    
    indexerProcess->waitForReadyRead(10);
    
    QByteArray stdoutData = indexerProcess->readAllStandardOutput();
    QByteArray stderrData = indexerProcess->readAllStandardError();
    
    if (!stdoutData.isEmpty()) {
        processOutput(QString::fromUtf8(stdoutData));
    }
    
    if (!stderrData.isEmpty()) {
        processOutput(QString::fromUtf8(stderrData));
    }
}

void IndexerDialog::processOutput(const QString &output)
{
    QStringList lines = output.split(QRegularExpression("[\r\n]"));
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        
        QString cleanLine = trimmed;
        cleanLine.replace(QRegularExpression("[\U0001F300-\U0001F9FF]"), "");
        
        QString color = "lightblue";
        if (cleanLine.contains("complete", Qt::CaseInsensitive) || 
            cleanLine.contains("success", Qt::CaseInsensitive) ||
            cleanLine.contains("Found")) {
            color = "green";
        } else if (cleanLine.contains("Error", Qt::CaseInsensitive) || 
                   cleanLine.contains("failed", Qt::CaseInsensitive)) {
            color = "red";
        }
        
        appendOutput(cleanLine, color);
    }
}