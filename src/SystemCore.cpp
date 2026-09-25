#include "SystemCore.h"
#include <FleetI2C.h>
#include "ExternalEntities.h"
#include "ExternalEntities_HA.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"   // esp_ptr_external_ram()
#include "SystemReport.h"   // fmtBytes - one memory-reporting convention
#include "bsp_loader.h"

// Normally injected by scripts/fw_version.py via extra_scripts (derived from
// `git describe`). Defined defensively here so a build still succeeds if that
// hook is ever skipped - a missing version string must never break the build,
// it just becomes unknown. See docs/ROADMAP.md section 3.3.
#include "fleet_fw_version.h"
#ifndef FW_VERSION
    #define FW_VERSION "unknown"
#endif
#ifndef FW_COMMIT
    #define FW_COMMIT "unknown"
#endif

// Who and what this board is. Every line here is BSP identity or chip
// inventory - none of it is about the display, which is why it no longer opens
// DisplayManager::begin(). Printed first so a pasted boot log always says which
// build, on which board, before anything can go wrong.
void SystemCore::printIdentity() {
    Serial.println();
    Serial.printf("Firmware: v%s (%s)\n", FW_VERSION, FW_COMMIT);
    Serial.printf("Device init: %s\n", bsp_hw.device_name);
    Serial.printf("Display hardware: %s\n", bsp_display.PANEL_MODEL);
    Serial.printf("Touch panel: %s\n", bsp_touch.NAME);
    char b[48];
    Serial.printf("PSRAM: %s\n", SystemReport::fmtBytes(ESP.getPsramSize(), b, sizeof(b)));
    if (ESP.getPsramSize() == 0) {
        Serial.println("CRITICAL ERROR: PSRAM not found! Display will fail.");
    }
    Serial.printf("Flash: %s\n", SystemReport::fmtBytes(ESP.getFlashChipSize(), b, sizeof(b)));

    // THE NUMBER THAT DECIDES WHETHER THE NETWORK WORKS.
    //
    // Internal heap is what the WiFi driver and LWIP take socket buffers from,
    // and it is the first thing a large static array eats. WS_P4_5 spent an
    // evening unable to hold a TCP connection - MQTT timing out at 60 s, every
    // reconnect failing raw=-2, unpingable - with 10,928 bytes free, and
    // nothing in the boot log said so. Printed here so that next time it is
    // the first thing anyone sees.
    const uint32_t internalFree = ESP.getFreeHeap();
    Serial.printf("Internal heap free: %s%s\n",
                  SystemReport::fmtBytes(internalFree, b, sizeof(b)),
                  internalFree < 40000 ? "   *** LOW - expect network failures ***" : "");

    Serial.println("------------------------------");
}

void SystemCore::heapMark(const char *stage) {
    static uint32_t last = 0;
    const uint32_t now = ESP.getFreeHeap();
    char a[32], d[32];
    SystemReport::fmtBytes(now, a, sizeof(a));
    if (last) {
        const int32_t delta = (int32_t)now - (int32_t)last;
        SystemReport::fmtBytes((uint32_t)(delta < 0 ? -delta : delta), d, sizeof(d));
        Serial.printf("[Heap] %-22s %10s free  (%s%s)\n",
                      stage, a, delta < 0 ? "-" : "+", d);
    } else {
        Serial.printf("[Heap] %-22s %10s free\n", stage, a);
    }
    last = now;
}

bool SystemCore::begin() {
    // --= 0. Identity =--
    printIdentity();

    // --= 1. I2C =--
    // This used to happen as a side effect inside DisplayManager::begin() -
    // the display simply happened to be the first thing that needed the bus.
    // Touch, the codecs and the IO expanders all need it too, so it is named
    // here rather than inferred. FleetI2C::begin() is idempotent by design, so
    // DisplayManager's own call is now a no-op and that component stays usable
    // on its own.
    FleetI2C::begin(bsp_hw.SDA_PIN, bsp_hw.SCL_PIN);
    heapMark("after I2C");

    // --= 2. Display =--
    // Must precede LVGL, which sizes its draw buffers from gfx->width() and
    // gfx->height().
    if (!_display.begin()) {
    heapMark("after display");
        Serial.println("[Core] Display init FAILED.");
        return false;
    }

    // --= 3. Touch =--
    // Needs the I2C bus, and on several boards needs the panel's reset line to
    // have been driven already - hence after the display, not before it.
    #ifndef DEBUG_SKIP_TOUCH
    _touch.begin();
    heapMark("after touch");
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
    heapMark("after wifi");

    // --= 6. Broker session =--
    // Needs the link object. Does nothing until it reports online, and stays
    // cleanly disabled when no MqttLocalSecrets.h is present.
    _mqtt.begin(&_conn);
    heapMark("after mqtt");

    // --= 7. Registry storage =--
    // Must precede every provider.
    beginEntityStorage();
    heapMark("after registry");

    // --= 8. Providers =--
    // All three need the registry; two of them also need the broker.
    _sysProvider.begin(&_entities, &_conn);
    _haPub.setSwVersion(FW_VERSION);   // see HaPublisher::setSwVersion
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

    // The owner's 18 Home Assistant entities (#43). Same standing as the MQTT
    // block above - someone else owns them, we only read - and registered in
    // the same place for the same reason: HaProvider derives its subscription
    // from the registry, so an entity that is not registered by now is an
    // entity that will not be subscribed to.
    //
    // Note two of these are the SAME PHYSICAL SENSOR as the deck entries
    // above, deliberately, to compare the two transports. See the header.
    for (uint8_t i = 0; i < HA_ENTITY_COUNT; i++) {
        _entities.add(HA_ENTITIES[i]);
    }

    // --= 10. Virtual test entities =--
    // TEMPORARY scaffolding for milestone 2.4, removed with #44. Registers two
    // writable switches so the command path has something to command - as of
    // this milestone nothing else in the fleet is writable at all, and
    // commandValue() has never run on hardware. Registered last because
    // nothing else depends on it.
    _virtProv.begin(&_entities);

    // --= 11. Home Assistant websocket session =--
    //
    // LAST ON PURPOSE, and for a reason that will matter more than it does
    // today. Right now HaClient only owns the socket, so it could start
    // anywhere after the link and the registry exist. But #43's next piece
    // subscribes to exactly the entities the registry holds - one
    // subscribe_trigger naming all of them - so the session must not open
    // before the registry is fully populated, or the subscription is built
    // from a half-filled list and silently misses whatever registered late.
    //
    // Starting it here costs nothing: begin() does not connect. It allocates
    // the reassembly buffer and moves to LINK_IDLE, and the first connection
    // attempt happens on the first loop() where Fleet_Connectivity reports
    // online. A board with no network boots exactly as fast as before.
    _ha.begin(&_conn, &_entities);

    // Registers itself as the session's message handler and, once a session is
    // up, issues ONE subscribe_trigger built from the registry. After
    // _ha.begin() so the handler is attached to a live object, though
    // setMessageHandler only stores a pointer and the order is not load-bearing.
    _haRest.begin(&_entities);
    _haProv.begin(&_entities, &_ha, &_haRest);

    // --= 12. Outbound commands =--
    //
    // Registers itself as the registry's command sink, so a card tap becomes a
    // call_service or an MQTT publish. Before this, commandValue() applied a
    // value optimistically and nothing ever transmitted it - #44 was the half
    // of the command path that had never been written.
    //
    // After the transports exist and before the pause restore, which only
    // touches flags.
    _cmdRouter.begin(&_entities, &_mqtt, &_ha);

    // --= 13. Restore the user's pauses =--
    //
    // LAST, and it has to be: restorePaused() marks entities by id, and an id
    // that is not in the table yet cannot be marked. Every provider above has
    // now registered, so the table is complete.
    //
    // Issue #60 - "if I want a card paused I do not want a reboot to unpause
    // it." A pause that survived the user but not the power cut would be worse
    // than no pause at all.
    _entities.restorePaused();

    heapMark("core ready");

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
        char b[48];
        Serial.printf("[Entities] Capacity %u entities, %s, in %s\n",
                      (unsigned)ENTITY_MAX,
                      SystemReport::fmtBytes(bytes, b, sizeof(b)),
                      esp_ptr_external_ram(store) ? "PSRAM" : "internal RAM");
    }
}

void SystemCore::mqttEvidence() {
    static uint8_t  lastEnvFails  = 0;
    static uint32_t lastRefreshMs = 0;

    const uint32_t now       = millis();
    const bool     connected = _mqtt.isConnected();
    const uint8_t  envFails  = _mqtt.consecutiveEnvFailures();

    // A HELD broker session is continuous evidence, not a one-off event, and
    // that distinction is what keeps the gateway probe asleep.
    //
    // It is real evidence rather than a convenient assumption: PubSubClient
    // sends PINGREQ every keepalive interval and tears the session down if no
    // PINGRESP comes back - which is precisely how the original fault
    // announced itself ("state=-4 after 6080715 ms"). So a session that is
    // still up has completed a round trip to another host recently, which is
    // exactly the question the probe would have asked, already answered by
    // traffic the board was sending anyway.
    if (connected && (lastRefreshMs == 0 || (now - lastRefreshMs) >= 15000)) {
        lastRefreshMs = now;
        _conn.noteRemoteReachable();
    }
    if (!connected) lastRefreshMs = 0;

    // Edge-triggered, not level-triggered. escalate() increments the count once
    // per attempt and the backoff ladder stretches to minutes, but loop() runs
    // thousands of times in between - reporting every pass would inflate one
    // failure into thousands and convict the link instantly. Only a CHANGE is
    // news.
    if (envFails > lastEnvFails) _conn.noteRemoteUnreachable();
    lastEnvFails = envFails;
}

void SystemCore::loop() {
    // Non-blocking: advances connect deadlines, retry escalation, AP fallback,
    // the AP idle timer and async scan collection. Must be called every loop -
    // ConnectivityManager deliberately never spins on WiFi.status() itself.
    _conn.loop();

    // Broker connect/backoff ladder plus PubSubClient's keepalive pump.
    // Non-blocking, and a no-op while the link is down or MQTT is disabled.
    _mqtt.loop();

    // --- Issue #49: MQTT's verdict becomes evidence about the LINK ---------
    //
    // The joint lives here because SystemCore is the only thing that owns both
    // objects. Fleet_MQTT must not know what carries it and Fleet_Connectivity
    // must not know a broker exists (ROADMAP Q9), so neither can make this
    // call; the owner of both can, and that is exactly the "hardware is owned
    // by SystemCore and borrowed by everything above it" rule from
    // docs/design/startup.md pointed at a new problem.
    //
    // During the original fault the device printed "broker unreachable" every
    // few seconds for six hours while ConnectivityManager printed nothing at
    // all. This is the wire that was missing.
    mqttEvidence();

    const uint32_t now = millis();
    _sysProvider.loop(now);
    // Expires stale values and reverts optimistic writes whose echo never
    // arrived. Cheap; safe from any task.
    _entities.tick(now);
    _haPub.loop(now);
    _mqttProv.loop(now);
    _virtProv.loop(now);

    // The HA session's LOOP-TASK half: it drives connect and backoff, sends
    // what the websocket task deferred, and enforces the handshake timeout.
    // Receiving does not happen here - that runs on the websocket task and
    // reaches us through EntityRegistry's mutex. See the long note in
    // HaClient.h; this is the project's first provider where loop() is not the
    // whole story.
    _ha.loop(now);
    _haProv.loop(now);
    _haRest.loop(now);

    // Starts the HTTP server the first time the link is up, if any route was
    // registered; a no-op every call after that.
    _http.loop(_conn.isOnline());
}
