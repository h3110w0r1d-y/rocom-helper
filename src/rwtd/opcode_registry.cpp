#include "opcode_registry.h"

namespace rwtd {

const QList<OpcodeEntry> &entries()
{
    static const QList<OpcodeEntry> table = {
        {ZoneOpcode::ZoneLoginRsp, OpcodeProfile::External},
        {ZoneOpcode::ZoneSceneMoveReq, OpcodeProfile::Map},
        {ZoneOpcode::ZoneSceneClientEnterSceneFinishNtyAck, OpcodeProfile::Map},
        {ZoneOpcode::ZonePetFreeRsp, OpcodeProfile::External},
        {ZoneOpcode::ZoneSceneEndThrowRsp, OpcodeProfile::CatchLog | OpcodeProfile::External},
        {ZoneOpcode::ZoneUseMultiBagItemRsp, OpcodeProfile::External},
        {ZoneOpcode::ZoneScenePlayActsBatchNotify, OpcodeProfile::Map},
        {ZoneOpcode::ZoneScenePlayActsNotify, OpcodeProfile::Map},
        {ZoneOpcode::ZoneBattlePerformStartNotify, OpcodeProfile::BoxHint},
        {ZoneOpcode::ZoneBattleFinishNotify, OpcodeProfile::BoxHint},
        {ZoneOpcode::ZoneBattleForceFinishNotify, OpcodeProfile::BoxHint},
        {ZoneOpcode::ZoneGetPetInfoByPageRsp, OpcodeProfile::External},
        {ZoneOpcode::ZonePetMedalCommonRsp, OpcodeProfile::External},
        {ZoneOpcode::ZonePetBoxTidyRsp, OpcodeProfile::External},
        {ZoneOpcode::ZonePetBoxChangePetRsp, OpcodeProfile::External},
        {ZoneOpcode::ZoneSceneThrowCatchFinishRsp, OpcodeProfile::CatchLog | OpcodeProfile::External},
        {ZoneOpcode::ZoneHomeQueryFriendHomeInfoRsp, OpcodeProfile::EggTime},
    };
    return table;
}

const QHash<quint32, OpcodeProfiles> &profileLookup()
{
    static const QHash<quint32, OpcodeProfiles> lookup = [] {
        QHash<quint32, OpcodeProfiles> map;
        map.reserve(entries().size());
        for (const OpcodeEntry &entry : entries()) {
            map.insert(static_cast<quint32>(entry.opcode), entry.profiles);
        }
        return map;
    }();
    return lookup;
}

const QList<OpcodeEntry> &allOpcodeEntries()
{
    return entries();
}

const QSet<quint32> &allUsedOpcodes()
{
    static const QSet<quint32> opcodes = [] {
        QSet<quint32> set;
        set.reserve(entries().size());
        for (const OpcodeEntry &entry : entries()) {
            set.insert(static_cast<quint32>(entry.opcode));
        }
        return set;
    }();
    return opcodes;
}

const QHash<quint32, ZoneOpcode> &usedZoneOpcodeByRaw()
{
    static const QHash<quint32, ZoneOpcode> lookup = [] {
        QHash<quint32, ZoneOpcode> map;
        map.reserve(entries().size());
        for (const OpcodeEntry &entry : entries()) {
            map.insert(static_cast<quint32>(entry.opcode), entry.opcode);
        }
        return map;
    }();
    return lookup;
}

OpcodeProfiles profilesForOpcode(quint32 opcode)
{
    return profileLookup().value(opcode, OpcodeProfile::None);
}

bool isUsedOpcode(quint32 opcode)
{
    return profileLookup().contains(opcode);
}

} // namespace rwtd
