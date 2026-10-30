// A CLUSTER'S WINDOW IS ITS MEMBERS (2.11 round 16, the owner: the Details view
// held settings, not details). A long press on its header (K107) opens Members -
// every member, hidden ones too, each opening its own view, whose Visibility
// shows or hides it - with the CLUSTER panel beside SETTINGS:
//
//   Rename cluster - its own words on the keyboard (K95); Header options - which
//   sensors its header shows (four at most), Sensor icons and Header style (the
//   page's, Swatch or Band - K102); Add cards - the page's cards that can join,
//   Copy or Move (K91, K100).
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
// each, not a cluster, not in this one already - each No, Copy or Move.
constexpr uint8_t ADD_MAX = 16;
Card       *s_cand[ADD_MAX] = {};
uint8_t     s_pick[ADD_MAX] = {};   // 0 no, 1 copy, 2 move
uint8_t     s_nCand = 0;
const char *const ADD_W[] = { "No", "Copy", "Move" };

// THE OPTIONS ARE CHIP TABLES (round 4, the owner: "look more like the Card UI or
// UI Tweaks sheet"): an option's name right-justified against its chips.
const char *const STYLE_W[]  = { "Inherit", "Swatch", "Band" };   // "Band", as the label type's (round 10); saved as "bar"
const char *const STYLE_IS[] = { nullptr, "swatch", "bar" };
const char *const ICONS_W[]  = { "Show", "Hide" };
constexpr uint8_t ROW_ICONS = 0, ROW_STYLE = 1;   // Header options' chip rows, under the sensors (round 16)

lv_obj_t *s_table = nullptr;   // the view's chip table

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
    if (id[0] && Settings::card(id, "hidden", v, sizeof(v))) return !strcmp(v, "no");   // "yes", or "moved" to the page
    return !inList(id, ids, n);
}

bool movedOut(const Card *m) { return m && m->hasId() && CardAdded::isMovedToPage(m->id()); }

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

void note(lv_obj_t *list, const char *words) {
    lv_obj_t *l = Kit::label(list, KS::KS_LABEL_TAG_DIM);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, words);
}

// The foot's buttons, as Card UI's and UI Tweaks' are: a row, one width, centred.
void footButtons(lv_obj_t *list, const char *const *words, const Kit::ButtonRole *roles,
                 const lv_event_cb_t *cbs, uint8_t n) {
    if (!n) return;
    const UIShape &k = UI::shape();
    const int32_t gap = mm(k.S5);
    int32_t widest = 0;
    for (uint8_t i = 0; i < n; i++) widest = LV_MAX(widest, Kit::textW(words[i], UI::type().NAME));
    const int32_t bw = LV_MIN((listW() - (n - 1) * gap) / LV_MAX(n, 1), widest + 2 * mm(k.S7));
    lv_obj_t *row = Kit::buttonRow(list);
    for (uint8_t i = 0; i < n; i++) Kit::button(row, roles[i], words[i], bw, Kit::controlH(), cbs[i]);
}

void rebuildView() {
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(s.view); }, nullptr);
}

void changed() { s_rebuildPage = true; }

// --- What the rows do ----------------------------------------------------------------
// A member shown as a tile or not: only what differs from the default is kept
// (Settings.h: a missing key inherits) - the default a tile unless in the header.
void setVisible(const ClusterCard &cl, uint8_t i, bool want) {
    const Card *m = cl.memberAt(i);
    if (!m || !m->hasId() || movedOut(m)) return;   // its card on the page moves it back, nothing here
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t n = headerIds(cl, ids, buf, sizeof(buf));
    const bool dflt = !inList(m->id(), ids, n);
    Settings::setCard(m->id(), "hidden", want == dflt ? nullptr : (want ? "no" : "yes"));
}

// Header options' chips: lit at once, kept from the next timer pass.
void mainChoose(void *p) {
    const uintptr_t v = (uintptr_t)p;
    const uint8_t row = (uint8_t)(v >> 8), i = (uint8_t)(v & 0xFF);
    ClusterCard *cl = held();
    if (!cl || !cl->hasId() || s.phase != PopupPhase::PHASE_OPEN) return;
    if (row == ROW_STYLE)      Settings::setCard(cl->id(), "cl_style", i < 3 ? STYLE_IS[i] : nullptr);
    else if (row == ROW_ICONS) Settings::setCard(cl->id(), "cl_icons", i ? "off" : nullptr);
    changed();
}

void mainChipCb(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    Kit::chipTableLit(s_table, (uint8_t)(v >> 8), (uint8_t)(v & 0xFF));
    lv_async_call(mainChoose, (void *)v);
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
    ClusterCard *cl = held();
    if (!cl || !cl->hasId()) return;
    if (ev) chartSet(DECK_PEEK);   // from the CLUSTER panel: it folds under the keyboard
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

// A SENSOR'S ROW IS A BUTTON (round 16, components.md's surfaces): satin while
// it can be chosen, glass while it is in the header. A tap puts it in the
// header - appended, last in priority - or takes it out. A member that leaves
// the header comes back as a tile unless hidden (K104).
uint8_t   s_slotMember[GROUP_MEMBERS_MAX];   // Header options' rows: each row's member (one moved out has none)
lv_obj_t *s_slotRow[GROUP_MEMBERS_MAX];
bool      s_slotOn[GROUP_MEMBERS_MAX];
uint8_t   s_nSlotRows = 0;

void slotLook(uint8_t row) {
    lv_obj_t *r = s_slotRow[row];
    if (!r) return;
    const bool on = s_slotOn[row];
    Kit::finish(r, lv_obj_get_width(r), lv_obj_get_height(r), lv_obj_get_style_radius(r, LV_PART_MAIN),
                on ? Kit::Finish::FINISH_GLASS : Kit::Finish::FINISH_SATIN);
    // Its name in the accent while chosen, as a loaded scene's.
    for (uint32_t k = 0; k < lv_obj_get_child_count(r); k++) {
        lv_obj_t *l = lv_obj_get_child(r, (int32_t)k);
        if (!lv_obj_check_type(l, &lv_label_class)) continue;
        if (on) lv_obj_set_style_text_color(l, UI::c(UI::pal().ACCENT), 0);
        else    lv_obj_remove_local_style_prop(l, LV_STYLE_TEXT_COLOR, 0);   // its role's again
        break;
    }
}

void slotChoose(uint8_t row) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId() || s.phase != PopupPhase::PHASE_OPEN || row >= s_nSlotRows) return;
    const bool want = !s_slotOn[row];
    const uint8_t i = s_slotMember[row];
    const Card *m = cl->memberAt(i);
    if (!m || !m->hasId()) return;
    const char *ids[CLUSTER_SLOTS_MAX + 1];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    uint8_t n = headerIds(*cl, ids, buf, sizeof(buf));
    char list[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1] = "";
    uint8_t kept = 0;
    for (uint8_t k = 0; k < n; k++) {
        if (!want && !strcmp(ids[k], m->id())) continue;
        snprintf(list + strlen(list), sizeof(list) - strlen(list), "%s%s", kept ? "," : "", ids[k]);
        kept++;
    }
    if (want && !inList(m->id(), ids, n)) {
        if (kept >= CLUSTER_SLOTS_MAX) {   // four at most: the header has no room for more
            UIToolkit::show_toast("Limit reached");
            return;
        }
        snprintf(list + strlen(list), sizeof(list) - strlen(list), "%s%s", kept ? "," : "", m->id());
    }
    Settings::setCard(cl->id(), "slots", list);
    s_slotOn[row] = want;
    slotLook(row);
    changed();
}

void slotRowCb(lv_event_t *ev) {
    slotChoose((uint8_t)(uintptr_t)lv_event_get_user_data(ev));
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
    for (uint8_t &p : s_pick) p = 0;
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CLUSTER_ADD); }, nullptr);
}

void addChipCb(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    const uint8_t row = (uint8_t)(v >> 8), i = (uint8_t)(v & 0xFF);
    Kit::chipTableLit(s_table, row, i);
    if (row < ADD_MAX) s_pick[row] = i;
}

void addGoCb(lv_event_t *) {
    lv_async_call([](void *) {
        uint32_t copy = 0, move = 0;
        for (uint8_t i = 0; i < ADD_MAX; i++) {
            if (s_pick[i] == 1) copy |= 1u << i;
            if (s_pick[i] == 2) move |= 1u << i;
        }
        clusterAddCards(copy, move);
    }, nullptr);
}

}  // namespace

// The cards picked (by their place in the list): a copy of each into the cluster,
// with the name it shows; one to Move also hides from the page. The window closes
// and the page is built again. How many went in.
uint8_t clusterAddCards(uint32_t copyMask, uint32_t moveMask) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId() || s.phase != PopupPhase::PHASE_OPEN) return 0;
    s_nCand = 0;
    findCandidates(lv_screen_active(), *cl);
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nCand; i++) {
        const bool move = moveMask & (1u << i);
        if (!move && !(copyMask & (1u << i))) continue;
        Card *k = s_cand[i];
        char id[CARD_ID_MAX + 1];
        if (!CardAdded::addTo(*cl, *k->firstEntity(), id, sizeof(id), k->id())) break;   // full
        const char *name = k->shownName(CardLabel::LBL_NAME);
        if (name && name[0]) Settings::setCard(id, "name", name);
        if (move) CardAdded::moveOut(k->id());   // off the page, not one of its hidden cards (round 7)
        n++;
    }
    if (!n) return 0;
    char msg[64];
    snprintf(msg, sizeof(msg), "Added %u to %s", (unsigned)n, s.name);
    UIToolkit::show_toast(msg);
    CardPopup::close();
    return n;
}

// Add cards: each card on the page that can join, No / Copy / Move; Add.
void buildClusterAdd() {
    ClusterCard *cl = held();
    if (!cl) return;
    s_nCand = 0;
    findCandidates(lv_screen_active(), *cl);
    Kit::ChipRowSpec rows[ADD_MAX];
    for (uint8_t i = 0; i < s_nCand; i++) {
        Card *k = s_cand[i];
        if (k->area()[0]) snprintf(s_candName[i], sizeof(s_candName[i]), "%s (%s)", k->shownName(CardLabel::LBL_NAME), k->area());
        else              snprintf(s_candName[i], sizeof(s_candName[i]), "%s", k->shownName(CardLabel::LBL_NAME));
        rows[i] = { s_candName[i], ADD_W, 3, s_pick[i] };
    }
    s_table = Kit::chipTable(s.stage, listW(), rows, s_nCand, addChipCb);
    if (!s_nCand) {
        note(s_table, "No card on this page can join: a card joins with one entity, and not one this cluster "
                      "has already.");
        return;
    }
    static const char *const W[] = { "Add" };
    static const Kit::ButtonRole R[] = { Kit::ButtonRole::BTN_CONFIRM };
    static const lv_event_cb_t C[] = { addGoCb };
    footButtons(s_table, W, R, C, 1);
    note(s_table, "Copy: creates a separate card to add to the cluster. Move: card is removed from the "
                  "dashboard and added to the cluster.");
}

// --- A member shown or not (its own Visibility, round 6) ------------------------------
void clusterMemberShow(const ClusterCard &cl, uint8_t i, bool want) {
    setVisible(cl, i, want);
    changed();
}

MemberShown clusterMemberShown(const ClusterCard &cl, uint8_t i) {
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t n = headerIds(cl, ids, buf, sizeof(buf));
    if (movedOut(cl.memberAt(i))) return MemberShown::SHOWN_MOVED;
    if (visible(cl, i, ids, n)) return MemberShown::SHOWN_TILE;
    return inList(idOf(cl.memberAt(i)), ids, n) ? MemberShown::SHOWN_HEADER : MemberShown::SHOWN_HIDDEN;
}

int8_t clusterMemberOf(const ClusterCard &cl, const Entity *e) {
    if (!e) return -1;
    for (uint8_t i = 0; i < cl.memberCount(); i++) {
        const Card *m = cl.memberAt(i);
        const Entity *me = m ? m->firstEntity() : nullptr;
        if (me && !strcmp(me->desc.id, e->desc.id)) return (int8_t)i;
    }
    return -1;
}

// --- The views ------------------------------------------------------------------------
// THE CLUSTER PANEL (round 16): on Members, beside SETTINGS. Greyed for a
// cluster without a usable id, which could keep none of it.
uint8_t clusterSpecs(DeckRowSpec *r) {
    ClusterCard *cl = held();
    const bool live = cl && cl->hasId();
    uint8_t n = 0;
    r[n++] = { "Rename cluster", DeckRowKind::ROW_BUTTON, "Edit", 0, live, renameCb, nullptr };
    r[n++] = { "Header options", DeckRowKind::ROW_BUTTON, "Open", 0, live, slotsViewCb, nullptr };
    r[n++] = { "Add cards", DeckRowKind::ROW_BUTTON, "Open", 0, live, addViewCb, nullptr };
    return n;
}

// HEADER OPTIONS (round 16, the owner's layout): which sensors the header
// shows - each a member's row as Members has it, with its reading - then Sensor
// icons and Header style. One column holds it all, so it scrolls as one; the
// chip table's rows are found by their place in it, so it holds only its own.
void buildClusterSlots() {
    ClusterCard *cl = held();
    if (!cl) return;
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t nIds = headerIds(*cl, ids, buf, sizeof(buf));
    char v[16];
    uint8_t style = 0;
    if (cl->hasId() && Settings::card(cl->id(), "cl_style", v, sizeof(v)))
        style = !strcmp(v, "bar") ? 2 : !strcmp(v, "swatch") ? 1 : 0;
    const bool icons = !(cl->hasId() && Settings::card(cl->id(), "cl_icons", v, sizeof(v)) && !strcmp(v, "off"));
    // As tall as it holds, never taller than the stage, which centres it (Members').
    lv_obj_t *col = plain(s.stage);
    lv_obj_set_size     (col, listW(), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(col, lv_pct(100), 0);
    lv_obj_add_flag     (col, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, mm(UI::shape().S5), 0);
    note(col, "Select the entities to display in the header (max: 4)");

    // The sensors, Members' width and spacing.
    lv_obj_t *box = plain(col);
    const int32_t rowW = membersRowW();
    lv_obj_set_size     (box, rowW > 0 ? LV_MIN(rowW, listW()) : lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, mm(UI::shape().S5), 0);

    const Kit::ChipRowSpec opts[2] = {
        { "Sensor icons:", ICONS_W, 2, (uint8_t)(icons ? 0 : 1) },
        { "Header style:", STYLE_W, 3, style },
    };
    s_table = Kit::chipTable(col, listW(), opts, 2, mainChipCb);
    // In the column, which scrolls: as tall as its rows.
    lv_obj_set_flex_grow(s_table, 0);
    lv_obj_set_height   (s_table, LV_SIZE_CONTENT);
    lv_obj_remove_flag  (s_table, LV_OBJ_FLAG_SCROLLABLE);
    s_nSlotRows = 0;
    names(*cl);
    for (uint8_t i = 0; i < cl->memberCount() && s_nSlotRows < GROUP_MEMBERS_MAX; i++) {
        if (movedOut(cl->memberAt(i))) continue;   // its card is on the page: not the cluster's to show
        // SENSORS ONLY (round 10, the owner): a light or a switch is a tile. One
        // already in the header is listed, so it can be taken out.
        const bool in = inList(idOf(cl->memberAt(i)), ids, nIds);
        const Entity *e = cl->memberAt(i) ? cl->memberAt(i)->firstEntity() : nullptr;
        const bool sensor = e && (e->desc.kind == EntityKind::SENSOR || e->desc.kind == EntityKind::BINARY_SENSOR);
        if (!e || (!sensor && !in)) continue;
        const uint8_t row = s_nSlotRows++;
        s_slotMember[row] = i;
        s_slotOn[row]     = in;
        s_slotRow[row]    = memberRow(box, s_names[i], *e, MemberShown::SHOWN_TILE);
        lv_obj_add_event_cb(s_slotRow[row], slotRowCb, LV_EVENT_CLICKED, (void *)(uintptr_t)row);
    }
    if (!s_nSlotRows) note(box, "This cluster has no sensors to show in its header.");
    // Satin or glass, painted at the rows' laid-out size, which only the layout knows.
    lv_obj_update_layout(col);
    for (uint8_t r = 0; r < s_nSlotRows; r++) slotLook(r);
}

void clusterRename() { renameCb(nullptr); }

bool clusterTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

}  // namespace popup
