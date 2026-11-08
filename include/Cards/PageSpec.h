#pragma once
#ifndef PAGE_SPEC_H
#define PAGE_SPEC_H

#include "Cards/CardTypes.h"

struct SlotSpec;   // UI/HeaderSlots.h - a page's header slots

// ---------------------------------------------------------------------------
// What a page IS, as data.
//
// A page is data: a CardSpec is a card's construction written down. What a
// card spec expresses:
//
//   a domain          NOT here - see below
//   a header mode     band / tag / none, or inherit the page's
//   area on, colour on   two independent settings, on the page
//   spans and priority   CardPlacement, in UNITS - see CardTypes.h
//   a variant override   for when VAR_AUTO guesses wrong
//   staleness overrides  longStaleMs, per card
//
// THERE IS NO DOMAIN FIELD, deliberately. A card's type comes from its primary
// entity's EntityKind by way of cardForEntity(), which is how every card in the
// project is built. Writing the domain here as well would let a spec disagree
// with the registry about what a thing is, and there is no good answer to that
// disagreement.
// ---------------------------------------------------------------------------

// The page's header mode, rather than this card's own. Not a value of
// CardHeaderStyle because HDR_NONE is 0 and is a real choice, so zero-fill
// cannot mean "inherit" - it has to be a sentinel outside the enum's range.
static constexpr uint8_t CARD_HDR_INHERIT = 0xFF;

// Sub-grid units (D-4):
//
//   "Author the page as 3x3 CELLS, but allocate 6x6 UNITS underneath (each
//    cell = 2x2 units). Cards span in units. A quarter-page card is 3x3 units
//    - exactly expressible. A normal card is 2x2 units."
//
// So a page declares how finely its cells divide and every span, every
// coordinate and every minimum below is in UNITS. Two is the default because
// halves are the common case; a page needing thirds sets 3.
static constexpr uint8_t PAGE_SUBDIVISION_DEFAULT = 2;

// Sentinel for "no explicit coordinate - let the page flow me".
// Flow is the normal case and explicit placement is the exception, which is
// why this is the default rather than something a card has to ask for.
static constexpr int8_t PAGE_FLOW = -1;

// ---------------------------------------------------------------------------
// One card on a page.
//
// Entities are named by ID and must already be in the registry. The page skips
// - and reports - any spec whose primary is missing, which is what makes one
// fleet-wide dashboard safe: a board that does not have a given entity simply
// does not draw that card rather than failing to build a page.
// ---------------------------------------------------------------------------
struct CardSpec {
    // STABLE IDENTITY (DECISIONS K33). Saved settings are keyed by it,
    // so it never changes once a card exists - not when the card is relabelled,
    // moved to another page or given other entities - and is never reused.
    // Unique across every page on the device. Never shown in normal use.
    //
    // Made when the card is created: area and label, then the creation stamp,
    // <area>_<label>_<yymmdd>_<hhmm>, the label left out where it repeats the
    // area ("kitchen_261008_0310"); a second card with the same name in the
    // same minute takes a letter ("..._0310b"). Lowercase letters, digits and
    // '_' only. Checked at boot (checkCardIds()): a card with no id, a bad one
    // or a duplicate still draws, but its settings cannot be saved.
    const char *id = nullptr;

    // 1..6 primaries. More than one is the aggregate case - every light in a
    // room behind a single tile, which CardTypes.h explains at length. The
    // FIRST primary decides the card's type.
    const char *primaries[CARD_PRIMARY_MAX] = {nullptr};

    // Battery, last-seen: another entity of the SAME PHYSICAL DEVICE, capped
    // at two by cards.md section 1. Anything wanting a third is a group card.
    const char *secondaries[CARD_SECONDARY_MAX] = {nullptr};

    // The LOCATION, not the measurement - cards.md section 4. The tinted icon
    // already says what is being measured, so "Deck", never "Temperature".
    const char *label = nullptr;

    // nullptr leaves the card with no area, whatever the page's showArea says.
    const char *area = nullptr;

    uint8_t       header  = CARD_HDR_INHERIT;
    CardVariant   variant = CardVariant::VAR_AUTO;
    CardPlacement place   = {};
    TempUnit      tempUnit = TempUnit::TEMP_INHERIT;

    // What the line under a state card's hero says. See CardLabel.
    CardLabel     labelMode = CardLabel::LBL_INHERIT;

    // With several primaries: on when any of them is, or only when all are.
    GroupOn       groupOn = GroupOn::GROUP_ON_ANY;

    // 0 = use cardLongStaleMs() for this entity's kind.
    uint32_t longStaleMs = 0;

    bool paused = false;

    // A GROUP CARD MADE ON THE DEVICE (K88-K92): no primaries of its own,
    // its MEMBERS instead - each a card of its own, with its own id (K33) and
    // settings, wearing the group card's group (K92 rule 2). One level only: a
    // member is never a group card made here (rule 3). `present` is how it is
    // shown (the saved "present" wins); `slots` the members that fill a
    // cluster's header, by id, highest priority first (K101, K104) - a member in
    // the header is not also a tile unless it is made visible (K92 rule 7).
    const CardSpec *members     = nullptr;
    uint8_t         memberCount = 0;
    GroupPresent    present     = GroupPresent::PRESENT_CARD;
    const char     *slots[CLUSTER_SLOTS_MAX] = {nullptr};
    // BUILT IN (K171): on every board and never hidden from its page - the
    // Fleet page's Panel group. Visibility offers no Hide for it.
    bool            builtIn     = false;
};

// Every card a page's spec holds, in order: each card, and right after a group
// card its members. For whatever must see every card id or every entity a page
// names (the ids' check, the settings file's claims, the groups' HA areas).
template <typename F> void forEachCardSpec(const struct PageSpec &page, F fn);

// ---------------------------------------------------------------------------
// A page of cards.
// ---------------------------------------------------------------------------
struct PageSpec {
    // STABLE IDENTITY, and it is append-only.
    //
    // The rule (from the NINA project's page_registry.h, docs/
    // REFERENCE_PROJECTS.md): a page has a stable numeric id AND a stable
    // string slug, a retired id is reserved forever and never reused, and
    // reordering the table never renumbers anything. A saved setting persists
    // "which page", so a refactor that renumbers pages would silently re-point
    // a user's dashboard at a different one.
    //
    // id   - internal, never reused, never reordered
    // slug - the external key. What the settings file or a URL holds
    uint8_t     id   = 0;
    const char *slug = nullptr;

    const char *title = nullptr;

    const CardSpec *cards = nullptr;
    uint8_t         count = 0;

    // The default every card inherits unless it overrides.
    CardHeaderStyle headerDefault = CardHeaderStyle::HDR_TAG;

    // Compact-vs-full for the whole page.
    //
    // VAR_AUTO doubles as the inherit sentinel, which is exactly right here
    // rather than a shortcut: a card whose own variant is VAR_AUTO is saying
    // "derive it", and so is a page whose default is VAR_AUTO. The two mean
    // the same thing, so there is nothing for a separate INHERIT value to
    // express - unlike CARD_HDR_INHERIT, where "tag" and "inherit" are
    // genuinely different answers.
    CardVariant     variantDefault = CardVariant::VAR_AUTO;

    // Two independent settings: the colour is available whether or not the
    // area's NAME is displayed.
    bool showArea  = true;
    bool areaColor = true;

    uint8_t  subdivision = PAGE_SUBDIVISION_DEFAULT;
    TempUnit tempUnit    = TempUnit::TEMP_INHERIT;
    CardLabel labelMode  = CardLabel::LBL_INHERIT;

    // THE PAGE'S HEADER (UI/HeaderSlots.h): its slots, each with its place
    // (nullptr: the ones every page has, HEADER_DEFAULT), and whether it shows.
    // `headerShown` is the compiled default; the owner's choice for the page is
    // saved by its slug and wins (Settings: pages.<slug>.header). A full-screen
    // page - the Clock page to come (K82) - is a page with headerShown false.
    const SlotSpec *headerSlots = nullptr;
    uint8_t  headerSlotCount = 0;
    bool     headerShown     = true;

    // Its clusters' header style (K102); the saved pages.<slug>.cl_style
    // wins, and a cluster's own over both.
    ClusterStyle clusterStyle = ClusterStyle::CLSTYLE_SWATCH;
};

template <typename F> void forEachCardSpec(const PageSpec &page, F fn) {
    for (uint8_t i = 0; i < page.count; i++) {
        const CardSpec &cs = page.cards[i];
        fn(cs);
        for (uint8_t k = 0; k < cs.memberCount && cs.members; k++) fn(cs.members[k]);
    }
}

// ---------------------------------------------------------------------------
// Card ids. checkCardIds() runs once at boot over every page: it logs
// each card with no id, a malformed one or one used twice, and returns how many
// problems it found. cardIdUsable() is then false for those, and such a card
// draws but is never given its id, so nothing is ever saved under an id that
// could mean two cards. Implemented in CardPage.cpp.
// ---------------------------------------------------------------------------
static constexpr uint8_t CARD_ID_MAX = 47;   // characters, without the terminator

bool    cardIdWellFormed(const char *id);
uint8_t checkCardIds(const PageSpec *const *pages, uint8_t n);
bool    cardIdUsable(const char *id);

// Author-facing span constants, for the default subdivision of 2.
//
// Spans are in units and a normal card is 2x2, which reads oddly enough in a
// table of literals to be worth naming. A page that changes `subdivision` must
// not use these - it is saying its cells divide differently.
static constexpr uint8_t U_CELL = PAGE_SUBDIVISION_DEFAULT;      // one whole cell
static constexpr uint8_t U_HALF = PAGE_SUBDIVISION_DEFAULT / 2;  // half a cell
static constexpr uint8_t U_2    = U_CELL * 2;
static constexpr uint8_t U_3    = U_CELL * 3;

#endif // PAGE_SPEC_H
