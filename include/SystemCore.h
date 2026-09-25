#pragma once
//
// SystemCore - everything the device is, minus the screen.
//
// Owns every non-UI subsystem and starts them in one place, in an order whose
// constraints are written down rather than implied by line order (which is all
// setup() used to be). See docs/design/startup.md section 4.
//
// This file deliberately includes no LVGL header, and must not grow one. That
// property is what makes a GUI-less variant a build_src_filter line rather than
// a redesign - see docs/design/startup.md section 7.
//
#include <Arduino.h>
#include "BoardDisplay.h"   // DisplayManager or Fleet_Display, per board (2.9)
#include "TouchManager.h"
#include "ConnectivityManager.h"
#include "MqttManager.h"
#include "EntityRegistry.h"
#include "SystemProvider.h"
#include "HaPublisher.h"
#include "MqttProvider.h"
#include "VirtualProvider.h"
#include "HaClient.h"
#include "HaProvider.h"
#include "HaRest.h"
#include "CommandRouter.h"
#include "HttpServer.h"
#ifdef HAS_AUDIO_HW
#include "AudioManager.h"
#endif

class SystemCore {
public:
    // Internal-heap checkpoint. Prints what is free and what the last step
    // cost, so "where did the RAM go" is answered by the boot log instead of
    // by bisecting. Internal heap is what the WiFi driver and LWIP allocate
    // from, and on CYD_S3_3248 - the only QSPI board, so the only one whose
    // LVGL buffers must ALSO be internal - it is the scarcest thing on the
    // device. A board that boots with a few KB free cannot hold a socket, and
    // nothing in the log used to say so.
    static void heapMark(const char *stage);

    // Hardware, then the data layer. Returns false only when the display fails
    // to initialise - the one startup failure the previous code treated as
    // fatal. The caller decides what fatal means; SystemCore does not halt.
    bool begin();

    // Non-blocking pump for everything above. Call every loop().
    void loop();

    BoardDisplay        &display()  { return _display; }
    TouchManager        &touch()    { return _touch; }
    ConnectivityManager &conn()     { return _conn; }
    MqttManager         &mqtt()     { return _mqtt; }
    EntityRegistry      &entities() { return _entities; }
    HaClient            &ha()       { return _ha; }
    HttpServer          &http()     { return _http; }
#ifdef HAS_AUDIO_HW
    AudioManager        &audio()    { return _audio; }
#endif

private:
    void printIdentity();

    void beginEntityStorage();

    // Issue #49. Turns MqttManager's read-only verdict into a call on
    // ConnectivityManager, which neither class may make for itself. See the
    // long note at the call site in loop().
    void mqttEvidence();

    BoardDisplay        _display;
    TouchManager        _touch;
#ifdef HAS_AUDIO_HW
    AudioManager        _audio;
#endif
    ConnectivityManager _conn;
    MqttManager         _mqtt;

    // The registry every card will bind to and HA discovery is generated from.
    // Populated by providers; see ROADMAP 4.1.
    EntityRegistry      _entities;

    // This board's own telemetry into the registry. Writes values only - never
    // renders, never publishes.
    SystemProvider      _sysProvider;

    // Announces the entities we own to Home Assistant and publishes their
    // values. Ignores anything with advertise = false.
    HaPublisher         _haPub;

    // Reads values other devices publish, deriving its subscriptions from
    // whatever external entities are registered.
    MqttProvider        _mqttProv;

    // TEMPORARY, with #44. The fleet has no writable entity of any kind, so
    // the registry's optimistic-write path has never once executed on
    // hardware. This provides two switches to tap - one that answers and one
    // that does not. See VirtualEntities.h.
    VirtualProvider     _virtProv;

    // The Home Assistant websocket session (#43). Owns the socket and the auth
    // handshake only - it knows nothing about areas, entities or cards.
    //
    // THE FIRST COMPONENT IN THIS PROJECT THAT RUNS ON ITS OWN TASK. Its
    // receive path is called by esp_websocket_client's task, not by loop(), so
    // it must never touch LVGL and writes through EntityRegistry's mutex
    // instead. That rule has always been in CLAUDE.md for providers; this is
    // the first one that could actually break it. See HaClient.h.
    HaClient            _ha;

    // One-shot initial values over REST, because HA's websocket has NO
    // per-entity state call - get_states is 787 KB on the owner's instance.
    // Without this the dashboard comes up blank and fills in over hours as
    // things happen to change, since subscribe_trigger only fires on change.
    // See HaRest.h.
    HaRest              _haRest;

    // Reads HA's entities off that session into the registry. The websocket
    // twin of MqttProvider, and like it, derives its subscription from the
    // entity table rather than a hardcoded list. Also owns the decision that a
    // new session has begun, and tells _haRest to re-fetch when it does.
    HaProvider          _haProv;

    // The outbound leg (#44). The ONLY code that knows a command can travel
    // two ways - the registry stays transport-agnostic, per ROADMAP 4.1.
    CommandRouter       _cmdRouter;

    // The device's one HTTP server (#58; Phase 4's web config joins it). Owned
    // here because it is a network service, but it serves nothing of its own:
    // routes are registered by whoever owns them, and it only starts once
    // somebody has. See HttpServer.h.
    HttpServer          _http;
};
