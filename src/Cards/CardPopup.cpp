#include "Popup/PopupState.h"

// See CardPopup.h for what this is and docs/design/card-sheet.md sections 11-13
// for why it looks the way it does. Step 1 of 2.10a: the window, its header,
// the four ways to close, the body and the inner views. The settings deck
// comes next.
//
// USABLE FIRST (owner, 2026-10-04 evening). The window used to grow out of the
// card and land with the dim; every version of that was measured, and the
// motion was never the slow part - the frame it landed in was (a full-screen
// redraw for the dim, ~120 ms on Midnight, ~200 ms on Linen, after ~240 ms of
// growing). The window now appears complete in the first frame after the long
// press, and closes in one. What was learned is in docs/LESSONS.md, "Effects
// that cover the screen". Nothing is drawn behind it: the dim was tried that
// evening, drawn around the window a frame after it, and the owner chose
// speed - "the delay seems like an eternity and adds very little".
//
// What the eye gets instead is cheap because it is small: the held card is
// pressed in while its border fades to the accent (see "The hold"), now and
// then a spark runs along the window's edge (see "Interference"), and the
// switch's knob slides - and can be dragged.
//
// 2.12 P1: this file is the frame - open and close, the header row and its corner icons, the
// views, the clock - and the public interface. The rest of the window is in src/Cards/Popup/,
// listed in PopupState.h.

namespace popup {

// The window's state, one of each (PopupState.h says what each is).
Popup s;
Hold h;
lv_indev_t *s_indev = nullptr;
lv_obj_t *s_catcher = nullptr;
bool      s_openFromTap = false;

// Changes whenever anything the window shows could have changed. An FNV-1a
// over the fields, so a re-render costs nothing when nothing moved.
uint32_t signature() {
    uint32_t h = 2166136261u;
    auto mixIn = [&h](uint32_t v) { h = (h ^ v) * 16777619u; };
    // The entities shown, and - for a group defined at the source - its
    // members, whose rows the Members view shows and whose pauses change what
    // the window stands for (2.10c).
    const Entity *all[2 * POPUP_ENT_MAX];
    uint8_t nAll = 0;
    for (uint8_t i = 0; i < s.nEnt; i++) all[nAll++] = s.ent[i];
    if (s.native) for (uint8_t i = 0; i < s.native->nMembers && nAll < 2 * POPUP_ENT_MAX; i++)
        all[nAll++] = memberAt(i);
    for (uint8_t i = 0; i < nAll; i++) {
        const Entity *e = all[i];
        if (!e) continue;
        mixIn(e->lastChangeMs);
        mixIn(e->lastUpdateMs);
        mixIn((e->available ? 1u : 0u) | (e->paused ? 2u : 0u) | (e->pending ? 4u : 0u) |
              (e->cmdFailed ? 8u : 0u) | (e->everSet ? 16u : 0u) | (e->attrPending ? 32u : 0u));
        if (e->value.type == ValueType::BOOL) mixIn(e->value.b ? 1u : 0u);
        // A light's levels move without its value changing (2.10b).
        const EntityAttrs &at = e->attrs;
        mixIn((uint32_t)(uint16_t)at.brightness | ((uint32_t)at.lightCaps << 16) | ((uint32_t)at.lightMode << 24));
        mixIn((uint32_t)(uint16_t)at.colorTempK | ((uint32_t)(uint16_t)at.hue << 16));
        mixIn((uint32_t)(uint8_t)at.sat);
    }
    mixIn((uint32_t)s.groupOn);
    return h;
}

void forgetDeck();   // below, with the control deck

// The main view's widgets, before its stage is cleaned or the window goes.
void forgetMainWidgets() {
    forgetDeck();
    s.hero = s.knob = s.heroIcon = nullptr;
    s.lblWhat = s.lblValue = s.lblUnit = s.lblAgo = nullptr;
    s.fill = s.grip = s.mark = s.pill = nullptr;
    for (lv_obj_t *&b : s.btnCtl) b = nullptr;
    for (lv_obj_t *&w : s.swatch) w = nullptr;
    for (lv_obj_t *&b : s.sceneBtn) b = nullptr;
    s.sliding = false;
}

void forgetWidgets() {
    s.btnLeft = s.lblLeft = s.btnHistory = s.btnMembers = s.btnScenes = nullptr;
    s.lblTitleArea = s.lblTitleName = nullptr;
    s.titleLeft = nullptr;
    s.stage = s.autoBar = nullptr;
    forgetMainWidgets();
}

// Share the title row between "Area > " and the name.
//  - Both fit in the middle, between ends of equal width: centred, as always.
//  - They fit if the title takes the room beside the X (a window with two
//    chips on the right has a chip's width spare on the left): the X's slot
//    shrinks to the X, and the title sits left of centre (owner, round 7).
//  - Neither: THE NAME ALONE (owner, round 7, the 4B: "Offi...Desk" was hard to
//    read). The link back goes with it; the back arrow still does that job.
void layoutTitle() {
    if (!s.lblTitleArea || !s.lblTitleName) return;
    const lv_font_t *f = UI::type().NAME;
    const char *area = lv_label_get_text(s.lblTitleArea);
    const int32_t aw = (area && area[0]) ? textW(area, f) + 1 : 0;
    int32_t nw = textW(lv_label_get_text(s.lblTitleName), f) + 1;
    bool crumb = true, wide = false;
    if (aw + nw > s.titleW) {
        if (aw + nw <= s.titleW + s.titleSlack) wide = true;
        else { crumb = false; wide = nw > s.titleW; }
    }
    const int32_t room = s.titleW + (wide ? s.titleSlack : 0);
    if (s.titleLeft) lv_obj_set_width(s.titleLeft, s.titleSlotW - (wide ? s.titleSlack : 0));
    if (nw > room) nw = room;
    if (crumb && aw) {
        lv_obj_clear_flag(s.lblTitleArea, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(s.lblTitleArea, aw);
    } else {
        lv_obj_add_flag(s.lblTitleArea, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_width(s.lblTitleName, nw);
}

// --- THE CORNER ICONS (owner, 2026-10-06) ------------------------------------
// History (the chart), Members and Scenes are views of their own, and the icons
// in the corner work as tabs: shown on every view they belong to, the one
// showing is lit (as a chosen selector button is), and a tap on a lit one goes
// back to the controls. Members on the window's own views, not inside one
// member; Scenes only on a light's controls and its scenes; the chart
// everywhere - the group's from the group's views, a member's from its own.
// The X closes from every view of the window; inside one member the arrow goes
// back to Members.
// Lit as the control deck's selector is (owner, 2026-10-07): the same metal,
// the icon in the accent. Unlit: a soft dent in the surface (round 6).
void cornerLook(lv_obj_t *b, bool shown, bool lit) {
    if (!b) return;
    if (!shown) { lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN); return; }
    lv_obj_clear_flag(b, LV_OBJ_FLAG_HIDDEN);
    const UIPalette  &p   = UI::pal();
    const DeckColours col = deckColours();
    const int32_t sz = UI::minTouch();
    setFace(b, sz, sz, sz / 2, lit ? metalKind() : FaceKind::FACE_DENT, p.SURFACE);
    // Lit, the icon a face up too (owner, round 7), as the selector's is.
    if (lv_obj_t *l = labelIn(b)) {
        lv_obj_set_style_text_color(l, UI::c(lit ? col.chosen : p.TEXT), 0);
        lv_obj_set_style_text_font (l, lit ? UI::type().ICON : UI::type().ICON_MD, 0);
        lv_obj_center(l);
    }
}

void showView(PopupView v) {
    const PopupView was = s.view;
    s.view = v;
    forgetMainWidgets();
    lv_obj_clean(s.stage);
    // HISTORY REACHES UP INTO THE HEADER ROW (owner, round 6): see histTopRow().
    lv_obj_set_style_pad_row(s.win, v == PopupView::VIEW_HISTORY ? -histLift() : mm(1.6f), 0);

    // SCENES SHOWS THE BRIGHTNESS SLIDER (owner): whatever the slider showed
    // before comes back with the controls.
    if (v == PopupView::VIEW_SCENES && was != PopupView::VIEW_SCENES) {
        s.ctlBeforeScenes = s.lightCtl;
        if (lightAggregate().caps & LIGHT_CAN_DIM) s.lightCtl = LCTL_DIM;
    } else if (v != PopupView::VIEW_SCENES && was == PopupView::VIEW_SCENES) {
        s.lightCtl = s.ctlBeforeScenes;
    }

    const bool member   = inMember();
    const bool controls = (v == PopupView::VIEW_MAIN || v == PopupView::VIEW_MEMBER);
    // X on the controls, the back arrow everywhere else (owner, round 4).
    setText(s.lblLeft, v == PopupView::VIEW_MAIN ? LV_SYMBOL_CLOSE : LV_SYMBOL_LEFT);
    cornerLook(s.btnHistory, true, v == PopupView::VIEW_HISTORY);
    cornerLook(s.btnMembers, !member && hasMembers(), v == PopupView::VIEW_MEMBERS);
    cornerLook(s.btnScenes, (controls || v == PopupView::VIEW_SCENES) && hasScenes(),
               v == PopupView::VIEW_SCENES);
    // The left panel belongs to its view: CHART to History, SCENES to Scenes.
    sideFor(v);

    // Whose view it is: the window's, or (inside one) the member's.
    const char *who = (member && s.ent[0]) ? s.ent[0]->desc.name : s.name;
    char title[ENTITY_NAME_MAX + 16];
    if (v == PopupView::VIEW_MAIN) {
        if (s.area[0]) { snprintf(title, sizeof(title), "%s > ", s.area); setText(s.lblTitleArea, title); }
        else           setText(s.lblTitleArea, "");
        setText(s.lblTitleName, s.name);
    } else if (v == PopupView::VIEW_MEMBER) {
        // "Group > member" (card-sheet 11.1).
        snprintf(title, sizeof(title), "%s > ", s.name);
        setText(s.lblTitleArea, title);
        setText(s.lblTitleName, who);
    } else {
        snprintf(title, sizeof(title), "%s > ", who);
        setText(s.lblTitleArea, title);
        setText(s.lblTitleName, v == PopupView::VIEW_HISTORY ? "History"
                              : v == PopupView::VIEW_SCENES  ? "Scenes"
                              : v == PopupView::VIEW_PROPS   ? "Properties" : "Members");
    }
    // SIZED BEFORE THE VIEW IS BUILT: placeWide() lays the window out, and a
    // DOT label laid out at the last view's width rewrites its own text -
    // layoutTitle() then measured "Sce..." and kept it (round 7, the 4B).
    layoutTitle();
    if (v == PopupView::VIEW_HISTORY)      buildHistory();
    else if (v == PopupView::VIEW_MEMBERS) buildMembers();
    else if (v == PopupView::VIEW_PROPS)   buildProps();
    else                                   buildMain();
    deckSync();     // SETTINGS' rows are the view's (a member's own has more)
    deckRender();   // Paused follows whatever the window is showing now
    s.lastSig = signature();
}

// The controls view of whatever the window is showing: the window's, or one
// member's.
PopupView controlsView() { return inMember() ? PopupView::VIEW_MEMBER : PopupView::VIEW_MAIN; }

// A corner icon: its view, or - when lit - back to the controls.
void cornerTap(PopupView v) { showView(s.view == v ? controlsView() : v); }

// BACK IS HARD-LINKED, NOT A HISTORY (owner, round 4): every view has one
// fixed place it goes back to, so "back" never depends on how you got there.
//   History, Members, Scenes  -> the controls
//   a member                  -> Members
//   a member's History/Scenes -> that member
// On the controls it is the X, and closes the window.
void goBack() {
    switch (s.view) {
        case PopupView::VIEW_MAIN:   CardPopup::close();     break;
        case PopupView::VIEW_MEMBER: leaveMember();          break;
        default:                     showView(controlsView()); break;
    }
}

void leftCb(lv_event_t *ev) { (void)ev; goBack(); }

// THE TITLE'S FIRST PART IS A LINK (owner, round 4): "Desk >" goes to Desk's
// controls, "Office lamp >" to that member's - from any view below them. On
// the controls themselves it is the area, which leads nowhere yet.
void crumbCb(lv_event_t *ev) {
    (void)ev;
    if (s.view == PopupView::VIEW_MAIN) return;
    if (s.view == PopupView::VIEW_MEMBER) { leaveMember(PopupView::VIEW_MAIN); return; }
    showView(controlsView());
}
void historyCb(lv_event_t *ev) { (void)ev; cornerTap(PopupView::VIEW_HISTORY); }
void membersCb(lv_event_t *ev) { (void)ev; cornerTap(PopupView::VIEW_MEMBERS); }
void scenesCb (lv_event_t *ev) { (void)ev; cornerTap(PopupView::VIEW_SCENES); }

// D6's drag down, on the HEADER ROW only (card-sheet 13): the light's tall
// slider in 2.10b would fight a drag that could start anywhere. How the
// gesture gets here is in buildContents() - it is not the obvious way.
void headerGestureCb(lv_event_t *ev) {
    (void)ev;
    lv_indev_t *in = lv_indev_active();
    if (!in || lv_indev_get_gesture_dir(in) != LV_DIR_BOTTOM) return;
    lv_indev_wait_release(in);   // before the action - LESSONS
    CardPopup::close();
}

#ifdef DEBUG_POPUP
// The interference trial: a long press on the title cycles how often a spark
// runs along the edge - now and then, once per window, never - from the next
// window. Debug builds only, until the owner chooses (a setting, later).
void intfCycleCb(lv_event_t *ev) {
    (void)ev;
    s_intfMode = (s_intfMode == IntfMode::INTF_NOW_AND_THEN) ? IntfMode::INTF_ONCE
               : (s_intfMode == IntfMode::INTF_ONCE)         ? IntfMode::INTF_OFF
                                                             : IntfMode::INTF_NOW_AND_THEN;
    UIToolkit::show_toast(s_intfMode == IntfMode::INTF_NOW_AND_THEN ? "Sparks: now and then"
                        : s_intfMode == IntfMode::INTF_ONCE         ? "Sparks: once per window"
                                                                    : "Sparks: off");
}
#endif

// ---------------------------------------------------------------------------
// The window's contents
// ---------------------------------------------------------------------------
void buildContents() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const int32_t btn  = UI::minTouch();
    const int32_t gap  = mm(1.5f);

    lv_obj_set_flex_flow(s.win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s.win, s.pad, 0);
    lv_obj_set_style_pad_row(s.win, mm(1.6f), 0);

    // --- Header row --------------------------------------------------------
    lv_obj_t *hdr = plain(s.win);
    lv_obj_set_size     (hdr, lv_pct(100), btn);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hdr, gap, 0);
    // Clickable so a press on the title lands HERE and can become a drag.
    lv_obj_add_flag(hdr, LV_OBJ_FLAG_CLICKABLE);
    // AND IT MUST NOT BUBBLE GESTURES. LVGL does not deliver a gesture to the
    // pressed object and bubble it up; it walks UP from the pressed object
    // PAST every one that has GESTURE_BUBBLE - on by default - and sends it to
    // the first that does not (lv_indev.c, indev_gesture()). With the flag
    // left on, the drag went past this row, the window and the top layer, and
    // reached nobody. The owner's C3, 2026-10-03. The buttons in the row keep
    // the flag, so a drag that starts on the X still arrives here.
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(hdr, headerGestureCb, LV_EVENT_GESTURE, nullptr);
#ifdef DEBUG_POPUP
    lv_obj_add_event_cb(hdr, intfCycleCb, LV_EVENT_LONG_PRESSED, nullptr);
#endif

    // Both ends the same width, so the title sits in the true middle.
    const uint8_t nRight = hasMembers() ? 2 : 1;
    const int32_t slotW  = btn * nRight + gap * (nRight - 1);

    lv_obj_t *left = plain(hdr);
    lv_obj_set_size(left, slotW, btn);
    s.titleLeft  = left;
    s.titleSlotW = slotW;
    s.titleSlack = slotW - btn;
    // The X and the back arrow are LVGL's own symbols, which every built-in
    // Montserrat carries; the hero face is the largest one already linked.
    s.btnLeft = iconButton(left, LV_SYMBOL_CLOSE, UIToolkit::Font_Hero, leftCb, &s.lblLeft);

    lv_obj_t *title = plain(hdr);
    lv_obj_set_height   (title, btn);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_set_flex_flow(title, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s.lblTitleArea = makeLabel(title, t.NAME, p.TEXT_DIM);
    s.lblTitleName = makeLabel(title, t.NAME, p.TEXT);
    // ONE LINE EACH, ellipsised, at widths layoutTitle() works out per view.
    // A DOT label with a content height wraps instead of ellipsising, which is
    // how "Members" ended up on two lines and "All Lamps >" overflowed the
    // centred row and lost its left half (owner's V3).
    const int32_t lh = lv_font_get_line_height(t.NAME);
    lv_label_set_long_mode(s.lblTitleArea, LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(s.lblTitleName, LV_LABEL_LONG_DOT);
    lv_obj_set_height(s.lblTitleArea, lh);
    lv_obj_set_height(s.lblTitleName, lh);
    s.titleW = lv_area_get_width(&s.winRect) - 2 * s.pad - 2 * slotW - 2 * gap;
    // The first part is a link (crumbCb()): the whole header row's height
    // takes the tap, and it brightens while pressed. Gestures still bubble to
    // the row, so a drag down that starts on it still closes the window.
    lv_obj_add_flag(s.lblTitleArea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s.lblTitleArea, (btn - lh) / 2);
    lv_obj_set_style_text_color(s.lblTitleArea, UI::c(p.TEXT), UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_add_event_cb(s.lblTitleArea, crumbCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *right = plain(hdr);
    lv_obj_set_size     (right, slotW, btn);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, gap, 0);
    // The mock's chart icon - axes and bars (mdi:chart-bar, generated
    // 2026-10-05; clock-outline stood in until then).
    // D4: a card standing for several things gets the members icon - INSIDE
    // the chart, which is always in the corner (owner, 2026-10-06: nearly
    // every window has a chart, few have members). Made first: the row is
    // end-aligned, so the last made is the one in the corner.
    if (hasMembers()) s.btnMembers = iconButton(right, MDI_LIGHTBULB_GROUP, t.ICON_MD, membersCb);
    s.btnHistory = iconButton(right, MDI_CHART_BAR, t.ICON_MD, historyCb);

    // --- The stage: whichever view is showing -------------------------------
    s.stage = plain(s.win);
    lv_obj_set_width    (s.stage, lv_pct(100));
    lv_obj_set_flex_grow(s.stage, 1);
    lv_obj_set_flex_flow(s.stage, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.stage, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- Scenes: under the chart (owner, 2026-10-06) ------------------------
    // Out of the flow, in the corner of the window's contents, a header row
    // and its gap down - so it never pushes the stage. Shown by showView()
    // when the light has scenes; mdi:movie-open, a clapperboard, already in
    // the icon faces (HA's own scene icon, the palette, is Colour's here).
    s.btnScenes = iconButton(s.win, MDI_MOVIE_OPEN, t.ICON_MD, scenesCb);
    lv_obj_add_flag(s.btnScenes, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(s.btnScenes, LV_ALIGN_TOP_RIGHT, 0, btn + mm(1.6f));
    lv_obj_add_flag(s.btnScenes, LV_OBJ_FLAG_HIDDEN);

    // --- The auto-close bar: how long until the window gives up -------------
    s.autoBar = plain(s.win);
    lv_obj_add_flag(s.autoBar, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(s.autoBar, lv_area_get_width(&s.winRect) - 2 * s.pad, mm(0.5f) < 3 ? 3 : mm(0.5f));
    lv_obj_set_style_radius(s.autoBar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s.autoBar, UI::c(p.ACCENT), 0);
    lv_obj_set_style_bg_opa  (s.autoBar, LV_OPA_COVER, 0);
    lv_obj_align(s.autoBar, LV_ALIGN_BOTTOM_MID, 0, s.pad / 2);

    showView(PopupView::VIEW_MAIN);
}

// ---------------------------------------------------------------------------
// The clock: auto-close, the bar, and following the entity
// ---------------------------------------------------------------------------
void tickCb(lv_timer_t *t) {
    (void)t;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    const uint32_t now  = millis();
    const uint32_t idle = now - s.lastTouchMs;
    if (idle >= POPUP_AUTOCLOSE_MS) { CardPopup::close(); return; }

    if (s.autoBar) {
        const int32_t full = lv_area_get_width(&s.winRect) - 2 * s.pad;
        const int32_t w = (int32_t)((int64_t)full * (POPUP_AUTOCLOSE_MS - idle) / POPUP_AUTOCLOSE_MS);
        if (w != lv_obj_get_width(s.autoBar)) {
            lv_obj_set_width(s.autoBar, w < 1 ? 1 : w);
            lv_obj_align(s.autoBar, LV_ALIGN_BOTTOM_MID, 0, s.pad / 2);
        }
    }

    // A MEMBER OF A GROUP DEFINED IN HA PAUSED OR RESUMED (2.10c): the window
    // now stands for the members, or for the group again. Checked every tick,
    // not only on a change, because leaving a member's own view settles the
    // signature before this could see it. Not in a member's own view, whose
    // ent[] is that member.
    if (s.native && !inMember() && nativeWantsMembers() != s.expanded) {
        applyNative();
        // Scenes go while a member is paused: from their view, back to the controls.
        showView((s.view == PopupView::VIEW_SCENES && !hasScenes()) ? controlsView() : s.view);
        s.lastAgeMs = now;
        return;
    }

    // History's answer (2.10e), drawn when it arrives.
    histPoll();

    // A scene's ring goes when the light no longer looks as the scene left it.
    if (!sceneRingCheck(now)) renderMain();

    // Scenes learnt after the window opened: its icon appears.
    if (s.btnScenes && lv_obj_has_flag(s.btnScenes, LV_OBJ_FLAG_HIDDEN) && hasScenes() &&
        (s.view == PopupView::VIEW_MAIN || s.view == PopupView::VIEW_MEMBER))
        cornerLook(s.btnScenes, true, false);

    // The window is live (card-sheet 7): it follows the entity while open.
    const uint32_t sig = signature();
    if (sig != s.lastSig) {
        s.lastSig = sig;
        if (s.view == PopupView::VIEW_MEMBERS) showView(PopupView::VIEW_MEMBERS);
        else                                    renderMain();
        s.lastAgeMs = now;
    } else if (now - s.lastAgeMs >= 1000) {
        s.lastAgeMs = now;
        renderMain();   // the "N ago" line; no-op off the main view
    }
}

// The window, ON THE SCREEN AS ITS TOPMOST OBJECT - not lv_layer_top(). LVGL
// draws the top layer OVER the page, never instead of it (lv_refr.c:1049/1081),
// so with the window up there every redraw inside it - opening, a toggle, the
// countdown bar - drew the cards beneath it first. On the screen, an opaque
// window is what lv_refr_get_top_obj() finds covering those areas, and LVGL
// draws only the window. Created last, so it is above the header, the deck and
// the drawer; a page rebuild moves only the cards to the back.
//
// The scheme's raised surface (the mock's window is Midnight's SURFACE_ALT
// exactly) and the border token. No clip_corner: it would cost a window-sized
// layer (card-sheet 8). Clickable, so a tap inside never reaches the catcher.
void makeWindow() {
    const UIPalette &p = UI::pal();
    s.win = plain(lv_screen_active());
    lv_obj_add_flag(s.win, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos (s.win, s.winRect.x1, s.winRect.y1);
    lv_obj_set_size(s.win, lv_area_get_width(&s.winRect), lv_area_get_height(&s.winRect));
    lv_obj_set_style_radius      (s.win, s.winRadius, 0);
    lv_obj_set_style_bg_color    (s.win, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa      (s.win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s.win, UI::border(), 0);
    lv_obj_set_style_border_width(s.win, UI::met().BORDER_W ? UI::met().BORDER_W : 1, 0);
    // The scheme's lift - a real shadow on Linen, nothing on the dark schemes
    // (owner, L2).
    lv_obj_add_style(s.win, UI::paint(UIPaint::PAINT_LIFT), 0);
}

// The deferred half of close(). Runs from LVGL's own timer handler, outside any
// event callback, so deleting the window's children here is safe. Everything
// goes in one frame, the card's border with it.
void closeNow(void *unused) {
    (void)unused;
    s.closeQueued = false;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    confirmClose();   // Remove card's dialog, if it was up
#ifdef DEBUG_POPUP
    const uint32_t t0 = micros();
#endif
    if (s.timer) { lv_timer_delete(s.timer); s.timer = nullptr; }
    intfEnd();
    chartEnd();
    deckEnd();
    if (s.win)   { lv_obj_delete(s.win);     s.win = nullptr; }
    forgetWidgets();
    holdDetach();
    if (s_catcher) lv_obj_clear_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
    s.phase = PopupPhase::PHASE_CLOSED;
    s.nEnt = 0;
    s.groupN = 0;
    s.native = nullptr;
    s.expanded = false;
    s.lastScene = -1;
    s.rebuildQueued = false;
    // K8: settings are written when the window closes - once, if anything
    // changed, on the settings task, 400 ms on so the write never lands in
    // the frames of the close or of SETTINGS' slide.
    Settings::save(400);
#ifdef DEBUG_POPUP
    char mem[96];
    dbgMem(mem, sizeof(mem));
    dbgLater("close: torn down in %lu us; %s", (unsigned long)(micros() - t0), mem);
#endif
}

// What the window's first frame must draw: the window's own rectangle, and -
// only where the scheme gives it a shadow - four thin bands around it. NOT one
// area of the window plus its shadow: LVGL draws an area in strips as wide as
// the area, and a strip that sticks out past the window is not covered by it,
// so the cards under the whole window would be drawn first (LESSONS, "Effects
// that cover the screen").
void invalidateWindow() {
    lv_obj_t *scr = lv_screen_active();
    const lv_area_t &W = s.winRect;
    lv_obj_invalidate_area(scr, &W);
    const int32_t e = lv_obj_get_ext_draw_size(s.win);
    if (e <= 0) return;
    const lv_area_t bands[4] = {
        { W.x1 - e, W.y1 - e, W.x2 + e, W.y1 - 1 },   // above
        { W.x1 - e, W.y2 + 1, W.x2 + e, W.y2 + e },   // below
        { W.x1 - e, W.y1,     W.x1 - 1, W.y2     },   // left
        { W.x2 + 1, W.y1,     W.x2 + e, W.y2     },   // right
    };
    for (const lv_area_t &b : bands) lv_obj_invalidate_area(scr, &b);
}

// The window itself: built, made modal, the clock started, the deck sliding
// up and the sparks waiting their turn.
//
// BUILT QUIET (2026-10-05). The window's frame took ~77 ms where ~25 was
// expected, and DEBUG_POPUP showed why: it redrew 0,0..1032,627, not the
// window. LVGL lays out a new object's SIZE before its POSITION
// (lv_obj_refr_size, then lv_obj_refr_pos), so for one step the full-size
// window sat at (0,0) and that area was invalidated too; joined with the real
// one, it covered the cards under the window and the header, and every strip
// of it was wider than the window. So: everything is made and laid out with
// invalidation off, and then exactly what must be drawn is asked for.
void showWindow() {
#ifdef DEBUG_POPUP
    const uint32_t t0 = micros();
#endif
    s.phase = PopupPhase::PHASE_OPEN;
    s_faceEpoch++;                                    // faces the last window used may be taken back
    lv_obj_t *scr = lv_screen_active();
    lv_obj_update_layout(scr);                        // settle anything pending, aloud
    lv_display_enable_invalidation(nullptr, false);
    s_hv = HistView{};   // History starts on 24 hours in every window (2.10e)
    makeWindow();
    buildContents();
    intfCreate();
    deckCreate();
    lv_obj_update_layout(scr);
    lv_display_enable_invalidation(nullptr, true);
    invalidateWindow();

    if (s_catcher) lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
    s.lastTouchMs = millis();
    s.lastAgeMs   = s.lastTouchMs;
    s.timer = lv_timer_create(tickCb, POPUP_TICK_MS, nullptr);
    intfStart();
    deckSet(DECK_PEEK);
#ifdef DEBUG_POPUP
    char mem[96];
    dbgMem(mem, sizeof(mem));
    dbgLater("open: window and deck built in %lu us; %s", (unsigned long)(micros() - t0), mem);
#endif
}
} // namespace popup

using namespace popup;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void CardPopup::setHistory(HaHistory *history) { s_hist = history; }

void CardPopup::begin() {
    // Every press and release, whatever it lands on - the same reason
    // GUIManager hooks the input device rather than the screen
    // (screenPressCb()). It also spares every card four event registrations.
    if (lv_indev_t *in = lv_indev_get_next(nullptr)) {
        s_indev = in;
        lv_indev_add_event_cb(in, pressCb,   LV_EVENT_PRESSED,  nullptr);
        lv_indev_add_event_cb(in, releaseCb, LV_EVENT_RELEASED, nullptr);
    }
#ifdef DEBUG_POPUP
    // What the window's own frames cost, and what they redrew.
    if (lv_display_t *d = lv_display_get_default()) {
        lv_display_add_event_cb(d, dbgRenderStart, LV_EVENT_RENDER_START, nullptr);
        lv_display_add_event_cb(d, dbgRenderReady, LV_EVENT_RENDER_READY, nullptr);
    }
#endif
    // The tap catcher, once, for the life of the device. See s_catcher.
    s_catcher = plain(lv_layer_top());
    lv_obj_set_size(s_catcher, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_HIT_TEST, nullptr);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_PRESSED,  nullptr);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_CLICKED,  nullptr);
}

bool CardPopup::isOpen() { return s.phase != PopupPhase::PHASE_CLOSED; }

void CardPopup::close() {
    if (s.closeQueued) return;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    s.closeQueued = true;
    lv_async_call(closeNow, nullptr);
}

void CardPopup::open(Card &card) {
    if (s.phase != PopupPhase::PHASE_CLOSED) return;
    if (!card._surface || !card._nPrimary) return;

    // A FINGER THAT MOVED IS A DRAG, NOT A LONG PRESS (card-sheet 7). LVGL
    // fires LONG_PRESSED for a slow drag that never crossed the gesture
    // distance; it must not open a window.
    if (movedTooFar()) { letGo(); return; }
#ifdef DEBUG_POPUP
    s_dbgPressMs = millis();
#endif

    // --- Copy what the window needs; never keep the card -------------------
    s.nEnt = 0;
    for (uint8_t i = 0; i < card._nPrimary && i < CARD_PRIMARY_MAX; i++) {
        if (card._primary[i]) s.ent[s.nEnt++] = card._primary[i];
    }
    if (!s.nEnt) { letGo(); return; }
    // Release the touch BEFORE building anything (LESSONS): the finger is still
    // down, and its release must not land on the window. (It also means LVGL
    // sends no RELEASED for this touch - PRESS_LOST to the card instead.)
    // Not from a tap (openOn()): that finger is already up, and waiting for a
    // release would swallow the next touch - the first tap in the window.
    if (!s_openFromTap)
        if (lv_indev_t *in = lv_indev_active()) lv_indev_wait_release(in);
    s.reg      = Card::s_reg;
    // One entity that HA defines as a group, with its members learnt (2.10c).
    s.native   = (s.nEnt == 1 && s.ent[0]->nMembers) ? s.ent[0] : nullptr;
    applyNative();
    s.tempUnit = card.tempUnit();
    s.groupOn  = card.groupOn();
    s.sceneShow = card.sceneShow();
    s.lightCtl = firstCtl(lightAggregate().caps);   // D3: open on the control
    snprintf(s.area, sizeof(s.area), "%s", card._area);
    snprintf(s.name, sizeof(s.name), "%s", card.label());

    // --- Where the window goes ---------------------------------------------
    // Tall: from just under the system header to just above the deck's tab.
    //
    // WIDE: UP TO TWICE AS WIDE AS TALL, AND NEVER NEAR THE SCREEN'S SIDES
    // (owner, 2026-10-06). Round 1 of 2.10b gave every board the P4_5
    // window's 787:545 shape - itself inherited from the mock, which was drawn
    // for the 4B - and that held the P4_5 to 68 mm on a 110 mm screen, its
    // title ellipsised, with room to spare. Now 2:1, capped so a gap of at
    // least 6 mm or 8% of the screen's width stays on each side: P4_5 ~93 mm,
    // the 7" panels ~129 mm, the 4B ~60 mm. The height used is the one WITH
    // the system header showing, so hiding the header makes the window taller
    // but never wider. The hero stays tied to the height (propH, below).
    lv_obj_t *scr = lv_screen_active();
    const int32_t sw = lv_obj_get_width(scr), sh = lv_obj_get_height(scr);
    const int32_t top    = UIToolkit::systemHeaderPx() + mm(2);
    const int32_t bottom = sh - mm(6) - mm(2);   // mm(6): the deck's tab
    const int32_t refH   = bottom - (UIToolkit::systemHeaderFullPx() + mm(2));
    // AND NEVER WIDER THAN 100 mm (owner, round 5): on a 7" panel the 2:1
    // window was ~129 mm, and every reach to the X or across the window got
    // longer while the space to tap outside it shrank. 100 mm is the P4_5's
    // window and a little - a hand's span.
    const int32_t side   = LV_MAX(mm(6), sw * 8 / 100);
    int32_t w = LV_MIN(2 * refH, mm(POPUP_MAX_W_MM));
    if (w > sw - 2 * side) w = sw - 2 * side;
    s.winRect.x1 = (sw - w) / 2;
    s.winRect.x2 = s.winRect.x1 + w - 1;
    s.winRect.y1 = top;
    s.winRect.y2 = bottom - 1;
    // What the contents are sized from: the height of the biggest window of
    // the P4_5's shape that fits inside this one. The P4_5 itself gives its own
    // height (with the header shown); the 7" panels theirs; the 4B, whose width
    // is capped, less than its height. So the hero is the same share of the
    // window everywhere (owner), and the 4B's shrinks.
    s.propH = LV_MIN(refH, (int32_t)lroundf((float)w / POPUP_ASPECT));
    s.winRadius  = mm(2.4f);
    // 1.2 mm, down from the mock's 2.2: the owner wanted the X "closer to the
    // corner" (H1). Still clear of the 2.4 mm corner radius.
    s.pad        = mm(1.2f);

    s.view        = PopupView::VIEW_MAIN;
    s.closeQueued = false;

    // --- The card stays pressed in, with the accent, while its window is open.
    // The held card stops where its hold got to and finishes it; a card that
    // somehow was not the held one gets the same look.
    if (h.surface == card._surface) {
        lv_anim_delete(h.surface, holdExec);
        lv_anim_delete(h.surface, letGoExec);
    } else {
        holdDetach();
        holdAttach(card._surface);
    }
    holdLook(h.surface, 255);
    h.phase = HoldPhase::HOLD_OWNED;
    showWindow();
}

void CardPopup::cycleSelectorLook() {
    const int8_t cur = chosenLook();
    keepLook(cur < 0 ? 0 : cur >= 3 ? (int8_t)-1 : (int8_t)(cur + 1));
    Settings::save();   // no window closes to ask for it
}

const char *CardPopup::selectorLookShort() {
    static const char *const SHORT[5] = { "Sel Own", "Blk Sq", "Blk Rd", "Slv Sq", "Slv Rd" };
    const int8_t l = chosenLook();
    return SHORT[(l < 0 || l > 3) ? 0 : l + 1];
}

void CardPopup::openOn(Card &card, uint8_t view) {
    s_openFromTap = true;
    open(card);
    s_openFromTap = false;
    if (!isOpen()) return;
    if (view == OPEN_HISTORY)                      showView(PopupView::VIEW_HISTORY);
    else if (view == OPEN_MEMBERS && hasMembers()) showView(PopupView::VIEW_MEMBERS);
}
