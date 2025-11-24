#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include "UDPMulticaster.hpp"
#include "services/MetricsCollector.h"
#include <iostream>
#include <MSWSock.h>

UDPMulticaster& UDPMulticaster::getInstance() {
    static UDPMulticaster inst;
    return inst;
}

UDPMulticaster::UDPMulticaster() {}
UDPMulticaster::~UDPMulticaster() { stop(); }

bool UDPMulticaster::start(const std::string& group, uint16_t port,
                           const std::string& ifaddr, int ttl, bool loopback) {
    if (running_.exchange(true)) return true; // already running

    // Winsock init
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "[Multicast] WSAStartup failed\n";
        running_ = false;
        return false;
    }

    // Create UDP socket
    sock_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == INVALID_SOCKET) {
        std::cerr << "[Multicast] socket() failed: " << WSAGetLastError() << "\n";
        running_ = false;
        WSACleanup();
        return false;
    }

    // Optional: bigger send buffer
    int snd = 1<<20; // 1 MB
    ::setsockopt(sock_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&snd), sizeof(snd));

    // TTL + loopback
    unsigned char ttl_uc = static_cast<unsigned char>(ttl);
    ::setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl_uc), sizeof(ttl_uc));
    unsigned char loop_uc = loopback ? 1 : 0;
    ::setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop_uc), sizeof(loop_uc));

    // Choose egress interface if provided
    if (!ifaddr.empty()) {
        in_addr iface{};
        if (InetPtonA(AF_INET, ifaddr.c_str(), &iface) == 1) {
            ::setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&iface), sizeof(iface));
        } else {
            std::cerr << "[Multicast] Invalid interface IP: " << ifaddr << "\n";
        }
    }

    // (Nice-to-have) suppress UDP “connection reset” noise
    DWORD bytes=0, newBehavior = FALSE;
    ::WSAIoctl(sock_, SIO_UDP_CONNRESET, &newBehavior, sizeof(newBehavior), nullptr, 0, &bytes, nullptr, nullptr);

    // Destination group:port
    memset(&dst_, 0, sizeof(dst_));
    dst_.sin_family = AF_INET;
    dst_.sin_port   = htons(port);
    if (InetPtonA(AF_INET, group.c_str(), &dst_.sin_addr) != 1) {
        std::cerr << "[Multicast] Invalid group IP: " << group << "\n";
        ::closesocket(sock_); sock_ = INVALID_SOCKET;
        running_ = false;
        WSACleanup();
        return false;
    }

    group_ = group;
    port_  = port;

    std::cout << "[Multicast] Sending to " << group_ << ":" << port_
              << (ifaddr.empty() ? "" : (" via " + ifaddr))
              << " ttl=" << ttl << " loop=" << (loopback?1:0) << "\n";

    sender_ = std::thread(&UDPMulticaster::senderLoop, this);
    return true;
}

void UDPMulticaster::stop() {
    if (!running_.exchange(false)) return;

    // Wake the blocking dequeue with a sentinel
    queue_.enqueue(std::vector<uint8_t>{});

    if (sender_.joinable()) sender_.join();

    if (sock_ != INVALID_SOCKET) {
        ::closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    WSACleanup();

    std::cout << "[Multicast] Stopped\n";
}

void UDPMulticaster::enqueue(std::vector<uint8_t> pdu) {
    queue_.enqueue(std::move(pdu));
}

void UDPMulticaster::senderLoop() {
    while (true) {
        auto pdu = queue_.dequeue(); // blocks
        // std::cout << "Dequeued PDU of size " << pdu.size() << "\n";
        if (!running_.load(std::memory_order_relaxed) || pdu.empty()) break;

        int rc = ::sendto(sock_,
                          reinterpret_cast<const char*>(pdu.data()),
                          static_cast<int>(pdu.size()),
                          0,
                          reinterpret_cast<sockaddr*>(&dst_),
                          sizeof(dst_));

        // Record metrics
        if (rc == SOCKET_ERROR) {
            std::cerr << "[Multicast] sendto() failed: " << WSAGetLastError() << "\n";
            DISBridge::Services::MetricsCollector::getInstance().recordError("network",
                "Multicast sendto failed with error: " + std::to_string(WSAGetLastError()));
        } else {
            DISBridge::Services::MetricsCollector::getInstance().recordPacketSent(pdu.size());
        }
    }
}
