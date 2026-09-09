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
#include "DisplayManager.h"
#include "TouchManager.h"
#include "ConnectivityManager.h"
#include "MqttManager.h"
#include "EntityRegistry.h"
#include "SystemProvider.h"
#include "HaPublisher.h"
#include "MqttProvider.h"
#ifdef HAS_AUDIO_HW
#include "AudioManager.h"
#endif

class SystemCore {
public:
    // Hardware, then the data layer. Returns false only when the display fails
    // to initialise - the one startup failure the previous code treated as
    // fatal. The caller decides what fatal means; SystemCore does not halt.
    bool begin();

    // Non-blocking pump for everything above. Call every loop().
    void loop();

    DisplayManager      &display()  { return _display; }
    TouchManager        &touch()    { return _touch; }
    ConnectivityManager &conn()     { return _conn; }
    MqttManager         &mqtt()     { return _mqtt; }
    EntityRegistry      &entities() { return _entities; }
#ifdef HAS_AUDIO_HW
    AudioManager        &audio()    { return _audio; }
#endif

private:
    void beginEntityStorage();

    DisplayManager      _display;
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
};
