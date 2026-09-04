#include "DeviceIdentity.h"
#include "bsp_loader.h"   // brings the active board's identity macro into scope
#include <esp_mac.h>
#include <ctype.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Board slug. Derived from the same per-board identity macro every BSP header
// already defines (see CLAUDE.md's Board selection section) - no new build flag
// and no second list to keep in sync. Lowercase with hyphens so it drops
// straight into a DNS label; the underscore form is produced by substitution.
// ---------------------------------------------------------------------------
#if   defined(WS_P4_7B)
    #define FLEET_BOARD_SLUG "ws-p4-7b"
#elif defined(WS_P4_5)
    #define FLEET_BOARD_SLUG "ws-p4-5"
#elif defined(WS_P4_4B)
    #define FLEET_BOARD_SLUG "ws-p4-4b"
#elif defined(WS_S3_5B)
    #define FLEET_BOARD_SLUG "ws-s3-5b"
#elif defined(WS_S3_4B)
    #define FLEET_BOARD_SLUG "ws-s3-4b"
#elif defined(CYD_P4_1060)
    #define FLEET_BOARD_SLUG "cyd-p4-1060"
#elif defined(CYD_S3_3248)
    #define FLEET_BOARD_SLUG "cyd-s3-3248"
#elif defined(CYD_S3_8048)
    #define FLEET_BOARD_SLUG "cyd-s3-8048"
#else
    // Deliberately not a hard error: an unknown board still gets a usable,
    // unique identity from its MAC. It just is not self-describing in HA.
    #warning "DeviceIdentity: unrecognised board macro - falling back to 'fleet-dev'"
    #define FLEET_BOARD_SLUG "dev"
#endif

namespace {

char g_macSuffix[8]   = {0};
char g_hostname[64]   = {0};
char g_deviceId[64]   = {0};
char g_apSsid[36]     = {0};
char g_apPassword[16] = {0};
bool g_built          = false;

void buildOnce() {
    if (g_built) return;
    g_built = true;

    // efuse MAC rather than the WiFi-interface MAC: it is readable before the
    // radio is up (which matters, since the hostname must be applied before
    // WiFi.begin()), and on the P4 boards it identifies the board itself
    // rather than the ESP32-C6 co-processor that happens to carry the radio.
    uint8_t mac[6] = {0};
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        esp_read_mac(mac, ESP_MAC_WIFI_STA);   // last resort
    }
    snprintf(g_macSuffix, sizeof(g_macSuffix), "%02x%02x%02x", mac[3], mac[4], mac[5]);

    snprintf(g_hostname, sizeof(g_hostname), "fleet-%s-%s", FLEET_BOARD_SLUG, g_macSuffix);

    // Same identity, underscore rendering. Built by substitution rather than a
    // second literal so the two can never drift apart.
    snprintf(g_deviceId, sizeof(g_deviceId), "%s", g_hostname);
    for (char *p = g_deviceId; *p; ++p) {
        if (*p == '-') *p = '_';
    }

    char upper[8];
    snprintf(upper, sizeof(upper), "%s", g_macSuffix);
    for (char *p = upper; *p; ++p) *p = (char)toupper((unsigned char)*p);
    snprintf(g_apSsid, sizeof(g_apSsid), "FleetSetup-%s", upper);

    // Per-device provisioning password derived from the MAC. Not a secret in
    // the cryptographic sense - anyone who can see the AP can see its SSID -
    // but far better than one shared password compiled into every board, and
    // it is displayed in Settings so it stays discoverable to the owner.
    snprintf(g_apPassword, sizeof(g_apPassword), "fleet%s", g_macSuffix);  // 11 chars, >= WPA2 minimum of 8
}

} // namespace

namespace DeviceIdentity {

const char *boardSlug()  { return FLEET_BOARD_SLUG; }
const char *macSuffix()  { buildOnce(); return g_macSuffix; }
const char *hostname()   { buildOnce(); return g_hostname; }
const char *deviceId()   { buildOnce(); return g_deviceId; }
const char *apSsid()     { buildOnce(); return g_apSsid; }
const char *apPassword() { buildOnce(); return g_apPassword; }

bool sanitizeHostname(const char *in, char *out, size_t outLen) {
    if (!in || !out || outLen < 2) return false;

    size_t n = 0;
    for (const char *p = in; *p && n < outLen - 1; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c)) {
            out[n++] = (char)tolower(c);
        } else if (c == '-' || c == '_' || c == ' ' || c == '.') {
            // Collapse separators to a single hyphen and never lead with one.
            if (n > 0 && out[n - 1] != '-') out[n++] = '-';
        }
        // Everything else is dropped rather than transliterated.
    }
    while (n > 0 && out[n - 1] == '-') n--;   // no trailing separator
    out[n] = '\0';

    if (n == 0)  return false;   // nothing usable survived
    if (n > 63)  { out[63] = '\0'; }  // DNS label ceiling
    return true;
}

} // namespace DeviceIdentity
