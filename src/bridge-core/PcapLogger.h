#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

class PcapLogger {
public:
    bool open(const std::string& filename);
    void writePacket(const std::vector<uint8_t>& rawPayload);
    void close();
    bool isOpen() const;

    //runtime parameters
    void setDestinationIp(const std::string& ip);
    void setUdpPorts(uint16_t srcPort, uint16_t dstPort);

private:
    std::ofstream file;
    in_addr localIp{};
    in_addr destinationIp{};

    uint16_t udpSrcPort = 54321;
    uint16_t udpDstPort = 12345;

    static in_addr getLocalIPAddress();
};