#!/usr/bin/env node
// 3D 实时仿真的自动测试：真实 H7 固件（WebAssembly）+ 物理世界模型。用法：node test_live_sim.js
'use strict';
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const TOOLS = path.join(__dirname, '..');
const CODE = path.join(TOOLS, '..');
const fw = require(path.join(TOOLS, 'sim_3d', 'firmware_wasm.js'));
const { World, presetParams, netEffect, ESP, SCENES, PARAMS } = require(path.join(TOOLS, 'sim_3d', 'world.js'));
global.window = {};
require(path.join(TOOLS, 'sim_3d', 'nets.js'));
const NETS = global.window.SIM_NETS;

let failed = 0, passed = 0;
function check(ok, msg) { if (ok) passed++; else { failed++; console.log('  失败：' + msg); } }
function section(s) { console.log('[' + s + ']'); }
// 仿真发现的固件现有问题：不算测试失败，修好后这里会提示
function known(stillThere, msg) { console.log((stillThere ? '  已知问题：' : '  已修复，可以把这条改成正式检查：') + msg); }

let mod;
async function make(scene, preset, seed, tweak) {
  const params = presetParams(preset || 'ideal');
  Object.assign(params, tweak || {});
  const w = new World({ module: mod, params, seed: seed || 1 });
  await w.loadScene(scene || 'empty');
  return w;
}
function learn(w) { let n = 0; while (w.metrics.bgAt === null && n++ < 120000) w.step(); return w.metrics.bgAt !== null; }
function count(w, name) { return w.log.filter((l) => l.split(',')[2] === name).length; }
function runUntil(w, name, n, ms) { for (let i = 0; i < ms && count(w, name) < n; i++) w.step(); return count(w, name) >= n; }

function constantsMatchFirmware() {
  section('模型常数和 ESP32 固件一致');
  const cl = fs.readFileSync(path.join(CODE, 'ESP32_ov2640', 'tracking', 'closed_loop.c'), 'utf8');
  const tr = fs.readFileSync(path.join(CODE, 'ESP32_ov2640', 'tracking', 'tracker.c'), 'utf8');
  const sys = fs.readFileSync(path.join(CODE, 'ESP32_ov2640', 'Core', 'system.c'), 'utf8');
  const num = (s, re) => { const m = re.exec(s); return m ? parseFloat(m[1]) : NaN; };
  check(num(cl, /DEFAULT_KP\s+([\d.]+)/) === ESP.KP, 'Kp');
  check(num(cl, /DEFAULT_KI\s+([\d.]+)/) === ESP.KI, 'Ki');
  check(num(cl, /DEFAULT_KD\s+([\d.]+)/) === ESP.KD, 'Kd');
  check(num(cl, /I_LIMIT\s*=\s*([\d.]+)/) === ESP.I_LIMIT, 'I_LIMIT');
  check(num(cl, /CTRL_MAX\s+([\d.]+)/) === ESP.CTRL_MAX, 'CTRL_MAX');
  check(num(tr, /LOST_THRESHOLD\s+(\d+)/) === ESP.LOST_THRESHOLD, 'LOST_THRESHOLD');
  check(num(sys, /CAM_FOV_H_DEG\s+([\d.]+)/) === ESP.FOV, 'FOV');
}

function firmwareIsCurrent() {
  section('内嵌固件是当前源码');
  let ok = true;
  try { execFileSync('python', [path.join(__dirname, 'build_firmware.py'), '--check'], { stdio: 'pipe' }); } catch (e) { ok = false; }
  check(ok, '内嵌固件过期，请运行 python build_firmware.py');
}

async function behaviours() {
  section('理想传感器下的完整流程（和 tests/test_h7_demo_flow 对应）');
  let w = await make('empty');
  check(learn(w), '背景要能学完');
  w.advance(13000);
  check(count(w, 'CAND') === 0, '空房间不应有候选');

  w.addActor('person', -100, 200);
  check(runUntil(w, 'CAND', 1, 15000), '雷达应发现人');
  check(runUntil(w, 'LOCK', 1, 8000), 'ESP32 应锁定');
  w.advance(2000);
  check(count(w, 'RELEASE') === 0, '人没动就不该释放');
  const fuse = w.log.filter((l) => l.includes(',FUSE,')).slice(-1)[0];
  check(!!fuse, '应输出融合位置');
  const f = fuse.split(',');
  check(Math.hypot(+f[4] + 100, +f[5] - 200) < 60, '估计位置要接近真实位置，实际 ' + f[4] + ',' + f[5]);

  // 第二个人：第二块板要接上
  w.addActor('person', 120, 180);
  check(runUntil(w, 'CAND', count(w, 'CAND') + 1, 15000), '雷达应发现第二个人');
  check(runUntil(w, 'LOCK', 2, 10000), '第二块板应锁定第二个人');

  // STOP / START
  w.command('STOP');
  w.advance(3000);
  check(Math.abs(w.pans().est[0] - 135) < 1 && Math.abs(w.pans().real[0] - 135) < 1, 'STOP 后云台回中位');
  const lockBefore = count(w, 'LOCK');
  w.command('START');
  check(runUntil(w, 'LOCK', lockBefore + 1, 20000), 'START 后重新锁定');

  // RELEARN
  w.actors = [];
  w.command('RELEARN');
  check(learn(w), '重新学习背景应完成');
  const cand = count(w, 'CAND');
  w.advance(13000);
  check(count(w, 'CAND') === cand, '重新学习后空房间没有候选');

  section('遮挡：丢失后复锁');
  w = await make('empty');
  learn(w);
  w.furniture = [];
  const p = w.addActor('person', -100, 200);
  check(runUntil(w, 'LOCK', 1, 20000), '先锁定');
  w.setFurniture([{ x0: -110, x1: -90, y0: 100, y1: 110 }]);   // 挡在 ESP32 和人之间，相当于瞬间遮挡
  const a = w.layout.a, h = w.pans().real[0] - 135 + a.off;
  const blocker = w.visible(p, a.x, a.y, h, 900);
  w.furniture = [{ x0: -60, x1: -30, y0: 60, y1: 110 }];
  const hidden = blocker === null || w.visible(p, a.x, a.y, h, 900) === null;
  check(hidden, '测试布置：柜子应挡住视线');
  check(runUntil(w, 'LOST', 1, 3000), '被挡住应报丢失');
  w.furniture = [];
  check(runUntil(w, 'RELOCK', 1, 8000), '露出来后应复锁');

  section('物品：判定不是人，并入背景');
  w = await make('object');
  learn(w);
  w.advance(4000);
  check(runUntil(w, 'CAND', 1, 15000), '雷达应发现物品');
  check(runUntil(w, 'RELEASE', 1, 20000), '84E 判定不是人后释放');
  check(count(w, 'LOCK') === 0, '物品不该被跟踪板锁定');
  w.advance(30000);
  check(count(w, 'LOCK') === 0, '并入背景后也不该再锁定');

  section('典型参数下的空房间');
  let falseCand = 0;
  const SEEDS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  for (const seed of SEEDS) {
    w = await make('empty', 'typical', seed);
    check(learn(w), '种子 ' + seed + ' 背景要能学完');
    w.advance(60000);
    falseCand += w.metrics.cand;
    check(w.metrics.cand <= 3, '种子 ' + seed + ' 一分钟内候选太多：' + w.metrics.cand);
  }
  known(falseCand > 0, `空房间 ${SEEDS.length} 分钟共 ${falseCand} 个假候选：斜对墙面时回波时有时无、距离随角度变化快，同一格两次测距可差 30 cm 以上（波束和噪声是估计值，需实测）`);
  w = await make('empty', 'ideal', 1);
  learn(w);
  w.advance(60000);
  check(w.metrics.cand === 0, '理想传感器下空房间不应有候选，实际 ' + w.metrics.cand);

  section('相同种子结果可复现');
  const a1 = await make('intrude', 'typical', 7); learn(a1); a1.advance(40000);
  const a2 = await make('intrude', 'typical', 7); learn(a2); a2.advance(40000);
  check(a1.log.join('\n') === a2.log.join('\n'), '两次日志应完全相同');
}

const pickNet = (title) => NETS.find((n) => n.title === title);

async function faults() {
  section('接线故障');
  async function locked(title, tweak) {
    const w = await make('empty', 'ideal', 1, tweak);
    if (title) w.setCut(pickNet(title), true);
    learn(w);
    w.addActor('person', -100, 200);
    w.advance(20000);
    return w;
  }
  let w = await locked(null);
  check(count(w, 'LOCK') >= 1, '接线完好时能锁定');

  w = await locked('H7→84E 命令（3.3 V 侧）');
  check(count(w, 'TO84E') >= 1 && count(w, 'LOCK') === 0, '84E 命令线断：超时且不锁定');

  w = await locked('84E→H7 结果（3.3 V 侧）');
  check(count(w, 'TO84E') >= 1 && count(w, 'LOCK') === 0, '84E 结果线断：超时且不锁定');

  w = await locked('雷达回波（分压后）');
  check(count(w, 'CAND') === 0, '雷达回波线断：不应发现目标');

  w = await locked('H7→ESP32-A 命令');
  check(w.boards[1].sent > 0, 'ESP32-A 命令线断：B 板仍在工作');
  known(count(w, 'LOCK') === 0, 'ESP32-A 命令线断（或 A 板死机）时 H7 不换主板：B 板看得见人，目标却一直锁不上');

  w = await locked('调试串口 H7 发送');
  check(count(w, 'LOCK') >= 1, '调试串口断开不影响系统');

  // 雷达舵机不转：只能看到一个方向
  w = await make('empty', 'ideal', 1);
  w.setCut(pickNet('雷达水平舵机 信号'), true);
  learn(w);
  w.advance(10000);
  const pans = w.pans();
  check(Math.abs(pans.radar - 135) < 1 && Math.abs(pans.estRadar - 135) > 1, '雷达舵机线断：实际不动而固件以为在转');

  section('每根线的后果都有说明');
  NETS.forEach((n) => { const e = netEffect(n); check(e && e.text, '缺少说明：' + n.title); });
  const flagNames = new Set(['e84Tx', 'e84Rx', 'espTx0', 'espRx0', 'espTx1', 'espRx1', 'radar', 'servoPower', 'servoSR', 'servoSAP', 'servoSAT', 'servoSBP', 'servoSBT', 'servoSEP', 'servoSET']);
  NETS.forEach((n) => netEffect(n).flags.forEach((f) => check(flagNames.has(f), n.title + ' 的标志未定义：' + f)));
}

async function speedSweep() {
  section('当前固件能跟多快（理想传感器，2.4 m 处横穿）');
  const rows = [];
  for (const gain of [1, 5, 10, 20]) {
    const row = { 增益倍数: gain };
    for (const v of [20, 40, 60, 80, 100, 150]) {
      const w = await make('empty', 'ideal', 1, { ctrlGain: gain });
      learn(w);
      const person = w.addActor('person', -150, 240, [], v);
      w.advance(6000);                                 // 站着等锁定
      const locked = count(w, 'LOCK') >= 1;
      person.path = [{ x: 150, y: 240 }];
      const lost0 = count(w, 'LOST');
      w.advance(Math.ceil(300 / v * 1000));
      row[v + ' cm/s'] = !locked ? '未锁' : (count(w, 'LOST') > lost0 ? '丢' : '跟住');
    }
    rows.push(row);
  }
  console.table(rows);
  check(rows[0]['20 cm/s'] === '跟住', '20 cm/s 应能跟住');
  known(rows[0]['100 cm/s'] !== '跟住', '当前 PID 增益跟不上 1 m/s 的正常走路（closed_loop.c 的 Kp 乘 H7 的换算系数，每帧只修正约 1.5% 的偏差）');
}

async function main() {
  mod = await WebAssembly.compile(Buffer.from(fw.base64, 'base64'));
  check(Object.keys(SCENES).length >= 6, '场景数量');
  check(Object.keys(PARAMS).every((k) => PARAMS[k].label && PARAMS[k].why), '每个参数都要有说明和依据');
  firmwareIsCurrent();
  constantsMatchFirmware();
  await behaviours();
  await faults();
  await speedSweep();
  console.log(failed ? `失败 ${failed} 项，通过 ${passed} 项` : `全部通过：${passed} 项`);
  process.exit(failed ? 1 : 0);
}
main().catch((e) => { console.error(e); process.exit(1); });
