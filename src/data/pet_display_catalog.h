#pragma once

#include <QHash>
#include <QString>

namespace app {

struct WeightRange {
    double lower = 0; // weight_low + 2% 区间上界（≤ 为小不点）
    double upper = 0; // weight_high - 2% 区间下界（≥ 为大块头）
};

class PetDisplayCatalog {
public:
    static PetDisplayCatalog load(const QString &rootPath = QStringLiteral(":/web"));

    QString medalName(int medalConfId) const;
    QString natureName(int natureId) const;
    QString talentRankName(int talentRank) const;
    QString specialityName(int specialityId) const;
    QString voiceText(int voice) const;
    QString weightText(int baseConfId, int weight) const;

private:
    static QHash<int, QString> loadNameMap(const QString &resourcePath);
    static QHash<int, WeightRange> loadPetBaseWeightRanges(const QString &resourcePath);
    static QHash<int, QString> defaultTalentRankNames();

    QHash<int, QString> m_medals;
    QHash<int, QString> m_natures;
    QHash<int, QString> m_talentRanks;
    QHash<int, QString> m_specialities;
    QHash<int, WeightRange> m_weightRanges;
};

} // namespace app
