// A CLUSTER'S MANAGEMENT SCREEN (2.11 round 3, DECISIONS K104, the owner's
// layout): the main view of a cluster's window (a long press on its header,
// K107). Two parts:
//
//   HEADER   Name - its own words, typed on the keyboard (K95); Style - the
//            page's, Swatch or Bar (K102); Sensor icons - the
//            header's values with their icons or without; Header slots - a view
//            of the members, each ticked into the header or not (K104's Slots).
//   MEMBERS  each member's name and Visible: a tile in the cluster or not
//            (hidden is not removed, K92 rule 8). Add cards at the foot: the
//            page's cards that can join, ticked, then Copy or Move (K91, K100);
//            HA entities not on the page come with the card list.
//
// Kept under the cluster's id (label_text, cl_style, cl_icons, slots) and each member's
// (hidden) - the settings file's entries agreed in K106 - written when the
// window closes, as SETTINGS' rows are; the page is built again once it has
// gone, so what the cluster shows follows. Every state shown is read from the
// file first, then from the cluster as built, so the view says what the next
// build will draw.
#include "PopupState.h"
#include "Cards/ClusterCard.h"

namespace popup {

namespace {

bool s_rebuildPage = false;

// ADD CARDS (K104's foot, K91): the page's cards that can join - one entity
// each, not a cluster, not in this one already - ticked, then Copy or Move.
constexpr uint8_t ADD_MAX = 16;
Card       *s_cand[ADD_MAX] = {};
lv_obj_t   *s_cbAdd[ADD_MAX] = {};
bool        s_tick[ADD_MAX] = {};
uint8_t     s_nCand = 0;
bool        s_addMove = false;
const char *const ADD_COPY_W[] = { "Copy - stay here", "Move" };

const char *const STYLE_W[]  = { "Page's", "Swatch", "Bar" };
const char *const STYLE_IS[] = { nullptr, "swatch", "bar" };

lv_obj_t *s_cbVis[GROUP_MEMBERS_MAX]  = {};
lv_obj_t *s_cbSlot[GROUP_MEMBERS_MAX] = {};
lv_obj_t *s_cbIcons = nullptr;

ClusterCard *held() {
    Card *c = cardOf(h.surface);
    return (c && s.cluster) ? static_cast<ClusterCard *>(c) : nullptr;
}

const char *idOf(const Card *m) { return (m && m->hasId()) ? m->id() : ""; }

// The members in the header, highest priority first: the saved list, else the
// cluster's as built. Ids point into `buf` or at the members' own.
uint8_t headerIds(const ClusterCard &cl, const char **ids, char *buf, size_t cap) {
    uint8_t n = 0;
    if (cl.hasId() && Settings::card(cl.id(), "slots", buf, cap)) {
        for (char *p = buf; *p && n < CLUSTER_SLOTS_MAX;) {
            ids[n++] = p;
            char *comma = strchr(p, ',');
            if (!comma) break;
            *comma = '\0';
            p = comma + 1;
        }
        return n;
    }
    for (int8_t r = 0; r < (int8_t)CLUSTER_SLOTS_MAX; r++)
        for (uint8_t i = 0; i < cl.memberCount(); i++)
            if (cl.slotAt(i) == r && n < CLUSTER_SLOTS_MAX) ids[n++] = idOf(cl.memberAt(i));
    return n;
}

bool inList(const char *id, const char *const *ids, uint8_t n) {
    for (uint8_t k = 0; k < n; k++) if (id[0] && !strcmp(ids[k], id)) return true;
    return false;
}

// A member shown as a tile: its saved Visible, else - as the page builds it -
// a tile unless it is in the header (K92 rule 7).
bool visible(const ClusterCard &cl, uint8_t i, const char *const *ids, uint8_t n) {
    const Card *m = cl.memberAt(i);
    const char *id = idOf(m);
    char v[8];
    if (id[0] && Settings::card(id, "hidden", v, sizeof(v))) {
        if (!strcmp(v, "yes")) return false;
        if (!strcmp(v, "no"))  return true;
    }
    return !inList(id, ids, n);
}

const char *nameOf(const Card *m) { return m ? m->shownName(CardLabel::LBL_NAME) : "?"; }

// Each member's name in the rows; two called the same ("Office", its temperature
// and its occupancy) say what each is.
char s_names[GROUP_MEMBERS_MAX][ENTITY_NAME_MAX + 24];
void names(const ClusterCard &cl) {
    for (uint8_t i = 0; i < cl.memberCount() && i < GROUP_MEMBERS_MAX; i++) {
        const Card *m = cl.memberAt(i);
        const char *nm = nameOf(m);
        bool twin = false;
        for (uint8_t j = 0; j < cl.memberCount() && !twin; j++)
            twin = j != i && !strcmp(nameOf(cl.memberAt(j)), nm);
        const Entity *e = m ? m->firstEntity() : nullptr;
        char w[32];
        if (twin && e) snprintf(s_names[i], sizeof(s_names[i]), "%s (%s)", nm, whatWord(*e, w, sizeof(w)));
        else           snprintf(s_names[i], sizeof(s_names[i]), "%s", nm);
    }
}

int32_t listW() { return lv_area_get_width(&s.winRect) - 2 * s.pad; }

// A scrolling column of rows and titles, the stage's whole height at most.
lv_obj_t *column() {
    lv_obj_t *list = plain(s.stage);
    lv_obj_set_size(list, listW(), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(list, lv_pct(100), 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, mm(UI::shape().S3), 0);
    return list;
}

void title(lv_obj_t *list, const char *words) {
    lv_obj_t *l = Kit::label(list, KS::KS_PANEL_TITLE);
    lv_label_set_text(l, words);
}

void note(lv_obj_t *list, const char *words) {
    lv_obj_t *l = Kit::label(list, KS::KS_LABEL_TAG_DIM);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, words);
}

void rebuildView() {
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(s.view); }, nullptr);
}

void changed() { s_rebuildPage = true; }

// --- What the rows do ----------------------------------------------------------------
void styleCb(lv_event_t *ev) {
    ClusterCard *cl = held();
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (!cl || !cl->hasId() || i >= 3) return;
    Settings::setCard(cl->id(), "cl_style", STYLE_IS[i]);
    changed();
    rebuildView();
}

void iconsCb(lv_event_t *ev) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId()) return;
    const bool on = lv_obj_has_state(lv_event_get_target_obj(ev), LV_STATE_CHECKED);
    Settings::setCard(cl->id(), "cl_icons", on ? nullptr : "off");
    changed();
}

// RENAME (K95): its own words on the keyboard - the header's name, in the
// group's colour (K46 rename a); none, and it is its group's name again.
void renamed(const char *words, void *) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId() || s.phase != PopupPhase::PHASE_OPEN) return;
    CardGroups::Group g;
    const bool groups = CardGroups::ofCard(*cl, g) && !strcmp(words, g.name);   // its group's: none of its own
    Settings::setCard(cl->id(), "label_text", (words[0] && !groups) ? words : nullptr);
    CardGroups::apply(*cl, cl->pageAreaColor());
    snprintf(s.area, sizeof(s.area), "%s", cl->area());
    snprintf(s.name, sizeof(s.name), "%s", cl->area()[0] ? cl->area() : cl->shownName(CardLabel::LBL_NAME));
    changed();
    rebuildView();   // the title says it at once; the cluster when the window closes
}

void renameCb(lv_event_t *ev) {
    (void)ev;
    ClusterCard *cl = held();
    if (!cl || !cl->hasId()) return;
    static char own[40], dflt[48];
    if (!Settings::card(cl->id(), "label_text", own, sizeof(own))) own[0] = '\0';
    CardGroups::Group g;
    snprintf(dflt, sizeof(dflt), "%s", CardGroups::ofCard(*cl, g) ? g.name : cl->shownName(CardLabel::LBL_NAME));
    Kit::KeyboardSpec k = {};
    k.title  = "Name this cluster";
    k.text   = own[0] ? own : dflt;
    k.hint   = dflt;           // shown when the field is empty: what it is called then
    k.maxLen = sizeof(own) - 1;
    k.save   = renamed;
    Kit::keyboard(k);
}

void slotsViewCb(lv_event_t *ev) {
    (void)ev;
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CLUSTER_SLOTS); },
                  nullptr);
}

int8_t indexOf(lv_obj_t *const *boxes, lv_obj_t *o) {
    for (uint8_t i = 0; i < GROUP_MEMBERS_MAX; i++) if (boxes[i] == o) return (int8_t)i;
    return -1;
}

void visibleCb(lv_event_t *ev) {
    ClusterCard *cl = held();
    lv_obj_t *box = lv_event_get_target_obj(ev);
    const int8_t i = indexOf(s_cbVis, box);
    if (!cl || i < 0) return;
    const Card *m = cl->memberAt((uint8_t)i);
    if (!m || !m->hasId()) return;
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t n = headerIds(*cl, ids, buf, sizeof(buf));
    const bool want = lv_obj_has_state(box, LV_STATE_CHECKED);
    const bool dflt = !inList(m->id(), ids, n);
    // Only what differs from the default is kept (Settings.h: a missing key inherits).
    Settings::setCard(m->id(), "hidden", want == dflt ? nullptr : (want ? "no" : "yes"));
    changed();
}

// Ticked into the header: appended, last in priority; unticked: taken out. A
// member that leaves the header comes back as a tile unless hidden (K104).
void slotCb(lv_event_t *ev) {
    ClusterCard *cl = held();
    lv_obj_t *box = lv_event_get_target_obj(ev);
    const int8_t i = indexOf(s_cbSlot, box);
    if (!cl || !cl->hasId() || i < 0) return;
    const Card *m = cl->memberAt((uint8_t)i);
    if (!m || !m->hasId()) return;
    const char *ids[CLUSTER_SLOTS_MAX + 1];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    uint8_t n = headerIds(*cl, ids, buf, sizeof(buf));
    const bool want = lv_obj_has_state(box, LV_STATE_CHECKED);
    char list[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1] = "";
    uint8_t kept = 0;
    for (uint8_t k = 0; k < n; k++) {
        if (!want && !strcmp(ids[k], m->id())) continue;
        snprintf(list + strlen(list), sizeof(list) - strlen(list), "%s%s", kept ? "," : "", ids[k]);
        kept++;
    }
    if (want && !inList(m->id(), ids, n)) {
        if (kept >= CLUSTER_SLOTS_MAX) {   // four at most: the header has no room for more
            lv_obj_clear_state(box, LV_STATE_CHECKED);
            UIToolkit::show_toast("The header holds four at most");
            return;
        }
        snprintf(list + strlen(list), sizeof(list) - strlen(list), "%s%s", kept ? "," : "", m->id());
    }
    Settings::setCard(cl->id(), "slots", list);
    changed();
}

// --- Add cards -------------------------------------------------------------------------
bool memberHas(const ClusterCard &cl, const Entity &e) {
    for (uint8_t i = 0; i < cl.memberCount(); i++) {
        const Card *m = cl.memberAt(i);
        const Entity *me = m ? m->firstEntity() : nullptr;
        if (me && !strcmp(me->desc.id, e.desc.id)) return true;
    }
    return false;
}

// The page's cards, in screen order; a card's own objects hold no more cards.
void findCandidates(lv_obj_t *o, const ClusterCard &cl) {
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n && s_nCand < ADD_MAX; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (!lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) { findCandidates(c, cl); continue; }
        Card *k = cardOf(c);
        const Entity *e = k ? k->firstEntity() : nullptr;
        if (k && e && k->hasId() && !k->mini() && k->entityCount() == 1 && strcmp(k->typeName(), "cluster") &&
            !memberHas(cl, *e))
            s_cand[s_nCand++] = k;
    }
}

char s_candName[ADD_MAX][ENTITY_NAME_MAX + 24];

void addViewCb(lv_event_t *) {
    for (bool &t : s_tick) t = false;
    s_addMove = false;
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CLUSTER_ADD); }, nullptr);
}

void addTickCb(lv_event_t *ev) {
    lv_obj_t *box = lv_event_get_target_obj(ev);
    for (uint8_t i = 0; i < ADD_MAX; i++)
        if (s_cbAdd[i] == box) s_tick[i] = lv_obj_has_state(box, LV_STATE_CHECKED);
}

void addMoveCb(lv_event_t *ev) {
    s_addMove = (uintptr_t)lv_event_get_user_data(ev) == 1;
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CLUSTER_ADD); }, nullptr);
}

void addGoCb(lv_event_t *) {
    lv_async_call([](void *) {
        uint32_t mask = 0;
        for (uint8_t i = 0; i < ADD_MAX; i++) if (s_tick[i]) mask |= 1u << i;
        clusterAddCards(mask, s_addMove);
    }, nullptr);
}

}  // namespace

// The ticked cards (`mask`, by their place in the list) into the cluster - copies
// with the names they show; Move hides each from the page too. The window closes
// and the page is built again. How many went in.
uint8_t clusterAddCards(uint32_t mask, bool move) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId() || s.phase != PopupPhase::PHASE_OPEN) return 0;
    s_nCand = 0;
    findCandidates(lv_screen_active(), *cl);
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nCand; i++) {
        if (!(mask & (1u << i))) continue;
        Card *k = s_cand[i];
        char id[CARD_ID_MAX + 1];
        if (!CardAdded::addTo(*cl, *k->firstEntity(), id, sizeof(id))) break;   // full
        const char *name = k->shownName(CardLabel::LBL_NAME);
        if (name && name[0]) Settings::setCard(id, "name", name);
        if (move) CardAdded::hide(k->id());
        n++;
    }
    if (!n) return 0;
    char msg[64];
    snprintf(msg, sizeof(msg), "%s %u to %s", move ? "Moved" : "Copied", (unsigned)n, s.name);
    UIToolkit::show_toast(msg);
    CardPopup::close();
    return n;
}

void buildClusterAdd() {
    ClusterCard *cl = held();
    if (!cl) return;
    s_nCand = 0;
    findCandidates(lv_screen_active(), *cl);
    lv_obj_t *list = column();
    title(list, "ADD CARDS");
    if (!s_nCand) {
        note(list, "No card on this page can join: a card joins with one entity, and not one this cluster "
                   "has already.");
        return;
    }
    DeckRowSpec rows[ADD_MAX];
    for (uint8_t i = 0; i < ADD_MAX; i++) s_cbAdd[i] = nullptr;
    for (uint8_t i = 0; i < s_nCand; i++) {
        Card *k = s_cand[i];
        if (k->area()[0]) snprintf(s_candName[i], sizeof(s_candName[i]), "%s (%s)", k->shownName(CardLabel::LBL_NAME), k->area());
        else              snprintf(s_candName[i], sizeof(s_candName[i]), "%s", k->shownName(CardLabel::LBL_NAME));
        rows[i] = { s_candName[i], Kit::RowKind::ROW_CHECK, nullptr, (uint16_t)s_tick[i], true, addTickCb, &s_cbAdd[i] };
    }
    Kit::rows(list, rows, s_nCand);
    lv_obj_t *r = Kit::row(list, "These cards", true);
    Kit::chipRow(r, ADD_COPY_W, 2, s_addMove ? 1 : 0, listW() * 2 / 3, addMoveCb);
    const DeckRowSpec go = { "Add the ticked cards", Kit::RowKind::ROW_BUTTON, "Add", 0, true, addGoCb, nullptr };
    Kit::rows(list, &go, 1);
    note(list, "Copy: each card stays on the page and a copy joins the cluster. Move: each joins it and "
               "leaves this page for its list of hidden cards.");
}

// --- The views ------------------------------------------------------------------------
void buildCluster() {
    ClusterCard *cl = held();
    if (!cl) return;
    char v[16];
    uint8_t style = 0;
    if (cl->hasId() && Settings::card(cl->id(), "cl_style", v, sizeof(v)))
        style = !strcmp(v, "bar") ? 2 : !strcmp(v, "swatch") ? 1 : 0;
    const bool icons = !(cl->hasId() && Settings::card(cl->id(), "cl_icons", v, sizeof(v)) && !strcmp(v, "off"));
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t nIds = headerIds(*cl, ids, buf, sizeof(buf));

    lv_obj_t *list = column();
    title(list, "HEADER");
    lv_obj_t *r = Kit::row(list, "Style", true);
    Kit::chipRow(r, STYLE_W, 3, style, listW() / 2, styleCb);
    s_cbIcons = nullptr;
    const DeckRowSpec hdr[3] = {
        { "Name",         Kit::RowKind::ROW_BUTTON, "Rename", 0,               true, renameCb,    nullptr },
        { "Sensor icons", Kit::RowKind::ROW_CHECK,  nullptr,  (uint16_t)icons, true, iconsCb,     &s_cbIcons },
        { "Header slots", Kit::RowKind::ROW_BUTTON, "Choose", 0,               true, slotsViewCb, nullptr },
    };
    Kit::rows(list, hdr, 3);

    title(list, "MEMBERS");
    DeckRowSpec rows[GROUP_MEMBERS_MAX + 1];
    uint8_t n = 0;
    names(*cl);
    for (uint8_t i = 0; i < GROUP_MEMBERS_MAX; i++) s_cbVis[i] = nullptr;
    for (uint8_t i = 0; i < cl->memberCount() && n < GROUP_MEMBERS_MAX; i++, n++)
        rows[n] = { s_names[i], Kit::RowKind::ROW_CHECK, nullptr,
                    (uint16_t)visible(*cl, i, ids, nIds), true, visibleCb, &s_cbVis[i] };
    // The page's cards (K91); HA entities not on the page (K92 rule 11) come with the card list.
    rows[n++] = { "Add cards", Kit::RowKind::ROW_BUTTON, "Add", 0, cl->hasId(), addViewCb, nullptr };
    Kit::rows(list, rows, n);
    note(list, "Visible: shown as a tile. Changes show when this window closes.");
}

void buildClusterSlots() {
    ClusterCard *cl = held();
    if (!cl) return;
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t nIds = headerIds(*cl, ids, buf, sizeof(buf));
    lv_obj_t *list = column();
    title(list, "IN THE HEADER");
    note(list, "Ticked members show their value in the header, in the order ticked; one that does "
               "not fit gives way, the last first. A member in the header leaves the tiles unless "
               "made Visible.");
    DeckRowSpec rows[GROUP_MEMBERS_MAX];
    uint8_t n = 0;
    names(*cl);
    for (uint8_t i = 0; i < GROUP_MEMBERS_MAX; i++) s_cbSlot[i] = nullptr;
    for (uint8_t i = 0; i < cl->memberCount() && n < GROUP_MEMBERS_MAX; i++, n++)
        rows[n] = { s_names[i], Kit::RowKind::ROW_CHECK, nullptr,
                    (uint16_t)inList(idOf(cl->memberAt(i)), ids, nIds), true, slotCb, &s_cbSlot[i] };
    Kit::rows(list, rows, n);
}

void clusterRename() { renameCb(nullptr); }

bool clusterTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

}  // namespace popup
