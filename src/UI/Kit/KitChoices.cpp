// Choice rows (2.8 round 3) and inherit rows (round 4) - see include/UI/Kit.h,
// "CHOICE ROWS". UI Tweaks' rows, and a card window's Card UI: words, and the
// kit's compact selector.
#include "UI/Kit.h"
#include "UI/UITokens.h"

namespace Kit {

lv_obj_t *choiceList(lv_obj_t *parent, int32_t w, bool stacked) {
    const UIShape &k = UI::shape();
    lv_obj_t *list = plain(parent);
    lv_obj_set_width    (list, w);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, stacked ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, UI::mm(stacked ? k.S3 : k.S4), 0);
    UI::tameScroll(list);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    UI::unclipShadows(list);
    return list;
}

lv_obj_t *choiceRows(lv_obj_t *parent, int32_t w, const ChoiceRow *rows, uint8_t n, Selector *S) {
    const UIShape &k = UI::shape();
    const int32_t gapX = UI::mm(k.S5);
    const int32_t g    = UI::mm(k.S1);

    int32_t wordsW = 0, slotW = 0;
    uint8_t most = 1;
    for (uint8_t r = 0; r < n; r++) {
        wordsW = LV_MAX(wordsW, textW(rows[r].what, UI::type().NAME));
        slotW  = LV_MAX(slotW, selectorSlotW(rows[r].words, rows[r].n));
        most   = LV_MAX(most, rows[r].n);
    }
    const int32_t selMaxW = most * slotW + (most + 1) * g;
    const bool    stacked = wordsW + gapX + selMaxW > w;
    if (stacked && selMaxW > w) slotW = (w - (most + 1) * g) / most;

    lv_obj_t *list = choiceList(parent, w, stacked);
    for (uint8_t r = 0; r < n; r++) {
        const ChoiceRow &row = rows[r];
        lv_obj_t *line = plain(list);
        lv_obj_set_size(line, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(line, stacked ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(line, gapX, 0);
        lv_obj_set_style_pad_row(line, g, 0);
        UI::unclipShadows(line);

        lv_obj_t *words = label(line, KitStyle::KS_LABEL_NAME_DIM);
        lv_label_set_text(words, row.what);
        if (!stacked) lv_obj_set_width(words, wordsW);

        SelectorSpec sp = {};
        for (uint8_t i = 0; i < row.n && i < SELECTOR_MAX; i++) sp.glyph[i] = row.words[i];
        sp.n        = row.n;
        sp.chosen   = row.chosen;
        sp.compact  = true;
        sp.slotW    = slotW;
        sp.onChosen = row.onChosen;
        selector(S[r], line, sp);
    }
    return list;
}

lv_obj_t *inheritRow(lv_obj_t *list, const char *what, bool inherited, lv_event_cb_t cb, void *user) {
    const UIShape &k = UI::shape();
    lv_obj_t *row = plain(list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(row, UI::mm(k.S1), 0);
    UI::unclipShadows(row);

    // Its words, and at the end of the line the Inherit toggle: glass while on.
    lv_obj_t *head = plain(row);
    lv_obj_set_size(head, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    UI::unclipShadows(head);
    lv_label_set_text(label(head, KitStyle::KS_LABEL_NAME_DIM), what);
    const int32_t tw = textW("Inherit", UI::type().NAME) + 2 * UI::mm(k.S5);
    lv_obj_t *t = toggleButton(head, "Inherit", tw, UI::mm(k.H_CHIP), inherited, nullptr);
    if (cb) lv_obj_add_event_cb(t, cb, LV_EVENT_CLICKED, user);
    if (inherited) return nullptr;

    // Off: the row's own choices, under its words.
    lv_obj_t *line = plain(row);
    lv_obj_set_size(line, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(line, UI::mm(k.S5), 0);
    UI::unclipShadows(line);
    return line;
}

void choiceSelector(Selector &S, lv_obj_t *line, const char *const *words, uint8_t n, uint8_t chosen,
                    void (*onChosen)(uint8_t i), int32_t room) {
    SelectorSpec sp = {};
    for (uint8_t i = 0; i < n && i < SELECTOR_MAX; i++) sp.glyph[i] = words[i];
    sp.n        = n;
    sp.chosen   = chosen;
    sp.compact  = true;
    // This choice's own longest word - narrowed to fit `room` (the 4B's window).
    const int32_t g = UI::mm(UI::shape().S1);
    sp.slotW    = selectorSlotW(words, n);
    if (room > 0 && n && n * sp.slotW + (n + 1) * g > room) sp.slotW = (room - (n + 1) * g) / n;
    sp.room     = room;
    sp.onChosen = onChosen;
    selector(S, line, sp);
}

lv_obj_t *buttonRow(lv_obj_t *list) {
    const UIShape &k = UI::shape();
    lv_obj_t *r = plain(list);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, UI::mm(k.S5), 0);
    lv_obj_set_style_pad_top(r, UI::mm(k.S3), 0);
    UI::unclipShadows(r);
    return r;
}

} // namespace Kit
