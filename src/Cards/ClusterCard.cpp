// A group card shown as a cluster (2.11): include/Cards/ClusterCard.h says what
// and why. The sizes are UIShape's C_ tokens, real millimetres (the "2.11
// Clusters" mock's); the looks are the kit's KS_CLUSTER_* and KS_TILE_* roles.
#include "Cards/ClusterCard.h"
#include "Cards/CardBinder.h"
#include "Cards/CardIcons.h"
#include "Cards/CardPopup.h"
#include "Layout/Cluster.h"
#include "Layout/HeaderPlan.h"
#include "UI/Kit.h"
#include "UI/UITokens.h"
#include <stdio.h>

using KS = Kit::KitStyle;

namespace {
int32_t hdrH()  { return UI::mm(UI::shape().C_HDR_H); }
int32_t pad()   { return UI::mm(UI::shape().C_PAD); }
int32_t vgap()  { return UI::mm(UI::shape().C_VALUE_GAP); }
int32_t tgap()  { return UI::mm(UI::shape().C_TILE_GAP); }
// The tiles' box starts a little under the header and ends a little above the
// frame's foot (the mock's: 0.8 and 1.0 of the padding).
int32_t tilesTop() { return hdrH() + pad() * 8 / 10; }
}  // namespace

ClusterCard::ClusterCard() {
    // A tap on the cluster itself (its header) does nothing (owner, 2026-10-19);
    // its own header is drawn here, not a card label.
    setTapAction(TapAction::TAP_NOTHING);
    setHeaderStyle(CardHeaderStyle::HDR_NONE);
    for (int8_t &s : _slotOf) s = -1;
}

ClusterCard::~ClusterCard() {
    // Each member deletes its own widgets (children of this card's, still alive
    // here); Card's destructor then deletes this card's.
    for (uint8_t i = 0; i < _n; i++) {
        if (_binder) _binder->remove(_m[i]);
        delete _m[i];
        _m[i] = nullptr;
    }
}

bool ClusterCard::addMember(Card *m, bool tile, int8_t slot) {
    if (!m) return false;
    if (_n >= GROUP_MEMBERS_MAX) { delete m; return false; }
    _m[_n] = m;
    _tile[_n] = tile;
    _slotOf[_n] = (slot >= 0 && slot < (int8_t)CLUSTER_SLOTS_MAX) ? slot : -1;
    _n++;
    // Its window lists the members (Members): their entities are the cluster's
    // primaries, as many as a card binds.
    bindPrimary(m->firstEntity());
    return true;
}

uint8_t ClusterCard::tileCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < _n; i++) if (_tile[i]) n++;
    return n;
}

uint8_t ClusterCard::headerCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < _n; i++) if (_slotOf[i] >= 0) n++;
    return n;
}

int32_t ClusterCard::chromeW() { return 2 * UI::met().BORDER_W + 2 * pad(); }
int32_t ClusterCard::chromeH() { return 2 * UI::met().BORDER_W + tilesTop() + pad(); }

bool ClusterCard::owns(const char *entityId) const {
    if (!entityId || !entityId[0]) return false;
    for (const Slot &s : _slot) {
        if (s.member < 0) continue;
        const Entity *e = _m[s.member]->firstEntity();
        if (e && !strcmp(e->desc.id, entityId)) return true;
    }
    return false;
}

void ClusterCard::onSnapshot(const Entity &snap) {
    (void)snap;
    if (root()) render();
}

void ClusterCard::onLongPress() { CardPopup::openMembers(*this); }

// The cluster lays out its own room: the header row along the top, the tiles
// under it where Layout::tileGrid() puts them.
void ClusterCard::buildBody(lv_obj_t *body) {
    _room = body;
    Kit::add(body, KS::KS_CARD_PLAIN);   // no padding, no flow: placed by hand
    const int32_t bw = UI::met().BORDER_W;
    _innerW = surfaceWidthPx() - 2 * bw;
    const int32_t innerH = surfaceHeightPx() - 2 * bw;

    _hdr = Kit::plain(body);
    Kit::add(_hdr, KS::KS_CLUSTER_HDR);
    lv_obj_set_size(_hdr, _innerW, hdrH());
    lv_obj_set_pos (_hdr, 0, 0);
    // A bar's lower corners are square (the card band's skirt, without a layer).
    _skirt = Kit::plain(_hdr);
    lv_obj_set_size (_skirt, lv_pct(100), lv_pct(50));
    lv_obj_align    (_skirt, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag (_skirt, LV_OBJ_FLAG_IGNORE_LAYOUT);
    _swatch = Kit::plain(_hdr);
    Kit::add(_swatch, KS::KS_CLUSTER_SWATCH);
    _title = Kit::label(_hdr, KS::KS_CLUSTER_TITLE);
    lv_label_set_long_mode(_title, LV_LABEL_LONG_DOT);
    lv_obj_set_height(_title, lv_font_get_line_height(UI::type().NAME));
    for (uint8_t i = 0; i < _n; i++) {
        const int8_t r = _slotOf[i];
        if (r < 0 || _slot[r].member >= 0) continue;
        Slot &s = _slot[r];
        s.member = (int8_t)i;
        s.box  = Kit::plain(_hdr);
        lv_obj_set_height(s.box, hdrH());
        s.icon = Kit::label(s.box, KS::KS_CARD_ICON_SM);
        s.text = Kit::label(s.box, KS::KS_CLUSTER_VALUE);
    }

    // The tiles.
    const uint8_t nTiles = tileCount();
    const int32_t boxW = _innerW - 2 * pad();
    const int32_t boxH = innerH - tilesTop() - pad();
    const Layout::TileGrid g = Layout::tileGrid(nTiles, boxW, boxH, tgap());
    uint8_t t = 0;
    for (uint8_t i = 0; i < _n; i++) {
        if (!_tile[i]) continue;
        int32_t x = 0, y = 0, w = 0;
        Layout::tileAt(g, nTiles, t++, tgap(), x, y, w);
        Card *m = _m[i];
        m->setCellWidthPx(w);
        m->setCellHeightPx(g.tileH);
        m->build(body);
        lv_obj_set_pos (m->root(), pad() + x, tilesTop() + y);
        lv_obj_set_size(m->root(), w, g.tileH);
        if (_binder) _binder->add(m);
    }
}

void ClusterCard::render() {
    if (!_hdr) return;
    // A floating tag's style pads a card's body from the top (Card::restyle());
    // the cluster's room is its own, from the frame's edge.
    lv_obj_set_style_pad_top(_room, 0, 0);
    const UIPalette &p = UI::pal();
    const bool     bar  = (_style == ClusterStyle::CLSTYLE_BAR);
    const uint32_t gcol = headerColor();   // the group's colour (K46)
    const uint32_t ink  = bar ? p.SURFACE : p.TEXT;

    // THE GROUP'S COLOUR (K102): a bar across the header, or a swatch beside its
    // name. Never colour alone: the name says which group it is.
    if (bar) {
        lv_obj_set_style_bg_color   (_hdr, UI::c(gcol), 0);
        lv_obj_set_style_bg_opa     (_hdr, LV_OPA_COVER, 0);
        lv_obj_set_style_radius     (_hdr, UI::sc(UI::met().RADIUS) - UI::met().BORDER_W, 0);
        lv_obj_set_style_border_width(_hdr, 0, 0);
        lv_obj_set_style_bg_color   (_skirt, UI::c(gcol), 0);
        lv_obj_set_style_bg_opa     (_skirt, LV_OPA_COVER, 0);
        lv_obj_clear_flag(_skirt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag  (_swatch, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_local_style_prop(_hdr, LV_STYLE_BG_COLOR, 0);
        lv_obj_remove_local_style_prop(_hdr, LV_STYLE_BG_OPA, 0);
        lv_obj_remove_local_style_prop(_hdr, LV_STYLE_RADIUS, 0);
        lv_obj_remove_local_style_prop(_hdr, LV_STYLE_BORDER_WIDTH, 0);
        lv_obj_add_flag  (_skirt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(_swatch, UI::c(gcol), 0);
        lv_obj_clear_flag(_swatch, LV_OBJ_FLAG_HIDDEN);
    }

    const char *name = area()[0] ? area() : label();
    lv_label_set_text(_title, name);
    lv_obj_set_style_text_color(_title, UI::c(ink), 0);

    for (Slot &s : _slot) {
        if (s.member < 0) continue;
        const Card   *m = _m[s.member];
        const Entity *e = m->firstEntity();
        if (!e) continue;
        const bool binary = (e->value.type == ValueType::BOOL);
        const bool noData = !e->everSet || !e->available;   // dashes, never an old value (D-15)
        const bool on     = binary && e->value.b;
        // OCCUPANCY HAS ITS OWN ICON FOR EACH STATE (K101): the hero glyph of
        // that state, from the device_class table - never one icon outlined.
        lv_label_set_text(s.icon, binary ? cardHeroIcon(*e, on) : cardCornerIcon(*e, false));
        uint32_t ic = binary ? (on ? p.ST_ACTIVE : p.TEXT_DIM) : cardTintFor(e->desc);
        if (noData) ic = p.TEXT_DIM;
        lv_obj_set_style_text_color(s.icon, UI::c(bar ? p.SURFACE : ic), 0);
        char buf[32] = "";
        if (!binary) {
            if (noData) snprintf(buf, sizeof(buf), "--");
            else        cardFormatValue(*e, buf, sizeof(buf), true, m->tempUnit());
        }
        lv_label_set_text(s.text, buf);
        lv_obj_set_style_text_color(s.text, UI::c(ink), 0);
    }
    layoutHeader();
}

// WHERE THE HEADER'S PIECES STAND: the page header's planner (D-19). The name is
// a resident; each value a claim - its icon an extra that goes first, then the
// value itself, the last slot first (K104: a priority list, no fixed number).
void ClusterCard::layoutHeader() {
    const UIType &t = UI::type();
    const bool bar = (_style == ClusterStyle::CLSTYLE_BAR);
    const int32_t swW = bar ? 0 : lv_obj_get_style_width(_swatch, LV_PART_MAIN) + vgap();

    Layout::HeaderClaim c[1 + CLUSTER_SLOTS_MAX];
    int8_t claimOf[CLUSTER_SLOTS_MAX];
    uint8_t n = 0;
    c[n].place   = Layout::HeaderPlace::PLACE_LEFT;
    c[n].forms   = 1;
    c[n].width[0] = swW + Kit::textW(lv_label_get_text(_title), t.NAME);
    c[n].keep[0]  = Layout::KEEP_ALWAYS;
    n++;
    for (uint8_t r = 0; r < CLUSTER_SLOTS_MAX; r++) {
        claimOf[r] = -1;
        const Slot &s = _slot[r];
        if (s.member < 0) continue;
        const char   *txt = lv_label_get_text(s.text);
        const int32_t iw  = Kit::textW(lv_label_get_text(s.icon), t.ICON_SM);
        const int32_t tw  = txt[0] ? Kit::textW(txt, t.NAME) : 0;
        Layout::HeaderClaim &v = c[n];
        v.place = Layout::HeaderPlace::PLACE_RIGHT;
        const uint8_t shown = (uint8_t)(120 - r * 10);
        if (!txt[0]) {                       // an icon alone (occupancy)
            v.forms = 1; v.width[0] = iw; v.keep[0] = shown;
        } else if (!_icons) {                // "Show sensor icons" off (K104)
            v.forms = 1; v.width[0] = tw; v.keep[0] = shown;
        } else {
            v.forms = 2;
            v.width[0] = iw + vgap() + tw; v.keep[0] = (uint8_t)(60 - r * 10);
            v.width[1] = tw;               v.keep[1] = shown;
        }
        claimOf[r] = (int8_t)n;
        n++;
    }

    Layout::HeaderBar hb;
    hb.width = _innerW;
    hb.edge  = pad();
    hb.space = vgap();
    hb.clear = vgap();
    Layout::HeaderSpot spot[1 + CLUSTER_SLOTS_MAX];
    Layout::planHeader(hb, c, n, spot);

    // The values first: the name gets what they leave.
    int32_t rightmost = _innerW - pad();
    for (uint8_t r = 0; r < CLUSTER_SLOTS_MAX; r++) {
        Slot &s = _slot[r];
        if (s.member < 0 || claimOf[r] < 0) continue;
        const Layout::HeaderSpot &sp = spot[claimOf[r]];
        if (!sp.shown) { lv_obj_add_flag(s.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(s.box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(s.box, sp.w);
        lv_obj_align(s.box, LV_ALIGN_LEFT_MID, sp.x, 0);
        const bool iconShown = !lv_label_get_text(s.text)[0] || (_icons && sp.form == 0);
        if (iconShown) {
            lv_obj_clear_flag(s.icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(s.icon, LV_ALIGN_LEFT_MID, 0, 0);
            const int32_t iw = Kit::textW(lv_label_get_text(s.icon), UI::type().ICON_SM);
            lv_obj_align(s.text, LV_ALIGN_LEFT_MID, iw + vgap(), 0);
        } else {
            lv_obj_add_flag(s.icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(s.text, LV_ALIGN_LEFT_MID, 0, 0);
        }
        if (sp.x < rightmost) rightmost = sp.x;
    }
    const int32_t x0 = pad();
    if (!bar) lv_obj_align(_swatch, LV_ALIGN_LEFT_MID, x0, 0);
    const int32_t tx   = x0 + swW;
    const int32_t room = rightmost - vgap() - tx;
    const int32_t want = Kit::textW(lv_label_get_text(_title), t.NAME);
    lv_obj_set_width(_title, LV_MAX(0, LV_MIN(want, room)));
    lv_obj_align(_title, LV_ALIGN_LEFT_MID, tx, 0);
}
