// THE GROUP VIEW IS THE CLUSTER (2.13 round 3, DECISIONS K154; refines K143, K150).
//
// A tap on a cluster opens it: the group drawn as its cluster is on the page - its
// own header (swatch or band, name, header sensors) and real tiles that tap and
// long-press as they do there - with every member, the hidden ones last, dimmed and
// marked "Hidden". No title bar and no X: the window is the cluster, and it closes
// by a tap outside or its countdown. It is sized to fit its tiles at the page's
// tile size, bounded only by the page - under the system header, a gap above the
// panels' tabs. A tap on its header opens Members, whose name comes back here.
//
// The cluster is the page's own code (CardPage::makeGroup(), a ClusterCard): made
// for the view, live through the binder, deleted before the stage is cleaned. A
// tile asking to open a window opens its member here instead (groupViewRoute()).
//
// A GROUP SHOWN AS ONE CARD (K155) has the same cluster made again, never built:
// its GROUP panel and Members read their members from it (heldGroup()).
#include "PopupState.h"
#include "Cards/ClusterCard.h"
#include "Cards/CardBinder.h"

namespace popup {

ClusterCard *(*s_groupMaker)(const char *id, bool allTiles) = nullptr;

namespace {

lv_area_t    s_smallRect = {};   // the window's own place, while a view has another
bool         s_moved = false;
ClusterCard *s_view  = nullptr;  // the group view's cluster, while it shows
ClusterCard *s_model = nullptr;  // a group shown as one card: its cluster, read only

ClusterCard *heldCluster() {
    Card *c = cardOf(h.surface);
    return (c && s.cluster) ? static_cast<ClusterCard *>(c) : nullptr;
}

// THE PAGE'S ROOM (K154): under the system header, above the panels' tabs, a
// visible gap from each - the owner: the window "kisses the top of the panels".
lv_area_t pageArea() {
    const int32_t sw = lv_obj_get_width(lv_screen_active()), sh = lv_obj_get_height(lv_screen_active());
    const int32_t gap = mm(UI::shape().S4);
    lv_area_t a;
    a.x1 = gap;
    a.x2 = sw - gap - 1;
    a.y1 = UIToolkit::systemHeaderPx() + gap;
    a.y2 = sh - s.panelHead - gap - 1;
    return a;
}

// THE GROUP VIEW'S SIZE (K154): its tiles at the page's tile size, as few rows as
// the page's width allows, the rows evened out - and NEVER SMALLER THAN THE
// WINDOW'S OWN SIZE (`normal`; the owner, round 3: the page's size looked odd),
// the tiles then laid out for the room. Where they do not fit at that size, the
// page's room, the tiles smaller.
lv_area_t fitted(const lv_area_t &room, const lv_area_t &normal) {
    const ClusterCard *pg = heldCluster();
    const int32_t maxW = lv_area_get_width(&room), maxH = lv_area_get_height(&room);
    const int32_t g  = mm(UI::shape().C_TILE_GAP);
    const int32_t cw = ClusterCard::chromeW(), ch = ClusterCard::chromeH();
    int32_t tw = 0, th = 0, minW = 0;
    if (pg && pg->root()) {
        minW = lv_obj_get_width(pg->root());
        for (uint8_t i = 0; i < pg->memberCount() && !tw; i++) {
            const Card *m = pg->memberAt(i);
            if (pg->tileAt(i) && m && m->root()) { tw = lv_obj_get_width(m->root()); th = lv_obj_get_height(m->root()); }
        }
        if (!tw) th = tw = LV_MAX(mm(UI::shape().C_TILE_MIN), lv_obj_get_height(pg->root()) - ch);
    }
    if (!tw) tw = th = mm(UI::shape().C_TILE_MIN) * 2;
    const uint8_t n = s_view ? s_view->tileCount() : 0;
    int32_t W = maxW, H = maxH;
    if (n) {
        int32_t c = LV_MIN((int32_t)n, LV_MAX(1, (maxW - cw + g) / (tw + g)));
        const int32_t r = ((int32_t)n + c - 1) / c;
        c = ((int32_t)n + r - 1) / r;   // the rows evened out: 3 + 2, never 4 + 1
        const int32_t w = c * tw + (c - 1) * g + cw, hh = r * th + (r - 1) * g + ch;
        if (hh <= maxH && w <= maxW) {
            W = w; H = hh;
            s_view->setTileCols((uint8_t)c);
        }
    } else if (pg && pg->root()) {
        H = LV_MIN(maxH, lv_obj_get_height(pg->root()));
        W = minW;
    }
    W = LV_MAX(W, minW);
    const int32_t nW = lv_area_get_width(&normal), nH = lv_area_get_height(&normal);
    if (s_view && (W < nW || H < nH)) s_view->setTileCols(0);   // the room's own rule (Layout::tileGrid)
    W = LV_MIN(maxW, LV_MAX(W, nW));
    H = LV_MIN(maxH, LV_MAX(H, nH));
    lv_area_t a;
    a.x1 = room.x1 + (maxW - W) / 2;
    a.x2 = a.x1 + W - 1;
    a.y1 = room.y1 + (maxH - H) / 2;
    a.y2 = a.y1 + H - 1;
    return a;
}

// The window to `r`, or back to its own place (null). `bare`: no header row,
// no padding, its corners a card's (the group view, K154).
void place(const lv_area_t *r, bool bare) {
    if (r) {
        if (!s_moved) s_smallRect = s.winRect;
        s_moved = true;
        s.winRect = *r;
    } else if (s_moved) {
        s.winRect = s_smallRect;
        s_moved = false;
    }
    lv_obj_set_pos(s.win, s.winRect.x1, s.winRect.y1);
    lv_obj_set_size(s.win, lv_area_get_width(&s.winRect), lv_area_get_height(&s.winRect));
    Kit::sheetBare(s, bare, UI::sc(UI::met().RADIUS));
    Kit::sheetLayoutTitle(s);
}

}  // namespace

// The window for view `v`: the group view fitted to its cluster and bare; Edit
// cards the page's room (K148); every other view the window's own place.
void groupWindowFor(PopupView v) {
    if (!s.win) return;
    if (v == PopupView::VIEW_GROUP) {
        ClusterCard *pg = heldCluster();
        if (pg && pg->hasId() && s_groupMaker) s_view = s_groupMaker(pg->id(), true);
        const lv_area_t room = pageArea();
        const lv_area_t normal = s_moved ? s_smallRect : s.winRect;   // the window's own place
        const lv_area_t r = fitted(room, normal);
        place(&r, s_view != nullptr);
    } else if (v == PopupView::VIEW_CLUSTER_ADD) {
        const lv_area_t room = pageArea();
        place(&room, false);
    } else {
        place(nullptr, false);
    }
}

void groupViewForget() {
    if (!s_view) return;
    if (CardBinder *b = s_view->binder()) b->remove(s_view);
    delete s_view;   // its members' widgets and its own; the stage is still alive
    s_view = nullptr;
}

void groupForget() {
    s_moved = false;
}

void buildGroup() {
    if (!s_view) return;
    lv_obj_update_layout(s.win);
    const int32_t W = lv_obj_get_content_width(s.stage), H = lv_obj_get_content_height(s.stage);
    s_view->setCellWidthPx(W);
    s_view->setCellHeightPx(H);
    // ITS HEADER OPENS MEMBERS (K154): a tap or a long press - the window's own
    // name, which the title bar would have had.
    s_view->setTapAction(TapAction::TAP_MEMBERS);
    s_view->setLongOpen(CardPopup::OPEN_MEMBERS);
    s_view->build(s.stage);
    lv_obj_set_size(s_view->root(), W, H);
    if (CardBinder *b = s_view->binder()) b->add(s_view);   // its header follows its sensors
}

// A TILE IN THE GROUP VIEW ASKED FOR A WINDOW (K154): its member here, as from its
// row in Members - its card on the page's cluster (the view's goes when the view
// does), on the view it asked for. The view's own header: Members.
bool groupViewHas(const Card &card) {
    if (!s_view) return false;
    if (&card == s_view) return true;
    for (uint8_t i = 0; i < s_view->memberCount(); i++)
        if (s_view->memberAt(i) == &card) return true;
    return false;
}

bool groupViewRoute(Card &card, uint8_t view) {
    if (!s_view || s.view != PopupView::VIEW_GROUP || s.phase != PopupPhase::PHASE_OPEN) return false;
    if (&card == s_view) {
        lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_MEMBERS); }, nullptr);
        return true;
    }
    if (!groupViewHas(card)) return false;
    ClusterCard *g = heldGroup();
    Card *twin = nullptr;
    for (uint8_t i = 0; g && i < g->memberCount() && !twin; i++) {
        Card *m = g->memberAt(i);
        if (m && m->hasId() && card.hasId() && !strcmp(m->id(), card.id())) twin = m;
    }
    const PopupView v = view == CardPopup::OPEN_HISTORY ? PopupView::VIEW_HISTORY
                      : view == CardPopup::OPEN_MEMBERS ? PopupView::VIEW_MEMBERS : PopupView::VIEW_MEMBER;
    enterMember(twin, twin ? nullptr : card.firstEntity(), v);
    return true;
}

// --- A group shown as one card: its cluster, read only (K155) --------------------------
ClusterCard *heldGroup() {
    if (ClusterCard *cl = heldCluster()) return cl;
    return s.groupCard ? s_model : nullptr;
}

void groupModelOpen(Card &card) {
    groupModelForget();
    if (s.cluster || !card.groupCard() || !card.hasId() || !s_groupMaker) return;
    s_model = s_groupMaker(card.id(), false);
}

void groupModelForget() {
    delete s_model;   // never built, never bound: its member cards only
    s_model = nullptr;
}

}  // namespace popup
