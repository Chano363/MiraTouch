#pragma once
#include "device_types.h"
#include <string>

struct DeviceInfo {
    std::string ip_address;       
    std::string hostname;          
    std::string mac_address;      
    DeviceType device_type{DeviceType::UNKNOWN};
    bool is_online{false};        
    bool is_connected{false};    
    std::string last_seen;       
    std::string username;         
    std::string password;         
};