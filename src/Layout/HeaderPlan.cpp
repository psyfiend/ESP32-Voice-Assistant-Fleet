// The header's planner (2.8 round 2). See include/Layout/HeaderPlan.h.
#include "Layout/HeaderPlan.h"

namespace Layout {

namespace {

// A place's width with the forms chosen so far: its shown claims and the space
// between them.
int32_t placeWidth(const HeaderClaim *c, uint8_t n, const uint8_t *form, HeaderPlace p, int32_t space) {
    int32_t w = 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (c[i].place != p || form[i] >= c[i].forms) continue;
        w += c[i].width[form[i]];
        count++;
    }
    return count ? w + space * (count - 1) : 0;
}

}  // namespace

bool planHeader(const HeaderBar &bar, const HeaderClaim *c, uint8_t n, HeaderSpot *out) {
    if (n > HEADER_CLAIMS_MAX) n = HEADER_CLAIMS_MAX;
    uint8_t form[HEADER_CLAIMS_MAX];
    for (uint8_t i = 0; i < n; i++) {
        const uint8_t f = c[i].forms > HEADER_FORMS_MAX ? HEADER_FORMS_MAX : c[i].forms;
        form[i] = (c[i].wanted && f) ? 0 : f;   // == forms: not on the bar
    }

    bool fits = false;
    // Each pass takes one step from one claim, so this many passes always end it.
    for (uint16_t pass = 0; pass <= (uint16_t)n * HEADER_FORMS_MAX; pass++) {
        const int32_t L = placeWidth(c, n, form, HeaderPlace::PLACE_LEFT,   bar.space);
        const int32_t M = placeWidth(c, n, form, HeaderPlace::PLACE_MIDDLE, bar.space);
        const int32_t R = placeWidth(c, n, form, HeaderPlace::PLACE_RIGHT,  bar.space);
        const int32_t leftEnd    = bar.edge + L;
        const int32_t rightStart = bar.width - bar.edge - R;
        bool overL, overR;
        if (M > 0) {
            // The middle stands on the centre, whatever the sides hold.
            const int32_t midL = (bar.width - M) / 2;
            if (midL < 0) {
                overL = overR = true;   // the middle alone is wider than the bar
            } else {
                overL = L > 0 && leftEnd + bar.clear > midL;
                overR = R > 0 && rightStart - bar.clear < midL + M;
            }
        } else {
            // No middle: the two sides have the whole bar between them.
            overL = L > 0 && leftEnd > (R > 0 ? rightStart - bar.clear : bar.width - bar.edge);
            overR = R > 0 && rightStart < (L > 0 ? leftEnd + bar.clear : bar.edge);
        }
        if (!overL && !overR) { fits = true; break; }

        // The lowest keep among the claims that can help - the overflowing
        // side's and the middle's - gives way. Ties: the later claim.
        int best = -1;
        for (uint8_t i = 0; i < n; i++) {
            if (form[i] >= c[i].forms) continue;
            const HeaderPlace p = c[i].place;
            const bool helps = p == HeaderPlace::PLACE_MIDDLE ||
                               (p == HeaderPlace::PLACE_LEFT && overL) ||
                               (p == HeaderPlace::PLACE_RIGHT && overR);
            if (!helps || c[i].keep[form[i]] == KEEP_ALWAYS) continue;
            if (best < 0 || c[i].keep[form[i]] <= c[best].keep[form[best]]) best = i;
        }
        if (best < 0) break;   // nothing left that may give way
        form[best]++;
    }

    // Where each stands: the left from the bar's start, the right back from its
    // end, the middle on the centre.
    for (uint8_t i = 0; i < n; i++) {
        out[i] = HeaderSpot();
        out[i].form  = form[i];
        out[i].shown = form[i] < c[i].forms;
        if (out[i].shown) out[i].w = c[i].width[form[i]];
    }
    int32_t x = bar.edge;
    for (uint8_t i = 0; i < n; i++) {
        if (!out[i].shown || c[i].place != HeaderPlace::PLACE_LEFT) continue;
        out[i].x = x;
        x += out[i].w + bar.space;
    }
    x = bar.width - bar.edge;
    for (int i = n - 1; i >= 0; i--) {
        if (!out[i].shown || c[i].place != HeaderPlace::PLACE_RIGHT) continue;
        x -= out[i].w;
        out[i].x = x;
        x -= bar.space;
    }
    x = (bar.width - placeWidth(c, n, form, HeaderPlace::PLACE_MIDDLE, bar.space)) / 2;
    for (uint8_t i = 0; i < n; i++) {
        if (!out[i].shown || c[i].place != HeaderPlace::PLACE_MIDDLE) continue;
        out[i].x = x;
        x += out[i].w + bar.space;
    }
    return fits;
}

}  // namespace Layout
