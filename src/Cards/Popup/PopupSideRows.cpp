// The left panel's rows: CHART, ACTIVITY and SCENES (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

// The scene row: the offered scenes by name, in the window's order - and, kept
// with it, their HA ids, so a pick means the scene that was on that line.
char    s_tapSceneOpts[ENTITY_SCENES_MAX * (ENTITY_NAME_MAX + 1)];
char    s_tapSceneRefs[ENTITY_SCENES_MAX][ENTITY_TOPIC_MAX];
uint8_t s_tapSceneN = 0;
uint16_t tapSceneRowBuild(const Card &c, uint8_t &n) {
    const Entity *sc[ENTITY_SCENES_MAX];
    n = c.offeredScenes(sc, ENTITY_SCENES_MAX);
    size_t used = 0;
    uint16_t sel = 0;
    s_tapSceneOpts[0] = '\0';
    for (uint8_t i = 0; i < n; i++) {
        used += snprintf(s_tapSceneOpts + used, sizeof(s_tapSceneOpts) - used, "%s%s", i ? "\n" : "",
                         sc[i]->desc.name);
        snprintf(s_tapSceneRefs[i], sizeof(s_tapSceneRefs[i]), "%s", sc[i]->desc.externalRef);
        if (!strcmp(sc[i]->desc.externalRef, c.tapScene())) sel = i;
    }
    s_tapSceneN = n;
    return sel;
}

// SCENES' Scene action (owner, 2026-10-13): Load default / Cycle scenes. Kept
// for the card; and when its Tap action is "Scenes...", the tap does it now.
void sceneActionDropCb(lv_event_t *ev) {
    const uint32_t k = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    Card *c = cardOf(h.surface);
    if (!c) return;
    const TapAction want = k ? TapAction::TAP_CYCLE_SCENES : TapAction::TAP_LOAD_SCENE;
    keepCardSetting("scene_action", k ? "cycle" : "load");
    const TapAction t = c->tapAction();
    if (t == TapAction::TAP_CYCLE_SCENES || t == TapAction::TAP_LOAD_SCENE) {
        c->setTapAction(want);
        keepCardSetting("tap", tapActionName(want));
    }
    if (want == TapAction::TAP_LOAD_SCENE) ensureDefaultScene(*c);
}

void tapSceneDropCb(lv_event_t *ev) {
    const uint32_t k = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    Card *c = cardOf(h.surface);
    if (!c || k >= s_tapSceneN) return;
    c->setTapScene(s_tapSceneRefs[k]);   // the scene on that line when the row was made
    keepCardSetting("tap_scene", s_tapSceneRefs[k]);
}

// --- CHART and ACTIVITY: History's own options (2.10e) ----------------------
// CHART, for a number: the span (as the chips on the view - the two follow
// each other), the Legend, the min and max, the shading, the span buttons.
// ACTIVITY, for anything that turns on and off: its Span (named so since round
// 2, as the chart's), whether each change says how long it lasted and what
// caused it, the span buttons. Every
// choice is kept under the card's id (owner, round 2 - H3).
lv_obj_t *s_ddSpan = nullptr;   // the panel's Span, kept in step with the chips

bool checked(lv_event_t *ev) { return lv_obj_has_state((lv_obj_t *)lv_event_get_target(ev), LV_STATE_CHECKED); }
const char *onOff(bool on) { return on ? "on" : "off"; }

void spanDropCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    if (i == s_hv.span || i >= HIST_SPANS) return;
    s_hv.span = i;
    histKeep("hist_span", HIST_SPAN_NAME[i]);
    histAsk();
    histDraw();
}

void minMaxCb(lv_event_t *ev) {
    s_hv.minMax = checked(ev);
    s_hv.chipRoom = 0;   // the left corner comes or goes: the selector's room afresh
    histKeep("hist_minmax", onOff(s_hv.minMax));
    histDraw();
}

void legendDropCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    if (i > LEGEND_BOTH) return;
    s_hv.legend = i;
    histKeep("hist_legend", HIST_LEGEND_NAME[i]);
    histDraw();
}

// Shading: under line (owner, round 5) - the chart's gradient fill.
void shadeCb(lv_event_t *ev) {
    s_hv.shade = checked(ev);
    histKeep("hist_shade", onOff(s_hv.shade));
    histDraw();
}

// ACTIVITY's Show duration (owner, round 5 - it replaced Details).
void durationCb(lv_event_t *ev) {
    s_hv.duration = checked(ev);
    histKeep("act_duration", onOff(s_hv.duration));
    histDraw();
}

// Show span buttons: off gives the chart (or the list) their room.
void chipsCb(lv_event_t *ev) {
    bool &on = s_hv.chart ? s_hv.chips : s_hv.actChips;
    on = checked(ev);
    histKeep(s_hv.chart ? "hist_chips" : "act_chips", onOff(on));
    histDraw();
}

void periodDropCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    if (i == s_hv.period || i >= HIST_PERIODS) return;
    s_hv.period = i;
    s_hv.more = false;
    histKeep("act_span", HIST_PERIOD_NAME[i]);
    histAsk();
    histDraw();
}

void causesCb(lv_event_t *ev) {
    s_hv.causes = checked(ev);
    histKeep("act_causes", onOff(s_hv.causes));
    histDraw();
}

// A chip was tapped: the panel's Span says the same.
void histSyncPanel() {
    const uint8_t want = s_hv.chart ? s_hv.span : s_hv.period;
    if (s_ddSpan && lv_dropdown_get_selected(s_ddSpan) != want) lv_dropdown_set_selected(s_ddSpan, want);
}

uint8_t chartSpecs(DeckRowSpec *r) {
    uint8_t n = 0;
    // The owner's order and words (rounds 4 and 5).
    r[n++] = { "Span", DeckRowKind::ROW_DROP, "24 hours\n12 hours\n6 hours\n1 hour", s_hv.span, true,
               spanDropCb, &s_ddSpan };
    r[n++] = { "Legend", DeckRowKind::ROW_DROP, "None\nX axis\nY axis\nX / Y axes", s_hv.legend, true, legendDropCb, nullptr };
    r[n++] = { "Show min/max", DeckRowKind::ROW_CHECK, nullptr, (uint16_t)s_hv.minMax, true, minMaxCb, nullptr };
    r[n++] = { "Shading: under line", DeckRowKind::ROW_CHECK, nullptr, (uint16_t)s_hv.shade, true, shadeCb, nullptr };
    r[n++] = { "Span buttons", DeckRowKind::ROW_CHECK, nullptr, (uint16_t)s_hv.chips, true, chipsCb, nullptr };
    return n;
}

uint8_t activitySpecs(DeckRowSpec *r) {
    uint8_t n = 0;
    r[n++] = { "Span", DeckRowKind::ROW_DROP, "Last 24 hours\nLast 7 days", s_hv.period, true,
               periodDropCb, &s_ddSpan };
    r[n++] = { "Show duration", DeckRowKind::ROW_CHECK, nullptr, (uint16_t)s_hv.duration, true, durationCb, nullptr };
    r[n++] = { "Show triggers", DeckRowKind::ROW_CHECK, nullptr, (uint16_t)s_hv.causes, true, causesCb, nullptr };
    r[n++] = { "Span buttons", DeckRowKind::ROW_CHECK, nullptr, (uint16_t)s_hv.actChips, true, chipsCb, nullptr };
    return n;
}

// --- SCENES: the Scenes view's own options (K43; owner, 2026-10-13) ---------
// Show hidden scenes, a checkbox (the card's Scenes setting: Visible / All -
// "Off" was dropped); the Default scene; and the Scene action - what SETTINGS'
// Tap action "Scenes..." does: Load default or Cycle scenes. The default
// scene's choices are kept by their HA id when the row is made, so a later
// change to the list cannot make a pick land on another scene.
void hiddenScenesCb(lv_event_t *ev);
void sceneActionDropCb(lv_event_t *ev);
lv_obj_t *s_cbHidden = nullptr;

uint8_t scenesSpecs(DeckRowSpec *r) {
    uint8_t n = 0;
    r[n++] = { "Show hidden scenes", DeckRowKind::ROW_CHECK, nullptr,
               (uint16_t)(s.sceneShow == SceneShow::SCENES_ALL), true, hiddenScenesCb, &s_cbHidden };
    Card *held = cardOf(h.surface);
    if (held) {
        uint8_t nSc = 0;
        const uint16_t sel = tapSceneRowBuild(*held, nSc);
        if (nSc) {
            r[n++] = { "Default scene", DeckRowKind::ROW_DROP, s_tapSceneOpts, sel, held->hasId(),
                       tapSceneDropCb, &s.ddTapScene };
            r[n++] = { "Scene action", DeckRowKind::ROW_DROP, "Load default\nCycle scenes",
                       (uint16_t)(sceneActionOf(*held) == TapAction::TAP_CYCLE_SCENES), held->hasId(),
                       sceneActionDropCb, nullptr };
        }
    }
    return n;
}

// Show hidden scenes: the card's Scenes setting, Visible or All (K21, K43).
// The view follows at once; with nothing left to show, back to the controls.
void hiddenScenesCb(lv_event_t *ev) {
    const bool all = lv_obj_has_state((lv_obj_t *)lv_event_get_target(ev), LV_STATE_CHECKED);
    s.sceneShow = all ? SceneShow::SCENES_ALL : SceneShow::SCENES_VISIBLE;
    if (Card *c = cardOf(h.surface)) c->setSceneShow(s.sceneShow);
    keepCardSetting("scenes", sceneShowName(s.sceneShow));
    s.lastScene = -1;   // the offered list changed under the ring
    if (s.view != PopupView::VIEW_SCENES) return;
    if (hasScenes()) showView(PopupView::VIEW_SCENES);   // the stage only; this panel stays
    // Leaving Scenes ends this panel - and the checkbox whose event is
    // running. Not from inside the event (LESSONS): afterwards.
    else lv_async_call([](void *) { if (CardPopup::isOpen()) showView(controlsView()); }, nullptr);
}

} // namespace popup
