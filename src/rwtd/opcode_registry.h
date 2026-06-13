#pragma once

#include "rwtd/zone_opcodes.h"

#include <QFlags>
#include <QHash>
#include <QSet>

namespace rwtd {

enum class OpcodeProfile : quint8 {
    None     = 0,
    Map      = 1 << 0,
    CatchLog = 1 << 1,
    BoxHint  = 1 << 2,
    EggTime  = 1 << 3,
    External = 1 << 4,
};
Q_DECLARE_FLAGS(OpcodeProfiles, OpcodeProfile)
Q_DECLARE_OPERATORS_FOR_FLAGS(OpcodeProfiles)

struct OpcodeEntry {
    ZoneOpcode opcode = ZoneOpcode::ZoneLoginRsp;
    OpcodeProfiles profiles = OpcodeProfile::None;
};

const QList<OpcodeEntry> &allOpcodeEntries();
const QSet<quint32> &allUsedOpcodes();
const QHash<quint32, ZoneOpcode> &usedZoneOpcodeByRaw();
OpcodeProfiles profilesForOpcode(quint32 opcode);
bool isUsedOpcode(quint32 opcode);

} // namespace rwtd
