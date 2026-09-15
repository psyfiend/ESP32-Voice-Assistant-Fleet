#pragma once
#ifndef VIRTUAL_PROVIDER_H
#define VIRTUAL_PROVIDER_H

#include "EntityRegistry.h"

// ---------------------------------------------------------------------------
// VirtualProvider - the other half of VirtualEntities.h, and just as temporary.
//
// It is a provider in the ordinary sense: it writes values into the registry
// and never touches LVGL. What makes it a test harness rather than a peripheral
// driver is WHAT it decides to write - it plays the part of a device on the
// far end of a command, and it plays it badly on purpose.
//
//   test_switch  echoes the commanded value back after VIRT_ECHO_DELAY_MS,
//                which is exactly what a real light does through the broker
//                and back. The card's command resolves as confirmed.
//   test_stuck   is never touched at all. EntityRegistry::tick() reaches its
//                reconcile deadline, reverts the optimistic value and dirties
//                the entity, and the card renders that as ST_REFUSED.
//
// Goes away with VirtualEntities.h when #44 lands.
// ---------------------------------------------------------------------------

class VirtualProvider {
public:
    void begin(EntityRegistry *reg);
    void loop(uint32_t nowMs);

private:
    EntityRegistry *_reg = nullptr;
};

#endif // VIRTUAL_PROVIDER_H
