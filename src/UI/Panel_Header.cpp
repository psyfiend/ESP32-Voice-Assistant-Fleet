#include "UI/Panel_Header.h"
#include "UI/UITokens.h"
#include "UI/UIToolkit.h"  // Semantic fonts
#include "ConnectivityManager.h"

Panel_Header::Panel_Header() {
    lbl_title  = NULL;
    btn_status = NULL;
    slots      = NULL;
}

void Panel_Header::init(lv_obj_t* parent, const char* title, ConnectivityManager* conn,
                        MqttManager* mqtt) {
    // Top Bar Container
    container = lv_obj_create(parent);
    lv_obj_set_size             (container, lv_pct(100), UIToolkit::systemHeaderPx());
    lv_obj_set_style_bg_color   (container, UI::c(UI::pal().SURFACE_ALT), 0);

    // Bottom Border Only (Blue Line)
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_border_width(container, UIToolkit::sc(2), UI::part(LV_PART_MAIN));
    lv_obj_set_style_border_side (container, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(container, UI::c(UI::pal().ACCENT), 0); // Cyan Blue

    lv_obj_set_style_radius     (container, 0, 0);
    lv_obj_set_style_pad_all    (container, 0, 0); // Remove padding so button hits edge
    lv_obj_set_style_pad_left   (container, UIToolkit::sc(15), 0); // Restore left pad for title
    lv_obj_set_style_pad_right  (container, UIToolkit::sc(5), 0); // Small pad for button

    lv_obj_set_flex_flow        (container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align       (container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag           (container, LV_OBJ_FLAG_SCROLLABLE);

    // Title Label
    lbl_title = lv_label_create(container);
    lv_label_set_text           (lbl_title, title);
    lv_obj_set_style_text_font  (lbl_title, UIToolkit::Font_PanelHeader, 0); // Semantic Font
    lv_obj_set_style_text_color (lbl_title, UI::c(UI::pal().ACCENT), 0);
    
    // Status Button Wrapper (Touch Hotspot) --
    btn_status = lv_obj_create(container);
    // Was sc(80) for one glyph; two need the room. Still one hotspot, because
    // the tap means "show me connectivity" whichever half you hit - splitting
    // it into two targets would make the smaller boards' glyphs harder to hit
    // than UI::minTouch() allows for no gain.
    lv_obj_set_size             (btn_status, UIToolkit::sc(116), lv_pct(100)); // Wide touch target
    lv_obj_set_style_bg_opa     (btn_status, LV_OPA_TRANSP, 0); // Invisible
    lv_obj_set_style_border_width(btn_status, 0, 0);
    lv_obj_set_style_pad_all    (btn_status, 0, 0);

    // Enable Clicking on the wrapper
    lv_obj_add_flag             (btn_status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag           (btn_status, LV_OBJ_FLAG_SCROLLABLE);

    // The status cluster. A flex row inside the hotspot rather than each glyph
    // centred on top of the other - which is what a second lv_obj_center()
    // call would have produced.
    //
    // Order is MQTT then WiFi, left to right, matching the Fleet Status Glyphs
    // artifact's own header mock. It also reads correctly as a dependency
    // chain: the link is nearest the edge and everything else rides on it.
    slots = lv_obj_create(btn_status);
    lv_obj_remove_style_all(slots);
    lv_obj_set_size(slots, LV_SIZE_CONTENT, lv_pct(100));
    lv_obj_center(slots);
    lv_obj_set_flex_flow(slots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(slots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(slots, UIToolkit::sc(8), 0);
    // The hotspot above owns the touch; these must not eat it.
    lv_obj_remove_flag(slots, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(slots, LV_OBJ_FLAG_SCROLLABLE);

    // Both glyphs are omitted rather than faked when their subsystem is absent
    // - a GUI-less or broker-less build should show nothing, not a grey lie.
    if (mqtt) mqttStatus.init(slots, mqtt);

    // Live connectivity glyph, inside the existing touch hotspot. Replaces the
    // old always-green LV_SYMBOL_WIFI label, which reported nothing.
    if (conn) connStatus.init(slots, conn);
}

void Panel_Header::tick() {
    // Self-throttling and change-guarded internally, so calling this every
    // loop costs a millis() compare in the common case. Both glyphs no-op
    // when their subsystem was never handed to init().
    connStatus.tick();
    mqttStatus.tick();
}
void Panel_Header::restyle() {
    if (!container) return;
    lv_obj_set_style_bg_color    (container, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_border_color(container, UI::c(UI::pal().ACCENT), 0);
    if (lbl_title) lv_obj_set_style_text_color(lbl_title, UI::c(UI::pal().ACCENT), 0);
    paintPage();
}

void Panel_Header::setPage(const char *title, uint8_t index, uint8_t count) {
    if (!container) return;

    if (!pageBox) {
        // OUT OF THE BAR'S FLEX ROW and centred on the bar itself, so it sits
        // in the true middle of the screen whatever the device name on the
        // left and the status glyphs on the right happen to measure.
        pageBox = lv_obj_create(container);
        lv_obj_remove_style_all(pageBox);
        lv_obj_add_flag       (pageBox, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_size       (pageBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow  (pageBox, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align (pageBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                               LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(pageBox, UIToolkit::sc(8), 0);
        lv_obj_remove_flag    (pageBox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag    (pageBox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align          (pageBox, LV_ALIGN_CENTER, 0, 0);

        pageTitle = lv_label_create(pageBox);
        lv_obj_set_style_text_font(pageTitle, UIToolkit::Font_PanelHeader, 0);

        pageDots = lv_obj_create(pageBox);
        lv_obj_remove_style_all(pageDots);
        lv_obj_set_size       (pageDots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow  (pageDots, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align (pageDots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                               LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(pageDots, UIToolkit::sc(5), 0);
        lv_obj_remove_flag    (pageDots, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_label_set_text(pageTitle, title ? title : "");

    // Rebuild the dots - there are at most a dozen, and it happens once per
    // page change. The ">12 pages" case is a label instead of dots.
    lv_obj_clean(pageDots);
    if (count > 1 && count <= PAGE_DOTS_MAX) {
        // CURRENT AND OTHER DIFFER BY SHAPE AND SIZE, NOT ONLY BY COLOUR. The
        // owner is a little colour-blind and found two same-sized dots in two
        // colours hard to tell apart. The current page is a larger filled dot;
        // the others are smaller hollow rings. Readable in greyscale.
        for (uint8_t i = 0; i < count; i++) {
            const bool cur = (i == index);
            const int32_t d = UIToolkit::sc(cur ? 11 : 8);
            lv_obj_t *dot = lv_obj_create(pageDots);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size        (dot, d, d);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(dot, cur ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(dot, cur ? 0 : UIToolkit::sc(2), 0);
        }
    } else if (count > PAGE_DOTS_MAX) {
        lv_obj_t *l = lv_label_create(pageDots);
        lv_label_set_text_fmt(l, "%u / %u", (unsigned)index + 1, (unsigned)count);
        lv_obj_set_style_text_font(l, UIToolkit::Font_Label, 0);
    }
    paintPage();

    // ROOM FOR THE DEVICE NAME, measured. On CYD_S3_3248 (320 px) the centred
    // page title landed on top of the device name. Where the name cannot have
    // a useful width left of the centred group, it gives way entirely and the
    // page group takes its place at the left - which is where the owner first
    // asked for the page title to go. Elsewhere it is shortened with "..."
    // only if it has to be.
    lv_obj_update_layout(container);
    const int32_t barW   = lv_obj_get_content_width(container);
    const int32_t boxW   = lv_obj_get_width(pageBox);
    const int32_t roomL  = barW / 2 - boxW / 2 - UIToolkit::sc(10);
    if (roomL >= UIToolkit::sc(60)) {
        lv_obj_clear_flag     (lbl_title, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
        lv_obj_set_width      (lbl_title, LV_SIZE_CONTENT);
        lv_obj_update_layout  (container);
        if (lv_obj_get_width(lbl_title) > roomL) lv_obj_set_width(lbl_title, roomL);
        lv_obj_align(pageBox, LV_ALIGN_CENTER, 0, 0);
    } else {
        lv_obj_add_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align   (pageBox, LV_ALIGN_LEFT_MID, 0, 0);
    }
}

void Panel_Header::paintPage() {
    if (!pageBox) return;
    const UIPalette &p = UI::pal();
    lv_obj_set_style_text_color(pageTitle, UI::c(p.TEXT), 0);
    const uint32_t n = lv_obj_get_child_count(pageDots);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(pageDots, i);
        // Which dot is current is decided by its SHAPE, in setPage(); this
        // only colours them. The filled dot takes the accent, the hollow rings
        // take TEXT rather than TEXT_DIM so a ring is still plainly visible.
        // The "3 / 15" label reads its colour from text_color.
        lv_obj_set_style_bg_color    (c, UI::c(p.ACCENT), 0);
        lv_obj_set_style_border_color(c, UI::c(p.TEXT), 0);
        lv_obj_set_style_text_color  (c, UI::c(p.TEXT_DIM), 0);
    }
}
