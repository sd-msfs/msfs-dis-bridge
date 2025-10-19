#pragma once
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include "ThreadSafeQueue.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

class UDPMulticaster {
public:
    static UDPMulticaster& getInstance();

    bool start(const std::string& group, uint16_t port,
               const std::string& ifaddr = "", int ttl = 1, bool loopback = false);
    void stop();
    void enqueue(std::vector<uint8_t> pdu);

private:
    UDPMulticaster();
    ~UDPMulticaster();

    void senderLoop();

    std::atomic<bool> running_{false};
    SOCKET sock_{INVALID_SOCKET};
    std::thread sender_;
    ThreadSafeQueue<std::vector<uint8_t>> queue_;
    std::string group_;
    uint16_t port_;
    sockaddr_in dst_; // Added declaration for destination address
};