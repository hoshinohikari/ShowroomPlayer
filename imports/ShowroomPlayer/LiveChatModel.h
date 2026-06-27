#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QTimer>
#include <QVector>

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
    void clearMessages();
    void clearGiftNames();

signals:
    void messagesFlushed();

private slots:
    void flushPending();

private:
    void scheduleFlush();
    bool tryMergeGift(const LiveChatEntry &entry);
    void trimExcessBeforeInsert(int incomingCount);
    bool parsePayload(const QJsonObject &payload, LiveChatEntry *entry) const;
    QString giftNameForId(int giftId) const;
    QString giftTextForEntry(const LiveChatEntry &entry) const;
    void refreshGiftRows();

    QList<LiveChatEntry> m_entries;
    QVector<LiveChatEntry> m_pending;
    QHash<int, QString> m_giftNames;
    QTimer *m_flushTimer = nullptr;

    static constexpr int kMaxEntries = 300;
    static constexpr int kMaxPendingPerFlush = 40;
    static constexpr int kFlushIntervalMs = 33;
};
