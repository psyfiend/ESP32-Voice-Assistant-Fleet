#include "Cards/CardWhere.h"
#include "Cards/Card.h"
#include "Cards/ClusterCard.h"
#include "Cards/CardPopup.h"   // CARD_SURFACE_FLAG
#include "EntityRegistry.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

namespace {

struct Find {
    const Entity         *e;
    const Card           *self;
    const EntityRegistry *reg;
    char                  cluster[48];   // the first other cluster holding it
    char                  group[48];     // the first group card holding it
    bool                  self_, header, page;
};

bool same(const Entity *a, const Entity *b) { return a && b && !strcmp(a->desc.id, b->desc.id); }

void walk(lv_obj_t *o, Find &f) {
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (!lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) { walk(c, f); continue; }
        const Card *k = (const Card *)lv_obj_get_user_data(c);
        if (!k) continue;
        if (!strcmp(k->typeName(), "cluster")) {
            const ClusterCard *cl = static_cast<const ClusterCard *>(k);
            for (uint8_t m = 0; m < cl->memberCount(); m++) {
                const Card *mc = cl->memberAt(m);
                if (!mc || !same(mc->firstEntity(), f.e)) continue;
                if (k == f.self) { f.self_ = true; f.header |= cl->slotAt(m) >= 0; }
                else if (!f.cluster[0]) snprintf(f.cluster, sizeof(f.cluster), "%s", k->shownName(CardLabel::LBL_NAME));
            }
            continue;   // its tiles are its members, counted above
        }
        if (k->mini()) continue;
        // A summary of several (the garage's Doors): each of its entities is in it.
        if (k->entityCount() > 1) {
            for (uint8_t p = 0; p < k->entityCount() && !f.group[0]; p++)
                if (same(k->entityAt(p), f.e)) snprintf(f.group, sizeof(f.group), "%s", k->shownName(CardLabel::LBL_NAME));
            continue;
        }
        const Entity *ke = k->firstEntity();
        if (same(ke, f.e)) { f.page = true; continue; }
        // A group card: its entity's members (Desk's lamps).
        if (ke && f.reg && !f.group[0])
            for (uint8_t m = 0; m < ke->nMembers; m++)
                if (same(f.reg->memberOf(*ke, m), f.e)) {
                    snprintf(f.group, sizeof(f.group), "%s", k->shownName(CardLabel::LBL_NAME));
                    break;
                }
    }
}

}  // namespace

namespace CardWhere {

bool words(const Entity &e, const Card *self, const EntityRegistry *reg, char *out, size_t cap) {
    Find f{};
    f.e = &e;
    f.self = self;
    f.reg = reg;
    walk(lv_screen_active(), f);
    // K145's words, the most telling first.
    if (f.self_)          snprintf(out, cap, "%s", f.header ? "In the header" : "In this cluster");
    else if (f.page)      snprintf(out, cap, "On the page");
    else if (f.group[0])  snprintf(out, cap, "In %s", f.group);
    else if (f.cluster[0]) snprintf(out, cap, "In %s", f.cluster);
    else return false;
    return true;
}

}  // namespace CardWhere
