// Members, and a member's own controls (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

void memberRowCb(lv_event_t *ev);   // below, with the member view
void deckRender();                  // below, with the deck
void deckSync();                    // below: SETTINGS made again when its rows change
void chartSet(uint8_t state);       // below, with the deck: the CHART panel
void chartFold();                   // open: back to its tab
void sideFor(PopupView v);          // below: the left panel a view wants (CHART / SCENES)

void buildMembers() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    // The stage's height, scrolling if a group has more members than fit. As
    // wide as the controls view's slider and words together, centred, so the
    // rows line up with them (owner, 2.10c G2: full width looked awkward on
    // the P4_5).
    // Centred vertically too (owner, round 4): as tall as its rows, never
    // taller than the stage, which centres it.
    lv_obj_t *list = plain(s.stage);
    lv_obj_set_size     (list, s.mainRowW > 0 ? s.mainRowW : lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(list, lv_pct(100), 0);
    lv_obj_add_flag     (list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, mm(UI::shape().S_MEMBERS), 0);
    for (uint8_t i = 0; i < memberCount(); i++) {
        const Entity *e = memberAt(i);
        if (!e) continue;
        lv_obj_t *r = plain(list);
        lv_obj_set_size     (r, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(r, mm(UI::shape().S_MEMBERS), 0);
        lv_obj_set_style_radius (r, UI::sc(UI::met().RADIUS), 0);
        lv_obj_set_style_bg_color(r, UI::c(p.SURFACE), 0);
        lv_obj_set_style_bg_opa (r, LV_OPA_COVER, 0);
        // A tap opens that member's own controls (owner, 2.10b).
        lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(r, UI::c(UI::mix(p.SURFACE, p.TEXT, 15)),
                                  UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
        lv_obj_add_event_cb(r, memberRowCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *n = makeLabel(r, t.NAME, p.TEXT);
        lv_label_set_text(n, e->desc.name);
        lv_obj_t *v = makeLabel(r, t.TAG, p.TEXT_DIM);
        char buf[48];
        const EntityAttrs &at = e->attrs;
        if (e->paused) {
            // Out of the group, but still listed and saying why (owner, 2.10b).
            lv_label_set_text(v, "Paused");
        } else if (!e->available) {
            lv_label_set_text(v, "Unavailable");
        } else if (e->desc.kind == EntityKind::LIGHT && e->value.type == ValueType::BOOL && e->value.b &&
                   at.brightness >= 0) {
            // A light's levels, so a group's commands can be checked member by
            // member (2.10b): "On, 40%, 2700 K", "On, 40%, colour 30".
            const int pct = LV_MAX(1, (at.brightness * 100 + 127) / 255);
            if (at.lightMode == LightMode::LMODE_TEMP && at.colorTempK > 0)
                snprintf(buf, sizeof(buf), "On, %d%%, %d K", pct, at.colorTempK);
            else if (at.lightMode == LightMode::LMODE_COLOUR && at.hue >= 0)
                snprintf(buf, sizeof(buf), "On, %d%%, color %d", pct, at.hue);
            else
                snprintf(buf, sizeof(buf), "On, %d%%", pct);
            lv_label_set_text(v, buf);
        } else if (e->value.type == ValueType::BOOL) {
            lv_label_set_text(v, cardStateWord(e->desc, e->value.b));
        } else {
            cardFormatValue(*e, buf, sizeof(buf), true, s.tempUnit);
            lv_label_set_text(v, buf);
        }
    }
    lv_obj_t *note = makeLabel(list, t.TAG, p.TEXT_DIM);
    lv_label_set_text(note, "Tap a member for more details");   // owner's words, 2026-10-07
}

// --- A MEMBER'S OWN CONTROLS (2.10b; owner: tap a row in Members) -----------
// The same window and the same views, pointed at one member: the group's
// entities are set aside, and the back arrow puts them back and returns to
// Members. It needs no card of its own, so Lamp 4 works too.
uint8_t s_memberPick = 0;
uint8_t s_groupCtl   = 0;   // the group's control, restored on the way back

void enterMemberAsync(void *unused) {
    (void)unused;
    if (s.phase != PopupPhase::PHASE_OPEN || s.view != PopupView::VIEW_MEMBERS || inMember()) return;
    const Entity *m = (s_memberPick < memberCount()) ? memberAt(s_memberPick) : nullptr;
    if (!m) return;
    for (uint8_t i = 0; i < POPUP_ENT_MAX; i++) s.groupEnt[i] = s.ent[i];
    s.groupN = s.nEnt;
    s_groupCtl = s.lightCtl;
    for (uint8_t i = 0; i < POPUP_ENT_MAX; i++) s.ent[i] = nullptr;
    s.ent[0] = m;
    s.nEnt   = 1;
    s.lightCtl = firstCtl(lightAggregate().caps);
    showView(PopupView::VIEW_MEMBER);
}

// A row in Members. Deferred: the row is in the stage that is about to be
// cleaned, and an event callback must not delete its own object.
void memberRowCb(lv_event_t *ev) {
    s_memberPick = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    lv_async_call(enterMemberAsync, nullptr);
}

// Out of a member, to the group's Members (the back arrow) or its controls
// (the "Desk" in the title).
void leaveMember(PopupView to) {
    if (!inMember()) return;
    for (uint8_t i = 0; i < POPUP_ENT_MAX; i++) s.ent[i] = s.groupEnt[i];
    s.nEnt     = s.groupN;
    s.groupN   = 0;
    applyNative();   // the member may have been paused or resumed meanwhile
    s.view     = PopupView::VIEW_MEMBERS;   // not Scenes: its slider swap is the member's
    s.lightCtl = s_groupCtl;
    showView(to);
}

} // namespace popup
