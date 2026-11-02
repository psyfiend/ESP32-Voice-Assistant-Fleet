// THE GROUP VIEW (2.13, DECISIONS K143; artifact "2.13 Add Cards Mocks", screen 5).
//
// A tap on a cluster opens it; a long press opens Members - one window, two
// views, the corner tab going between them. Every member as a tile, the ones
// hidden from the cluster last, each saying Hidden: a cluster on the page keeps
// the few tiles that earn their space, the group view has them all. It fills
// the page area under the header - a bigger screen shows bigger tiles - and has
// no panels: CLUSTER and SETTINGS are Members'.
//
// A tile that switches (a light, a switch) switches on a tap; any other opens
// that member's own view, as its row in Members does; a long press on any tile
// opens its own view.
#include "PopupState.h"
#include "Cards/ClusterCard.h"

namespace popup {

namespace {

lv_area_t s_smallRect = {};   // the window's own place, while the group view has the page's
bool      s_big = false;

// The member's index among the window's entities (memberAt()), or -1.
int windowIndexOf(const Entity *e) {
    for (uint8_t k = 0; e && k < memberCount(); k++)
        if (memberAt(k) && !strcmp(memberAt(k)->desc.id, e->desc.id)) return k;
    return -1;
}

void openMember(const Entity *e) {
    const int k = windowIndexOf(e);
    if (k < 0) return;
    enterMemberAt((uint8_t)k);
}

void tileCb(lv_event_t *ev) {
    Card *m = (Card *)lv_event_get_user_data(ev);
    if (!m) return;
    if (lv_event_get_code(ev) == LV_EVENT_LONG_PRESSED || !m->tapToggles()) { openMember(m->firstEntity()); return; }
    m->tap();
}

ClusterCard *heldCluster() {
    Card *c = cardOf(h.surface);
    return (c && s.cluster) ? static_cast<ClusterCard *>(c) : nullptr;
}

// One tile: its icon (or a sensor's value), its name, its state.
void tile(lv_obj_t *grid, Card *m, const char *name, bool hidden, int32_t w, int32_t hgt) {
    const Entity *e = m->firstEntity();
    lv_obj_t *t = Kit::item(grid);
    lv_obj_set_size(t, w, hgt);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(t, mm(UI::shape().S4), 0);
    const bool binary = e->value.type == ValueType::BOOL;
    const bool on = binary && e->value.b && e->everSet && e->available;
    if (binary || !e->everSet) {
        lv_obj_t *ic = Kit::label(t, KS::KS_CARD_ICON_SM);
        lv_label_set_text(ic, cardHeroIcon(*e, on));
        lv_obj_set_style_text_font(ic, UI::type().ICON, 0);   // a state card's disc glyph, the tile's size
        lv_obj_set_style_text_color(ic, UI::c(on ? UI::pal().ST_ACTIVE : UI::pal().TEXT_DIM), 0);
    } else {
        // A measure card's number and its unit beside it.
        lv_obj_t *vr = plain(t);
        lv_obj_set_size(vr, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(vr, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(vr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_column(vr, mm(UI::shape().S3), 0);
        char v[32];
        cardFormatValue(*e, v, sizeof(v), false, s.tempUnit);
        lv_obj_t *num = Kit::label(vr, KS::KS_CLUSTER_VALUE);
        lv_obj_set_style_text_font(num, UI::type().VALUE, 0);
        lv_label_set_text(num, v);
        lv_obj_t *u = Kit::label(vr, KS::KS_CARD_UNIT);
        lv_label_set_text(u, e->desc.unit);
    }
    lv_obj_t *n = Kit::label(t, hidden ? KS::KS_LABEL_NAME_DIM : KS::KS_LABEL_NAME);
    lv_obj_set_width(n, w - 2 * mm(UI::shape().S4));
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    lv_obj_set_height(n, lv_font_get_line_height(lv_obj_get_style_text_font(n, LV_PART_MAIN)));
    lv_label_set_text(n, name);
    char st[48];
    const KS role = memberState(*e, MemberShown::SHOWN_TILE, st, sizeof(st));
    if (binary) lv_label_set_text(Kit::label(t, role), st);
    // HIDDEN, SAID IN WORDS AND BY THE EYE (K143: "clearly noted"), never by colour alone.
    if (hidden) {
        lv_obj_t *tag = Kit::label(t, KS::KS_LABEL_TAG_DIM);
        lv_obj_add_flag(tag, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(tag, LV_ALIGN_TOP_LEFT, mm(UI::shape().S5), mm(UI::shape().S4));
        lv_label_set_text(tag, "Hidden");   // K143's word; the tile dimmed too
    }
    lv_obj_add_event_cb(t, tileCb, LV_EVENT_SHORT_CLICKED, m);
    lv_obj_add_event_cb(t, tileCb, LV_EVENT_LONG_PRESSED, m);
    lv_obj_update_layout(t);
    Kit::finish(t, w, hgt, lv_obj_get_style_radius(t, LV_PART_MAIN),
                on ? Kit::Finish::FINISH_LIT : Kit::Finish::FINISH_SATIN);
    if (hidden) lv_obj_set_style_opa(t, LV_OPA_70, 0);
}

}  // namespace

// The window to the page's size, its panels away - or back as it was.
void groupWindow(bool big) {
    if (big == s_big || !s.win) return;
    if (big) {
        s_smallRect = s.winRect;
        const int32_t sw = lv_obj_get_width(lv_screen_active()), sh = lv_obj_get_height(lv_screen_active());
        const int32_t gap = mm(UI::shape().S4);
        s.winRect.x1 = gap;
        s.winRect.x2 = sw - gap - 1;
        s.winRect.y1 = UIToolkit::systemHeaderPx() + gap;
        s.winRect.y2 = sh - gap - 1;
    } else {
        s.winRect = s_smallRect;
    }
    s_big = big;
    lv_obj_set_pos(s.win, s.winRect.x1, s.winRect.y1);
    lv_obj_set_size(s.win, lv_area_get_width(&s.winRect), lv_area_get_height(&s.winRect));
    for (lv_obj_t *p : { s.right.root, s.left.root })
        if (p) { if (big) lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(p, LV_OBJ_FLAG_HIDDEN); }
    Kit::sheetLayoutTitle(s);
}

void groupForget() { s_big = false; }   // the window went

void buildGroup() {
    ClusterCard *cl = heldCluster();
    if (!cl) return;
    clusterMemberNames(*cl);
    // The tiles shown on the page, then the hidden ones; moved ones are elsewhere.
    Card   *order[GROUP_MEMBERS_MAX];
    uint8_t name[GROUP_MEMBERS_MAX];
    bool    hid[GROUP_MEMBERS_MAX];
    uint8_t n = 0;
    for (int pass = 0; pass < 2; pass++)
        for (uint8_t i = 0; i < cl->memberCount() && n < GROUP_MEMBERS_MAX; i++) {
            Card *m = cl->memberAt(i);
            if (!m || !m->firstEntity()) continue;
            const MemberShown sh = clusterMemberShown(*cl, i);
            if (sh == MemberShown::SHOWN_MOVED || sh == MemberShown::SHOWN_HEADER) continue;
            const bool hidden = sh == MemberShown::SHOWN_HIDDEN;
            if (hidden != (pass == 1)) continue;
            order[n] = m; name[n] = i; hid[n] = hidden; n++;
        }

    lv_obj_update_layout(s.win);
    const int32_t W = lv_obj_get_content_width(s.stage), H = lv_obj_get_content_height(s.stage);
    const int32_t gap = mm(UI::shape().S5);
    // As near square as the page allows: the columns that make the tiles' shape
    // closest to square, the rows following.
    uint8_t cols = 1;
    float best = 1e9f;
    for (uint8_t c = 1; c <= (n ? n : 1); c++) {
        const uint8_t r = (uint8_t)((n + c - 1) / c);
        const float tw = (float)(W - (c - 1) * gap) / c, th = (float)(H - (r - 1) * gap) / r;
        const float off = tw > th ? tw / th : th / tw;
        if (off < best) { best = off; cols = c; }
    }
    const uint8_t rows = (uint8_t)((n + cols - 1) / LV_MAX(cols, 1));
    const int32_t tw = (W - (cols - 1) * gap) / cols;
    const int32_t th = rows ? LV_MIN((H - (rows - 1) * gap) / rows, tw * 11 / 10) : 0;

    lv_obj_t *grid = plain(s.stage);
    lv_obj_set_size(grid, W, H);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(grid, gap, 0);
    lv_obj_set_style_pad_column(grid, gap, 0);
    // A TILE'S NAME IS THE TILE'S (the dashboard's short label, as on the page:
    // "Desk"); Members' rows have HA's in full.
    (void)name;
    for (uint8_t k = 0; k < n; k++) tile(grid, order[k], order[k]->shownName(CardLabel::LBL_NAME), hid[k], tw, th);
}

}  // namespace popup
