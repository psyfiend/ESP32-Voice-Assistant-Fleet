"""Write host-tests/test/test_arranger/test_main.cpp from Layout Playground v9's own answers.
From the repo root (needs node and python; ~10 minutes):
python host-tests/tools/gen_arranger_tests.py host-tests/tools/playground.html host-tests/tools/run_playground.js host-tests/test/test_arranger/test_main.cpp"""
import json
import subprocess
import sys

html, runner, out = sys.argv[1:4]


def run(data, board="all", grid="auto"):
    r = subprocess.run(["node", runner, html, data, board, grid], capture_output=True, text=True, timeout=1200)
    if r.returncode:
        raise SystemExit(r.stderr)
    return json.loads(r.stdout)


CASES = []   # (name, comment, dataset, board json, pinned (c, r) or None)
SETS = {}


def add(data, board="all", grid="auto", label=""):
    res = run(data, board, grid)
    SETS[data] = res["blocks"]
    for b in res["boards"]:
        pin = None
        if grid != "auto":
            c, r = grid.split("x")
            pin = (int(c), int(r))
        CASES.append((data, b, pin, label))


add("device")
add("playground")
add("1060", "1060")
add("fleet", "p45")
add("fleet", "p47b")
add("device", "p45", "4x2", "the P4_5's House page as the owner pinned it")
add("playground", "p45", "5x3", "")
add("device", "p44b", "4x5", "the 4B pinned at five rows: bands spread down the page")
add("device", "p44b", "4x3", "the 4B pinned at its own 4 x 3: a page, its clusters at their tightest (round 12)")
add("1060", "p44b", "4x5", "the 4B's House at 4 x 5: its clusters grown into the spare height (round 12)")
add("1060", "p44b", "4x3", "the 4B's House at 4 x 3: a page, where round 11 had none")

L = []
w = L.append
w("// The arranger (2.11, K109, K120): Layout Playground v9's answers are the expected values.")
w("// GENERATED from the playground's own code (host-tests/tools/playground.html, its page v12,")
w("// 2026-10-26) by host-tests/tools/gen_arranger_tests.py with run_playground.js - do not edit the")
w("// cases by hand; run it again when the playground changes.")
w("// Four sets of blocks: DEVICE (the House page as the boards showed it in round 6: Doors, Kitchen")
w("// 4 tiles, Outside and Office 3), PLAYGROUND (its own House page by area), P1060 (the 1060's page in")
w("// round 9: Doors, three 3-tile clusters, Thermo) and FLEET (sixteen solo cards, Free Heap and Uptime")
w("// preferring 1.5 cells across). The screens are the playground's boards, its geometry() worked out")
w("// for each, Float labels. Every grid, place, trimmed box, finished rectangle, score and empty unit")
w("// is checked.")
w("#include <unity.h>")
w("#include <math.h>")
w('#include "Layout/Arranger.h"')
w("")
w("using namespace Layout;")
w("")
w("void setUp() {}")
w("void tearDown() {}")
w("")
NAMES = {"device": "DEVICE", "playground": "PLAYGROUND", "1060": "P1060", "fleet": "FLEET"}
for data, blocks in SETS.items():
    w("static const ArrBlock %s[%d] = {" % (NAMES[data], len(blocks)))
    for b in blocks:
        cl = b["type"] == "cluster"
        w("    { %s, %d, %d, %d, %r }," % ("true" if cl else "false", b["tiles"] if cl else 0, b["hdr"] if cl else 0,
                                          b["members"], float(b["aspect"])))
    w("};")
w("")
w("struct Want { uint8_t c, r, w, h, tc, tr; uint8_t bc, br, bw, bh; int32_t x, y, rw, rh; };")
w("")
w("static ArrScreen screen(double pmm, double aw, double ah, double gap, double tagH, uint8_t rc, uint8_t rr) {")
w("    ArrScreen s;")
w("    s.pmm = pmm; s.aw = aw; s.ah = ah; s.gap = gap; s.tagH = tagH;")
w("    s.label = ArrLabel::LABEL_FLOAT;")
w("    s.recCols = rc; s.recRows = rr;")
w("    return s;")
w("}")
w("")
w("static void check(const ArrScreen &s, const ArrBlock *blocks, uint8_t n, uint8_t cols, uint8_t rows,")
w("                  double score, uint16_t holes, bool capped, const Want *want) {")
w("    ArrResult out;")
w("    TEST_ASSERT_TRUE(arrange(s, blocks, n, ArrWeights(), out));")
w("    TEST_ASSERT_EQUAL_UINT8(cols, out.cols);")
w("    TEST_ASSERT_EQUAL_UINT8(rows, out.rows);")
w("    TEST_ASSERT_TRUE(fabs(score - out.score) < 1e-6);   // Unity's doubles are off here")
w("    TEST_ASSERT_EQUAL_UINT16(holes, out.holes);")
w("    TEST_ASSERT_EQUAL(capped, out.capped);")
w("    for (uint8_t i = 0; i < n; i++) {")
w("        const Want &q = want[i];")
w("        TEST_ASSERT_EQUAL_UINT8(q.c, out.pos[i].c);  TEST_ASSERT_EQUAL_UINT8(q.r, out.pos[i].r);")
w("        TEST_ASSERT_EQUAL_UINT8(q.w, out.pos[i].w);  TEST_ASSERT_EQUAL_UINT8(q.h, out.pos[i].h);")
w("        TEST_ASSERT_EQUAL_UINT8(q.tc, out.pos[i].tc); TEST_ASSERT_EQUAL_UINT8(q.tr, out.pos[i].tr);")
w("        TEST_ASSERT_EQUAL_UINT8(q.bc, out.box[i].c); TEST_ASSERT_EQUAL_UINT8(q.br, out.box[i].r);")
w("        TEST_ASSERT_EQUAL_UINT8(q.bw, out.box[i].w); TEST_ASSERT_EQUAL_UINT8(q.bh, out.box[i].h);")
w("        TEST_ASSERT_EQUAL_INT32(q.x, out.rect[i].x); TEST_ASSERT_EQUAL_INT32(q.y, out.rect[i].y);")
w("        TEST_ASSERT_EQUAL_INT32(q.rw, out.rect[i].w); TEST_ASSERT_EQUAL_INT32(q.rh, out.rect[i].h);")
w("    }")
w("}")
w("")
tests = []
for data, b, pin, label in CASES:
    name = "test_%s_%s%s" % (NAMES[data].lower(), b["id"], ("_pinned_%dx%d" % pin) if pin else "")
    blocks = NAMES[data]
    n = len(SETS[data])
    G = b["G"]
    rc, rr = b["rec"]
    args = "%r, %r, %r, %r, %r, %d, %d" % (G["pmm"], float(G["aw"]), float(G["ah"]), float(G["gap"]), float(G["tagH"]), rc, rr)
    if label:
        w("// %s" % label)
    w("void %s() {" % name)
    w("    ArrScreen s = screen(%s);" % args)
    if pin:
        w("    s.fixCols = %d; s.fixRows = %d;" % pin)
    if "grid" not in b or b.get("grid") is None:
        w("    ArrResult out;")
        w("    TEST_ASSERT_FALSE(arrange(s, %s, %d, ArrWeights(), out));" % (blocks, n))
    else:
        rows = []
        for p, bx, rc_ in zip(b["pos"], b["box"], b["rect"]):
            rows.append("{%d,%d,%d,%d,%d,%d, %d,%d,%d,%d, %d,%d,%d,%d}" % tuple(p + bx + rc_))
        w("    const Want want[%d] = {" % n)
        for k in range(0, len(rows), 2):
            w("        " + ", ".join(rows[k:k + 2]) + ",")
        w("    };")
        w("    check(s, %s, %d, %d, %d, %r, %d, %s, want);" % (blocks, n, b["grid"]["cols"], b["grid"]["rows"], b["score"],
                                                            b["holes"], "true" if b["capped"] else "false"))
    w("}")
    w("")
    tests.append(name)

w("// The owner's two references, in the playground's geometry (its refSizes()).")
w("void test_references() {")
w("    const ArrRef r = arrRef();")
w("    TEST_ASSERT_TRUE(fabs(r.soloW - 13.539770330741101) < 1e-9);")
w("    TEST_ASSERT_TRUE(fabs(r.soloH - 16.19995635071031) < 1e-9);")
w("    TEST_ASSERT_TRUE(fabs(r.tall - 1.1964720194647203) < 1e-9);")
w("    TEST_ASSERT_TRUE(fabs(r.tileW - 15.296916845527548) < 1e-9);")
w("    TEST_ASSERT_TRUE(fabs(r.tileH - 6.718958234792058) < 1e-9);")
w("}")
w("")
w("// One count pinned (beyond the playground, which pins both or neither): the columns are")
w("// obeyed and the rows chosen.")
w("void test_pinned_columns_only() {")
w("    ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);")
w("    s.fixCols = 4;")
w("    ArrResult out;")
w("    TEST_ASSERT_TRUE(arrange(s, DEVICE, 4, ArrWeights(), out));")
w("    TEST_ASSERT_EQUAL_UINT8(8, out.cols);")
w("    TEST_ASSERT_TRUE(out.rows >= 4 && out.rows <= 8);")
w("}")
w("")
w("// A kept answer finished again gives the same boxes and rectangles (PageArranger keeps pos).")
w("void test_finish_again() {")
w("    const ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);")
w("    ArrResult out;")
w("    TEST_ASSERT_TRUE(arrange(s, PLAYGROUND, 6, ArrWeights(), out));")
w("    ArrPlace box[ARR_BLOCKS_MAX];")
w("    ArrRect rect[ARR_BLOCKS_MAX];")
w("    finishPage(s, PLAYGROUND, 6, out.cols, out.rows, out.pos, box, rect);")
w("    for (int i = 0; i < 6; i++) {")
w("        TEST_ASSERT_EQUAL_UINT8(out.box[i].r, box[i].r);")
w("        TEST_ASSERT_EQUAL_UINT8(out.box[i].h, box[i].h);")
w("        TEST_ASSERT_EQUAL_INT32(out.rect[i].x, rect[i].x);")
w("        TEST_ASSERT_EQUAL_INT32(out.rect[i].y, rect[i].y);")
w("    }")
w("}")
w("")
w("// A kept answer holds a cluster's tiles across, not down (round 12): finished without them, the")
w("// page is the same - its clusters still grow into the spare height (the 4B's House at 4 x 5).")
w("void test_finish_without_tiles_down() {")
w("    ArrScreen s = screen(10.021985875084926, 670.0, 620.0, 18.0, 29.0, 4, 3);")
w("    s.fixCols = 4; s.fixRows = 5;")
w("    ArrResult out;")
w("    TEST_ASSERT_TRUE(arrange(s, P1060, 5, ArrWeights(), out));")
w("    ArrPlace pos[ARR_BLOCKS_MAX], box[ARR_BLOCKS_MAX];")
w("    ArrRect rect[ARR_BLOCKS_MAX];")
w("    for (int i = 0; i < 5; i++) { pos[i] = out.pos[i]; pos[i].tr = 0; }")
w("    finishPage(s, P1060, 5, out.cols, out.rows, pos, box, rect);")
w("    const double sy = (s.ah + s.gap) / out.rows;")
w("    bool grew = false;")
w("    for (int i = 0; i < 5; i++) {")
w("        TEST_ASSERT_EQUAL_INT32(out.rect[i].y, rect[i].y);")
w("        TEST_ASSERT_EQUAL_INT32(out.rect[i].h, rect[i].h);")
w("        if (P1060[i].cluster && rect[i].h > out.box[i].h * sy - s.gap + 1) grew = true;")
w("    }")
w("    TEST_ASSERT_TRUE(grew);")
w("}")
w("")
w("// No two finished rectangles overlap, and all lie in the page's room.")
w("void test_no_overlap() {")
w("    const ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);")
w("    ArrResult out;")
w("    TEST_ASSERT_TRUE(arrange(s, FLEET, 16, ArrWeights(), out));")
w("    for (int i = 0; i < 16; i++) {")
w("        const ArrRect &a = out.rect[i];")
w("        TEST_ASSERT_TRUE(a.x >= 0 && a.y >= 0 && a.x + a.w <= 1222 && a.y + a.h <= 604);")
w("        for (int j = i + 1; j < 16; j++) {")
w("            const ArrRect &b = out.rect[j];")
w("            TEST_ASSERT_TRUE(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);")
w("        }")
w("    }")
w("}")
w("")
tests += ["test_references", "test_pinned_columns_only", "test_finish_again", "test_finish_without_tiles_down", "test_no_overlap"]
w("int main(int, char **) {")
w("    UNITY_BEGIN();")
for t in tests:
    w("    RUN_TEST(%s);" % t)
w("    return UNITY_END();")
w("}")
with open(out, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(L) + "\n")
print(len(tests), "tests written")
