#include "mainwindow.h"
#include "botviewdelegate.h"
#include "spinbox.h"
#include "tournamentcontroller.h"

#include <QHeaderView>
#include <QItemDelegate>
#include <QItemEditorFactory>
#include <QComboBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QApplication>
#include <QGroupBox>
#include <QSettings>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QCheckBox>
#include <QDebug>
#include <QFileDialog>
#include <QTimer>
#include <QLabel>
#include <QThread>
#include <QTime>
#include <QTabWidget>
#include <QQuickWidget>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QAction>
#include <QDirIterator>

#define SERVERPATH_KEY    "serverpath"
#define ROUNDS_KEY        "rounds"
#define AUTOSTART_KEY     "autostart"
#define AUTOQUIT_KEY      "autoquit"
#define TICKINTERVAL_KEY  "tickinterval"
#define FULLSCREEN_KEY    "fullscreen"
#define HEADLESS_KEY      "headless"
#define TICKLESS_KEY      "tickless"
#define ROUNDSPLAYED_KEY  "roundsplayed"

static MainWindow *instance = 0;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();

    if (instance) {
        switch (type) {
        case QtWarningMsg:
        case QtCriticalMsg:
            instance->errorOutput(msg);
            break;
        case QtFatalMsg:
            fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
            abort();
        case QtDebugMsg:
        case QtInfoMsg:
        default:
            instance->normalOutput(msg);
            break;
        }
    }
    switch (type) {
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        abort();
    case QtDebugMsg:
    case QtInfoMsg:
    default:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QSplitter(parent),
      m_botsView(new QTableView(this)),
      m_botModel(BotModel::instance()),
      m_logFile("BORG.log")
{
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append);

    instance = this;
    qInstallMessageHandler(messageHandler);

    QSettings settings;
    m_roundsPlayed = settings.value(ROUNDSPLAYED_KEY, 0).toInt();

    ///////////
    /// Main/left part of window
    ///
    QTabWidget *tabWidget = new QTabWidget;
    addWidget(tabWidget);
    tabWidget->setTabPosition(QTabWidget::East);

    ///////////
    /// Tournament overview
    ///
    QQuickWidget *tournamentView = new QQuickWidget(QUrl("qrc:/TournamentView.qml"));
    tournamentView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    tabWidget->addTab(tournamentView, "Tournament view");

    QWidget *setupWidget = new QWidget;
    QLayout *setupLayout = new QVBoxLayout;
    setupWidget->setLayout(setupLayout);

    ///////////
    /// Bot list view
    ///
    m_botsView->setItemDelegate(new BotViewDelegate);
    m_botsView->setModel(m_botModel);
    m_botsView->setShowGrid(false);
    m_botsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_botsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_botsView->resizeColumnsToContents();
    m_botsView->horizontalHeader()->setStretchLastSection(true);
    m_botsView->selectRow(0);
    m_botsView->setAlternatingRowColors(true);
    setupLayout->addWidget(m_botsView);


    ///////////
    /// Add/remove buttons
    ///
    QWidget *addRemoveGroup = new QWidget;
    addRemoveGroup->setLayout(new QHBoxLayout);
    setupLayout->addWidget(addRemoveGroup);
    // Reset tournament button
    QPushButton *resetBotButton = new QPushButton(tr("Reset &bots"));
    addRemoveGroup->layout()->addWidget(resetBotButton);
    // Reset tournament button
    QPushButton *resetTournamentButton = new QPushButton(tr("&Reset tournament"));
    addRemoveGroup->layout()->addWidget(resetTournamentButton);
    connect(resetTournamentButton, &QAbstractButton::clicked, this, &MainWindow::resetBots);
    addRemoveGroup->layout()->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding));
    // Add button
    QPushButton *addButton = new QPushButton(tr("&Add new bot..."));
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addBot);
    addRemoveGroup->layout()->addWidget(addButton);
    // Add multiple button
    QPushButton *addMultipleButton = new QPushButton(tr("Add &multiple bots..."));
    connect(addMultipleButton, &QPushButton::clicked, this, &MainWindow::addBots);
    addRemoveGroup->layout()->addWidget(addMultipleButton);
    // Remove button
    QPushButton *removeButton = new QPushButton(tr("Remo&ve selected"));
    connect(removeButton, SIGNAL(clicked()), SLOT(removeBot()));
    addRemoveGroup->layout()->addWidget(removeButton);

    tabWidget->addTab(setupWidget, "S&etup");

    ///////////
    /// Server control
    ///
    QWidget *serverBox = new QWidget;
    serverBox->setLayout(new QVBoxLayout);
    setupLayout->addWidget(serverBox);

    ///////////
    /// Server settings
    ///
    QWidget *serverLaunch = new QWidget;
    serverLaunch->setLayout(new QHBoxLayout);
    serverBox->layout()->addWidget(serverLaunch);
    // Server path editor
    m_serverPath = new PathEditor;
    serverLaunch->layout()->addWidget(m_serverPath);
    m_serverPath->setPath(settings.value(SERVERPATH_KEY, "").toString());

    ///////////
    /// Server settings
    ///
    QWidget *serverSettings = new QWidget;
    QBoxLayout *serverSettingsLayout = new QHBoxLayout;
    serverSettings->setLayout(serverSettingsLayout);
    serverBox->layout()->addWidget(serverSettings);
    // Round count editor
    m_rounds = new QSpinBox;
    m_rounds->setMinimum(1);
    m_rounds->setMaximum(10000);
    m_rounds->setSuffix(tr(" rounds"));
    serverSettings->layout()->addWidget(m_rounds);
    m_rounds->setValue(settings.value(ROUNDS_KEY, 4).toInt());
    // Tick interval editor
    m_tickInterval = new QSpinBox;
    m_tickInterval->setMinimum(10);
    m_tickInterval->setMaximum(1000);
    m_tickInterval->setSuffix(" ms ticks");
    serverSettings->layout()->addWidget(m_tickInterval);
    m_tickInterval->setValue(settings.value(TICKINTERVAL_KEY, 50).toInt());
    // Autostart checkbox
    m_autoLaunch = new QCheckBox(tr("Start automatically"));
    serverSettings->layout()->addWidget(m_autoLaunch);
    m_autoLaunch->setChecked(settings.value(AUTOSTART_KEY, false).toBool());
    // Autoquit checkbox
    m_autoQuit = new QCheckBox(tr("Quit on game over"));
    serverSettings->layout()->addWidget(m_autoQuit);
    m_autoQuit->setChecked(settings.value(AUTOQUIT_KEY, false).toBool());
    // Fullscreen checkbox
    m_fullscreen = new QCheckBox(tr("Fullscreen"));
    serverSettings->layout()->addWidget(m_fullscreen);
    m_fullscreen->setChecked(settings.value(FULLSCREEN_KEY, false).toBool());
    // Headless checkbox
    m_headless = new QCheckBox(tr("Headless mode"));
    serverSettings->layout()->addWidget(m_headless);
    m_headless->setChecked(settings.value(HEADLESS_KEY, false).toBool());
    // Tickless checkbox
    m_tickless = new QCheckBox(tr("Tickless mode"));
    serverSettings->layout()->addWidget(m_tickless);
    m_tickless->setChecked(settings.value(TICKLESS_KEY, false).toBool());
    serverSettingsLayout->addStretch();

    ///////////
    /// Right part of window
    ///
    QWidget *rightWidget = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightWidget->setLayout(rightLayout);
    addWidget(rightWidget);
    // Top players list
    m_topPlayers.setText("<h3>Top players</h3><ol><li>...</li></ol>");
    rightWidget->layout()->addWidget(&m_topPlayers);
    updateTopPlayers();
    // Log/output view
    m_serverOutput.setReadOnly(true);
//    m_serverOutput.setTextColor(Qt::black);
    rightLayout->addWidget(&m_serverOutput, 10);
    // Server launch button
    QPushButton *launchButton = new QPushButton(tr("&Start server"));
    launchButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rightLayout->addWidget(launchButton, 2);
    // Kill button
    QPushButton *killButton = new QPushButton(tr("&Kill server"));
    rightWidget->layout()->addWidget(killButton);

    rightLayout->addSpacing(10);

    ///////////
    /// Quit button
    ///
    QPushButton *quitButton = new QPushButton(tr("&Quit"));
    connect(quitButton, SIGNAL(clicked()), qApp, SLOT(quit()));
    rightWidget->layout()->addWidget(quitButton);

    QAction *showUiAction = new QAction;
    showUiAction->setShortcut(Qt::Key_Escape);
    connect(showUiAction, &QAction::triggered, this, [=]() {
        rightWidget->setVisible(!rightWidget->isVisible());
        tabWidget->tabBar()->setVisible(!tabWidget->tabBar()->isVisible());
    });
    addAction(showUiAction);

    // Connections
//    connect(killButton, SIGNAL(clicked()), SLOT(kill()));
    connect(killButton, &QPushButton::clicked, this, &MainWindow::kill);

    connect(m_serverPath, &PathEditor::pathChanged, this, &MainWindow::saveSettings);
    connect(m_rounds, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &MainWindow::saveSettings);

    connect(m_autoLaunch, &QCheckBox::stateChanged, this, &MainWindow::saveSettings);
    connect(m_autoQuit, &QCheckBox::stateChanged, this, &MainWindow::saveSettings);
    connect(m_tickInterval, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &MainWindow::saveSettings);
    connect(m_fullscreen, &QCheckBox::stateChanged, this, &MainWindow::saveSettings);
    connect(m_headless, &QCheckBox::stateChanged, this, &MainWindow::saveSettings);
    connect(m_tickless, &QCheckBox::stateChanged, this, &MainWindow::saveSettings);

    connect(launchButton, &QAbstractButton::clicked, this, &MainWindow::launchServer);
    connect(&m_serverProcess, &QProcess::readyReadStandardError, this, &MainWindow::readServerErr);
    connect(&m_serverProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::readServerOut);
    connect(&m_serverProcess, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &MainWindow::serverFinished);

    setMinimumSize(1920, 1080);
}

MainWindow::~MainWindow()
{
    kill();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(SERVERPATH_KEY, m_serverPath->path());
    settings.setValue(ROUNDS_KEY, m_rounds->value());
    settings.setValue(AUTOSTART_KEY, m_autoLaunch->isChecked());
    settings.setValue(AUTOQUIT_KEY, m_autoQuit->isChecked());
    settings.setValue(TICKINTERVAL_KEY, m_tickInterval->value());
    settings.setValue(FULLSCREEN_KEY, m_fullscreen->isChecked());
    settings.setValue(HEADLESS_KEY, m_headless->isChecked());
    settings.setValue(TICKLESS_KEY, m_tickless->isChecked());
    settings.setValue(ROUNDSPLAYED_KEY, m_roundsPlayed);
}

void MainWindow::launchServer()
{
    if (m_botModel->enabledPlayers() > 10 || m_botModel->enabledPlayers() < 1) {
        QMessageBox::warning(this, tr("Invalid number of players"), tr("Either too few or too many players enabled"));
        return;
    }

    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    m_logFile.setFileName(QDateTime::currentDateTime().toString(Qt::ISODate) + ".log");
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    m_logFile.write("Starting " + QByteArray::number(m_roundsPlayed) + "\n");

    if (m_serverProcess.state() == QProcess::Running) {
        QMessageBox::warning(this, tr("Server already running"), tr("The server executable is still running"));
        return;
    }

    QFileInfo serverExecutable(m_serverPath->path());
    if (!serverExecutable.exists()) {
        QMessageBox::warning(this, tr("Server not found"), tr("Can't find the server at the path provided!"));
        return;
    }

    if (!serverExecutable.isExecutable()) {
        QMessageBox::warning(this, tr("Server not runnable"), tr("The server is not an executable file!"));
        return;
    }

    QStringList arguments;
    arguments << "--tick-interval" << QString::number(m_tickInterval->value());
    arguments << "--rounds" << QString::number(m_rounds->value());

    if (m_autoLaunch->isChecked()) {
        arguments << "--start-at" << QString::number(m_botModel->enabledPlayers());
    }
    if (m_autoQuit->isChecked()) {
        arguments << "--quit-on-finish";
    }
    if (m_fullscreen->isChecked()) {
        arguments << "--fullscreen";
    }
    if (m_headless->isChecked()) {
        arguments << "--headless";
    }
    if (m_tickless->isChecked()) {
        arguments << "--tickless";
    }

    m_serverProcess.setWorkingDirectory(serverExecutable.path());
    QFile::remove(m_serverProcess.workingDirectory() + "/scores.txt");
    m_serverProcess.start(serverExecutable.filePath(), arguments);

    QTimer::singleShot(5000, m_botModel, SLOT(launchBots()));
}

void MainWindow::readServerErr()
{
    QByteArray output = m_serverProcess.readAllStandardError();
    errorOutput(output);
}

void MainWindow::readServerOut()
{
    QByteArray output = m_serverProcess.readAllStandardOutput();
    normalOutput(output);
}

void MainWindow::addBot()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select bot"), QStringLiteral("/home/sandsmark/tg/17/ai"));
    if (path.isEmpty()) {
        return;
    }

    m_botModel->addBot(path);
}

void MainWindow::addBotsFromPath(QString dirPath){
    if (dirPath.isNull() || dirPath.isEmpty())
    {
        dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                    QStringLiteral("/home/sandsmark/tg/17/ai"),
                                                        QFileDialog::ShowDirsOnly
                                                        | QFileDialog::DontResolveSymlinks);
    }

    if (dirPath.isEmpty()) {
        return;
    }

    qDebug() << "Checking for bots in" << dirPath;

    QDir dir (dirPath);

    QStringList nameFilter ("entrypoint_*");
    QDirIterator dirIterator (dirPath, QDir::Dirs | QDir::NoDotAndDotDot);

    while (dirIterator.hasNext())
    {
        QDir subdir (dirIterator.next());
        QFileInfoList files = subdir.entryInfoList(nameFilter, QDir::Files);

        QMutableListIterator<QFileInfo> fileIterator(files);
        while  (fileIterator.hasNext()){
            QFileInfo file = fileIterator.next();
            if (file.baseName().startsWith("entrypoint_")){
                qDebug() << "Found bot in " << file.dir();
                m_botModel->addBot(file.absoluteFilePath());
                break;
            }
        }
    }

}

void MainWindow::addBots()
{
    addBotsFromPath(QString::null);
}

void MainWindow::removeBot()
{
    const QModelIndex index = m_botsView->currentIndex();
    if (!index.isValid()) {
        return;
    }
    const int row = index.row();
    QString name = m_botModel->data(m_botModel->index(row, BotModel::Name)).toString();
    if (QMessageBox::question(this, tr("Really remove?"), tr("Are you sure you want to remove %1?").arg(name)) == QMessageBox::No) {
        return;
    }
    m_botModel->removeRow(row);
}

void MainWindow::kill()
{
    m_serverProcess.terminate();
    QThread::usleep(200);
    m_serverProcess.kill();
    m_botModel->killBots();
}

void MainWindow::serverFinished(int status)
{
    m_botModel->killBots();

    m_logFile.close();
    m_logFile.setFileName("BORG.log");
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append);

    qWarning() << m_roundsPlayed << "finished";

    if (status != 0) {
        qWarning() << "Server finished with unclean status" << status;
        m_serverOutput.append(QStringLiteral("Server finished with unclean status %1!\n").arg(status));
    }
    QFile resultsLog(m_serverProcess.workingDirectory() + "/scores.txt");
    if (!resultsLog.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Unable to open server log"), tr("Unable to open server log with results"));
        qWarning() << "unable to open results log!";
        return;
    }

    int playersRead = 0;

    QMap<QString, int> results;
    while (!resultsLog.atEnd()) {
        QList<QByteArray> player = resultsLog.readLine().trimmed().split(';');
        if (player.length() != 3) {
            QMessageBox::warning(this, tr("Invalid scores.txt"), tr("The scores.txt file is corrupt (invalid column), please adjust scores manually."));
            return;
        }

        bool roundWinsValid = false;
        bool scoreValid = false;
        QString name = QString::fromUtf8(player[0]);
        int wins = player[1].toInt(&roundWinsValid);
        int points = player[2].toInt(&scoreValid);

        if (!roundWinsValid || !scoreValid || name.isEmpty()) {
            qWarning() << roundWinsValid << scoreValid << player;
            QMessageBox::warning(this, tr("Invalid scores.txt"), tr("The scores.txt file is corrupt (invalid values), please adjust scores manually."));
            return;
        }

        m_botModel->roundOver(name, (playersRead == 0), wins, points);
        results[name] = wins;
        playersRead++;
    }

    if (playersRead != m_botModel->enabledPlayers()) {
        QMessageBox::warning(this, tr("Missing players"), tr("Missing players (%1 read of %2) from the scores.txt, please adjust manually").arg(playersRead).arg(m_botModel->enabledPlayers()));
    }

    TournamentController::instance()->onMatchCompleted(results);

    m_roundsPlayed++;
    updateTopPlayers();
//    updateName();
    saveSettings();
}

void MainWindow::resetBots()
{
    if (QMessageBox::question(this, tr("Really reset?"), tr("Are you sure you want to reset everything?")) == QMessageBox::No) {
        return;
    }
    m_roundsPlayed = 0;
    saveSettings();
    m_botModel->resetBots();
    TournamentController::instance()->initializeMatches();
    updateTopPlayers();
}

void MainWindow::updateTopPlayers()
{
    QString labelContent("<h3>Top players:</h3><ol>");

    foreach(const QString name, m_botModel->topPlayers()) {
        labelContent += QString("<li>%1</li>").arg(name);
    }

    labelContent += "</ol><br>";
    m_topPlayers.setText(labelContent);
}

void MainWindow::errorOutput(QString message)
{
    message = message.trimmed();
    if (message.isEmpty()) {
        return;
    }
    m_serverOutput.setTextColor(Qt::red);
    m_serverOutput.append(message);
    m_serverOutput.setTextColor(Qt::black);

    if (m_logFile.isOpen()) {
        m_logFile.write(message.toUtf8() + "\n");
    }
}

void MainWindow::normalOutput(QString message)
{
    message = message.trimmed();
    if (message.isEmpty()) {
        return;
    }
    m_serverOutput.append(message);
//    m_serverOutput.setTextColor(Qt::black);

    if (m_logFile.isOpen()) {
        m_logFile.write(message.toUtf8() + "\n");
    }
}
