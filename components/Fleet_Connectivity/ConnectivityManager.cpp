#include "ConnectivityManager.h"
#include <WiFi.h>
#include <Preferences.h>

static const char *NVS_NAMESPACE      = "wifi";
static const char *NVS_KEY_CONFIGURED = "configured";
static const char *NVS_KEY_SSID       = "ssid";
static const char *NVS_KEY_PASSWORD   = "password";

ConnectivityManager::ConnectivityManager()
    : _wifiDefaults(CONNECTIVITY_DEFAULT_WIFI), _state(WiFiConnState::NOT_STARTED) {}

void ConnectivityManager::loadCredentialsFromNvs(String &ssidOut, String &passwordOut, bool &configuredOut) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
    configuredOut = prefs.getBool(NVS_KEY_CONFIGURED, false);
    ssidOut        = prefs.getString(NVS_KEY_SSID, "");
    passwordOut    = prefs.getString(NVS_KEY_PASSWORD, "");
    prefs.end();
}

bool ConnectivityManager::setStationCredentials(const char *ssid, const char *password) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return false;
    prefs.putBool(NVS_KEY_CONFIGURED, true);
    prefs.putString(NVS_KEY_SSID, ssid);
    prefs.putString(NVS_KEY_PASSWORD, password);
    prefs.end();
    return true;
}

void ConnectivityManager::clearStationCredentials() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
    prefs.clear();
    prefs.end();
}

bool ConnectivityManager::connectSTA(const char *ssid, const char *password, uint32_t timeoutMs, uint8_t retries) {
    if (!ssid || ssid[0] == '\0') {
        Serial.println("[WiFi] No STA SSID configured - not attempting to connect.");
        _state = WiFiConnState::FAILED;
        return false;
    }

    // Baseline: mode changes and connect start/success/failure always print.
    // Per-attempt detail (retry count, raw status codes, timeouts) is
    // troubleshooting-only - gate that behind DEBUG_WIFI, not this.
    Serial.printf("[WiFi] Mode: STA - connecting to \"%s\"...\n", ssid);
    WiFi.mode(WIFI_STA);

    for (uint8_t attempt = 1; attempt <= retries; attempt++) {
        #ifdef DEBUG_WIFI
        Serial.printf("[WiFi:debug] STA connect attempt %u/%u to \"%s\"...\n", attempt, retries, ssid);
        #endif
        _state = WiFiConnState::CONNECTING;
        WiFi.begin(ssid, password);

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
            delay(250);
        }

        if (WiFi.status() == WL_CONNECTED) {
            _state = WiFiConnState::CONNECTED;
            Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }

        #ifdef DEBUG_WIFI
        Serial.printf("[WiFi:debug] Attempt %u timed out (status=%d).\n", attempt, WiFi.status());
        #endif
        WiFi.disconnect();
    }

    _state = WiFiConnState::FAILED;
    Serial.printf("[WiFi] Failed to connect to \"%s\" after %u attempt(s).\n", ssid, retries);
    return false;
}

bool ConnectivityManager::begin() {
    String nvsSsid, nvsPassword;
    bool   nvsConfigured = false;
    loadCredentialsFromNvs(nvsSsid, nvsPassword, nvsConfigured);

    const char *ssid     = _wifiDefaults.STA_SSID;
    const char *password = _wifiDefaults.STA_PASSWORD;

    if (nvsConfigured && nvsSsid.length() > 0) {
        ssid     = nvsSsid.c_str();
        password = nvsPassword.c_str();
        #ifdef DEBUG_WIFI
        Serial.println("[WiFi:debug] Using NVS-stored STA credentials.");
        #endif
    } else {
        #ifdef DEBUG_WIFI
        Serial.println("[WiFi:debug] NVS unconfigured - falling back to compile-time default.");
        #endif
    }

    bool connected = connectSTA(ssid, password, _wifiDefaults.STA_CONNECT_TIMEOUT_MS, _wifiDefaults.STA_RETRY_COUNT);

    #ifdef DEBUG_WIFI
    if (!connected) {
        Serial.println("[WiFi:debug] AP/captive-portal fallback not implemented yet (Phase 2) - device has no connectivity.");
    }
    #endif

    return connected;
}

bool ConnectivityManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

IPAddress ConnectivityManager::getLocalIP() const {
    return WiFi.localIP();
}
