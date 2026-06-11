#pragma once

#include "opcode_registry.h"

#include <QMutex>
#include <QObject>
#include <QSet>

namespace rwtd {

class OpcodeFilter : public QObject {
    Q_OBJECT

public:
    explicit OpcodeFilter(QObject *parent = nullptr);

    void setUiProfileEnabled(OpcodeProfile profile, bool enabled);
    bool isUiProfileEnabled(OpcodeProfile profile) const;

    QSet<quint32> enabledOpcodes() const;
    bool isOpcodeEnabled(quint32 opcode) const;

signals:
    void enabledOpcodesChanged(const QSet<quint32> &opcodes);

private:
    void recompute();

    mutable QMutex m_mutex;
    OpcodeProfiles m_uiProfiles = OpcodeProfile::None;
    QSet<quint32> m_enabledOpcodes;
};

} // namespace rwtd
