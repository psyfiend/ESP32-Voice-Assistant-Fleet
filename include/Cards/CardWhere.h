#pragma once
//
// CardWhere - where an entity already is on the screen, in the owner's words
// (K140, K145): what a picker row says instead of offering a tick.
//
//   "In this cluster" / "In the header"   a member of `self`, a tile / a header sensor
//   "In <cluster>"                        a member of another cluster ("In Outside")
//   "On the page"                         a card of its own on the page
//   "In <card>"                           a member of a group card on the page ("In Desk")
//
// Read from the page as built (the cards' surfaces), so it says what is on the
// glass. LVGL thread.
//
#include <stddef.h>

struct Entity;
class Card;
class EntityRegistry;

namespace CardWhere {

// False when the entity is nowhere on the screen. `self`: the cluster asking,
// or null (the page's Add card).
bool words(const Entity &e, const Card *self, const EntityRegistry *reg, char *out, size_t cap);

}  // namespace CardWhere
