#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QString>

struct LiveChatEntry
{
    QString kind;
    QString account;
    QString text;
    int typeCode = 0;
    qint64 timestamp = 0;
    int giftId = 0;
    int giftCount = 0;
};

class LiveChatModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        KindRole = Qt::UserRole + 1,
        AccountRole,
        TextRole,
        TypeCodeRole,
        TimestampRole,
    };

    explicit LiveChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void ingestPayload(const QJsonObject &payload);
    void ingestGiftList(const QJsonObject &giftList);
    void clear();

signals:
    void messageAppended();

private:
    void appendEntry(const LiveChatEntry &entry);
    bool parsePayload(const QJsonObject &payload, LiveChatEntry *entry) const;
    QString giftNameForId(int giftId) const;
    QString giftTextForEntry(const LiveChatEntry &entry) const;

    QList<LiveChatEntry> m_entries;
    QHash<int, QString> m_giftNames;
    static constexpr int kMaxEntries = 400;
};
