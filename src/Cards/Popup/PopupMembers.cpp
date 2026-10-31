// Members, and a member's own controls (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"
#include "Cards/ClusterCard.h"
#include "Cards/CardIcons.h"

namespace popup {

void memberRowCb(lv_event_t *ev);   // below, with the member view
void deckRender();                  // below, with the deck
void deckSync();                    // below: SETTINGS made again when its rows change
void chartSet(uint8_t state);       // below, with the deck: the CHART panel
void chartFold();                   // open: back to its tab
void sideFor(PopupView v);          // below: the left panel a view wants (CHART / SCENES)

// ITS STATE IN COLOUR (owner, the #99 mock): on in the active colour, off and
// any other value in white, paused or unavailable dimmed. Returns the role.
KS memberState(const Entity &e, MemberShown shown, char *buf, size_t cap) {
    const EntityAttrs &at = e.attrs;
    if (shown == MemberShown::SHOWN_HIDDEN || shown == MemberShown::SHOWN_MOVED) {
        // Moved out (the sprint after round 7): its card is on the page.
        snprintf(buf, cap, shown == MemberShown::SHOWN_MOVED ? "On the page" : "Hidden");
        return KS::KS_LABEL_TAG_DIM;
    }
    if (e.paused) {
        // Out of the group, but still listed and saying why (owner, 2.10b).
        snprintf(buf, cap, "Paused");
        return KS::KS_LABEL_TAG_DIM;
    }
    if (!e.available) {
        snprintf(buf, cap, "Unavailable");
        return KS::KS_LABEL_TAG_DIM;
    }
    if (e.desc.kind == EntityKind::LIGHT && e.value.type == ValueType::BOOL && e.value.b && at.brightness >= 0) {
        // A light's levels, so a group's commands can be checked member by
        // member (2.10b): "On, 40%, 2700 K", "On, 40%, colour 30".
        const int pct = LV_MAX(1, (at.brightness * 100 + 127) / 255);
        if (at.lightMode == LightMode::LMODE_TEMP && at.colorTempK > 0)
            snprintf(buf, cap, "On, %d%%, %d K", pct, at.colorTempK);
        else if (at.lightMode == LightMode::LMODE_COLOUR && at.hue >= 0)
            snprintf(buf, cap, "On, %d%%, color %d", pct, at.hue);
        else
            snprintf(buf, cap, "On, %d%%", pct);
        return KS::KS_LABEL_TAG_ACTIVE;
    }
    if (e.value.type == ValueType::BOOL) {
        snprintf(buf, cap, "%s", cardStateWord(e.desc, e.value.b));
        return e.value.b ? KS::KS_LABEL_TAG_ACTIVE : KS::KS_LABEL_TAG;
    }
    cardFormatValue(e, buf, cap, true, s.tempUnit);
    return KS::KS_LABEL_TAG;
}

// A member's row: its name, and its state beside it. The caller says what a tap
// does and gives it its finish once laid out.
lv_obj_t *memberRow(lv_obj_t *list, const char *name, const Entity &e, MemberShown shown,
                    const char *says) {
    lv_obj_t *r = Kit::item(list);
    lv_obj_set_size     (r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(r, mm(UI::shape().S5), 0);
    lv_obj_t *n = Kit::label(r, KS::KS_LABEL_NAME);
    lv_label_set_text(n, name);
    char buf[48];
    const KS role = says ? KS::KS_LABEL_TAG_DIM : memberState(e, shown, buf, sizeof(buf));
    lv_obj_t *v = Kit::label(r, role);
    lv_label_set_text(v, says ? says : buf);
    return r;
}

// THE HEADER'S GLYPH, SQUARE (owner, round 2 of the follow-ups): the one the
// cluster's header shows for this member, in its colour, `gap` from the row.
static void headerGlyph(lv_obj_t *col, const Entity &e, int32_t gap) {
    const UIPalette &p = UI::pal();
    lv_obj_t *g = plain(col);
    Kit::add(g, KS::KS_CLUSTER_GLYPH_SQUARE);
    lv_obj_align(g, LV_ALIGN_LEFT_MID, gap, 0);
    lv_obj_t *ic = Kit::label(g, KS::KS_CARD_ICON_SM);
    const bool binary = e.value.type == ValueType::BOOL;
    const bool on = binary && e.value.b;
    lv_label_set_text(ic, binary ? cardHeroIcon(e, on) : cardCornerIcon(e, false));
    uint32_t tint = binary ? (on ? p.ST_ACTIVE : p.TEXT_DIM) : cardTintFor(e.desc);
    if (!e.everSet || !e.available) tint = p.TEXT_DIM;
    lv_obj_set_style_text_color(ic, UI::c(tint), 0);
    lv_obj_center(ic);
}

void buildMembers() {
    // The stage's height, scrolling if a group has more members than fit. As
    // wide as the controls view's slider and words together, centred, so the
    // rows line up with them (owner, 2.10c G2: full width looked awkward on
    // the P4_5).
    // Centred vertically too (owner, round 4): as tall as its rows, never
    // taller than the stage, which centres it.
    lv_obj_t *list = plain(s.stage);
    const int32_t rowW = membersRowW();
    // A CLUSTER'S MEMBERS ARE ALL HERE, hidden ones too (round 6), each saying so.
    Card *held = cardOf(h.surface);
    const ClusterCard *cl = (held && s.cluster) ? static_cast<const ClusterCard *>(held) : nullptr;
    // IN THE HEADER, ITS GLYPH OUTSIDE THE ROW ON THE RIGHT (owner, round 2 of the
    // follow-ups): the rows as they were, centred, with a column each side as wide
    // as the glyph and its gap - only when one is in the header.
    bool anyHeader = false;
    for (uint8_t i = 0; cl && i < cl->memberCount(); i++) anyHeader |= clusterMemberInHeader(*cl, i);
    const int32_t gap = mm(UI::shape().S5);
    int32_t side = 0;
    if (anyHeader) {
        lv_obj_t *probe = plain(s.stage);
        Kit::add(probe, KS::KS_CLUSTER_GLYPH_SQUARE);
        side = lv_obj_get_style_width(probe, LV_PART_MAIN) + gap;
        lv_obj_delete(probe);
    }
    const int32_t avail = lv_area_get_width(&s.winRect) - 2 * s.pad;
    const int32_t rowsW = anyHeader ? LV_MIN(rowW > 0 ? rowW : avail, avail - 2 * side) : rowW;
    lv_obj_set_size     (list, anyHeader ? rowsW + 2 * side : rowW > 0 ? rowW : lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(list, lv_pct(100), 0);
    lv_obj_add_flag     (list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, mm(UI::shape().S5), 0);
    lv_obj_t *rows[POPUP_ENT_MAX] = {};
    uint8_t nRows = 0;
    if (cl) clusterMemberNames(*cl);
    for (uint8_t i = 0; i < memberCount(); i++) {
        const Entity *e = memberAt(i);
        if (!e) continue;
        // A tap opens that member's own controls (owner, 2.10b).
        const int8_t k = cl ? clusterMemberOf(*cl, e) : -1;
        const MemberShown shown = k >= 0 ? clusterMemberShown(*cl, (uint8_t)k) : MemberShown::SHOWN_TILE;
        // A cluster's member: its name as Header options gives it, whether its
        // sensor is in the header, and where it went if it moved out.
        char where[48];
        const char *says = (k >= 0 && shown == MemberShown::SHOWN_MOVED)
                               ? movedWords(cl->memberAt((uint8_t)k), where, sizeof(where)) : nullptr;
        lv_obj_t *holder = list;
        lv_obj_t *right = nullptr;
        if (anyHeader) {
            // [a column][the row][a column, holding the glyph when it is in the header]
            holder = plain(list);
            lv_obj_set_size     (holder, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(holder, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(holder, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_size(plain(holder), side, 1);
        }
        lv_obj_t *r = memberRow(holder, k >= 0 ? clusterMemberName((uint8_t)k) : e->desc.name, *e, shown, says);
        if (anyHeader) {
            lv_obj_set_width(r, rowsW);
            right = plain(holder);
            lv_obj_set_size(right, side, LV_SIZE_CONTENT);
            if (k >= 0 && clusterMemberInHeader(*cl, (uint8_t)k)) headerGlyph(right, *e, gap);
        }
        lv_obj_add_event_cb(r, memberRowCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        if (nRows < POPUP_ENT_MAX) rows[nRows++] = r;
    }
    lv_obj_t *note = Kit::label(list, KS::KS_LABEL_TAG_DIM);
    // Owner's words, 2026-10-07; a cluster's member is shown or hidden from its own view.
    lv_label_set_text(note, "Tap a member for details");   // the owner's words, round 14
    // SATIN (#99, K77): a row can be tapped and is not chosen. Painted at the
    // rows' laid-out size, which only the layout knows.
    lv_obj_update_layout(list);
    for (uint8_t i = 0; i < nRows; i++)
        Kit::finish(rows[i], lv_obj_get_width(rows[i]), lv_obj_get_height(rows[i]),
                    lv_obj_get_style_radius(rows[i], LV_PART_MAIN), Kit::Finish::FINISH_SATIN);
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
