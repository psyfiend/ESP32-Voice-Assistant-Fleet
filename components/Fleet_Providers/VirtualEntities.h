#pragma once
#ifndef VIRTUAL_ENTITIES_H
#define VIRTUAL_ENTITIES_H

#include "Entity.h"

// ---------------------------------------------------------------------------
// TEMPORARY, and more obviously so than the other two tables beside it.
//
// These entities model nothing. They exist because milestone 2.4 has to prove
// that a tap works, and as of this milestone THERE IS NOT ONE WRITABLE ENTITY
// ANYWHERE IN THE FLEET - all eight are read-only telemetry and Zigbee sensor
// readings. EntityRegistry::commandValue() and its optimistic-write reconcile
// have been written, reviewed and never once executed.
//
// They go away when outbound commands (#44) land and a real light can be
// tapped. Until then this is the only way the command path gets exercised at
// all, and "coded, reviewed, and never run" is exactly the category that
// produced #42 the moment someone finally ran it.
//
//   source     = VIRTUAL  - not a real peripheral, not a topic. MqttProvider
//                           skips anything that is not EntitySource::MQTT, so
//                           these cost nothing on the wire.
//   advertise  = false    - publishing a switch to Home Assistant that we
//                           cannot actually action would put a broken entity
//                           in the owner's HA. The flag is the whole
//                           difference between ours and theirs; see
//                           ExternalEntities.h.
//   writable   = true     - the point.
//
// THE PAIR IS THE DESIGN. VirtualProvider echoes one of them back and
// deliberately never echoes the other, so both halves of the optimistic write
// are visible on glass at the same time: one card confirms and goes active,
// the other reverts after the reconcile window and goes loud. Rendering only
// the failure would have left the success path exactly as untested as it is
// today.
// ---------------------------------------------------------------------------

#define VIRT_ENT_SWITCH "test_switch"   // echoes. The command takes
#define VIRT_ENT_STUCK  "test_stuck"    // never echoes. The command is refused

// Four more well-behaved switches, added purely so the larger boards have
// enough cards to fill more than a row or two. WS_P4_7B derives SEVEN columns
// at 1024 px, and nine demo cards cannot show what four rows of grid look
// like. All four echo, like VIRT_ENT_SWITCH.
#define VIRT_ENT_L1     "test_lamp_1"
#define VIRT_ENT_L2     "test_lamp_2"
#define VIRT_ENT_L3     "test_lamp_3"
#define VIRT_ENT_L4     "test_lamp_4"

// How long the obedient one takes to answer. Long enough to actually see the
// optimistic value land before it is confirmed, short enough that it does not
// read as a fault. Well inside EntityRegistry's 5 s reconcile window.
#define VIRT_ECHO_DELAY_MS 800

inline const EntityDescriptor VIRTUAL_ENTITIES[] = {
    {
        .id          = VIRT_ENT_SWITCH,
        .name        = "Test Switch",
        .kind        = EntityKind::SWITCH,
        .source      = EntitySource::VIRTUAL,
        .valueType   = ValueType::BOOL,
        .icon        = "mdi:toggle-switch",
        .writable    = true,
        .diagnostic  = false,
        .advertise   = false,
        // 0 = never goes stale. Correct for something we command rather than
        // observe: a switch holds the value it was set to, and greying it out
        // after a quiet minute would be inventing a fault. Entity.h says this
        // in the staleAfterMs comment; it is the case that comment describes.
        .staleAfterMs = 0,
    },
    {
        .id          = VIRT_ENT_STUCK,
        .name        = "Stuck Switch",
        .kind        = EntityKind::SWITCH,
        .source      = EntitySource::VIRTUAL,
        .valueType   = ValueType::BOOL,
        .icon        = "mdi:toggle-off",   // ENTITY_SHORT_MAX is 20 incl. the null
        .writable    = true,
        .diagnostic  = false,
        .advertise   = false,
        .staleAfterMs = 0,
    },
    {
        .id          = VIRT_ENT_L1,
        .name        = "Lamp 1",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::VIRTUAL,
        .valueType   = ValueType::BOOL,
        .icon        = "mdi:lightbulb",
        .writable    = true,
        .diagnostic  = false,
        .advertise   = false,
        .staleAfterMs = 0,
    },
    {
        .id          = VIRT_ENT_L2,
        .name        = "Lamp 2",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::VIRTUAL,
        .valueType   = ValueType::BOOL,
        .icon        = "mdi:lightbulb",
        .writable    = true,
        .diagnostic  = false,
        .advertise   = false,
        .staleAfterMs = 0,
    },
    {
        .id          = VIRT_ENT_L3,
        .name        = "Lamp 3",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::VIRTUAL,
        .valueType   = ValueType::BOOL,
        .icon        = "mdi:lightbulb",
        .writable    = true,
        .diagnostic  = false,
        .advertise   = false,
        .staleAfterMs = 0,
    },
    {
        .id          = VIRT_ENT_L4,
        .name        = "Lamp 4",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::VIRTUAL,
        .valueType   = ValueType::BOOL,
        .icon        = "mdi:lightbulb",
        .writable    = true,
        .diagnostic  = false,
        .advertise   = false,
        .staleAfterMs = 0,
    },
};

inline constexpr uint8_t VIRTUAL_ENTITY_COUNT =
    sizeof(VIRTUAL_ENTITIES) / sizeof(VIRTUAL_ENTITIES[0]);

#endif // VIRTUAL_ENTITIES_H
