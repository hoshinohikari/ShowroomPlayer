#pragma once

#include <QString>

class ShowroomSessionStore
{
public:
    static QString loadSrId();
    static bool saveSrId(const QString &srId);
    static bool clear();
};
