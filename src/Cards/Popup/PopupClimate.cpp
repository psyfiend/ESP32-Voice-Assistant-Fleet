// A THERMOSTAT'S WINDOW, READ ONLY (K144): the room's temperature large with what it is doing under
// it, and beside them the facts - its mode, its setpoint, what it is doing, the
// humidity and the fan, each only when HA says it. History is its tab, as any
// card's. Controls - the setpoint, the mode - are not built (card library, #31).
//
// Wording still to confirm with the owner: "Mode", "Heat to" (K145's, as a row),
// "Doing", "Humidity", "Fan".
#include "PopupState.h"

namespace popup {

namespace {

void capital(char *s) {
    if (s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 'a' + 'A');
    for (char *p = s; *p; p++) if (*p == '_') *p = ' ';
}

}  // namespace

void buildClimate() {
    const Entity &e = *s.ent[0];
    const EntityAttrs &a = e.attrs;
    lv_obj_t *row = plain(s.stage);
    lv_obj_set_size(row, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, pm(UI::shape().P_GAP), 0);

    // The room's temperature, and what the thermostat is doing about it.
    lv_obj_t *hero = plain(row);
    lv_obj_set_size(hero, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(hero, mm(UI::shape().S5), 0);
    lv_obj_t *vr = plain(hero);
    lv_obj_set_size(vr, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(vr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(vr, mm(UI::shape().S3), 0);
    char v[24];
    cardFormatValue(e, v, sizeof(v), false, s.tempUnit);
    lv_obj_t *num = Kit::label(vr, KS::KS_CLUSTER_VALUE);
    lv_obj_set_style_text_font(num, UI::type().VALUE, 0);
    lv_obj_set_size(num, LV_SIZE_CONTENT, LV_SIZE_CONTENT);   // the role is a header's, sized for its face
    lv_label_set_text(num, v);
    lv_label_set_text(Kit::label(vr, KS::KS_CARD_UNIT), cardDisplayUnit(e, s.tempUnit));
    char act[20];
    snprintf(act, sizeof(act), "%s", a.action[0] ? a.action : "");
    capital(act);
    if (act[0]) lv_label_set_text(Kit::label(hero, KS::KS_LABEL_TAG), act);

    // The facts, each only when HA gave it.
    lv_obj_t *list = Kit::factList(row);
    lv_obj_set_size(list, lv_pct(50), LV_SIZE_CONTENT);
    char w[32];
    if (e.everSet && e.value.type == ValueType::TEXT_VAL) {
        snprintf(w, sizeof(w), "%s", e.value.text);
        capital(w);
        Kit::factRow(list, "Mode", w);
    }
    if (a.target10 != EntityAttrs::ATTR_NONE) {
        snprintf(w, sizeof(w), "%.0f%s", (double)cardDisplayNumber(e, a.target10 / 10.0f, s.tempUnit),
                 cardDisplayUnit(e, s.tempUnit));
        char what[16] = "Heat to";
        if (e.everSet && !strcmp(e.value.text, "cool")) snprintf(what, sizeof(what), "Cool to");
        Kit::factRow(list, what, w);
    }
    if (act[0]) Kit::factRow(list, "Doing", act);
    if (a.curHumidity >= 0) { snprintf(w, sizeof(w), "%d%%", (int)a.curHumidity); Kit::factRow(list, "Humidity", w); }
    if (a.fanMode[0]) { snprintf(w, sizeof(w), "%s", a.fanMode); capital(w); Kit::factRow(list, "Fan", w); }
    Kit::factColumns(list);
}

}  // namespace popup
