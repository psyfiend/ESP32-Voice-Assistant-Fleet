#include "SystemProvider.h"

namespace {

// Small helper so each registration below reads as data rather than as ten
// lines of field assignment.
EntityDescriptor makeDesc(const char *id, const char *name,
                          EntityKind kind, ValueType vt,
                          const char *unit, const char *deviceClass,
                          const char *stateClass, const char *icon,
                          uint32_t staleAfterMs) {
    EntityDescriptor d;
    snprintf(d.id,          sizeof(d.id),          "%s", id);
    snprintf(d.name,        sizeof(d.name),        "%s", name);
    snprintf(d.unit,        sizeof(d.unit),        "%s", unit ? unit : "");
    snprintf(d.deviceClass, sizeof(d.deviceClass), "%s", deviceClass ? deviceClass : "");
    snprintf(d.stateClass,  sizeof(d.stateClass),  "%s", stateClass ? stateClass : "");
    snprintf(d.icon,        sizeof(d.icon),        "%s", icon ? icon : "");
    d.kind         = kind;
    d.source       = EntitySource::SYSTEM;
    d.valueType    = vt;
    d.writable     = false;   // telemetry: nothing here is commandable
    d.advertise    = true;    // we own these
    d.diagnostic   = true;    // panel health, not room state
    d.staleAfterMs = staleAfterMs;
    return d;
}

} // namespace

void SystemProvider::begin(EntityRegistry *reg, ConnectivityManager *link) {
    if (_begun) return;
    _reg  = reg;
    _link = link;
    if (!_reg) return;

    // Stale window is 3x the poll interval: long enough that one missed poll is
    // not an alarm, short enough that a wedged provider greys the cards out
    // instead of leaving a confident number on screen forever.
    const uint32_t stale = _intervalMs * 3;

    // RSSI. device_class signal_strength + state_class measurement is what
    // makes HA graph it and keep long-term statistics; without state_class it
    // is just a number that HA forgets.
    _reg->add(makeDesc(ID_RSSI, "WiFi Signal", EntityKind::SENSOR, ValueType::INT,
                       "dBm", "signal_strength", "measurement", "mdi:wifi", stale));

    // IP address. A text entity, and deliberately not given a device_class -
    // HA has none that fits, and inventing one makes the entity behave oddly.
    _reg->add(makeDesc(ID_IP, "IP Address", EntityKind::TEXT, ValueType::TEXT_VAL,
                       "", "", "", "mdi:ip-network", stale));

    // Uptime in seconds. NOT state_class total_increasing, which sounds right
    // and is wrong: a reboot resets this to zero, and total_increasing would
    // make HA interpret that as a counter rollover and log a spurious jump.
    _reg->add(makeDesc(ID_UPTIME, "Uptime", EntityKind::SENSOR, ValueType::INT,
                       "s", "duration", "", "mdi:timer-outline", stale));

    // Free heap in bytes. HA's data_size device class formats this into kB/MB
    // on its own, so the raw byte count is the honest thing to publish.
    _reg->add(makeDesc(ID_HEAP, "Free Heap", EntityKind::SENSOR, ValueType::INT,
                       "B", "data_size", "measurement", "mdi:memory", stale));

    _begun     = true;
    _lastPollMs = 0;   // poll immediately on the first loop()
}

void SystemProvider::loop(uint32_t nowMs) {
    if (!_begun || !_reg) return;
    if (_lastPollMs != 0 && (nowMs - _lastPollMs) < _intervalMs) return;
    _lastPollMs = nowMs;

    // Always available, link or no link.
    _reg->setValue(ID_UPTIME, EntityValue::makeInt((int32_t)(nowMs / 1000)), nowMs);
    _reg->setValue(ID_HEAP,   EntityValue::makeInt((int32_t)ESP.getFreeHeap()), nowMs);

    // RSSI and IP are meaningful only while associated. When offline they are
    // deliberately NOT written: the values then age past staleAfterMs and the
    // cards grey out on their own. Writing a placeholder (0 dBm, "0.0.0.0")
    // would be the same class of lie this project has been bitten by three
    // times - a confident value that is not true.
    if (_link && _link->isOnline()) {
        _reg->setValue(ID_RSSI, EntityValue::makeInt((int32_t)_link->getRssi()), nowMs);

        char ip[20];
        snprintf(ip, sizeof(ip), "%s", _link->getIP().toString().c_str());
        _reg->setValue(ID_IP, EntityValue::makeText(ip), nowMs);
    }
}
