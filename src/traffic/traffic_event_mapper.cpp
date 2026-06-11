#include "traffic_event_mapper.h"

#include "traffic/protobuf_json_util.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QtMath>

#include <cmath>

#include "rwtd/opcode_filter.h"
#include "rwtd/opcode_registry.h"

#include "battle_proto.pb.h"
#include "space_action.pb.h"
#include "zonesvr.pb.h"
#include "xls_enum.pb.h"

namespace app {
namespace {

template<typename Message>
bool parseMessage(const QByteArray &payload, Message &message)
{
    return message.ParseFromArray(payload.constData(), payload.size());
}

template<typename Message, typename Handler>
bool processRetInfoResponse(const QByteArray &payload, Handler &&handler)
{
    Message message;
    if (!parseMessage(payload, message) || !message.has_ret_info()) {
        return false;
    }
    handler(message.ret_info());
    return true;
}

QString formatActorId(quint64 actorId)
{
    return QString::number(actorId);
}

QString formatRefreshPoint(quint32 refreshPoint)
{
    QString refreshPointText = QString::number(refreshPoint);
    if (refreshPointText.endsWith(QStringLiteral("01"))) {
        refreshPointText.chop(2);
    }
    return refreshPointText;
}

double gameRotationToMapRotation(const Next::Position &rotation)
{
    if (!rotation.has_z()) {
        return 0.0;
    }
    return std::fmod(static_cast<double>(rotation.z()) / 10.0 + 90.0 + 360.0, 360.0);
}

QJsonObject markerPayload(
    const QString &id,
    const QString &markerType,
    const Next::Position &pos,
    bool temporary = false,
    const QJsonObject &extra = {})
{
    QJsonObject payload{
        {QStringLiteral("id"), id},
        {QStringLiteral("marker_type"), markerType},
        {QStringLiteral("game_x"), pos.has_x() ? pos.x() : 0},
        {QStringLiteral("game_y"), pos.has_y() ? pos.y() : 0},
        {QStringLiteral("game_z"), pos.has_z() ? pos.z() : 0},
        {QStringLiteral("visible"), true},
        {QStringLiteral("temporary"), temporary},
    };
    if (!extra.isEmpty()) {
        payload.insert(QStringLiteral("extra"), extra);
    }
    return payload;
}

QString decodedPetName(const Next::PetData &petData)
{
    if (!petData.has_name()) {
        return {};
    }
    const QString rawName = QString::fromStdString(petData.name());
    const QByteArray decoded = QByteArray::fromBase64(rawName.toUtf8());
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }
    return rawName;
}

CatchRecord catchRecordFromPetData(const Next::PetData &petData)
{
    CatchRecord record;
    record.name = decodedPetName(petData);
    record.nature = petData.has_nature() ? static_cast<int>(petData.nature()) : 0;
    record.talentRank = petData.has_talent_rank() ? static_cast<int>(petData.talent_rank()) : 1;
    record.specialityId = petData.has_speciality_id() ? static_cast<int>(petData.speciality_id()) : 0;
    record.wearMedalConfId = petData.has_wear_medal_conf_id() ? static_cast<int>(petData.wear_medal_conf_id()) : 0;
    record.voice = petData.has_voice() ? static_cast<int>(petData.voice()) : 0;
    record.weight = petData.has_weight() ? static_cast<int>(petData.weight()) : 0;
    record.baseConfId = petData.has_base_conf_id() ? static_cast<int>(petData.base_conf_id()) : 0;
    record.caughtAt = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    return record;
}

QJsonArray petGidsToJsonArray(const google::protobuf::RepeatedField<quint32> &petGids)
{
    QJsonArray array;
    for (const quint32 petGid : petGids) {
        array.append(static_cast<int>(petGid));
    }
    return array;
}

QJsonArray boxesFromPetBoxes(const google::protobuf::RepeatedPtrField<Next::PetBox> &boxes)
{
    QJsonArray result;
    for (int i = 0; i < boxes.size(); ++i) {
        result.append(QJsonObject{
            {QStringLiteral("id"), i},
            {QStringLiteral("data"), petGidsToJsonArray(boxes.Get(i).pet_gid())},
        });
    }
    return result;
}

const Next::Position &actorPosition(const Next::ActorInfo &actor)
{
    static const Next::Position empty;
    if (!actor.has_npc() || !actor.npc().has_base() || !actor.npc().base().has_pt()
        || !actor.npc().base().pt().has_pos()) {
        return empty;
    }
    return actor.npc().base().pt().pos();
}

const QHash<int, QString> &boxTypes()
{
    static const QHash<int, QString> types = {
        {55519, QStringLiteral("普通")}, {55504, QStringLiteral("普通")},
        {55531, QStringLiteral("普通")}, {55164, QStringLiteral("普通")},

        {55532, QStringLiteral("华丽")}, {65182, QStringLiteral("华丽")},
        {52021, QStringLiteral("华丽")}, {52022, QStringLiteral("华丽")},
        {52023, QStringLiteral("华丽")}, {52024, QStringLiteral("华丽")},
        {52025, QStringLiteral("华丽")}, {52026, QStringLiteral("华丽")},
        {52027, QStringLiteral("华丽")}, {52028, QStringLiteral("华丽")},
        {52029, QStringLiteral("华丽")}, {52030, QStringLiteral("华丽")},
        {52031, QStringLiteral("华丽")}, {52032, QStringLiteral("华丽")},
        {52033, QStringLiteral("华丽")}, {55134, QStringLiteral("华丽")},
        {55135, QStringLiteral("华丽")}, {55136, QStringLiteral("华丽")},
        {55137, QStringLiteral("华丽")}, {55138, QStringLiteral("华丽")},
        {55165, QStringLiteral("华丽")}, {55167, QStringLiteral("华丽")},
        {55168, QStringLiteral("华丽")}, {55169, QStringLiteral("华丽")},
        {55170, QStringLiteral("华丽")}, {55171, QStringLiteral("华丽")},
        {55172, QStringLiteral("华丽")}, {55173, QStringLiteral("华丽")},
        {55174, QStringLiteral("华丽")}, {55175, QStringLiteral("华丽")},
        {55176, QStringLiteral("华丽")}, {55177, QStringLiteral("华丽")},
        {55178, QStringLiteral("华丽")}, {55179, QStringLiteral("华丽")},
        {55180, QStringLiteral("华丽")}, {55181, QStringLiteral("华丽")},
        {55182, QStringLiteral("华丽")}, {55183, QStringLiteral("华丽")},
        {55184, QStringLiteral("华丽")},

        {55533, QStringLiteral("贵重")}, {55166, QStringLiteral("贵重")},
    };
    return types;
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

void TrafficEventMapper::setOpcodeFilter(rwtd::OpcodeFilter *filter)
{
    m_opcodeFilter = filter;
}

void TrafficEventMapper::mapDecodedAction(const rwtd::DecodedAction &action)
{
    const QHash<quint32, rwtd::ZoneOpcode> &opcodeLookup = rwtd::usedZoneOpcodeByRaw();
    const auto mappedOpcode = opcodeLookup.constFind(action.opcode);
    if (mappedOpcode == opcodeLookup.constEnd()) {
        return;
    }

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
    auto emitCatchRecordEvent = [this](const CatchRecord &record) {
        if (m_opcodeFilter != nullptr
            && !m_opcodeFilter->isUiProfileEnabled(rwtd::OpcodeProfile::CatchLog)) {
            return;
        }
        emit eventCreated(makeCatchRecordAddedEvent(
            EventSource::Traffic,
            EventFlag::Persist | EventFlag::UpdateUi | EventFlag::PushSse,
            record));
    };
    auto emitExternalEvent = [&emitEvent](EventType type, const QJsonObject &payload) {
        emitEvent(type, payload, EventFlag::Persist | EventFlag::PushSse);
    };

    auto processPetDataChanged = [&emitExternalEvent](const Next::PetData &petData) {
        const int petId = petData.has_gid() ? static_cast<int>(petData.gid()) : 0;
        if (petId <= 0) {
            return;
        }
        emitExternalEvent(EventType::PetInfoChanged, {
            {QStringLiteral("id"), petId},
            {QStringLiteral("data"), protobufToJsonObject(petData)},
        });
    };

    auto processGoodsChange = [&processPetDataChanged, &emitExternalEvent](const Next::GoodsChange &goodsChange) {
        for (const Next::GoodsChangeItem &change : goodsChange.changes()) {
            if (!change.has_type()) {
                continue;
            }
            const auto changeType = change.type();
            if (changeType == dataconfig::GT_PET) {
                if (change.has_pet_data()) {
                    processPetDataChanged(change.pet_data());
                }
            } else if (changeType == dataconfig::GT_PETBOX_PET_CHANGE) {
                if (!change.has_box_pet_change()) {
                    continue;
                }
                const Next::PetBoxPetChange &boxChange = change.box_pet_change();
                emitExternalEvent(EventType::BoxInfoChanged, {
                    {QStringLiteral("id"), boxChange.has_id() ? static_cast<int>(boxChange.id()) - 1 : 0},
                    {QStringLiteral("pos"), boxChange.has_pos() ? static_cast<int>(boxChange.pos()) - 1 : 0},
                    {QStringLiteral("value"), boxChange.has_pet_gid() ? static_cast<int>(boxChange.pet_gid()) : 0},
                });
            }
        }
    };

    auto processGoodsReward = [&processPetDataChanged, &emitCatchRecordEvent](const Next::GoodsReward &goodsReward) {
        for (const Next::GoodsItem &reward : goodsReward.rewards()) {
            if (!reward.has_type() || reward.type() != dataconfig::GT_PET || !reward.has_pet_data()) {
                continue;
            }
            const Next::PetData &petData = reward.pet_data();
            processPetDataChanged(petData);
            emitCatchRecordEvent(catchRecordFromPetData(petData));
        }
    };

    auto processRetInfo = [&processGoodsChange, &processGoodsReward](const Next::RetInfo &retInfo) {
        if (retInfo.has_goods_change_info()) {
            processGoodsChange(retInfo.goods_change_info());
        }
        if (retInfo.has_goods_reward()) {
            processGoodsReward(retInfo.goods_reward());
        }
    };

    auto processActorsEnter = [&emitBusinessEvent](const google::protobuf::RepeatedPtrField<Next::ActorInfo> &actors) {
        for (const Next::ActorInfo &actor : actors) {
            if (actor.has_actor_detail_type() && actor.actor_detail_type() == 1) {
                continue;
            }
            if (!actor.has_npc() || !actor.npc().has_npc_base()) {
                continue;
            }

            const Next::ActorInfo_Npc &npc = actor.npc();
            const Next::ActorInfo_NpcBase &npcBase = npc.npc_base();
            const Next::Position &pos = actorPosition(actor);

            QString refreshPoint;
            if (npcBase.has_refresh_point()) {
                refreshPoint = formatRefreshPoint(npcBase.refresh_point());
            }
            if (refreshPoint.isEmpty() && npc.has_base() && npc.base().has_actor_id()) {
                refreshPoint = formatActorId(npc.base().actor_id());
            }

            const int npcConfigId = npcBase.has_npc_cfg_id() ? npcBase.npc_cfg_id() : 0;
            const int actorDetailType = actor.has_actor_detail_type() ? actor.actor_detail_type() : 0;
            if (actorDetailType == 6 && npcBase.has_glass_info()
                && static_cast<int>(npcBase.glass_info().glass_type()) != 0) {
                emitBusinessEvent(EventType::MapMarkerAdded, markerPayload(refreshPoint, QStringLiteral("glass"), pos));
            }

            if (isOreConfigId(npcConfigId)) {
                emitBusinessEvent(EventType::MapMarkerAdded, markerPayload(refreshPoint, QStringLiteral("ore"), pos));
            } else if (boxTypes().contains(npcConfigId)) {
                emitBusinessEvent(EventType::MapMarkerAdded, markerPayload(
                    refreshPoint,
                    QStringLiteral("chest"),
                    pos,
                    true,
                    {{QStringLiteral("type"), boxTypes().value(npcConfigId)}}));
            } else if (flowerTypes().contains(npcConfigId)) {
                QJsonObject extra{
                    {QStringLiteral("actor_id"), npc.has_base() && npc.base().has_actor_id()
                        ? formatActorId(npc.base().actor_id()) : QString()},
                    {QStringLiteral("type"), flowerTypes().value(npcConfigId)},
                };
                emitBusinessEvent(EventType::MapMarkerAdded, markerPayload(refreshPoint, QStringLiteral("plant"), pos, false, extra));
            }
        }
    };

    auto processActs = [&emitBusinessEvent, &processActorsEnter](
        const google::protobuf::RepeatedPtrField<Next::SpaceActionCollection> &acts) {
        for (const Next::SpaceActionCollection &act : acts) {
            if (act.has_actor_enter()) {
                processActorsEnter(act.actor_enter().actors());
            }

            if (act.has_actor_die_begin() && act.actor_die_begin().has_actor_id()) {
                emitBusinessEvent(EventType::MapMarkerVisibilityChanged, {
                    {QStringLiteral("id"), formatActorId(act.actor_die_begin().actor_id())},
                    {QStringLiteral("visible"), false},
                });
            }

            if (act.has_end_drop_item() && act.end_drop_item().has_src_npc_ref_cfg_id()) {
                emitBusinessEvent(EventType::MapMarkerVisibilityChanged, {
                    {QStringLiteral("id"), QString::number(act.end_drop_item().src_npc_ref_cfg_id())},
                    {QStringLiteral("visible"), false},
                });
            }

            if (act.has_server_move() && act.server_move().to_pos_list_size() > 0) {
                const Next::SpaceAct_ServerMove &serverMove = act.server_move();
                const Next::Position &targetPos = serverMove.to_pos_list(serverMove.to_pos_list_size() - 1);
                emitBusinessEvent(EventType::MapMarkerMoved, {
                    {QStringLiteral("id"), serverMove.has_actor_id() ? formatActorId(serverMove.actor_id()) : QString()},
                    {QStringLiteral("game_x"), targetPos.has_x() ? targetPos.x() : 0},
                    {QStringLiteral("game_y"), targetPos.has_y() ? targetPos.y() : 0},
                    {QStringLiteral("game_z"), targetPos.has_z() ? targetPos.z() : 0},
                });
            }
        }
    };

    const QByteArray &payload = action.payload;

    switch (*mappedOpcode) {
    case rwtd::ZoneOpcode::ZoneSceneMoveReq: {
        Next::ZoneSceneMoveReq message;
        if (!parseMessage(payload, message) || !message.has_to_pos()) {
            return;
        }
        PlayerPositionPayload position;
        position.gameX = message.to_pos().has_x() ? message.to_pos().x() : 0;
        position.gameY = message.to_pos().has_y() ? message.to_pos().y() : 0;
        position.gameZ = message.to_pos().has_z() ? message.to_pos().z() : 0;
        position.rotation = message.has_to_rot() ? gameRotationToMapRotation(message.to_rot()) : 0.0;
        position.visible = true;
        emit eventCreated(makePlayerPositionChangedEvent(EventSource::Traffic, EventFlag::UpdateUi, position));
        return;
    }
    case rwtd::ZoneOpcode::ZoneSceneClientEnterSceneFinishNtyAck: {
        Next::ZoneSceneClientEnterSceneFinishNtyAck message;
        if (!parseMessage(payload, message)) {
            return;
        }
        processActorsEnter(message.other_actors());
        return;
    }
    case rwtd::ZoneOpcode::ZoneScenePlayActsBatchNotify: {
        Next::ZoneScenePlayActsBatchNotify message;
        if (!parseMessage(payload, message)) {
            return;
        }
        for (const Next::ZoneScenePlayActsNotify &batch : message.acts()) {
            processActs(batch.acts());
        }
        return;
    }
    case rwtd::ZoneOpcode::ZoneScenePlayActsNotify: {
        Next::ZoneScenePlayActsNotify message;
        if (!parseMessage(payload, message)) {
            return;
        }
        processActs(message.acts());
        return;
    }
    case rwtd::ZoneOpcode::ZonePetBoxTidyRsp: {
        Next::ZonePetBoxTidyRsp message;
        if (!parseMessage(payload, message)) {
            return;
        }
        emitExternalEvent(EventType::BoxInfoReload, {{QStringLiteral("boxes"), boxesFromPetBoxes(message.box_info())}});
        return;
    }
    case rwtd::ZoneOpcode::ZoneLoginRsp: {
        Next::ZoneLoginRsp message;
        if (!parseMessage(payload, message) || !message.has_player_info() || !message.player_info().has_pet_info()
            || !message.player_info().pet_info().has_backpack_info()) {
            return;
        }
        emitExternalEvent(EventType::BoxInfoReload, {
            {QStringLiteral("boxes"),
             boxesFromPetBoxes(message.player_info().pet_info().backpack_info().boxes())},
        });
        return;
    }
    case rwtd::ZoneOpcode::ZonePetBoxChangePetRsp:
        processRetInfoResponse<Next::ZonePetBoxChangePetRsp>(payload, processRetInfo);
        return;
    case rwtd::ZoneOpcode::ZonePetMedalCommonRsp:
        processRetInfoResponse<Next::ZonePetMedalCommonRsp>(payload, processRetInfo);
        return;
    case rwtd::ZoneOpcode::ZoneSceneThrowCatchFinishRsp:
        processRetInfoResponse<Next::ZoneSceneThrowCatchFinishRsp>(payload, processRetInfo);
        return;
    case rwtd::ZoneOpcode::ZoneSceneEndThrowRsp:
        processRetInfoResponse<Next::ZoneSceneEndThrowRsp>(payload, processRetInfo);
        return;
    case rwtd::ZoneOpcode::ZoneUseMultiBagItemRsp: {
        Next::ZoneUseMultiBagItemRsp message;
        if (!parseMessage(payload, message) || !message.has_ret_info()
            || !message.ret_info().has_goods_change_info()) {
            return;
        }
        processGoodsChange(message.ret_info().goods_change_info());
        return;
    }
    case rwtd::ZoneOpcode::ZoneGetPetInfoByPageRsp: {
        Next::ZoneGetPetInfoByPageRsp message;
        if (!parseMessage(payload, message) || !message.has_pet_info()) {
            return;
        }
        emitExternalEvent(EventType::PetInfoReload, {
            {QStringLiteral("page_no"), message.has_req_page() ? static_cast<int>(message.req_page()) : 1},
            {QStringLiteral("total_page"), message.has_total_page() ? static_cast<int>(message.total_page()) : 1},
            {QStringLiteral("data"), protobufPetDataListToJsonArray(message.pet_info().pet_data())},
        });
        return;
    }
    case rwtd::ZoneOpcode::ZonePetFreeRsp: {
        Next::ZonePetFreeRsp message;
        if (!parseMessage(payload, message)) {
            return;
        }
        const QJsonArray ids = petGidsToJsonArray(message.pet_gid());
        emitExternalEvent(EventType::PetInfoDeleted, {{QStringLiteral("ids"), ids}});
        return;
    }
    case rwtd::ZoneOpcode::ZoneBattleFinishNotify:
    case rwtd::ZoneOpcode::ZoneBattleForceFinishNotify:
        emitUiEvent(EventType::BoxHintUpdated, {{QStringLiteral("clear"), true}});
        return;
    case rwtd::ZoneOpcode::ZoneBattlePerformStartNotify: {
        Next::ZoneBattlePerformStartNotify message;
        if (!parseMessage(payload, message) || !message.has_perform_cmd()) {
            return;
        }

        const Next::BattlePerformCmd &performCmd = message.perform_cmd();
        for (const Next::BattlePerformInfo &perform : performCmd.perform_info()) {
            if (!perform.has_type() || perform.type() != Next::BPT_BOX_SHIELD_BREAK
                || !perform.has_box_shield_break()) {
                continue;
            }

            const Next::BattleBoxShieldBreak &shieldBreak = perform.box_shield_break();
            const int attrType = shieldBreak.has_pet_attr_type()
                ? static_cast<int>(shieldBreak.pet_attr_type()) : 0;
            const QString attrName = QHash<int, QString>{
                {1, QStringLiteral("生命")},
                {2, QStringLiteral("物攻")},
                {3, QStringLiteral("魔攻")},
                {4, QStringLiteral("物防")},
                {5, QStringLiteral("魔防")},
                {6, QStringLiteral("速度")},
            }.value(attrType, QStringLiteral("unknow"));

            const bool isShiny = shieldBreak.has_is_shiny() && shieldBreak.is_shiny();
            const bool isFantastic = shieldBreak.has_is_fantastic() && shieldBreak.is_fantastic();
            const bool isNightmare = shieldBreak.has_is_nightmare() && shieldBreak.is_nightmare();
            const int petRarityType = shieldBreak.has_pet_rarity_type() ? shieldBreak.pet_rarity_type() : 0;
            const int petMutationType = shieldBreak.has_pet_mutation_type() ? shieldBreak.pet_mutation_type() : 0;
            const QString displayKind = isShiny
                ? QStringLiteral("异色")
                : isFantastic ? QStringLiteral("奇异")
                              : isNightmare ? QStringLiteral("污染") : QStringLiteral("普通");
            const QString logKind = isShiny
                ? QStringLiteral("异色！！")
                : isFantastic ? QStringLiteral("奇异")
                              : isNightmare ? QStringLiteral("污染") : QStringLiteral("普通");

            qInfo().noquote()
                << logKind
                << QStringLiteral("+%1").arg(attrName)
                << QStringLiteral("pet_rarity_type=%1").arg(petRarityType)
                << QStringLiteral("pet_mutation_type=%1").arg(petMutationType);

            emitUiEvent(EventType::BoxHintUpdated, {
                {QStringLiteral("kind"), displayKind},
                {QStringLiteral("base_conf_id"), shieldBreak.has_base_conf_id() ? static_cast<int>(shieldBreak.base_conf_id()) : 0},
                {QStringLiteral("attr_name"), attrName},
                {QStringLiteral("pet_rarity_type"), petRarityType},
                {QStringLiteral("pet_mutation_type"), petMutationType},
            });

            if (isShiny) {
                emitBusinessEvent(EventType::ShinyPetDetected, {
                    {QStringLiteral("title"), QStringLiteral("异色提示")},
                    {QStringLiteral("message"), QStringLiteral("发现异色！！ +%1").arg(attrName)},
                    {QStringLiteral("base_conf_id"), shieldBreak.has_base_conf_id() ? static_cast<int>(shieldBreak.base_conf_id()) : 0},
                    {QStringLiteral("attr_name"), attrName},
                    {QStringLiteral("pet_rarity_type"), petRarityType},
                    {QStringLiteral("pet_mutation_type"), petMutationType},
                });
            }
        }
        return;
    }
    default:
        return;
    }
}

} // namespace app
