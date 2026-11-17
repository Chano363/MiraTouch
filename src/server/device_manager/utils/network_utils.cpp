// src/server/device_manager/utils/network_utils.cpp
#include "network_utils.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

void NetworkUtils::calculateSubnetRange(const std::string& subnet, uint32_t& network_addr, uint32_t& broadcast_addr) {
    size_t slash_pos = subnet.find('/');
    if (slash_pos == std::string::npos) {
        // 如果没有子网掩码，假设为单个IP
        network_addr = inet_addr(subnet.c_str());
        broadcast_addr = network_addr;
        return;
    }

    std::string ip_str = subnet.substr(0, slash_pos);
    int prefix_len = std::stoi(subnet.substr(slash_pos + 1));

    uint32_t ip_addr = inet_addr(ip_str.c_str());
    ip_addr = ntohl(ip_addr); // 转换为主机字节序

    uint32_t mask = (prefix_len == 32) ? 0xFFFFFFFF : (~(0xFFFFFFFF >> prefix_len));

    network_addr = htonl(ip_addr & mask);               // 网络地址
    broadcast_addr = htonl((ip_addr & mask) | (~mask)); // 广播地址
}

std::vector<std::string> NetworkUtils::generateIPRange(const std::string& subnet) {
    std::vector<std::string> ips;

    uint32_t network_addr, broadcast_addr;
    calculateSubnetRange(subnet, network_addr, broadcast_addr);

    network_addr = ntohl(network_addr);
    broadcast_addr = ntohl(broadcast_addr);

    // 生成该子网内所有主机地址
    for (uint32_t ip = network_addr + 1; ip < broadcast_addr; ip++) {
        struct in_addr addr;
        addr.s_addr = htonl(ip);
        ips.push_back(inet_ntoa(addr));
    }

    return ips;
}

std::string NetworkUtils::resolveHostname(const std::string& ip_address) {
#ifdef _WIN32
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip_address.c_str(), &addr.sin_addr);

    char hostname[NI_MAXHOST] = { 0 };
    int result = getnameinfo((struct sockaddr*)&addr, sizeof(addr),
                             hostname, sizeof(hostname),
                             NULL, 0, 0);

    if (result == 0 && hostname[0] != '\0') {
        return std::string(hostname);
    }
#else
    struct in_addr addr;
    inet_aton(ip_address.c_str(), &addr);
    
    struct hostent* he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
    if (he != nullptr) {
        return std::string(he->h_name);
    }
#endif

    return ip_address;
}

std::string NetworkUtils::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::vector<std::string> NetworkUtils::getLocalNetworkSubnets() {
    std::vector<std::string> subnets;

#ifdef _WIN32
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
    ULONG outBufLen = 0;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    // First call to get the size needed
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (pAddresses) {
            // Second call to get the actual data
            if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &outBufLen) == NO_ERROR) {
                PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
                while (pCurrAddresses) {
                    if (pCurrAddresses->OperStatus == IfOperStatusUp) {
                        PIP_ADAPTER_UNICAST_ADDRESS_LH pUnicast = pCurrAddresses->FirstUnicastAddress;
                        while (pUnicast) {
                            sockaddr* sa = pUnicast->Address.lpSockaddr;
                            if (sa->sa_family == AF_INET) {
                                sockaddr_in* sin = (sockaddr_in*)sa;
                                char ip_str[INET_ADDRSTRLEN];
                                inet_ntop(AF_INET, &(sin->sin_addr), ip_str, INET_ADDRSTRLEN);

                                // Calculate subnet mask
                                ULONG prefixLength = pUnicast->OnLinkPrefixLength;
                                if (prefixLength > 0 && prefixLength <= 32) {
                                    // Calculate network address
                                    uint32_t ip_addr = ntohl(sin->sin_addr.s_addr);
                                    uint32_t mask = (prefixLength == 32) ? 0xFFFFFFFF : (~(0xFFFFFFFF >> prefixLength));
                                    uint32_t network_addr = ip_addr & mask;
                                    
                                    // Convert network address back to string
                                    in_addr network_in_addr;
                                    network_in_addr.s_addr = htonl(network_addr);
                                    char network_str[INET_ADDRSTRLEN];
                                    inet_ntop(AF_INET, &network_in_addr, network_str, INET_ADDRSTRLEN);
                                    
                                    // Create subnet string
                                    std::string subnet = std::string(network_str) + "/" + std::to_string(prefixLength);
                                    subnets.push_back(subnet);
                                }
                            }
                            pUnicast = pUnicast->Next;
                        }
                    }
                    pCurrAddresses = pCurrAddresses->Next;
                }
            }
            free(pAddresses);
        }
    }
#endif

    // If no local subnets found, return common subnets
    if (subnets.empty()) {
        subnets = {"192.168.1.0/24", "192.168.0.0/24", "10.0.0.0/8", "172.16.0.0/12"};
    }

    return subnets;
}