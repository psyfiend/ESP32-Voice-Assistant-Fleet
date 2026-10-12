#include "Cards/CardAdded.h"
#include "Cards/Card.h"
#include "Cards/CardGroups.h"
#include "Cards/PageSpec.h"
#include "Entity.h"
#include "Settings.h"
#include <Arduino.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// See CardAdded.h.

namespace {

struct Rec {
    char    id[CARD_ID_MAX + 1];
    char    page[24];
    char    after[CARD_ID_MAX + 1];
    char    entity[ENTITY_ID_MAX];
    char    area[ENTITY_SHORT_MAX];
    uint8_t pri;
    bool    removed;
};

Rec      s_rec[CardAdded::MAX];
CardSpec s_spec[CardAdded::MAX];   // each record as a spec - built once, never moved
uint8_t  s_n = 0;
char     s_page[24] = "";
bool     s_changed = false;
bool   (*s_compiled)(const char *) = nullptr;

void copy(char *dst, size_t cap, const char *src) { snprintf(dst, cap, "%s", src ? src : ""); }

// The record's spec: what a compiled card would say, from the record.
void specOf(uint8_t i) {
    const Rec &r = s_rec[i];
    CardSpec cs{};
    cs.id           = r.id;
    cs.primaries[0] = r.entity;
    cs.area         = r.area[0] ? r.area : nullptr;
    cs.label        = nullptr;                    // Inherit: HA's name (K65)
    cs.labelMode    = CardLabel::LBL_INHERIT;
    cs.place.priority = r.pri;
    s_spec[i] = cs;
}

void readOne(const char *id, const char *page, void *) {
    if (s_n >= CardAdded::MAX || !cardIdWellFormed(id)) return;
    Rec &r = s_rec[s_n];
    r = Rec{};
    copy(r.id, sizeof(r.id), id);
    copy(r.page, sizeof(r.page), page);
    char v[48];
    Settings::card(id, "after", v, sizeof(v));  copy(r.after, sizeof(r.after), v);
    Settings::card(id, "entity", v, sizeof(v)); copy(r.entity, sizeof(r.entity), v);
    Settings::card(id, "area", v, sizeof(v));   copy(r.area, sizeof(r.area), v);
    r.pri = Settings::card(id, "pri", v, sizeof(v)) ? (uint8_t)atoi(v) : 128;
    r.removed = Settings::card(id, "removed", v, sizeof(v)) && !strcmp(v, "yes");
    if (!r.entity[0]) return;   // nothing to show: not taken
    specOf(s_n);
    s_n++;
}

bool idTaken(const char *id) {
    if (s_compiled && s_compiled(id)) return true;
    for (uint8_t i = 0; i < s_n; i++) if (!strcmp(s_rec[i].id, id)) return true;
    char v[8];
    return Settings::card(id, "page", v, sizeof(v));   // one this firmware could not take
}

// Lowercase letters and digits, '_' between words, at most `cap - 1`.
void slug(const char *in, char *out, size_t cap) {
    size_t k = 0;
    for (const char *p = in ? in : ""; *p && k < cap - 1; p++) {
        const char ch = (char)tolower((unsigned char)*p);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) out[k++] = ch;
        else if (k && out[k - 1] != '_') out[k++] = '_';
    }
    while (k && out[k - 1] == '_') k--;
    out[k] = '\0';
}

// K33: <area>_<label>_<yymmdd>_<hhmm>, the label dropped where it repeats the
// area (and a leading area word dropped from it: "Office lamp" in Office is
// "office_lamp", not "office_office_lamp"), a letter for a same-minute twin.
bool makeId(const char *area, const char *label, char *id, size_t cap) {
    char a[14], l[18], base[CARD_ID_MAX + 1];
    slug(area, a, sizeof(a));
    slug(label, l, sizeof(l));
    const size_t an = strlen(a);
    const char *lp = l;
    if (an && !strncmp(l, a, an) && (l[an] == '_' || l[an] == '\0')) lp = l[an] ? l + an + 1 : l + an;
    if (a[0] && lp[0]) snprintf(base, sizeof(base), "%s_%s", a, lp);
    else               snprintf(base, sizeof(base), "%s", a[0] ? a : lp[0] ? lp : "card");
    char stamp[16] = "000000_0000";
    time_t now = time(nullptr);
    struct tm tmv;
    if (now > 1700000000 && localtime_r(&now, &tmv)) strftime(stamp, sizeof(stamp), "%y%m%d_%H%M", &tmv);
    for (char suffix = 0; suffix <= 'z'; suffix = suffix ? (char)(suffix + 1) : 'b') {
        if (suffix) snprintf(id, cap, "%s_%s%c", base, stamp, suffix);
        else        snprintf(id, cap, "%s_%s", base, stamp);
        if (cardIdWellFormed(id) && !idTaken(id)) return true;
    }
    return false;
}

}  // namespace

namespace CardAdded {

void begin(bool (*compiledId)(const char *id)) {
    s_compiled = compiledId;
    s_n = 0;
    Settings::forEachCardWith("page", readOne, nullptr);
    uint8_t live = 0;
    for (uint8_t i = 0; i < s_n; i++) if (!s_rec[i].removed) live++;
    Serial.printf("[CardAdded] %u card%s added on this device (%u removed)\n",
                  (unsigned)s_n, s_n == 1 ? "" : "s", (unsigned)(s_n - live));
}

void setPage(const char *slugName) { copy(s_page, sizeof(s_page), slugName); }

uint8_t merge(const PageSpec &page, CardSpec *out, uint8_t cap) {
    uint8_t n = 0;
    bool done[MAX] = {};
    const char *pg = page.slug ? page.slug : "";
    // Each card, then whatever was added after it - and after those, in turn.
    auto followers = [&](const char *id, auto &&self) -> void {
        if (!id) return;
        for (uint8_t i = 0; i < s_n && n < cap; i++) {
            const Rec &r = s_rec[i];
            if (done[i] || r.removed || strcmp(r.page, pg) || strcmp(r.after, id)) continue;
            done[i] = true;
            out[n++] = s_spec[i];
            self(r.id, self);
        }
    };
    for (uint8_t c = 0; c < page.count && n < cap; c++) {
        out[n++] = page.cards[c];
        followers(page.cards[c].id, followers);
    }
    // Their card is gone: at the end.
    for (uint8_t i = 0; i < s_n && n < cap; i++) {
        if (done[i] || s_rec[i].removed || strcmp(s_rec[i].page, pg)) continue;
        done[i] = true;
        out[n++] = s_spec[i];
        followers(s_rec[i].id, followers);
    }
    return n;
}

bool known(const char *cardId) {
    if (!cardId) return false;
    for (uint8_t i = 0; i < s_n; i++) if (!strcmp(s_rec[i].id, cardId)) return true;
    return false;
}

bool isAdded(const char *cardId) {
    if (!cardId) return false;
    for (uint8_t i = 0; i < s_n; i++) if (!s_rec[i].removed && !strcmp(s_rec[i].id, cardId)) return true;
    return false;
}

const char *addedFor(const char *parentId, const char *entityId) {
    if (!parentId || !entityId) return nullptr;
    for (uint8_t i = 0; i < s_n; i++) {
        const Rec &r = s_rec[i];
        if (!r.removed && !strcmp(r.after, parentId) && !strcmp(r.entity, entityId)) return r.id;
    }
    return nullptr;
}

bool add(const Card &parent, const Entity &member, char *idOut, size_t cap) {
    if (s_n >= MAX || !parent.hasId() || !s_page[0]) return false;
    Rec &r = s_rec[s_n];
    r = Rec{};
    if (!makeId(parent.sheetArea(), member.desc.name, r.id, sizeof(r.id))) return false;
    copy(r.page, sizeof(r.page), s_page);
    copy(r.after, sizeof(r.after), parent.id());
    copy(r.entity, sizeof(r.entity), member.desc.id);
    copy(r.area, sizeof(r.area), parent.sheetArea());
    r.pri = parent.placement().priority;
    r.removed = false;

    char pri[8];
    snprintf(pri, sizeof(pri), "%u", (unsigned)r.pri);
    Settings::setCard(r.id, "page", r.page);
    Settings::setCard(r.id, "after", r.after);
    Settings::setCard(r.id, "entity", r.entity);
    if (r.area[0]) Settings::setCard(r.id, "area", r.area);
    Settings::setCard(r.id, "pri", pri);
    // ITS PARENT'S GROUP (owner, K65): the card label's words, colour and type.
    CardGroups::Group g;
    if (CardGroups::ofCard(parent, g)) Settings::setCard(r.id, "group", g.id);

    specOf(s_n);
    s_n++;
    s_changed = true;
    if (idOut && cap) copy(idOut, cap, r.id);
    Serial.printf("[CardAdded] %s: %s after %s on %s\n", r.id, r.entity, r.after, r.page);
    return true;
}

bool remove(const char *cardId) {
    for (uint8_t i = 0; i < s_n; i++) {
        Rec &r = s_rec[i];
        if (r.removed || strcmp(r.id, cardId)) continue;
        r.removed = true;
        Settings::setCard(r.id, "removed", "yes");
        s_changed = true;
        Serial.printf("[CardAdded] %s removed (kept in the page's pool)\n", r.id);
        return true;
    }
    return false;
}

bool takeChanged() {
    const bool c = s_changed;
    s_changed = false;
    return c;
}

}  // namespace CardAdded
