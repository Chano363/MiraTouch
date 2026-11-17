#include "scanner.h"
#include "nmap_scanner.h"
#include "ping_scanner.h"

std::unique_ptr<NetworkScannerInterface> ScannerFactory::createScanner(ScannerFactory::ScannerType type) {
    switch (type) {
        case ScannerType::NMAP:
            return std::make_unique<NmapScanner>();
        case ScannerType::PING:
            return std::make_unique<PingScanner>();
        default:
            return nullptr;
    }
}