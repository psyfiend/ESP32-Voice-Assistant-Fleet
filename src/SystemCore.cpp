#include "SystemCore.h"
#include <FleetI2C.h>
#include "ExternalEntities.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"   // esp_ptr_external_ram()
#include "bsp_loader.h"

bool SystemCore::begin() {
    // --= 1. I2C =--
    // This used to happen as a side effect inside DisplayManager::begin() -
    // the display simply happened to be the first thing that needed the bus.
    // Touch, the codecs and the IO expanders all need it too, so it is named
    // here rather than inferred. FleetI2C::begin() is idempotent by design, so
    // DisplayManager's own call is now a no-op and that component stays usable
    // on its own.
    FleetI2C::begin(bsp_hw.SDA_PIN, bsp_hw.SCL_PIN);

    // --= 2. Display =--
    // Must precede LVGL, which sizes its draw buffers from gfx->width() and
    // gfx->height().
    if (!_display.begin()) {
        Serial.println("[Core] Display init FAILED.");
        return false;
    }

    // --= 3. Touch =--
    // Needs the I2C bus, and on several boards needs the panel's reset line to
    // have been driven already - hence after the display, not before it.
    #ifndef DEBUG_SKIP_TOUCH
    _touch.begin();
    #else
    Serial.println("[Core] DEBUG_SKIP_TOUCH: skipping touch.begin() entirely.");
    #endif

    // --= 4. Audio =--
    // Codecs sit on the same I2C bus.
    #ifdef HAS_AUDIO_HW
    _audio.begin();
    #endif

    // --= 5. Network =--
    // Independent of the display. Sets the hostname before WiFi.mode(), an
    // ordering trap documented inside ConnectivityManager itself.
    _conn.begin();

    // --= 6. Broker session =--
    // Needs the link object. Does nothing until it reports online, and stays
    // cleanly disabled when no MqttLocalSecrets.h is present.
    _mqtt.begin(&_conn);

    // --= 7. Registry storage =--
    // Must precede every provider.
    beginEntityStorage();

    // --= 8. Providers =--
    // All three need the registry; two of them also need the broker.
    _sysProvider.begin(&_entities, &_conn);
    _haPub.begin(&_entities, &_mqtt);
    _mqttProv.begin(&_entities, &_mqtt);

    // --= 9. Externally-owned entities =--
    // Entities other devices own, which we only read. Temporary stand-in for
    // the build sheet (#20) - registered here so MqttProvider can derive its
    // subscriptions from the registry like any other entity. Must be after the
    // registry exists and after mqttProv.begin().
    for (uint8_t i = 0; i < EXTERNAL_ENTITY_COUNT; i++) {
        _entities.add(EXTERNAL_ENTITIES[i]);
    }

    return true;
}

// Entity storage lives in PSRAM, not internal SRAM.
//
// It used to be a fixed array inside EntityRegistry, which put ~21 KB in
// internal DRAM on every board. CYD_S3_3248 could not afford it: as the
// fleet's only QSPI panel it is the only board whose LVGL buffers must also
// be in internal SRAM, and the two together starved the WiFi driver badly
// enough that softAP() crashed inside ieee80211_hostap_attach.
//
// PSRAM also removes the ceiling: a build can size the registry for what it
// actually needs rather than for the worst case across the fleet.
void SystemCore::beginEntityStorage() {
    const size_t bytes = (size_t)ENTITY_MAX * sizeof(Entity);
    Entity *store = (Entity *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!store) {
        // Every board in the fleet has PSRAM, so this should not happen - but
        // falling back to internal RAM is better than a registry that silently
        // accepts nothing.
        Serial.println("[Entities] PSRAM alloc FAILED; falling back to internal RAM.");
        store = (Entity *)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL);
    }
    if (!_entities.begin(store, ENTITY_MAX)) {
        Serial.println("[Entities] Registry unavailable - no entities will register.");
    } else {
        Serial.printf("[Entities] Capacity %u (%u bytes in %s)\n",
                      (unsigned)ENTITY_MAX, (unsigned)bytes,
                      esp_ptr_external_ram(store) ? "PSRAM" : "internal RAM");
    }
}

void SystemCore::loop() {
    // Non-blocking: advances connect deadlines, retry escalation, AP fallback,
    // the AP idle timer and async scan collection. Must be called every loop -
    // ConnectivityManager deliberately never spins on WiFi.status() itself.
    _conn.loop();

    // Broker connect/backoff ladder plus PubSubClient's keepalive pump.
    // Non-blocking, and a no-op while the link is down or MQTT is disabled.
    _mqtt.loop();

    const uint32_t now = millis();
    _sysProvider.loop(now);
    // Expires stale values and reverts optimistic writes whose echo never
    // arrived. Cheap; safe from any task.
    _entities.tick(now);
    _haPub.loop(now);
    _mqttProv.loop(now);
}
