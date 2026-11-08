// A CLUSTER'S WINDOW IS ITS MEMBERS (2.11 round 16, the owner: the Details view
// held settings, not details). A long press on its header (K107) opens Members -
// every member, hidden ones too, each opening its own view, whose Visibility
// shows or hides it - with the CLUSTER panel beside SETTINGS:
//
//   Rename group - the group's name on the keyboard (K95, K146); Header options - which
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
#include "Cards/CardWhere.h"   // 2.13: what a picker row says it already is
#include "UI/HaOffer.h"        // 2.13: Add cards from Home Assistant
#include "UI/EditCards.h"      // 2.13 round 2: Edit cards (K148)
#include "HaCatalog.h"         // AREAS_MAX: the Area row's choices
#include "HaValue.h"           // haDescriptor(): an HA row's header glyph (K168)
#include "PsramArray.h"
#include <stdarg.h>

namespace popup {

namespace {

bool s_rebuildPage = false;

// THE PAGE'S CARDS THAT CAN JOIN (K91): one entity each, not a cluster, not in
// this one already - Edit cards' "On the page".
constexpr uint8_t ADD_MAX = 16;
Card       *s_cand[ADD_MAX] = {};
uint8_t     s_nCand = 0;

// THE OPTIONS ARE CHIP TABLES (round 4, the owner: "look more like the Card UI or
// UI Tweaks sheet"): an option's name right-justified against its chips.
const char *const STYLE_W[]  = { "Inherit", "Swatch", "Band" };   // "Band", as the label type's (round 10); saved as "bar"
const char *const STYLE_IS[] = { nullptr, "swatch", "bar" };
const char *const ICONS_W[]  = { "Show", "Hide" };
constexpr uint8_t ROW_ICONS = 0, ROW_STYLE = 1;   // Header options' chip rows, under the sensors (round 16)

lv_obj_t *s_table = nullptr;   // the view's chip table

// The group the panel is about: the held cluster, or a group shown as one card's
// cluster made again (K155) - its GROUP panel is the same panel.
ClusterCard *held() { return heldGroup(); }

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

// THE LISTS NAME A MEMBER IN FULL (owner, after v0.2.19 and again after round 1:
// "Eric Bedroom Temperature", never "Eric"): HA's name, always, in both lists.
// The tile keeps the dashboard's short label.
void nameOf(const Card *m, char *out, size_t cap) {
    const Entity *e = m ? m->firstEntity() : nullptr;
    snprintf(out, cap, "%s", e ? e->desc.name : m ? m->shownName(CardLabel::LBL_NAME) : "?");
}

}  // namespace

// Each member's name in the rows - Members' and Header options' alike; two
// called the same ("Office", its temperature and its occupancy) say what each is.
static char s_names[GROUP_MEMBERS_MAX][ENTITY_NAME_MAX + 24];
void clusterMemberNames(const ClusterCard &cl) {
    const uint8_t n = (uint8_t)LV_MIN(cl.memberCount(), GROUP_MEMBERS_MAX);
    bool twin[GROUP_MEMBERS_MAX] = {};
    for (uint8_t i = 0; i < n; i++) nameOf(cl.memberAt(i), s_names[i], ENTITY_NAME_MAX);
    for (uint8_t i = 0; i < n; i++)
        for (uint8_t j = 0; j < n && !twin[i]; j++) twin[i] = j != i && !strcmp(s_names[j], s_names[i]);
    for (uint8_t i = 0; i < n; i++) {
        const Entity *e = cl.memberAt(i) ? cl.memberAt(i)->firstEntity() : nullptr;
        char w[32];
        if (twin[i] && e) {
            const size_t len = strlen(s_names[i]);
            snprintf(s_names[i] + len, sizeof(s_names[i]) - len, " (%s)", whatWord(*e, w, sizeof(w)));
        }
    }
}

const char *clusterMemberName(uint8_t i) { return i < GROUP_MEMBERS_MAX ? s_names[i] : "?"; }

namespace {

int32_t listW() { return lv_area_get_width(&s.winRect) - 2 * s.pad; }

void note(lv_obj_t *list, const char *words) {
    lv_obj_t *l = Kit::label(list, KS::KS_LABEL_TAG_DIM);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, words);
    Kit::helperNote(l);   // UI Tweaks' Show helper text (2.13)
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

// RENAME (K95; K146: a group keeps one name): the GROUP is renamed, so every
// card wearing it says the new name too - its colour never follows a rename
// (K46). None, and a group from an HA area or the dashboard has its own name
// again; a group made here keeps the one it has. A cluster's own words from
// before K146 (label_text) go with the first rename.
void renamed(const char *words, void *) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId() || s.phase != PopupPhase::PHASE_OPEN) return;
    CardGroups::Group g;
    if (!CardGroups::ofCard(*cl, g)) return;
    const bool hasBase = g.haName[0] || !strncmp(g.id, "sheet_", 6);
    if (words[0])     Settings::setGroup(g.id, "name", words);
    else if (hasBase) Settings::setGroup(g.id, "name", nullptr);
    Settings::setCard(cl->id(), "label_text", nullptr);
    CardGroups::apply(*cl, cl->pageAreaColor());
    snprintf(s.area, sizeof(s.area), "%s", cl->area());
    // A cluster is called its group's name; a solo card keeps its own (K146).
    if (s.cluster) snprintf(s.name, sizeof(s.name), "%s", cl->area()[0] ? cl->area() : cl->shownName(CardLabel::LBL_NAME));
    changed();
    rebuildView();   // the title says it at once; the cluster when the window closes
}

void renameCb(lv_event_t *ev) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId()) return;
    if (ev) chartSet(DECK_PEEK);   // from the CLUSTER panel: it folds under the keyboard
    static char now[40], dflt[48];
    snprintf(now, sizeof(now), "%s", s.name);   // as it is shown: its group's, or its own words from before K146
    CardGroups::Group g;
    snprintf(dflt, sizeof(dflt), "%s", CardGroups::ofCard(*cl, g) && g.haName[0] ? g.haName : now);
    Kit::KeyboardSpec k = {};
    k.title  = "Name this group";   // K146
    k.text   = now;
    k.hint   = dflt;           // shown when the field is empty: what it is called then
    k.maxLen = sizeof(now) - 1;
    k.save   = renamed;
    Kit::keyboard(k);
}

// The GROUP panel folds as the view opens, as Visibility's and Card UI's do (owner, 2026-11-06).
// GROUP is the left panel (CHART's place, PopupPanels.cpp's sideTitle) - folding only
// the right one left it open over Header options (F46, owner 11-07).
void slotsViewCb(lv_event_t *ev) {
    (void)ev;
    deckSet(DECK_PEEK);
    chartSet(DECK_PEEK);
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
// One moved out to the page is not in the group (K161: Table, moved out of
// Kitchen, was counted in and could not be added back).
bool memberHas(const ClusterCard &cl, const Entity &e) {
    for (uint8_t i = 0; i < cl.memberCount(); i++) {
        const Card *m = cl.memberAt(i);
        if (movedOut(m)) continue;
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

void addViewCb(lv_event_t *) {
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CLUSTER_ADD); }, nullptr);
}

}  // namespace

// The cards picked (by their place in the page's list), for /popup's addcards=:
// a copy of each into the cluster, with the name it shows; one to Move also
// hides from the page. The window closes and the page is built again. How many
// went in.
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

// --- Edit cards (2.13 round 2, K148, K149; mocks 8 and 8b) -------------------------------
// The group's members, the page's cards that can join and HA's areas in one list
// (UI/EditCards): ticked to add, to hide or remove, or to restore - one kind at a
// time - then the go-or-no-go question; the window closes and the page is built
// again.
namespace {

const char *haHeld(const HaOffer::Row &r, void *ctx) {
    static char words[56];
    return (r.have && CardWhere::words(*r.have, (const Card *)ctx, HaOffer::registry(), words, sizeof(words)))
               ? words : nullptr;
}

int8_t memberIndexOf(const ClusterCard &cl, const char *id) {
    for (uint8_t i = 0; i < cl.memberCount(); i++)
        if (cl.memberAt(i) && cl.memberAt(i)->hasId() && !strcmp(cl.memberAt(i)->id(), id)) return (int8_t)i;
    return -1;
}

// IN THIS GROUP (K160, K161): every member, the pinned first - each ending in its
// square (its header glyph, a pin, or nothing) and each a tap cycle. One moved out
// to the page is not here: it is the page's card, under On the page.
uint8_t fillMembers(EditCards::Item *out, uint8_t cap, void *) {
    ClusterCard *cl = held();
    if (!cl) return 0;
    clusterMemberNames(*cl);
    uint8_t n = 0;
    for (int pass = 0; pass < 2; pass++)
        for (uint8_t i = 0; i < cl->memberCount() && n < cap; i++) {
            const Card *m = cl->memberAt(i);
            const Entity *e = m ? m->firstEntity() : nullptr;
            if (!e || movedOut(m)) continue;
            const bool pinned = clusterMemberPinned(*cl, i);
            if (pinned != (pass == 0)) continue;
            EditCards::Item &it = out[n++];
            it = EditCards::Item{};
            snprintf(it.key, sizeof(it.key), "%s", idOf(m));
            if (e->desc.source == EntitySource::HA) snprintf(it.ref, sizeof(it.ref), "%s", e->desc.externalRef);   // 1.5
            snprintf(it.name, sizeof(it.name), "%s", clusterMemberName(i));
            char st[48];
            memberState(*e, MemberShown::SHOWN_TILE, st, sizeof(st));
            snprintf(it.state, sizeof(it.state), "%s", st);
            it.from   = EditCards::From::FROM_GROUP;
            it.pinned = pinned;
            if (clusterMemberInHeader(*cl, i)) it.signIcon = headerGlyphOf(*e, it.signTint);
            else if (pinned)                   it.signIcon = MDI_PIN;
            if (!m->hasId()) continue;   // nothing can be kept for it
            it.tick = EditCards::Tick::TICK_HIDE;   // a member: its tap cycle
            // A copy made here may go for good; a card moved in goes home by Restore (K137).
            it.canRemove = CardAdded::isMember(m->id()) && !CardAdded::isMoveOf(m->id());
        }
    return n;
}

// ON THE PAGE: the page's cards that can join - one entity, not a group, not in
// this one already (findCandidates()).
uint8_t fillPage(EditCards::Item *out, uint8_t cap, void *) {
    ClusterCard *cl = held();
    if (!cl) return 0;
    s_nCand = 0;
    findCandidates(lv_screen_active(), *cl);
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nCand && n < cap; i++) {
        const Card *k = s_cand[i];
        EditCards::Item &it = out[n++];
        it = EditCards::Item{};
        snprintf(it.key, sizeof(it.key), "%s", k->id());
        if (k->firstEntity()->desc.source == EntitySource::HA)
            snprintf(it.ref, sizeof(it.ref), "%s", k->firstEntity()->desc.externalRef);   // 1.5
        snprintf(it.name, sizeof(it.name), "%s", k->shownName(CardLabel::LBL_NAME));
        char st[48];
        memberState(*k->firstEntity(), MemberShown::SHOWN_TILE, st, sizeof(st));
        snprintf(it.state, sizeof(it.state), "%s", st);
        it.from = EditCards::From::FROM_PAGE;
        it.tick = EditCards::Tick::TICK_ADD;
        const EntityKind kind = k->firstEntity()->desc.kind;
        it.sensor = kind == EntityKind::SENSOR || kind == EntityKind::BINARY_SENSOR;   // K168
        if (it.sensor) it.signIcon = headerGlyphOf(*k->firstEntity(), it.signTint);
    }
    return n;
}

// The glyph this group's header would show for an HA row not on the board yet
// (K168, a sensor's third tap): its kind as HA describes it.
const char *haRowGlyph(const HaOffer::Row &r, uint32_t &tint) {
    if (r.have) return headerGlyphOf(*r.have, tint);
    return cardHeaderGlyphFor(r.ref, r.dc, r.unit, tint);
}

// INTO THE HEADER (K168): `id` after the group's header members, four at most.
void headerAppend(const ClusterCard &cl, const char *id) {
    char list[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1] = "";
    uint8_t n = 0;
    for (uint8_t i = 0; i < cl.memberCount() && n < CLUSTER_SLOTS_MAX; i++) {
        if (!clusterMemberInHeader(cl, i)) continue;
        const Card *m = cl.memberAt(i);
        if (!m || !m->hasId()) continue;
        snprintf(list + strlen(list), sizeof(list) - strlen(list), "%s%s", n ? "," : "", m->id());
        n++;
    }
    if (n >= CLUSTER_SLOTS_MAX) return;   // full: it stays a member, unpinned
    snprintf(list + strlen(list), sizeof(list) - strlen(list), "%s%s", n ? "," : "", id);
    Settings::setCard(cl.id(), "slots", list);
}

// The dialog's words: four lines at most, names in the accent (as PopupMove's).
char        s_eLine[4][420];
const char *s_eLines[4];
uint8_t     s_eN = 0;

void eLine(const char *fmt, ...) {
    if (s_eN >= 4) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_eLine[s_eN], sizeof(s_eLine[0]), fmt, ap);
    va_end(ap);
    s_eLines[s_eN] = s_eLine[s_eN];
    s_eN++;
}

const char *eAcc(char *buf, size_t cap, const char *name) {
    snprintf(buf, cap, "#%06lX %s#", (unsigned long)(UI::pal().ACCENT & 0xFFFFFF), name ? name : "");
    return buf;
}

// What was ticked when the question was asked.
PsramArray<EditCards::Item, 24> s_eItems;   // ~7 KB
uint8_t         s_eNItems = 0;
bool            s_eMove = false;

void editDone(uint8_t n, const char *fmt) {
    if (!n) return;
    char msg[64];
    snprintf(msg, sizeof(msg), fmt, (unsigned)n, s.name);
    UIToolkit::show_toast(msg);
    EditCards::forget();
    changed();
    CardPopup::close();
}

// ADD: HA's entities as members made here ("in"), each one the compiled table
// lacks keeping its HA id (design/ha-cards.md 2.3); the page's cards copied in,
// or moved - off the page, not one of its hidden cards (round 7).
void doEditAdd() {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId()) return;
    s_nCand = 0;
    findCandidates(lv_screen_active(), *cl);
    uint8_t added = 0;
    for (uint8_t i = 0; i < s_eNItems; i++) {
        const EditCards::Item &it = s_eItems[i];
        char id[CARD_ID_MAX + 1];
        if (it.from == EditCards::From::FROM_HA) {
            HaOffer::Row r;
            const Entity *e = EditCards::haRowOf(it.key, r) ? HaOffer::entityFor(r) : nullptr;
            if (!e) continue;
            if (!CardAdded::addTo(*cl, *e, id, sizeof(id))) break;   // full
            if (e->learnt) {
                Settings::setCard(id, "ha", r.ref);
                if (r.dc[0])   Settings::setCard(id, "ha_dc", r.dc);
                if (r.unit[0]) Settings::setCard(id, "ha_unit", r.unit);
            }
            added++;
        } else if (it.from == EditCards::From::FROM_PAGE) {
            Card *k = nullptr;
            for (uint8_t c = 0; c < s_nCand && !k; c++) if (!strcmp(s_cand[c]->id(), it.key)) k = s_cand[c];
            if (!k) continue;
            if (!CardAdded::addTo(*cl, *k->firstEntity(), id, sizeof(id), k->id())) break;
            const char *name = k->shownName(CardLabel::LBL_NAME);
            if (name && name[0]) Settings::setCard(id, "name", name);
            if (s_eMove) CardAdded::moveOut(k->id(), id);
            added++;
        }
    }
    editDone(added, "Added %u to %s");
}

void doEditHide() {
    ClusterCard *cl = held();
    if (!cl) return;
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_eNItems; i++) {
        const int8_t k = memberIndexOf(*cl, s_eItems[i].key);
        if (k >= 0) { setVisible(*cl, (uint8_t)k, false); n++; }
    }
    editDone(n, "Hid %u in %s");   // to confirm
}

void doEditRemove() {
    ClusterCard *cl = held();
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_eNItems; i++) n += CardAdded::removeMember(s_eItems[i].key) ? 1 : 0;
    if (n && cl && cl->hasId()) CardAdded::emptied(cl->id());   // the last one out: the group goes
    editDone(n, "Removed %u from %s");   // to confirm
}

void doEditRestore() {
    ClusterCard *cl = held();
    if (!cl) return;
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_eNItems; i++) {
        const int8_t k = memberIndexOf(*cl, s_eItems[i].key);
        if (k >= 0) { setVisible(*cl, (uint8_t)k, true); n++; }
    }
    editDone(n, "Restored %u to %s");   // to confirm
}

// SAVE N (K161): every change the taps made, at once. A card added on its first
// tap joins unpinned (K160), on its second pinned; a page card that came from
// this group goes home, as its Restore would (K137). A member flips its pin, or
// - marked red - is removed. Then the toast, and the page built again.
void doEditSave() {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId()) return;
    s_nCand = 0;
    findCandidates(lv_screen_active(), *cl);
    uint8_t done = 0;
    for (uint8_t i = 0; i < s_eNItems; i++) {
        const EditCards::Item &it = s_eItems[i];
        char id[CARD_ID_MAX + 1] = "";
        if (it.from == EditCards::From::FROM_GROUP) {
            const int8_t k = memberIndexOf(*cl, it.key);
            if (it.step == 2)  done += CardAdded::removeMember(it.key) ? 1 : 0;
            else if (k >= 0) { clusterMemberShow(*cl, (uint8_t)k, !it.pinned); done++; }
            continue;
        }
        if (it.from == EditCards::From::FROM_HA) {
            HaOffer::Row r;
            const Entity *e = EditCards::haRowOf(it.key, r) ? HaOffer::entityFor(r) : nullptr;
            if (!e) continue;
            if (!CardAdded::addTo(*cl, *e, id, sizeof(id))) break;   // full
            if (e->learnt) {
                Settings::setCard(id, "ha", r.ref);
                if (r.dc[0])   Settings::setCard(id, "ha_dc", r.dc);
                if (r.unit[0]) Settings::setCard(id, "ha_unit", r.unit);
            }
        } else {
            Card *k = nullptr;
            for (uint8_t c = 0; c < s_nCand && !k; c++) if (!strcmp(s_cand[c]->id(), it.key)) k = s_cand[c];
            if (!k) continue;
            const char *from = CardAdded::fromOf(k->id());
            if (CardAdded::movedHere(k->id()) && from && memberIndexOf(*cl, from) >= 0) {
                // Home again: the page card goes, the member it was comes back.
                snprintf(id, sizeof(id), "%s", from);
                if (!CardAdded::moveBack(k->id())) continue;
            } else {
                if (!CardAdded::addTo(*cl, *k->firstEntity(), id, sizeof(id), k->id())) break;
                const char *name = k->shownName(CardLabel::LBL_NAME);
                if (name && name[0]) Settings::setCard(id, "name", name);
                if (s_eMove) CardAdded::moveOut(k->id(), id);
            }
        }
        // Unpinned unless tapped twice (K160): new members never move the page. A
        // sensor's third tap: in the header, not a tile (K168).
        Settings::setCard(id, "hidden", it.step == 2 ? nullptr : "yes");
        if (it.step == 3) headerAppend(*cl, id);
        done++;
    }
    if (!done) return;
    CardAdded::emptied(cl->id());   // every member removed and none added: the group goes
    char msg[64];
    snprintf(msg, sizeof(msg), "Saved %u change%s to %s", (unsigned)done, done == 1 ? "" : "s", s.name);   // the owner's (K161)
    UIToolkit::show_toast(msg);
    EditCards::forget();
    changed();
    CardPopup::close();
}

// EVERY CHOICE BEFORE THE QUESTION (K149): the dialog is go or no-go, in the
// existing dialogs' sentences made plural (round-2 mocks 12 and 13).
void editAct(EditCards::Act a, void *) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId()) return;
    s_eNItems = EditCards::chosen(s_eItems, 24);
    s_eMove = EditCards::moving();
    if (!s_eNItems) return;
    static char g[96], names[400], more[400];
    eAcc(g, sizeof(g), s.name);
    s_eN = 0;
    const bool many = s_eNItems > 1;
    using F = EditCards::From;
    // SAVE (K161) ALWAYS ASKS (owner, 2026-11-05, K165: every change to a group's
    // membership): one line per kind of change - added, pinned, unpinned, removed -
    // each naming its cards. The words follow K149's; to confirm (round 5).
    if (a == EditCards::Act::ACT_SAVE) {
        enum Kind : uint8_t { K_ADD, K_PIN, K_UNPIN, K_GONE, K_HEADER, K_KINDS };
        auto kindsOf = [](const EditCards::Item &it, bool out[K_KINDS]) {
            for (uint8_t k = 0; k < K_KINDS; k++) out[k] = false;
            if (it.from == F::FROM_GROUP) {
                if (it.step == 2) out[K_GONE] = true;
                else              out[it.pinned ? K_UNPIN : K_PIN] = true;
            } else {
                out[K_ADD] = true;
                if (it.step == 2) out[K_PIN] = true;      // added pinned
                if (it.step == 3) out[K_HEADER] = true;   // added to the header (K168)
            }
        };
        static char list[K_KINDS][300];
        uint8_t cnt[K_KINDS] = {};
        bool in[K_KINDS];
        for (uint8_t i = 0; i < s_eNItems; i++) { kindsOf(s_eItems[i], in); for (uint8_t k = 0; k < K_KINDS; k++) cnt[k] += in[k]; }
        const unsigned long acc = (unsigned long)(UI::pal().ACCENT & 0xFFFFFF);
        for (uint8_t k = 0; k < K_KINDS; k++) {
            list[k][0] = '\0';
            uint8_t at = 0;
            for (uint8_t i = 0; i < s_eNItems; i++) {
                kindsOf(s_eItems[i], in);
                if (!in[k]) continue;
                const size_t used = strlen(list[k]);
                snprintf(list[k] + used, sizeof(list[k]) - used, "%s#%06lX %s#",
                         at == 0 ? "" : (at + 1 == cnt[k] ? " and " : ", "), acc, s_eItems[i].name);
                at++;
            }
        }
        if (cnt[K_ADD])
            eLine(cnt[K_ADD] > 1 ? "The cards %s will be added to the group %s." : "The card %s will be added to the group %s.",
                  list[K_ADD], g);
        if (cnt[K_PIN])
            eLine(cnt[K_PIN] > 1 ? "The cards %s will be pinned, and visible from the page in the group %s."
                                 : "The card %s will be pinned, and visible from the page in the group %s.", list[K_PIN], g);
        if (cnt[K_HEADER])
            eLine(cnt[K_HEADER] > 1 ? "The cards %s will be shown in the header of %s."
                                    : "The card %s will be shown in the header of %s.", list[K_HEADER], g);
        if (cnt[K_UNPIN])
            eLine(cnt[K_UNPIN] > 1 ? "The cards %s will be unpinned: still members of %s, shown in its 'Group view' and 'Members view'."
                                   : "The card %s will be unpinned: still a member of %s, shown in its 'Group view' and 'Members view'.",
                  list[K_UNPIN], g);
        if (cnt[K_GONE])
            eLine(cnt[K_GONE] > 1 ? "The cards %s will be removed from this device. Nothing will be removed from Home Assistant."
                                  : "The card %s will be removed from this device. Nothing will be removed from Home Assistant.",
                  list[K_GONE]);
        if (!s_eN) { doEditSave(); return; }
        char title[48];
        static char titleKeep[48];
        snprintf(title, sizeof(title), "Save %u change%s?", (unsigned)s_eNItems, s_eNItems == 1 ? "" : "s");
        snprintf(titleKeep, sizeof(titleKeep), "%s", title);
        confirmAsk(cnt[K_GONE] ? Kit::DialogRole::DLG_DESTRUCTIVE : Kit::DialogRole::DLG_CONFIRM, titleKeep, s_eLines,
                   s_eN, "Save", cnt[K_GONE] ? Kit::ButtonRole::BTN_DESTRUCTIVE : Kit::ButtonRole::BTN_CONFIRM, doEditSave);
        return;
    }
    if (a == EditCards::Act::ACT_ADD) {
        const uint8_t nHa = EditCards::countOf(F::FROM_HA), nPg = EditCards::countOf(F::FROM_PAGE);
        if (nHa) {
            EditCards::namesOf(names, sizeof(names), true, F::FROM_HA);
            eLine(nHa > 1 ? "The cards %s will be added from Home Assistant to the group %s."
                          : "The card %s will be added from Home Assistant to the group %s.", names, g);
        }
        if (nPg) {
            EditCards::namesOf(more, sizeof(more), true, F::FROM_PAGE);
            if (s_eMove) {
                eLine(nPg > 1 ? "The cards %s will be removed from this page and moved to the group %s."
                              : "The card %s will be removed from this page and moved to the group %s.", more, g);
                eLine(nPg > 1 ? "The moved cards will become members of the %s group, and settings applied to that "
                                "group may be propagated to the new cards."
                              : "The moved card will become a member of the %s group, and settings applied to that "
                                "group may be propagated to the new card.", g);
            } else {
                eLine(nPg > 1 ? "The cards %s will be copied to the group %s and a copy of each will remain on this page."
                              : "The card %s will be copied to the group %s and a copy will remain on this page.", more, g);
            }
        }
        eLine("A card can always be restored to its original configuration.");
        confirmAsk(Kit::DialogRole::DLG_CONFIRM, many ? "Add cards to group?" : "Add card to group?", s_eLines, s_eN,
                   "Confirm", Kit::ButtonRole::BTN_CONFIRM, doEditAdd);
        return;
    }
    EditCards::namesOf(names, sizeof(names));
    if (a == EditCards::Act::ACT_HIDE) {
        eLine(many ? "The cards %s will be hidden from the group %s." : "The card %s will be hidden from the group %s.",
              names, g);
        eLine("Hidden cards still show in the group view, and can always be restored.");
        confirmAsk(Kit::DialogRole::DLG_DESTRUCTIVE, many ? "Hide these cards?" : "Hide this card?", s_eLines, s_eN,
                   "Hide", Kit::ButtonRole::BTN_DESTRUCTIVE, doEditHide);
    } else if (a == EditCards::Act::ACT_REMOVE) {
        eLine(many ? "The cards %s will be removed from this device." : "The card %s will be removed from this device.",
              names);
        eLine("Nothing will be removed from Home Assistant. You can always add these entities back to your device.");
        eLine(many ? "Tap Remove to confirm you wish to remove these cards from the device."
                   : "Tap Remove to confirm you wish to remove this card from the device.");
        confirmAsk(Kit::DialogRole::DLG_DESTRUCTIVE, many ? "Remove these cards?" : "Remove this card?", s_eLines, s_eN,
                   "Remove", Kit::ButtonRole::BTN_DESTRUCTIVE, doEditRemove);
    } else {
        // RESTORE ASKS TOO (K165: every Edit cards action). Words to confirm (round 5).
        eLine(many ? "The cards %s will be restored to their original configuration."
                   : "The card %s will be restored to its original configuration.", names);
        confirmAsk(Kit::DialogRole::DLG_CONFIRM, many ? "Restore these cards?" : "Restore this card?", s_eLines, s_eN,
                   "Restore", Kit::ButtonRole::BTN_CONFIRM, doEditRestore);
    }
}

}  // namespace

// EDIT CARDS (K148): the window has the page's room (groupWindowFor()), the list all of it.
void buildClusterAdd() {
    ClusterCard *cl = held();
    if (!cl) return;
    HaOffer::refresh();   // what HA offers now
    lv_obj_update_layout(s.win);
    EditCards::Spec sp;
    sp.lists[0] = { "In this group", fillMembers, true };   // K146
    sp.lists[1] = { "On the page", fillPage, false };       // K145
    sp.nLists   = 2;
    sp.held     = haHeld;
    sp.ctx      = cl;
    sp.copyMove = true;
    sp.cycle    = true;   // K161
    sp.haGlyph  = haRowGlyph;   // K168: a sensor's third tap
    sp.act      = editAct;
    EditCards::build(s.stage, lv_obj_get_content_width(s.stage), lv_obj_get_content_height(s.stage), sp);
}

// --- A member pinned or not (K160; its own Visibility, Edit cards) ---------------------
// Unpinned, it leaves the header too: the page shows no part of it.
static void headerDrop(const ClusterCard &cl, uint8_t i) {
    const Card *m = cl.memberAt(i);
    if (!m || !m->hasId() || !cl.hasId()) return;
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t n = headerIds(cl, ids, buf, sizeof(buf));
    if (!inList(m->id(), ids, n)) return;
    char list[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1] = "";
    uint8_t kept = 0;
    for (uint8_t k = 0; k < n; k++) {
        if (!strcmp(ids[k], m->id())) continue;
        snprintf(list + strlen(list), sizeof(list) - strlen(list), "%s%s", kept++ ? "," : "", ids[k]);
    }
    Settings::setCard(cl.id(), "slots", list);
}

void clusterMemberShow(const ClusterCard &cl, uint8_t i, bool want) {
    if (!want) headerDrop(cl, i);
    setVisible(cl, i, want);
    changed();
}

bool clusterMemberPinned(const ClusterCard &cl, uint8_t i) {
    const MemberShown sh = clusterMemberShown(cl, i);
    return sh == MemberShown::SHOWN_TILE || sh == MemberShown::SHOWN_HEADER;
}

MemberShown clusterMemberShown(const ClusterCard &cl, uint8_t i) {
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t n = headerIds(cl, ids, buf, sizeof(buf));
    if (movedOut(cl.memberAt(i))) return MemberShown::SHOWN_MOVED;
    if (visible(cl, i, ids, n)) return MemberShown::SHOWN_TILE;
    return inList(idOf(cl.memberAt(i)), ids, n) ? MemberShown::SHOWN_HEADER : MemberShown::SHOWN_HIDDEN;
}

bool clusterMemberInHeader(const ClusterCard &cl, uint8_t i) {
    const char *ids[CLUSTER_SLOTS_MAX];
    char buf[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1];
    const uint8_t n = headerIds(cl, ids, buf, sizeof(buf));
    return !movedOut(cl.memberAt(i)) && inList(idOf(cl.memberAt(i)), ids, n);
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
// AREA (2.13, K141): the HA area or floor the cluster's GROUP is linked to -
// None, then each floor and its areas. A choice changes the group's link, so
// every card in that group follows it.
static char    s_areaOpts[768];
static char    s_areaIds[HaCatalog::AREAS_MAX + 8][32];
static uint8_t s_areaN = 0;
static char    s_addWords[12];

// SHOW AS (K151): the group as a cluster or as one card - the members that switch
// together behind it, or the one Face picks. The page is built again once the
// window has gone. Here in every presentation (K155).
// Since 2.13 round 5 (K174) a chip row in Visibility, with Rename group: what the
// group looks like and does on the page.
void groupShowAs(bool asCard) {
    ClusterCard *cl = held();
    if (!cl || !cl->hasId()) return;
    Settings::setCard(cl->id(), "present", asCard ? "card" : "cluster");
    changed();
}

// FACE (K151): which member a group shown as one card shows - the members that
// switch together ("All lights"), or one. Greyed while it is a cluster (K155).
static char    s_faceOpts[400];
static char    s_faceIds[GROUP_MEMBERS_MAX][CARD_ID_MAX + 1];
static uint8_t s_faceN = 0;

static uint16_t faceRowBuild(const ClusterCard &cl) {
    bool lights = false;
    for (uint8_t k = 0; k < cl.memberCount(); k++) {
        const Entity *e = cl.memberAt(k) ? cl.memberAt(k)->firstEntity() : nullptr;
        lights |= e && (e->desc.kind == EntityKind::LIGHT || e->desc.kind == EntityKind::SWITCH);
    }
    size_t used = (size_t)snprintf(s_faceOpts, sizeof(s_faceOpts), "%s", lights ? "All lights" : "All members");
    s_faceN = 0;
    char cur[CARD_ID_MAX + 1] = "";
    if (cl.hasId()) Settings::card(cl.id(), "face", cur, sizeof(cur));
    uint16_t sel = 0;
    for (uint8_t k = 0; k < cl.memberCount() && s_faceN < GROUP_MEMBERS_MAX; k++) {
        const Card *m = cl.memberAt(k);
        if (!m || !m->hasId() || !m->firstEntity() || movedOut(m)) continue;
        const char *name = m->shownName(CardLabel::LBL_NAME);
        if (used + strlen(name) + 2 >= sizeof(s_faceOpts)) break;
        used += (size_t)snprintf(s_faceOpts + used, sizeof(s_faceOpts) - used, "\n%s", name);
        snprintf(s_faceIds[s_faceN], sizeof(s_faceIds[0]), "%s", m->id());
        if (!strcmp(cur, m->id())) sel = (uint16_t)(s_faceN + 1);
        s_faceN++;
    }
    return sel;
}

static void faceDropCb(lv_event_t *ev) {
    const uint32_t k = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    ClusterCard *cl = held();
    if (!cl || !cl->hasId() || k > s_faceN) return;
    Settings::setCard(cl->id(), "face", k ? s_faceIds[k - 1] : nullptr);
    changed();
}

static void areaDropCb(lv_event_t *ev) {
    const uint32_t k = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    ClusterCard *cl = held();
    CardGroups::Group g;
    if (!cl || k >= s_areaN || !CardGroups::ofCard(*cl, g)) return;
    HaOffer::setGroupArea(g.id, k ? s_areaIds[k] : "none");
    changed();
}

static uint16_t areaOptions(const char *current) {
    static PsramArray<HaOffer::AreaRow, HaCatalog::AREAS_MAX> rows;   // ~6.5 KB: not on the loop task's stack
    const uint8_t na = HaOffer::areas(rows, HaCatalog::AREAS_MAX);
    size_t used = (size_t)snprintf(s_areaOpts, sizeof(s_areaOpts), "None");
    s_areaIds[0][0] = '\0';
    s_areaN = 1;
    uint16_t sel = 0;
    auto add = [&](const char *id, const char *words) {
        if (s_areaN >= sizeof(s_areaIds) / sizeof(s_areaIds[0]) || used + strlen(words) + 2 >= sizeof(s_areaOpts)) return;
        used += (size_t)snprintf(s_areaOpts + used, sizeof(s_areaOpts) - used, "\n%s", words);
        snprintf(s_areaIds[s_areaN], sizeof(s_areaIds[0]), "%s", id);
        if (current && !strcmp(current, id)) sel = s_areaN;
        s_areaN++;
    };
    const char *floor = nullptr;
    for (uint8_t i = 0; i < na; i++) {
        if (!rows[i].id[0]) continue;   // No area is no place to link to
        if (rows[i].floor[0] && (!floor || strcmp(floor, rows[i].floor))) {
            add(rows[i].floor, rows[i].floorName);   // the floor itself: Outside
            floor = rows[i].floor;
        }
        add(rows[i].id, rows[i].name);
    }
    return sel;
}

uint8_t clusterSpecs(DeckRowSpec *r) {
    ClusterCard *cl = held();
    const bool live = cl && cl->hasId();
    uint8_t n = 0;
    // ITS AREA'S NEW ENTITIES (K142): "1 new" on Add cards until it has shown them.
    char area[40] = "";
    CardGroups::Group g;
    if (cl && CardGroups::ofCard(*cl, g)) HaOffer::groupArea(g.id, area, sizeof(area));
    const uint8_t fresh = area[0] ? HaOffer::newIn(area) : 0;
    snprintf(s_addWords, sizeof(s_addWords), fresh ? "%u new" : "Open", (unsigned)fresh);   // K145
    // THE SAME ROWS IN EVERY PRESENTATION (K155): what does not apply greyed -
    // Header options while the group is one card, Face while it is a cluster.
    // WHAT CONCERNS ITS MEMBERS (owner, 2026-11-05, K174): Rename group and Show as
    // went to Visibility, with the rest of how it stands on the page.
    r[n++] = { "Header options", DeckRowKind::ROW_BUTTON, "Open", 0, live && s.cluster, slotsViewCb, nullptr };
    r[n++] = { "Edit cards", DeckRowKind::ROW_BUTTON, s_addWords, 0, live, addViewCb, nullptr };   // K148
    const uint16_t face = cl ? faceRowBuild(*cl) : 0;
    if (!cl) snprintf(s_faceOpts, sizeof(s_faceOpts), "All members");
    r[n++] = { "Face", DeckRowKind::ROW_DROP, s_faceOpts, face, live && !s.cluster, faceDropCb, nullptr };   // K151
    const uint16_t sel = areaOptions(area);
    r[n++] = { "Area", DeckRowKind::ROW_DROP, s_areaOpts, sel, live && HaOffer::ready(), areaDropCb, nullptr };   // K145
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
    clusterMemberNames(*cl);
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
    if (!s_nSlotRows) note(box, "This group has no sensors to show in its header.");
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
