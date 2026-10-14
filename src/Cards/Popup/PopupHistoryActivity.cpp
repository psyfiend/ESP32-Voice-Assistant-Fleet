// History: activity (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

// --- Activity --------------------------------------------------------------

// What caused a change, in a few words; "" when HA recorded nothing.
void histCause(const HistEvent &ev, char *out, size_t n) {
    out[0] = '\0';
    switch (ev.cause) {
        case CauseKind::CAUSE_USER: {
            const char *who = s_hist ? s_hist->personName(ev.who) : nullptr;
            snprintf(out, n, "By %s", who ? who : "a person");
            break;
        }
        case CauseKind::CAUSE_AUTOMATION: snprintf(out, n, "Automation: %s", ev.who); break;
        case CauseKind::CAUSE_ENTITY:     snprintf(out, n, "By %s", ev.who); break;
        case CauseKind::CAUSE_ACTION:     snprintf(out, n, "Action %s", ev.who); break;
        case CauseKind::CAUSE_OTHER:      snprintf(out, n, "By %s", ev.who); break;
        default: break;
    }
}

// The state as the card says it: On / Off, Open / Closed... - else HA's word.
void histStateWord(const HistEvent &ev, char *out, size_t n) {
    const EntityDescriptor &d = s_hv.e->desc;
    if (!strcmp(ev.state, "on"))       snprintf(out, n, "%s", cardStateWord(d, true));
    else if (!strcmp(ev.state, "off")) snprintf(out, n, "%s", cardStateWord(d, false));
    else if (!strcmp(ev.state, "unavailable")) snprintf(out, n, "Unavailable");
    else {
        snprintf(out, n, "%s", ev.state);
        if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
    }
}

void histDrawActivity(lv_obj_t *body) {
    const HaHistory &H = *s_hist;
    const uint16_t n = H.events();
    // The span buttons, as on the chart (owner, round 2), centred (round 6);
    // turned off, the span in words in their place (round 3).
    histTopMiddle(histTopRow(body));
    if (!n) {
        histNote(body, s_hv.period ? "No changes in the last 7 days" : "No changes in the last 24 hours");
        return;
    }
    // In from the window's sides (owner, round 2: the rows were wider than
    // they need be, even on the 4B), and NO WIDER THAN W_ROWS_MAX (round 6: on
    // the P4_5 and the 1060 the state and its time sat a long way apart). The
    // 4B's window is narrower than that, so it keeps its rows.
    const UIShape &k = UI::shape();
    lv_obj_update_layout(body);
    lv_obj_t *list = plain(body);
    s_histList = list;
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_style_pad_hor(list, LV_MAX(mm(k.S10), (lv_obj_get_width(body) - mm(k.W_ROWS_MAX)) / 2), 0);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, mm(k.S3), 0);

    // ONE VIEW (owner, round 5). The state and when are what matter: the state
    // on the left in the larger text; on the right the time in the larger
    // text over how long ago ("12 minutes ago", "Yesterday", "3 days ago - Oct
    // 8"). Under the state, in the small text: how long it lasted (Show
    // duration) and what triggered it (Show triggers). A tap adds the next
    // thing down - the trigger when they are off, HA's own words about it when
    // they are on. The days as headings between them. Words: TimeFormat.
    const uint16_t show = s_hv.more ? LV_MIN(n, HIST_ROWS_MAX) : LV_MIN(n, HIST_ROWS_FIRST);
    const time_t now = time(nullptr);
    char word[24], cause[96], when[40], ago[40], buf[200], lasted[24];
    int lastDay = INT32_MIN;
    for (uint16_t r = 0; r < show; r++) {
        const uint16_t i = n - 1 - r;   // newest first
        const HistEvent &ev = H.event(i);
        const bool open = (s_hv.openRow == (int16_t)i);
        // A heading where a new day starts, centred, the state's size.
        {
            const int d = tfDaysAgo((time_t)ev.t, now);
            if (d != lastDay) {
                lastDay = d;
                tfDay(buf, sizeof(buf), (time_t)ev.t, now);
                lv_obj_t *hd = Kit::label(list, KS::KS_LABEL_NAME_DIM);
                lv_label_set_text(hd, buf);
                lv_obj_set_width(hd, lv_pct(100));
                lv_obj_set_style_text_align(hd, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_style_pad_top(hd, r ? mm(k.S4) : 0, 0);
            }
        }
        lv_obj_t *row = Kit::item(list);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(row, mm(k.S4), 0);
        lv_obj_set_style_pad_row(row, mm(k.S1), 0);
        lv_obj_add_event_cb(row, histRowCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        lv_obj_t *line = plain(row);
        lv_obj_set_size(line, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(line, mm(k.S6), 0);
        lv_obj_t *col = plain(line);
        lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        histStateWord(ev, word, sizeof(word));
        lv_obj_t *l = Kit::label(col, KS::KS_LABEL_NAME);
        lv_label_set_text(l, word);
        // The small lines under the state; a long one wraps.
        auto small = [&](const char *txt) {
            lv_obj_t *s2 = Kit::label(col, KS::KS_LABEL_TAG_DIM);
            lv_label_set_long_mode(s2, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(s2, lv_pct(100));
            lv_label_set_text(s2, txt);
        };
        uint8_t lines = 1;
        if (s_hv.duration) {
            // "for: 2 hrs 38 min"; the newest, still going, "(and counting)".
            const time_t until = (i + 1 < n) ? (time_t)H.event(i + 1).t : now;
            tfDuration(lasted, sizeof(lasted), until > (time_t)ev.t ? (int32_t)(until - (time_t)ev.t) : 0);
            snprintf(buf, sizeof(buf), "for: %s%s", lasted, (i + 1 < n) ? "" : " (and counting)");
            small(buf);
            lines++;
        }
        histCause(ev, cause, sizeof(cause));
        const char *none = "Home Assistant recorded no trigger";
        if (s_hv.causes) {
            if (cause[0]) { small(cause); lines++; }
            if (open) { small(ev.detail[0] ? ev.detail : cause[0] ? "Home Assistant recorded nothing more" : none); lines++; }
        } else if (open) {
            small(cause[0] ? cause : none);
            lines++;
        }
        // The state alone stands in the middle of the row; with lines under
        // it, the two sides line up from the top - the larger texts together,
        // the smaller ones together.
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              lines > 1 ? LV_FLEX_ALIGN_START : LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        // The right side: the time over how long ago, or the day's word and date.
        lv_obj_t *right = plain(line);
        lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        tfClock(when, sizeof(when), (time_t)ev.t);
        l = Kit::label(right, KS::KS_LABEL_NAME);
        lv_label_set_text(l, when);
        tfAgo(ago, sizeof(ago), (time_t)ev.t, now, TfAgo::AGO_DATED);
        l = Kit::label(right, KS::KS_LABEL_TAG_DIM);
        lv_label_set_text(l, ago);
    }
    // More than shows: "Show more", or - shown all there is room for - how many
    // older ones are left out.
    if (!s_hv.more && n > HIST_ROWS_FIRST) {
        lv_obj_t *b = Kit::item(list);
        lv_obj_set_size(b, lv_pct(100), mm(k.H_ROW));
        lv_obj_add_event_cb(b, histMoreCb, LV_EVENT_CLICKED, nullptr);
        snprintf(buf, sizeof(buf), "Show more (%u)", (unsigned)(n - HIST_ROWS_FIRST));
        lv_obj_t *l = Kit::label(b, KS::KS_LABEL_TAG_ACCENT);
        lv_label_set_text(l, buf);
        lv_obj_center(l);
    } else {
        const uint32_t left = (uint32_t)(n - show) + H.eventsDropped();
        if (left) {
            snprintf(buf, sizeof(buf), "%lu older not shown", (unsigned long)left);
            lv_obj_t *l = Kit::label(list, KS::KS_LABEL_TAG_DIM);
            lv_label_set_text(l, buf);
        }
    }
}

} // namespace popup
