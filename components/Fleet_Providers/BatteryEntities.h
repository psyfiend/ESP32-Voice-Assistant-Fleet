#pragma once
#ifndef BATTERY_ENTITIES_H
#define BATTERY_ENTITIES_H

#include "Entity.h"

// ---------------------------------------------------------------------------
// What the Battery provider offers, declared as data - the SystemEntities.h
// shape, for the same reasons. Registered only on a board whose BSP declares a
// battery ADC (BoardHardware.BAT_DIV_X1000 != 0), so a card bound to these on
// any other board is skipped by the page like any unregistered entity.
// ---------------------------------------------------------------------------

// Ids. Frozen once a dashboard uses them.
#define BATT_ENT_PCT "sys_batt"
#define BATT_ENT_MV  "sys_batt_mv"

// Polled every 5 s. A reading that stops arriving greys the card out.
#define BATT_ENT_STALE_MS 30000

inline const EntityDescriptor BATTERY_ENTITIES[] = {
    {
        .id          = BATT_ENT_PCT,
        .name        = "Battery",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::SYSTEM,
        .valueType   = ValueType::INT,
        .unit        = "%",
        .deviceClass = "battery",
        .stateClass  = "measurement",
        .icon        = "mdi:battery",
        .writable    = false,
        .diagnostic  = true,
        .advertise   = true,
        .staleAfterMs = BATT_ENT_STALE_MS,
    },
    {
        // The raw evidence behind the percentage, kept so the curve and the
        // divider can be checked against a meter. mV rather than V: the card's
        // float format is one decimal, which would hide everything interesting.
        .id          = BATT_ENT_MV,
        .name        = "Battery Voltage",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::SYSTEM,
        .valueType   = ValueType::INT,
        .unit        = "mV",
        .deviceClass = "voltage",
        .stateClass  = "measurement",
        .icon        = "mdi:flash-triangle",
        .writable    = false,
        .diagnostic  = true,
        .advertise   = true,
        .staleAfterMs = BATT_ENT_STALE_MS,
    },
};

inline constexpr uint8_t BATTERY_ENTITY_COUNT =
    sizeof(BATTERY_ENTITIES) / sizeof(BATTERY_ENTITIES[0]);

#endif // BATTERY_ENTITIES_H
