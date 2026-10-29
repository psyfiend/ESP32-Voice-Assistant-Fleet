// Chip tables (2.8 round 5) - see include/UI/Kit.h, "CHIP TABLES". UI Tweaks'
// rows and a card window's Card UI: an option's name and its chips, one row each.
#include "UI/Kit.h"
#include "UI/UITokens.h"
#include <stdio.h>

namespace Kit {

namespace {
// UNLIT IS SATIN (owner, round 6): flat, a chip "just looks like a void on the
// window" on the dark schemes and a white pill lost in it on Linen, where
// satin's edge is a shade. Lit keeps its role's tint and accent ring.
void chipFinish(lv_obj_t *b, bool on) {
    if (on) { unfinish(b); return; }
    const UIShape &k = UI::shape();
    finish(b, lv_obj_get_style_width(b, LV_PART_MAIN), UI::mm(k.H_CHIP), UI::mm(k.R_CHIP), Finish::FINISH_SATIN);
}

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
    chipFinish(b, on);
    return b;
}

// A SLIDER ROW (2.11 round 4): its words - the value - follow the knob as it
// moves (the row's last child, after the slider); the caller hears it on
// release, as it hears a chip's tap.
void sliderWords(lv_obj_t *words, int32_t v, const char *unit) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%s", (long)v, unit ? unit : "");
    lv_label_set_text(words, buf);
}

void sliderMoved(lv_event_t *e) {
    lv_obj_t *sl = lv_event_get_target_obj(e);
    lv_obj_t *line = lv_obj_get_parent(sl);
    lv_obj_t *words = lv_obj_get_child(line, -1);
    sliderWords(words, lv_slider_get_value(sl), (const char *)lv_event_get_user_data(e));
}

lv_obj_t *sliderAt(lv_obj_t *line, const ChipRowSpec &row, int32_t w, lv_event_cb_t cb, uintptr_t ud) {
    const UIShape &k = UI::shape();
    char widest[16];
    snprintf(widest, sizeof(widest), "%d%s", (int)row.hi, row.unit ? row.unit : "");
    const int32_t ww = textW(widest, UI::type().TAG);
    lv_obj_t *sl = lv_slider_create(line);
    lv_obj_remove_style_all(sl);
    add(sl, KitStyle::KS_SLIDER);
    add(sl, KitStyle::KS_SLIDER_IND, LV_PART_INDICATOR);
    add(sl, KitStyle::KS_SLIDER_KNOB, LV_PART_KNOB);
    lv_obj_set_width(sl, LV_MAX(UI::mm(k.H_CHIP) * 2, w - ww - UI::mm(k.S5)));
    lv_obj_set_ext_click_area(sl, UI::mm(k.H_CHIP) / 2);   // a finger's room round a thin track
    lv_slider_set_range(sl, row.lo, row.hi);
    lv_slider_set_value(sl, row.value, LV_ANIM_OFF);
    lv_obj_add_event_cb(sl, sliderMoved, LV_EVENT_VALUE_CHANGED, (void *)row.unit);
    if (cb) lv_obj_add_event_cb(sl, cb, LV_EVENT_RELEASED, (void *)ud);
    lv_obj_t *words = label(line, KitStyle::KS_LABEL_TAG);   // last: sliderMoved() finds it so
    sliderWords(words, row.value, row.unit);
    return sl;
}

bool isCheck(const ChipRowSpec &row) { return !row.n && !row.words && row.hi <= row.lo && !row.says; }

// A chip lit or not, after a choice: its face and words swapped over.
void lightChip(lv_obj_t *b, bool on) {
    lv_obj_t *l = labelIn(b);   // after the face and its veil, on a satin chip
    lv_obj_remove_style(b, style(on ? KitStyle::KS_CHIP_UNLIT : KitStyle::KS_CHIP_LIT), 0);
    add(b, on ? KitStyle::KS_CHIP_LIT : KitStyle::KS_CHIP_UNLIT);
    chipFinish(b, on);
    if (l) {
        lv_obj_remove_style(l, style(on ? KitStyle::KS_LABEL_TAG : KitStyle::KS_LABEL_TAG_ACCENT), 0);
        add(l, on ? KitStyle::KS_LABEL_TAG_ACCENT : KitStyle::KS_LABEL_TAG);
    }
}

// A check row's box: the row's second child, after its name.
lv_obj_t *checkOf(lv_obj_t *table, uint8_t row) {
    lv_obj_t *line = table ? lv_obj_get_child(table, row) : nullptr;
    lv_obj_t *c = line ? lv_obj_get_child(line, 1) : nullptr;
    return (c && lv_obj_check_type(c, &lv_checkbox_class)) ? c : nullptr;
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
            if (isCheck(rows[r])) span[r] = controlH();   // the box
            if (rows[r].says) span[r] = textW(rows[r].says, UI::type().NAME);
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
    // A slider row is as wide as the widest row of chips (half the table with none).
    const int32_t sliderW = maxSpan > 0 ? LV_MIN(maxSpan, w) : w / 2;
    for (uint8_t r = 0; r < n; r++) {
        const ChipRowSpec &row = rows[r];
        lv_obj_t *line = plain(list);
        lv_obj_t *words = label(line, nameLook);
        lv_label_set_text(words, row.what);
        if (row.hi > row.lo) {
            const int32_t trackH = LV_MAX(4, chipH / 3);
            int32_t x, y;
            if (stacked) {
                lv_obj_set_size(line, w, nameH + UI::mm(k.S1) + chipH);
                lv_obj_set_pos(words, (w - nameW[r]) / 2, 0);
                x = (w - sliderW) / 2;
                y = nameH + UI::mm(k.S1);
            } else {
                lv_obj_set_size(line, w, chipH);
                lv_obj_set_pos(words, col - nameW[r], (chipH - nameH) / 2);
                x = col + gapX;
                y = 0;
            }
            lv_obj_t *sl = sliderAt(line, row, sliderW, cb, ((uintptr_t)r << 8) | CHIP_SLIDER);
            lv_obj_set_pos(sl, x, y + (chipH - trackH) / 2);
            lv_obj_t *val = lv_obj_get_child(line, -1);
            lv_obj_set_pos(val, x + lv_obj_get_style_width(sl, LV_PART_MAIN) + UI::mm(k.S5),
                           y + (chipH - lv_font_get_line_height(UI::type().TAG)) / 2);
            continue;
        }
        if (row.says) {
            lv_obj_t *v = label(line, KitStyle::KS_LABEL_TAG_ACCENT);
            lv_obj_set_style_text_font(v, UI::type().NAME, 0);
            lv_label_set_text(v, row.says);
            const int32_t vh = lv_font_get_line_height(UI::type().NAME);
            if (stacked) {
                lv_obj_set_size(line, w, nameH + UI::mm(k.S1) + vh);
                lv_obj_set_pos(words, (w - nameW[r]) / 2, 0);
                lv_obj_set_pos(v, (w - span[r]) / 2, nameH + UI::mm(k.S1));
            } else {
                lv_obj_set_size(line, w, LV_MAX(chipH, vh));
                lv_obj_set_pos(words, col - nameW[r], (LV_MAX(chipH, vh) - nameH) / 2);
                lv_obj_set_pos(v, col + gapX, (LV_MAX(chipH, vh) - vh) / 2);
            }
            continue;
        }
        if (isCheck(row)) {
            const int32_t box = controlH();
            const RowSpec rs = { row.what, RowKind::ROW_CHECK, nullptr, row.lit, true, nullptr, nullptr };
            lv_obj_t *c = checkbox(line, rs);
            if (cb) lv_obj_add_event_cb(c, cb, LV_EVENT_VALUE_CHANGED, (void *)(((uintptr_t)r << 8) | CHIP_CHECK));
            if (stacked) {
                lv_obj_set_size(line, w, nameH + UI::mm(k.S1) + box);
                lv_obj_set_pos(words, (w - nameW[r]) / 2, 0);
                lv_obj_set_pos(c, (w - box) / 2, nameH + UI::mm(k.S1));
            } else {
                lv_obj_set_size(line, w, LV_MAX(chipH, box));
                lv_obj_set_pos(words, col - nameW[r], (LV_MAX(chipH, box) - nameH) / 2);
                lv_obj_set_pos(c, col + gapX, (LV_MAX(chipH, box) - box) / 2);
            }
            continue;
        }
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
    if (!line || lit == CHIP_SLIDER || lit == CHIP_CHECK) return;
    if (lv_obj_check_type(lv_obj_get_child(line, 1), &lv_slider_class)) return;   // a slider row
    if (checkOf(table, row)) return;                                              // a check row
    if (lv_obj_get_child_count(line) == 2 && lv_obj_check_type(lv_obj_get_child(line, 1), &lv_label_class))
        return;                                                                   // a words row
    for (uint32_t i = 1; i < lv_obj_get_child_count(line); i++)   // child 0: the row's name
        lightChip(lv_obj_get_child(line, (int32_t)i), (i - 1) == lit);
}

// A CHIP GRID (round 14, the owner's group picker): the chips one width - the
// longest word's - wrapping in rows, centred, the chosen one lit.
lv_obj_t *chipGrid(lv_obj_t *parent, int32_t w, const char *const *words, uint8_t n, uint8_t lit,
                   lv_event_cb_t cb) {
    const UIShape &k = UI::shape();
    int32_t widest = 0;
    for (uint8_t i = 0; i < n; i++) widest = LV_MAX(widest, textW(words[i], UI::type().TAG));
    const int32_t cw = LV_MIN(w, LV_MAX(2 * UI::mm(k.H_CHIP), widest + 2 * UI::mm(k.S5)));
    lv_obj_t *grid = plain(parent);
    lv_obj_set_size(grid, w, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(grid, UI::mm(k.S3), 0);
    lv_obj_set_style_pad_row(grid, UI::mm(k.S3), 0);
    for (uint8_t i = 0; i < n; i++) chipAt(grid, words[i], cw, i == lit, cb, i);
    return grid;
}

void chipGridLit(lv_obj_t *grid, uint8_t lit) {
    for (uint32_t i = 0; grid && i < lv_obj_get_child_count(grid); i++)
        lightChip(lv_obj_get_child(grid, (int32_t)i), i == lit);
}

// A slider row's slider: the row's second child, after its name.
static lv_obj_t *sliderOf(lv_obj_t *table, uint8_t row) {
    lv_obj_t *line = table ? lv_obj_get_child(table, row) : nullptr;
    lv_obj_t *sl = line ? lv_obj_get_child(line, 1) : nullptr;
    return (sl && lv_obj_check_type(sl, &lv_slider_class)) ? sl : nullptr;
}

int16_t chipTableValue(lv_obj_t *table, uint8_t row) {
    lv_obj_t *sl = sliderOf(table, row);
    return sl ? (int16_t)lv_slider_get_value(sl) : 0;
}

void chipTableSet(lv_obj_t *table, uint8_t row, int16_t value) {
    lv_obj_t *sl = sliderOf(table, row);
    if (!sl) return;
    lv_slider_set_value(sl, value, LV_ANIM_OFF);
    lv_obj_send_event(sl, LV_EVENT_VALUE_CHANGED, nullptr);   // its words follow
}

bool chipTableChecked(lv_obj_t *table, uint8_t row) {
    lv_obj_t *c = checkOf(table, row);
    return c && lv_obj_has_state(c, LV_STATE_CHECKED);
}

void chipTableCheck(lv_obj_t *table, uint8_t row, bool on) {
    lv_obj_t *c = checkOf(table, row);
    if (!c) return;
    if (on) lv_obj_add_state(c, LV_STATE_CHECKED);
    else    lv_obj_remove_state(c, LV_STATE_CHECKED);
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
