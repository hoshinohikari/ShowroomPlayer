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
    case UserIdRole:
        return row.userId;
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
        {UserIdRole, "userId"},
        {AccountRole, "account"},
        {TotalPtRole, "totalPt"},
    };
}

QString GiftContributorModel::displayNameFor(qint64 userId) const
{
    const QString name = m_displayNames.value(userId).trimmed();
    if (!name.isEmpty())
        return name;
    return QStringLiteral("#%1").arg(userId);
}

void GiftContributorModel::addPoints(qint64 userId, const QString &displayName, int points)
{
    if (userId <= 0 || points <= 0)
        return;

    const QString trimmedName = displayName.trimmed();
    if (!trimmedName.isEmpty())
        m_displayNames.insert(userId, trimmedName);

    m_totals[userId] += points;
    rebuildRanking();
}

int GiftContributorModel::rankForUserId(qint64 userId) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).userId == userId)
            return row + 1;
    }
    return 0;
}

int GiftContributorModel::totalPtForUserId(qint64 userId) const
{
    return m_totals.value(userId, 0);
}

void GiftContributorModel::rebuildRanking()
{
    QList<ContributorRow> nextRows;
    nextRows.reserve(m_totals.size());
    for (auto it = m_totals.constBegin(); it != m_totals.constEnd(); ++it) {
        ContributorRow row;
        row.userId = it.key();
        row.account = displayNameFor(row.userId);
        row.totalPt = it.value();
        nextRows.append(row);
    }

    std::sort(nextRows.begin(), nextRows.end(),
              [](const ContributorRow &left, const ContributorRow &right) {
                  if (left.totalPt != right.totalPt)
                      return left.totalPt > right.totalPt;
                  return left.userId < right.userId;
              });

    beginResetModel();
    m_rows = std::move(nextRows);
    endResetModel();
    emit rankingChanged();
}

void GiftContributorModel::clear()
{
    if (m_totals.isEmpty() && m_rows.isEmpty() && m_displayNames.isEmpty())
        return;

    beginResetModel();
    m_totals.clear();
    m_displayNames.clear();
    m_rows.clear();
    endResetModel();
    emit rankingChanged();
}
