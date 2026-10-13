// History: the chart (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

// The chart itself: HA's points sampled at even steps across the span, each
// step holding the last value at or before it - the changes are steps in
// time, as HA's own graph draws a sensor's history.
void histDrawChart(lv_obj_t *body) {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const Entity    &e = *s_hv.e;
    const HaHistory &H = *s_hist;
    const uint32_t t0 = H.startSec(), t1 = H.endSec(), span = t1 > t0 ? t1 - t0 : 1;
    const uint16_t n = H.points();

    // Min and max over the span: the buckets' own when from statistics.
    float lo = INFINITY, hi = -INFINITY;
    for (uint16_t i = 0; i < n; i++) {
        const HistPoint &q = H.point(i);
        const float a = isnan(q.lo) ? q.v : q.lo, b = isnan(q.hi) ? q.v : q.hi;
        if (!isnan(a)) lo = LV_MIN(lo, cardDisplayNumber(e, a, s.tempUnit));
        if (!isnan(b)) hi = LV_MAX(hi, cardDisplayNumber(e, b, s.tempUnit));
    }
    const char *unit = cardDisplayUnit(e, s.tempUnit);
    // When the min and the max were: the first time each was reached.
    uint32_t tLo = t0, tHi = t0;
    {
        float l2 = INFINITY, h2 = -INFINITY;
        for (uint16_t i = 0; i < n; i++) {
            const HistPoint &q = H.point(i);
            const float a = isnan(q.lo) ? q.v : q.lo, b = isnan(q.hi) ? q.v : q.hi;
            if (!isnan(a) && a < l2) { l2 = a; tLo = LV_MAX(q.t, t0); }
            if (!isnan(b) && b > h2) { h2 = b; tHi = LV_MAX(q.t, t0); }
        }
    }

    // THE TOP LINE (histTopRow()), the same shape on every board: Min over Max
    // in the left corner, Now in the right, the chips or "Showing:" between.
    // The chips keep clear of the corners, narrowing to fit (the 4B).
    char buf[96], nowTxt[32];
    lv_obj_t *top = histTopRow(body);
    cardFormatValue(e, nowTxt, sizeof(nowTxt), true, s.tempUnit);
    snprintf(buf, sizeof(buf), "Now: %s%s", nowTxt, H.fromStatistics() ? "\n5-minute averages" : "");
    lv_obj_t *read = makeLabel(top, t.TAG, p.TEXT);
    lv_label_set_text(read, buf);
    lv_obj_set_style_text_align(read, LV_TEXT_ALIGN_RIGHT, 0);
    histCorner(read, LV_ALIGN_RIGHT_MID);
    // Min and Max, their numbers lined up: the words in one label, the numbers
    // in another beside it, right-aligned (owner, round 5).
    lv_obj_update_layout(top);
    const int32_t topW = lv_obj_get_width(top);
    int32_t leftEnd = 0;
    if (s_hv.minMax && lo <= hi) {
        lv_obj_t *words = makeLabel(top, t.TAG, p.TEXT);
        lv_label_set_text(words, "Min:\nMax:");
        histCorner(words, LV_ALIGN_LEFT_MID);
        snprintf(buf, sizeof(buf), "%.1f%s\n%.1f%s", (double)lo, unit, (double)hi, unit);
        lv_obj_t *nums = makeLabel(top, t.TAG, p.TEXT);
        lv_label_set_text(nums, buf);
        lv_obj_set_style_text_align(nums, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align_to(nums, words, LV_ALIGN_OUT_RIGHT_MID, mm(UI::shape().S3), 0);
        lv_obj_update_layout(top);
        leftEnd = lv_obj_get_width(words) + mm(UI::shape().S3) + lv_obj_get_width(nums);
    }
    const int32_t half = LV_MIN(topW / 2 - leftEnd, topW / 2 - lv_obj_get_width(read)) - mm(UI::shape().S_GUTTER);
    histTopMiddle(top, 2 * LV_MAX(half, 1));

    if (lo > hi) { histNote(body, "Nothing recorded in this span"); return; }

    lv_obj_t *area = plain(body);
    lv_obj_set_width(area, lv_pct(100));
    lv_obj_set_flex_grow(area, 1);
    // Half a line closer to the top line (owner, round 6): the strip for the
    // finger's reading, empty until a finger is down, made the gap look a line.
    lv_obj_set_style_margin_top(area, -lv_font_get_line_height(t.TAG) / 2, 0);
    // A FINGER READS THE CHART (owner, round 2): see histScrub().
    lv_obj_add_flag(area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(area, histChartCb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(area, histChartCb, LV_EVENT_PRESSING, nullptr);
    lv_obj_update_layout(area);
    const int32_t w  = lv_obj_get_width(area);
    const int32_t ah = lv_obj_get_height(area);

    // A strip above the plot for the finger's reading; one below
    // it for the times, when the Legend shows them.
    const int32_t lh   = lv_font_get_line_height(t.TAG);
    const bool    xLeg = s_hv.legend == LEGEND_X || s_hv.legend == LEGEND_BOTH;
    const bool    yLeg = s_hv.legend == LEGEND_Y || s_hv.legend == LEGEND_BOTH;
    const int32_t topH = lh + mm(UI::shape().S1);
    const int32_t botH = xLeg ? lh + mm(UI::shape().S1) : mm(UI::shape().S2);
    const int32_t plotH = LV_MAX(10, ah - topH - botH);

    // THE VALUE SCALE, IN ROUND STEPS (owner, round 3): as many lines as the
    // plot's height has room for - a label every ~2.4 lines of text - each step
    // 1, 2, 2.5 or 5 times a power of ten, the range widened to whole steps. So
    // 0-100 gets lines every 20 or 25, 40-60 every 5; a taller chart (the 1060,
    // no span buttons) gets more.
    const int32_t wantY = LV_CLAMP(2, plotH / (lh * 12 / 5), 8);
    float rng = hi - lo;
    if (rng < 0.001f) rng = 2.0f;
    const float step = histNice(rng / (float)wantY);
    float ymin = floorf(lo / step) * step, ymax = ceilf(hi / step) * step;
    if (ymax - ymin < step * 0.5f) { ymin -= step; ymax += step; }
    const int dec = step >= 1.0f ? 0 : step >= 0.1f ? 1 : 2;

    // The value labels' gutter, as wide as the widest of them.
    int32_t gutter = 0;
    if (yLeg) {
        for (float v = ymin; v <= ymax + step * 0.01f; v += step) {
            snprintf(buf, sizeof(buf), "%.*f%s", dec, (double)v, v + step > ymax + step * 0.01f ? unit : "");
            gutter = LV_MAX(gutter, textW(buf, t.TAG));
        }
        gutter += mm(UI::shape().S_GUTTER);
    }
    const int32_t plotW = LV_MAX(10, w - gutter);
    s_hv.plotL = gutter; s_hv.plotW = plotW;
    s_hv.plotTop = topH; s_hv.plotBot = topH + plotH;
    s_hv.area = area; s_hv.t0 = t0; s_hv.spanSec = span;

    // The grid, under the line: made before the chart, so drawn first.
    const lv_color_t gridCol = UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, 14));
    auto gridLine = [&](int32_t x, int32_t y, int32_t gw, int32_t gh) {
        lv_obj_t *g = plain(area);
        lv_obj_set_pos(g, x, y);
        lv_obj_set_size(g, gw, gh);
        lv_obj_set_style_bg_color(g, gridCol, 0);
        lv_obj_set_style_bg_opa(g, LV_OPA_COVER, 0);
    };
    int32_t lastLblY = INT32_MIN;
    for (float v = ymax; v >= ymin - step * 0.01f; v -= step) {
        const int32_t y = topH + (int32_t)lroundf((ymax - v) / (ymax - ymin) * (float)(plotH - 1));
        gridLine(gutter, y, plotW, 1);
        if (!yLeg) continue;
        snprintf(buf, sizeof(buf), "%.*f%s", dec, (double)v, v >= ymax - step * 0.01f ? unit : "");
        const int32_t ly = LV_CLAMP(0, y - lh / 2, ah - lh);
        if (ly < lastLblY + lh) continue;   // never two on top of each other
        lv_obj_t *yl = makeLabel(area, t.TAG, p.TEXT_DIM);
        lv_label_set_text(yl, buf);
        lv_obj_set_pos(yl, gutter - mm(UI::shape().S_GUTTER) - textW(buf, t.TAG), ly);
        lastLblY = ly;
    }

    // THE TIME SCALE ON THE CLOCK (owner, round 3): ticks on whole hours, half
    // hours or minutes, as many as fit their labels across the plot - every 2
    // or 3 hours on a day, 10 or 15 minutes on an hour. A faint line at each.
    {
        static const uint32_t IV[] = { 300, 600, 900, 1800, 3600, 7200, 10800, 14400, 21600, 43200, 86400 };
        const int32_t lblW = textW("12 PM", t.TAG) + mm(UI::shape().S8);
        const uint32_t fit = (uint32_t)LV_MAX(2, plotW / lblW);
        uint32_t iv = IV[sizeof(IV) / sizeof(IV[0]) - 1];
        for (uint32_t c : IV) if (span / c <= fit) { iv = c; break; }
        // The local clock's offset from UTC, so ticks land on local hours.
        const time_t tt = (time_t)t0;
        struct tm lt;
        localtime_r(&tt, &lt);
        const int32_t local = lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
        int32_t off = local - (int32_t)(t0 % 86400);
        if (off > 50400) off -= 86400;
        if (off < -50400) off += 86400;
        const int64_t first = (((int64_t)t0 + off + iv - 1) / iv) * iv - off;
        int32_t lastRight = INT32_MIN;
        for (int64_t tk = first; tk <= (int64_t)t1; tk += iv) {
            const int32_t x = gutter + (int32_t)((uint64_t)(tk - t0) * (uint32_t)(plotW - 1) / span);
            gridLine(x, topH, 1, plotH);
            if (!xLeg) continue;
            const time_t tl = (time_t)tk;
            struct tm a;
            localtime_r(&tl, &a);
            int hr = a.tm_hour % 12;
            if (!hr) hr = 12;
            if (iv >= 3600) snprintf(buf, sizeof(buf), "%d %s", hr, a.tm_hour < 12 ? "AM" : "PM");
            else            snprintf(buf, sizeof(buf), "%d:%02d", hr, a.tm_min);
            const int32_t bw = textW(buf, t.TAG);
            const int32_t bx = LV_CLAMP(0, x - bw / 2, w - bw);
            if (bx < lastRight + mm(UI::shape().S_GUTTER)) continue;
            lv_obj_t *xl = makeLabel(area, t.TAG, p.TEXT_DIM);
            lv_label_set_text(xl, buf);
            lv_obj_set_pos(xl, bx, topH + plotH + mm(UI::shape().S1));
            lastRight = bx + bw;
        }
    }

    lv_obj_t *ch = lv_chart_create(area);
    lv_obj_remove_style_all(ch);
    lv_obj_set_size(ch, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(ch, 0, 0);
    lv_obj_set_style_pad_left(ch, gutter, 0);
    lv_obj_set_style_pad_top(ch, topH, 0);
    lv_obj_set_style_pad_bottom(ch, ah - topH - plotH, 0);
    lv_obj_set_style_line_width(ch, LV_MAX(2, mm(UI::shape().LINE_PLOT)), LV_PART_ITEMS);
    lv_obj_set_style_width (ch, 0, LV_PART_INDICATOR);   // no dots on the points
    lv_obj_set_style_height(ch, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(ch, LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_type(ch, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(ch, 0, 0);   // the grid above is ours
    lv_chart_set_axis_range(ch, LV_CHART_AXIS_PRIMARY_Y, (int32_t)lroundf(ymin * 10), (int32_t)lroundf(ymax * 10));
    if (s_hv.shade) {   // the fill under the line (Shading: under line, round 5)
        lv_obj_add_flag(ch, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
        lv_obj_add_event_cb(ch, histFillCb, LV_EVENT_DRAW_TASK_ADDED, nullptr);
    }

    const uint16_t m = (uint16_t)LV_CLAMP(48, plotW / 4, 240);
    lv_chart_set_point_count(ch, m);
    lv_chart_series_t *ser = lv_chart_add_series(ch, UI::c(p.ACCENT), LV_CHART_AXIS_PRIMARY_Y);
    int32_t *ys = lv_chart_get_series_y_array(ch, ser);
    uint16_t k = 0;
    float last = NAN;
    for (uint16_t i = 0; i < m; i++) {
        const uint32_t at = t0 + (uint32_t)(((uint64_t)span * (2 * i + 1)) / (2 * m));
        bool seen = (k > 0);
        while (k < n && H.point(k).t <= at) { last = H.point(k).v; k++; seen = true; }
        ys[i] = (!seen || isnan(last)) ? LV_CHART_POINT_NONE
                                       : (int32_t)lroundf(cardDisplayNumber(e, last, s.tempUnit) * 10);
    }
    lv_chart_refresh(ch);

    // Kept for the finger (histScrub()); the plot's place was kept above.
    s_hv.ys = ys; s_hv.m = m;
    s_hv.scrubLine = s_hv.scrubLbl = nullptr;

    // MIN AND MAX WHERE THEY HAPPENED (owner, rounds 2, 4 and 5): a faint dotted
    // line down the plot at each, its time IN THE PLOT - the max just above
    // the line where its label stands, the min just below, each moved to the
    // other side if there is no room, so a label never sits on the line.
    if (s_hv.minMax) {
        static lv_point_precise_t ptsLo[2], ptsHi[2];
        const uint32_t faint = UI::mix(p.SURFACE_ALT, p.TEXT, 35);
        const int32_t xLo = histX(tLo), xHi = histX(tHi);
        histVLine(area, ptsLo, xLo, faint, LV_MAX(1, mm(UI::shape().LINE_FINE)));
        histVLine(area, ptsHi, xHi, faint, LV_MAX(1, mm(UI::shape().LINE_FINE)));
        const int32_t pad = mm(UI::shape().S1), gap = mm(UI::shape().S2);
        // Where the line runs under columns x0..x1: its highest and lowest y.
        auto lineSpan = [&](int32_t x0, int32_t x1, int32_t &yTop, int32_t &yBot) {
            const int32_t pw = LV_MAX(2, plotW);
            int32_t i0 = (x0 - gutter) * (m - 1) / (pw - 1) - 1, i1 = (x1 - gutter) * (m - 1) / (pw - 1) + 1;
            i0 = LV_CLAMP(0, i0, m - 1);
            i1 = LV_CLAMP(0, i1, m - 1);
            yTop = INT32_MAX; yBot = INT32_MIN;
            for (int32_t i = i0; i <= i1; i++) {
                if (ys[i] == LV_CHART_POINT_NONE) continue;
                const int32_t y = topH + (int32_t)lroundf((ymax - ys[i] / 10.0f) / (ymax - ymin) * (float)(plotH - 1));
                yTop = LV_MIN(yTop, y);
                yBot = LV_MAX(yBot, y);
            }
            if (yTop > yBot) { yTop = yBot = topH + plotH / 2; }
        };
        auto place = [&](const char *txt, int32_t x, bool above) {
            lv_obj_t *l = makeLabel(area, t.TAG, p.TEXT_DIM);
            lv_label_set_text(l, txt);
            // A backing in the surface's colour: the grid and its dotted line stop at its edge.
            lv_obj_set_style_bg_color(l, UI::c(p.SURFACE_ALT), 0);
            lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_hor(l, pad, 0);
            const int32_t lw = textW(txt, t.TAG) + 2 * pad + 2;
            const int32_t lx = LV_CLAMP(gutter, x - lw / 2, w - lw);
            int32_t yTop, yBot;
            lineSpan(lx, lx + lw, yTop, yBot);
            const int32_t top = topH, bot = topH + plotH - lh;
            int32_t y = above ? yTop - gap - lh : yBot + gap;
            if (y < top || y > bot) y = above ? yBot + gap : yTop - gap - lh;   // no room: the other side
            lv_obj_set_pos(l, lx, LV_CLAMP(top, y, bot));
        };
        // Which, and when (owner, rounds 5 and 6): "Max: 3:53 PM" - the values
        // are in the top line.
        char a[32], b[32], c[16];
        tfClock(c, sizeof(c), (time_t)tHi);
        snprintf(a, sizeof(a), "Max: %s", c);
        tfClock(c, sizeof(c), (time_t)tLo);
        snprintf(b, sizeof(b), "Min: %s", c);
        place(a, xHi, true);
        place(b, xLo, false);
    }
}

// THE FILL UNDER THE LINE (owner, round 4): the accent, faint at the line and
// gone at the plot's foot. Drawn as LVGL's own area-chart example does
// (lv_example_chart_5): for each piece of the line, a triangle down to its
// lower end and a rectangle from there to the foot, each a vertical gradient.
void histFillCb(lv_event_t *ev) {
    lv_draw_task_t *task = lv_event_get_draw_task(ev);
    lv_draw_dsc_base_t *base = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(task);
    if (base->part != LV_PART_ITEMS || lv_draw_task_get_type(task) != LV_DRAW_TASK_TYPE_LINE) return;
    lv_obj_t *obj = lv_event_get_target_obj(ev);
    lv_area_t c;
    lv_obj_get_coords(obj, &c);
    const int32_t top = c.y1 + lv_obj_get_style_pad_top(obj, LV_PART_MAIN);
    const int32_t foot = c.y2 - lv_obj_get_style_pad_bottom(obj, LV_PART_MAIN);
    const int32_t h = LV_MAX(1, foot - top);
    constexpr int32_t OPA_TOP = 90;   // ~35% at the plot's top, less lower down
    auto opaAt = [&](int32_t y) { return (lv_opa_t)LV_CLAMP(0, OPA_TOP * (foot - y) / h, 255); };
    const lv_color_t col = UI::c(UI::pal().ACCENT);
    lv_draw_line_dsc_t *ld = lv_draw_task_get_line_dsc(task);
    for (int32_t i = 0; i + 1 < (int32_t)ld->point_cnt; i++) {
        const lv_point_precise_t p1 = ld->points[i], p2 = ld->points[i + 1];
        if (p1.x == LV_DRAW_LINE_POINT_NONE || p1.y == LV_DRAW_LINE_POINT_NONE ||
            p2.x == LV_DRAW_LINE_POINT_NONE || p2.y == LV_DRAW_LINE_POINT_NONE) continue;
        const int32_t yHi = (int32_t)LV_MIN(p1.y, p2.y), yLo = (int32_t)LV_MAX(p1.y, p2.y);
        if (yLo > yHi) {
            lv_draw_triangle_dsc_t tri;
            lv_draw_triangle_dsc_init(&tri);
            tri.p[0] = p1;
            tri.p[1] = p2;
            tri.p[2].x = p1.y < p2.y ? p1.x : p2.x;
            tri.p[2].y = yLo;
            tri.grad.dir = LV_GRAD_DIR_VER;
            tri.grad.stops[0].color = col; tri.grad.stops[0].opa = opaAt(yHi); tri.grad.stops[0].frac = 0;
            tri.grad.stops[1].color = col; tri.grad.stops[1].opa = opaAt(yLo); tri.grad.stops[1].frac = 255;
            lv_draw_triangle(base->layer, &tri);
        }
        lv_draw_rect_dsc_t rd;
        lv_draw_rect_dsc_init(&rd);
        rd.bg_grad.dir = LV_GRAD_DIR_VER;
        rd.bg_grad.stops[0].color = col; rd.bg_grad.stops[0].opa = opaAt(yLo); rd.bg_grad.stops[0].frac = 0;
        rd.bg_grad.stops[1].color = col; rd.bg_grad.stops[1].opa = 0;           rd.bg_grad.stops[1].frac = 255;
        lv_area_t a;
        a.x1 = (int32_t)p1.x;
        a.x2 = (int32_t)p2.x - 1;
        a.y1 = yLo;
        a.y2 = foot;
        if (a.x2 >= a.x1 && a.y2 > a.y1) lv_draw_rect(base->layer, &rd, &a);
    }
}

// --- The finger on the chart (owner, round 2) --------------------------------
// Press anywhere on it and a dotted line stands at that time, with the time and
// the value in the strip above the plot - never under the finger. Slide to move
// it; it stays where the finger lifts, until the span changes.

// A time to the plot's x (in the chart area, past the value labels' gutter).
int32_t histX(uint32_t t) {
    if (!s_hv.area) return 0;
    const uint32_t d = t > s_hv.t0 ? t - s_hv.t0 : 0;
    const int32_t pw = LV_MAX(2, s_hv.plotW);
    return s_hv.plotL + LV_CLAMP(0, (int32_t)((uint64_t)d * (uint32_t)(pw - 1) / LV_MAX(1u, s_hv.spanSec)), pw - 1);
}

// A round step near `raw`: 1, 2, 2.5 or 5 times a power of ten.
float histNice(float raw) {
    if (raw <= 0.0f) return 1.0f;
    const float e = powf(10.0f, floorf(log10f(raw)));
    const float f = raw / e;
    return (f <= 1.0f ? 1.0f : f <= 2.0f ? 2.0f : f <= 2.5f ? 2.5f : f <= 5.0f ? 5.0f : 10.0f) * e;
}

// A dotted line down the plot at x. `pts` must outlive the line (LVGL keeps the
// pointer): each line its own static pair.
lv_obj_t *histVLine(lv_obj_t *parent, lv_point_precise_t *pts, int32_t x, uint32_t col, int32_t width) {
    const int32_t hgt = LV_MAX(1, s_hv.plotBot - s_hv.plotTop);
    pts[0] = { width / 2, 0 };
    pts[1] = { width / 2, hgt };
    lv_obj_t *l = lv_line_create(parent);
    lv_obj_remove_style_all(l);
    lv_line_set_points(l, pts, 2);
    lv_obj_set_size(l, width, hgt);
    lv_obj_set_pos(l, x - width / 2, s_hv.plotTop);
    lv_obj_set_style_line_width     (l, width, 0);
    lv_obj_set_style_line_color     (l, UI::c(col), 0);
    lv_obj_set_style_line_dash_width(l, LV_MAX(2, mm(UI::shape().DASH)), 0);
    lv_obj_set_style_line_dash_gap  (l, LV_MAX(2, mm(UI::shape().DASH)), 0);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

void histScrub(int32_t x) {
    if (!s_hv.area || !s_hv.ys || s_hv.m < 2) return;
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const int32_t w  = lv_obj_get_width(s_hv.area);
    const int32_t pw = LV_MAX(2, s_hv.plotW);
    const int32_t xp = LV_CLAMP(0, x - s_hv.plotL, pw - 1);
    const uint16_t i = (uint16_t)LV_CLAMP(0, (xp * (s_hv.m - 1) + (pw - 1) / 2) / (pw - 1), s_hv.m - 1);
    const int32_t xi = s_hv.plotL + (int32_t)i * (pw - 1) / (s_hv.m - 1);   // where the chart drew point i
    const uint32_t at = s_hv.t0 + (uint32_t)(((uint64_t)s_hv.spanSec * (2 * i + 1)) / (2 * s_hv.m));

    static lv_point_precise_t ptsScrub[2];
    if (!s_hv.scrubLine) {
        s_hv.scrubLine = histVLine(s_hv.area, ptsScrub, xi, UI::mix(p.SURFACE_ALT, p.TEXT, 70), LV_MAX(2, mm(UI::shape().LINE_MID)));
        s_hv.scrubLbl  = makeLabel(s_hv.area, t.TAG, p.TEXT);
        // Opaque, over the min and max labels it may cover.
        lv_obj_set_style_bg_color(s_hv.scrubLbl, UI::c(p.SURFACE_ALT), 0);
        lv_obj_set_style_bg_opa  (s_hv.scrubLbl, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor (s_hv.scrubLbl, mm(UI::shape().S2), 0);
    } else {
        lv_obj_set_x(s_hv.scrubLine, xi - lv_obj_get_width(s_hv.scrubLine) / 2);
    }
    char tm[32], buf[64];
    tfWhen(tm, sizeof(tm), (time_t)at, time(nullptr), TfWhen::WHEN_FULL);
    if (s_hv.ys[i] == LV_CHART_POINT_NONE) snprintf(buf, sizeof(buf), "%s  no value", tm);
    else snprintf(buf, sizeof(buf), "%s  %.1f%s", tm, s_hv.ys[i] / 10.0, cardDisplayUnit(*s_hv.e, s.tempUnit));
    lv_label_set_text(s_hv.scrubLbl, buf);
    const int32_t lw = textW(buf, t.TAG) + 2 * mm(UI::shape().S2) + 2;
    lv_obj_set_pos(s_hv.scrubLbl, LV_CLAMP(0, xi - lw / 2, w - lw), 0);
}

void histChartCb(lv_event_t *ev) {
    (void)ev;
    lv_indev_t *in = lv_indev_active();
    if (!in || !s_hv.area) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    lv_area_t a;
    lv_obj_get_coords(s_hv.area, &a);
    histScrub(pt.x - a.x1);
}

} // namespace popup
