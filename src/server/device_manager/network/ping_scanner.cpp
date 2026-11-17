// src/server/device_manager/network/ping_scanner.cpp
#include "ping_scanner.h"
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include "../utils/network_utils.h"
#include "../utils/mac_utils.h"

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#endif

bool PingScanner::pingDevice(const std::string& ip_address) {
#ifdef _WIN32
    HANDLE hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    ULONG dest_ip = inet_addr(ip_address.c_str());
    if (dest_ip == INADDR_NONE) {
        IcmpCloseHandle(hIcmpFile);
        return false;
    }

    char sendData[32] = "ping";
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
    std::unique_ptr<char[]> replyBuffer(new char[replySize]);

    DWORD status = IcmpSendEcho(hIcmpFile, dest_ip, sendData, sizeof(sendData),
                                NULL, replyBuffer.get(), replySize, 1000); 

    bool result = (status != 0);

    IcmpCloseHandle(hIcmpFile);
    return result;
#else
    return false;
#endif
}

std::vector<std::string> PingScanner::asyncPingScan(const std::vector<std::string>& ip_list, int max_threads) {
    std::vector<std::string> alive_ips;
    std::mutex result_mutex;
    std::atomic<int> completed(0);
    std::atomic<int> total(static_cast<int>(ip_list.size()));

    size_t thread_count = (std::min)(max_threads, static_cast<int>(ip_list.size()));
    std::vector<std::thread> threads;

    std::queue<std::string> task_queue;
    for (const auto& ip : ip_list) {
        task_queue.push(ip);
    }
    std::mutex queue_mutex;

    auto worker_func = [&]() {
        while (true) {
            std::string ip;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (task_queue.empty())
                    break;
                ip = task_queue.front();
                task_queue.pop();
            }

            if (pingDevice(ip)) {
                std::lock_guard<std::mutex> lock(result_mutex);
                alive_ips.push_back(ip);
            }

            completed++;
        }
    };

    for (size_t i = 0; i < thread_count; i++) {
        threads.emplace_back(worker_func);
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    return alive_ips;
}

std::vector<DeviceInfo> PingScanner::scan(const std::string& target, const std::string& mode) {
    std::vector<std::string> ip_list = NetworkUtils::generateIPRange(target);
    std::vector<std::string> alive_ips = asyncPingScan(ip_list);

    std::vector<DeviceInfo> devices;
    for (const auto& ip : alive_ips) {
        DeviceInfo device;
        device.ip_address = ip;
        device.hostname = NetworkUtils::resolveHostname(ip);
        device.mac_address = MacUtils::getRealMacAddress(ip);
        device.device_type = MacUtils::identifyDeviceType(device.mac_address);
        device.is_online = true;
        device.is_connected = false;
        device.last_seen = NetworkUtils::getCurrentTime();
        devices.push_back(device);
    }

    return devices;
}