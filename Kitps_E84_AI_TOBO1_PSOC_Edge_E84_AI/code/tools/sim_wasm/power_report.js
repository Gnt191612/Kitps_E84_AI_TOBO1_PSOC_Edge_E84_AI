#!/usr/bin/env node
// 舵机供电估算：真实 H7 固件决定每只舵机什么时候转，按模型算电流、压降和对跟踪的影响。
// 用法：node power_report.js        电流模型是按标称值估的，换成实测值后结论才可靠
'use strict';
const path = require('path');
const fw = require(path.join(__dirname, '..', 'sim_3d', 'firmware_wasm.js'));
const { presetParams, runTrials, SCENES, POWER_PATHS } = require(path.join(__dirname, '..', 'sim_3d', 'world.js'));

async function main() {
  const mod = await WebAssembly.compile(Buffer.from(fw.base64, 'base64'));
  const scenes = ['intrude', 'twoPeople', 'orbit'];
  const n = 10;

  console.log('[电源不受限时，舵机到底要多少电流]');
  const need = [];
  for (const scene of scenes) {
    const p = presetParams('typical');
    const probe = await runTrials({ module: mod, params: Object.assign({}, p, { powerLimitA: 50 }), scene, n, powerPath: 'bb' });
    const r = await runTrials({ module: mod, params: Object.assign({}, p, { powerLimitA: 50 }), scene, n, powerPath: 'drv' });
    r.overA1 = probe.ratedMs;   // 面包板额定就是 1 A
    need.push({ 场景: SCENES[scene].label, 平均A: +(r.sumA / r.powerMs).toFixed(2), 峰值A: +r.peakA.toFixed(2), '超过1A的时间': (r.overA1 / r.powerMs * 100).toFixed(1) + '%' });
  }
  console.table(need);

  console.log('[不同供电方式下的后果（' + n + ' 次平均）]');
  for (const scene of scenes) {
    const rows = [];
    for (const pathName of Object.keys(POWER_PATHS)) {
      for (const limit of [1, 1.5, 2, 3, 5]) {
        const p = presetParams('typical');
        p.powerLimitA = limit;
        const r = await runTrials({ module: mod, params: p, scene, n, powerPath: pathName });
        rows.push({
          供电: POWER_PATHS[pathName].label,
          电源A: limit,
          最低电压V: +r.minV.toFixed(2),
          超电源能力: (r.overMs / r.powerMs * 100).toFixed(1) + '%',
          超路径额定: (r.ratedMs / r.powerMs * 100).toFixed(1) + '%',
          欠压停转: (r.stallMs / r.powerMs * 100).toFixed(1) + '%',
          跟踪覆盖: r.coverage === null ? '-' : (r.coverage * 100).toFixed(0) + '%',
          锁定中位ms: r.lockP50,
          跟丢: Math.round(r.lost / n),
          接力: (r.hoOk / n).toFixed(1),
        });
      }
    }
    console.log(SCENES[scene].label);
    console.table(rows);
  }
}
main().catch((e) => { console.error(e); process.exit(1); });
