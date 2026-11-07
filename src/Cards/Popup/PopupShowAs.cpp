// SHOW AS... (owner, 2026-11-06, K191; replaces K174's Show as chips, which applied
// at a tap with no word of it and nothing to confirm). A group's Visibility opens
// it; it walks as Add to dashboard does (PopupMove.cpp) - a choice per step, Next,
// then a dialog that says what will happen - and nothing changes until Confirm.
//
//   Show this group as:            Solo card, Group
//   Solo card  -> the face         "All lights" / "All members", then each member -
//                                  the one pinned card offered first (K191)
//              -> the tap          Toggle (every member a light or switch), Group
//                                  view, Members view, History view (K188)
//   Group      -> the pins         a check per member; at least two, or it stays a
//                                  solo card - a line says which (K191)
//
// Back goes one step back; from the first, to Visibility. Pins are never touched
// on the way to a solo card (K188: they are kept for the group).
#include "PopupState.h"
#include "Cards/ClusterCard.h"

namespace popup {

namespace {

enum class Step : uint8_t { SA_PRESENT, SA_FACE, SA_TAP, SA_PINS };

constexpr uint8_t FACE_MAX = 16;
constexpr uint8_t PIN_MAX  = Kit::CHIP_ROWS_MAX;
constexpr uint8_t TAP_MAX  = 4;

struct ShowAs {
    Step    step = Step::SA_PRESENT;
    bool    solo = true;
    uint8_t face = 0;      // 0 all, else s_faceIdx[face - 1]
    uint8_t tap  = 0;      // index in s_tapMap
    bool    pin[PIN_MAX] = {};
} a;

const char *s_faceW[FACE_MAX + 1];
uint8_t     s_faceIdx[FACE_MAX];   // a face chip's member
uint8_t     s_nFace = 0;
const char *s_tapW[TAP_MAX];
TapAction   s_tapMap[TAP_MAX];
uint8_t     s_nTap = 0;
uint8_t     s_pinIdx[PIN_MAX];     // a check row's member
uint8_t     s_nPin = 0;
lv_obj_t   *s_grid = nullptr, *s_table = nullptr, *s_says = nullptr;

int32_t listW() { return lv_area_get_width(&s.winRect) - 2 * s.pad; }

lv_obj_t *words(lv_obj_t *list, KS role, const char *w) {
    lv_obj_t *l = Kit::label(list, role);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, w);
    return l;
}

// A member the flow can use: a card with an id and an entity, not moved out.
bool usable(const Card *m) { return m && m->hasId() && m->firstEntity() && !CardAdded::isMovedToPage(m->id()); }

bool switches(const Entity *e) {
    return e && e->desc.writable && (e->desc.kind == EntityKind::LIGHT || e->desc.kind == EntityKind::SWITCH);
}

// Not every member a light or a switch: its solo card toggles nothing (K188). The
// header's sensors are the group view's, not cards on it: they do not count.
bool mixed(const ClusterCard &g) {
    for (uint8_t i = 0; i < g.memberCount(); i++)
        if (usable(g.memberAt(i)) && !clusterMemberInHeader(g, i) && !switches(g.memberAt(i)->firstEntity())) return true;
    return false;
}

uint8_t pinnedCount() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nPin; i++) n += a.pin[i] ? 1 : 0;
    return n;
}

// --- What each step offers ------------------------------------------------------------
void faceChoices(const ClusterCard &g) {
    s_nFace = 0;
    bool lights = false;
    for (uint8_t i = 0; i < g.memberCount(); i++) lights |= usable(g.memberAt(i)) && switches(g.memberAt(i)->firstEntity());
    s_faceW[0] = lights ? "All lights" : "All members";   // the Face row's words (K151)
    char cur[CARD_ID_MAX + 1] = "";
    if (g.hasId()) Settings::card(g.id(), "face", cur, sizeof(cur));
    int8_t only = -1;   // the one pinned card, when there is one (K191)
    uint8_t nPinned = 0;
    a.face = 0;
    for (uint8_t i = 0; i < g.memberCount() && s_nFace < FACE_MAX; i++) {
        const Card *m = g.memberAt(i);
        if (!usable(m)) continue;
        s_faceIdx[s_nFace] = i;
        s_faceW[s_nFace + 1] = m->shownName(CardLabel::LBL_NAME);
        if (cur[0] && !strcmp(cur, m->id())) a.face = (uint8_t)(s_nFace + 1);
        if (clusterMemberPinned(g, i)) { nPinned++; only = (int8_t)(s_nFace + 1); }
        s_nFace++;
    }
    if (!cur[0] && nPinned == 1) a.face = (uint8_t)only;
}

void tapChoices(const ClusterCard &g) {
    s_nTap = 0;
    const bool mx = mixed(g);
    auto add = [](TapAction t, const char *w) { if (s_nTap < TAP_MAX) { s_tapMap[s_nTap] = t; s_tapW[s_nTap++] = w; } };
    if (!mx) add(TapAction::TAP_TOGGLE, "Toggle");
    add(TapAction::TAP_GROUP, "Group view");
    add(TapAction::TAP_MEMBERS, "Members view");
    add(TapAction::TAP_HISTORY, "History view");
    // The card's own when it is one of these; else Toggle, or Members when mixed.
    TapAction want = mx ? TapAction::TAP_MEMBERS : TapAction::TAP_TOGGLE;
    char v[16];
    TapAction saved;
    if (g.hasId() && Settings::card(g.id(), "card_tap", v, sizeof(v)) && tapActionFromName(v, saved)) want = saved;
    a.tap = 0;
    for (uint8_t i = 0; i < s_nTap; i++) if (s_tapMap[i] == want) a.tap = i;
}

void pinChoices(const ClusterCard &g) {
    s_nPin = 0;
    for (uint8_t i = 0; i < g.memberCount() && s_nPin < PIN_MAX; i++) {
        // The header's sensors stand in the header, not as tiles: Header options' (K129).
        if (!usable(g.memberAt(i)) || clusterMemberInHeader(g, i)) continue;
        s_pinIdx[s_nPin] = i;
        a.pin[s_nPin] = clusterMemberPinned(g, i);
        s_nPin++;
    }
}

// The line under the pins: whether it will be a group (K191).
const char *pinsSay() {
    return pinnedCount() >= 2 ? "The group will be shown on the page with its pinned cards."
                              : "Fewer than two cards are pinned, so the group will still be shown as a solo card.";
}

// --- Done --------------------------------------------------------------------------------
void finish() {
    visibilityWantRebuild();
    CardPopup::close();   // the page is built again once the window has gone
}

void goSolo() {
    ClusterCard *g = heldGroup();
    if (!g || !g->hasId() || s.phase != PopupPhase::PHASE_OPEN) return;
    Settings::setCard(g->id(), "present", "card");
    const Card *f = a.face ? g->memberAt(s_faceIdx[a.face - 1]) : nullptr;
    Settings::setCard(g->id(), "face", f ? f->id() : nullptr);
    Settings::setCard(g->id(), "card_tap", tapActionName(s_tapMap[a.tap]));
    finish();
}

void goGroup() {
    ClusterCard *g = heldGroup();
    if (!g || !g->hasId() || s.phase != PopupPhase::PHASE_OPEN) return;
    for (uint8_t k = 0; k < s_nPin; k++)
        if (clusterMemberPinned(*g, s_pinIdx[k]) != a.pin[k]) clusterMemberShow(*g, s_pinIdx[k], a.pin[k]);
    Settings::setCard(g->id(), "present", "cluster");
    finish();
}

// The dialog: what will happen, in the owner's pattern (names in the accent).
char s_line[4][240];
const char *s_lines[4];
uint8_t s_n = 0;
char s_acc[80];

const char *acc(const char *name) {
    snprintf(s_acc, sizeof(s_acc), "#%06lX %s#", (unsigned long)(UI::pal().ACCENT & 0xFFFFFF), name ? name : "");
    return s_acc;
}

void line(const char *fmt, const char *x = nullptr) {
    if (s_n >= 4) return;
    snprintf(s_line[s_n], sizeof(s_line[0]), fmt, x ? x : "");
    s_lines[s_n] = s_line[s_n];
    s_n++;
}

const char *tapWords(TapAction t) {
    switch (t) {
        case TapAction::TAP_TOGGLE:  return "Tapping the card will turn the group on or off.";
        case TapAction::TAP_GROUP:   return "Tapping the card will open the 'Group view'.";
        case TapAction::TAP_MEMBERS: return "Tapping the card will open the 'Members view'.";
        default:                     return "Tapping the card will open the 'History view'.";
    }
}

void ask() {
    ClusterCard *g = heldGroup();
    if (!g) return;
    s_n = 0;
    const char *name = g->area()[0] ? g->area() : g->shownName(CardLabel::LBL_NAME);
    if (a.solo) {
        line("The group %s will be shown on the page as a solo card.", acc(name));
        if (a.face) line("The card %s will be shown as the face.", acc(s_faceW[a.face]));
        else        line(s_faceW[0][4] == 'l' ? "The face will show all of the group's lights."
                                              : "The face will show all of the group's members.");
        line(tapWords(s_tapMap[a.tap]));
        line("Pinned cards are kept and will be shown again if the group is shown as a group.");
        confirmAsk(Kit::DialogRole::DLG_CONFIRM, "Show group as solo card?", s_lines, s_n, "Confirm",
                   Kit::ButtonRole::BTN_CONFIRM, goSolo);
    } else {
        line("The group %s will be shown on the page as a group.", acc(name));
        static char names[200];
        names[0] = '\0';
        for (uint8_t k = 0; k < s_nPin; k++) {
            if (!a.pin[k]) continue;
            const Card *m = g->memberAt(s_pinIdx[k]);
            snprintf(names + strlen(names), sizeof(names) - strlen(names), "%s%s", names[0] ? ", " : "",
                     m ? m->shownName(CardLabel::LBL_NAME) : "?");
        }
        if (names[0]) line("The pinned cards will be: %s", names);
        if (pinnedCount() < 2) line("Fewer than two cards are pinned, so the group will still be shown as a solo card.");
        confirmAsk(Kit::DialogRole::DLG_CONFIRM, "Show group as group?", s_lines, s_n, "Confirm",
                   Kit::ButtonRole::BTN_CONFIRM, goGroup);
    }
}

// --- The steps ---------------------------------------------------------------------------
void showStep(Step st) {
    a.step = st;
    showView(PopupView::VIEW_SHOW_AS);
}

void next() {
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    switch (a.step) {
        case Step::SA_PRESENT: showStep(a.solo ? Step::SA_FACE : Step::SA_PINS); break;
        case Step::SA_FACE:    showStep(Step::SA_TAP);                         break;
        default:               ask();                                          break;
    }
}

void nextCb(lv_event_t *) { lv_async_call([](void *) { next(); }, nullptr); }

void gridCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (a.step == Step::SA_PRESENT) a.solo = i == 0;
    else if (a.step == Step::SA_FACE) a.face = i;
    else if (a.step == Step::SA_TAP) a.tap = i;
    Kit::chipGridLit(s_grid, i);
}

// The prompt stands first in the check table (buildShowAs), so check row k is the
// table's child k + 1 - reading child k read the row above it (F11, owner 11-07).
constexpr uint8_t PIN_ROW0 = 1;

void pinCb(lv_event_t *ev) {
    const uint8_t row = (uint8_t)((uintptr_t)lv_event_get_user_data(ev) >> 8);
    if (row >= s_nPin || !s_table) return;
    a.pin[row] = Kit::chipTableChecked(s_table, (uint8_t)(row + PIN_ROW0));
    if (s_says) lv_label_set_text(s_says, pinsSay());
}

void nextRow(lv_obj_t *list) {
    lv_obj_t *row = Kit::buttonRow(list);
    const int32_t w = Kit::textW("Next", UI::type().NAME) + 2 * mm(UI::shape().S9);
    Kit::button(row, Kit::ButtonRole::BTN_CONFIRM, "Next", w, Kit::controlH(), nextCb);
}

}  // namespace

void showAsOpen() {
    ClusterCard *g = heldGroup();
    if (!g || !g->hasId()) return;
    a = ShowAs{};
    a.solo = !s.cluster;   // as it is presented now
    faceChoices(*g);
    tapChoices(*g);
    pinChoices(*g);
    showStep(Step::SA_PRESENT);
}

bool showAsBack() {
    switch (a.step) {
        case Step::SA_FACE: showStep(Step::SA_PRESENT); return true;
        case Step::SA_TAP:  showStep(Step::SA_FACE);    return true;
        case Step::SA_PINS: showStep(Step::SA_PRESENT); return true;
        default:            return false;   // to Visibility
    }
}

bool showAsDebug(int v) {
    if (s.phase != PopupPhase::PHASE_OPEN) return false;
    if (v == 0) {
        if (!heldGroup() || !heldGroup()->hasId()) return false;
        showAsOpen();
        return true;
    }
    if (s.view != PopupView::VIEW_SHOW_AS) return false;
    if (v == 99) { next(); return true; }
    const uint8_t i = (uint8_t)(v - 1);
    if (a.step == Step::SA_PINS) {
        if (i >= s_nPin || !s_table) return false;
        Kit::chipTableCheck(s_table, (uint8_t)(i + PIN_ROW0), !a.pin[i]);
        a.pin[i] = !a.pin[i];
        if (s_says) lv_label_set_text(s_says, pinsSay());
        return true;
    }
    const uint8_t n = a.step == Step::SA_PRESENT ? 2 : a.step == Step::SA_FACE ? (uint8_t)(s_nFace + 1) : s_nTap;
    if (i >= n) return false;
    if (a.step == Step::SA_PRESENT) a.solo = i == 0;
    else if (a.step == Step::SA_FACE) a.face = i;
    else a.tap = i;
    Kit::chipGridLit(s_grid, i);
    return true;
}

void buildShowAs() {
    ClusterCard *g = heldGroup();
    s_grid = s_table = s_says = nullptr;
    if (!g) return;
    if (a.step == Step::SA_PINS) {
        // A check per member (Kit's check rows), the prompt above them.
        Kit::ChipRowSpec rows[PIN_MAX];
        for (uint8_t k = 0; k < s_nPin; k++) {
            rows[k] = {};
            rows[k].what = g->memberAt(s_pinIdx[k])->shownName(CardLabel::LBL_NAME);
            rows[k].lit  = a.pin[k] ? 1 : 0;
        }
        s_table = Kit::chipTable(s.stage, listW(), rows, s_nPin, pinCb);
        lv_obj_move_to_index(words(s_table, KS::KS_LABEL_NAME, "Select cards to pin:"), 0);
        s_says = words(s_table, KS::KS_LABEL_TAG_DIM, pinsSay());
        nextRow(s_table);
        return;
    }
    lv_obj_t *list = Kit::chipTable(s.stage, listW(), nullptr, 0, nullptr);
    static const char *const PRESENT_W[] = { "Solo card", "Group" };   // K191
    switch (a.step) {
        case Step::SA_PRESENT:
            words(list, KS::KS_LABEL_NAME, "Show this group as:");
            s_grid = Kit::chipGrid(list, listW(), PRESENT_W, 2, a.solo ? 0 : 1, gridCb);
            break;
        case Step::SA_FACE:
            words(list, KS::KS_LABEL_NAME, "Select card to show as the face:");
            s_grid = Kit::chipGrid(list, listW(), s_faceW, (uint8_t)(s_nFace + 1), a.face, gridCb);
            break;
        default:
            words(list, KS::KS_LABEL_NAME, "Select tap action:");
            s_grid = Kit::chipGrid(list, listW(), s_tapW, s_nTap, a.tap, gridCb);
            break;
    }
    nextRow(list);
}

}  // namespace popup
