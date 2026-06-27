#pragma once

#include <QString>

struct GiftCatalogEntry
{
    QString name;
    int point = 1;
    bool free = true;
    int giftType = 0;
};

namespace ShowroomGiftPt {

double comboMultiplier(int count, int pointPerGift);
bool isRainbowStar(const QString &giftName);
bool qualifiesForEventBonus(bool eventActive, const GiftCatalogEntry &gift);
int calculateGiftPt(const GiftCatalogEntry &gift, int count, bool eventActive);

} // namespace ShowroomGiftPt
