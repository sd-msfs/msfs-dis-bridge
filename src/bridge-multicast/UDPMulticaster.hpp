#pragma once
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include "ThreadSafeQueue.hpp"

class UDPMulticaster {
public:
    static UDPMulticaster& getInstance();

    void start(const std::string& group, uint16_t port);
    void stop();
    void enqueue(std::vector<uint8_t> pdu);

    // no copies
    UDPMulticaster(const UDPMulticaster&) = delete;
    UDPMulticaster& operator=(const UDPMulticaster&) = delete;

private:
    UDPMulticaster();
    ~UDPMulticaster();
    void senderLoop();

    int sock_{-1};
    std::thread sender_;
    std::atomic<bool> running_{false};

    ThreadSafeQueue<std::vector<uint8_t>> queue_;
};
