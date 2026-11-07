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
    if (shown == MemberShown::SHOWN_MOVED) {
        // Moved out (the sprint after round 7): its card is on the page. An
        // unpinned member says its state like any other (K160).
        snprintf(buf, cap, "On the page");
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
const char *headerGlyphOf(const Entity &e, uint32_t &tint) { return cardHeaderGlyph(e, tint); }   // CardIcons

static void headerGlyph(lv_obj_t *col, const Entity &e, int32_t gap) {
    lv_obj_t *g = plain(col);
    Kit::add(g, KS::KS_CLUSTER_GLYPH_SQUARE);
    lv_obj_align(g, LV_ALIGN_LEFT_MID, gap, 0);
    lv_obj_t *ic = Kit::label(g, KS::KS_CARD_ICON_SM);
    uint32_t tint = 0;
    lv_label_set_text(ic, headerGlyphOf(e, tint));
    lv_obj_set_style_text_color(ic, UI::c(tint), 0);
    lv_obj_center(ic);
}

// PINNED, A PIN IN THE SAME SQUARE (K160): a tile on the page. A header member
// shows its own glyph instead; one not on the page, nothing.
void pinGlyph(lv_obj_t *col, int32_t gap) {
    lv_obj_t *g = plain(col);
    Kit::add(g, KS::KS_CLUSTER_GLYPH_SQUARE);
    lv_obj_align(g, LV_ALIGN_LEFT_MID, gap, 0);
    lv_obj_t *ic = Kit::label(g, KS::KS_CARD_ICON_SM);
    lv_label_set_text(ic, MDI_PIN);
    lv_obj_set_style_text_color(ic, UI::c(UI::pal().ACCENT), 0);   // K178
    lv_obj_center(ic);
}

// Whatever says a member is on the page (K160), `gap` from its row: its header
// glyph, a pin, or nothing.
void pageSign(lv_obj_t *col, const ClusterCard &cl, uint8_t i, const Entity &e, int32_t gap) {
    if (clusterMemberInHeader(cl, i))                          headerGlyph(col, e, gap);
    // A GROUP SHOWN AS ITS SOLO CARD SHOWS NO PINS (owner, 2026-11-06, K188): only
    // which members stand in its group view's header. Its pins are kept.
    else if (s.cluster && clusterMemberShown(cl, i) == MemberShown::SHOWN_TILE) pinGlyph(col, gap);
}

// Whether a member has a sign beside its row.
static bool hasSign(const ClusterCard &cl, uint8_t i) {
    return clusterMemberInHeader(cl, i) || (s.cluster && clusterMemberPinned(cl, i));
}

// The width a sign's column takes beside a row: the square and its gap.
int32_t pageSignW(int32_t gap) {
    lv_obj_t *probe = plain(lv_layer_top());
    Kit::add(probe, KS::KS_CLUSTER_GLYPH_SQUARE);
    const int32_t w = lv_obj_get_style_width(probe, LV_PART_MAIN) + gap;
    lv_obj_delete(probe);
    return w;
}

// MEMBERS' ROWS: a group's member cards - every member, hidden ones too (round
// 6), and a group shown as one card's too, its header's sensors with them (K157)
// - else the entities the window stands for (a member's own, inside one).
static ClusterCard *rowGroup() { return inMember() ? nullptr : heldGroup(); }

void buildMembers() {
    // The stage's height, scrolling if a group has more members than fit. As
    // wide as the controls view's slider and words together, centred, so the
    // rows line up with them (owner, 2.10c G2: full width looked awkward on
    // the P4_5).
    // Centred vertically too (owner, round 4): as tall as its rows, never
    // taller than the stage, which centres it.
    lv_obj_t *list = plain(s.stage);
    const int32_t rowW = membersRowW();
    const ClusterCard *cl = rowGroup();
    // ON THE PAGE, ITS SIGN OUTSIDE THE ROW ON THE RIGHT (owner, round 2 of the
    // follow-ups; K160): the rows as they were, centred, with a column each side
    // as wide as the sign and its gap - a header member's glyph, a pinned tile's
    // pin - only when one of them is on the page.
    bool anyHeader = false;
    for (uint8_t i = 0; cl && i < cl->memberCount(); i++) anyHeader |= hasSign(*cl, i);
    const int32_t gap = mm(UI::shape().S5);
    const int32_t side = anyHeader ? pageSignW(gap) : 0;
    const int32_t avail = lv_area_get_width(&s.winRect) - 2 * s.pad;
    const int32_t rowsW = anyHeader ? LV_MIN(rowW > 0 ? rowW : avail, avail - 2 * side) : rowW;
    lv_obj_set_size     (list, anyHeader ? rowsW + 2 * side : rowW > 0 ? rowW : lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(list, lv_pct(100), 0);
    lv_obj_add_flag     (list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, mm(UI::shape().S5), 0);
    constexpr uint8_t ROWS = GROUP_MEMBERS_MAX > POPUP_ENT_MAX ? GROUP_MEMBERS_MAX : POPUP_ENT_MAX;
    lv_obj_t *rows[ROWS] = {};
    uint8_t nRows = 0;
    if (cl) clusterMemberNames(*cl);
    const uint8_t total = cl ? cl->memberCount() : memberCount();
    for (uint8_t i = 0; i < total; i++) {
        const Entity *e = cl ? (cl->memberAt(i) ? cl->memberAt(i)->firstEntity() : nullptr) : memberAt(i);
        if (!e) continue;
        // A tap opens that member's own views (owner, 2.10b; K157).
        const int8_t k = cl ? (int8_t)i : -1;
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
            if (k >= 0) pageSign(right, *cl, (uint8_t)k, *e, gap);
        }
        lv_obj_add_event_cb(r, memberRowCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        if (nRows < ROWS) rows[nRows++] = r;
    }
    lv_obj_t *note = Kit::label(list, KS::KS_LABEL_TAG_DIM);
    // Owner's words, 2026-10-07; a cluster's member is shown or hidden from its own view.
    lv_label_set_text(note, "Tap a member for details");   // the owner's words, round 14
    Kit::helperNote(note);   // UI Tweaks' Show helper text (2.13)
    // SATIN (#99, K77): a row can be tapped and is not chosen. Painted at the
    // rows' laid-out size, which only the layout knows.
    lv_obj_update_layout(list);
    for (uint8_t i = 0; i < nRows; i++)
        Kit::finish(rows[i], lv_obj_get_width(rows[i]), lv_obj_get_height(rows[i]),
                    lv_obj_get_style_radius(rows[i], LV_PART_MAIN), Kit::Finish::FINISH_SATIN);
}

// --- A MEMBER'S OWN VIEWS (2.10b; owner: tap a row in Members) --------------
// The same window and the same views, pointed at one member: the window's
// entities are set aside, and the back arrow puts them back and returns where
// the member was opened from - Members, or the group view. It needs no card of
// its own, so Lamp 4 works too. A MEMBER THAT IS A CARD OF ITS OWN opens as a
// long press on its tile does (K157): its entities, its name, its own Members -
// Desk in Office, and one of Desk's lamps from there. Two levels at most (K147).
namespace {
constexpr uint8_t DEPTH_MAX = 2;
struct Level {
    const Entity *ent[POPUP_ENT_MAX];
    uint8_t       nEnt;
    const Entity *native;
    bool          expanded;
    uint8_t       lightCtl;   // the control it showed, restored on the way back
    PopupView     back;       // where the member was opened from
};
Level s_lv[DEPTH_MAX];
Card *s_mCard[DEPTH_MAX] = {};                // each level's member: its card, or null
char  s_mName[DEPTH_MAX][ENTITY_NAME_MAX];    // ... and its name

// What is to be entered (deferred: a row or a tile is in the stage about to be cleaned).
Card         *s_pickCard = nullptr;
const Entity *s_pickEnt  = nullptr;
PopupView     s_pickView = PopupView::VIEW_MEMBER;
}  // namespace

void enterMemberAsync(void *unused) {
    (void)unused;
    if (s.phase != PopupPhase::PHASE_OPEN || s.depth >= DEPTH_MAX ||
        (s.view != PopupView::VIEW_MEMBERS && s.view != PopupView::VIEW_GROUP)) return;   // a row, or a tile (2.13)
    Card *mc = s_pickCard;
    const Entity *m = mc ? mc->firstEntity() : s_pickEnt;
    if (!m) return;
    Level &L = s_lv[s.depth];
    for (uint8_t i = 0; i < POPUP_ENT_MAX; i++) L.ent[i] = s.ent[i];
    L.nEnt     = s.nEnt;
    L.native   = s.native;
    L.expanded = s.expanded;
    L.lightCtl = s.lightCtl;
    L.back     = s.view;
    s_mCard[s.depth] = mc;
    // ITS CARD'S NAME ("Desk"), the window's title says (K157); HA's for an entity with none.
    snprintf(s_mName[s.depth], sizeof(s_mName[0]), "%s", mc ? mc->shownName(CardLabel::LBL_NAME) : m->desc.name);
    s.depth++;
    for (uint8_t i = 0; i < POPUP_ENT_MAX; i++) s.ent[i] = nullptr;
    s.nEnt = 0;
    if (mc) {
        for (uint8_t i = 0; i < mc->entityCount() && s.nEnt < POPUP_ENT_MAX; i++)
            if (const Entity *e = mc->entityAt(i)) s.ent[s.nEnt++] = e;
    } else {
        s.ent[s.nEnt++] = m;
    }
    // One entity HA defines as a group (Desk is light.office): its members are its Members.
    s.native   = (s.nEnt == 1 && s.ent[0]->nMembers) ? s.ent[0] : nullptr;
    s.expanded = false;
    applyNative();
    s.lightCtl = firstCtl(lightAggregate().caps);
    const PopupView v = s_pickView == PopupView::VIEW_HISTORY ? PopupView::VIEW_HISTORY
                      : s_pickView == PopupView::VIEW_MEMBERS && hasMembers() ? PopupView::VIEW_MEMBERS
                      : PopupView::VIEW_MEMBER;
    showView(v);
    if (v == PopupView::VIEW_HISTORY) markDirectView();   // back returns here (K169)
}

void pickMemberRow(uint8_t i) {
    const ClusterCard *cl = rowGroup();
    s_pickCard = cl ? cl->memberAt(i) : nullptr;
    s_pickEnt  = cl ? nullptr : memberAt(i);
    s_pickView = PopupView::VIEW_MEMBER;
}

// A row in Members. Deferred: the row is in the stage that is about to be
// cleaned, and an event callback must not delete its own object.
void memberRowCb(lv_event_t *ev) {
    pickMemberRow((uint8_t)(uintptr_t)lv_event_get_user_data(ev));
    lv_async_call(enterMemberAsync, nullptr);
}

void enterMember(Card *card, const Entity *e, PopupView v) {
    s_pickCard = card;
    s_pickEnt  = card ? nullptr : e;
    s_pickView = v;
    lv_async_call(enterMemberAsync, nullptr);
}

Card *memberCard() { return s.depth ? s_mCard[s.depth - 1] : nullptr; }
const char *memberName() { return s.depth ? s_mName[s.depth - 1] : s.name; }
const char *memberParentName() { return s.depth >= 2 ? s_mName[s.depth - 2] : s.name; }

void memberForget() {
    s.depth = 0;
    for (Card *&c : s_mCard) c = nullptr;
    s_pickCard = nullptr;
    s_pickEnt  = nullptr;
}

// Out of a member, one level: back where it was opened from, or to the
// controls there (the "Desk" in the title).
void leaveMember(PopupView to) {
    if (!inMember()) return;
    s.depth--;
    const Level &L = s_lv[s.depth];
    for (uint8_t i = 0; i < POPUP_ENT_MAX; i++) s.ent[i] = L.ent[i];
    s.nEnt     = L.nEnt;
    s.native   = L.native;
    s.expanded = L.expanded;
    s_mCard[s.depth] = nullptr;
    applyNative();   // the member may have been paused or resumed meanwhile
    s.view     = PopupView::VIEW_MEMBERS;   // not Scenes: its slider swap is the member's
    s.lightCtl = L.lightCtl;
    showView(to == PopupView::VIEW_MEMBERS ? L.back : to == PopupView::VIEW_MAIN ? controlsView() : to);
}

} // namespace popup
