// src/server/device_manager/utils/network_utils.h
#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>

class NetworkUtils {
public:
    static std::vector<std::string> generateIPRange(const std::string& subnet);

    static std::string resolveHostname(const std::string& ip_address);

    static std::string getCurrentTime();

    static std::vector<std::string> getLocalNetworkSubnets();

private:
    static void calculateSubnetRange(const std::string& subnet, uint32_t& network_addr, uint32_t& broadcast_addr);
};