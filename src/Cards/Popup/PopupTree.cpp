// GET /popup?tree=1|page (2.12 P0, split from CardPopup.cpp at P1). Debug builds only.
#include "PopupState.h"

#ifdef DEBUG_POPUP
namespace popup {

// ---------------------------------------------------------------------------
// GET /popup?tree=1|page (2.12, P0 - docs/design/ui-foundation.md): LVGL's
// object tree, one line per object, for scripts/ui_golden.py. It is the
// refactor's safety net: geometry is the same every time for the same state,
// where a screenshot is not (values and times change minute to minute), so two
// dumps of one state compare line by line.
//
// A line, indented two spaces a level:
//   <class> @x,y wxh fl=<flags> st=<state> [ev=<codes>] [what it shows]
//     {<MAIN's style as drawn now>} [<part>.<state>{<what that state changes>}]
//     t=<text, newlines as |>
// The style is LVGL's own resolution - theme, shared and local styles together,
// as drawn - so a property moved from a local style to a shared one leaves the
// line as it was; one that changes value, or a state that stops changing it,
// does not. Only values that differ from LVGL's defaults are printed.
//
// tree=1 is the window and what goes with it (its panels, the dialog, the
// dropdowns' lists); never the sparks, which are random by design. tree=page
// is everything: the screen, the top layer and the system layer.
//
// Written into PSRAM by the LVGL thread; the handler sends it and frees it.
// The UI pauses while it is written (tens of ms).
// ---------------------------------------------------------------------------
constexpr size_t TREE_CAP = 384 * 1024;
char  *s_tree    = nullptr;
size_t s_treeLen = 0;
bool   s_treeCut = false;
bool   s_treeLists = false;   // tree=1: a dropdown's list follows it (it lives on the screen)

void treeOut(const char *fmt, ...) {
    if (!s_tree || s_treeLen >= TREE_CAP - 1) { s_treeCut = true; return; }
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(s_tree + s_treeLen, TREE_CAP - s_treeLen, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= TREE_CAP - s_treeLen) s_treeCut = true;
    s_treeLen = LV_MIN(TREE_CAP - 1, s_treeLen + (size_t)n);
}

// Text on one line: a newline becomes '|'.
void treeText(const char *t) {
    if (!t) return;
    for (; *t && s_treeLen < TREE_CAP - 1; t++) s_tree[s_treeLen++] = (*t == '\n') ? '|' : *t;
    s_tree[s_treeLen] = '\0';
}

// Short names, so a line stays readable; anything else prints as p<id>.
const char *treePropName(lv_style_prop_t p) {
    switch (p) {
        case LV_STYLE_WIDTH: return "w";                 case LV_STYLE_HEIGHT: return "h";
        case LV_STYLE_LENGTH: return "len";
        case LV_STYLE_TRANSFORM_WIDTH: return "trw";     case LV_STYLE_TRANSFORM_HEIGHT: return "trh";
        case LV_STYLE_MIN_WIDTH: return "minw";          case LV_STYLE_MAX_WIDTH: return "maxw";
        case LV_STYLE_MIN_HEIGHT: return "minh";         case LV_STYLE_MAX_HEIGHT: return "maxh";
        case LV_STYLE_TRANSLATE_X: return "tx";          case LV_STYLE_TRANSLATE_Y: return "ty";
        case LV_STYLE_X: return "x";                     case LV_STYLE_Y: return "y";
        case LV_STYLE_ALIGN: return "align";
        case LV_STYLE_PAD_TOP: return "pt";              case LV_STYLE_PAD_BOTTOM: return "pb";
        case LV_STYLE_PAD_LEFT: return "pl";             case LV_STYLE_PAD_RIGHT: return "pr";
        case LV_STYLE_PAD_ROW: return "prow";            case LV_STYLE_PAD_COLUMN: return "pcol";
        case LV_STYLE_MARGIN_TOP: return "mt";           case LV_STYLE_MARGIN_BOTTOM: return "mb";
        case LV_STYLE_MARGIN_LEFT: return "ml";          case LV_STYLE_MARGIN_RIGHT: return "mr";
        case LV_STYLE_BG_GRAD: return "grad";            case LV_STYLE_BG_GRAD_DIR: return "gdir";
        case LV_STYLE_BG_MAIN_OPA: return "gmopa";       case LV_STYLE_BG_GRAD_OPA: return "gopa";
        case LV_STYLE_BG_GRAD_COLOR: return "gcol";      case LV_STYLE_BG_MAIN_STOP: return "gmstop";
        case LV_STYLE_BG_GRAD_STOP: return "gstop";
        case LV_STYLE_BG_IMAGE_SRC: return "bgimg";      case LV_STYLE_BG_IMAGE_OPA: return "bgimgopa";
        case LV_STYLE_BG_IMAGE_RECOLOR_OPA: return "bgimgrcopa";
        case LV_STYLE_BG_IMAGE_RECOLOR: return "bgimgrc";
        case LV_STYLE_BORDER_WIDTH: return "bw";         case LV_STYLE_BORDER_COLOR: return "bc";
        case LV_STYLE_BORDER_OPA: return "bopa";         case LV_STYLE_BORDER_POST: return "bpost";
        case LV_STYLE_BORDER_SIDE: return "bside";
        case LV_STYLE_OUTLINE_WIDTH: return "ow";        case LV_STYLE_OUTLINE_COLOR: return "oc";
        case LV_STYLE_OUTLINE_OPA: return "oopa";        case LV_STYLE_OUTLINE_PAD: return "opad";
        case LV_STYLE_BG_OPA: return "bgopa";            case LV_STYLE_BG_COLOR: return "bg";
        case LV_STYLE_SHADOW_WIDTH: return "shw";        case LV_STYLE_LINE_WIDTH: return "lw";
        case LV_STYLE_ARC_WIDTH: return "aw";            case LV_STYLE_TEXT_FONT: return "font";
        case LV_STYLE_IMAGE_RECOLOR_OPA: return "imgrcopa";
        case LV_STYLE_IMAGE_OPA: return "imgopa";        case LV_STYLE_SHADOW_OPA: return "shopa";
        case LV_STYLE_LINE_OPA: return "lopa";           case LV_STYLE_ARC_OPA: return "aopa";
        case LV_STYLE_TEXT_OPA: return "topa";           case LV_STYLE_SHADOW_COLOR: return "shc";
        case LV_STYLE_IMAGE_RECOLOR: return "imgrc";     case LV_STYLE_LINE_COLOR: return "lc";
        case LV_STYLE_ARC_COLOR: return "ac";            case LV_STYLE_TEXT_COLOR: return "tc";
        case LV_STYLE_SHADOW_OFFSET_X: return "shx";     case LV_STYLE_SHADOW_OFFSET_Y: return "shy";
        case LV_STYLE_SHADOW_SPREAD: return "shs";       case LV_STYLE_LINE_DASH_WIDTH: return "ldw";
        case LV_STYLE_TEXT_ALIGN: return "talign";       case LV_STYLE_TEXT_LETTER_SPACE: return "tls";
        case LV_STYLE_TEXT_LINE_SPACE: return "tlns";    case LV_STYLE_LINE_DASH_GAP: return "ldg";
        case LV_STYLE_LINE_ROUNDED: return "lround";     case LV_STYLE_ARC_ROUNDED: return "around";
        case LV_STYLE_OPA: return "opa";                 case LV_STYLE_OPA_LAYERED: return "opalay";
        case LV_STYLE_ANIM: return "anim";               case LV_STYLE_ANIM_DURATION: return "animdur";
        case LV_STYLE_TRANSITION: return "trans";        case LV_STYLE_RADIUS: return "r";
        case LV_STYLE_BLEND_MODE: return "blend";        case LV_STYLE_CLIP_CORNER: return "clip";
        case LV_STYLE_BASE_DIR: return "dir";            case LV_STYLE_LAYOUT: return "layout";
        case LV_STYLE_TRANSFORM_SCALE_X: return "scx";   case LV_STYLE_TRANSFORM_SCALE_Y: return "scy";
        case LV_STYLE_TRANSFORM_PIVOT_X: return "pvx";   case LV_STYLE_TRANSFORM_PIVOT_Y: return "pvy";
        case LV_STYLE_TRANSFORM_ROTATION: return "rot";
        case LV_STYLE_FLEX_FLOW: return "flow";          case LV_STYLE_FLEX_MAIN_PLACE: return "fmain";
        case LV_STYLE_FLEX_CROSS_PLACE: return "fcross"; case LV_STYLE_FLEX_TRACK_PLACE: return "ftrack";
        case LV_STYLE_FLEX_GROW: return "grow";
        default: return nullptr;
    }
}

enum : uint8_t { TP_NUM, TP_COLOR, TP_FONT, TP_PTR };
uint8_t treePropKind(lv_style_prop_t p) {
    switch (p) {
        case LV_STYLE_BG_COLOR: case LV_STYLE_BG_GRAD_COLOR: case LV_STYLE_BG_IMAGE_RECOLOR:
        case LV_STYLE_BORDER_COLOR: case LV_STYLE_OUTLINE_COLOR: case LV_STYLE_SHADOW_COLOR:
        case LV_STYLE_IMAGE_RECOLOR: case LV_STYLE_LINE_COLOR: case LV_STYLE_ARC_COLOR:
        case LV_STYLE_TEXT_COLOR: case LV_STYLE_TEXT_OUTLINE_STROKE_COLOR:
        case LV_STYLE_DROP_SHADOW_COLOR: case LV_STYLE_RECOLOR:
            return TP_COLOR;
        case LV_STYLE_TEXT_FONT:
            return TP_FONT;
        case LV_STYLE_BG_GRAD: case LV_STYLE_BG_IMAGE_SRC: case LV_STYLE_ARC_IMAGE_SRC:
        case LV_STYLE_BITMAP_MASK_SRC: case LV_STYLE_COLOR_FILTER_DSC: case LV_STYLE_ANIM:
        case LV_STYLE_TRANSITION: case LV_STYLE_GRID_COLUMN_DSC_ARRAY: case LV_STYLE_GRID_ROW_DSC_ARRAY:
            return TP_PTR;
        default:
            return TP_NUM;
    }
}

bool treeSame(lv_style_prop_t p, const lv_style_value_t &a, const lv_style_value_t &b) {
    switch (treePropKind(p)) {
        case TP_COLOR: return lv_color_eq(a.color, b.color);
        case TP_FONT: case TP_PTR: return a.ptr == b.ptr;
        default: return a.num == b.num;
    }
}

// A font by what it draws, never by its address (which moves with every build):
// line height, the widths of '0' and 'M', and the baseline.
void treeVal(lv_style_prop_t p, const lv_style_value_t &v) {
    const char *n = treePropName(p);
    char nb[8];
    if (!n) { snprintf(nb, sizeof(nb), "p%u", (unsigned)p); n = nb; }
    switch (treePropKind(p)) {
        case TP_COLOR:
            treeOut(" %s=%06lX", n, (unsigned long)(lv_color_to_u32(v.color) & 0xFFFFFFu));
            break;
        case TP_FONT: {
            const lv_font_t *f = (const lv_font_t *)v.ptr;
            if (!f) { treeOut(" %s=0", n); break; }
            treeOut(" %s=%ld/%u/%u/%ld", n, (long)lv_font_get_line_height(f),
                    (unsigned)lv_font_get_glyph_width(f, '0', 0), (unsigned)lv_font_get_glyph_width(f, 'M', 0),
                    (long)f->base_line);
            break;
        }
        case TP_PTR: treeOut(" %s=%s", n, v.ptr ? "p" : "0"); break;
        default:     treeOut(" %s=%ld", n, (long)v.num); break;
    }
}

// Every property any of the object's styles sets, per part (bit per property).
constexpr uint8_t TREE_PARTS = 9;   // MAIN .. CUSTOM_FIRST, by part >> 16
struct TreeProps { uint32_t bits[TREE_PARTS][8]; uint16_t partsUsed; };

void treeCollect(const lv_obj_t *o, TreeProps &tp) {
    memset(&tp, 0, sizeof(tp));
    for (uint32_t i = 0; i < o->style_cnt; i++) {
        const lv_obj_style_t &os = o->styles[i];
        if (os.is_trans || os.is_disabled || !os.style) continue;
        const uint32_t part = lv_obj_style_get_selector_part(os.selector) >> 16;
        if (part >= TREE_PARTS) continue;
        tp.partsUsed |= (uint16_t)(1u << part);
        const lv_style_t *st = os.style;
        auto mark = [&](lv_style_prop_t p) { if (p < 256) tp.bits[part][p >> 5] |= 1u << (p & 31); };
        if (lv_style_is_const(st)) {
            const lv_style_const_prop_t *cp = (const lv_style_const_prop_t *)st->values_and_props;
            for (uint32_t k = 0; cp[k].prop != LV_STYLE_PROP_INV; k++) mark(cp[k].prop);
        } else {
            const lv_style_prop_t *pp = (const lv_style_prop_t *)((const uint8_t *)st->values_and_props +
                                                                  st->prop_cnt * sizeof(lv_style_value_t));
            for (uint32_t k = 0; k < st->prop_cnt; k++) mark(pp[k]);
        }
    }
}

bool treeHas(const TreeProps &tp, uint32_t part, lv_style_prop_t p) {
    return p < 256 && (tp.bits[part][p >> 5] & (1u << (p & 31)));
}

// LVGL's own resolution (lv_obj_style.c, get_prop_core()) for any state, not
// only the one the object is in: the most specific matching state wins, and
// among equals the first in the list (local styles, then the newest added).
bool treeResolve(const lv_obj_t *o, lv_part_t part, lv_state_t state, lv_style_prop_t prop, lv_style_value_t &v) {
    int32_t weight = -1;
    for (uint32_t i = 0; i < o->style_cnt; i++) {
        const lv_obj_style_t &os = o->styles[i];
        if (os.is_trans || os.is_disabled || !os.style) continue;
        if (lv_obj_style_get_selector_part(os.selector) != part) continue;
        const lv_state_t st = lv_obj_style_get_selector_state(os.selector);
        if (st & (lv_state_t)~state) continue;
        if ((int32_t)st <= weight) continue;
        lv_style_value_t tmp;
        if (lv_style_get_prop(os.style, prop, &tmp) == LV_STYLE_RES_FOUND) {
            v = tmp;
            if (st == state) return true;
            weight = st;
        }
    }
    return weight >= 0;
}

// The style of one object: each part as drawn now, then what each other state
// that a style names would change.
void treeStyle(lv_obj_t *o) {
    TreeProps tp;
    treeCollect(o, tp);
    // Text properties are inherited, so a label's come from its parents: always
    // look at those on the MAIN part.
    static const lv_style_prop_t TEXT[] = { LV_STYLE_TEXT_COLOR, LV_STYLE_TEXT_FONT, LV_STYLE_TEXT_OPA,
                                            LV_STYLE_TEXT_LETTER_SPACE, LV_STYLE_TEXT_LINE_SPACE,
                                            LV_STYLE_TEXT_ALIGN };
    const bool texty = lv_obj_check_type(o, &lv_label_class) || lv_obj_check_type(o, &lv_checkbox_class) ||
                       lv_obj_check_type(o, &lv_dropdown_class);
    for (uint32_t part = 0; part < TREE_PARTS; part++) {
        if (!(tp.partsUsed & (1u << part)) && !(part == 0 && texty)) continue;
        const lv_part_t pt = (lv_part_t)(part << 16);
        bool open = false;
        for (uint32_t p = 1; p < LV_STYLE_LAST_BUILT_IN_PROP; p++) {
            bool want = treeHas(tp, part, (lv_style_prop_t)p);
            if (!want && part == 0 && texty)
                for (lv_style_prop_t t : TEXT) if (t == p) want = true;
            if (!want) continue;
            const lv_style_value_t v   = lv_obj_get_style_prop(o, pt, (lv_style_prop_t)p);
            const lv_style_value_t def = lv_style_prop_get_default((lv_style_prop_t)p);
            if (treeSame((lv_style_prop_t)p, v, def)) continue;
            if (!open) {
                if (part) treeOut(" %lu.0{", (unsigned long)part);
                else      treeOut(" {");
                open = true;
            }
            treeVal((lv_style_prop_t)p, v);
        }
        if (open) treeOut(" }");
    }
    // The states the styles name (pressed, checked, disabled...), each with what
    // it changes from the part's plain state.
    lv_style_selector_t seen[24];
    uint8_t nSeen = 0;
    for (uint32_t i = 0; i < o->style_cnt && nSeen < 24; i++) {
        const lv_obj_style_t &os = o->styles[i];
        if (os.is_trans || os.is_disabled || !os.style) continue;
        if (!lv_obj_style_get_selector_state(os.selector)) continue;
        bool dup = false;
        for (uint8_t k = 0; k < nSeen; k++) if (seen[k] == os.selector) dup = true;
        if (!dup) seen[nSeen++] = os.selector;
    }
    // In a fixed order, whatever order the styles were added in.
    for (uint8_t a = 0; a < nSeen; a++)
        for (uint8_t b = a + 1; b < nSeen; b++)
            if (seen[b] < seen[a]) { const lv_style_selector_t t = seen[a]; seen[a] = seen[b]; seen[b] = t; }
    for (uint8_t k = 0; k < nSeen; k++) {
        const lv_part_t  pt = lv_obj_style_get_selector_part(seen[k]);
        const lv_state_t st = lv_obj_style_get_selector_state(seen[k]);
        const uint32_t part = pt >> 16;
        if (part >= TREE_PARTS) continue;
        bool open = false;
        for (uint32_t p = 1; p < LV_STYLE_LAST_BUILT_IN_PROP; p++) {
            if (!treeHas(tp, part, (lv_style_prop_t)p)) continue;
            lv_style_value_t vs, v0;
            if (!treeResolve(o, pt, st, (lv_style_prop_t)p, vs)) continue;
            if (!treeResolve(o, pt, LV_STATE_DEFAULT, (lv_style_prop_t)p, v0))
                v0 = lv_obj_get_style_prop(o, pt, (lv_style_prop_t)p);
            if (treeSame((lv_style_prop_t)p, vs, v0)) continue;
            if (!open) { treeOut(" %lu.%x{", (unsigned long)part, (unsigned)st); open = true; }
            treeVal((lv_style_prop_t)p, vs);
        }
        if (open) treeOut(" }");
    }
}

const char *treeClass(const lv_obj_t *o) {
    const lv_obj_class_t *c = lv_obj_get_class(o);
    const char *n = (c && c->name) ? c->name : "?";
    return strncmp(n, "lv_", 3) ? n : n + 3;
}

void treeObj(lv_obj_t *o, int depth) {
    if (!o) return;
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    treeOut("%*s%s @%ld,%ld %ldx%ld fl=%lx st=%x", depth * 2, "", treeClass(o), (long)a.x1, (long)a.y1,
            (long)lv_area_get_width(&a), (long)lv_area_get_height(&a), (unsigned long)o->flags,
            (unsigned)lv_obj_get_state(o));
    // A countdown bar shrinks with every tick: its size is the clock's - the
    // card window's, About's, a dialog's (any object with the bar's role). And
    // what is inside the chart's area stands where the time and the values put
    // it - the 24 hours slide with the clock - so only its look compares.
    bool bar = o == s.autoBar;
    for (uint32_t i = 0; i < o->style_cnt && !bar; i++)
        bar = o->styles[i].style == Kit::style(Kit::KitStyle::KS_SHEET_BAR);
    if (bar) treeOut(" vol");
    if (o == s_hv.area) treeOut(" volkids");
    // The events it listens for, in a fixed order; a small user value with each
    // (a row's or a chip's index).
    const uint32_t ne = lv_obj_get_event_count(o);
    if (ne) {
        uint32_t keys[16];
        uint8_t nk = 0;
        for (uint32_t i = 0; i < ne && nk < 16; i++) {
            lv_event_dsc_t *d = lv_obj_get_event_dsc(o, i);
            if (!d) continue;
            const uintptr_t ud = (uintptr_t)lv_event_dsc_get_user_data(d);
            keys[nk++] = ((d->filter & 0xFFFFu) << 16) | (ud < 0xFFFFu ? (uint32_t)ud : 0xFFFFu);
        }
        for (uint8_t i = 0; i < nk; i++)
            for (uint8_t j = i + 1; j < nk; j++)
                if (keys[j] < keys[i]) { const uint32_t t = keys[i]; keys[i] = keys[j]; keys[j] = t; }
        treeOut(" ev=");
        for (uint8_t i = 0; i < nk; i++) {
            const uint32_t ud = keys[i] & 0xFFFFu;
            treeOut(i ? ",%lu" : "%lu", (unsigned long)(keys[i] >> 16));
            if (ud == 0xFFFFu) treeOut(":p");
            else if (ud) treeOut(":%lu", (unsigned long)ud);
        }
    }
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_SCROLLABLE) && (lv_obj_get_scroll_x(o) || lv_obj_get_scroll_y(o)))
        treeOut(" sc=%ld,%ld", (long)lv_obj_get_scroll_x(o), (long)lv_obj_get_scroll_y(o));
    const char *text = nullptr;
    if (lv_obj_check_type(o, &lv_label_class)) {
        text = lv_label_get_text(o);
    } else if (lv_obj_check_type(o, &lv_checkbox_class)) {
        text = lv_checkbox_get_text(o);
    } else if (lv_obj_check_type(o, &lv_dropdown_class)) {
        treeOut(" sel=%lu", (unsigned long)lv_dropdown_get_selected(o));
        text = lv_dropdown_get_options(o);
    } else if (lv_obj_check_type(o, &lv_image_class)) {
        const void *src = lv_image_get_src(o);
        switch (lv_image_src_get_type(src)) {
            case LV_IMAGE_SRC_VARIABLE: {
                const lv_image_dsc_t *d = (const lv_image_dsc_t *)src;
                treeOut(" img=%lux%lu/%u", (unsigned long)d->header.w, (unsigned long)d->header.h,
                        (unsigned)d->header.cf);
                break;
            }
            case LV_IMAGE_SRC_SYMBOL: text = (const char *)src; break;
            case LV_IMAGE_SRC_FILE:   treeOut(" img=file"); break;
            default:                  treeOut(" img=none"); break;
        }
    } else if (lv_obj_check_type(o, &lv_line_class)) {
        const uint32_t n = lv_line_get_point_count(o);
        const lv_point_precise_t *p = lv_line_get_points(o);
        treeOut(" pts=%lu", (unsigned long)n);
        if (p && n) treeOut(":%ld,%ld..%ld,%ld", (long)p[0].x, (long)p[0].y, (long)p[n - 1].x, (long)p[n - 1].y);
    } else if (lv_obj_check_type(o, &lv_arc_class)) {
        treeOut(" arc=%ld-%ld/%ld", (long)lv_arc_get_bg_angle_start(o), (long)lv_arc_get_bg_angle_end(o),
                (long)lv_arc_get_rotation(o));
    } else if (lv_obj_check_type(o, &lv_chart_class)) {
        treeOut(" chart=%u/%lu", (unsigned)lv_chart_get_type(o), (unsigned long)lv_chart_get_point_count(o));
    }
    treeStyle(o);
    if (text) { treeOut(" t="); treeText(text); }
    treeOut("\n");
    const uint32_t cnt = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < cnt; i++) treeObj(lv_obj_get_child(o, (int32_t)i), depth + 1);
    // tree=1: a dropdown's list, which lives on the screen, under the dropdown.
    if (s_treeLists && lv_obj_check_type(o, &lv_dropdown_class)) {
        treeOut("%*s(list)\n", (depth + 1) * 2, "");
        treeObj(lv_dropdown_get_list(o), depth + 2);
    }
}

void treeRoot(const char *name, lv_obj_t *o) {
    treeOut("@%s\n", name);
    if (o) treeObj(o, 0);
}

// The whole dump: a line of the window's own numbers, then each root.
void treeDump(bool page) {
    s_treeLen = 0;
    s_treeCut = false;
    s_tree = (char *)heap_caps_malloc(TREE_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_tree) { dbgOut("no PSRAM for the tree\n"); return; }
    s_tree[0] = '\0';
    // As the next frame would: anything waiting to be laid out is laid out.
    lv_obj_update_layout(lv_screen_active());
    lv_obj_update_layout(lv_layer_top());
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    treeOut("# lv_mem_used=%lu lv_mem_free=%lu biggest=%lu\n", (unsigned long)(m.total_size - m.free_size),
            (unsigned long)m.free_size, (unsigned long)m.free_biggest_size);
    if (CardPopup::isOpen())
        treeOut("# window view=%u ctl=%u nEnt=%u native=%d expanded=%d member=%u hero=%s/%s/%ldx%ld/r%ld"
                " stacked=%d mainRowW=%ld propH=%ld win=%ld,%ld %ldx%ld r%ld pad=%ld title=%ld/%ld/%ld"
                " deck=%u/%d/%ld side=%u/%u/%ld faces_flat=%lu sparks=%u\n",
                (unsigned)s.view, (unsigned)s.lightCtl, (unsigned)s.nEnt, s.native ? 1 : 0, s.expanded ? 1 : 0,
                (unsigned)s.groupN, s.lightHero ? "light" : s.toggleHero ? "toggle" : "disc",
                s.builtPaused ? "paused" : "live", (long)s.heroW, (long)s.heroH, (long)s.heroR,
                s.stacked ? 1 : 0, (long)s.mainRowW, (long)s.propH, (long)s.winRect.x1, (long)s.winRect.y1,
                (long)lv_area_get_width(&s.winRect), (long)lv_area_get_height(&s.winRect), (long)s.winRadius,
                (long)s.pad, (long)s.titleW, (long)s.titleSlotW, (long)s.titleSlack, (unsigned)s.right.state,
                s.right.filled ? 1 : 0, (long)s.right.h, (unsigned)s.left.state, (unsigned)s.left.kind,
                (long)s.left.h, (unsigned long)Kit::facesFlat(), (unsigned)Kit::sparksMode());
    else
        treeOut("# no window\n");
    if (page) {
        s_treeLists = false;
        treeRoot("screen", lv_screen_active());
        treeRoot("top", lv_layer_top());
        treeRoot("sys", lv_layer_sys());
    } else if (CardPopup::isOpen()) {
        s_treeLists = true;
        treeRoot("window", s.win);
        treeRoot("settings", s.right.track);
        treeRoot("side", s.left.track);
        treeRoot("dialog", s_confirm);
        s_treeLists = false;
    }
    if (s_treeCut) treeOut("# TRUNCATED\n");
}

} // namespace popup

#endif // DEBUG_POPUP
