#!/usr/bin/env node
// 3D 实时仿真的自动测试：真实 H7 固件（WebAssembly）+ 物理世界模型。用法：node test_live_sim.js
'use strict';
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const TOOLS = path.join(__dirname, '..');
const CODE = path.join(TOOLS, '..');
const fw = require(path.join(TOOLS, 'sim_3d', 'firmware_wasm.js'));
const { World, presetParams, netEffect, ESP, SCENES, PARAMS, runTrials, RADAR_EDGE_DEG } = require(path.join(TOOLS, 'sim_3d', 'world.js'));
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
  // tests/test_h7_demo_flow.c 的 ESP32 替身也要和真实 PID 一致
  const flow = fs.readFileSync(path.join(CODE, 'tests', 'test_h7_demo_flow.c'), 'utf8');
  check(num(flow, /ESP_KP\s+([\d.]+)/) === ESP.KP, '整机流程替身的 Kp');
  check(num(flow, /ESP_KI\s+([\d.]+)/) === ESP.KI, '整机流程替身的 Ki');
  check(num(flow, /ESP_KD\s+([\d.]+)/) === ESP.KD, '整机流程替身的 Kd');
  check(num(flow, /ESP_I_LIMIT\s+([\d.]+)/) === ESP.I_LIMIT, '整机流程替身的 I_LIMIT');
  check(num(flow, /ESP_CTRL_MAX\s+([\d.]+)/) === ESP.CTRL_MAX, '整机流程替身的 CTRL_MAX');
  // 死区按偏差算要保持约 4 mm（2 px），否则 Kp 变大后噪声会一直推云台
  const gim = fs.readFileSync(path.join(CODE, 'STM32H7', 'Drivers', 'gimbal.h'), 'utf8');
  check(Math.abs(num(gim, /GIMBAL_CTRL_DEADBAND\s+([\d.]+)/) / ESP.KP - 4) < 0.01, 'H7 死区要随 Kp 同比变化');
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
  check(count(w, 'FAILOVER') === 1 && count(w, 'LOCK') >= 1, 'ESP32-A 命令线断：应改派给 B 板并锁定');

  w = await locked('ESP32-A→H7 结果');
  check(count(w, 'FAILOVER') === 1 && count(w, 'LOCK') >= 1, 'ESP32-A 结果线断：应改派给 B 板并锁定');

  // 锁定之后才断线，人还在走
  w = await make('empty', 'ideal', 1);
  learn(w);
  const walker = w.addActor('person', -100, 200, [], 30);
  check(runUntil(w, 'LOCK', 1, 20000), '断线前先锁定');
  walker.path = [{ x: 100, y: 200 }];
  w.advance(1000);
  w.setCut(pickNet('H7→ESP32-A 命令'), true);
  w.setCut(pickNet('ESP32-A→H7 结果'), true);
  check(runUntil(w, 'FAILOVER', 1, 6000), '锁定后断线：应改派');
  // 人刚起步就断线时速度还估不准，B 板可能转到旧位置，要靠雷达重新发现，所以等得久一点
  const failoverAt = w.t;
  let recoveredAt = null;
  for (let i = 0; i < 15000 && recoveredAt === null; i++) {
    w.step();
    if (w.boards[1].id && w.boards[1].actor === walker && !w.boards[1].lost) recoveredAt = w.t;
  }
  w.advance(5000);
  const fpsB = w.log.filter((l) => { const f = l.split(','); return f[2] === 'FPS' && +f[4] === 1; }).slice(-1)[0];
  const b = fpsB ? fpsB.split(',') : [];
  check(recoveredAt !== null && fpsB && +b[5] > 0 && +b[6] === 0, '锁定后断线：B 板应在 15 秒内跟上并稳定，最后一行 ' + fpsB);
  known(recoveredAt !== null && recoveredAt - failoverAt > 3000, '锁定后断线且人刚起步时，改派后要 ' + (recoveredAt === null ? '-' : ((recoveredAt - failoverAt) / 1000).toFixed(1)) + ' 秒才跟上');

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

async function tiltAndBatch() {
  section('俯仰');
  const gh = fs.readFileSync(path.join(CODE, 'STM32H7', 'Drivers', 'gimbal.h'), 'utf8');
  check(/GIMBAL_TILT_0DEG\s+500\b/.test(gh) && /GIMBAL_TILT_180DEG\s+2500\b/.test(gh) && /朝上/.test(gh), '俯仰脉宽和方向要和 world.js 一致');
  // 人站着不动，云台要把中线扫到的那段身体带到画面中央
  let w = await make('empty');
  learn(w);
  const p = w.addActor('person', -100, 90);
  check(runUntil(w, 'LOCK', 1, 20000), '近处的人应能锁定');
  w.advance(3000);
  const b = w.boards.find((x) => x.actor === p);
  check(!!b, '应有跟踪板跟着这个人');
  // 人走近后要抬头或低头跟住同一段身体
  const el0 = w.pans().tilt[b.i];
  p.path = [{ x: b.i ? 60 : -60, y: 30 }]; p.speed = 15;
  w.advance(8000);
  check(Math.abs(w.pans().tilt[b.i] - el0) > 1, '人走近时俯仰角应变化，实际 ' + el0.toFixed(1) + ' → ' + w.pans().tilt[b.i].toFixed(1));
  check(!b.lost, '走近过程中应一直跟住');

  // 俯仰装反：越修正偏得越远，目标跑出画面
  w = await make('empty');
  learn(w);
  w.setFault('tiltReversed', true);
  const q = w.addActor('person', -100, 90);
  runUntil(w, 'LOCK', 1, 20000);
  q.path = [{ x: -60, y: 30 }]; q.speed = 15;
  w.advance(15000);
  check(count(w, 'LOST') >= 1, '俯仰装反时应该跟丢');
  w.setFault('tiltReversed', false);
  check(!w.flags.tiltReversed, '故障能恢复');
  w.setFault('tiltReversed', true);
  w.setCut(pickNet('调试串口 H7 发送'), true);
  check(w.flags.tiltReversed, '拔线不能清掉手动故障');

  section('只拍到腿时不容易判成人');
  w = await make('empty');
  learn(w);
  const near = w.addActor('person', 0, 40);
  check(w.heightFraction(near, 25.5, 0, 40) < 0.4, '40 cm 处只拍得到一小段身体');
  check(w.heightFraction(near, 25.5, 0, 400) > 0.9, '4 m 处应拍得到全身');

  section('批量统计');
  const params = presetParams('ideal');
  const r = await runTrials({ module: mod, params, scene: 'sneak', n: 3, durMs: 40000 });
  check(r.n === 3 && r.withPeople === 3, '应跑完 3 次');
  check(r.noLock === 0 && r.lockSamples.length >= 3, '理想传感器下每次都应锁定');
  check(r.indicators.length === 4 && r.indicators.every((x) => x.name && x.target && x.value), '四项指标都要有结果');
  const lockRow = r.indicators[0];
  check(lockRow.pass === (r.lockP50 <= 500), '发现→锁定的判定要和数字一致');
  const r2 = await runTrials({ module: mod, params, scene: 'sneak', n: 3, durMs: 40000 });
  check(JSON.stringify(r.lockSamples) === JSON.stringify(r2.lockSamples), '批量统计也应可复现');
  const cut = await runTrials({ module: mod, params, scene: 'sneak', n: 2, durMs: 25000, cuts: [pickNet('H7→84E 命令（3.3 V 侧）')] });
  check(cut.noLock === 2 && cut.to84e > 0, '批量统计要带上拔线状态');
  const empty = await runTrials({ module: mod, params, scene: 'empty', n: 1, durMs: 5000 });
  check(empty.indicators.every((x) => x.pass === null), '空房间没有数据时不能判达标或未达标');
}

async function cooperation() {
  section('超出射界时多摄像头协作');
  let w = await make('orbit');
  learn(w);
  w.advance(60000);
  check(count(w, 'HOOK') >= 2, '绕一圈应至少接力两次，实际 ' + count(w, 'HOOK'));
  check(count(w, 'HOFAIL') === 0, '接力不应超时');
  check(w.metrics.trackedMs / w.metrics.presentMs > 0.8, '绕一圈大部分时间应被跟踪，实际 ' + (w.metrics.trackedMs / w.metrics.presentMs).toFixed(2));
  const hooks = w.log.filter((l) => l.split(',')[2] === 'HOOK').map((l) => +l.split(',')[4]);
  check(new Set(hooks).size === 2, '两块板都应接手过，实际 ' + hooks.join());

  // 只看理想参数会漏问题：典型参数下波束更宽，还有跟丢和噪声
  const typical = await runTrials({ module: mod, params: presetParams('typical'), scene: 'orbit', n: 20 });
  check(typical.cand <= 30, '典型参数绕一圈，同一个人不该变成多个目标：20 次共 ' + typical.cand + ' 个候选');
  check(typical.hoStart >= 30, '典型参数绕一圈，20 次应接力约 40 次，实际 ' + typical.hoStart);
  check(typical.hoOk >= typical.hoStart * 0.9, '典型参数接力成功率应 ≥90%，实际 ' + typical.hoOk + '/' + typical.hoStart);
  check(typical.coverage > 0.8, '典型参数大部分时间应被跟踪，实际 ' + typical.coverage.toFixed(2));
  console.log(`  典型参数绕一圈 20 次：候选 ${typical.cand}，接力 ${typical.hoOk}/${typical.hoStart}，跟丢 ${typical.lost}，跟踪覆盖 ${(typical.coverage * 100).toFixed(0)}%`);

  section('正常走路闯入：第一轮就要锁上');
  const firstLock = [];
  let firstCover = 0;
  for (let seed = 1; seed <= 20; seed++) {
    const t = new World({ module: mod, params: presetParams('typical'), seed });
    await t.loadScene('intrude');
    learn(t);
    let cand = null, lock = null;
    for (let i = 0; i < 40000; i++) {
      t.step();
      if (cand === null && t.metrics.candAt !== null) cand = t.metrics.candAt;
      if (lock === null && t.metrics.lockAt !== null) lock = t.metrics.lockAt;
    }
    firstLock.push(lock === null || cand === null ? Infinity : lock - cand);
    firstCover += t.metrics.trackedMs / Math.max(1, t.metrics.presentMs) / 20;
  }
  firstLock.sort((a, b) => a - b);
  console.log(`  正常走路 20 次：发现后到第一次锁定中位 ${firstLock[10]} ms，最慢 ${firstLock[19]} ms；跟踪覆盖 ${(firstCover * 100).toFixed(0)}%`);
  check(firstLock[10] < 1500, '发现后应很快锁定（中位），实际 ' + firstLock[10] + ' ms');
  check(firstLock[17] < 4000, '绝大多数情况第一轮就锁上，第 18 名 ' + firstLock[17] + ' ms');

  section('84E 边扫描边跟踪，认定后才交给 ESP32');
  w = await make('empty');
  learn(w);
  const p = w.addActor('person', -150, 200, [{ x: 150, y: 200 }], 60);
  let espEarly = false, worst = 0, scanFrames = 0;
  for (let i = 0; i < 12000 && count(w, 'LOCK') === 0; i++) {
    w.step();
    if (w.e84) {
      if (w.boards.some((b) => b.id)) espEarly = true;
      if (w.lastConf && w.lastConf.t === w.t) scanFrames++;
      if (count(w, 'REQ84E') >= 1 && scanFrames > 2) {
        worst = Math.max(worst, Math.abs(Math.atan2(p.x, p.y) / Math.PI * 180 - (w.pans().real[2] - 135)));
      }
    }
  }
  check(count(w, 'LOCK') === 1, '应锁定');
  check(!espEarly, '84E 认定之前 ESP32 不该动');
  const res84 = w.log.find((l) => l.split(',')[2] === 'RES84E');
  check(res84 && +res84.split(',')[6] >= 3, '84E 应连续看多帧才认定：' + res84);
  check(worst < 25, '84E 扫描时人应一直在画面内，最大偏差 ' + worst.toFixed(1) + '°');
  const order = w.log.map((l) => l.split(',')[2]);
  check(order.indexOf('CAND') < order.indexOf('REQ84E') && order.indexOf('REQ84E') < order.indexOf('RES84E') && order.indexOf('RES84E') < order.indexOf('ASSIGN') && order.indexOf('ASSIGN') < order.indexOf('LOCK'), '顺序应为 发现 → 84E 扫描 → 认定 → 派 ESP32 → 锁定');

  // ESP32 接手后 84E 停下待命
  check(!w.e84, '认定后 84E 应停止扫描');
  const pan84 = w.pans().real[2], req84 = count(w, 'REQ84E');
  p.path = [];
  w.advance(8000);
  check(count(w, 'REQ84E') === req84, 'ESP32 接手后 84E 不再被调用');
  check(Math.abs(w.pans().real[2] - pan84) < 1, 'ESP32 接手后 84E 云台保持原位');

  section('物品：84E 放弃，ESP32 全程不动');
  w = await make('object');
  learn(w);
  let moved = false;
  for (let i = 0; i < 25000; i++) { w.step(); if (w.boards.some((b) => b.id)) moved = true; }
  check(count(w, 'RELEASE') >= 1, '84E 判定不是人后应放弃');
  check(!moved, '物品不该让 ESP32 动');

  section('84E 跟踪的副本和 ESP32 一致');
  const CM = path.join(CODE, '..', 'Workspace', 'PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision', 'proj_cm55', 'source', 'scan_track');
  ['algorithm/track_algorithm.c', 'algorithm/track_algorithm.h', 'algorithm/filter.c', 'algorithm/filter.h', 'algorithm/predict.c', 'algorithm/predict.h', 'Vision/feature.c', 'Vision/feature.h', 'Vision/roi/roi_select.c', 'Vision/roi/roi_select.h'].forEach((rel) => {
    const norm = (f) => fs.readFileSync(f).toString('latin1').replace(/\r\n/g, '\n');
    check(norm(path.join(CODE, 'ESP32_ov2640', rel)) === norm(path.join(CM, rel)), rel + ' 和 ESP32 的不一致，改了算法要一起更新');
  });
}

async function radarEdge() {
  section('雷达不把自身的 ESP32 报成目标');
  for (const [beamHalf, deviceEcho] of [[10, 0.7], [15, 0.7], [15, 0.4], [15, 1]]) {
    let near = 0;
    for (let seed = 1; seed <= 3; seed++) {
      const w = await make('empty', 'typical', seed, { beamHalf, deviceEcho, falseEcho: 0 });
      learn(w);
      w.advance(60000);
      near += w.metrics.cand;
    }
    check(near === 0, '空房间（波束 ' + beamHalf + '°，回波率 ' + deviceEcho + '）不该有候选，实际 ' + near);
  }
  const w = await make('empty', 'typical', 1);
  learn(w);
  let lo = 999, hi = -999;
  for (let i = 0; i < 25000; i++) { w.step(); const a = w.fw.fw_est_servo(); lo = Math.min(lo, a); hi = Math.max(hi, a); }
  check(Math.abs(lo - RADAR_EDGE_DEG) < 1 && Math.abs(hi - (270 - RADAR_EDGE_DEG)) < 1, '固件实际扫描范围应和界面画的一致：' + lo.toFixed(1) + '~' + hi.toFixed(1));
}

async function noDuplicate() {
  section('杂散回波不会让两块板跟同一个人');
  let dup = 0;
  for (let seed = 1; seed <= 8; seed++) {
    const w = await make('twoPeople', 'typical', seed);
    learn(w);
    let ms = 0;
    for (let t = 0; t < 60000; t += 200) {
      w.advance(200);
      const a = w.boards.filter((b) => b.actor);
      if (a.length === 2 && a[0].actor === a[1].actor) ms += 200;
    }
    if (ms > 1000) dup++;
  }
  check(dup === 0, '两人闯入 8 次，不该有两块板持续跟同一个人，实际 ' + dup + ' 次');
}

async function power() {
  section('舵机供电');
  // 电流由固件的实际动作决定
  const quiet = await make('empty', 'typical', 1, { powerLimitA: 50 });
  learn(quiet);
  quiet.advance(20000);
  check(quiet.metrics.sumA / quiet.metrics.powerMs < 0.5, '空房间只有雷达舵机在转，平均电流应很小，实际 ' + (quiet.metrics.sumA / quiet.metrics.powerMs).toFixed(2));

  const run = async (limit, powerPath) => {
    const p = presetParams('typical');
    p.powerLimitA = limit;
    return runTrials({ module: mod, params: p, scene: 'orbit', n: 5, powerPath });
  };
  const plenty = await run(10, 'drv');
  check(plenty.overMs === 0 && plenty.stallMs === 0, '电源充足时不该超限也不该停转');
  check(plenty.peakA > 0.8 && plenty.peakA <= 7 * 0.65 + 0.01, '峰值应超过单只舵机、不超过 7 只同时启动，实际 ' + plenty.peakA.toFixed(2));
  check(plenty.minV > 4.5, '驱动板加充足电源，电压应很稳，实际 ' + plenty.minV.toFixed(2));
  const bb = await run(10, 'bb');
  check(bb.minV < plenty.minV - 0.3, '面包板路径的压降应明显更大：' + bb.minV.toFixed(2) + ' vs ' + plenty.minV.toFixed(2));
  check(bb.ratedMs > 0, '面包板应出现超过额定电流的时刻');
  const weak = await run(0.8, 'drv');
  check(weak.stallMs > 0, '电源太弱时应出现欠压停转');
  check(weak.coverage < plenty.coverage, '电源太弱时跟踪应变差：' + weak.coverage.toFixed(2) + ' vs ' + plenty.coverage.toFixed(2));
  const v = [weak.minV, plenty.minV];
  check(v.every((x) => x >= 0), '电压不能算成负的：' + v);

  section('连线：两种供电方式都接得上');
  const servoLines = NETS.filter((n) => n.group === 'servoPwr');
  check(servoLines.length === 14, '7 只舵机各有电源和地');
  check(servoLines.every((n) => n.drvTo && /^DRV\./.test(n.drvTo)), '每根舵机电源线都要能改接到驱动板');
  check(NETS.filter((n) => n.group === 'servoSig' && !n.only).every((n) => /^H7\./.test(n.to) && /^DRV\.S_/.test(n.drvFrom)), '信号从分配板 S 针接到 H7');
  check(NETS.filter((n) => n.group === 'servoSig' && n.only === 'drv').length === 7, '7 只舵机的插头都插在分配板上');
  const usbNets = NETS.filter((n) => n.group === 'usb');
  ['H7', 'A', 'B', 'E'].forEach((k) => check(usbNets.some((n) => n.from === k + '.USB' && /^HUB\./.test(n.to)), k + ' 用 USB 线供电'));
  check(usbNets.some((n) => n.from === 'PSU.USB' && n.to === 'CHG.USB'), '舵机电源单独一个充电头');
  // 杜邦线不能给开发板供电：角色脚、电平转换基准、雷达 5 V 之外，不许有线接到开发板电源脚
  const boardPower = /^(H7\.(5V|3V3)|A\.(5V|3V3)|B\.(5V|3V3)|E\.(VIN|VBUS|5V))/;
  const allowed = ['ESP32-A 角色脚', '电平转换 3.3 V 供电', '雷达供电'];
  NETS.forEach((n) => { if (boardPower.test(n.to)) check(allowed.includes(n.title), n.title + ' 不该给开发板灌电'); if (boardPower.test(n.from) && !allowed.includes(n.title)) check(false, n.title + ' 不该从开发板取电'); });
  check(quiet.powerPath === 'drv' && PARAMS.powerLimitA.v === 3, '默认按接线方案仿真：分配板，3 A');
}

// 每个场景按界面默认（典型参数）跑，看它承诺的效果有没有出现
async function sceneAcceptance() {
  section('场景验收（典型参数，每个场景 5 个种子）');
  const peopleTracked = (w) => w.actors.filter((o) => o.kind === 'person' && w.boards.some((b) => b.actor === o)).length;
  const walked = (w) => w.actors.every((o) => o.path.length === 0);
  const SPEC = {
    intrude: (w, m) => [[m.lock >= 1, '要锁定'], [walked(w), '路线要走完'], [peopleTracked(w) === 1, '走完时要跟着人']],
    sneak: (w, m) => [[m.lock >= 1, '要锁定'], [peopleTracked(w) === 1, '走完时要跟着人'], [m.trackedMs / m.presentMs > 0.8, '大部分时间要被跟踪']],
    twoPeople: (w, m) => [[m.lock >= 2, '两个人都要锁定'], [peopleTracked(w) === 2, '两块板要各跟一个人，实际 ' + peopleTracked(w)]],
    occlude: (w, m) => [[m.lost >= 1, '被柜子挡住要报丢失'], [m.relock >= 1, '要复锁'], [peopleTracked(w) === 1, '出来后要跟着人']],
    object: (w, m) => [[m.cand >= 1, '雷达要发现'], [m.lock === 0, '不能锁定'], [m.cand <= 5, '要并入背景，不能一直报：候选 ' + m.cand], [w.boards.every((b) => !b.id), 'ESP32 不能动']],
    boundary: (w, m) => [[m.hoStart >= 1 && m.handover >= 1, '要接力成功 ' + m.handover + '/' + m.hoStart], [walked(w), '路线要走完']],
    orbit: (w, m) => [[walked(w), '一圈要走完，还剩 ' + w.actors.map((o) => o.path.length)], [m.handover >= 2, '要接力至少两次，实际 ' + m.handover], [peopleTracked(w) === 1, '走完时要跟着人']],
    closeUp: (w, m) => [[m.lock >= 1, '要锁定'], [walked(w), '路线要走完']],
    empty: (w, m) => [[m.lock === 0, '不能锁定']],
  };
  check(Object.keys(SPEC).length === Object.keys(SCENES).length, '每个场景都要有验收标准');
  for (const name of Object.keys(SPEC)) {
    const fails = [];
    for (const seed of [1, 2, 3, 4, 5]) {
      const w = await make(name, 'typical', seed);
      learn(w);
      w.advance(SCENES[name].dur || 60000);
      SPEC[name](w, w.metrics).forEach(([ok, msg]) => { if (!ok) fails.push('种子' + seed + ' ' + msg); });
    }
    check(fails.length === 0, SCENES[name].label + '：' + fails.join('；'));
  }
}

async function speedSweep() {
  section('当前固件能跟多快（理想传感器，2.4 m 处横穿；1 倍是当前固件，死区固定不随倍数变）');
  const rows = [];
  for (const gain of [1, 0.2, 0.4, 0.6, 1.6, 2]) {
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
  check(rows[0]['100 cm/s'] === '跟住', '当前 PID 增益要跟得上 1 m/s 的正常走路');
  check(rows[0]['150 cm/s'] === '跟住', '当前 PID 增益要跟得上 1.5 m/s');
}

async function main() {
  mod = await WebAssembly.compile(Buffer.from(fw.base64, 'base64'));
  check(Object.keys(SCENES).length >= 6, '场景数量');
  check(Object.keys(PARAMS).every((k) => PARAMS[k].label && PARAMS[k].why), '每个参数都要有说明和依据');
  firmwareIsCurrent();
  constantsMatchFirmware();
  await behaviours();
  await faults();
  await tiltAndBatch();
  await cooperation();
  await sceneAcceptance();
  await radarEdge();
  await noDuplicate();
  await power();
  await speedSweep();
  console.log(failed ? `失败 ${failed} 项，通过 ${passed} 项` : `全部通过：${passed} 项`);
  process.exit(failed ? 1 : 0);
}
main().catch((e) => { console.error(e); process.exit(1); });
