#include "VirtualProvider.h"
#include "VirtualEntities.h"

void VirtualProvider::begin(EntityRegistry *reg) {
    _reg = reg;
    if (!_reg) return;

    for (uint8_t i = 0; i < VIRTUAL_ENTITY_COUNT; i++) {
        _reg->add(VIRTUAL_ENTITIES[i]);
    }

    // Both start off, and start SET rather than never-set. A switch that has
    // never had a value is a different thing from a switch that is off, and
    // only one of those is true here - we know perfectly well what state these
    // are in, because we are the device.
    _reg->setValue(VIRT_ENT_SWITCH, EntityValue::makeBool(false), 0);
    _reg->setValue(VIRT_ENT_STUCK,  EntityValue::makeBool(false), 0);
}

void VirtualProvider::loop(uint32_t nowMs) {
    if (!_reg) return;

    Entity *e = _reg->find(VIRT_ENT_SWITCH);
    if (!e || !e->pending) return;
    if (nowMs - e->pendingSinceMs < VIRT_ECHO_DELAY_MS) return;

    // Echo back the value that was optimistically applied - which is what a
    // cooperative device does, and which is why the echo carries no new
    // information. setValue() will find nothing changed and decline to dirty
    // the entity; the ONE thing it does that matters here is clear `pending`,
    // and that flag is what the card is actually watching. See Card.h.
    // Copied rather than passed by reference: setValue() takes the lock and
    // then assigns into the very field the reference points at. It is a POD
    // and self-assignment would be harmless, but relying on that is the kind
    // of thing that stops being true when someone adds a member.
    const EntityValue echoed = e->value;
    _reg->setValue(VIRT_ENT_SWITCH, echoed, nowMs);

    // test_stuck is deliberately absent from this function. Its command is
    // left to expire, which is the failure this whole harness exists to show.
}
