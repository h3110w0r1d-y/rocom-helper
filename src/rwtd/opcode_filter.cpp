#include "opcode_filter.h"

namespace rwtd {

OpcodeFilter::OpcodeFilter(QObject *parent)
    : QObject(parent)
{
    recompute();
}

void OpcodeFilter::setUiProfileEnabled(OpcodeProfile profile, bool enabled)
{
    if (profile == OpcodeProfile::None || profile == OpcodeProfile::External) {
        return;
    }

    QSet<quint32> next;
    {
        QMutexLocker locker(&m_mutex);
        const OpcodeProfiles bit = OpcodeProfiles(profile);
        if (enabled) {
            m_uiProfiles |= bit;
        } else {
            m_uiProfiles &= ~bit;
        }
        recompute();
        next = m_enabledOpcodes;
    }
    emit enabledOpcodesChanged(next);
}

bool OpcodeFilter::isUiProfileEnabled(OpcodeProfile profile) const
{
    QMutexLocker locker(&m_mutex);
    return m_uiProfiles.testFlag(profile);
}

QSet<quint32> OpcodeFilter::enabledOpcodes() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabledOpcodes;
}

bool OpcodeFilter::isOpcodeEnabled(quint32 opcode) const
{
    QMutexLocker locker(&m_mutex);
    return m_enabledOpcodes.contains(opcode);
}

void OpcodeFilter::recompute()
{
    OpcodeProfiles active = OpcodeProfile::External | m_uiProfiles;
    m_enabledOpcodes.clear();
    m_enabledOpcodes.reserve(allOpcodeEntries().size());
    for (const OpcodeEntry &entry : allOpcodeEntries()) {
        if ((entry.profiles & active) != OpcodeProfile::None) {
            m_enabledOpcodes.insert(static_cast<quint32>(entry.opcode));
        }
    }
}

} // namespace rwtd
