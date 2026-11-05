#!/usr/bin/env node
// 命令行跑一个场景并打印指标：node try_scene.js <场景> <秒数> [典型|ideal|harsh] [种子] [-v]
'use strict';
const path = require('path');
const fw = require(path.join(__dirname, '..', 'sim_3d', 'firmware_wasm.js'));
const { World, presetParams } = require(path.join(__dirname, '..', 'sim_3d', 'world.js'));

async function main() {
  const [scene = 'intrude', secs = '60', preset = 'typical', seed = '1'] = process.argv.slice(2);
  const verbose = process.argv.includes('-v');
  const module_ = await WebAssembly.compile(Buffer.from(fw.base64, 'base64'));
  const w = new World({ module: module_, params: presetParams(preset), seed: +seed, keepLog: true, onLine: verbose ? (l) => {
    if (!/,(RAD|GIM|FUSE|TRI|COMM),/.test(l)) console.log(l);
  } : null });
  await w.loadScene(scene);
  const t0 = Date.now();
  let guard = 0;
  while (w.metrics.bgAt === null && guard++ < 200000) w.step();
  console.log('背景学习完成，仿真', (w.t / 1000).toFixed(1), 's');
  w.advance(+secs * 1000);
  console.log('仿真', (w.t / 1000).toFixed(1), 's，耗时', Date.now() - t0, 'ms');
  const m = w.metrics;
  console.log({ detectMs: m.detectMs, lockMs: m.lockMs, cand: m.cand, falseCand: m.falseCand, lock: m.lock, lost: m.lost, relock: m.relock, release: m.release, to84e: m.to84e, handover: m.handover, errAvgCm: m.errN ? +(m.errSum / m.errN).toFixed(1) : null, coverage: m.presentMs ? +(m.trackedMs / m.presentMs).toFixed(3) : null });
  w.actors.forEach((a) => console.log('actor', a.kind, a.x.toFixed(0), a.y.toFixed(0)));
  console.log('pans', w.pans().real.map((x) => x.toFixed(1)), 'est', w.pans().est.map((x) => x.toFixed(1)));
}
main().catch((e) => { console.error(e); process.exit(1); });
