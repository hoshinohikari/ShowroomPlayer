#include "ShowroomGiftPt.h"

#include <QtMath>

namespace ShowroomGiftPt {

double comboMultiplier(int count, int pointPerGift)
{
    if (pointPerGift >= 500)
        return 1.2;
    if (count <= 1)
        return 1.0;
    if (count >= 10)
        return 1.2;
    return 1.0 + (0.02 * count);
}

bool isRainbowStar(const QString &giftName)
{
    return giftName.contains(QStringLiteral("レインボー"));
}

bool qualifiesForEventBonus(bool eventActive, const GiftCatalogEntry &gift)
{
    if (!eventActive)
        return false;
    return !gift.free || isRainbowStar(gift.name);
}

int calculateGiftPt(const GiftCatalogEntry &gift, int count, bool eventActive)
{
    if (count < 1)
        return 0;

    const int pointPerGift = gift.point > 0 ? gift.point : 1;
    const double base = static_cast<double>(pointPerGift * count);
    const double combo = comboMultiplier(count, pointPerGift);
    const double eventMult = qualifiesForEventBonus(eventActive, gift) ? 2.5 : 1.0;
    return static_cast<int>(qFloor(base * eventMult * combo));
}

} // namespace ShowroomGiftPt
