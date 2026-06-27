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
        AccountRole,
        TotalPtRole,
    };

    explicit GiftContributorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addPoints(const QString &account, int points);
    int rankForAccount(const QString &account) const;
    int totalPtForAccount(const QString &account) const;
    void clear();

signals:
    void rankingChanged();

private:
    void rebuildRanking();

    struct ContributorRow
    {
        QString account;
        int totalPt = 0;
    };

    QHash<QString, int> m_totals;
    QList<ContributorRow> m_rows;
};
