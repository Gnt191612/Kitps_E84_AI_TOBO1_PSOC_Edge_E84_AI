/*
 * 3D 仿真的物理世界：房间、人和物体、超声波、舵机、84E、两块 ESP32、串口和接线故障。
 * H7 固件是真实代码（firmware_wasm.js），这里只提供它看到的外部世界，不依赖界面，可以在 Node 里跑（sim_wasm/test_live_sim.js）。
 *
 * 只有 H7 是真实代码。其余都是模型：
 *   ESP32 的闭环控制量和丢失判定照抄 ESP32_ov2640/tracking（测试会核对常数），图像跟踪本身按“看得见就跟得上”简化；
 *   84E 的准确率来自模型评估（仅 AI 生成图，未实拍）；
 *   超声波、舵机速度、串口干扰是按数据手册和经验估的，都没有实测。
 */
(function (root) {
  'use strict';
  var RAD = Math.PI / 180;

  // 和 ESP32_ov2640 一致：tracking/closed_loop.c、tracking/tracker.c、Core/system.c
  var ESP = { KP: 0.5, KI: 0.01, KD: 0.1, I_LIMIT: 200, CTRL_MAX: 400, CENTER_X: 150, CENTER_Y: 112.5, LOST_THRESHOLD: 5, FOV: 50, WIDTH_PX: 160, MM_PER_PX: 1.875 };

  // 每个参数：默认值（典型）、取值范围、说明和依据
  var PARAMS = {
    beamHalf: { v: 10, min: 4, max: 20, step: 1, unit: '°', label: '超声波半波束角', why: '数据手册标称 15°，实际范围未测' },
    rangeNoise: { v: 1, min: 0, max: 8, step: 0.5, unit: 'cm', label: '测距噪声', why: '估计值' },
    echoDrop: { v: 0.02, min: 0, max: 0.3, step: 0.01, unit: '', label: '回波丢失率', why: '估计值' },
    personEcho: { v: 0.92, min: 0.4, max: 1, step: 0.01, unit: '', label: '人体回波率', why: '衣物吸声，估计值' },
    wallIncidence: { v: 55, min: 20, max: 90, step: 5, unit: '°', label: '墙面最大入射角', why: '斜着打到墙，回波反射走了，估计值' },
    falseEcho: { v: 0.002, min: 0, max: 0.05, step: 0.001, unit: '', label: '杂散回波率', why: '估计值' },
    servoSpeed: { v: 400, min: 60, max: 600, step: 20, unit: '°/s', label: '舵机速度', why: 'SG90 标称约 500°/s，带载未测' },
    mountErr: { v: 0, min: 0, max: 8, step: 0.5, unit: '°', label: '舵机零位误差', why: '未标定时每只舵机随机偏差的上限' },
    fps: { v: 25, min: 5, max: 40, step: 1, unit: 'fps', label: 'ESP32 帧率', why: '设计目标 ≥25，未实测' },
    ctrlGain: { v: 1, min: 0.5, max: 40, step: 0.5, unit: '倍', label: 'ESP32 闭环增益', why: '1 倍等于当前固件的 PID 参数；调大可以试改参数的效果，试好后要改 closed_loop.c' },
    trackNoisePx: { v: 1, min: 0, max: 8, step: 0.5, unit: 'px', label: '跟踪噪声', why: '合成图测试误差约 2.6 px' },
    trackMiss: { v: 0.005, min: 0, max: 0.2, step: 0.005, unit: '', label: '单帧跟丢率', why: '估计值，光照差时升高' },
    maxTrackCm: { v: 500, min: 150, max: 800, step: 10, unit: 'cm', label: '最远可跟踪距离', why: 'QQVGA 下人太小就没有特征，估计值' },
    inferMs: { v: 120, min: 20, max: 1500, step: 10, unit: 'ms', label: '84E 回复耗时', why: '指标是推理 ≤50 ms，整体回复未实测' },
    humanRecall: { v: 0.925, min: 0.3, max: 1, step: 0.005, unit: '', label: '84E 识别出人的概率', why: '模型 v2 按场景留出，漏检 7.5%，未实拍' },
    falsePositive: { v: 0.076, min: 0, max: 0.5, step: 0.004, unit: '', label: '84E 误报率', why: '模型 v2 按场景留出 7.6%，未实拍' },
    uartDelay: { v: 3, min: 0, max: 40, step: 1, unit: 'ms', label: '串口延时', why: '按帧长和波特率估算' },
    uartDrop: { v: 0, min: 0, max: 0.3, step: 0.01, unit: '', label: '串口丢帧率', why: '接线正常时估计很低' }
  };
  var PRESETS = {
    ideal: { label: '理想', v: { beamHalf: 8, rangeNoise: 0, echoDrop: 0, personEcho: 1, wallIncidence: 90, falseEcho: 0, servoSpeed: 600, trackNoisePx: 0, trackMiss: 0, inferMs: 120, humanRecall: 1, falsePositive: 0, uartDelay: 0, uartDrop: 0, fps: 30, mountErr: 0, maxTrackCm: 800 } },
    typical: { label: '典型', v: {} },
    harsh: { label: '恶劣', v: { beamHalf: 14, rangeNoise: 4, echoDrop: 0.12, personEcho: 0.75, wallIncidence: 40, falseEcho: 0.02, servoSpeed: 200, trackNoisePx: 4, trackMiss: 0.05, inferMs: 400, humanRecall: 0.7, falsePositive: 0.2, uartDelay: 10, uartDrop: 0.05, fps: 12, mountErr: 4, maxTrackCm: 300 } }
  };
  function presetParams(name) {
    var p = {};
    Object.keys(PARAMS).forEach(function (k) { p[k] = PARAMS[k].v; });
    var v = (PRESETS[name] || PRESETS.typical).v;
    Object.keys(v).forEach(function (k) { p[k] = v[k]; });
    return p;
  }

  // 按接线组给出拔线的后果。没有列出的线（调试串口）不影响系统行为
  function netEffect(n) {
    var f = n.from + ' ' + n.to;
    if (n.group === 'uart84') {
      if (/PB6|LS\.LV1/.test(f)) return { flags: ['e84Tx'], text: 'H7 的命令到不了 84E，H7 会报 84E 超时，目标一直无法确认' };
      if (/E\.J5_3|LS\.HV2/.test(f)) return { flags: ['e84Rx'], text: '84E 的结果回不到 H7，H7 会报 84E 超时' };
      return { flags: ['e84Tx', 'e84Rx'], text: '电平转换或 84E 失去参考，两个方向都不通' };
    }
    if (n.group === 'uartA' || n.group === 'uartB') {
      var e = n.group === 'uartA' ? 0 : 1, name = e ? 'ESP32-B' : 'ESP32-A';
      if (/G14/.test(f)) return { flags: [], text: name + ' 的角色脚：实物上可能变成错误的 A/B 角色，这个后果没有建模' };
      if (/PD5|PD8/.test(f)) return { flags: ['espTx' + e], text: 'H7 的跟踪命令到不了 ' + name };
      if (/PD6|PD9/.test(f)) return { flags: ['espRx' + e], text: name + ' 的回传到不了 H7，H7 收不到这块板的结果' };
      return { flags: ['espTx' + e, 'espRx' + e], text: name + ' 共地断开，两个方向都不通' };
    }
    if (n.group === 'radar') return { flags: ['radar'], text: '雷达没有回波，背景学不完或发现不了新目标' };
    if (n.group === 'servoSig' || n.group === 'servoPwr') {
      var m = /^(SR|SAP|SAT|SBP|SBT|SEP|SET)\./.exec(n.from);
      var key = m ? m[1] : '';
      var tilt = /T$/.test(key) && key !== 'SR';
      return { flags: key ? ['servo' + key] : [], text: tilt ? '俯仰舵机不动。仿真只计算水平方向，所以看不出影响；实物上可能看不到目标' : '这只舵机停在当前角度，固件仍以为它转到位了' };
    }
    if (n.group === 'power') return { flags: ['servoPower'], text: '所有舵机失去电源或参考地，全部停在当前角度' };
    return { flags: [], text: '只影响调试串口，日志看不到了，系统行为不变' };
  }

  function wrap180(a) { a = (a + 180) % 360; if (a < 0) a += 360; return a - 180; }

  // 可复现的随机数
  function Rng(seed) { this.s = (seed >>> 0) || 1; }
  Rng.prototype.next = function () {
    var x = this.s; x ^= x << 13; x >>>= 0; x ^= x >>> 17; x ^= x << 5; x >>>= 0; this.s = x;
    return x / 4294967296;
  };
  Rng.prototype.gauss = function () {
    var u = Math.max(1e-9, this.next()), v = this.next();
    return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
  };

  /* ---------- 射线几何 ---------- */
  function rayRoom(room, ox, oy, dx, dy) {
    var best = { t: Infinity, nx: 0, ny: 0 };
    function test(t, nx, ny) { if (t > 1e-6 && t < best.t) best = { t: t, nx: nx, ny: ny }; }
    if (dx > 1e-6) test((room.x1 - ox) / dx, 1, 0);
    if (dx < -1e-6) test((room.x0 - ox) / dx, 1, 0);
    if (dy > 1e-6) test((room.y1 - oy) / dy, 0, 1);
    if (dy < -1e-6) test((room.y0 - oy) / dy, 0, 1);
    return best;
  }
  function rayCircle(ox, oy, dx, dy, cx, cy, r) {
    var fx = ox - cx, fy = oy - cy, b = fx * dx + fy * dy, c = fx * fx + fy * fy - r * r, d = b * b - c;
    if (d < 0) return Infinity;
    var t = -b - Math.sqrt(d);
    return t > 0 ? t : Infinity;
  }
  function rayBox(ox, oy, dx, dy, b) {
    var t0 = -Infinity, t1 = Infinity;
    if (Math.abs(dx) < 1e-9) { if (ox < b.x0 || ox > b.x1) return Infinity; } else {
      var a0 = (b.x0 - ox) / dx, a1 = (b.x1 - ox) / dx; t0 = Math.max(t0, Math.min(a0, a1)); t1 = Math.min(t1, Math.max(a0, a1));
    }
    if (Math.abs(dy) < 1e-9) { if (oy < b.y0 || oy > b.y1) return Infinity; } else {
      var c0 = (b.y0 - oy) / dy, c1 = (b.y1 - oy) / dy; t0 = Math.max(t0, Math.min(c0, c1)); t1 = Math.min(t1, Math.max(c0, c1));
    }
    return (t0 <= t1 && t0 > 0) ? t0 : Infinity;
  }

  /* ---------- 世界 ---------- */
  function World(opts) {
    opts = opts || {};
    this.module = opts.module;              // 编译好的 WebAssembly.Module
    this.layout = opts.layout || { a: { x: -20, y: 0, off: -90 }, b: { x: 20, y: 0, off: 90 }, e: { x: 0, y: 0, off: 0 }, r: { x: 0, y: 20, off: 0 } };
    this.params = opts.params || presetParams('typical');
    this.seed = opts.seed || 1;
    this.onLine = opts.onLine || null;      // 每行固件日志
    this.keepLog = opts.keepLog !== false;
    this.room = { x0: -320, x1: 320, y0: -150, y1: 360 };
    this.furniture = [];
    this.actors = [];
    this.cuts = {};                         // 被拔掉的线（net id → true）
    this.flags = {};
    this.nextActor = 1;
    this.script = [];
    this.fw = null;
  }

  // 启动或重启：新建实例，固件所有静态变量回到上电状态
  World.prototype.boot = function () {
    var self = this;
    this.rng = new Rng(this.seed);
    this.t = 0;
    this.log = [];
    this.servos = {};
    this.uart = [];
    this.e84 = null;
    this.boards = [0, 1].map(function (i) {
      return { i: i, id: 0, actor: null, miss: 0, state: 'idle', rel: 0, next: 5 + i * 7, pid: { ix: 0, px: 0, out: 0 }, sent: 0, lost: false };
    });
    this.resetMetrics();
    var imports = {
      env: {
        js_log: function (ptr, len) { self.handleLog(ptr, len); },
        js_pwm: function (tim, ch, us) { self.handlePwm(tim, ch, us); },
        js_radar_ping: function () { return self.radarPing(); },
        js_cmd_84e: function (id, a, w, d0, d1) { self.cmd84e(id, a, w, d0, d1); },
        js_cmd_track: function (e, a, d, id) { self.cmdTrack(e, a, d, id); },
        js_cmd_release: function (e) { self.cmdRelease(e); },
        js_abort: function () { throw new Error('固件进入 Error_Handler'); }
      },
      wasi_snapshot_preview1: {}
    };
    WebAssembly.Module.imports(this.module).forEach(function (im) {
      if (im.module === 'wasi_snapshot_preview1') imports.wasi_snapshot_preview1[im.name] = function () { return 0; };
    });
    return WebAssembly.instantiate(this.module, imports).then(function (inst) {
      self.fw = inst.exports;
      self.mem = inst.exports.memory;
      self.fw.fw_init();
      return self;
    });
  };

  World.prototype.resetMetrics = function () {
    this.metrics = { enterAt: null, candAt: null, lockAt: null, detectMs: null, lockMs: null, errSum: 0, errN: 0, errLast: null, cand: 0, falseCand: 0, lock: 0, lost: 0, relock: 0, release: 0, to84e: 0, handover: 0, trackedMs: 0, presentMs: 0, bgAt: null };
  };

  /* ---------- 固件回调 ---------- */
  World.prototype.handleLog = function (ptr, len) {
    var bytes = new Uint8Array(this.mem.buffer, ptr, len), s = '';
    for (var i = 0; i < len; i++) s += String.fromCharCode(bytes[i]);
    if (this.keepLog) this.log.push(s);
    this.noteEvent(s);
    if (this.onLine) this.onLine(s);
  };
  World.prototype.noteEvent = function (line) {
    if (line.charAt(0) !== '$') return;
    var p = line.split(','), m = this.metrics, name = p[2], t = +p[1];
    if (name === 'BGOK' && m.bgAt === null) m.bgAt = t;
    else if (name === 'CAND') {
      m.cand++;
      // 雷达的方位和距离换算到房间坐标，附近没有人和物体就是误报
      var a = (+p[4]) / 10 * RAD, d = +p[5], r = this.layout.r, x = r.x + d * Math.sin(a), y = r.y + d * Math.cos(a);
      var near = this.actors.some(function (o) { return Math.hypot(o.x - x, o.y - y) < 90; });
      if (!near) m.falseCand++;
      if (m.candAt === null && near) { m.candAt = t; if (m.enterAt !== null) m.detectMs = t - m.enterAt; }
    } else if (name === 'LOCK') { m.lock++; if (m.lockAt === null) { m.lockAt = t; m.lockMs = +p[5]; } }
    else if (name === 'LOST') m.lost++;
    else if (name === 'RELOCK') m.relock++;
    else if (name === 'RELEASE') m.release++;
    else if (name === 'TO84E') m.to84e++;
    else if (name === 'HOOK') m.handover++;
    else if (name === 'FUSE') {
      var fx = +p[4], fy = +p[5], best = Infinity;
      this.actors.forEach(function (o) { if (o.kind === 'person') best = Math.min(best, Math.hypot(o.x - fx, o.y - fy)); });
      if (best < Infinity) { m.errSum += best; m.errN++; m.errLast = best; }
    }
  };
  World.prototype.handlePwm = function (tim, ch, us) {
    var key = tim === 2 ? ['SR', 'SAP', 'SAT', 'SBP'][ch - 1] : [null, 'SBT', 'SEP', 'SET'][ch - 1];
    if (!key) return;
    var tilt = key === 'SAT' || key === 'SBT' || key === 'SET';
    var cmd = tilt ? 90 : (us - 500) / 2000 * 270;
    var s = this.servos[key];
    if (!s) {
      var err = this.params.mountErr ? (this.rng.next() * 2 - 1) * this.params.mountErr : 0;
      s = this.servos[key] = { cmd: cmd, pos: cmd, err: err };
    }
    s.cmd = cmd;
  };
  World.prototype.servoCut = function (key) { return this.flags.servoPower || this.flags['servo' + key]; };
  // 云台实际朝向（房间坐标，正前方为 0，顺时针为正）
  World.prototype.heading = function (key, off) {
    var s = this.servos[key];
    return (s ? s.pos + s.err : 135) - 135 + off;
  };

  /* ---------- 超声波 ---------- */
  World.prototype.radarPing = function () {
    var P = this.params;
    if (this.flags.radar) return -1;
    if (this.rng.next() < P.echoDrop) return -1;
    if (this.rng.next() < P.falseEcho) return 20 + this.rng.next() * 380;
    var r = this.layout.r, ctr = this.heading('SR', r.off), best = Infinity, self = this;
    var seesPerson = this.rng.next() < P.personEcho;
    [-1, -0.5, 0, 0.5, 1].forEach(function (k) {
      var b = (ctr + k * P.beamHalf) * RAD, dx = Math.sin(b), dy = Math.cos(b);
      var w = rayRoom(self.room, r.x, r.y, dx, dy);
      var inc = Math.acos(Math.min(1, Math.abs(dx * w.nx + dy * w.ny))) / RAD;
      if (inc <= P.wallIncidence && w.t < best) best = w.t;
      self.furniture.forEach(function (f) { var t = rayBox(r.x, r.y, dx, dy, f); if (t < best) best = t; });
      self.actors.forEach(function (o) {
        if (o.kind === 'person' && !seesPerson) return;
        var t = rayCircle(r.x, r.y, dx, dy, o.x, o.y, o.r);
        if (t < best) best = t;
      });
    });
    if (best > 400) return -1;
    return Math.max(2, best + this.rng.gauss() * P.rangeNoise);
  };

  /* ---------- 视线 ---------- */
  World.prototype.visible = function (o, ox, oy, heading, maxCm) {
    var dx = o.x - ox, dy = o.y - oy, d = Math.hypot(dx, dy);
    if (d < 1 || d > maxCm) return null;
    var err = wrap180(Math.atan2(dx, dy) / RAD - heading);
    if (Math.abs(err) >= ESP.FOV / 2) return null;
    var ux = dx / d, uy = dy / d;
    for (var i = 0; i < this.furniture.length; i++) if (rayBox(ox, oy, ux, uy, this.furniture[i]) < d) return null;
    for (var j = 0; j < this.actors.length; j++) {
      var q = this.actors[j];
      if (q !== o && rayCircle(ox, oy, ux, uy, q.x, q.y, q.r * 0.8) < d - o.r) return null;
    }
    return { err: err, d: d };
  };

  /* ---------- 串口 ---------- */
  World.prototype.send = function (fn, ch) {
    var P = this.params;
    if (P.uartDrop && this.rng.next() < P.uartDrop) {
      if (ch !== undefined) this.fw.fw_note_bad(ch);
      return;
    }
    if (P.uartDelay <= 0) { fn(); return; }
    this.uart.push({ at: this.t + P.uartDelay, fn: fn });
  };

  /* ---------- 84E ---------- */
  World.prototype.cmd84e = function (id, angle, width, dmin, dmax) {
    if (this.flags.e84Tx) return;
    var self = this;
    this.send(function () {
      self.e84 = { id: id, angle: angle, dist: (dmin + dmax) / 2, due: self.t + self.params.inferMs };
    });
  };
  World.prototype.step84e = function () {
    var c = this.e84;
    if (!c || this.t < c.due) return;
    this.e84 = null;
    var P = this.params, e = this.layout.e, h = this.heading('SEP', e.off), best = null, bestErr = Infinity, self = this;
    this.actors.forEach(function (o) {
      var v = self.visible(o, e.x, e.y, h, 900);
      if (v && Math.abs(v.err) < bestErr) { bestErr = Math.abs(v.err); best = { o: o, d: v.d }; }
    });
    var conf, r = this.rng;
    if (best && best.o.kind === 'person') {
      // 远处人在画面里太小，识别率随距离下降
      var recall = P.humanRecall * (best.d > 300 ? Math.max(0.3, 1 - (best.d - 300) / 400) : 1);
      conf = r.next() < recall ? 0.55 + r.next() * 0.42 : 0.05 + r.next() * 0.42;
    } else {
      conf = r.next() < P.falsePositive ? 0.52 + r.next() * 0.2 : 0.03 + r.next() * 0.42;
    }
    if (this.flags.e84Rx) return;
    this.send(function () { self.fw.fw_on_84e(c.id, conf, c.dist, c.angle); }, 0);
  };

  /* ---------- ESP32 ---------- */
  World.prototype.cmdTrack = function (e, angle, dist, id) {
    if (this.flags['espTx' + e]) return;
    var b = this.boards[e], self = this;
    this.send(function () {
      if (b.id !== id) { b.actor = null; b.pid = { ix: 0, px: 0, out: 0 }; }
      else if (b.state === 'lost') b.actor = null;   // 同一目标重发：丢失时以 H7 给的位置重新建模
      b.id = id; b.state = 'tracking'; b.miss = 0; b.rel = angle; b.pending = true;
    });
  };
  World.prototype.cmdRelease = function (e) {
    if (this.flags['espTx' + e]) return;
    var b = this.boards[e], self = this;
    this.send(function () { b.id = 0; b.actor = null; b.state = 'idle'; b.lost = false; self.boards[e].pid = { ix: 0, px: 0, out: 0 }; });
  };
  World.prototype.stepBoard = function (b) {
    var P = this.params;
    if (this.t < b.next) return;
    b.next = this.t + 1000 / Math.max(1, P.fps);
    if (!b.id) return;
    var m = b.i ? this.layout.b : this.layout.a, h = this.heading(b.i ? 'SBP' : 'SAP', m.off), self = this, v = null;
    if (b.actor && this.actors.indexOf(b.actor) < 0) b.actor = null;
    if (!b.actor) {
      // 按 H7 给的相对角在画面里找最近的目标；图像算法本身没有建模
      var best = 18;
      this.actors.forEach(function (o) {
        var q = self.visible(o, m.x, m.y, h, P.maxTrackCm);
        if (q && Math.abs(q.err - b.rel) < best) { best = Math.abs(q.err - b.rel); b.actor = o; v = q; }
      });
    } else v = this.visible(b.actor, m.x, m.y, h, P.maxTrackCm);
    if (v && this.rng.next() < P.trackMiss) v = null;

    var lost = 1, xmm = 0, pan = b.pid.out;
    if (v) {
      b.miss = 0; b.state = 'tracking'; lost = 0;
      var px = ESP.WIDTH_PX / 2 + v.err * (ESP.WIDTH_PX / ESP.FOV) + this.rng.gauss() * P.trackNoisePx;
      xmm = px * ESP.MM_PER_PX;
      // tracking/closed_loop.c
      var err = xmm - ESP.CENTER_X, pid = b.pid;
      pid.ix = Math.max(-ESP.I_LIMIT, Math.min(ESP.I_LIMIT, pid.ix + err));
      var out = (ESP.KP * err + ESP.KI * pid.ix + ESP.KD * (err - pid.px)) * P.ctrlGain;
      pid.out = Math.max(-ESP.CTRL_MAX, Math.min(ESP.CTRL_MAX, out));
      pid.px = err;
      pan = pid.out;
    } else if (b.state === 'tracking' && ++b.miss >= ESP.LOST_THRESHOLD) {
      b.state = 'lost'; b.pid = { ix: 0, px: 0, out: 0 }; pan = 0;
    }
    b.lost = !!lost;
    b.sent++;
    if (this.flags['espRx' + b.i]) return;
    var id = b.id, y = ESP.CENTER_Y;
    this.send(function () { self.fw.fw_on_esp(b.i, id, xmm, y, pan, 0, lost); }, b.i + 1);
  };

  /* ---------- 推进 ---------- */
  World.prototype.stepActors = function () {
    var dt = 0.001, self = this;
    this.actors.forEach(function (o) {
      if (o.wait > 0) { o.wait--; return; }
      while (o.path && o.path.length) {
        var w = o.path[0];
        if (w.wait) { o.wait = w.wait; w.wait = 0; return; }
        var dx = w.x - o.x, dy = w.y - o.y, d = Math.hypot(dx, dy), s = (w.speed || o.speed) * dt;
        if (d <= s) { o.x = w.x; o.y = w.y; o.path.shift(); if (w.remove) o.gone = true; continue; }
        o.x += dx / d * s; o.y += dy / d * s; o.moving = true;
        return;
      }
      o.moving = false;
    });
    if (this.actors.some(function (o) { return o.gone; })) {
      this.actors = this.actors.filter(function (o) { return !o.gone; });
      this.boards.forEach(function (b) { if (b.actor && self.actors.indexOf(b.actor) < 0) b.actor = null; });
    }
  };
  World.prototype.inRoom = function (o) { var r = this.room; return o.x > r.x0 && o.x < r.x1 && o.y > r.y0 && o.y < r.y1; };
  World.prototype.step = function () {
    this.t++;
    this.stepActors();
    var self = this, speed = this.params.servoSpeed * 0.001;
    Object.keys(this.servos).forEach(function (k) {
      var s = self.servos[k];
      if (self.servoCut(k)) return;
      var d = s.cmd - s.pos;
      s.pos = Math.abs(d) <= speed ? s.cmd : s.pos + Math.sign(d) * speed;
    });
    while (this.uart.length && this.uart[0].at <= this.t) this.uart.shift().fn();
    this.step84e();
    this.stepBoard(this.boards[0]);
    this.stepBoard(this.boards[1]);
    this.fw.fw_step();

    // 剧本和指标
    while (this.script.length && this.fwReady() && this.t >= this.scriptBase() + this.script[0].at) this.script.shift().run(this);
    var m = this.metrics, ppl = this.actors.filter(function (o) { return o.kind === 'person' && self.inRoom(o); });
    if (ppl.length && m.bgAt !== null) {
      if (m.enterAt === null) m.enterAt = this.fw.fw_tick();
      m.presentMs++;
      var on = this.boards.some(function (b) { return b.actor && b.actor.kind === 'person' && b.state === 'tracking' && !b.lost; });
      if (on) m.trackedMs++;
    }
  };
  World.prototype.fwReady = function () { return this.metrics.bgAt !== null || this.scriptNoBg; };
  World.prototype.scriptBase = function () { return this.scriptT0 === undefined ? (this.scriptT0 = this.t) : this.scriptT0; };
  World.prototype.advance = function (ms) { for (var i = 0; i < ms; i++) this.step(); };

  /* ---------- 操作 ---------- */
  World.prototype.addActor = function (kind, x, y, path, speed) {
    var o = { id: this.nextActor++, kind: kind, x: x, y: y, r: kind === 'person' ? 20 : 25, path: path || [], speed: speed || 100, wait: 0, moving: false };
    this.actors.push(o);
    return o;
  };
  World.prototype.removeActor = function (o) {
    this.actors = this.actors.filter(function (a) { return a !== o; });
    this.boards.forEach(function (b) { if (b.actor === o) b.actor = null; });
  };
  World.prototype.setFurniture = function (list) { this.furniture = list; };
  World.prototype.setCut = function (net, cut) {
    if (cut) this.cuts[net.id] = net; else delete this.cuts[net.id];
    var flags = {}, c = this.cuts;
    Object.keys(c).forEach(function (id) { netEffect(c[id]).flags.forEach(function (f) { flags[f] = true; }); });
    this.flags = flags;
  };
  World.prototype.setFault = function (name, on) {
    this.manualFaults = this.manualFaults || {};
    this.manualFaults[name] = on;
    if (on) this.flags[name] = true; else delete this.flags[name];
  };
  World.prototype.command = function (cmd) {
    if (cmd === 'STOP') this.fw.fw_stop();
    else if (cmd === 'START') this.fw.fw_start();
    else if (cmd === 'RELEARN') { this.fw.fw_relearn(); this.metrics.bgAt = null; }
    else if (cmd === 'STATUS') this.fw.fw_status();
  };
  // 实际角度和固件以为的角度，用来看舵机滞后
  World.prototype.pans = function () {
    var L = this.layout, self = this;
    function pos(k) { var s = self.servos[k]; return s ? s.pos + s.err : 135; }
    return { real: [pos('SAP'), pos('SBP'), pos('SEP')], radar: pos('SR'), est: [this.fw.fw_est_pan(0), this.fw.fw_est_pan(1), this.fw.fw_est_pan(2)], estRadar: this.fw.fw_est_servo(), layout: L };
  };

  /* ---------- 场景 ---------- */
  var SCENES = {
    intrude: { label: '正常走路闯入', hint: '人从门口以 1 m/s 穿过房间（夜间闯入场景）', furniture: [],
      script: [{ at: 2000, run: function (w) { w.addActor('person', -340, 280, [{ x: -160, y: 240 }, { x: 180, y: 240 }, { x: 260, y: 120 }], 100); } }] },
    sneak: { label: '慢速走动', hint: '人以 0.3 m/s 缓慢穿过，用来和正常走路对比', furniture: [],
      script: [{ at: 2000, run: function (w) { w.addActor('person', -340, 260, [{ x: -160, y: 240 }, { x: 180, y: 240 }, { x: 260, y: 120 }], 30); } }] },
    twoPeople: { label: '两人闯入', hint: '小型库房：先后进来两个人，第二块跟踪板接第二个目标', furniture: [{ x0: -320, x1: -240, y0: -150, y1: 40 }],
      script: [
        { at: 2000, run: function (w) { w.addActor('person', -340, 280, [{ x: -120, y: 220, speed: 60 }, { x: -120, y: 220, wait: 40000 }], 60); } },
        { at: 16000, run: function (w) { w.addActor('person', 340, 280, [{ x: 130, y: 200, speed: 60 }, { x: 130, y: 200, wait: 40000 }], 60); } }] },
    occlude: { label: '柜子遮挡', hint: '人走到柜子后面再出来，看丢失和复锁', furniture: [{ x0: -30, x1: 40, y0: 150, y1: 190 }],
      script: [{ at: 2000, run: function (w) { w.addActor('person', -340, 260, [{ x: -120, y: 250, speed: 60 }, { x: -120, y: 250, wait: 8000 }, { x: 150, y: 250, speed: 40 }, { x: 150, y: 250, wait: 15000 }], 60); } }] },
    object: { label: '放一件物品', hint: '放一把椅子：雷达发现，84E 判定不是人，并入背景', furniture: [],
      script: [{ at: 2000, run: function (w) { w.addActor('object', 150, 180); } }] },
    empty: { label: '空房间', hint: '没有人，用来看误报', furniture: [], script: [] }
  };
  World.prototype.loadScene = function (name) {
    var s = SCENES[name] || SCENES.empty;
    this.sceneName = name;
    this.actors = [];
    this.setFurniture(s.furniture.map(function (f) { return { x0: f.x0, x1: f.x1, y0: f.y0, y1: f.y1 }; }));
    this.script = s.script.slice();
    this.scriptT0 = undefined;
    this.scriptNoBg = false;
    var self = this;
    return this.boot().then(function () { return self; });
  };

  var api = { World: World, PARAMS: PARAMS, PRESETS: PRESETS, SCENES: SCENES, ESP: ESP, presetParams: presetParams, netEffect: netEffect };
  if (typeof module !== 'undefined' && module.exports) module.exports = api; else root.Sim3DWorld = api;
})(this);
