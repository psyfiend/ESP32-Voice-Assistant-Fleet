#pragma once
#ifndef CLOCK_CARD_H
#define CLOCK_CARD_H
// ---------------------------------------------------------------------------
// THE CLOCK CARD (#100, a prototype): NINA's Classic clock face, time and date
// only, drawn into the largest square inside the card. A card with no entity -
// a dashboard asks for it by name (CardSpec::type = SPEC_CLOCK). Its own timer
// looks at the C library's clock once a second and redraws when the minute
// turns; TimeService sets that clock (SNTP, FLEET_TZ).
//
// FROM ESP32-P4-NINA-Display by chvvkumar
// (https://github.com/chvvkumar/ESP32-P4-NINA-Display), main/ui/nina_clock.c's
// Classic face, used with the author's permission for NON-COMMERCIAL use only:
// https://github.com/chvvkumar/ESP32-P4-NINA-Display/issues/326 (2026-10-06).
// That condition travels with this file and anything derived from it.
// ---------------------------------------------------------------------------
#include "Cards/Card.h"

class ClockCard : public Card {
public:
    ~ClockCard() override;
    const char *typeName() const override { return "clock"; }

protected:
    void buildBody(lv_obj_t *body) override;
    void render() override;

private:
    static void tickCb(lv_timer_t *t);

    lv_timer_t *_timer = nullptr;
    lv_obj_t   *_face = nullptr;
    lv_obj_t   *_day = nullptr, *_date = nullptr, *_year = nullptr;   // the top-left block
    lv_obj_t   *_time = nullptr, *_ampm = nullptr;
    int32_t     _shown = -1;   // the minute on the face (day of year * 1440 + minute); -1: none
};

#endif // CLOCK_CARD_H
