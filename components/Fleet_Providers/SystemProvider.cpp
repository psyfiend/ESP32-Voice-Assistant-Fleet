#include "SystemProvider.h"
#include "SystemEntities.h"

// ---------------------------------------------------------------------------
// The provider half: where the values come from.
//
// WHAT the entities are lives next door in SystemEntities.h, as a declarative
// table shaped like a BSP header. This file contains no descriptions, only
// polling - which is the split ROADMAP 4.1 means by "providers write".
// ---------------------------------------------------------------------------

void SystemProvider::begin(EntityRegistry *reg, ConnectivityManager *link) {
    if (_begun) return;
    _reg  = reg;
    _link = link;
    if (!_reg) return;

    // Registration is a loop over the table, not four hand-written blocks.
    // Adding a fifth entity means adding a row next door and nothing here.
    for (uint8_t i = 0; i < SYSTEM_ENTITY_COUNT; i++) {
        if (!_reg->add(SYSTEM_ENTITIES[i])) {
            // Only fails on a duplicate id or a full table, both of which are
            // programming errors rather than runtime conditions - so say so
            // rather than failing silently and leaving a card bound to nothing.
            Serial.printf("[SysProvider] FAILED to register \"%s\" "
                          "(duplicate id, or registry full at %u)\n",
                          SYSTEM_ENTITIES[i].id, (unsigned)ENTITY_MAX);
        }
    }

    _begun      = true;
    _lastPollMs = 0;   // poll immediately on the first loop()
    Serial.printf("[SysProvider] Registered %u entities.\n",
                  (unsigned)SYSTEM_ENTITY_COUNT);
}

void SystemProvider::loop(uint32_t nowMs) {
    if (!_begun || !_reg) return;
    if (_lastPollMs != 0 && (nowMs - _lastPollMs) < _intervalMs) return;
    _lastPollMs = nowMs;

    // Always available, link or no link.
    _reg->setValue(SYS_ENT_UPTIME, EntityValue::makeInt((int32_t)(nowMs / 1000)), nowMs);
    _reg->setValue(SYS_ENT_HEAP,   EntityValue::makeInt((int32_t)ESP.getFreeHeap()), nowMs);

    // RSSI and IP are meaningful only while associated. When offline they are
    // deliberately NOT written: the values age past staleAfterMs and the cards
    // grey out on their own. Writing a placeholder (0 dBm, "0.0.0.0") would be
    // the same class of lie this project has been bitten by three times - a
    // confident value that is not true.
    if (_link && _link->isOnline()) {
        _reg->setValue(SYS_ENT_RSSI, EntityValue::makeInt((int32_t)_link->getRssi()), nowMs);

        char ip[20];
        snprintf(ip, sizeof(ip), "%s", _link->getIP().toString().c_str());
        _reg->setValue(SYS_ENT_IP, EntityValue::makeText(ip), nowMs);
    }
}
