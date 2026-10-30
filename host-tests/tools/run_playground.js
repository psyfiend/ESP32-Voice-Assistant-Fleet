// Runs Layout Playground v9's own arranger (the page's script, cut out by its section markers) on
// its boards and prints each answer as JSON:
//   node host-tests/tools/run_playground.js host-tests/tools/playground.html <device|1060|fleet|playground> [board id|all] [CxR pinned|auto] [big]
// Board ids: the playground's BOARDS (p45, p47b, 1060, p44b, s34b, 4880l, 4880p, 3248).
const fs = require("fs");
const vm = require("vm");
const html = fs.readFileSync(process.argv[2], "utf8");
const cut = (from, to) => { const a = html.indexOf(from), b = html.indexOf(to, a); if (a < 0 || b < 0) throw new Error("marker " + from); return html.slice(a, b); };
const data = cut("const BOARDS = [", "/* ---------------- state");
const state = cut("let S;", "/* ---------------- the arranger ---------------- */");
const arr = cut("/* ---------------- the arranger ---------------- */", "/* ---------------- rendering");
const tagH = cut("const tagHeight", "function tagHtml");
const src = [data, state, arr, tagH].join("\n")
  + "\nthis.out = { BOARDS, freshState, buildBlocks, arrangeV9, finishV9, splitHeader, setS: v => { S = v; } };";
const ctx = { performance: { now: () => Date.now() }, Math, Infinity, Object, Array, Uint8Array, Int32Array, JSON };
vm.createContext(ctx);
vm.runInContext(src, ctx);
const P = ctx.out;
const S = P.freshState();
P.setS(S);
S.data = process.argv[3] || "device";
S.blocks = P.buildBlocks(true, S.data);
if (process.argv[6]) S.w.big = +process.argv[6];
const only = process.argv[4] && process.argv[4] !== "all" ? process.argv[4] : null;
if (process.argv[5] && process.argv[5] !== "auto") S.grid = process.argv[5];
const res = { data: S.data, w: S.w, blocks: S.blocks.map(b => ({ type: b.type, tiles: P.splitHeader(b).tiles.length, hdr: P.splitHeader(b).hdr.length, members: b.members.length, aspect: b.aspect || 1 })), boards: [] };
for (const board of P.BOARDS) {
  if (only && board.id !== only) continue;
  const R = P.arrangeV9(board, S.blocks), G = R.G, lay = R.list[0];
  const one = { id: board.id, rec: board.rec, G: { pmm: G.pmm, aw: G.aw, ah: G.ah, gap: G.gap, tagH: Math.round(Math.round(2.0 * G.pmm) * 1.45) }, nodes: R.v6.nodes, ms: R.v6.ms };
  if (lay) {
    const F = P.finishV9(lay, G, S.blocks);
    one.grid = { cols: lay.grid.cols, rows: lay.grid.rows };
    one.score = lay.score; one.holes = lay.holes; one.mean = lay.mean; one.capped = !!lay.capped;
    one.pos = lay.pos.map(p => [p.c, p.r, p.w, p.h, p.tc || 0, p.tr || 0]);
    one.box = F.boxes.map(b => [b.c, b.r, b.w, b.h]);
    one.rect = F.rects.map(r => [r.x, r.y, r.w, r.h]);
    one.sizes = lay.pos.map(p => +p.size.toFixed(2));
  }
  res.boards.push(one);
}
console.log(JSON.stringify(res));
