#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>

class GiftContributorModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        RankRole = Qt::UserRole + 1,
        UserIdRole,
        AccountRole,
        TotalPtRole,
    };

    explicit GiftContributorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addPoints(qint64 userId, const QString &displayName, int points);
    int rankForUserId(qint64 userId) const;
    int totalPtForUserId(qint64 userId) const;
    void clear();

signals:
    void rankingChanged();

private:
    void rebuildRanking();
    QString displayNameFor(qint64 userId) const;

    struct ContributorRow
    {
        qint64 userId = 0;
        QString account;
        int totalPt = 0;
    };

    QHash<qint64, int> m_totals;
    QHash<qint64, QString> m_displayNames;
    QList<ContributorRow> m_rows;
};
