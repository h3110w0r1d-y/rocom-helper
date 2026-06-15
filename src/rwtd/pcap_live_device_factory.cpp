#define protected public
#include <PcapLiveDevice.h>
#if defined(_WIN32)
#include <WinPcapLiveDevice.h>
#endif
#undef protected

#include "pcap_live_device_factory.h"

#include <pcap.h>

namespace rwtd {

std::unique_ptr<pcpp::PcapLiveDevice> createLiveDevice(pcap_if_t *iface, bool calculateMtu, bool calculateMacAddress,
                                                       bool calculateDefaultGateway)
{
#if defined(_WIN32)
    return std::unique_ptr<pcpp::PcapLiveDevice>(
        new pcpp::WinPcapLiveDevice(iface, calculateMtu, calculateMacAddress, calculateDefaultGateway));
#else
    return std::unique_ptr<pcpp::PcapLiveDevice>(
        new pcpp::PcapLiveDevice(iface, calculateMtu, calculateMacAddress, calculateDefaultGateway));
#endif
}

} // namespace rwtd
