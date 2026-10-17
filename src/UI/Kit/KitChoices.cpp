// Chip tables (2.8 round 5) - see include/UI/Kit.h, "CHIP TABLES". UI Tweaks'
// rows and a card window's Card UI: an option's name and its chips, one row each.
#include "UI/Kit.h"
#include "UI/UITokens.h"

namespace Kit {

namespace {
lv_obj_t *chipAt(lv_obj_t *line, const char *word, int32_t cw, bool on, lv_event_cb_t cb, uintptr_t ud) {
    lv_obj_t *b = plain(line);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    add(b, KitStyle::KS_CHIP);
    add(b, on ? KitStyle::KS_CHIP_LIT : KitStyle::KS_CHIP_UNLIT);
    add(b, KitStyle::KS_CHIP_PRESSED, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_set_width(b, cw);   // every chip in its row one width
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)ud);
    lv_obj_t *l = label(b, on ? KitStyle::KS_LABEL_TAG_ACCENT : KitStyle::KS_LABEL_TAG);
    lv_label_set_text(l, word ? word : "");
    lv_obj_center(l);
    return b;
}
}  // namespace

// THE LAYOUT (owner, round 5): the longest row - name, gap, chips - centred in
// the window; every other row's name right-justified against it, its chips
// left-justified from the same column. A row's chips one width (its longest
// word and sides, at least two chips' heights - the timeouts a little wider
// than they need, to look like the rest). Too narrow for that (the 4B): every
// row's name above its chips, both centred.
lv_obj_t *chipTable(lv_obj_t *parent, int32_t w, const ChipRowSpec *rows, uint8_t n, lv_event_cb_t cb) {
    const UIShape &k = UI::shape();
    const int32_t gapX = UI::mm(k.S5), chipH = UI::mm(k.H_CHIP);
    const lv_font_t *nameF = UI::type().NAME;
    KitStyle nameLook = KitStyle::KS_LABEL_NAME_DIM;
    if (n > CHIP_ROWS_MAX) n = CHIP_ROWS_MAX;

    int32_t cw[CHIP_ROWS_MAX] = {}, span[CHIP_ROWS_MAX] = {}, nameW[CHIP_ROWS_MAX] = {};
    int32_t total = 0, maxName = 0, maxSpan = 0, gap = 0, col = 0;
    // Measured at the chips' full sides; if the longest row will not fit, at
    // tighter ones (Card UI's five-chip rows on the P4_5) before giving up on
    // the two columns.
    auto measure = [&](int32_t side, int32_t g, int32_t minChip) {
        gap = g;
        total = maxName = maxSpan = 0;
        uint8_t longest = 0;
        for (uint8_t r = 0; r < n; r++) {
            int32_t widest = 0;
            for (uint8_t i = 0; i < rows[r].n; i++) widest = LV_MAX(widest, textW(rows[r].words[i], UI::type().TAG));
            cw[r]    = LV_MAX(minChip, widest + 2 * side);
            span[r]  = rows[r].n * cw[r] + (rows[r].n ? rows[r].n - 1 : 0) * gap;
            nameW[r] = textW(rows[r].what, nameF);
            maxName  = LV_MAX(maxName, nameW[r]);
            maxSpan  = LV_MAX(maxSpan, span[r]);
            if (nameW[r] + gapX + span[r] > total) { total = nameW[r] + gapX + span[r]; longest = r; }
        }
        // The column between names and chips: the longest row's, centred -
        // moved right if a longer name would otherwise run off the left, then
        // back left as far as the widest chips need (a long name on a short row).
        col = LV_MAX((w - total) / 2 + nameW[longest], maxName);
        col = LV_MIN(col, w - gapX - maxSpan);
        return total <= w && col >= maxName;
    };
    bool fits = measure(UI::mm(k.S5), UI::mm(k.S3), 2 * chipH);
    if (!fits) fits = measure(UI::mm(k.S2), UI::mm(k.S1), chipH * 3 / 2);
    if (!fits) {   // last: the names in the chips' own face (a long name beside a five-chip row)
        nameF = UI::type().TAG;
        nameLook = KitStyle::KS_LABEL_TAG_DIM;
        fits = measure(UI::mm(k.S2), UI::mm(k.S1), chipH * 3 / 2);
    }
    const bool stacked = !fits;

    lv_obj_t *list = plain(parent);
    lv_obj_set_width    (list, w);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, UI::mm(stacked ? k.S4 : k.S3), 0);
    UI::tameScroll(list);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    const int32_t nameH = lv_font_get_line_height(nameF);
    for (uint8_t r = 0; r < n; r++) {
        const ChipRowSpec &row = rows[r];
        lv_obj_t *line = plain(list);
        lv_obj_t *words = label(line, nameLook);
        lv_label_set_text(words, row.what);
        int32_t x, y, c = cw[r], s = span[r];
        if (stacked) {
            if (s > w) { c = (w - (row.n - 1) * gap) / LV_MAX(row.n, 1); s = w; }
            lv_obj_set_size(line, w, nameH + UI::mm(k.S1) + chipH);
            lv_obj_set_pos(words, (w - nameW[r]) / 2, 0);
            x = (w - s) / 2;
            y = nameH + UI::mm(k.S1);
        } else {
            lv_obj_set_size(line, w, chipH);
            lv_obj_set_pos(words, col - nameW[r], (chipH - nameH) / 2);
            x = col + gapX;
            y = 0;
        }
        for (uint8_t i = 0; i < row.n; i++) {
            lv_obj_t *b = chipAt(line, row.words[i], c, i == row.lit, cb, ((uintptr_t)r << 8) | i);
            lv_obj_set_pos(b, x + i * (c + gap), y);
        }
    }
    return list;
}

// A row's lit chip, after a choice: the chips' faces and words swapped over.
void chipTableLit(lv_obj_t *table, uint8_t row, uint8_t lit) {
    lv_obj_t *line = table ? lv_obj_get_child(table, row) : nullptr;
    if (!line) return;
    for (uint32_t i = 1; i < lv_obj_get_child_count(line); i++) {   // child 0: the row's name
        lv_obj_t *b = lv_obj_get_child(line, (int32_t)i);
        lv_obj_t *l = lv_obj_get_child(b, 0);
        const bool on = (i - 1) == lit;
        lv_obj_remove_style(b, style(on ? KitStyle::KS_CHIP_UNLIT : KitStyle::KS_CHIP_LIT), 0);
        add(b, on ? KitStyle::KS_CHIP_LIT : KitStyle::KS_CHIP_UNLIT);
        if (l) {
            lv_obj_remove_style(l, style(on ? KitStyle::KS_LABEL_TAG : KitStyle::KS_LABEL_TAG_ACCENT), 0);
            add(l, on ? KitStyle::KS_LABEL_TAG_ACCENT : KitStyle::KS_LABEL_TAG);
        }
    }
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
