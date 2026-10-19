// A CLUSTER'S MANAGEMENT SCREEN (2.11 round 3, DECISIONS K104, the owner's
// layout): the main view of a cluster's window (a long press on its header,
// K107). Two parts:
//
//   HEADER   Style - the page's, Swatch or Bar (K102); Sensor icons - the
//            header's values with their icons or without; Header slots - a view
//            of the members, each ticked into the header or not (K104's Slots).
//   MEMBERS  each member's name and Visible: a tile in the cluster or not
//            (hidden is not removed, K92 rule 8). Add cards at the foot comes
//            with the card list (greyed until then).
//
// Kept under the cluster's id (cl_style, cl_icons, slots) and each member's
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

}  // namespace

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
    const DeckRowSpec hdr[2] = {
        { "Sensor icons", Kit::RowKind::ROW_CHECK,  nullptr,  (uint16_t)icons, true, iconsCb,     &s_cbIcons },
        { "Header slots", Kit::RowKind::ROW_BUTTON, "Choose", 0,               true, slotsViewCb, nullptr },
    };
    Kit::rows(list, hdr, 2);

    title(list, "MEMBERS");
    DeckRowSpec rows[GROUP_MEMBERS_MAX + 1];
    uint8_t n = 0;
    names(*cl);
    for (uint8_t i = 0; i < GROUP_MEMBERS_MAX; i++) s_cbVis[i] = nullptr;
    for (uint8_t i = 0; i < cl->memberCount() && n < GROUP_MEMBERS_MAX; i++, n++)
        rows[n] = { s_names[i], Kit::RowKind::ROW_CHECK, nullptr,
                    (uint16_t)visible(*cl, i, ids, nIds), true, visibleCb, &s_cbVis[i] };
    // The card list (K91, K92 rule 11) comes later: greyed until then.
    rows[n++] = { "Add cards", Kit::RowKind::ROW_BUTTON, "Add", 0, false, nullptr, nullptr };
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

bool clusterTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

}  // namespace popup
