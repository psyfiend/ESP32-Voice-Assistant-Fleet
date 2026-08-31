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
        #ifdef DEBUG_WIFI
        Serial.println("[ConnectivityManager] No STA SSID configured (NVS empty, no compile-time default) - skipping connect attempt.");
        #endif
        _state = WiFiConnState::FAILED;
        return false;
    }

    WiFi.mode(WIFI_STA);

    for (uint8_t attempt = 1; attempt <= retries; attempt++) {
        #ifdef DEBUG_WIFI
        Serial.printf("[ConnectivityManager] STA connect attempt %u/%u to \"%s\"...\n", attempt, retries, ssid);
        #endif
        _state = WiFiConnState::CONNECTING;
        WiFi.begin(ssid, password);

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
            delay(250);
        }

        if (WiFi.status() == WL_CONNECTED) {
            _state = WiFiConnState::CONNECTED;
            #ifdef DEBUG_WIFI
            Serial.printf("[ConnectivityManager] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
            #endif
            return true;
        }

        #ifdef DEBUG_WIFI
        Serial.printf("[ConnectivityManager] Attempt %u timed out (status=%d).\n", attempt, WiFi.status());
        #endif
        WiFi.disconnect();
    }

    _state = WiFiConnState::FAILED;
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
        Serial.println("[ConnectivityManager] Using NVS-stored STA credentials.");
        #endif
    } else {
        #ifdef DEBUG_WIFI
        Serial.println("[ConnectivityManager] NVS unconfigured - falling back to compile-time default.");
        #endif
    }

    bool connected = connectSTA(ssid, password, _wifiDefaults.STA_CONNECT_TIMEOUT_MS, _wifiDefaults.STA_RETRY_COUNT);

    #ifdef DEBUG_WIFI
    if (!connected) {
        Serial.println("[ConnectivityManager] STA connect failed after all retries. AP/captive-portal fallback not implemented yet (Phase 2).");
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
