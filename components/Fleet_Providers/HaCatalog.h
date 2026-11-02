#pragma once
#ifndef HA_CATALOG_H
#define HA_CATALOG_H

#include <Arduino.h>
#include <atomic>
#include <mutex>

// ---------------------------------------------------------------------------
// WHAT HOME ASSISTANT OFFERS THE BOARDS, 2.13 (DECISIONS K140; design/ha-cards.md
// section 2.4): the entities the owner labels "fleet", each with its area, and
// HA's floors and areas in order - what the pickers list.
//
// Data only. HaProvider asks HA for it (one render_template, answered once and
// cancelled) and hands the answer to parse() on the websocket task; the pickers
// read it on the LVGL thread between lock() and unlock(). Nothing here knows
// about cards, groups or the settings file - what is new, and what a board
// does about it, is the UI's (UI/HaOffer).
//
// In PSRAM: ~250 bytes an item, so 160 items are 40 KB.
// ---------------------------------------------------------------------------

class HaCatalog {
public:
    struct Area {
        char id[32];
        char name[40];
        char floor[32];       // the floor's id; "" for an area on no floor
        char floorName[32];
        uint8_t items;        // how many fleet entities are in it
    };
    struct Item {
        char ref[72];         // HA's entity id
        char name[48];        // friendly_name, or the id
        char areaId[32];      // "" for no area
        char dc[20];          // device_class
        char unit[12];        // unit_of_measurement
        char state[24];
    };
    static constexpr uint8_t AREAS_MAX = 48;
    static constexpr uint8_t ITEMS_MAX = 160;

    bool begin();   // the storage, once; false without memory

    // The template HaProvider sends: a render_template's "template" value,
    // single-quoted inside so the JSON around it needs no escapes.
    static const char *templateText();

    // The answer, on the websocket task. Replaces everything under the lock.
    void parse(const char *result);

    // Counts each parse(): a picker or a mark compares it with the one it saw.
    uint32_t epoch() const { return _epoch.load(); }
    bool     ready() const { return _epoch.load() != 0; }

    // Readers hold the lock across the reads (LVGL thread; keep it short).
    void lock()   const { _mx.lock(); }
    void unlock() const { _mx.unlock(); }
    uint8_t     areaCount() const { return _nAreas; }
    const Area &area(uint8_t i) const { return _areas[i]; }
    uint8_t     itemCount() const { return _nItems; }
    const Item &item(uint8_t i) const { return _items[i]; }
    int         findArea(const char *id) const;   // its index, or -1
    int         findItem(const char *ref) const;
    uint8_t     noArea() const;                   // items with no area

private:
    Area *_areas = nullptr;
    Item *_items = nullptr;
    uint8_t _nAreas = 0, _nItems = 0;
    std::atomic<uint32_t> _epoch{0};
    mutable std::mutex _mx;
};

#endif // HA_CATALOG_H
