// The Scenes view (2.12 P1, split from CardPopup.cpp): which scenes are offered, the ring round
// one loaded from the window, and the stack of buttons.
#include "PopupState.h"

namespace popup {

// THE LIGHT WHOSE SCENES ARE OFFERED (2.10c): the one light the window shows -
// a card's own, a group defined in HA as itself, or a member in its own view
// (a bulb has none, a room does). None while the window works through a
// group's members (a scene would reach the paused one), or for a group
// defined here.
const Entity *sceneHost() {
    if (!s.reg) return nullptr;
    if (inMember()) return s.ent[0];
    if (s.native) return s.expanded ? nullptr : s.native;
    return (s.nEnt == 1) ? s.ent[0] : nullptr;
}

// THE SCENES OFFERED, by the card's Scenes setting (owner, 2026-10-07): those
// not hidden in HA's own UI, all of them, or none. `i` counts offered scenes.
uint8_t shownScenes() {
    const Entity *h = sceneHost();
    if (!h || s.sceneShow == SceneShow::SCENES_OFF ||
        h->desc.kind != EntityKind::LIGHT || !h->desc.writable) return 0;
    uint8_t n = 0;
    for (uint8_t i = 0; i < h->nScenes; i++) {
        const Entity *sc = s.reg->sceneOf(*h, i);
        if (sc && (s.sceneShow == SceneShow::SCENES_ALL || !sc->sourceHidden)) n++;
    }
    return n;
}

const Entity *shownScene(uint8_t i) {
    const Entity *h = sceneHost();
    if (!h) return nullptr;
    for (uint8_t k = 0; k < h->nScenes; k++) {
        const Entity *sc = s.reg->sceneOf(*h, k);
        if (!sc || (s.sceneShow != SceneShow::SCENES_ALL && sc->sourceHidden)) continue;
        if (i-- == 0) return sc;
    }
    return nullptr;
}

bool hasScenes() { return shownScenes() > 0; }

// THE RING ROUND A LOADED SCENE (owner, 2.10c S5) says "the light is as that
// scene left it", so it lasts only while that is true: until the window
// closes, any command from the window, or a change from elsewhere. A scene
// fades in over a second or two and the bridge reports as it goes, so a
// change is only counted once the levels have held still for SCENE_SETTLE_MS.
constexpr uint32_t SCENE_SETTLE_MS = 3000;

void sceneLoaded(uint8_t i) {
    s.lastScene    = (int8_t)i;
    s.sceneMs      = millis();
    s.sceneSettled = 0;
}

uint32_t sceneLevels() {
    const Entity *h = sceneHost();
    if (!h) return 1;
    const EntityAttrs &a = h->attrs;
    uint32_t v = 2166136261u;
    auto mixIn = [&v](uint32_t x) { v = (v ^ x) * 16777619u; };
    mixIn(h->value.type == ValueType::BOOL && h->value.b);
    mixIn((uint32_t)(uint16_t)a.brightness); mixIn((uint32_t)(uint16_t)a.colorTempK);
    mixIn((uint32_t)(uint16_t)a.hue);        mixIn((uint32_t)(uint8_t)a.sat);
    return v | 1;   // never 0, which means "not taken yet"
}

// From the tick: false when the ring has just gone.
bool sceneRingCheck(uint32_t now) {
    if (s.lastScene < 0 || now - s.sceneMs < SCENE_SETTLE_MS) return true;
    const uint32_t lv = sceneLevels();
    if (!s.sceneSettled) { s.sceneSettled = lv; return true; }
    if (lv == s.sceneSettled) return true;
    s.lastScene = -1;
    return false;
}

// A scene button: load it (scene.turn_on), and ring it - see above.
void sceneCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (pausedBlocks()) return;
    const Entity *sc = shownScene(i);
    if (!sc || !s.reg->press(sc->desc.id, millis())) return;
    sceneLoaded(i);
    renderLight();
}

// THE SCENES VIEW (2.10c; owner, 2026-10-06): the brightness slider stays where
// it is on the controls view, and the scene buttons take the column - the
// label, the value and the selector all give way, so three rows fit. Reached
// by the scenes icon under the chart, like the other views; the column is the
// controls view's own width, so the slider does not move.
//
// SIZED AS A SHARE OF THE WINDOW, like the slider (owner: on the 4B they were
// large for their window): 9 "P4_5 millimetres" tall - a full touch target on
// the P4_5 - never under the swatches' 6.5 mm, and three rows at most in the
// slider's height. Centred; it scrolls if a light has more than fit. Names as
// the source gives them, sorted ("Bright" .. "Relax").
// A STACK, NOT A FLOW (owner, after round 5: wrapped like text, the buttons
// looked scattered). One column of buttons all as wide as the longest name,
// centred in the slider's height; a second column only when one is full, and
// so on while the room lasts - then it scrolls. Measured before building, so
// the slider can sit beside exactly what is there.

SceneStack sceneStack(int32_t maxW) {
    const UIType &t = UI::type();
    const UIShape &u = UI::shape();
    SceneStack k;
    k.gap = pm(u.P_SCENE_GAP);
    // The stack's own padding holds the ring (outline pad + width), which its
    // edge would otherwise clip.
    k.pad = LV_MAX(2, mm(u.RING_PAD)) + LV_MAX(2, mm(u.RING_W)) + 1;
    k.ch  = LV_MAX(mm(u.SWATCH), LV_MIN(pm(u.P_SCENE_H), (s.heroH - 2 * k.gap - 2 * k.pad) / 3));
    k.bw  = k.ch * 16 / 10;
    const uint8_t n = shownScenes();
    for (uint8_t i = 0; i < n; i++)
        if (const Entity *sc = shownScene(i))
            k.bw = LV_MAX(k.bw, textW(sc->desc.name, t.TAG) + 2 * pm(u.P_SCENE_PAD));
    k.rowsFit = LV_MAX(1, (s.heroH - 2 * k.pad + k.gap) / (k.ch + k.gap));
    k.cols    = LV_MAX(1, (n + k.rowsFit - 1) / k.rowsFit);
    const int32_t maxCols = LV_MAX(1, (maxW - 2 * k.pad + k.gap) / (k.bw + k.gap));
    k.scroll = k.cols > maxCols;
    if (k.scroll) k.cols = maxCols;
    k.w = k.cols * k.bw + (k.cols - 1) * k.gap + 2 * k.pad;
    // Only as tall as its rows (the column centres it in the slider's
    // height), so placeWide() can see where the buttons really are.
    const int32_t rows = LV_MAX(1, LV_MIN((int32_t)n, k.rowsFit));
    k.h = k.scroll ? s.heroH : rows * k.ch + (rows - 1) * k.gap + 2 * k.pad;
    return k;
}

int32_t sceneGridW(int32_t maxW) { return sceneStack(maxW).w; }

void buildSceneGrid(lv_obj_t *col, int32_t colW) {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const Entity    *h = sceneHost();
    if (!h || !s.reg) return;
    const SceneStack k = sceneStack(colW + 1);
    const int32_t ch = k.ch, gap = k.gap, pad = k.pad;

    lv_obj_t *grid = plain(col);
    lv_obj_set_size(grid, k.w, k.h);
    if (k.scroll) {
        // More than the room holds: rows, scrolling, the names in order.
        lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
        UI::tameScroll(grid);
    } else {
        // Down each column, then the next; each column centred top to bottom.
        lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_COLUMN_WRAP);
        lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    }
    lv_obj_set_style_pad_column(grid, gap, 0);
    lv_obj_set_style_pad_row   (grid, gap, 0);
    lv_obj_set_style_pad_all   (grid, pad, 0);   // room for the ring

    for (uint8_t i = 0; i < shownScenes() && i < ENTITY_SCENES_MAX; i++) {
        const Entity *sc = shownScene(i);
        if (!sc) continue;
        lv_obj_t *b = plain(grid);
        lv_obj_set_size(b, k.bw, ch);
        Kit::add(b, KS::KS_PILL);
        Kit::add(b, KS::KS_PILL_PRESSED, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
        // Greyed in a paused window, as everything else is: its own colours.
        if (s.builtPaused) lv_obj_set_style_bg_color(b, UI::c(quiet(p.SURFACE)), 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, sceneCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *l = s.builtPaused ? makeLabel(b, t.TAG, quiet(p.TEXT)) : Kit::label(b, KS::KS_LABEL_TAG);
        lv_label_set_text(l, sc->desc.name);
        lv_obj_center(l);
        s.sceneBtn[i] = b;
    }
}

} // namespace popup
