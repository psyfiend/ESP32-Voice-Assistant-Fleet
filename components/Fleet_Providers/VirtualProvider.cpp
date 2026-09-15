#include "VirtualProvider.h"
#include "VirtualEntities.h"

// Every entity here that is expected to ANSWER. test_stuck is deliberately
// absent: its command is left to expire, which is the failure the whole
// harness exists to show.
static const char *const ECHOERS[] = {
    VIRT_ENT_SWITCH, VIRT_ENT_L1, VIRT_ENT_L2, VIRT_ENT_L3, VIRT_ENT_L4,
};
static const uint8_t ECHOER_COUNT = sizeof(ECHOERS) / sizeof(ECHOERS[0]);

void VirtualProvider::begin(EntityRegistry *reg) {
    _reg = reg;
    if (!_reg) return;

    for (uint8_t i = 0; i < VIRTUAL_ENTITY_COUNT; i++) {
        _reg->add(VIRTUAL_ENTITIES[i]);
    }

    // All start off, and start SET rather than never-set. A switch that has
    // never had a value is a different thing from a switch that is off, and
    // only one of those is true here - we know perfectly well what state these
    // are in, because we are the device.
    for (uint8_t i = 0; i < VIRTUAL_ENTITY_COUNT; i++) {
        _reg->setValue(VIRTUAL_ENTITIES[i].id, EntityValue::makeBool(false), 0);
    }
}

void VirtualProvider::loop(uint32_t nowMs) {
    if (!_reg) return;

    for (uint8_t i = 0; i < ECHOER_COUNT; i++) {
        Entity *e = _reg->find(ECHOERS[i]);
        if (!e || !e->pending) continue;
        if (nowMs - e->pendingSinceMs < VIRT_ECHO_DELAY_MS) continue;

        // Echo back the value that was optimistically applied - which is what
        // a cooperative device does, and why the echo carries no new
        // information. setValue() finds nothing changed and declines to dirty
        // the entity; the one thing that matters is that it clears `pending`,
        // and that flag is what the card is watching. See Card.h.
        //
        // Copied rather than passed by reference: setValue() takes the lock and
        // then assigns into the very field the reference points at.
        const EntityValue echoed = e->value;
        _reg->setValue(ECHOERS[i], echoed, nowMs);
    }
}
