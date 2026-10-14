// Chips (2.12 P3, from History's span buttons - owner, 2.10e round 2): a row of
// small buttons, the chosen one lit in the accent.
#include "UI/Kit.h"

namespace Kit {

lv_obj_t *chipRow(lv_obj_t *parent, const char *const *words, uint8_t n, uint8_t lit, int32_t room,
                  lv_event_cb_t cb) {
    const UIShape &k = UI::shape();
    lv_obj_t *row = plain(parent);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    int32_t padH = UI::mm(k.S_CHIP_SIDE), gap = UI::mm(k.S4);
    if (room > 0) {
        int32_t text = 0;
        for (uint8_t i = 0; i < n; i++) text += textW(words[i], UI::type().TAG);
        const int32_t spare = room - text;   // for 2*n paddings and n-1 gaps
        const int32_t full  = 2 * n * padH + (n - 1) * gap;
        if (spare < full) {
            const int32_t s = LV_MAX(spare, full / 2);
            padH = padH * s / full;
            gap  = gap * s / full;
        }
    }
    lv_obj_set_style_pad_column(row, gap, 0);
    for (uint8_t i = 0; i < n; i++) {
        const bool on = (i == lit);
        lv_obj_t *b = plain(row);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        add(b, KitStyle::KS_CHIP);
        add(b, on ? KitStyle::KS_CHIP_LIT : KitStyle::KS_CHIP_UNLIT);
        add(b, KitStyle::KS_CHIP_PRESSED, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
        lv_obj_set_style_pad_hor(b, padH, 0);
        if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *l = label(b, on ? KitStyle::KS_LABEL_TAG_ACCENT : KitStyle::KS_LABEL_TAG);
        lv_label_set_text(l, words[i]);
        lv_obj_center(l);
    }
    return row;
}

} // namespace Kit
