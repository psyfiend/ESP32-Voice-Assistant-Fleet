// GET /popup and what the window's frames cost (2.12 P1, split from CardPopup.cpp). Debug builds
// only (-D DEBUG_POPUP).
#include "PopupState.h"
#include "UI/AboutSheet.h"   // the Sheet's second client (2.12 P4)
#include "UI/TweaksSheet.h"  // and UI Tweaks (2.8 round 3)
#include "Cards/PageArranger.h"   // the arranger's last run, in the card list (2.11 round 2)
#include "UI/SlotPanels.h"        // restore=-2: the page panel's View hidden cards (2.11, K96)
#include "UI/HiddenSheet.h"       // restore=N: the hidden cards' sheet (round 6)

namespace popup {

#ifdef DEBUG_POPUP
char     s_dbgLine[120];
uint32_t s_dbgPressMs   = 0;   // when the long press fired
uint32_t s_dbgRenderT0  = 0;
uint32_t s_dbgFrameMs   = 0;   // how long the next frame took to draw
uint32_t s_dbgGlassMs   = 0;   // long press to that frame drawn
bool     s_dbgWantFrame = false;

// What that frame redraws: if the window's own frame costs more than the
// window, these say why (round 6: ~77 ms on Midnight where ~25 was expected).
constexpr uint8_t DBG_AREAS = 4;
lv_area_t s_dbgArea[DBG_AREAS];
uint8_t   s_dbgNArea = 0, s_dbgAllAreas = 0;

// The frame after a window appears or goes. -D DEBUG_FRAMES cannot see it: it
// only prints bursts of three frames or more, and closing is one.
void dbgRenderStart(lv_event_t *e) {
    if (!s_dbgWantFrame) return;
    s_dbgRenderT0 = millis();
    lv_display_t *d = (lv_display_t *)lv_event_get_current_target(e);
    s_dbgNArea = s_dbgAllAreas = 0;
    for (uint32_t i = 0; d && i < d->inv_p; i++) {
        if (d->inv_area_joined[i]) continue;
        s_dbgAllAreas++;
        if (s_dbgNArea < DBG_AREAS) s_dbgArea[s_dbgNArea++] = d->inv_areas[i];
    }
}
void dbgRenderReady(lv_event_t *e) {
    (void)e;
    if (!s_dbgWantFrame || !s_dbgRenderT0) return;
    s_dbgFrameMs   = millis() - s_dbgRenderT0;
    s_dbgGlassMs   = s_dbgPressMs ? millis() - s_dbgPressMs : 0;
    s_dbgWantFrame = false;
    s_dbgRenderT0  = 0;
}

void dbgProbe();   // below, where the window is known

void dbgPrint(lv_timer_t *t) {
    (void)t;
    if (s_dbgGlassMs) DBG_POPUP("%s; its frame drew in %lu ms; long press to glass %lu ms\n", s_dbgLine,
                                (unsigned long)s_dbgFrameMs, (unsigned long)s_dbgGlassMs);
    else              DBG_POPUP("%s; its frame drew in %lu ms\n", s_dbgLine, (unsigned long)s_dbgFrameMs);
    for (uint8_t i = 0; i < s_dbgNArea; i++) {
        const lv_area_t &a = s_dbgArea[i];
        DBG_POPUP("  redrew %ld,%ld..%ld,%ld (%ldx%ld)\n", (long)a.x1, (long)a.y1, (long)a.x2, (long)a.y2,
                  (long)lv_area_get_width(&a), (long)lv_area_get_height(&a));
    }
    if (s_dbgAllAreas > s_dbgNArea) DBG_POPUP("  ...%u areas in all\n", (unsigned)s_dbgAllAreas);
    dbgProbe();
    s_dbgPressMs = s_dbgGlassMs = 0;
}

// Printed 300 ms later, not now: a line written in the middle of the frames
// it describes blocks on the UART and becomes one of them (LESSONS, "A debug
// print is not free").
void dbgLater(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_dbgLine, sizeof(s_dbgLine), fmt, ap);
    va_end(ap);
    s_dbgWantFrame = true;
    s_dbgFrameMs   = 0;
    lv_timer_t *t = lv_timer_create(dbgPrint, 300, nullptr);
    lv_timer_set_repeat_count(t, 1);
}
#endif

#ifdef DEBUG_POPUP
// What LVGL's own pool holds right now (interview G1: a window, open).
void dbgMem(char *buf, size_t cap) {
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    snprintf(buf, cap, "lv_mem %lu KB used of %lu, biggest free %lu KB, frag %u%%",
             (unsigned long)((m.total_size - m.free_size) / 1024), (unsigned long)(m.total_size / 1024),
             (unsigned long)(m.free_biggest_size / 1024), (unsigned)m.frag_pct);
}

// Is the window what LVGL finds covering a strip across its middle? If not,
// every redraw inside it draws the page underneath first.
void dbgProbe() {
    if (!s.win) return;
    const lv_area_t &W = s.winRect;
    const int32_t mid = (W.y1 + W.y2) / 2;
    const lv_area_t strip = { W.x1, mid, W.x2, mid + 40 };
    lv_obj_t *top = lv_refr_get_top_obj(&strip, lv_screen_active());
    DBG_POPUP("  a strip across the window is covered by %s; window is child %ld of %lu\n",
              top == s.win ? "the window" : top ? "something else" : "nothing",
              (long)lv_obj_get_index(s.win), (unsigned long)lv_obj_get_child_count(lv_screen_active()));
}
#endif
} // namespace popup

using namespace popup;

#ifdef DEBUG_POPUP
// ---------------------------------------------------------------------------
// GET /popup (CardPopup.h): open, drive and measure a window from a PC. The
// handler runs on the HTTP server's task and never touches LVGL: it posts the
// request and waits; debugService(), an lv_timer on the LVGL thread, does the
// work and writes the reply.
// ---------------------------------------------------------------------------
namespace popup {
enum DbgReqState : int { DREQ_IDLE, DREQ_CLAIMED, DREQ_PENDING, DREQ_CLOSING, DREQ_DONE };
std::atomic<int> s_dreq{DREQ_IDLE};
struct DbgReq { int card = -1; int deck = -1; int ctl = -1; int set = -1; int view = -1; int pause = -1;
                int member = -1; int scene = -1; int look = -1; int show = -1; int scheme = -1;
                int tap = -1; int groups = -1; int clabel = -1; int span = -1; int row = -1; int x = -1;
                int dur = -1; int chips = -1; int addCard = -1; int removeCard = -1;
                int tree = 0;     // 1: the open window's objects; 2 (tree=page): the whole screen
                int sparks = -1;  // the interference from the next window: 0 now and then, 1 once, 2 off
                int about = -1;   // 2.12 P4: the About sheet, 1 open, 0 close
                int drawer = -1;  // #97: the system panel, 1 open, 0 close (GUIManager's hook)
                char slot[8] = {};// 2.8: a header slot's panel by its kind's name ("clock"), "-1" closes (GUIManager's hook)
                int glyphs = -1;  // 2.8: the header's glyph style, 0 bare, 1 square, 2 circle
                int pageHdr = -1; // 2.8: this page's header, 0 hidden, 1 shown (saved)
                int alarm = -2;   // 2.8 round 2: a pretend next alarm, HHMM (24 h); -1 clears (Alarms.h)
                int alarmIn = -1; // 2.8 round 3: a pretend next alarm N minutes from now
                int tweaks = -1;  // 2.8 round 3: UI Tweaks, 1 open (GUIManager's hook), 0 close
                int sensorErr = -1; // 2.8 round 3 (T10): the header's sensor shown failed, 1; as it is, 0
                int cardState = -2; // 2.8 round 4: the cards' active state 0-3, not saved; -1 the owner's
                int32_t active = -1;// 2.8 round 6: the active colour, RRGGBB, not saved; 0 the scheme's own
                int kbd = -1, kbMode = -1, kbPress = -1;   // 2.11 (K95): the keyboard
                int restore = -1;                          // 2.11 (K96): the hidden cards' Restore N; -2 View hidden cards
                int addTo = -1, mv = 0;                    // 2.11 (K91): Add to cluster N, copy or move
                int addCards = -1;                         // ... a cluster's Add cards: the ticked, as a bit mask
                int join = -1;                             // K92 rule 9: 1 opens "which group?", 2+N adds in group N
                char kbType[24] = {};                      // ... words typed on it ('+' a space)
                bool close = false, power = false, more = false, hist = false, mem = false; };
bool (*s_drawerHook)(bool open) = nullptr;
const char *(*s_headerHook)(const char *slot, int glyphs, int pageHdr, int alarm, int alarmIn) = nullptr;
DbgReq s_dreqArgs;
char   s_dreqOut[2048];
size_t s_dreqLen = 0;

void dbgOut(const char *fmt, ...) {
    if (s_dreqLen >= sizeof(s_dreqOut) - 1) return;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(s_dreqOut + s_dreqLen, sizeof(s_dreqOut) - s_dreqLen, fmt, ap);
    va_end(ap);
    if (n > 0) s_dreqLen = LV_MIN(sizeof(s_dreqOut) - 1, s_dreqLen + (size_t)n);
}

// The cards on the screen, in tree order - the surfaces CARD_SURFACE_FLAG marks;
// a cluster's mini tiles right after the cluster (2.11): they are inside it.
int dbgCards(lv_obj_t *o, lv_obj_t **out, int n, int cap) {
    const uint32_t cnt = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < cnt && n < cap; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) out[n++] = c;
        n = dbgCards(c, out, n, cap);
    }
    return n;
}

esp_err_t handlePopup(httpd_req_t *req) {
    DbgReq a;
    char q[64], v[8];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        if (httpd_query_key_value(q, "card",  v, sizeof(v)) == ESP_OK) a.card  = atoi(v);
        if (httpd_query_key_value(q, "deck",  v, sizeof(v)) == ESP_OK) a.deck  = atoi(v);
        if (httpd_query_key_value(q, "ctl",   v, sizeof(v)) == ESP_OK) a.ctl   = atoi(v);
        if (httpd_query_key_value(q, "set",   v, sizeof(v)) == ESP_OK) a.set   = atoi(v);
        if (httpd_query_key_value(q, "view",  v, sizeof(v)) == ESP_OK) a.view  = atoi(v);
        if (httpd_query_key_value(q, "power", v, sizeof(v)) == ESP_OK) a.power = true;
        if (httpd_query_key_value(q, "pause", v, sizeof(v)) == ESP_OK) a.pause = atoi(v);
        if (httpd_query_key_value(q, "member", v, sizeof(v)) == ESP_OK) a.member = atoi(v);
        if (httpd_query_key_value(q, "scene", v, sizeof(v)) == ESP_OK) a.scene = atoi(v);
        if (httpd_query_key_value(q, "look",  v, sizeof(v)) == ESP_OK) a.look  = atoi(v);
        if (httpd_query_key_value(q, "show",  v, sizeof(v)) == ESP_OK) a.show  = atoi(v);
        if (httpd_query_key_value(q, "scheme", v, sizeof(v)) == ESP_OK) a.scheme = atoi(v);
        if (httpd_query_key_value(q, "close", v, sizeof(v)) == ESP_OK) a.close = atoi(v) != 0;
        if (httpd_query_key_value(q, "tap", v, sizeof(v)) == ESP_OK) a.tap = atoi(v);
        if (httpd_query_key_value(q, "groups", v, sizeof(v)) == ESP_OK) a.groups = atoi(v);
        if (httpd_query_key_value(q, "clabel", v, sizeof(v)) == ESP_OK) a.clabel = atoi(v);
        if (httpd_query_key_value(q, "span", v, sizeof(v)) == ESP_OK) a.span = atoi(v);
        if (httpd_query_key_value(q, "more", v, sizeof(v)) == ESP_OK) a.more = atoi(v) != 0;
        if (httpd_query_key_value(q, "hist", v, sizeof(v)) == ESP_OK) a.hist = true;
        if (httpd_query_key_value(q, "row", v, sizeof(v)) == ESP_OK) a.row = atoi(v);
        if (httpd_query_key_value(q, "x", v, sizeof(v)) == ESP_OK) a.x = atoi(v);
        if (httpd_query_key_value(q, "dur", v, sizeof(v)) == ESP_OK) a.dur = atoi(v);
        if (httpd_query_key_value(q, "addcard", v, sizeof(v)) == ESP_OK) a.addCard = atoi(v);
        if (httpd_query_key_value(q, "removecard", v, sizeof(v)) == ESP_OK) a.removeCard = atoi(v);
        if (httpd_query_key_value(q, "chips", v, sizeof(v)) == ESP_OK) a.chips = atoi(v);
        if (httpd_query_key_value(q, "tree", v, sizeof(v)) == ESP_OK) a.tree = strcmp(v, "page") ? 1 : 2;
        if (httpd_query_key_value(q, "mem", v, sizeof(v)) == ESP_OK) a.mem = true;
        if (httpd_query_key_value(q, "sparks", v, sizeof(v)) == ESP_OK) a.sparks = atoi(v);
        if (httpd_query_key_value(q, "about", v, sizeof(v)) == ESP_OK) a.about = atoi(v);
        if (httpd_query_key_value(q, "drawer", v, sizeof(v)) == ESP_OK) a.drawer = atoi(v);
        if (httpd_query_key_value(q, "slot", v, sizeof(v)) == ESP_OK) snprintf(a.slot, sizeof(a.slot), "%s", v);
        // 2.11 round 9, through the header hook as slot= is: the development Reset
        // (dash=1 its dialog, 2 done) and the system panel's grid (grid=a Auto,
        // cu / cd Col + / -, ru / rd Row + / -).
        if (httpd_query_key_value(q, "dash", v, sizeof(v)) == ESP_OK) snprintf(a.slot, sizeof(a.slot), "dash%.1s", v);
        if (httpd_query_key_value(q, "grid", v, sizeof(v)) == ESP_OK) snprintf(a.slot, sizeof(a.slot), "grid%.2s", v);
        if (httpd_query_key_value(q, "glyphs", v, sizeof(v)) == ESP_OK) a.glyphs = atoi(v);
        if (httpd_query_key_value(q, "pagehdr", v, sizeof(v)) == ESP_OK) a.pageHdr = atoi(v);
        if (httpd_query_key_value(q, "alarm", v, sizeof(v)) == ESP_OK) a.alarm = atoi(v);
        if (httpd_query_key_value(q, "alarmin", v, sizeof(v)) == ESP_OK) a.alarmIn = atoi(v);
        if (httpd_query_key_value(q, "tweaks", v, sizeof(v)) == ESP_OK) a.tweaks = atoi(v);
        if (httpd_query_key_value(q, "sensorerr", v, sizeof(v)) == ESP_OK) a.sensorErr = atoi(v);
        if (httpd_query_key_value(q, "cardstate", v, sizeof(v)) == ESP_OK) a.cardState = atoi(v);
        if (httpd_query_key_value(q, "kbd", v, sizeof(v)) == ESP_OK) a.kbd = atoi(v);
        if (httpd_query_key_value(q, "restore", v, sizeof(v)) == ESP_OK) a.restore = atoi(v);
        if (httpd_query_key_value(q, "addto", v, sizeof(v)) == ESP_OK) a.addTo = atoi(v);
        if (httpd_query_key_value(q, "mv", v, sizeof(v)) == ESP_OK) a.mv = atoi(v);
        if (httpd_query_key_value(q, "addcards", v, sizeof(v)) == ESP_OK) a.addCards = atoi(v);
        if (httpd_query_key_value(q, "join", v, sizeof(v)) == ESP_OK) a.join = atoi(v);
        if (httpd_query_key_value(q, "kbmode", v, sizeof(v)) == ESP_OK) a.kbMode = atoi(v);
        if (httpd_query_key_value(q, "kbpress", v, sizeof(v)) == ESP_OK) a.kbPress = atoi(v);
        if (httpd_query_key_value(q, "kbtype", a.kbType, sizeof(a.kbType)) == ESP_OK)
            for (char *p = a.kbType; *p; p++) if (*p == '+') *p = ' ';
        if (httpd_query_key_value(q, "active", v, sizeof(v)) == ESP_OK)
            a.active = (int32_t)(strtoul(v[0] == '#' ? v + 1 : v, nullptr, 16) & 0xFFFFFF);
        // rows=N: test rows for SETTINGS, from the next window built (a byte
        // the LVGL thread reads; nothing else to hand over).
        if (httpd_query_key_value(q, "rows", v, sizeof(v)) == ESP_OK) {
            s_dbgRows = (uint8_t)LV_CLAMP(0, atoi(v), (int)DECK_ROWS_MAX);
            char out[96];
            const int n = snprintf(out, sizeof(out), "SETTINGS gets %u test row(s) from the next window opened\n",
                                   (unsigned)s_dbgRows);
            httpd_resp_set_type(req, "text/plain");
            return httpd_resp_send(req, out, n);
        }
    }
    int expected = DREQ_IDLE;
    if (!s_dreq.compare_exchange_strong(expected, DREQ_CLAIMED)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Busy; try again.\n");
    }
    s_dreqArgs = a;                 // written before the LVGL thread can see it
    s_dreq.store(DREQ_PENDING);
    for (int i = 0; i < 300 && s_dreq.load() != DREQ_DONE; i++) vTaskDelay(pdMS_TO_TICKS(10));
    expected = DREQ_PENDING;
    if (s_dreq.compare_exchange_strong(expected, DREQ_IDLE)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "The UI thread did not pick it up within 3 s.\n");
    }
    while (s_dreq.load() != DREQ_DONE) vTaskDelay(pdMS_TO_TICKS(10));   // a close always ends
    httpd_resp_set_type(req, "text/plain");
    esp_err_t r;
    if (s_tree) {   // tree=: the dump, written by the LVGL thread before DONE
        r = httpd_resp_send(req, s_tree, (ssize_t)s_treeLen);
        heap_caps_free(s_tree);
        s_tree = nullptr;
        s_treeLen = 0;
    } else {
        r = httpd_resp_send(req, s_dreqOut, (ssize_t)s_dreqLen);
    }
    s_dreq.store(DREQ_IDLE);
    return r;
}
} // namespace popup

void CardPopup::debugService(lv_timer_t *t) {
    (void)t;
    char mem[96];
    const int st = s_dreq.load();
    if (st == DREQ_CLOSING) {
        if (isOpen()) return;   // the sheet closes from lv_async_call (Kit::sheetClose())
        dbgMem(mem, sizeof(mem));
        dbgOut("closed; %s\n", mem);
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (st != DREQ_PENDING) return;
    const DbgReq a = s_dreqArgs;
    s_dreqLen = 0;
    s_dreqOut[0] = '\0';

    if (a.close) {
        if (!isOpen()) { dbgOut("no window open\n"); s_dreq.store(DREQ_DONE); return; }
        close();
        s_dreq.store(DREQ_CLOSING);
        return;
    }
    // 2.12 P4: the Sheet's second client, "About this board" (UI/AboutSheet.h).
    if (a.about >= 0) {
        if (a.about == 0)          { AboutSheet::close(); dbgOut("about: closing\n"); }
        else if (AboutSheet::open()) dbgOut("about: open\n");
        else                         dbgOut("a sheet is already showing; close it first\n");
        s_dreq.store(DREQ_DONE);
        return;
    }
    // 2.8 round 3: UI Tweaks, a Sheet client of GUIManager's (UI/TweaksSheet.h);
    // the header's sensor shown failed (T10). Both through the header hook.
    if (a.tweaks >= 0 || a.sensorErr >= 0 || a.cardState > -2 || a.active >= 0) {
        char cs[12];
        if (a.active >= 0) snprintf(cs, sizeof(cs), "act%06lX", (unsigned long)a.active);
        else               snprintf(cs, sizeof(cs), "cs%d", a.cardState);
        if (a.tweaks == 0) { TweaksSheet::close(); dbgOut("tweaks: closing\n"); }
        else {
            const char *what = a.tweaks > 0 ? "tweaks" : (a.cardState > -2 || a.active >= 0) ? cs
                             : a.sensorErr ? "sensorerr" : "sensorok";
            dbgOut("%s\n", s_headerHook ? s_headerHook(what, -1, -1, -2, -1) : "no header hook (GUIManager gives it)");
            if (a.tweaks == 2) TweaksSheet::debugScrollEnd();   // 2.11 round 4: to its last rows (the Halo slider)
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    // #97: the system panel, for the harness to see its About button.
    if (a.drawer >= 0) {
        if (!s_drawerHook) dbgOut("no drawer hook (GUIManager gives it)\n");
        else if (s_drawerHook(a.drawer != 0)) dbgOut("drawer: already %s\n", a.drawer ? "open" : "closed");
        else dbgOut("drawer: %s\n", a.drawer ? "opening" : "closing");
        s_dreq.store(DREQ_DONE);
        return;
    }
    // 2.8: the header - a slot's panel open or closed, the glyph style (not
    // saved: a capture leaves the owner's choice), the page's header (saved),
    // a pretend alarm (RAM only).
    if (a.slot[0] || a.glyphs >= 0 || a.pageHdr >= 0 || a.alarm > -2 || a.alarmIn >= 0) {
        dbgOut("%s\n", s_headerHook ? s_headerHook(a.slot, a.glyphs, a.pageHdr, a.alarm, a.alarmIn)
                                    : "no header hook (GUIManager gives it)");
        s_dreq.store(DREQ_DONE);
        return;
    }
    // 2.12 P0, for scripts/ui_golden.py: the object tree (above); LVGL's pool
    // to the byte; and the sparks, which are random, off for a capture.
    if (a.tree) {
        treeDump(a.tree == 2);
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.mem || a.sparks >= 0) {
        if (a.sparks >= 0) {
            using M = Kit::SparksMode;
            Kit::setSparksMode(a.sparks == 1 ? M::SPARKS_ONCE : a.sparks == 2 ? M::SPARKS_OFF : M::SPARKS_NOW_AND_THEN);
            dbgOut("sparks %d from the next window\n", (int)Kit::sparksMode());
        }
        lv_mem_monitor_t m;
        lv_mem_monitor(&m);
        dbgOut("lv_mem used %lu free %lu biggest %lu frag %u; window %s\n",
               (unsigned long)(m.total_size - m.free_size), (unsigned long)m.free_size,
               (unsigned long)m.free_biggest_size, (unsigned)m.frag_pct, isOpen() ? "open" : "closed");
        s_dreq.store(DREQ_DONE);
        return;
    }
    // 2.11 (K91, K100): addto=N as Add to a cluster with the Nth cluster on
    // the page chosen, mv=1 Move (else Copy) - the window closes.
    if (a.addTo >= 0) {
        if (!isOpen()) dbgOut("no window open\n");
        else if (visibilityAddTo((uint8_t)a.addTo, a.mv != 0)) { dbgOut("%s\n", a.mv ? "moved" : "copied");
                                                                  s_dreq.store(DREQ_CLOSING); return; }
        else dbgOut("cannot: not one entity, no such cluster, or in it already\n");
        s_dreq.store(DREQ_DONE);
        return;
    }
    // 2.11 (K92 rule 9): join=1 as Add card to dashboard - in a member's own
    // view, or a tile's window - opening "which group?"; join=2+N its Add with
    // group N chosen (0 the group card's, 1 the area's, then the others).
    if (a.join >= 1) {
        Card *held = isOpen() ? cardOf(h.surface) : nullptr;
        if (!held) dbgOut("no window open\n");
        else if (a.join == 1) {
            Card *parent = nullptr;
            const Entity *e = nullptr;
            if (inMember() && s.ent[0]) { parent = held; e = s.ent[0]; }
            else if (held->mini())
                for (lv_obj_t *o = lv_obj_get_parent(h.surface); o && !parent; o = lv_obj_get_parent(o))
                    if (lv_obj_has_flag(o, CardPopup::CARD_SURFACE_FLAG)) { parent = cardOf(o); e = held->firstEntity(); }
            // A tile's own card, as its Visibility passes it: Copy or Move (the sprint after round 7).
            Card *mc = (held->mini() && held->hasId()) ? held : (inMember() ? visibilitySubject() : nullptr);
            if (parent && e) { visibilityJoinOpen(parent, e, mc); dbgOut("which group? for %s\n", e->desc.id); }
            else dbgOut("not a member's view or a tile\n");
        } else {
            // join=2+N&mv=1: Move (out of a cluster) - the window closes.
            if (visibilityJoin((uint8_t)(a.join - 2), a.mv != 0)) {
                dbgOut(a.mv ? "moved\n" : "added\n");
                if (a.mv) { s_dreq.store(DREQ_CLOSING); return; }
            } else dbgOut("not added\n");
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    // 2.11 (K104, K91): addcards=MASK on a cluster's window - its Add cards
    // with the cards whose bits are set ticked (bit 0 the list's first), mv=1
    // Move, and Add. The window closes.
    if (a.addCards > 0) {
        if (!isOpen() || !s.cluster) dbgOut("no cluster's window open\n");
        else if (const uint8_t n = clusterAddCards(a.mv ? 0u : (uint32_t)a.addCards, a.mv ? (uint32_t)a.addCards : 0u)) {
            dbgOut("%s %u\n", a.mv ? "moved" : "copied", (unsigned)n);
            s_dreq.store(DREQ_CLOSING);
            return;
        } else dbgOut("none added\n");
        s_dreq.store(DREQ_DONE);
        return;
    }
    // 2.11 (K96, round 6): restore=N as a tap on the Nth card's Restore in the
    // hidden cards' sheet; restore=-2 as a tap on the page panel's "View hidden
    // cards" (slot=page opens the panel first).
    if (a.restore == -2) {
        dbgOut(SlotPanels::viewHiddenDebug() ? "hidden cards: open\n" : "no View hidden cards in an open page panel\n");
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.restore >= 0) {
        dbgOut(HiddenSheet::restoreDebug((uint8_t)a.restore) ? "restoring hidden card %d\n"
                                                             : "no hidden card %d in an open hidden cards sheet\n", a.restore);
        s_dreq.store(DREQ_DONE);
        return;
    }
    // 2.11 (K95): the keyboard. kbd=1 opens it as the window's Rename does (a
    // cluster) or Entity label's Custom (a card); kbd=2 as Card label's Custom.
    // kbtype= words as the keys type them, kbmode= 0 lower, 1 upper, 2 symbols,
    // kbpress= 1 Save, 2 Cancel, 3 the field emptied (before kbtype=).
    if (a.kbd > 0 || a.kbType[0] || a.kbMode >= 0 || a.kbPress > 0) {
        if (a.kbd > 0) {
            if (!isOpen())                     dbgOut("no window open\n");
            else if (s.cluster && a.kbd == 1)  clusterRename();
            else                               cardUiTypeWords(a.kbd == 1);
        }
        if ((a.kbType[0] || a.kbMode >= 0 || a.kbPress > 0) && !Kit::keyboardDebug(a.kbType, a.kbMode, a.kbPress))
            dbgOut("no keyboard showing\n");
        lv_mem_monitor_t m;
        lv_mem_monitor(&m);
        dbgOut("keyboard %s; lv_mem used %lu\n", Kit::keyboardShowing() ? "showing" : "gone",
               (unsigned long)(m.total_size - m.free_size));
        s_dreq.store(DREQ_DONE);
        return;
    }
    // K17: addcard=1 as SETTINGS' "Add as a card" in a member's own view;
    // removecard=1 as Visibility's "Hide from this page" and its dialog's Hide
    // (K96, every card since 2.11: to the page's pool).
    if (a.addCard > 0 || a.removeCard > 0) {
        Card *held = isOpen() ? cardOf(h.surface) : nullptr;
        char id[CARD_ID_MAX + 1] = "";
        if (!held) dbgOut("no window open\n");
        else if (a.addCard > 0) {
            if (!inMember() || !s.ent[0]) dbgOut("not in a member's own view\n");
            else if (CardAdded::add(*held, *s.ent[0], id, sizeof(id))) { dbgOut("added %s\n", id); deckSync(); }
            else dbgOut("not added\n");
        } else if (a.removeCard == 2 || a.removeCard == 3) {
            // removecard=2: the dialog, as "Hide" opens it. 3 (2.12, scripts/
            // ui_golden.py) the same, to be looked at - the harness never taps
            // its Hide, and close=1 takes it down with the window.
            confirmHide();
            dbgOut("dialog up\n");
        } else if (a.removeCard == 4) {
            // removecard=4 (2.11, K91): a copy in a cluster - its "Remove this copy?" dialog.
            confirmRemoveCopy();
            dbgOut(CardAdded::isMember(held->id()) ? "dialog up\n" : "not a copy made here\n");
        } else if (a.removeCard == 5) {
            // removecard=5: the copy removed, as the dialog's Remove - its entry deleted.
            snprintf(id, sizeof(id), "%s", held->id());
            if (CardAdded::removeMember(id)) { dbgOut("removed %s\n", id); close(); s_dreq.store(DREQ_CLOSING); return; }
            dbgOut("%s is not a copy made here\n", id);
        } else if (a.removeCard == 6 || a.removeCard == 7 || a.removeCard == 8) {
            // The sprint after round 7: a card taken out of a cluster. 6 Remove from
            // the page with no dialog (deleted), 7 its dialog, 8 Move back to the cluster.
            snprintf(id, sizeof(id), "%s", held->id());
            if (a.removeCard == 7) { confirmRemoveFromPage(); dbgOut("dialog up\n"); }
            else if (a.removeCard == 6 ? CardAdded::drop(id) : CardAdded::moveBack(id)) {
                dbgOut("%s %s\n", a.removeCard == 6 ? "removed" : "moved back", id);
                close();
                s_dreq.store(DREQ_CLOSING);
                return;
            } else dbgOut("%s: %s\n", id, a.removeCard == 6 ? "not a card made here, or a moved one"
                                                           : "not moved out of a cluster");
        } else if (held->mini()) {
            // A mini tile's: Visibility's Hide from the cluster.
            dbgOut("%s hidden from its cluster\n", held->id());
            visibilityHideTile();
            s_dreq.store(DREQ_CLOSING);
            return;
        } else {
            snprintf(id, sizeof(id), "%s", held->id());
            if (CardAdded::hide(id)) { dbgOut("hidden %s\n", id); close(); s_dreq.store(DREQ_CLOSING); return; }
            dbgOut("%s: no id, or hidden already\n", id);
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.pause >= 0 || a.member >= 0) {
        if (!isOpen()) dbgOut("no window open\n");
        else if (a.member >= 0) {
            // As a tap on row N of Members (opens Members first if needed).
            if (s.view != PopupView::VIEW_MEMBERS) showView(PopupView::VIEW_MEMBERS);
            s_memberPick = (uint8_t)a.member;
            enterMemberAsync(nullptr);
            dbgOut("member %d: %s\n", a.member, s.view == PopupView::VIEW_MEMBER ? "open" : "not open");
        } else {
            // Pause (1) or resume (0) whatever the window is showing, as the deck's chip.
            const uint32_t now = millis();
            for (uint8_t i = 0; i < s.nEnt; i++) if (s.ent[i]) s.reg->setPaused(s.ent[i]->desc.id, a.pause != 0, now);
            // A group defined in HA pauses with its members, as Card::setPaused does.
            if (s.native && !inMember()) {
                s.reg->setPaused(s.native->desc.id, a.pause != 0, now);
                for (uint8_t i = 0; i < s.native->nMembers; i++)
                    if (const Entity *m = memberAt(i)) s.reg->setPaused(m->desc.id, a.pause != 0, now);
            }
            dbgOut("pause %d\n", a.pause);
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.scheme >= 0) {
        // The colour scheme, as the drawer's button sets it (0 Fleet, 1
        // Midnight, 2 Linen) - with no window open, as from the dashboard.
        // Through GUIManager when it is there (2.8 round 3): the ground, the
        // header and the page follow, and nothing is saved.
        char name[12];
        snprintf(name, sizeof(name), "scheme%d", a.scheme);
        if (isOpen()) dbgOut("close the window first\n");
        else if (s_headerHook) dbgOut("%s\n", s_headerHook(name, -1, -1, -2, -1));
        else { UI::setSchemeIndex((uint8_t)a.scheme); dbgOut("scheme %d\n", a.scheme); }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.show >= 0) {
        // Which scenes the window offers, as SETTINGS' Scenes row: 0 visible
        // only, 1 all, 2 none. On the held card, as the row does.
        if (!isOpen()) dbgOut("no window open\n");
        else {
            s.sceneShow = a.show == 1 ? SceneShow::SCENES_ALL : a.show == 2 ? SceneShow::SCENES_OFF
                                                                           : SceneShow::SCENES_VISIBLE;
            if (Card *c = cardOf(h.surface)) c->setSceneShow(s.sceneShow);
            keepCardSetting("scenes", sceneShowName(s.sceneShow));   // as the row does
            s.lastScene = -1;
            showView(s.view == PopupView::VIEW_SCENES && !hasScenes() ? controlsView() : s.view);
            dbgOut("scenes shown %d, %u of them\n", a.show, (unsigned)shownScenes());
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.look >= 0) {
        // The selector's look for the scheme showing: 0 Black - Square,
        // 1 Black - Round, 2 Silver - Square, 3 Silver - Round; 4 the scheme's own.
        keepLook(a.look >= 4 ? (int8_t)-1 : (int8_t)(a.look & 3));   // kept as the row keeps it
        if (isOpen()) showView(s.view);
        else          Settings::save();   // no window to close and ask for it
        dbgOut("selector %d for scheme %u\n", (int)chosenLook(), (unsigned)UI::schemeIndex());
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.scene >= 0) {
        // As a tap on offered scene N (2.10c). Lists every scene the light has,
        // the offered ones numbered, either way.
        const Entity *h = isOpen() ? sceneHost() : nullptr;
        if (!h || !h->nScenes) dbgOut("no scenes in this window\n");
        else {
            for (uint8_t i = 0, k = 0; i < h->nScenes; i++) {
                const Entity *sc = s.reg->sceneOf(*h, i);
                if (!sc) continue;
                const bool shown = s.sceneShow == SceneShow::SCENES_ALL ||
                                   (s.sceneShow == SceneShow::SCENES_VISIBLE && !sc->sourceHidden);
                if (shown) dbgOut("  %u ", (unsigned)k++);
                else       dbgOut("  - ");
                dbgOut("%-12s %s%s%s\n", sc->desc.name, sc->desc.externalRef,
                       sc->sourceHidden ? " (hidden in HA)" : "", sc->cmdFailed ? " FAILED" : "");
            }
            const Entity *sc = shownScene((uint8_t)a.scene);
            const bool ok = sc && s.reg->press(sc->desc.id, millis());
            if (ok) { sceneLoaded((uint8_t)a.scene); renderMain(); }
            dbgOut("scene %d: %s\n", a.scene, ok ? "loaded" : "not loaded");
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.power) {
        // As a tap on Power (or on a switch's toggle).
        if (!isOpen()) dbgOut("no window open\n");
        else { const bool on = !aggregate().on; commandAll(on); dbgOut("power %s\n", on ? "on" : "off"); }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.view >= 0) {
        if (!isOpen()) dbgOut("no window open\n");
        else {
            // As the corner icons do: 0 the controls of whatever is showing (a
            // member's, inside one), 1 its history, 4 its scenes, 2 Members
            // (back out of a member first, as the arrow does). 3 only lists.
            if (a.view == 2) { if (inMember()) leaveMember(); showView(PopupView::VIEW_MEMBERS); }
            else if (a.view == 0) showView(controlsView());
            else if (a.view == 1) showView(PopupView::VIEW_HISTORY);
            else if (a.view == 4 && hasScenes()) showView(PopupView::VIEW_SCENES);
            else if (a.view == 5) showView(PopupView::VIEW_PROPS);   // as SETTINGS' Entity properties
            else if (a.view == 6) showView(PopupView::VIEW_CARD_UI); // as SETTINGS' Card UI (2.8 round 3)
            else if (a.view == 7 && s.cluster) showView(PopupView::VIEW_CLUSTER_SLOTS);   // 2.11: Header slots
            else if (a.view == 8) showView(PopupView::VIEW_VISIBILITY);   // 2.11 (K96): as SETTINGS' Visibility
            else if (a.view == 9) showView(PopupView::VIEW_ADD_TO);       // ... and its Add to a cluster
            else if (a.view == 10 && s.cluster) showView(PopupView::VIEW_CLUSTER_ADD);   // a cluster's Add cards
            dbgMem(mem, sizeof(mem));
            dbgOut("view %d; %s\n", a.view, mem);
            // Why the last commands failed, newest first (G8).
            EntityRegistry::FailNote notes[EntityRegistry::FAIL_NOTES];
            const uint8_t nn = s.reg ? s.reg->failNotes(notes, EntityRegistry::FAIL_NOTES) : 0;
            for (uint8_t i = 0; i < nn; i++)
                dbgOut("  failed %lus ago: %s: %s\n", (unsigned long)((millis() - notes[i].atMs) / 1000),
                       notes[i].id, notes[i].why);
            // What the window stands for, then a group defined in HA's members
            // ("m"), and whether it is working through them (2.10c).
            const Entity *list[2 * POPUP_ENT_MAX];
            uint8_t nList = 0;
            for (uint8_t i = 0; i < s.nEnt; i++) list[nList++] = s.ent[i];
            const uint8_t nShown = nList;
            if (s.native) {
                dbgOut("  group defined in HA: %s, %u members, %s\n", s.native->desc.externalRef,
                       (unsigned)s.native->nMembers, s.expanded ? "through its members" : "as one");
                for (uint8_t i = 0; i < s.native->nMembers; i++) list[nList++] = memberAt(i);
            }
            for (uint8_t i = 0; i < nList; i++) {
                const Entity *e = list[i];
                if (!e) continue;
                if (i >= nShown) dbgOut("m");
                const EntityAttrs &at = e->attrs;
                dbgOut("  %-12s %s caps %x mode %d bri %d K %d hue %d sat %d%s%s%s%s\n", e->desc.id,
                       e->value.type == ValueType::BOOL && e->value.b ? "on " : "off", at.lightCaps,
                       (int)at.lightMode, at.brightness, at.colorTempK, at.hue, at.sat,
                       e->pending ? " pending" : "", e->attrPending ? " levels-pending" : "",
                       e->cmdFailed ? " FAILED" : "", e->paused ? " PAUSED" : "");
            }
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.set >= 0) {
        // As if a finger tapped the slider at this value (percent, kelvin, hue).
        if (!isOpen() || !s.lightHero) dbgOut("no light window open\n");
        else { lightSend(a.set); renderLight(); dbgOut("sent %d to control %d\n", a.set, (int)s.lightCtl); }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.ctl >= 0) {
        if (!isOpen() || !s.lightHero) dbgOut("no light window open\n");
        else {
            s.lightCtl = (uint8_t)LV_MIN(a.ctl, (int)LCTL_COLOUR);
            const uint32_t t0 = micros();
            showView(PopupView::VIEW_MAIN);
            const uint32_t us = micros() - t0;
            dbgMem(mem, sizeof(mem));
            dbgOut("control %d built in %lu us; %s\n", (int)s.lightCtl, (unsigned long)us, mem);
            // Where the stage's children landed (L5: the deck moving on Color).
            lv_obj_update_layout(s.win);
            for (uint32_t i = 0; i < lv_obj_get_child_count(s.stage); i++) {
                lv_area_t c; lv_obj_get_coords(lv_obj_get_child(s.stage, (int32_t)i), &c);
                dbgOut("  stage child %lu: x %ld..%ld y %ld..%ld\n", (unsigned long)i,
                       (long)c.x1, (long)c.x2, (long)c.y1, (long)c.y2);
            }
            lv_area_t st; lv_obj_get_content_coords(s.stage, &st);
            dbgOut("  stage content x %ld..%ld, scroll %ld,%ld\n", (long)st.x1, (long)st.x2,
                   (long)lv_obj_get_scroll_x(s.stage), (long)lv_obj_get_scroll_y(s.stage));
            if (d.obj) { lv_area_t c; lv_obj_get_coords(d.obj, &c);
                         dbgOut("  deck x %ld..%ld\n", (long)c.x1, (long)c.x2); }
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.deck >= 0) {
        if (!isOpen()) dbgOut("no window open\n");
        else {
            // 2: the CHART panel open (History only).
            if (a.deck == 2)  { if (s.left.root) chartSet(DECK_OPEN); }   // CHART or SCENES (K43)
            else if (a.deck)  deckOpen();
            else            { deckSet(DECK_PEEK); chartFold(); }
            dbgMem(mem, sizeof(mem));
            dbgOut("deck %s; %s\n", a.deck == 2 ? "chart open" : a.deck ? "open" : "folded", mem);
        }
        s_dreq.store(DREQ_DONE);
        return;
    }

    // HISTORY (2.10e): the span or period, "Show more", and what HaHistory has.
    if (a.span >= 0 || a.more || a.row >= 0 || a.dur >= 0 || a.chips >= 0) {
        if (!isOpen() || s.view != PopupView::VIEW_HISTORY) { dbgOut("History is not showing\n"); s_dreq.store(DREQ_DONE); return; }
        // dur=0|1: activity's Show duration; chips=0|1: the span buttons (neither kept).
        if (a.dur >= 0) s_hv.duration = a.dur != 0;
        if (a.chips >= 0) { if (s_hv.chart) s_hv.chips = a.chips != 0; else s_hv.actChips = a.chips != 0; }
        // row=N: as a tap on activity row N from the top (its details).
        if (a.row >= 0 && s_hist && a.row < s_hist->events()) {
            const int16_t i = (int16_t)(s_hist->events() - 1 - a.row);
            s_hv.openRow = (s_hv.openRow == i) ? -1 : i;
        }
        if (a.span >= 0) {
            if (s_hv.chart && a.span < HIST_SPANS)        { s_hv.span = (uint8_t)a.span; histSyncPanel(); }
            else if (!s_hv.chart && a.span < HIST_PERIODS) { s_hv.period = (uint8_t)a.span; s_hv.more = false; }
            histAsk();
        }
        if (a.more) s_hv.more = true;
        histDraw();
    }
    // x=N: as a finger on the chart, N px from its left edge.
    if (a.x >= 0) {
        if (!isOpen() || s.view != PopupView::VIEW_HISTORY || !s_hv.area) { dbgOut("no chart showing\n"); s_dreq.store(DREQ_DONE); return; }
        histScrub(a.x);
    }
    if ((a.span >= 0 || a.more || a.hist || a.row >= 0 || a.x >= 0 || a.dur >= 0 || a.chips >= 0) && s_hist) {
        dbgOut("history: %s %s, state %u fail %u gen %lu/%lu, %u points%s, %u events (+%u older), last answer %lu B in %lu ms\n",
               s_hv.e ? s_hv.e->desc.externalRef : "-", s_hv.chart ? "chart" : "activity",
               (unsigned)s_hist->state(), (unsigned)s_hist->fail(), (unsigned long)s_hv.gen,
               (unsigned long)s_hist->gen(), (unsigned)s_hist->points(), s_hist->fromStatistics() ? " (statistics)" : "",
               (unsigned)s_hist->events(), (unsigned)s_hist->eventsDropped(), (unsigned long)s_hist->lastBytes(),
               (unsigned long)s_hist->lastMs());
        s_dreq.store(DREQ_DONE);
        return;
    }

    lv_obj_t *cards[48];
    const int n = dbgCards(lv_screen_active(), cards, 0, 48);
    if (a.clabel >= 0) {
        // As choosing line N of the open window's Card label row (K46).
        Card *c = isOpen() ? cardOf(h.surface) : nullptr;
        if (!c) { dbgOut("no window open\n"); s_dreq.store(DREQ_DONE); return; }
        uint16_t sel = 0;
        s_clN = CardGroups::rowChoices(*c, s_clMap, 24, s_clOpts, sizeof(s_clOpts), sel);
        if (a.clabel < s_clN) {
            CardGroups::choose(*c, s_clMap[a.clabel], c->pageAreaColor());
            c->restyle();
        }
        uint16_t now = 0;
        s_clN = CardGroups::rowChoices(*c, s_clMap, 24, s_clOpts, sizeof(s_clOpts), now);
        dbgOut("card label: \"%s\" #%06lX; the row, now on line %u:\n%s\n", c->area(),
               (unsigned long)c->headerColor(), (unsigned)now, s_clOpts);
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.groups >= 0) {
        // The groups for card labels (K46), and each card's on this page.
        CardGroups::forEach([](const CardGroups::Group &g, void *) {
            dbgOut("group %-28s %-14s #%06lX %s%s%s\n", g.id, g.name, (unsigned long)g.color,
                   g.haName[0] ? "HA area " : "local", g.haName, g.solo ? " (solo)" : "");
        }, nullptr);
        for (int i = 0; i < n; i++) {
            Card *c = cardOf(cards[i]);
            if (!c) continue;
            CardGroups::Group g;
            const bool has = CardGroups::ofCard(*c, g);
            dbgOut("%2d %-12s label \"%s\" #%06lX  group %s\n", i, c->label(), c->area(),
                   (unsigned long)c->headerColor(), has ? g.id : "none");
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.tap >= 0) {
        // As a TAP on card N (2.10d): whatever its Tap action says - toggle,
        // open the window on a view, load a scene, or nothing.
        Card *c = (a.tap < n) ? cardOf(cards[a.tap]) : nullptr;
        if (!c)            dbgOut("no card %d (%d on this page)\n", a.tap, n);
        else if (isOpen()) dbgOut("a window is already open; /popup?close=1 first\n");
        else {
            if (s_indev) lv_indev_get_point(s_indev, &s.pressStart);
            c->tap();
            dbgOut("tap card %d (%s): tap action %s%s%s; window %s\n", a.tap, c->label(),
                   tapActionName(c->tapAction()), c->tapScene()[0] ? ", scene " : "", c->tapScene(),
                   isOpen() ? (s.view == PopupView::VIEW_HISTORY ? "open on History"
                               : s.view == PopupView::VIEW_MEMBERS ? "open on Members" : "open") : "closed");
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.card < 0) {
        dbgMem(mem, sizeof(mem));
        dbgOut("%s; window %s; scheme %u, selector %d (-1: the scheme's own); faces flat so far %lu\n", mem,
               isOpen() ? "open" : "closed", (unsigned)UI::schemeIndex(), (int)chosenLook(),
               (unsigned long)Kit::facesFlat());
        dbgOut("arranger: %s\n", PageArranger::lastText());   // 2.11 round 2
        for (int i = 0; i < n; i++) {
            Card *c = cardOf(cards[i]);
            if (c) dbgOut("%2d  %-16s %-12s %u entit%s\n", i, c->label(), c->_area,
                          (unsigned)c->_nPrimary, c->_nPrimary == 1 ? "y" : "ies");
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.card >= n || !cardOf(cards[a.card])) dbgOut("no card %d (%d on this page)\n", a.card, n);
    else if (isOpen())                          dbgOut("a window is already open; /popup?close=1 first\n");
    else {
        dbgMem(mem, sizeof(mem));
        dbgOut("before: %s\n", mem);
        // A finger that moved is a drag (open() checks): there is no finger,
        // so where it "began" is wherever the input device last was.
        if (s_indev) lv_indev_get_point(s_indev, &s.pressStart);
        const uint32_t t0 = micros();
        open(*cardOf(cards[a.card]));
        const uint32_t us = micros() - t0;
        dbgMem(mem, sizeof(mem));
        dbgOut("open card %d: built in %lu us; window %ldx%ld px, propH %ld, hero %ldx%ld; %s\n",
               a.card, (unsigned long)us, (long)lv_area_get_width(&s.winRect),
               (long)lv_area_get_height(&s.winRect), (long)s.propH, (long)s.heroW, (long)s.heroH, mem);
    }
    s_dreq.store(DREQ_DONE);
}

void CardPopup::beginDebug(HttpServer &http) {
    http.addRoute("/popup", HTTP_GET, handlePopup);
    lv_timer_create(debugService, 20, nullptr);
}

void CardPopup::setDrawerHook(bool (*hook)(bool open)) { s_drawerHook = hook; }
void CardPopup::setHeaderHook(const char *(*hook)(const char *slot, int glyphs, int pageHdr, int alarm, int alarmIn)) { s_headerHook = hook; }
#endif
