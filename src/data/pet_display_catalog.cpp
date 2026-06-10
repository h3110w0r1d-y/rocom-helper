#include "pet_display_catalog.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace app {
namespace {

QString lookupName(const QHash<int, QString> &names, int id)
{
    const auto it = names.constFind(id);
    if (it != names.constEnd()) {
        return it.value();
    }
    return id > 0 ? QString::number(id) : QStringLiteral("-");
}

} // namespace

PetDisplayCatalog PetDisplayCatalog::load(const QString &rootPath)
{
    PetDisplayCatalog catalog;
    const QString prefix = rootPath.endsWith(QLatin1Char('/')) ? rootPath : rootPath + QLatin1Char('/');
    catalog.m_medals = loadNameMap(prefix + QStringLiteral("MEDAL_CONF.json"));
    catalog.m_natures = loadNameMap(prefix + QStringLiteral("NATURE_CONF.json"));
    catalog.m_specialities = loadNameMap(prefix + QStringLiteral("PET_TALENT_CONF.json"));
    catalog.m_talentRanks = defaultTalentRankNames();
    return catalog;
}

QHash<int, QString> PetDisplayCatalog::defaultTalentRankNames()
{
    return {
        {1, QStringLiteral("一般般")},
        {2, QStringLiteral("还不错")},
        {3, QStringLiteral("相当好")},
        {4, QStringLiteral("了不起")},
    };
}

QHash<int, QString> PetDisplayCatalog::loadNameMap(const QString &resourcePath)
{
    QHash<int, QString> names;
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return names;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return names;
    }

    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        const int id = entry.value(QStringLiteral("id")).toInt(it.key().toInt());
        const QString name = entry.value(QStringLiteral("name")).toString().trimmed();
        if (id > 0 && !name.isEmpty()) {
            names.insert(id, name);
        }
    }
    return names;
}

QString PetDisplayCatalog::medalName(int medalConfId) const
{
    return lookupName(m_medals, medalConfId);
}

QString PetDisplayCatalog::natureName(int natureId) const
{
    return lookupName(m_natures, natureId);
}

QString PetDisplayCatalog::talentRankName(int talentRank) const
{
    return lookupName(m_talentRanks, talentRank);
}

QString PetDisplayCatalog::specialityName(int specialityId) const
{
    return lookupName(m_specialities, specialityId);
}

} // namespace app
