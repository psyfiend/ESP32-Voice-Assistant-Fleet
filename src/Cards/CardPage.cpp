#include "Cards/CardPage.h"
#include "Cards/CardTypes.h"
#include "Cards/CardCatalog.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include "SystemReport.h"
#include "Settings.h"
#include "Cards/CardGroups.h"
#include "Cards/CardAdded.h"     // a cluster's members added on the device (2.11, K91)
#include "Cards/ClusterCard.h"
#include "Layout/Cluster.h"
#include <Arduino.h>
#ifdef DEBUG_PAGE_TIMING
#include <esp_timer.h>   // where a page rebuild's time goes - see commit()
#endif

CardPage::~CardPage() {
    for (uint8_t i = 0; i < _n; i++) {
        if (!_cards[i]) continue;
        if (_binder) _binder->remove(_cards[i]);
        delete _cards[i];           // Card's destructor deletes its widgets
    }
    if (_root) lv_obj_delete(_root);
}

void CardPage::begin(lv_obj_t *parent, CardBinder *binder, uint8_t subdivision) {
    _binder = binder;

    const UIGrid &g = UI::grid();

    const int32_t inset = UI::sc(g.INSET);
    _gap = UI::sc(g.GAP);

    _sub = subdivision ? subdivision : 1;
    if (_sub > 4) _sub = 4;     // thirds are already exotic; quarters are the
                                // most anyone has asked for

    _root = lv_obj_create(parent);
    lv_obj_set_size               (_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color     (_root, UI::c(UI::pal().GROUND), 0);
    lv_obj_set_style_border_width (_root, 0, 0);
    lv_obj_set_style_pad_all      (_root, inset, 0);
    lv_obj_set_style_pad_column   (_root, _gap, 0);
    lv_obj_set_style_pad_row      (_root, _gap, 0);

    // A DASHBOARD PAGE DOES NOT SCROLL. The owner, unprompted: "There should
    // never be any scrolling of cards on any dashboard pages, the cards should
    // effectively be locked in place. Only when swiping or navigating to a new
    // page entirely should new entities be on the screen."
    //
    // So cards that do not fit are not placed at all, and WHICH ones is now
    // CardPlacement::priority's decision rather than declaration order's.
    lv_obj_clear_flag        (_root, LV_OBJ_FLAG_SCROLLABLE);
    // NOT CLICKABLE either, so a tap on the gap between two cards falls
    // through to the screen instead of stopping here.
    //
    // The cards are the interactive things; this is the grid they sit in, and
    // it was absorbing every tap that missed one. That is invisible until
    // something wants those taps - 2.6's "tap anywhere else to dismiss" is the
    // first thing that does, and without this it would work on the header
    // strip and the margins and nowhere else, which is the kind of half-broken
    // that gets blamed on the gesture code.
    //
    // Gestures are unaffected: LV_OBJ_FLAG_GESTURE_BUBBLE is on by default, so
    // a swipe still reaches the screen either way.
    lv_obj_clear_flag        (_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(_root, LV_SCROLLBAR_MODE_OFF);

    // --- The unit grid ----------------------------------------------------
    //
    // Columns are FR units, not the pixel widths UI::grid() computed. That is
    // not ignoring the token - cellW is what DERIVED the column count, and the
    // count is the decision. Expressing the result as fractions is what makes
    // a spanning card come out exactly right, and it is also what makes the
    // subdivision free: `sub` FR tracks plus the gaps between them measure the
    // same as one cell did, exactly, with no rounding.
    uint16_t uc = (uint16_t)g.cols * _sub;
    if (uc < 1) uc = 1;
    if (uc > PAGE_MAX_UNIT_COLS) uc = PAGE_MAX_UNIT_COLS;
    _uCols = (uint8_t)uc;

    for (uint8_t i = 0; i < _uCols; i++) _colDsc[i] = LV_GRID_FR(1);
    _colDsc[_uCols] = LV_GRID_TEMPLATE_LAST;

    // ROWS ARE NOT DECIDED HERE ANY MORE, and that is the 2.5 fix the owner
    // found on the glass.
    //
    // They used to come straight from UI::grid().rows, which is pure geometry:
    // "how many rows of roughly this shape fit in this height". With 13 cards
    // on a 7-column board that asked for FOUR rows, and since the page then
    // divides the height by the row count, every card was sized for a row that
    // had nothing in it - which on the 7B pushed all of them under the
    // compact threshold. Hiding the deck made it WORSE, because the extra
    // height bought a fifth row rather than taller cards.
    //
    // His rule, and it is the right one: "there is no reason to create an
    // additional row if all the cards can be made to fit". So the page records
    // the height it has to spend and decides the row count in commit(), once
    // it knows what it is being asked to place.
    lv_obj_update_layout(_root);   // _root was created a moment ago; without
                                   // this its content height is still zero
    _availH = (int32_t)lv_obj_get_content_height(_root);
    _availW = (int32_t)lv_obj_get_content_width(_root);   // 0 if not laid out;
                                                          // widthOf() copes
    if (_availH <= 0) {
        // Called before the parent has been laid out. Fall back to the token's
        // own arithmetic rather than to zero.
        _availH = (int32_t)g.cellH * (g.rows ? g.rows : 1)
                + _gap * ((g.rows ? g.rows : 1) - 1);
    }

    // The most rows this height could carry before a card stops being able to
    // draw a full layout. Below this a card goes compact, which is a decision
    // the CARD makes about its own contents - the page just refuses to plan
    // more rows than could ever be useful.
    _maxRows = 1;
    for (uint8_t r = 1; r <= PAGE_MAX_UNIT_ROWS / _sub; r++) {
        const int32_t cellH = (_availH - _gap * (r - 1)) / r;
        if (cellH < Card::compactCellNeedPx()) break;
        _maxRows = r;
    }

    useRows(1);   // provisional; commit() replaces it

    lv_obj_set_grid_dsc_array(_root, _colDsc, _rowDsc);
    lv_obj_set_layout        (_root, LV_LAYOUT_GRID);

    _n = 0;
    _placed = 0;
    _committed = false;

    Serial.printf("[Cards] Page %u cols (%u units), %ld px of height, max %u rows, gap %ld\n",
                  (unsigned)g.cols, (unsigned)_uCols,
                  (long)_availH, (unsigned)_maxRows, (long)_gap);
}

// How many CELL rows the cards need, from the area they ask for.
//
// A lower bound rather than an answer: it assumes cards tile without waste,
// which a 2-wide card on an odd column count does not. commit() starts here
// and adds rows until placement succeeds.
uint8_t CardPage::rowsWanted() const {
    if (!_n) return 1;
    const uint8_t cellCols = _uCols / _sub ? _uCols / _sub : 1;

    Layout::Item items[CARD_PAGE_MAX];
    toItems(items);
    return Layout::rowsWanted(items, _n, cellCols, _sub);
}

// Spend the height on exactly this many cell rows.
//
// The rows share the whole height, so FEWER rows means TALLER cards - which is
// what makes hiding the deck grow the cards instead of shrinking them.
//
// One guard: a page with two cards on it would otherwise get one row as tall
// as the screen. UIGrid::ASPECT_PCT caps how tall a card may be relative to
// its width; past that the grid stops stretching and the slack is left at the
// bottom rather than poured into a card nobody wants that tall.
void CardPage::useRows(uint8_t cellRows) {
    if (cellRows < 1) cellRows = 1;
    if (cellRows > PAGE_MAX_UNIT_ROWS / _sub) cellRows = PAGE_MAX_UNIT_ROWS / _sub;

    int32_t cellH = (_availH - _gap * (cellRows - 1)) / cellRows;

    // The ASPECT ceiling is a guard against a two-card page making each card
    // as tall as the screen. It does NOT apply when a row count was demanded -
    // there the tall card is what was asked for, and quietly refusing to give
    // it would make the button feel broken in exactly the way the old one did.
    if (!_rowsOverride && !_arranged) {   // the arranger chose its rows for the whole height
        const int32_t capH = ((int32_t)UI::grid().cellW * UI::grid().ASPECT_PCT) / 100;
        if (capH > 0 && cellH > capH) cellH = capH;
    }
    if (cellH < 1) cellH = 1;

    _unitH = (cellH - _gap * (_sub - 1)) / _sub;
    if (_unitH < 1) _unitH = 1;

    uint16_t ur = (uint16_t)cellRows * _sub;
    if (ur > PAGE_MAX_UNIT_ROWS) ur = PAGE_MAX_UNIT_ROWS;
    _uRows = (uint8_t)ur;

    for (uint8_t i = 0; i < _uRows; i++) _rowDsc[i] = (lv_coord_t)_unitH;
    _rowDsc[_uRows] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(_root, _colDsc, _rowDsc);
}

int32_t CardPage::heightOf(uint8_t spanY) const {
    if (spanY < 1) spanY = 1;
    return _unitH * spanY + _gap * (spanY - 1);
}

// The same for WIDTH. Columns are FR tracks, so a unit's width is the content
// width less the gaps, shared equally - exactly what LVGL's grid will do. Zero
// when the page was never laid out; the card then falls back to the token.
int32_t CardPage::widthOf(uint8_t spanX) const {
    if (_availW <= 0 || _uCols < 1) return 0;
    if (spanX < 1) spanX = 1;
    const int32_t unitW = (_availW - _gap * (_uCols - 1)) / _uCols;
    return unitW * spanX + _gap * (spanX - 1);
}

// ---------------------------------------------------------------------------
// Placement - Layout's flow engine (2.12 P5). The rules, and why each is what
// it is - pins first, the widest span at the first free spot, the lowest
// priority dropped - moved to src/Layout/Layout.cpp unchanged.
// ---------------------------------------------------------------------------

// The cards as the engine sees them. A card commit() freed is an empty slot.
void CardPage::toItems(Layout::Item *out) const {
    for (uint8_t i = 0; i < _n; i++) {
        Layout::Item it;
        if (_cards[i]) {
            const CardPlacement &p = _cards[i]->placement();
            it.prefSpanX = p.prefSpanX; it.prefSpanY = p.prefSpanY;
            it.minSpanX  = p.minSpanX;  it.minSpanY  = p.minSpanY;
            it.col       = p.col;       it.row       = p.row;
            it.priority  = p.priority;
        } else {
            it.present = false;
        }
        out[i] = it;
    }
}

void CardPage::plan() {
    Layout::Item items[CARD_PAGE_MAX];
    toItems(items);

    // The engine says what it decided; the page says it with the card's name.
    Layout::Events ev;
#ifdef DEBUG_CARDS
    ev.ctx = this;
    ev.pinRejected = [](void *ctx, uint8_t i, int8_t col, int8_t row) {
        DBG_CARDS("%s: pin %d,%d rejected (out of bounds or occupied) - flowing\n",
                  ((CardPage *)ctx)->_cards[i]->typeName(), (int)col, (int)row);
    };
    ev.dropped = [](void *ctx, uint8_t i, uint8_t priority) {
        DBG_CARDS("dropped %s (priority %u) - page is full\n",
                  ((CardPage *)ctx)->_cards[i]->typeName(), (unsigned)priority);
    };
#endif
    _placed = _engine.place(Layout::Container{_uCols, _uRows}, items, _n, _slot, &ev);
}

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------

Card *CardPage::add(Card *c) {
    if (!c) return nullptr;
    if (_n >= CARD_PAGE_MAX) {
        // Deleted rather than dropped on the floor. A page that silently
        // leaked its overflow would be a slow leak on a device whose whole
        // widget budget is a 128 KB static array.
        delete c;
        return nullptr;
    }
    _cards[_n] = c;
    _slot [_n] = Slot();
    _n++;
    return c;
}

void CardPage::commit() {
    if (_committed) return;
#ifdef DEBUG_PAGE_TIMING
    // -D DEBUG_PAGE_TIMING: one line per commit - placement, building every
    // card's widgets, and binding them - so a slow page swipe can be split
    // into its parts. GUIManager::rebuildDashboard() prints the whole.
    const int64_t tPlan0 = esp_timer_get_time();
    int64_t tBuild = 0, tBind = 0, tWorst = 0;
    const char *worst = "";
#endif

    // Spend as few rows as the cards need, and only add more when placement
    // genuinely cannot fit them. Dropping cards is the LAST resort, after the
    // page has already grown to every row it could carry.
    if (_arranged) {
        // THE ARRANGER'S PAGE (2.11 round 2): its grid, every block pinned.
        useCols(_arrCols);
        useRows(_arrRows);
        plan();
    } else if (_rowsOverride) {
        // OBEYED, not negotiated. The caller asked for this many rows; cards
        // that do not fit are dropped by priority, which is the whole point of
        // asking. The only clamp is the descriptor's own size.
        uint8_t rows = _rowsOverride;
        if (rows > PAGE_MAX_UNIT_ROWS / _sub) rows = PAGE_MAX_UNIT_ROWS / _sub;
        useRows(rows);
        plan();
    } else {
        uint8_t rows = rowsWanted();
        if (rows > _maxRows) rows = _maxRows;

        for (;;) {
            useRows(rows);
            plan();
            if (_placed == _n) break;       // everything fits at this row count
            if (rows >= _maxRows) break;    // no more rows to give - plan() drops
            rows++;
        }
    }

#ifdef DEBUG_PAGE_TIMING
    const int64_t tPlan = esp_timer_get_time() - tPlan0;
#endif
    Serial.printf("[Cards] %u cards want %u rows; using %u of max %u -> cell %ldx%ld px\n",
                  (unsigned)_n, (unsigned)rowsWanted(), (unsigned)(_uRows / _sub),
                  (unsigned)_maxRows,
                  (long)UI::grid().cellW, (long)heightOf(_sub));


    for (uint8_t i = 0; i < _n; i++) {
        Card *c = _cards[i];
        if (!c || !_slot[i].placed) continue;
#ifdef DEBUG_PAGE_TIMING
        const int64_t t0 = esp_timer_get_time();
#endif

        // THE CARD IS TOLD ITS REAL HEIGHT BEFORE IT IS BUILT.
        //
        // Card::resolveVariant() used to derive this from UI::grid() and its
        // own row span, which was right only while a row was a whole cell. It
        // is not, now that rows are units - and it was already fragile, since
        // the grid it read is global state that any other page can move. The
        // page knows the answer exactly; handing it over removes the guess.
        c->setCellHeightPx(heightOf(_slot[i].spanY));
        c->setCellWidthPx (widthOf (_slot[i].spanX));
        c->build(_root);
        lv_obj_set_grid_cell(c->root(),
                             LV_GRID_ALIGN_STRETCH, _slot[i].col, _slot[i].spanX,
                             LV_GRID_ALIGN_STRETCH, _slot[i].row, _slot[i].spanY);
#ifdef DEBUG_PAGE_TIMING
        const int64_t t1 = esp_timer_get_time();
#endif
        if (_binder) _binder->add(c);
#ifdef DEBUG_PAGE_TIMING
        const int64_t t2 = esp_timer_get_time();
        tBuild += t1 - t0;
        tBind  += t2 - t1;
        if (t1 - t0 > tWorst) { tWorst = t1 - t0; worst = c->typeName(); }
#endif
    }
#ifdef DEBUG_PAGE_TIMING
    Serial.printf("[PageTiming] plan %lld us | build %u cards %lld us (slowest %s %lld) | bind %lld us\n",
                  (long long)tPlan, (unsigned)_placed, (long long)tBuild, worst,
                  (long long)tWorst, (long long)tBind);
#endif
    // THE PAGE'S HERO (round 4): a solo 1x1 card's disc and icon, measured now
    // it is built, and every cluster's tiles drawn again to match it where they
    // have the room - a tile's icon and a card's are the same size.
    // UNLESS THE CARD IS SQUEEZED (round 6, the owner: "the icons get smaller and
    // smaller the more rows are added", the clusters roomy around them): a card
    // whose icon had to step down from the full face sets nothing, and the tiles
    // size themselves - as large as a solo card's at most. Giving solo cards their
    // room is the arranger's (layout-engines.md, v8).
    {
        int32_t disc = 0;
        const lv_font_t *face = nullptr;
        for (uint8_t i = 0; i < _n && !face; i++) {
            Card *c = _cards[i];
            if (!c || !_slot[i].placed || _slot[i].spanX != _sub || _slot[i].spanY != _sub) continue;
            if (!strcmp(c->typeName(), "cluster")) continue;
            c->heroSizes(disc, face);
        }
        if (face != UI::type().ICON) { disc = 0; face = nullptr; }
        Card::setPageHero(disc, face);
        for (uint8_t i = 0; i < _n; i++) {
            Card *c = _cards[i];
            if (!c || !_slot[i].placed || strcmp(c->typeName(), "cluster")) continue;
            ClusterCard *cl = static_cast<ClusterCard *>(c);
            for (uint8_t k = 0; k < cl->memberCount(); k++)
                if (Card *m = cl->memberAt(k)) if (m->root()) m->restyle();
        }
    }

    // CARDS THAT DID NOT MAKE THE PAGE ARE FREED, not kept around.
    //
    // The owner asked directly: "is the device holding the cards not on the
    // page in memory?" It was. An unplaced card was never built, so it held no
    // LVGL widgets - but the object itself, its bound entity pointers and its
    // label buffers stayed alive for the life of the page. Small, and on the
    // board where this matters there is nothing to spare.
    //
    // Deleted here rather than in the destructor because they can never come
    // back: placement has already been decided, and a card that was not placed
    // will not be placed by anything short of a rebuild - which constructs
    // fresh ones from the spec anyway.
    // What they were is noted first, for the page's hidden cards (crowded()).
    _nCrowded = 0;
    for (uint8_t i = 0; i < _n; i++) {
        if (!_cards[i] || _slot[i].placed) continue;
        if (_nCrowded < CROWDED_MAX) {
            const Card *c = _cards[i];
            Crowded &k = _crowded[_nCrowded++];
            k.cluster = !strcmp(c->typeName(), "cluster");
            snprintf(k.id,   sizeof(k.id),   "%s", c->hasId() ? c->id() : "");
            snprintf(k.name, sizeof(k.name), "%s", k.cluster && c->area()[0] ? c->area() : c->shownName(CardLabel::LBL_NAME));
            snprintf(k.area, sizeof(k.area), "%s", c->area());
        }
        delete _cards[i];
        _cards[i] = nullptr;
    }

    _committed = true;

    if (_placed != _n) {
        Serial.printf("[Cards] %u of %u cards placed; %u dropped for space\n",
                      (unsigned)_placed, (unsigned)_n, (unsigned)(_n - _placed));
    }
}

void CardPage::relayout() {
    plan();
    for (uint8_t i = 0; i < _n; i++) {
        Card *c = _cards[i];
        if (!c || !c->root()) continue;
        if (!_slot[i].placed) { lv_obj_add_flag(c->root(), LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(c->root(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_grid_cell(c->root(),
                             LV_GRID_ALIGN_STRETCH, _slot[i].col, _slot[i].spanX,
                             LV_GRID_ALIGN_STRETCH, _slot[i].row, _slot[i].spanY);
    }
}

// ---------------------------------------------------------------------------
// From data
// ---------------------------------------------------------------------------

void CardPage::applySpec(const PageSpec &spec, EntityRegistry &reg) {
    Card::setShowAreaDefault(spec.showArea);

    for (uint8_t i = 0; i < spec.count; i++) {
        const CardSpec &cs = spec.cards[i];
        // A group card made on the device (2.11): a cluster, or its one card.
        Card *c = (cs.memberCount && cs.members) ? makeGroupCard(cs, spec, reg)
                                                 : makeCard(cs, spec, reg, i);
        if (c) add(c);
    }

    arrangePage(spec);
    commit();
}

void CardPage::arrangePage(const PageSpec &spec) {
    _arranged = false;
    if (!spec.slug || !_n) return;
    if (_n > Layout::ARR_BLOCKS_MAX) { PageArranger::flowed(spec.slug, "more blocks than the arranger takes"); return; }
    char off[4];   // system.arranger.off = 1: every page flows, as before 2.11 (to compare)
    if (Settings::system("arranger", "off", off, sizeof(off)) && off[0] == '1') {
        PageArranger::flowed(spec.slug, "the arranger is off");
        return;
    }
    PageArranger::Input in;
    in.slug = spec.slug;
    Layout::ArrScreen &s = in.scr;
    s.pmm  = (double)bspPixelDensity() / 25.4;
    s.aw   = _availW;
    s.ah   = _availH;
    s.gap  = _gap;
    s.tagH = Card::headerHeight();
    const UIGrid &g = UI::grid();
    s.recCols = g.cols;
    s.recRows = g.rows;
    // The page's own columns or rows (UI Tweaks): obeyed, the other the board's.
    if (g.COLS_OVERRIDE || _rowsOverride) {
        s.fixCols = g.COLS_OVERRIDE ? g.COLS_OVERRIDE : g.cols;
        s.fixRows = _rowsOverride ? _rowsOverride : g.rows;
    }
    for (uint8_t i = 0; i < _n; i++) {
        Card *c = _cards[i];
        if (!c) return;
        Layout::ArrBlock &b = in.blocks[in.n];
        b = Layout::ArrBlock();
        if (!strcmp(c->typeName(), "cluster")) {
            const ClusterCard *cl = static_cast<const ClusterCard *>(c);
            b.cluster = true;
            b.tiles   = cl->tileCount();
            // A cluster with a header always (one in a single cell, headerless,
            // is K103's - sensors only - and not drawn yet).
            b.hdr     = (uint8_t)LV_MAX(1, cl->headerCount());
            b.members = cl->memberCount();
        } else {
            b.members = 1;
        }
        in.ids[in.n] = c->id();
        in.n++;
    }
    Layout::ArrResult r;
    if (!PageArranger::kept(in, r)) { PageArranger::start(in); return; }
    for (uint8_t i = 0; i < _n; i++) {
        CardPlacement p = _cards[i]->placement();
        p.col = (int8_t)(r.pos[i].c * _sub);
        p.row = (int8_t)(r.pos[i].r * _sub);
        p.prefSpanX = p.minSpanX = (uint8_t)(r.pos[i].w * _sub);
        p.prefSpanY = p.minSpanY = (uint8_t)(r.pos[i].h * _sub);
        _cards[i]->setPlacement(p);
    }
    _arranged = true;
    _arrCols = r.cols;
    _arrRows = r.rows;
    Serial.printf("[Arranger] %s: the kept answer, %ux%u\n", spec.slug, (unsigned)r.cols, (unsigned)r.rows);
}

// The page's columns, from the arranger's grid (begin() took the board's).
void CardPage::useCols(uint8_t cellCols) {
    uint16_t uc = (uint16_t)cellCols * _sub;
    if (uc < 1) uc = 1;
    if (uc > PAGE_MAX_UNIT_COLS) uc = PAGE_MAX_UNIT_COLS;
    _uCols = (uint8_t)uc;
    for (uint8_t i = 0; i < _uCols; i++) _colDsc[i] = LV_GRID_FR(1);
    _colDsc[_uCols] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(_root, _colDsc, _rowDsc);
}

// One card from its spec, configured and with the owner's saved choices laid
// over it - or null when its entity is not here or has no card type.
Card *CardPage::makeCard(const CardSpec &cs, const PageSpec &spec, EntityRegistry &reg, uint8_t i, Card *made) {
    {
        // The FIRST primary that resolves decides the card's type. A spec may
        // name several - the aggregate light case - and a room where one bulb
        // has been unpaired should still render the rest.
        const Entity *first = nullptr;
        uint8_t       firstIdx = 0;
        for (uint8_t k = 0; k < CARD_PRIMARY_MAX; k++) {
            if (!cs.primaries[k]) continue;
            const Entity *e = reg.find(cs.primaries[k]);
            if (e) { first = e; firstIdx = k; break; }
            Serial.printf("[Cards] spec %u: entity \"%s\" not in registry\n",
                          (unsigned)i, cs.primaries[k]);
        }
        if (!first) { delete made; return nullptr; }

        Card *c = made ? made : cardForEntity(first);
        if (made) made->bindPrimary(first);
        if (!c) {
            // A real EntityKind with no card type yet - NUMBER, TEXT, CLIMATE,
            // WEATHER. Saying so beats rendering it as something it is not.
            Serial.printf("[Cards] spec %u: no card type for \"%s\"\n",
                          (unsigned)i, cs.primaries[firstIdx]);
            return nullptr;
        }

        for (uint8_t k = (uint8_t)(firstIdx + 1); k < CARD_PRIMARY_MAX; k++) {
            if (!cs.primaries[k]) continue;
            if (const Entity *e = reg.find(cs.primaries[k])) c->bindPrimary(e);
        }
        for (uint8_t k = 0; k < CARD_SECONDARY_MAX; k++) {
            if (!cs.secondaries[k]) continue;
            if (const Entity *e = reg.find(cs.secondaries[k])) c->bindSecondary(e);
        }

        if (cardIdUsable(cs.id)) c->setId(cs.id);
        if (cs.label) c->setLabel(cs.label);
        // THE CARD LABEL (2.10d, K44): the dashboard's area is the card's
        // group unless the owner chose another - CardGroups applies it, with
        // the group's colour (its colour-by-name unless one was saved, so two
        // cards in one area always agree and the colour survives a reboot).
        c->setSheetArea(cs.area);
        CardGroups::apply(*c, spec.areaColor);
        c->setHeaderStyle(cs.header == CARD_HDR_INHERIT
                          ? spec.headerDefault
                          : (CardHeaderStyle)cs.header);
        c->setVariant(cs.variant == CardVariant::VAR_AUTO ? spec.variantDefault
                                                          : cs.variant);
        c->setPlacement(cs.place);
        c->setTempUnit(cs.tempUnit == TempUnit::TEMP_INHERIT ? spec.tempUnit
                                                             : cs.tempUnit);
        // A NAMED CARD SAYS CUSTOM (owner, 2026-10-09, K49): a name the
        // dashboard gives ("Desk" for HA's "Office") is a custom name, and
        // the Entity label row says so. A card it does not name inherits.
        CardLabel lmDef = cs.labelMode == CardLabel::LBL_INHERIT ? spec.labelMode : cs.labelMode;
        if (lmDef == CardLabel::LBL_INHERIT && cs.label && cs.label[0]) lmDef = CardLabel::LBL_NAME;
        c->setLabelMode(lmDef);
        c->setGroupOn(cs.groupOn);

        // The owner's saved changes, over the spec (2.10d, Settings.h): the
        // file is the top layer. Read every time the page is built, which is
        // what makes a choice survive a page swipe as well as a reboot.
        if (c->hasId()) {
            char v[ENTITY_NAME_MAX];
            GroupOn g;
            SceneShow ss;
            CardLabel lm;
            if (Settings::card(c->id(), "active", v, sizeof(v)) && groupOnFromName(v, g)) c->setGroupOn(g);
            if (Settings::card(c->id(), "scenes", v, sizeof(v)) && sceneShowFromName(v, ss)) c->setSceneShow(ss);
            const bool lmSaved = Settings::card(c->id(), "label", v, sizeof(v)) && cardLabelFromName(v, lm);
            if (lmSaved) c->setLabelMode(lm);
            // A custom name set on the device replaces the dashboard's (K40),
            // and makes the card Custom unless a label choice was saved (K49).
            if (Settings::card(c->id(), "name", v, sizeof(v)) && v[0]) {
                c->setLabel(v);
                if (!lmSaved) c->setLabelMode(CardLabel::LBL_NAME);
            }
            TapAction ta;
            if (Settings::card(c->id(), "tap", v, sizeof(v)) && tapActionFromName(v, ta)) c->setTapAction(ta);
            TempUnit tu;   // SETTINGS' Units row (2.10e round 2)
            if (Settings::card(c->id(), "units", v, sizeof(v)) && tempUnitFromName(v, tu)) c->setTempUnit(tu);
            char ref[ENTITY_TOPIC_MAX];
            if (Settings::card(c->id(), "tap_scene", ref, sizeof(ref))) c->setTapScene(ref);
            CardVariant cv;   // Card UI's Card density (2.8 round 3, D-28)
            if (Settings::card(c->id(), "density", v, sizeof(v)) && cardVariantFromName(v, cv)) c->setVariant(cv);
        }
        if (cs.longStaleMs) c->setLongStaleMs(cs.longStaleMs);
        if (cs.paused)      c->setPaused(true);
        return c;
    }
}

// A GROUP CARD MADE ON THE DEVICE (2.11, K88-K92): its presentation is the
// saved "present", else the dashboard's. As a CLUSTER its members become mini
// tiles (the visible ones) and header values (the `slots`); it asks for as many
// cells as its tiles need (Layout::clusterSpan(), until the arranger). A cluster
// that cannot fit on this board is shown as its ONE CARD (K92 rule 6): the
// members that switch together - lights and switches (rule 5) - behind one tile
// whose tap acts on them all.
Card *CardPage::makeGroupCard(const CardSpec &cs, const PageSpec &spec, EntityRegistry &reg) {
    char v[64];
    const bool hasId = cardIdUsable(cs.id);
    GroupPresent pres = cs.present;
    if (hasId && Settings::card(cs.id, "present", v, sizeof(v))) groupPresentFromName(v, pres);

    // ITS MEMBERS: the dashboard's, then those added on the device (K91; CardAdded).
    const CardSpec *mem[GROUP_MEMBERS_MAX];
    uint8_t nMem = 0;
    for (uint8_t k = 0; k < cs.memberCount && nMem < GROUP_MEMBERS_MAX; k++) mem[nMem++] = &cs.members[k];
    if (hasId) nMem += CardAdded::membersIn(cs.id, mem + nMem, (uint8_t)(GROUP_MEMBERS_MAX - nMem));

    // The header's members, by id, highest priority first: the saved list, else the dashboard's.
    const char *slotIds[CLUSTER_SLOTS_MAX] = {nullptr};
    char saved[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1] = "";
    if (hasId && Settings::card(cs.id, "slots", saved, sizeof(saved))) {
        uint8_t k = 0;
        for (char *p = saved; *p && k < CLUSTER_SLOTS_MAX;) {
            slotIds[k++] = p;
            char *comma = strchr(p, ',');
            if (!comma) break;
            *comma = '\0';
            p = comma + 1;
        }
    } else {
        for (uint8_t k = 0; k < CLUSTER_SLOTS_MAX; k++) slotIds[k] = cs.slots[k];
    }
    // A MEMBER MOVED OUT TO THE PAGE ("hidden": "moved", the sprint after round
    // 7): its card is on the page now, so it is no tile, no header value, and
    // not behind the one card - still a member, so Members lists it.
    auto movedOut = [&](const CardSpec &m) { return cardIdUsable(m.id) && CardAdded::isMovedToPage(m.id); };
    auto slotOf = [&](const char *id) -> int8_t {
        if (id && cardIdUsable(id) && CardAdded::isMovedToPage(id)) return -1;
        for (uint8_t k = 0; k < CLUSTER_SLOTS_MAX; k++)
            if (id && slotIds[k] && !strcmp(slotIds[k], id)) return (int8_t)k;
        return -1;
    };
    // A member's tile: shown unless it is in the header (K92 rule 7); "hidden"
    // saved on it says otherwise either way (K104's Visible).
    auto tileOf = [&](const CardSpec &m) -> bool {
        bool tile = slotOf(m.id) < 0;
        char h[8];
        if (cardIdUsable(m.id) && Settings::card(m.id, "hidden", h, sizeof(h))) {
            if (strcmp(h, "no")) tile = false;   // "yes", or "moved"
            else                 tile = true;
        }
        return tile;
    };

    if (pres == GroupPresent::PRESENT_CLUSTER) {
        uint8_t nTiles = 0;
        for (uint8_t k = 0; k < nMem; k++)
            if (mem[k]->primaries[0] && reg.find(mem[k]->primaries[0]) && tileOf(*mem[k])) nTiles++;
        Layout::ClusterGeom g;
        g.cellW   = widthOf(_sub) > 0 ? widthOf(_sub) : (int32_t)UI::grid().cellW;
        g.cellH   = (int32_t)UI::grid().cellH;
        g.gap     = _gap;
        g.chromeW = ClusterCard::chromeW();
        g.chromeH = ClusterCard::chromeH();   // whole cells: no tag's room (round 4, below)
        g.tileGap = UI::mm(UI::shape().C_TILE_GAP);
        g.minTile = UI::mm(UI::shape().C_TILE_MIN);
        const uint8_t cellCols = _uCols / _sub ? _uCols / _sub : 1;
        const Layout::ClusterSpan sp = Layout::clusterSpan(nTiles, g, cellCols, _maxRows);
        if (sp.fits) {
            ClusterCard *cl = new ClusterCard();
            for (uint8_t k = 0; k < nMem; k++) {
                Card *m = makeCard(*mem[k], spec, reg, k);
                if (!m) continue;
                m->setMini(true);
                m->setHeaderStyle(CardHeaderStyle::HDR_NONE);
                cl->addMember(m, tileOf(*mem[k]), slotOf(mem[k]->id));
            }
            if (hasId) cl->setId(cs.id);
            if (cs.label) cl->setLabel(cs.label);
            cl->setSheetArea(cs.area);
            CardGroups::apply(*cl, spec.areaColor);
            // A CLUSTER FILLS ITS CELLS (owner, round 4): Float and Tag take room
            // from a solo card for its label, and a cluster has none - its header
            // names it. Round 1 kept the tag's room above it to line its top up
            // with its neighbours', and the owner saw its tiles shrink for it.
            cl->setHeaderStyle(CardHeaderStyle::HDR_NONE);
            cl->setShowArea(false);
            CardPlacement pl = cs.place;
            pl.prefSpanX = pl.minSpanX = (uint8_t)(sp.cols * _sub);
            pl.prefSpanY = pl.minSpanY = (uint8_t)(sp.rows * _sub);
            cl->setPlacement(pl);
            // The header's style (K102): the cluster's own, else the page's, else the spec's.
            ClusterStyle st = spec.clusterStyle;
            if (spec.slug && Settings::page(spec.slug, "cl_style", v, sizeof(v))) clusterStyleFromName(v, st);
            if (hasId && Settings::card(cs.id, "cl_style", v, sizeof(v))) clusterStyleFromName(v, st);
            cl->setStyle(st);
            cl->setIcons(!(hasId && Settings::card(cs.id, "cl_icons", v, sizeof(v)) && !strcmp(v, "off")));
            // ITS TAP (round 6): the one chosen in SETTINGS, read here because a
            // cluster is not made by makeCard() - it went back to Nothing whenever
            // the page was built again (the owner). Else Members, which lists every
            // member, hidden ones too (refines K107: opening a list switches nothing).
            TapAction ta = TapAction::TAP_MEMBERS;
            if (hasId && Settings::card(cs.id, "tap", v, sizeof(v))) tapActionFromName(v, ta);
            cl->setTapAction(ta);
            cl->setBinder(_binder);
            Serial.printf("[Cards] cluster %s: %u members, %u tiles, %ux%u cells\n",
                          cs.label ? cs.label : "?", (unsigned)cl->memberCount(), (unsigned)nTiles,
                          (unsigned)sp.cols, (unsigned)sp.rows);
            return cl;
        }
        Serial.printf("[Cards] cluster %s: %u tiles do not fit this page - shown as its card\n",
                      cs.label ? cs.label : "?", (unsigned)nTiles);
    }

    // AS ONE CARD: the members that switch together (lights and switches),
    // else every member, behind one tile. AS A SUMMARY (K98): every member -
    // one of a kind - with each one's name for the words under the icon.
    CardSpec one = {};
    one.id    = cs.id;
    one.label = cs.label;
    one.area  = cs.area;
    one.place = cs.place;
    one.groupOn = cs.groupOn;
    uint8_t n = 0;
    if (pres == GroupPresent::PRESENT_SUMMARY) {
        for (uint8_t k = 0; k < nMem && n < CARD_PRIMARY_MAX; k++)
            if (mem[k]->primaries[0] && reg.find(mem[k]->primaries[0]) && !movedOut(*mem[k]))
                one.primaries[n++] = mem[k]->primaries[0];
        if (!n) return nullptr;
        SummaryCard *sc = new SummaryCard();
        for (uint8_t k = 0; k < nMem; k++)
            if (mem[k]->primaries[0] && mem[k]->label) sc->setMemberName(mem[k]->primaries[0], mem[k]->label);
        Card *c = makeCard(one, spec, reg, 0, sc);
        // ITS TAP OPENS ITS MEMBERS (K98) - never a door's - unless the owner chose.
        char t[16];
        if (c && !(hasId && Settings::card(cs.id, "tap", t, sizeof(t)))) c->setTapAction(TapAction::TAP_MEMBERS);
        return c;
    }
    for (int pass = 0; pass < 2 && !n; pass++)
        for (uint8_t k = 0; k < nMem && n < CARD_PRIMARY_MAX; k++) {
            const char *pid = mem[k]->primaries[0];
            const Entity *e = pid ? reg.find(pid) : nullptr;
            if (!e || movedOut(*mem[k])) continue;
            const bool switches = e->desc.writable &&
                                  (e->desc.kind == EntityKind::LIGHT || e->desc.kind == EntityKind::SWITCH);
            if (pass == 0 && !switches) continue;
            one.primaries[n++] = pid;
        }
    return n ? makeCard(one, spec, reg, 0) : nullptr;
}

// ---------------------------------------------------------------------------

void CardPage::report() const {
    SystemReport::line("  Page: %ux%u units (subdivision %u), %u of %u cards placed%s",
                       (unsigned)_uCols, (unsigned)_uRows, (unsigned)_sub,
                       (unsigned)_placed, (unsigned)_n, _arranged ? ", by the arranger" : "");
    SystemReport::line("  Arranger: %s", PageArranger::lastText());
    for (uint8_t i = 0; i < _n; i++) {
        const Card *c = _cards[i];
        if (!c) continue;
        const CardPlacement &p = c->placement();
        if (_slot[i].placed) {
            SystemReport::line("    %-14s @ %u,%u  span %ux%u (min %ux%u) pri %3u  %-7s %s%s",
                               c->typeName(),
                               (unsigned)_slot[i].col,   (unsigned)_slot[i].row,
                               (unsigned)_slot[i].spanX, (unsigned)_slot[i].spanY,
                               (unsigned)p.minSpanX, (unsigned)p.minSpanY,
                               (unsigned)p.priority,
                               cardVariantName(c->variant()),
                               cardStateName(c->state()),
                               _slot[i].pinFail ? "  [pin rejected]" : "");
        } else {
            SystemReport::line("    %-14s DROPPED - no room  span %ux%u pri %3u",
                               c->typeName(),
                               (unsigned)p.prefSpanX, (unsigned)p.prefSpanY,
                               (unsigned)p.priority);
        }
    }
}

// ---------------------------------------------------------------------------
// Card ids - see PageSpec.h. Duplicates are remembered by pointer to the first
// spec's string and compared by content, so both copies are refused.
// ---------------------------------------------------------------------------
namespace {
constexpr uint8_t ID_DUPES_MAX = 16;
const char *s_idDupes[ID_DUPES_MAX] = {};
uint8_t     s_idDupeCount = 0;
}

bool cardIdWellFormed(const char *id) {
    if (!id || !*id) return false;
    size_t n = 0;
    for (const char *p = id; *p; p++, n++) {
        const char ch = *p;
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
        if (!ok || n >= CARD_ID_MAX) return false;
    }
    return true;
}

uint8_t checkCardIds(const PageSpec *const *pages, uint8_t n) {
    s_idDupeCount = 0;
    uint8_t problems = 0, total = 0;
    // Every card id seen so far, a group card's members among them (2.11).
    constexpr uint8_t SEEN_MAX = 128;
    const char *seen[SEEN_MAX];
    uint8_t nSeen = 0;
    for (uint8_t pi = 0; pi < n; pi++) {
        const PageSpec *pg = pages[pi];
        if (!pg) continue;
        uint8_t ci = 0;
        forEachCardSpec(*pg, [&](const CardSpec &cs) {
            const uint8_t here = ci++;
            total++;
            const char *what = cs.label ? cs.label : (cs.primaries[0] ? cs.primaries[0] : "?");
            if (!cs.id || !*cs.id) {
                Serial.printf("[CardIds] %s card %u (%s) has no id - its settings cannot be saved\n",
                              pg->slug, (unsigned)here, what);
                problems++;
                return;
            }
            if (!cardIdWellFormed(cs.id)) {
                Serial.printf("[CardIds] %s card %u (%s): id \"%s\" is not lowercase letters, digits and _ "
                              "(at most %u) - its settings cannot be saved\n",
                              pg->slug, (unsigned)here, what, cs.id, (unsigned)CARD_ID_MAX);
                problems++;
                return;
            }
            bool dupe = false;
            for (uint8_t k = 0; k < nSeen && !dupe; k++) dupe = !strcmp(seen[k], cs.id);
            if (dupe) {
                Serial.printf("[CardIds] id \"%s\" is used twice (again on %s, card %u, %s) - "
                              "neither card's settings can be saved\n", cs.id, pg->slug, (unsigned)here, what);
                problems++;
                if (s_idDupeCount < ID_DUPES_MAX) s_idDupes[s_idDupeCount++] = cs.id;
            } else if (nSeen < SEEN_MAX) {
                seen[nSeen++] = cs.id;
            }
        });
    }
    Serial.printf("[CardIds] %u cards on %u pages, %u id problem%s\n",
                  (unsigned)total, (unsigned)n, (unsigned)problems, problems == 1 ? "" : "s");
    return problems;
}

bool cardIdUsable(const char *id) {
    if (!cardIdWellFormed(id)) return false;
    for (uint8_t i = 0; i < s_idDupeCount; i++)
        if (strcmp(s_idDupes[i], id) == 0) return false;
    return true;
}
