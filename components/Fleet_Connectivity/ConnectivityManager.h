#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include "ConnectivityDefaults.h"

enum class WiFiConnState : uint8_t {
    NOT_STARTED,
    CONNECTING,
    CONNECTED,
    FAILED, // retries exhausted, no fallback yet - AP/captive-portal fallback is Phase 2
};

// Phase 1: station-mode connect only (NVS-stored credentials, falling back
// to the compile-time default when NVS has never been configured), no
// AP/captive-portal fallback yet. See docs/FUTURE_IMPROVEMENTS.md's
// Connectivity section for the full phased plan.
class ConnectivityManager {
public:
    ConnectivityManager();

    bool begin();

    WiFiConnState getState() const { return _state; }
    bool          isConnected() const;
    IPAddress     getLocalIP() const;

    // --= NVS credential access - used by the GUI once it exists (Phase 3) =--
    bool setStationCredentials(const char *ssid, const char *password);
    void clearStationCredentials();

private:
    bool connectSTA(const char *ssid, const char *password, uint32_t timeoutMs, uint8_t retries);
    void loadCredentialsFromNvs(String &ssidOut, String &passwordOut, bool &configuredOut);

    const WiFiDefaults &_wifiDefaults;
    WiFiConnState        _state;
};
