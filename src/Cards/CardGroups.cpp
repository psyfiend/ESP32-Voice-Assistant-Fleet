// CardGroups - see CardGroups.h.
#include "Cards/CardGroups.h"
#include "Cards/Card.h"
#include "Cards/CardIcons.h"   // cardAreaColor(): the colour of a name
#include "Cards/PageSpec.h"
#include "Settings.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

const PageSpec *const *s_pages = nullptr;
uint8_t s_nPages = 0;

// The dashboard's areas, each once, in page order.
uint8_t sheetAreas(const char **out, uint8_t cap) {
    uint8_t n = 0;
    for (uint8_t p = 0; p < s_nPages; p++)
        for (uint8_t i = 0; i < s_pages[p]->count; i++) {
            const char *a = s_pages[p]->cards[i].area;
            if (!a || !a[0]) continue;
            bool seen = false;
            for (uint8_t k = 0; k < n && !seen; k++) seen = !strcmp(out[k], a);
            if (!seen && n < cap) out[n++] = a;
        }
    return n;
}

// A group id's name: a dashboard area's, or a custom group's. "" if unknown.
void nameOf(const char *gid, char *out, size_t cap) {
    out[0] = '\0';
    if (!strncmp(gid, CardGroups::SHEET_PREFIX, strlen(CardGroups::SHEET_PREFIX))) {
        const char *areas[32];
        const uint8_t n = sheetAreas(areas, 32);
        char id[48];
        for (uint8_t i = 0; i < n; i++) {
            CardGroups::idFor(CardGroups::SHEET_PREFIX, areas[i], id, sizeof(id));
            if (!strcmp(id, gid)) { snprintf(out, cap, "%s", areas[i]); return; }
        }
        return;
    }
    Settings::group(gid, "name", out, cap);
}

void setLabel(Card &c, const char *text, uint32_t color, bool areaColor) {
    c.setArea(text ? text : "");
    c.setAreaColor(areaColor ? color : 0);
}

}  // namespace

namespace CardGroups {

void setPages(const PageSpec *const *pages, uint8_t n) {
    s_pages  = pages;
    s_nPages = n;
}

void idFor(const char *prefix, const char *name, char *out, size_t cap) {
    if (!cap) return;
    size_t k = (size_t)snprintf(out, cap, "%s", prefix);
    for (const char *p = name ? name : ""; *p && k + 1 < cap; p++) {
        const char ch = (char)tolower((unsigned char)*p);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) out[k++] = ch;
        else if (k && out[k - 1] != '_') out[k++] = '_';
    }
    out[k < cap ? k : cap - 1] = '\0';
}

uint32_t colorOfGroup(const char *groupId, const char *name) {
    char hex[12];
    if (groupId && Settings::group(groupId, "color", hex, sizeof(hex)) && strlen(hex) == 6)
        return (uint32_t)strtoul(hex, nullptr, 16);
    return cardAreaColor(name);
}

uint32_t colorFor(const char *prefix, const char *name) {
    char id[48];
    idFor(prefix, name, id, sizeof(id));
    return colorOfGroup(id, name);
}

void apply(Card &c, bool areaColor) {
    c.setPageAreaColor(areaColor);
    c.setFollowHaArea(false);
    c.setLabelAlways(false);
    char v[48];
    if (!c.hasId() || !Settings::card(c.id(), "group", v, sizeof(v)) || !v[0]) {
        // Nothing chosen: the dashboard's area, as before 2.10d - with its
        // group's colour, which is its colour-by-name unless one was saved.
        setLabel(c, c.sheetArea(), colorFor(SHEET_PREFIX, c.sheetArea()), areaColor && c.sheetArea()[0]);
        return;
    }
    if (!strcmp(v, "ha_area")) {
        c.setFollowHaArea(true);
        setLabel(c, c.haArea(), colorFor(HA_PREFIX, c.haArea()), areaColor);
        return;
    }
    if (!strcmp(v, "own")) {
        // Its own words, in their own colour: no group's colour reaches it.
        char text[40];
        Settings::card(c.id(), "label_text", text, sizeof(text));
        c.setLabelAlways(true);
        setLabel(c, text, cardAreaColor(text[0] ? text : c.id()), areaColor);
        return;
    }
    if (!strcmp(v, "none")) { setLabel(c, "", 0, false); return; }
    char name[40];
    nameOf(v, name, sizeof(name));
    if (!name[0]) {
        // A group that has gone: the dashboard's area, until the owner chooses.
        setLabel(c, c.sheetArea(), colorFor(SHEET_PREFIX, c.sheetArea()), areaColor && c.sheetArea()[0]);
        return;
    }
    setLabel(c, name, colorOfGroup(v, name), areaColor);
}

uint8_t rowChoices(const Card &c, Choice *out, uint8_t cap, char *opts, size_t optsCap, uint16_t &sel) {
    uint8_t n = 0;
    size_t used = 0;
    opts[0] = '\0';
    auto add = [&](ChoiceKind k, const char *gid, const char *line) {
        if (n >= cap) return;
        used += snprintf(opts + used, optsCap > used ? optsCap - used : 0, "%s%s", n ? "\n" : "", line);
        out[n].kind = k;
        snprintf(out[n].gid, sizeof(out[n].gid), "%s", gid ? gid : "");
        n++;
    };
    char line[64];
    if (c.haArea()[0]) snprintf(line, sizeof(line), "HA area (%s)", c.haArea());
    else               snprintf(line, sizeof(line), "HA area");
    add(CH_HA_AREA, nullptr, line);

    const char *areas[32];
    const uint8_t na = sheetAreas(areas, 32);
    char id[48];
    for (uint8_t i = 0; i < na; i++) {
        idFor(SHEET_PREFIX, areas[i], id, sizeof(id));
        add(CH_GROUP, id, areas[i]);
    }
    struct Ctx { decltype(add) *adder; } ctx{ &add };
    Settings::forEachGroup([](const char *gid, const char *name, void *p) {
        if (!strncmp(gid, SHEET_PREFIX, strlen(SHEET_PREFIX)) || !strncmp(gid, HA_PREFIX, strlen(HA_PREFIX))) return;
        (*((Ctx *)p)->adder)(CH_GROUP, gid, name);
    }, &ctx);
    add(CH_OWN, nullptr, "Custom");
    add(CH_NONE, nullptr, "None");

    // What is chosen now: the saved choice, else the dashboard's area.
    char v[48] = "";
    if (c.hasId()) Settings::card(c.id(), "group", v, sizeof(v));
    if (!v[0]) {
        if (c.sheetArea()[0]) idFor(SHEET_PREFIX, c.sheetArea(), v, sizeof(v));
        else                  snprintf(v, sizeof(v), "none");
    }
    sel = 0;
    for (uint8_t i = 0; i < n; i++) {
        const bool hit = (out[i].kind == CH_HA_AREA && !strcmp(v, "ha_area")) ||
                         (out[i].kind == CH_OWN && !strcmp(v, "own")) ||
                         (out[i].kind == CH_NONE && !strcmp(v, "none")) ||
                         (out[i].kind == CH_GROUP && !strcmp(v, out[i].gid));
        if (hit) { sel = i; break; }
    }
    return n;
}

void choose(Card &c, const Choice &ch, bool areaColor) {
    if (!c.hasId()) return;
    switch (ch.kind) {
        case CH_HA_AREA: Settings::setCard(c.id(), "group", "ha_area"); break;
        case CH_GROUP:   Settings::setCard(c.id(), "group", ch.gid);    break;
        case CH_NONE:    Settings::setCard(c.id(), "group", "none");    break;
        case CH_OWN: {
            // Detached with the words it had (F2): they are its own now.
            Settings::setCard(c.id(), "group", "own");
            char text[40];
            if (!Settings::card(c.id(), "label_text", text, sizeof(text)))
                Settings::setCard(c.id(), "label_text", c.area());
            break;
        }
    }
    apply(c, areaColor);
}

}  // namespace CardGroups
