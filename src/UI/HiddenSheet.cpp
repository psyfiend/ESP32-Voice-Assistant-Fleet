// "Hidden cards" (2.11 round 6) - see include/UI/HiddenSheet.h. One view: the
// page's hidden cards, each a row of the kit's chip table with one chip,
// Restore. A card restored leaves the list at once and is back on the page -
// which is built again under the sheet (it holds no card of the page's).
#include "UI/HiddenSheet.h"
#include "UI/Kit.h"
#include "UI/Sheet.h"
#include "UI/UIToolkit.h"
#include <stdio.h>
#include <string.h>

namespace HiddenSheet {

static Kit::Sheet     S;
static Kit::SheetSpec sheet;
static Spec           s_spec;
static uint8_t        s_at[MAX];   // a row's card, as its index in the caller's list
static uint8_t        s_n = 0;     // rows still listed
static char           s_title[48];

static const char *const RESTORE_W = "Restore";   // a row the caller gave no words

static void closeCb(lv_event_t *ev) { (void)ev; Kit::sheetClose(S); }

static void fill();

static bool restoreRow(uint8_t row) {
    if (row >= s_n || !s_spec.restore) return false;
    if (!s_spec.restore(s_spec.user, s_at[row])) return false;
    char msg[64];
    snprintf(msg, sizeof(msg), "%s restored", s_spec.names[s_at[row]]);
    UIToolkit::show_toast(msg);
    for (uint8_t k = row; k + 1 < s_n; k++) s_at[k] = s_at[k + 1];
    s_n--;
    fill();
    return true;
}

// Deferred: the chip is in the table about to be made again.
static void chipCb(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    lv_async_call([](void *p) { restoreRow((uint8_t)((uintptr_t)p >> 8)); }, (void *)v);
}

static void fill() {
    lv_obj_clean(S.stage);
    const int32_t w = lv_area_get_width(&S.winRect) - 2 * S.pad;
    Kit::ChipRowSpec rows[MAX];
    for (uint8_t k = 0; k < s_n; k++) {
        const char *const *chip = s_spec.chip[s_at[k]] ? &s_spec.chip[s_at[k]] : &RESTORE_W;   // one chip: its button
        rows[k] = { s_spec.names[s_at[k]], chip, 1, 1 };
    }
    lv_obj_t *t = Kit::chipTable(S.stage, w, rows, s_n, chipCb);
    lv_obj_t *l = Kit::label(t, Kit::KitStyle::KS_LABEL_TAG_DIM);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    // The owner's words (2.11 round 14b); a member hidden from its cluster is not here (K125).
    lv_label_set_text(l, s_n ? "Tap to restore a card back to the dashboard with its original settings intact.\n"
                               "Cards hidden from clusters can be restored from the cluster 'Members' view."
                             : "No hidden cards on this page.");
    if (s_n && s_spec.note) {
        lv_obj_t *m = Kit::label(t, Kit::KitStyle::KS_LABEL_TAG_DIM);
        lv_obj_set_width(m, lv_pct(100));
        lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
        lv_label_set_text(m, s_spec.note);
    }
}

static void build() {
    lv_label_set_text(S.lblTitleArea, s_title);
    lv_label_set_text(S.lblTitleName, "Hidden cards");
    Kit::sheetLayoutTitle(S);
    fill();
}

bool open(const Spec &spec) {
    if (Kit::sheetShowing()) return false;
    s_spec = spec;
    s_n = 0;
    for (uint8_t k = 0; k < spec.n && k < MAX; k++) s_at[s_n++] = k;
    snprintf(s_title, sizeof(s_title), "%s > ", spec.page ? spec.page : "Page");
    sheet = Kit::SheetSpec{};
    sheet.leftCb     = closeCb;
    sheet.buildFirst = build;
    sheet.sparks     = true;
    Kit::sheetPlace(S);
    Kit::sheetOpen(S, sheet);
    return true;
}

void close()  { Kit::sheetClose(S); }
bool isOpen() { return S.win != nullptr; }

#ifdef DEBUG_POPUP
bool restoreDebug(uint8_t i) { return isOpen() && restoreRow(i); }
#endif

}  // namespace HiddenSheet
