#pragma once

#include <pcap.h>

#include <memory>

namespace pcpp {
class PcapLiveDevice;
} // namespace pcpp

namespace rwtd {

std::unique_ptr<pcpp::PcapLiveDevice> createLiveDevice(pcap_if_t *iface, bool calculateMtu, bool calculateMacAddress,
                                                       bool calculateDefaultGateway);

} // namespace rwtd
