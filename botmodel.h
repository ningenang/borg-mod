#ifndef BOTMODEL_H
#define BOTMODEL_H

#include <QObject>
#include <QAbstractTableModel>
#include <QList>
#include <QProcess>
#include <QHash>
#include <QFile>
#include <QSharedPointer>

struct Bot {
    bool enabled;
    QString name;
    QString runtime;
    QString path;
    QString arguments;
    int wins;
    int roundWins;
    int totalScore;
    int roundsPlayed;
    QSharedPointer<QProcess> process;
    QSharedPointer<QFile> logfile;
};

QDataStream &operator<<(QDataStream &out, const Bot &bot);
QDataStream &operator>>(QDataStream &in, Bot &bot);
Q_DECLARE_METATYPE(Bot)

class BotModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Enabled = 0,
        Name,
        Runtime,
        Path,
        Arguments,
        Wins,
        RoundWins,
        TotalScore,
        RoundsPlayed,
        Running
    };

    static BotModel *instance() { static BotModel me; return &me; }

    ~BotModel();

    int rowCount(const QModelIndex & = QModelIndex()) const override { return m_bots.length(); }
    int columnCount(const QModelIndex &) const override { return 10; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void removeRow(int row);
    void addBot(QString path);

    void save();

    static QHash<QString, QString> runtimes();

    void roundOver(QString name, bool isWinner, int roundWins, int score);

    int enabledPlayers();

    QStringList topPlayers();

    void resetBots();

    QStringList botNames() const;
    int botWins(const QString &name) const;
    QString botName(int row) const { if (row >= rowCount()) return QString::null; return m_bots[row].name; }
    bool botIsValid(const QString botName);

public slots:
    void launchBots();
    void killBots();
    void removeBots();

    void handleProcessError(QProcess::ProcessError error);

private slots:
    void storeOutput();
    void botStateChanged();

private:
    BotModel();
    void initializeBotProcess(Bot *bot);

    QList<Bot> m_bots;
};

#endif // BOTMODEL_H
