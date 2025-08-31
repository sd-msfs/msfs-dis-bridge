#include "PcapLogger.h"
#include <chrono>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

// PCAP global and packet headers
#pragma pack(push, 1)
struct PcapGlobalHeader {
    uint32_t magic_number = 0xa1b2c3d4;
    uint16_t version_major = 2;
    uint16_t version_minor = 4;
    int32_t  thiszone = 0;
    uint32_t sigfigs = 0;
    uint32_t snaplen = 65535;
    uint32_t network = 1; // Ethernet
};

struct PcapPacketHeader {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};
#pragma pack(pop)

in_addr PcapLogger::getLocalIPAddress() {
    char hostname[256];
    in_addr addr{};
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        addr.s_addr = INADDR_ANY;
        return addr;
    }

    addrinfo hints{}, * info;
    hints.ai_family = AF_INET; // IPv4 only
    if (getaddrinfo(hostname, nullptr, &hints, &info) != 0) {
        addr.s_addr = INADDR_ANY;
        return addr;
    }

    sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(info->ai_addr);
    addr = ipv4->sin_addr;

    freeaddrinfo(info);
    return addr;
}

// Wrap raw DIS PDU into a fake Ethernet + IPv4 + UDP packet
static std::vector<uint8_t> wrapAsUdpPacket(const std::vector<uint8_t>& payload,
    const in_addr& srcIp,
    const in_addr& dstIp,
    uint16_t udpSrcPort,
    uint16_t udpDstPort) {
    std::vector<uint8_t> packet;

    // Ethernet header (14 bytes)
    uint8_t eth[14] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,   // Destination
        0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,   // Source
        0x08, 0x00                            // EtherType = IPv4
    };
    packet.insert(packet.end(), eth, eth + 14);

    // IPv4 header (20 bytes)
    uint8_t ip[20] = {};
    ip[0] = 0x45;  // Version 4 + IHL = 5
    ip[1] = 0x00;  // DSCP/ECN
    uint16_t total_len = htons(20 + 8 + payload.size());
    std::memcpy(&ip[2], &total_len, 2);
    ip[6] = 0x40;  // Don't Fragment
    ip[8] = 64;    // TTL
    ip[9] = 17;    // Protocol = UDP

    std::memcpy(&ip[12], &srcIp.s_addr, 4);
    std::memcpy(&ip[16], &dstIp.s_addr, 4);

    packet.insert(packet.end(), ip, ip + 20);

    // UDP header (8 bytes)
    uint8_t udp[8] = {};
    uint16_t src_port = htons(udpSrcPort);
    uint16_t dst_port = htons(udpDstPort);
    uint16_t udp_len = htons(8 + payload.size());

    std::memcpy(&udp[0], &src_port, 2);
    std::memcpy(&udp[2], &dst_port, 2);
    std::memcpy(&udp[4], &udp_len, 2);
    udp[6] = 0x00; udp[7] = 0x00; // No checksum

    packet.insert(packet.end(), udp, udp + 8);

    // Payload
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;
}

bool PcapLogger::open(const std::string& filename) {
    file.open(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    localIp = getLocalIPAddress();
    inet_pton(AF_INET, "192.168.1.238", &destinationIp); // default

    PcapGlobalHeader header;
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    return true;
}

void PcapLogger::setDestinationIp(const std::string& ip) {
    inet_pton(AF_INET, ip.c_str(), &destinationIp);
}

void PcapLogger::setUdpPorts(uint16_t srcPort, uint16_t dstPort) {
    udpSrcPort = srcPort;
    udpDstPort = dstPort;
}

void PcapLogger::writePacket(const std::vector<uint8_t>& rawPayload) {
    if (!file.is_open()) return;

    auto wrapped = wrapAsUdpPacket(rawPayload, localIp, destinationIp,
        udpSrcPort, udpDstPort);

    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(epoch);
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(epoch - sec);

    PcapPacketHeader pktHeader;
    pktHeader.ts_sec = static_cast<uint32_t>(sec.count());
    pktHeader.ts_usec = static_cast<uint32_t>(usec.count());
    pktHeader.incl_len = static_cast<uint32_t>(wrapped.size());
    pktHeader.orig_len = static_cast<uint32_t>(wrapped.size());

    file.write(reinterpret_cast<const char*>(&pktHeader), sizeof(pktHeader));
    file.write(reinterpret_cast<const char*>(wrapped.data()), wrapped.size());
}

void PcapLogger::close() {
    if (file.is_open()) {
        file.close();
    }
}

bool PcapLogger::isOpen() const {
    return file.is_open();
}