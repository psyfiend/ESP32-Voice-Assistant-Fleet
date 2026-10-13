// Properties (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

// --- PROPERTIES (owner, 2.10e round 2) ---------------------------------------
// What the entity is, read-only: HA's name and entity id, HA's own area (which
// can differ from the card's group), and what kind of thing it is. Reached
// from SETTINGS' "Entity properties" button - no corner icon, there is no room
// for another on the 4B (owner). Back goes to the controls.
Card *cardOf(lv_obj_t *sf);

void propRow(lv_obj_t *list, const char *what, const char *value) {
    lv_obj_t *r = plain(list);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, mm(UI::shape().S8), 0);
    lv_obj_t *a = Kit::label(r, KS::KS_LABEL_TAG_DIM);
    lv_label_set_text(a, what);
    lv_obj_t *b = Kit::label(r, KS::KS_LABEL_TAG);
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(b, (value && value[0]) ? value : "-");
}

void buildProps() {
    const Entity *e = inMember() ? s.ent[0] : s.native ? s.native : (s.nEnt == 1 ? s.ent[0] : nullptr);
    const UIShape &k = UI::shape();
    lv_obj_t *list = plain(s.stage);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(list, mm(k.S10), 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, mm(k.S5), 0);
    char buf[96];
    if (!e) {
        // A card standing for several entities made here: each one's id.
        snprintf(buf, sizeof(buf), "%u, made on this board", (unsigned)s.nEnt);
        propRow(list, "Entities", buf);
        for (uint8_t i = 0; i < s.nEnt; i++)
            if (s.ent[i]) propRow(list, s.ent[i]->desc.name,
                                  s.ent[i]->desc.externalRef[0] ? s.ent[i]->desc.externalRef : s.ent[i]->desc.id);
    } else {
        // The owner's order (2026-10-13): where it comes from, what it is
        // called and where it lives, then what kind of thing, then the card's.
        Card *c = inMember() ? nullptr : cardOf(h.surface);
        const bool ha = e->desc.source == EntitySource::HA;
        static const char *const SRC[] = { "This board (a sensor)", "This board", "MQTT", "Home Assistant", "Virtual" };
        propRow(list, "Source", SRC[(uint8_t)e->desc.source < 5 ? (uint8_t)e->desc.source : 0]);
        propRow(list, "Name in HA", ha ? (e->sourceName[0] ? e->sourceName : e->desc.name) : "");
        propRow(list, "Entity ID", e->desc.externalRef[0] ? e->desc.externalRef : e->desc.id);
        propRow(list, "Area in HA", ha ? (e->sourceArea[0] ? e->sourceArea : "None") : "");
        CardGroups::Group g;   // its group on this dashboard (K46)
        if (c) propRow(list, "Card group", CardGroups::ofCard(*c, g) ? g.name : "None");
        const bool mine = e->desc.source == EntitySource::SYSTEM || e->desc.source == EntitySource::LOCAL;
        propRow(list, "Device", ha ? (e->sourceDevice[0] ? e->sourceDevice : "None") : mine ? "This board" : "");
        propRow(list, "Device class", e->desc.deviceClass);
        propRow(list, "State class", e->desc.stateClass);
        propRow(list, "Unit", e->desc.unit);
        if (e->nMembers) { snprintf(buf, sizeof(buf), "%u", (unsigned)e->nMembers); propRow(list, "Members", buf); }
        if (c) propRow(list, "Card ID", c->hasId() ? c->id() : "");   // K33
    }
    // A card standing for several entities made here: its group and id after them.
    if (Card *c = (!e && !inMember()) ? cardOf(h.surface) : nullptr) {
        CardGroups::Group g;
        propRow(list, "Card group", CardGroups::ofCard(*c, g) ? g.name : "None");
        propRow(list, "Card ID", c->hasId() ? c->id() : "");
    }
    // ALL OF IT IN SIGHT where it can be (owner, round 6: the P4_5 cut "Card
    // id" in half): the gaps between the rows close up, as far as a third of
    // their size; past that the list scrolls, as before.
    lv_obj_update_layout(list);
    const uint32_t cnt = lv_obj_get_child_count(list);
    if (cnt > 1) {
        int32_t used = 0;
        for (uint32_t i = 0; i < cnt; i++) used += lv_obj_get_height(lv_obj_get_child(list, (int32_t)i));
        const int32_t fit = (lv_obj_get_content_height(list) - used) / (int32_t)(cnt - 1);
        if (fit < mm(k.S5)) lv_obj_set_style_pad_row(list, LV_MAX(fit, mm(k.S_ROW_MIN)), 0);
    }
}

} // namespace popup
