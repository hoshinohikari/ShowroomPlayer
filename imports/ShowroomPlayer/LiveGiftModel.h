#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QTimer>
#include <QVector>

struct LiveGiftEntry
{
    QString account;
    int giftId = 0;
    int giftCount = 0;
    qint64 timestamp = 0;
};

class LiveGiftModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        AccountRole = Qt::UserRole + 1,
        TextRole,
        GiftIdRole,
        GiftCountRole,
        TimestampRole,
    };

    explicit LiveGiftModel(QObject *parent = nullptr);

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
    bool tryMergeGift(const LiveGiftEntry &entry);
    void trimExcessBeforeInsert(int incomingCount);
    bool parseGiftPayload(const QJsonObject &payload, LiveGiftEntry *entry) const;
    QString giftNameForId(int giftId) const;
    QString giftTextForEntry(const LiveGiftEntry &entry) const;
    void refreshGiftRows();

    QList<LiveGiftEntry> m_entries;
    QVector<LiveGiftEntry> m_pending;
    QHash<int, QString> m_giftNames;
    QTimer *m_flushTimer = nullptr;

    static constexpr int kMaxEntries = 80;
    static constexpr int kMaxPendingPerFlush = 20;
    static constexpr int kFlushIntervalMs = 33;
};
