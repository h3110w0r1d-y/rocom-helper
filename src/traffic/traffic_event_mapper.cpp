#include "traffic_event_mapper.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <QtMath>

#include <cmath>

namespace app {
namespace {

QString messageBaseName(QString messageName)
{
    if (messageName.startsWith(QLatin1Char('.'))) {
        messageName.remove(0, 1);
    }
    const int index = messageName.lastIndexOf(QLatin1Char('.'));
    return index >= 0 ? messageName.mid(index + 1) : messageName;
}

QJsonValue valueForKeys(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (!value.isUndefined()) {
            return value;
        }
    }
    return {};
}

QJsonObject objectForKeys(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    return valueForKeys(object, keys).toObject();
}

QJsonArray arrayForKeys(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    return valueForKeys(object, keys).toArray();
}

int intValue(const QJsonObject &object, std::initializer_list<const char *> keys, int defaultValue = 0)
{
    const QJsonValue value = valueForKeys(object, keys);
    if (value.isDouble()) {
        return qRound(value.toDouble());
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        return ok ? qRound(number) : defaultValue;
    }
    return defaultValue;
}

bool boolValue(const QJsonObject &object, std::initializer_list<const char *> keys, bool defaultValue = false)
{
    const QJsonValue value = valueForKeys(object, keys);
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isDouble()) {
        return !qFuzzyIsNull(value.toDouble());
    }
    if (value.isString()) {
        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
            return true;
        }
        if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
            return false;
        }
    }
    return defaultValue;
}

QString stringValue(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    const QJsonValue value = valueForKeys(object, keys);
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(qRound64(value.toDouble()));
    }
    return {};
}

double gameRotationToMapRotation(const QJsonObject &rotationObject)
{
    const QJsonValue raw = valueForKeys(rotationObject, {"z"});
    if (raw.isUndefined() || raw.isNull()) {
        return 0.0;
    }
    return std::fmod(raw.toDouble() / 10.0 + 90.0 + 360.0, 360.0);
}

QJsonObject markerPayload(
    const QString &id,
    const QString &markerType,
    const QJsonObject &pos,
    bool temporary = false,
    const QJsonObject &extra = {})
{
    QJsonObject payload{
        {QStringLiteral("id"), id},
        {QStringLiteral("marker_type"), markerType},
        {QStringLiteral("game_x"), intValue(pos, {"x"})},
        {QStringLiteral("game_y"), intValue(pos, {"y"})},
        {QStringLiteral("game_z"), intValue(pos, {"z"})},
        {QStringLiteral("visible"), true},
        {QStringLiteral("temporary"), temporary},
    };
    if (!extra.isEmpty()) {
        payload.insert(QStringLiteral("extra"), extra);
    }
    return payload;
}

QString decodedPetName(const QJsonObject &petData)
{
    const QString rawName = stringValue(petData, {"name"});
    const QByteArray decoded = QByteArray::fromBase64(rawName.toUtf8());
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }
    return rawName;
}

const QSet<int> &boxIds()
{
    static const QSet<int> ids = {
        55519, 55504, 55531, 55532, 55533, 65182, 52021, 52022, 52023, 52024, 52025,
        52026, 52027, 52028, 52029, 52030, 52031, 52032, 52033, 55134, 55135, 55136,
        55137, 55138, 55164, 55165, 55166, 55167, 55168, 55169, 55170, 55171, 55172,
        55173, 55174, 55175, 55176, 55177, 55178, 55179, 55180, 55181, 55182, 55183,
        55184,
    };
    return ids;
}

const QHash<int, QString> &flowerTypes()
{
    static const QHash<int, QString> types = {
        {50042, QStringLiteral("光合球")}, {50327, QStringLiteral("光合球")},
        {65562, QStringLiteral("光合球")}, {50043, QStringLiteral("网兜球")},
        {65565, QStringLiteral("网兜球")}, {50326, QStringLiteral("网兜球")},
        {50820, QStringLiteral("网兜球")}, {50041, QStringLiteral("调温球")},
        {50324, QStringLiteral("调温球")}, {65464, QStringLiteral("调温球")},
        {65477, QStringLiteral("调温球")}, {50044, QStringLiteral("好战球")},
        {65560, QStringLiteral("好战球")}, {65469, QStringLiteral("好战球")},
        {65567, QStringLiteral("好战球")}, {65465, QStringLiteral("美妙球")},
        {50045, QStringLiteral("美妙球")}, {65468, QStringLiteral("美妙球")},
        {65595, QStringLiteral("美妙球")}, {50323, QStringLiteral("淘沙球")},
        {50325, QStringLiteral("淘沙球")}, {65561, QStringLiteral("淘沙球")},
        {65566, QStringLiteral("淘沙球")}, {65466, QStringLiteral("暗星球")},
        {50329, QStringLiteral("暗星球")}, {65473, QStringLiteral("暗星球")},
        {65467, QStringLiteral("绝缘球")}, {65472, QStringLiteral("绝缘球")},
        {65594, QStringLiteral("绝缘球")}, {65590, QStringLiteral("绝缘球")},
        {65470, QStringLiteral("变幻球")}, {65471, QStringLiteral("变幻球")},
        {65564, QStringLiteral("变幻球")}, {50328, QString()}, {50322, QString()},
    };
    return types;
}

bool isOreConfigId(int npcConfigId)
{
    return (50900 <= npcConfigId && npcConfigId <= 50902)
        || (50908 <= npcConfigId && npcConfigId <= 50910)
        || (50916 <= npcConfigId && npcConfigId <= 50918)
        || (50924 <= npcConfigId && npcConfigId <= 50926)
        || (50330 <= npcConfigId && npcConfigId <= 50332);
}

} // namespace

TrafficEventMapper::TrafficEventMapper(QObject *parent)
    : QObject(parent)
{
}

void TrafficEventMapper::mapDecodedAction(const rwtd::DecodedAction &action)
{
    auto emitEvent = [this](EventType type, const QJsonObject &payload, EventFlags flags) {
        AppEvent event;
        event.type = type;
        event.source = EventSource::Traffic;
        event.flags = flags;
        event.name = eventTypeName(type);
        event.payload = payload;
        emit eventCreated(event);
    };

    auto emitUiEvent = [&emitEvent](EventType type, const QJsonObject &payload) {
        emitEvent(type, payload, EventFlag::UpdateUi);
    };
    auto emitBusinessEvent = [&emitEvent](EventType type, const QJsonObject &payload) {
        emitEvent(type, payload, EventFlag::Persist | EventFlag::UpdateUi | EventFlag::PushSse);
    };
    auto emitExternalEvent = [&emitEvent](EventType type, const QJsonObject &payload) {
        emitEvent(type, payload, EventFlag::Persist | EventFlag::PushSse);
    };

    auto processPetDataChanged = [&emitExternalEvent](const QJsonObject &petData) {
        const int petId = intValue(petData, {"gid", "id"});
        if (petId <= 0) {
            return;
        }
        emitExternalEvent(EventType::PetInfoChanged, {
            {QStringLiteral("id"), petId},
            {QStringLiteral("data"), petData},
        });
    };

    auto processGoodsChange = [&processPetDataChanged, &emitExternalEvent](const QJsonArray &changes) {
        for (const QJsonValue &value : changes) {
            const QJsonObject change = value.toObject();
            const int type = intValue(change, {"type", "type_"});
            if (type == 4) {
                const QJsonObject petData = objectForKeys(change, {"pet_data"});
                if (!petData.isEmpty()) {
                    processPetDataChanged(petData);
                }
            } else if (type == 33) {
                const QJsonObject boxChange = objectForKeys(change, {"box_pet_change"});
                if (!boxChange.isEmpty()) {
                    emitExternalEvent(EventType::BoxInfoChanged, {
                        {QStringLiteral("id"), intValue(boxChange, {"id", "id_"}) - 1},
                        {QStringLiteral("pos"), intValue(boxChange, {"pos"}) - 1},
                        {QStringLiteral("value"), intValue(boxChange, {"pet_gid"})},
                    });
                }
            }
        }
    };

    auto processGoodsReward = [&processPetDataChanged, &emitBusinessEvent](const QJsonArray &rewards) {
        for (const QJsonValue &value : rewards) {
            const QJsonObject reward = value.toObject();
            if (intValue(reward, {"type", "type_"}) != 4) {
                continue;
            }
            const QJsonObject petData = objectForKeys(reward, {"pet_data"});
            if (petData.isEmpty()) {
                continue;
            }
            processPetDataChanged(petData);
            emitBusinessEvent(EventType::CatchRecordAdded, {
                {QStringLiteral("name"), decodedPetName(petData)},
                {QStringLiteral("nature"), intValue(petData, {"nature"})},
                {QStringLiteral("talent_rank"), intValue(petData, {"talent_rank"}, 1)},
                {QStringLiteral("caught_at"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))},
            });
        }
    };

    auto processRetInfo = [&processGoodsChange, &processGoodsReward](const QJsonObject &retInfo) {
        const QJsonObject goodsChangeInfo = objectForKeys(retInfo, {"goods_change_info"});
        processGoodsChange(arrayForKeys(goodsChangeInfo, {"changes"}));

        const QJsonObject goodsReward = objectForKeys(retInfo, {"goods_reward"});
        processGoodsReward(arrayForKeys(goodsReward, {"rewards"}));
    };

    auto processActorsEnter = [&emitBusinessEvent](const QJsonArray &actors) {
        for (const QJsonValue &value : actors) {
            const QJsonObject actor = value.toObject();
            if (intValue(actor, {"actor_detail_type"}) == 1) {
                continue;
            }
            const QJsonObject npc = objectForKeys(actor, {"npc"});
            const QJsonObject npcBase = objectForKeys(npc, {"npc_base"});
            if (npc.isEmpty() || npcBase.isEmpty()) {
                continue;
            }

            const QJsonObject base = objectForKeys(npc, {"base"});
            const QJsonObject pt = objectForKeys(base, {"pt"});
            const QJsonObject pos = objectForKeys(pt, {"pos"});
            QString refreshPoint = stringValue(npcBase, {"refresh_point"});
            if (refreshPoint.endsWith(QStringLiteral("01"))) {
                refreshPoint.chop(2);
            }
            if (refreshPoint.isEmpty()) {
                refreshPoint = stringValue(base, {"actor_id"});
            }

            const int npcConfigId = intValue(npcBase, {"npc_cfg_id"});
            const int actorDetailType = intValue(actor, {"actor_detail_type"});
            const QJsonObject glassInfo = objectForKeys(npcBase, {"glass_info"});
            if (actorDetailType == 6 && intValue(glassInfo, {"glass_type"}) != 0) {
                emitBusinessEvent(EventType::MapMarkerAdded, markerPayload(refreshPoint, QStringLiteral("glass"), pos));
            }

            if (isOreConfigId(npcConfigId)) {
                emitBusinessEvent(EventType::MapMarkerAdded, markerPayload(refreshPoint, QStringLiteral("ore"), pos));
            } else if (boxIds().contains(npcConfigId)) {
                emitBusinessEvent(EventType::MapMarkerAdded, markerPayload(refreshPoint, QStringLiteral("chest"), pos, true));
            } else if (flowerTypes().contains(npcConfigId)) {
                QJsonObject extra{
                    {QStringLiteral("actor_id"), stringValue(base, {"actor_id"})},
                    {QStringLiteral("type"), flowerTypes().value(npcConfigId)},
                };
                emitBusinessEvent(EventType::MapMarkerAdded, markerPayload(refreshPoint, QStringLiteral("plant"), pos, false, extra));
            }
        }
    };

    auto processActs = [&emitBusinessEvent, &processActorsEnter](const QJsonArray &acts) {
        for (const QJsonValue &value : acts) {
            const QJsonObject act = value.toObject();
            const QJsonObject actorEnter = objectForKeys(act, {"actor_enter"});
            processActorsEnter(arrayForKeys(actorEnter, {"actors"}));

            const QJsonObject actorDieBegin = objectForKeys(act, {"actor_die_begin"});
            const QString diedActorId = stringValue(actorDieBegin, {"actor_id"});
            if (!diedActorId.isEmpty()) {
                emitBusinessEvent(EventType::MapMarkerVisibilityChanged, {
                    {QStringLiteral("id"), diedActorId},
                    {QStringLiteral("visible"), false},
                });
            }

            const QJsonObject endDropItem = objectForKeys(act, {"end_drop_item"});
            const QString npcRefConfigId = stringValue(endDropItem, {"src_npc_ref_cfg_id"});
            if (!npcRefConfigId.isEmpty()) {
                emitBusinessEvent(EventType::MapMarkerVisibilityChanged, {
                    {QStringLiteral("id"), npcRefConfigId},
                    {QStringLiteral("visible"), false},
                });
            }

            const QJsonObject serverMove = objectForKeys(act, {"server_move"});
            const QJsonArray toPosList = arrayForKeys(serverMove, {"to_pos_list"});
            if (!toPosList.isEmpty()) {
                const QJsonObject targetPos = toPosList.last().toObject();
                emitBusinessEvent(EventType::MapMarkerMoved, {
                    {QStringLiteral("id"), stringValue(serverMove, {"actor_id"})},
                    {QStringLiteral("game_x"), intValue(targetPos, {"x"})},
                    {QStringLiteral("game_y"), intValue(targetPos, {"y"})},
                    {QStringLiteral("game_z"), intValue(targetPos, {"z"})},
                });
            }
        }
    };

    const QString messageName = messageBaseName(action.messageName);
    const QJsonObject payload = action.payload;

    if (messageName == QStringLiteral("ZoneSceneMoveReq")) {
        const QJsonObject toPos = objectForKeys(payload, {"to_pos"});
        if (!toPos.isEmpty()) {
            emitUiEvent(EventType::PlayerPositionChanged, {
                {QStringLiteral("game_x"), intValue(toPos, {"x"})},
                {QStringLiteral("game_y"), intValue(toPos, {"y"})},
                {QStringLiteral("game_z"), intValue(toPos, {"z"})},
                {QStringLiteral("rotation"), gameRotationToMapRotation(objectForKeys(payload, {"to_rot"}))},
                {QStringLiteral("visible"), true},
            });
        }
        return;
    }

    if (messageName == QStringLiteral("ZoneSceneClientEnterSceneFinishNtyAck")) {
        processActorsEnter(arrayForKeys(payload, {"other_actors"}));
        return;
    }
    if (messageName == QStringLiteral("ZoneScenePlayActsBatchNotify")) {
        for (const QJsonValue &batchValue : arrayForKeys(payload, {"acts"})) {
            processActs(arrayForKeys(batchValue.toObject(), {"acts"}));
        }
        return;
    }
    if (messageName == QStringLiteral("ZoneScenePlayActsNotify")) {
        processActs(arrayForKeys(payload, {"acts"}));
        return;
    }

    if (messageName == QStringLiteral("ZonePetBoxTidyRsp")) {
        QJsonArray boxes;
        const QJsonArray boxInfo = arrayForKeys(payload, {"box_info"});
        for (qsizetype i = 0; i < boxInfo.size(); ++i) {
            boxes.append(QJsonObject{
                {QStringLiteral("id"), static_cast<int>(i)},
                {QStringLiteral("data"), arrayForKeys(boxInfo.at(i).toObject(), {"pet_gid"})},
            });
        }
        emitExternalEvent(EventType::BoxInfoReload, {{QStringLiteral("boxes"), boxes}});
        return;
    }
    if (messageName == QStringLiteral("ZoneLoginRsp")) {
        const QJsonObject playerInfo = objectForKeys(payload, {"player_info"});
        const QJsonObject petInfo = objectForKeys(playerInfo, {"pet_info"});
        const QJsonObject backpackInfo = objectForKeys(petInfo, {"backpack_info"});
        QJsonArray boxes;
        const QJsonArray boxInfo = arrayForKeys(backpackInfo, {"boxes"});
        for (qsizetype i = 0; i < boxInfo.size(); ++i) {
            boxes.append(QJsonObject{
                {QStringLiteral("id"), static_cast<int>(i)},
                {QStringLiteral("data"), arrayForKeys(boxInfo.at(i).toObject(), {"pet_gid"})},
            });
        }
        emitExternalEvent(EventType::BoxInfoReload, {{QStringLiteral("boxes"), boxes}});
        return;
    }

    if (messageName == QStringLiteral("ZonePetBoxChangePetRsp")
        || messageName == QStringLiteral("ZoneSceneThrowCatchFinishRsp")
        || messageName == QStringLiteral("ZoneSceneEndThrowRsp")) {
        processRetInfo(objectForKeys(payload, {"ret_info"}));
        return;
    }
    if (messageName == QStringLiteral("ZoneUseMultiBagItemRsp")) {
        const QJsonObject retInfo = objectForKeys(payload, {"ret_info"});
        const QJsonObject goodsChangeInfo = objectForKeys(retInfo, {"goods_change_info"});
        processGoodsChange(arrayForKeys(goodsChangeInfo, {"changes"}));
        return;
    }
    if (messageName == QStringLiteral("ZoneGetPetInfoByPageRsp")) {
        const QJsonObject petInfo = objectForKeys(payload, {"pet_info"});
        emitExternalEvent(EventType::PetInfoReload, {
            {QStringLiteral("page_no"), intValue(payload, {"req_page"}, 1)},
            {QStringLiteral("total_page"), intValue(payload, {"total_page"}, 1)},
            {QStringLiteral("data"), arrayForKeys(petInfo, {"pet_data"})},
        });
        return;
    }
    if (messageName == QStringLiteral("ZonePetFreeRsp")) {
        QJsonArray ids;
        const QJsonValue petGid = valueForKeys(payload, {"pet_gid"});
        if (petGid.isArray()) {
            ids = petGid.toArray();
        } else if (!petGid.isUndefined()) {
            ids.append(petGid);
        }
        emitExternalEvent(EventType::PetInfoDeleted, {{QStringLiteral("ids"), ids}});
        return;
    }

    if (messageName == QStringLiteral("ZoneBattlePerformStartNotify")) {
        const QJsonObject performCmd = objectForKeys(payload, {"perform_cmd"});
        const QJsonArray performInfo = arrayForKeys(performCmd, {"perform_info"});
        for (const QJsonValue &value : performInfo) {
            const QJsonObject perform = value.toObject();
            if (intValue(perform, {"type", "type_"}) != 51) {
                continue;
            }
            const QJsonObject shieldBreak = objectForKeys(perform, {"box_shield_break"});
            if (!boolValue(shieldBreak, {"is_shiny"})) {
                continue;
            }
            const int attrType = intValue(shieldBreak, {"pet_attr_type"});
            const QString attrName = QHash<int, QString>{
                {1, QStringLiteral("生命")},
                {2, QStringLiteral("物攻")},
                {3, QStringLiteral("魔攻")},
                {4, QStringLiteral("物防")},
                {5, QStringLiteral("魔防")},
                {6, QStringLiteral("速度")},
            }.value(attrType, QStringLiteral("unknow"));
            emitBusinessEvent(EventType::ShinyPetDetected, {
                {QStringLiteral("title"), QStringLiteral("异色提示")},
                {QStringLiteral("message"), QStringLiteral("发现异色！！ +%1").arg(attrName)},
                {QStringLiteral("base_conf_id"), intValue(shieldBreak, {"base_conf_id"})},
                {QStringLiteral("attr_name"), attrName},
                {QStringLiteral("pet_rarity_type"), intValue(shieldBreak, {"pet_rarity_type"})},
                {QStringLiteral("pet_mutation_type"), intValue(shieldBreak, {"pet_mutation_type"})},
            });
        }
    }
}

} // namespace app
