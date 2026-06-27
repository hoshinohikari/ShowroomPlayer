#pragma once

#include "GiftContributorModel.h"
#include "ShowroomGiftPt.h"

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

struct LiveGiftEntry
{
    QString account;
    int giftId = 0;
    int giftCount = 0;
    int ptEarned = 0;
    qint64 userId = 0;
    qint64 timestamp = 0;
};

class LiveGiftModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(GiftContributorModel *contributors READ contributors CONSTANT)
    Q_PROPERTY(bool eventActive READ eventActive NOTIFY eventActiveChanged)

public:
    enum Roles {
        AccountRole = Qt::UserRole + 1,
        TextRole,
        GiftIdRole,
        GiftCountRole,
        PtRole,
        RankRole,
        TotalPtRole,
        TimestampRole,
    };

    explicit LiveGiftModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    GiftContributorModel *contributors() { return &m_contributors; }
    bool eventActive() const { return m_eventActive; }

public slots:
    void ingestPayload(const QJsonObject &payload);
    void ingestGiftList(const QJsonObject &giftList);
    void ingestGiftLog(const QJsonObject &giftLogRoot);
    void setEventActive(bool active);
    void clearMessages();
    void clearGiftCatalog();

signals:
    void messagesFlushed();
    void eventActiveChanged();

private slots:
    void flushPending();
    void onRankingChanged();

private:
    void scheduleFlush();
    bool tryMergeGift(LiveGiftEntry *entry);
    void trimExcessBeforeInsert(int incomingCount);
    bool parseGiftPayload(const QJsonObject &payload, LiveGiftEntry *entry) const;
    int recordGiftContribution(LiveGiftEntry *entry);
    int applyContribution(const QString &account, int giftId, int count, qint64 timestamp,
                        qint64 userId = 0);
    QStringList makeGiftDedupKeys(const QString &account, qint64 userId, int giftId,
                                  qint64 timestamp, int count) const;
    bool isDuplicateGift(const QStringList &dedupKeys) const;
    void markGiftProcessed(const QStringList &dedupKeys);
    GiftCatalogEntry catalogForGift(int giftId) const;
    QString giftNameForId(int giftId) const;
    QString giftTextForEntry(const LiveGiftEntry &entry) const;
    void refreshGiftRows();

    QList<LiveGiftEntry> m_entries;
    QVector<LiveGiftEntry> m_pending;
    QHash<int, GiftCatalogEntry> m_giftCatalog;
    QSet<QString> m_processedGiftKeys;
    GiftContributorModel m_contributors;
    QTimer *m_flushTimer = nullptr;
    bool m_eventActive = false;

    static constexpr int kMaxEntries = 80;
    static constexpr int kMaxPendingPerFlush = 20;
    static constexpr int kFlushIntervalMs = 33;
};
