#include "UDPMulticaster.hpp"
#include <iostream>
#include <chrono>
#include <thread>

UDPMulticaster& UDPMulticaster::getInstance() {
    static UDPMulticaster inst;
    return inst;
}

UDPMulticaster::UDPMulticaster() {}
UDPMulticaster::~UDPMulticaster() { stop(); }

void UDPMulticaster::start(const std::string& group, uint16_t port) {
    if (running_) return;
    running_ = true;
    std::cout << "Starting multicast to " << group << ":" << port << "\n";
    sender_ = std::thread(&UDPMulticaster::senderLoop, this);
    std::cout << "Started multicast thread\n";
}

void UDPMulticaster::stop() {
    if (!running_) return;
    running_ = false;
    if (sender_.joinable()) sender_.join();
    std::cout << "Stopped multicast thread\n";
}

void UDPMulticaster::enqueue(std::vector<uint8_t> pdu) {
    queue_.enqueue(std::move(pdu));
}

void UDPMulticaster::senderLoop() {
    while (running_) {
        auto pdu = queue_.dequeue();  // blocks until something arrives
        // TODO: send via sendto(sock_, pdu.data(), pdu.size(), ...)
        std::cout << "Sending " << pdu.size() << " bytes\n";
    }
}
