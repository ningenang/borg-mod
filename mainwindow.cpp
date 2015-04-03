#include "mainwindow.h"
#include "botviewdelegate.h"
#include <QHeaderView>
#include <QItemDelegate>
#include <QItemEditorFactory>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QApplication>
#include <QGroupBox>
#include <QSettings>
#include <QSpinBox>
#include <QPushButton>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QDebug>
#include <QFileDialog>
#include <QTimer>
#include <QLabel>
#include <QThread>
#include <QTime>

#define SERVERPATH_KEY "serverpath"
#define PLAYERS_KEY    "players"
#define ROUNDS_KEY     "rounds"
#define MAPPATH_KEY    "mappath"

static MainWindow *instance = 0;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();

    if (instance) {
        switch (type) {
        case QtDebugMsg:
            instance->normalOutput(msg);
            break;
        case QtWarningMsg:
        case QtCriticalMsg:
            instance->errorOutput(msg);
            break;
        case QtFatalMsg:
            fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
            abort();
        }
    }
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        abort();
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QSplitter(parent),
      m_botsView(new QTableView(this)),
      m_botModel(new BotModel(this)),
      m_serverPath(new PathEditor),
      m_rounds(new QSpinBox),
      m_mapPath(new PathEditor),
      m_launchButton(new QPushButton(tr("&Launch server"))),
      m_logFile("BORG.log")
{
    m_logFile.open(QIODevice::ReadOnly | QIODevice::Append);

    instance = this;
    qInstallMessageHandler(messageHandler);

    QWidget *leftWidget = new QWidget;
    QLayout *leftLayout = new QVBoxLayout;
    leftWidget->setLayout(leftLayout);
    addWidget(leftWidget);

    m_serverOutput.setReadOnly(true);
    addWidget(&m_serverOutput);

    QFile nameFile("names.txt");
    if (nameFile.open(QIODevice::ReadOnly)) {
        m_names = nameFile.readAll().trimmed().split('\n');
        qsrand(QTime::currentTime().msec());
        QFont f( "Arial", 10, QFont::Bold);
        m_name.setFont(f);
    } else {
        qDebug() << "Unable to open map file" << nameFile.errorString();
    }
    m_name.setText("...");
    updateName();
    leftLayout->addWidget(&m_name);

    ///////////
    /// Bot list view
    ///
    m_botsView->setItemDelegate(new BotViewDelegate);
    m_botsView->setModel(m_botModel);
    m_botsView->resizeColumnsToContents();
    m_botsView->setShowGrid(false);
    m_botsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_botsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    leftLayout->addWidget(m_botsView);

    ///////////
    /// Add/remove buttons
    ///
    QWidget *addRemoveGroup = new QWidget;
    addRemoveGroup->setLayout(new QHBoxLayout);
    QPushButton *addButton = new QPushButton(tr("Add"));
    connect(addButton, SIGNAL(clicked()), SLOT(addBot()));
    addRemoveGroup->layout()->addWidget(addButton);
    QPushButton *removeButton = new QPushButton(tr("Remove"));
    connect(removeButton, SIGNAL(clicked()), SLOT(removeBot()));
    addRemoveGroup->layout()->addWidget(removeButton);
    leftLayout->addWidget(addRemoveGroup);

    ///////////
    /// Server control
    ///
    QGroupBox *serverBox = new QGroupBox(tr("Server"));
    serverBox->setLayout(new QHBoxLayout);
    serverBox->layout()->addWidget(m_serverPath);
    serverBox->layout()->addWidget(new QLabel(tr("Rounds:")));
    m_rounds->setMinimum(1);
    m_rounds->setMaximum(10);
    serverBox->layout()->addWidget(m_rounds);
    serverBox->layout()->addWidget(m_mapPath);
    serverBox->layout()->addWidget(m_launchButton);
    leftLayout->addWidget(serverBox);


    leftLayout->addItem(new QSpacerItem(0, 50));

    ///////////
    /// Kill button
    ///
    QPushButton *killButton = new QPushButton(tr("&Kill"));
    connect(killButton, SIGNAL(clicked()), SLOT(kill()));
    leftLayout->addWidget(killButton);

    ///////////
    /// Quit button
    ///
    QPushButton *quitButton = new QPushButton(tr("&Quit"));
    connect(quitButton, SIGNAL(clicked()), qApp, SLOT(quit()));
    leftLayout->addWidget(quitButton);

    QSettings settings;
    m_serverPath->setPath(settings.value(SERVERPATH_KEY, "").toString());
    m_rounds->setValue(settings.value(ROUNDS_KEY, 4).toInt());
    m_mapPath->setPath(settings.value(MAPPATH_KEY, "map1.map").toString());

    connect(m_serverPath, SIGNAL(pathChanged(QString)), SLOT(saveSettings()));
    connect(m_mapPath, SIGNAL(pathChanged(QString)), SLOT(saveSettings()));
    connect(m_rounds, SIGNAL(valueChanged(int)), SLOT(saveSettings()));
    connect(m_launchButton, SIGNAL(clicked()), SLOT(launchServer()));
    connect(&m_serverProcess, SIGNAL(readyReadStandardError()), SLOT(readServerErr()));
    connect(&m_serverProcess, SIGNAL(readyReadStandardOutput()), SLOT(readServerOut()));
    connect(&m_serverProcess, SIGNAL(finished(int)), SLOT(serverFinished(int)));
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
    settings.setValue(MAPPATH_KEY, m_mapPath->path());
}

void MainWindow::launchServer()
{
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
    arguments << "server"
              << QString::number(m_botModel->enabledPlayers())
              << QString::number(m_rounds->value());

    QFileInfo mapFile(m_mapPath->path());
    if (mapFile.exists()) {
        arguments.append(mapFile.filePath());
    }

    m_serverProcess.setWorkingDirectory(serverExecutable.path());
    m_serverProcess.start(serverExecutable.filePath(), arguments);

    QTimer::singleShot(1000, m_botModel, SLOT(launchBots()));
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
    QString path = QFileDialog::getOpenFileName(this, tr("Select bot"));
    m_botModel->addBot(path);
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

    qWarning() << m_name.text() << "finished";

    if (status != 0) {
        qWarning() << "Server finished with unclean status" << status;
        m_serverOutput.append(QStringLiteral("Server finished with unclean status %1!\n").arg(status));
    }
    QFile resultsLog(m_serverProcess.workingDirectory() + "/scores.log");
    if (!resultsLog.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Unable to open server log"), tr("Unable to open server log with results"));
        qWarning() << "unable to open results log!";
        return;
    }
    QByteArray winner = resultsLog.readLine().trimmed();
    if (winner.isEmpty()) {
        qWarning() << "NO WINNER FOUND, race aborted?";
    } else {
        m_botModel->roundOver(QString::fromUtf8(winner));
    }

    resultsLog.close();
    resultsLog.remove();
}

void MainWindow::updateName()
{
    if (m_names.isEmpty()) {
        return;
    }
    m_name.setText("Round: " + m_names[qrand() % m_names.size()]);
}

void MainWindow::errorOutput(QString message)
{
    QColor oldColor = m_serverOutput.textColor();
    m_serverOutput.setTextColor(Qt::red);
    m_serverOutput.append(message);
    m_serverOutput.setTextColor(oldColor);

    if (m_logFile.isOpen()) {
        m_logFile.write(message.toUtf8());
    }
}

void MainWindow::normalOutput(QString message)
{
    m_serverOutput.append(message);

    if (m_logFile.isOpen()) {
        m_logFile.write(message.toUtf8());
    }
}
