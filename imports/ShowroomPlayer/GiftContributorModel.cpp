#include "GiftContributorModel.h"

#include <algorithm>

GiftContributorModel::GiftContributorModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GiftContributorModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant GiftContributorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const ContributorRow &row = m_rows.at(index.row());
    switch (role) {
    case RankRole:
        return index.row() + 1;
    case AccountRole:
        return row.account;
    case TotalPtRole:
        return row.totalPt;
    default:
        return {};
    }
}

QHash<int, QByteArray> GiftContributorModel::roleNames() const
{
    return {
        {RankRole, "rank"},
        {AccountRole, "account"},
        {TotalPtRole, "totalPt"},
    };
}

void GiftContributorModel::addPoints(const QString &account, int points)
{
    if (account.isEmpty() || points <= 0)
        return;

    m_totals[account] += points;
    rebuildRanking();
}

int GiftContributorModel::rankForAccount(const QString &account) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).account == account)
            return row + 1;
    }
    return 0;
}

int GiftContributorModel::totalPtForAccount(const QString &account) const
{
    return m_totals.value(account, 0);
}

void GiftContributorModel::rebuildRanking()
{
    QList<ContributorRow> nextRows;
    nextRows.reserve(m_totals.size());
    for (auto it = m_totals.constBegin(); it != m_totals.constEnd(); ++it)
        nextRows.append({it.key(), it.value()});

    std::sort(nextRows.begin(), nextRows.end(),
              [](const ContributorRow &left, const ContributorRow &right) {
                  if (left.totalPt != right.totalPt)
                      return left.totalPt > right.totalPt;
                  return left.account < right.account;
              });

    beginResetModel();
    m_rows = std::move(nextRows);
    endResetModel();
    emit rankingChanged();
}

void GiftContributorModel::clear()
{
    if (m_totals.isEmpty() && m_rows.isEmpty())
        return;

    beginResetModel();
    m_totals.clear();
    m_rows.clear();
    endResetModel();
    emit rankingChanged();
}
