#pragma once
#ifndef PING_SCANNER_H
#define PING_SCANNER_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "scanner.h"
#include <string>
#include <vector>

class PingScanner : public NetworkScannerInterface {
public:
  std::vector<DeviceInfo> scan(const std::string &target,
                               const std::string &mode) override;

private:
  bool pingDevice(const std::string &ip_address);
  std::vector<std::string>
  asyncPingScan(const std::vector<std::string> &ip_list, int max_threads = 50);
};

#endif // PING_SCANNER_H