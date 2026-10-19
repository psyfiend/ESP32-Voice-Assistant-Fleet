// The on-screen keyboard (2.11, K95): words typed on the glass. Put off three
// times (K37) and taken on for 2.11, where group and cluster names first need
// it; the card's custom name (K37) uses it too. LVGL's own keyboard and text
// area, every theme style taken off and the kit's roles put on, so a scheme
// change repaints it like everything else. Costs lv_mem only while it shows.
#include "UI/Kit.h"
#include "UI/Sheet.h"

namespace Kit {

static lv_obj_t *s_dim = nullptr, *s_ta = nullptr, *s_kb = nullptr;
static void    (*s_save)(const char *text, void *user) = nullptr;
static void     *s_user = nullptr;

// PLAIN ASCII, space to tilde: the panels' fonts draw nothing else, and a name
// with a character they lack shows a box. LVGL's keys are all in this range;
// the field refuses anything else (a newline above all - one line).
static const char *accepted() {
    static char s[0x7F - 0x20 + 1] = "";
    if (!s[0])
        for (char c = 0x20; c < 0x7F; c++) s[c - 0x20] = c;
    return s;
}

void keyboardClose() {
    if (!s_dim) return;
    lv_obj_delete(s_dim);
    s_dim = s_ta = s_kb = nullptr;
    s_save = nullptr;
    s_user = nullptr;
}

bool keyboardShowing() { return s_dim != nullptr; }

// Save: the words trimmed, handed over, and the keyboard gone. Deferred, as
// every close is: the key's own event is still running.
static void saveAsync(void *) {
    if (!s_ta) return;
    char buf[KEYBOARD_TEXT_MAX + 1];
    const char *t = lv_textarea_get_text(s_ta);
    while (*t == ' ') t++;
    snprintf(buf, sizeof(buf), "%s", t);
    for (size_t n = strlen(buf); n && buf[n - 1] == ' '; n--) buf[n - 1] = '\0';
    void (*save)(const char *, void *) = s_save;
    void *user = s_user;
    keyboardClose();
    if (save) save(buf, user);
}

static void cancelAsync(void *) { keyboardClose(); }

static void saveCb(lv_event_t *)   { lv_async_call(saveAsync, nullptr); }
static void cancelCb(lv_event_t *) { lv_async_call(cancelAsync, nullptr); }

// The tick and return send READY to the field, the hide key CANCEL.
static void fieldCb(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY)  saveCb(e);
    if (code == LV_EVENT_CANCEL) cancelCb(e);
}

// A tap on the dim itself - not on the box or the keys - is Cancel.
static void dimCb(lv_event_t *e) {
    if (lv_event_get_target(e) == lv_event_get_current_target(e)) cancelCb(e);
}

lv_obj_t *keyboard(const KeyboardSpec &spec) {
    keyboardClose();
    const UIShape &k = UI::shape();
    const int32_t scrW = lv_display_get_horizontal_resolution(nullptr);
    const int32_t scrH = lv_display_get_vertical_resolution(nullptr);

    s_save = spec.save;
    s_user = spec.user;
    s_dim = plain(lv_layer_top());
    lv_obj_set_size(s_dim, lv_pct(100), lv_pct(100));
    add(s_dim, KitStyle::KS_DIALOG_DIM);
    lv_obj_add_flag(s_dim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_dim, dimCb, LV_EVENT_CLICKED, nullptr);

    // The keys along the bottom: four rows KEY_H tall, KEY_GAP between and round them.
    s_kb = lv_keyboard_create(s_dim);
    lv_obj_remove_style_all(s_kb);
    add(s_kb, KitStyle::KS_KBD);
    add(s_kb, KitStyle::KS_KEY,         LV_PART_ITEMS);
    add(s_kb, KitStyle::KS_KEY_CTRL,    UI::part(LV_PART_ITEMS, LV_STATE_CHECKED));
    add(s_kb, KitStyle::KS_KEY_PRESSED, UI::part(LV_PART_ITEMS, LV_STATE_PRESSED));
    const int32_t kbH = 4 * UI::mm(k.KEY_H) + 5 * UI::mm(k.KEY_GAP);
    lv_obj_set_size(s_kb, lv_pct(100), kbH);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);

    // The box: what the words are for, the field, Cancel and Save.
    const int32_t boxW = LV_MIN(scrW - 2 * UI::mm(k.S7), UI::mm(k.W_DIALOG_MAX));
    lv_obj_t *box = plain(s_dim);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);   // a tap on it is not a tap outside
    lv_obj_set_size(box, boxW, LV_SIZE_CONTENT);
    add(box, KitStyle::KS_KBD_BOX);

    lv_obj_t *title = label(box, KitStyle::KS_LABEL_NAME);
    lv_label_set_text(title, spec.title ? spec.title : "");

    s_ta = lv_textarea_create(box);
    lv_obj_remove_style_all(s_ta);
    add(s_ta, KitStyle::KS_FIELD);
    add(s_ta, KitStyle::KS_FIELD_CURSOR, UI::part(LV_PART_CURSOR, LV_STATE_FOCUSED));
    add(s_ta, KitStyle::KS_LABEL_NAME_DIM, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_width(s_ta, lv_pct(100));
    lv_textarea_set_one_line(s_ta, true);
    lv_textarea_set_accepted_chars(s_ta, accepted());
    lv_textarea_set_max_length(s_ta, LV_CLAMP(1, spec.maxLen, KEYBOARD_TEXT_MAX));
    if (spec.hint) lv_textarea_set_placeholder_text(s_ta, spec.hint);
    lv_textarea_set_text(s_ta, spec.text ? spec.text : "");
    lv_textarea_set_cursor_pos(s_ta, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_add_event_cb(s_ta, fieldCb, LV_EVENT_READY,  nullptr);
    lv_obj_add_event_cb(s_ta, fieldCb, LV_EVENT_CANCEL, nullptr);
    lv_keyboard_set_textarea(s_kb, s_ta);   // focused: its cursor shows

    lv_obj_t *row = plain(box);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    add(row, KitStyle::KS_DIALOG_ROW);
    const int32_t rowW = boxW - 2 * UI::mm(k.S7);
    const int32_t gap  = UI::mm(k.S7);
    const int32_t btnW = LV_MIN(UI::mm(k.W_DIALOG_BTN), (rowW - 3 * gap) / 2);
    button(row, ButtonRole::BTN_CANCEL,  "Cancel", btnW, controlH(), cancelCb);
    button(row, ButtonRole::BTN_CONFIRM, "Save",   btnW, controlH(), saveCb);
    countdownFollow(box, boxW);

    // The box in the middle of what the keys leave.
    lv_obj_update_layout(s_dim);
    const int32_t boxH = lv_obj_get_height(box);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, LV_MAX(UI::mm(k.S4), (scrH - kbH - boxH) / 2));
    return s_dim;
}

#ifdef DEBUG_POPUP
bool keyboardDebug(const char *type, int mode, int press) {
    if (!s_dim) return false;
    if (press == 3) lv_textarea_set_text(s_ta, "");
    if (type && type[0]) lv_textarea_add_text(s_ta, type);
    if (mode == 0) lv_keyboard_set_mode(s_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    if (mode == 1) lv_keyboard_set_mode(s_kb, LV_KEYBOARD_MODE_TEXT_UPPER);
    if (mode == 2) lv_keyboard_set_mode(s_kb, LV_KEYBOARD_MODE_SPECIAL);
    if (press == 1) saveAsync(nullptr);
    if (press == 2) keyboardClose();
    return true;
}
#endif

} // namespace Kit
