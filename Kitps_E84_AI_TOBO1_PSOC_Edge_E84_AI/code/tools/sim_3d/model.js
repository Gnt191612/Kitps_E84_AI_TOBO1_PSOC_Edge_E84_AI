/*
 * 装置三维模型。单位 cm，世界坐标 x 右、y 前、z 上（与 config_mount.h 一致）。
 *
 * 引脚来源：
 *   ESP32-S3-CAM  实物丝印（材料/Esp32-S3-CAM.jpg、作品提交材料/ESP32.jpg）
 *   OV7675 转接板 实物丝印（作品提交材料/84E.jpg）
 *   84E J5        Infineon KIT_PSE84_AI 原理图：1 VDD_1V8，2 P9_3，3 P9_2，4 P9_1，5 P9_0，6 GND
 *   H7            外形、元件布局和 4×30 排针按宣传图（材料/Stm32H7zit6.jpg）量取，以 2.54 mm 针距换算；
 *                 宣传图这一面没有引脚名，背面照片（作品提交材料/H7.jpg）又太模糊，
 *                 所以引脚名在排针上的位置仍是按编号排的（H7_PINS_VERIFIED=false）
 * 其余板子的外形和尺寸是按照片估计的。
 */
window.Sim3DModel = (function () {
  'use strict';
  var T = window.THREE;
  T.ColorManagement.legacyMode = false;   // 颜色按 sRGB 解释，否则整体发白
  var H7_PINS_VERIFIED = false;

  var GOLD = new T.MeshStandardMaterial({ color: 0xd4af37, metalness: 0.9, roughness: 0.3 });
  var BLACK = new T.MeshStandardMaterial({ color: 0x15171a, roughness: 0.75 });
  var PLASTIC = new T.MeshStandardMaterial({ color: 0x1b1c1f, roughness: 0.6 });
  var METAL = new T.MeshStandardMaterial({ color: 0xb4b8bd, metalness: 0.85, roughness: 0.32 });
  var CHIP = new T.MeshStandardMaterial({ color: 0x111316, roughness: 0.45 });
  var WHITE = new T.MeshStandardMaterial({ color: 0xece9e0, roughness: 0.65 });
  var PIN_GEO = new T.BoxGeometry(0.064, 0.064, 1);
  var HIT_GEO = new T.SphereGeometry(0.17, 8, 6);
  var HIT_MAT = new T.MeshBasicMaterial({ visible: false });

  function std(color, rough, metal) { return new T.MeshStandardMaterial({ color: color, roughness: rough === undefined ? 0.6 : rough, metalness: metal || 0 }); }
  function box(parent, w, d, h, mat, x, y, z, shadow) {
    var m = new T.Mesh(new T.BoxGeometry(w, d, h), mat);
    m.position.set(x || 0, y || 0, z || 0);
    if (shadow !== false) { m.castShadow = true; m.receiveShadow = true; }
    parent.add(m);
    return m;
  }
  // 轴线沿 z 的圆柱
  function cyl(parent, r, h, mat, x, y, z, seg) {
    var m = new T.Mesh(new T.CylinderGeometry(r, r, h, seg || 24), mat);
    m.rotation.x = Math.PI / 2;
    m.position.set(x || 0, y || 0, z || 0);
    m.castShadow = true;
    parent.add(m);
    return m;
  }

  /* 丝印贴图：draw(ctx) 里用 cm 坐标，原点在板中心，y 向上 */
  function silkMaterial(w, h, pcbColor, draw, renderer) {
    var S = 230, cv = document.createElement('canvas');
    cv.width = Math.min(4096, Math.round(w * S)); cv.height = Math.min(4096, Math.round(h * S));
    var c = cv.getContext('2d'), kx = cv.width / w, ky = cv.height / h;
    c.fillStyle = pcbColor; c.fillRect(0, 0, cv.width, cv.height);
    var api = {
      x: function (x) { return (x + w / 2) * kx; }, y: function (y) { return (h / 2 - y) * ky; },
      text: function (x, y, s, size, color, align, rot, bold) {
        c.save(); c.translate(api.x(x), api.y(y));
        if (rot) c.rotate(-rot);
        c.font = (bold ? 'bold ' : '') + Math.round(size * ky) + 'px "Microsoft YaHei",Arial,sans-serif';
        c.fillStyle = color || '#f2f2f2'; c.textAlign = align || 'center'; c.textBaseline = 'middle';
        c.fillText(s, 0, 0); c.restore();
      },
      rect: function (x, y, rw, rh, color, fill) {
        c.strokeStyle = color || '#eee'; c.lineWidth = 0.02 * kx;
        if (fill) { c.fillStyle = color; c.fillRect(api.x(x - rw / 2), api.y(y + rh / 2), rw * kx, rh * ky); }
        else c.strokeRect(api.x(x - rw / 2), api.y(y + rh / 2), rw * kx, rh * ky);
      },
      pad: function (x, y) {
        c.fillStyle = '#c9a23a'; c.beginPath(); c.arc(api.x(x), api.y(y), 0.082 * kx, 0, 7); c.fill();
        c.fillStyle = '#0b0b0b'; c.beginPath(); c.arc(api.x(x), api.y(y), 0.04 * kx, 0, 7); c.fill();
      },
      trace: function (pts, color) {
        c.strokeStyle = color; c.lineWidth = 0.018 * kx; c.beginPath();
        pts.forEach(function (p, i) { if (i) c.lineTo(api.x(p[0]), api.y(p[1])); else c.moveTo(api.x(p[0]), api.y(p[1])); });
        c.stroke();
      }
    };
    draw(api, c);
    var tex = new T.CanvasTexture(cv);
    tex.encoding = T.sRGBEncoding;
    tex.anisotropy = renderer.capabilities.getMaxAnisotropy();
    return new T.MeshStandardMaterial({ map: tex, roughness: 0.5, metalness: 0.05 });
  }
  // 印制板：本地 z=0 是正面，向 -z 是板厚。四周画一些走线增加真实感
  function pcb(parent, w, h, color, draw, renderer, thick) {
    var t = thick || 0.16;
    var edge = std(color, 0.6);
    box(parent, w, h, t, edge, 0, 0, -t / 2);
    var top = new T.Mesh(new T.PlaneGeometry(w, h), silkMaterial(w, h, color, draw, renderer));
    top.position.z = 0.002;
    top.receiveShadow = true;
    parent.add(top);
  }
  // 右边缘带缺口的板面：x 以右、y0~y1 之间挖掉
  function notchedShape(w, h, n) {
    var s = new T.Shape();
    s.moveTo(-w / 2, -h / 2); s.lineTo(w / 2, -h / 2); s.lineTo(w / 2, n.y0); s.lineTo(n.x, n.y0);
    s.lineTo(n.x, n.y1); s.lineTo(w / 2, n.y1); s.lineTo(w / 2, h / 2); s.lineTo(-w / 2, h / 2); s.closePath();
    return s;
  }
  function notchedFace(parent, w, h, n, color, draw, renderer) {
    var geo = new T.ShapeGeometry(notchedShape(w, h, n)), p = geo.attributes.position, uv = geo.attributes.uv;
    for (var i = 0; i < p.count; i++) uv.setXY(i, (p.getX(i) + w / 2) / w, (p.getY(i) + h / 2) / h);
    var m = new T.Mesh(geo, silkMaterial(w, h, color, draw, renderer));
    m.position.z = 0.002; m.receiveShadow = true;
    parent.add(m);
  }
  function usbC(parent, x, y, rot) {
    var g = new T.Group(); g.position.set(x, y, 0.17); g.rotation.z = rot || 0; parent.add(g);
    box(g, 0.9, 0.75, 0.33, METAL, 0, 0, 0);
    box(g, 0.72, 0.1, 0.17, BLACK, 0, 0.34, 0, false);
    return g;
  }
  function button(parent, x, y) {
    box(parent, 0.45, 0.4, 0.14, METAL, x, y, 0.07);
    cyl(parent, 0.11, 0.1, BLACK, x, y, 0.18, 12);
  }
  function chip(parent, w, h, x, y, zh, legs, label, renderer) {
    box(parent, w, h, zh || 0.1, CHIP, x, y, (zh || 0.1) / 2);
    if (legs) {
      var n = Math.floor(w / 0.1);
      for (var i = 0; i < n; i++) {
        var o = -w / 2 + 0.05 + i * (w / n);
        box(parent, 0.03, 0.09, 0.03, METAL, x + o, y + h / 2 + 0.04, 0.02, false);
        box(parent, 0.03, 0.09, 0.03, METAL, x + o, y - h / 2 - 0.04, 0.02, false);
        box(parent, 0.09, 0.03, 0.03, METAL, x + w / 2 + 0.04, y + o, 0.02, false);
        box(parent, 0.09, 0.03, 0.03, METAL, x - w / 2 - 0.04, y + o, 0.02, false);
      }
    }
    if (label) {
      var cv = document.createElement('canvas'); cv.width = 512; cv.height = Math.round(512 * h / w);
      var c = cv.getContext('2d'); c.fillStyle = '#121417'; c.fillRect(0, 0, cv.width, cv.height);
      c.fillStyle = '#9aa0a6'; c.font = 'bold ' + Math.round(cv.width / 14) + 'px Arial'; c.textAlign = 'center'; c.textBaseline = 'middle';
      label.split('\n').forEach(function (s, i, a) { c.fillText(s, cv.width / 2, cv.height / 2 + (i - (a.length - 1) / 2) * cv.width / 11); });
      var tx = new T.CanvasTexture(cv); tx.encoding = T.sRGBEncoding; tx.anisotropy = renderer.capabilities.getMaxAnisotropy();
      var m = new T.Mesh(new T.PlaneGeometry(w, h), new T.MeshStandardMaterial({ map: tx, roughness: 0.5 }));
      m.position.set(x, y, (zh || 0.1) + 0.002); parent.add(m);
    }
  }
  function fpc(parent, w, x, y, rot) {
    var g = new T.Group(); g.position.set(x, y, 0); g.rotation.z = rot || 0; parent.add(g);
    box(g, w, 0.42, 0.2, WHITE, 0, 0, 0.1);
    box(g, w - 0.12, 0.16, 0.06, BLACK, 0, -0.14, 0.23, false);
    for (var i = 0; i < w / 0.08; i += 2) box(g, 0.03, 0.08, 0.02, GOLD, -w / 2 + 0.06 + i * 0.08, 0.25, 0.01, false);
  }

  /* ---------- 引脚 ---------- */
  function Registry() { this.pins = {}; this.hits = []; }
  // 在 parent 上加一根针。(x,y) 是本地位置，dir 是针尖朝向（本地单位向量），len 针尖长度
  Registry.prototype.pin = function (parent, id, x, y, dir, len, label, opt) {
    opt = opt || {};
    var L = len === undefined ? 0.6 : len, a = new T.Object3D();
    var z = opt.z || 0;
    a.position.set(x, y, z);
    parent.add(a);
    if (L > 0) {
      var m = new T.Mesh(PIN_GEO, GOLD);
      m.scale.z = L;
      m.position.copy(new T.Vector3(dir[0], dir[1], dir[2]).multiplyScalar(L / 2));
      m.lookAt(new T.Vector3(dir[0], dir[1], dir[2]).multiplyScalar(L));
      a.add(m);
      if (!opt.noBase) {
        var b = new T.Mesh(new T.BoxGeometry(0.25, 0.25, 0.25), PLASTIC);
        b.position.copy(new T.Vector3(dir[0], dir[1], dir[2]).multiplyScalar(0.125));
        a.add(b);
      }
    }
    var hit = new T.Mesh(HIT_GEO, HIT_MAT);
    hit.position.set(dir[0] * L, dir[1] * L, dir[2] * L);
    hit.userData.pinId = id;
    a.add(hit);
    this.hits.push(hit);
    var tip = new T.Object3D(); tip.position.copy(hit.position); a.add(tip);
    this.pins[id] = { id: id, label: label || id.split('.')[1], tip: tip, anchor: a, dir: new T.Vector3(dir[0], dir[1], dir[2]), note: opt.note || '', verified: opt.verified !== false };
    return this.pins[id];
  };

  /* ---------- SG90 舵机 ---------- */
  function servo(parent, reg, id, name, wireDir) {
    var g = new T.Group(); parent.add(g);
    var blue = std(0x2a58b0, 0.55);
    box(g, 2.28, 1.22, 2.25, blue, 0, 0, 1.125);
    box(g, 3.2, 1.22, 0.24, blue, 0, 0, 1.75);   // 安装耳
    var stk = new T.Mesh(new T.PlaneGeometry(1.7, 1.2), std(0x1b2030, 0.6));
    stk.rotation.x = Math.PI / 2; stk.position.set(0, -0.615, 1.05); g.add(stk);
    var lab = new T.Mesh(new T.PlaneGeometry(0.9, 0.3), std(0xe8c52a, 0.5));
    lab.rotation.x = Math.PI / 2; lab.position.set(0, -0.62, 1.3); g.add(lab);
    cyl(g, 0.56, 0.28, blue, 0.5, 0, 2.35, 20);
    cyl(g, 0.24, 0.3, WHITE, 0.5, 0, 2.6, 16);
    // 三根线从侧面引出
    [['S', '信号（橙）'], ['V', '电源（红）'], ['N', '地（棕）']].forEach(function (w, i) {
      reg.pin(g, id + '.' + w[0], -1.14, -0.09 + i * 0.09, wireDir || [-1, 0, 0], 0, name + ' ' + w[1], { z: 0.35, noBase: true });
    });
    return g;
  }

  /* ---------- ESP32-S3-CAM ---------- */
  var ESP_L = ['GND', 'G19', 'G20', 'G21', 'G47', 'G48', 'G45', 'G0', 'G35', 'G36', 'G37', 'G38', 'G39', 'G40', 'G41', 'G42', 'G2', 'G1', 'RXD', 'TXD'];
  var ESP_R = ['5V', 'G14', 'G13', 'G12', 'G11', 'G10', 'G9', 'G46', 'G3', 'G8', 'G18', 'G17', 'G16', 'G15', 'G7', 'G6', 'G5', 'G4', 'EN', '3V3'];
  function esp32(parent, reg, key, renderer) {
    var W = 2.86, H = 6.1, P = 0.254, g = new T.Group(); parent.add(g);
    var rowX = 1.27, y0 = H / 2 - 0.82;
    pcb(g, W, H, '#17191c', function (s) {
      ESP_L.forEach(function (n, i) { var y = y0 - i * P; s.pad(-rowX, y); s.text(-rowX + 0.14, y, n, 0.1, '#eee', 'left'); });
      ESP_R.forEach(function (n, i) { var y = y0 - i * P; s.pad(rowX, y); s.text(rowX - 0.14, y, n, 0.1, '#eee', 'right'); });
      s.text(0, 0.25, 'GOOUUU', 0.13); s.text(0, 0.08, 'ESP32-S3-CAM', 0.14); s.text(0.9, 0.17, 'V1.3', 0.09);
      s.text(-0.55, 2.05, 'BOOT', 0.09); s.text(0.86, 2.05, 'RST', 0.09);
      s.text(-1.1, 2.8, 'TTL', 0.11); s.text(1.1, 2.8, 'OTG', 0.11);
      s.text(0, -3.0, key === 'A' ? 'ESP32-A' : 'ESP32-B', 0.09, '#8fd');
    }, renderer);
    usbC(g, -0.55, H / 2 - 0.36); usbC(g, 0.55, H / 2 - 0.36);
    reg.pin(g, key + '.USB', -0.55, H / 2 + 0.05, [0, 1, 0], 0.5, 'USB（TTL 口）', { z: 0.17, noBase: true, note: '供电、下载和日志' });
    chip(g, 1.05, 0.5, 0, 1.8, 0.09, true, 'CH343', renderer);
    button(g, -0.62, 2.28); button(g, 0.78, 2.28);
    box(g, 0.14, 0.12, 0.05, std(0xf4f4f4), -0.25, 1.15, 0.03);
    fpc(g, 1.75, 0, -0.4);
    // 模组金属罩、天线
    box(g, 1.6, 1.8, 0.22, METAL, 0, -2.18, 0.11);
    box(g, 1.6, 0.6, 0.1, CHIP, 0, -3.2, 0.05);
    var flex = box(g, 0.28, 0.65, 0.02, std(0xc9771c, 0.5), 0, -1.0, 0.25, false);
    flex.rotation.x = 0.1;
    // OV2640 摄像头
    box(g, 0.85, 0.85, 0.42, BLACK, 0, -2.2, 0.43);
    cyl(g, 0.3, 0.3, std(0x0a0a0c, 0.35), 0, -2.2, 0.75);
    cyl(g, 0.16, 0.03, new T.MeshStandardMaterial({ color: 0x223044, metalness: 0.6, roughness: 0.08 }), 0, -2.2, 0.91);
    // 排针朝板背
    ESP_L.forEach(function (n, i) { reg.pin(g, key + '.' + n, -rowX, y0 - i * P, [0, 0, -1], 0.75, n); });
    ESP_R.forEach(function (n, i) { reg.pin(g, key + '.' + n, rowX, y0 - i * P, [0, 0, -1], 0.75, n); });
    return g;
  }

  /* ---------- KIT_PSE84_AI + OV7675 转接板 ---------- */
  var J14_T = ['GND', 'SDA', 'HS', 'XCLK', 'D6', 'D4', 'D2', 'D0', 'NC', 'PDN'];
  var J14_B = ['VCC', 'SCL', 'VS', 'PCLK', 'D7', 'D5', 'D3', 'D1', 'NC', 'PEN'];
  var J5 = ['VDD_1V8', 'P9_3', 'P9_2', 'P9_1', 'P9_0', 'GND'];
  function e84(parent, reg, renderer) {
    var W = 7.2, H = 4.4, g = new T.Group(); parent.add(g);
    var jx = -0.05, jy = H / 2 - 0.45, P = 0.254;
    pcb(g, W, H, '#131417', function (s) {
      s.text(2.6, -1.3, 'PSOC Edge', 0.14); s.text(2.6, -1.5, 'E84 AI KIT', 0.13);
      s.text(2.75, -1.75, 'infineon', 0.16, '#d0d4d8', 'center', 0, true);
      s.rect(jx, -H / 2 + 0.42, P * 6 + 0.1, 0.32, '#2f7fe0');
      s.text(jx - 0.95, -H / 2 + 0.42, 'J5', 0.11);
      s.text(jx - 0.68, -H / 2 + 0.68, '1', 0.09);
      s.text(jx + 0.68, -H / 2 + 0.68, '6', 0.09);
      s.text(-1, H / 2 - 0.2, 'J14', 0.1);
      s.text(0.0, -0.1, 'PSE846GPS2DBZC4A', 0.07, '#888');
      for (var i = 0; i < 40; i++) s.rect(-3 + (i * 0.71) % 5.7, -1.7 + (i * 1.37) % 3.2, 0.08, 0.04, '#6b6f75', true);
    }, renderer);
    usbC(g, -W / 2 + 0.45, 0.72, -Math.PI / 2); usbC(g, -W / 2 + 0.45, -0.72, -Math.PI / 2);
    reg.pin(g, 'E.USB', -W / 2, 0.72, [-0.5, 0, 0.86], 0.5, 'USB（KitProg3 口）', { z: 0.17, noBase: true, note: '两个 USB-C 哪个是 KitProg3 以板上丝印为准。泡沫不要挡住插头' });
    chip(g, 1.25, 1.25, 0, -0.55, 0.12, false, 'PSE846\nGPS2DBZC4A', renderer);
    box(g, 0.9, 1.0, 0.2, METAL, 1.55, -0.45, 0.1);          // Wi-Fi 模组
    box(g, 0.45, 2.3, 0.28, std(0x9c6a3e, 0.55), W / 2 - 0.45, 0.45, 0.14);  // MIPI 连接器
    button(g, -1.4, 0.4); button(g, -0.85, 0.4);
    chip(g, 1.05, 0.65, 0.3, 0.75, 0.1, false, null, renderer);
    // J5：6 针，朝下引出。第 1 针在左（对照官方原理图，以板上丝印为准）
    J5.forEach(function (n, i) {
      reg.pin(g, 'E.J5_' + (i + 1), jx - 0.635 + i * P, -H / 2 + 0.42, [0, -0.5, 0.86], 0.6, 'J5-' + (i + 1) + ' ' + n,
        { note: '官方原理图：J5 第 ' + (i + 1) + ' 针 = ' + n + '。J5 默认不焊，需自行焊接', z: 0.02 });
    });
    // J14 + OV7675 转接板
    var cam = new T.Group(); cam.position.set(jx, 1.25, 0.95); g.add(cam);
    var CW = 3.5, CH = 3.5, rowY = [-CH / 2 + 0.42, -CH / 2 + 0.17];
    pcb(cam, CW, CH, '#1f7a3a', function (s) {
      s.text(0, CH / 2 - 0.3, 'OV7675', 0.22, '#f2f2f2', 'center', 0, true);
      s.rect(0, -0.65, CW - 0.5, 0.95, '#f2f2f2');
      J14_T.forEach(function (n, i) {
        var x = -1.14 + i * P;
        s.pad(x, rowY[0]); s.pad(x, rowY[1]);
        s.text(x, -0.42, n, 0.085, '#fff', 'center', Math.PI / 2);
        s.text(x, -0.82, J14_B[i], 0.085, '#fff', 'center', Math.PI / 2);
      });
      s.text(-1.45, rowY[0], '2', 0.09); s.text(-1.45, rowY[1], '1', 0.09);
      s.text(1.5, rowY[0], '20', 0.09); s.text(1.5, rowY[1], '19', 0.09);
      [[-1.4, 1.4], [1.4, 1.4], [-1.4, -0.1], [1.4, -0.1]].forEach(function (p) { s.pad(p[0], p[1]); s.pad(p[0] + 0.01, p[1]); });
    }, renderer);
    box(cam, 0.8, 0.8, 0.3, BLACK, 0.3, 0.75, 0.15);
    cyl(cam, 0.3, 0.3, std(0x0a0a0c, 0.35), 0.3, 0.75, 0.42);
    cyl(cam, 0.16, 0.03, new T.MeshStandardMaterial({ color: 0x223044, metalness: 0.6, roughness: 0.08 }), 0.3, 0.75, 0.58);
    box(cam, 0.4, 0.9, 0.12, BLACK, -0.75, 0.75, 0.06);
    box(cam, 0.65, 0.3, 0.01, std(0xc9771c, 0.5), -0.25, 0.75, 0.05, false);
    // 插座
    box(g, 2.6, 0.55, 0.85, PLASTIC, jx, cam.position.y - CH / 2 + 0.3, 0.42);
    return { group: g, lensLocal: new T.Vector3(jx + 0.3, 2.0, 1.55) };
  }

  /* ---------- H7（LXB743ZI-P1，朝上的一面是有丝印的背面） ---------- */
  function h7Names() {
    var a = [];
    ['A', 'B', 'C', 'D', 'E', 'F', 'G'].forEach(function (p) { for (var i = 0; i < 16; i++) a.push('P' + p + i); });
    // 112 个 IO + 8 个电源。电源放在每排两端，用到的命名为 A/B 以区分
    var pw = ['5VA', 'GNDA', '3V3A', 'GNDB', '5VB', 'GNDC', '3V3B', 'GNDD'];
    return { io: a, pw: pw };
  }
  function h7(parent, reg, renderer) {
    var W = 7.62, H = 4.18, P = 0.254, COLOR = '#14161a', g = new T.Group(); parent.add(g);
    var n = h7Names(), rows = [[], [], [], []], order = n.io.slice();
    // 每排 30 针：两端各放一个电源，其余按编号
    var seq = 0, pi = 0;
    for (var r = 0; r < 4; r++) for (var i = 0; i < 30; i++) {
      if ((i === 0 || i === 29) && pi < n.pw.length) rows[r].push(n.pw[pi++]);
      else rows[r].push(order[seq++]);
    }
    var y = [H / 2 - 0.13, H / 2 - 0.384, -H / 2 + 0.384, -H / 2 + 0.13], x0 = -(29 * P) / 2;
    // 以下位置都从宣传图量取。4 针弯排针所在的缺口、固定孔（x, y, 半径）、弯排针各针的 y
    var NOTCH = { x: 2.98, y0: 0.36, y1: 1.42 }, DBG_Y = [1.31, 1.056, 0.802, 0.548];
    var HOLES = [[1.54, 1.25, 0.18], [1.54, -1.22, 0.18], [-3.42, 1.25, 0.18], [-3.42, -1.22, 0.18], [-2.79, 1.0, 0.13], [-2.79, -1.0, 0.13]];
    function holes(s, c, flip) {
      HOLES.forEach(function (h) {
        var cx = s.x(h[0]), cy = s.y(flip ? -h[1] : h[1]), k = s.x(1) - s.x(0);
        c.fillStyle = '#c9a23a'; c.beginPath(); c.arc(cx, cy, h[2] * k, 0, 7); c.fill();
        c.fillStyle = '#2a2c30'; c.beginPath(); c.arc(cx, cy, (h[2] - 0.05) * k, 0, 7); c.fill();
      });
    }
    var body = new T.Mesh(new T.ExtrudeGeometry(notchedShape(W, H, NOTCH), { depth: 0.16, bevelEnabled: false }), std(COLOR, 0.6));
    body.position.z = -0.16; body.castShadow = true; body.receiveShadow = true; g.add(body);
    // 丝印面（杜邦线从这一面插）
    notchedFace(g, W, H, NOTCH, COLOR, function (s, c) {
      rows.forEach(function (row, ri) {
        row.forEach(function (name, i) {
          var xx = x0 + i * P;
          s.pad(xx, y[ri]);
          var ty = ri < 2 ? y[1] - (ri === 1 ? 0.175 : 0.245) : y[2] + (ri === 2 ? 0.175 : 0.245);
          s.text(xx, ty, name.replace(/[AB]$/, function (m) { return /^(5V|3V3|GND)/.test(name) ? '' : m; }).replace(/[CD]$/, function (m) { return /^GND/.test(name) ? '' : m; }), 0.07, '#e8e8e8', 'center');
        });
      });
      holes(s, c, false);
      s.rect(0.2, 0.85, 2.15, 0.34, '#e8e8e8', true);
      s.text(0.2, 0.85, 'LXB743ZI-P1', 0.2, '#111', 'center', 0, true);
      s.text(0.2, 0.35, '鹿小班', 0.3, '#f2f2f2', 'center', 0, true);
      s.text(0.2, -0.75, '引脚名位置待核对', 0.16, '#ffb74d');
      ['RXD?', 'TXD?', 'RST?', '3V3?'].forEach(function (n, i) { s.text(2.85, DBG_Y[i], n, 0.09, '#ffb74d', 'right'); });
    }, renderer);
    // TF 卡座
    box(g, 1.4, 1.5, 0.2, std(0x9da2a8, 0.35, 0.8), -W / 2 + 0.72, -0.1, 0.1);
    box(g, 0.3, 1.1, 0.05, BLACK, -W / 2 + 0.17, -0.1, 0.17, false);
    // 芯片面。绕 x 轴翻转，所以本地 y = -板面 y
    var bot = new T.Group(); bot.rotation.x = Math.PI; bot.position.z = -0.16; g.add(bot);
    notchedFace(bot, W, H, { x: NOTCH.x, y0: -NOTCH.y1, y1: -NOTCH.y0 }, COLOR, function (s, c) {
      rows.forEach(function (row, ri) { row.forEach(function (name, i) { s.pad(x0 + i * P, -y[ri]); }); });
      holes(s, c, true);
      ['CLK', 'DIO', 'GND', '5V'].forEach(function (n, i) { s.text(2.5, -DBG_Y[i], n, 0.13, '#f2f2f2', 'right', 0, true); });
      s.text(2.82, -0.135, 'BOOT0', 0.13, '#f2f2f2', 'center', Math.PI / 2, true);
      s.text(2.82, 0.34, 'RST', 0.13, '#f2f2f2', 'center', Math.PI / 2, true);
      s.text(-2.35, 0, 'LCD', 0.2, '#f2f2f2', 'center', Math.PI / 2, true);
      s.text(-2.75, 0, 'CAMERA', 0.2, '#f2f2f2', 'center', Math.PI / 2, true);
    }, renderer);
    chip(bot, 2.0, 2.0, 0, 0, 0.14, true, 'STM32H743\nZIT6', renderer);
    fpc(bot, 0.96, -1.89, 0, Math.PI / 2); fpc(bot, 1.63, -3.4, 0, Math.PI / 2);
    button(bot, 3.46, -0.135); button(bot, 3.46, 0.34);
    usbC(bot, 3.42, 0.98, -Math.PI / 2);
    reg.pin(g, 'H7.USB', 3.87, -0.98, [1, 0, 0], 0.5, 'USB', { z: -0.17, noBase: true, verified: false, note: 'H7 板没有原理图，上电后看电源灯确认这个口能供电' });
    // 4 针弯排针：焊在芯片面，针尖伸出缺口。丝印两面对不上，本项目接线也不用它
    box(bot, 0.26, 1.02, 0.25, std(0xf2d21b, 0.6), 3.11, -(DBG_Y[0] + DBG_Y[3]) / 2, 0.125);
    DBG_Y.forEach(function (py, k) {
      box(bot, 0.4, 0.064, 0.064, GOLD, 2.8, -py, 0.125, false);
      reg.pin(g, 'H7.DBG' + k, 3.24, py, [1, 0, 0], 0.55, ['CLK', 'DIO', 'GND', '5V'][k],
        { z: -0.285, noBase: true, verified: false, note: '宣传图芯片面印的是 CLK/DIO/GND/5V；丝印面照片模糊，疑似 RXD/TXD/RST/3V3。两者矛盾，使用前以实物为准。本项目接线不用这个排针' });
    });
    // 排针朝上（实物杜邦线从这一面插）
    rows.forEach(function (row, ri) {
      row.forEach(function (name, i) {
        reg.pin(g, 'H7.' + name, x0 + i * P, y[ri], [0, 0, 1], 0.6, name.replace(/([AB]|[CD])$/, function (m) { return /^(5V|3V3|GND)/.test(name) ? '' : m; }),
          { note: H7_PINS_VERIFIED ? '' : '引脚名正确；在排针上的物理位置未核对，接线时以实物丝印为准', verified: H7_PINS_VERIFIED });
      });
    });
    return g;
  }

  /* ---------- HC-SR04 ---------- */
  function hcsr04(parent, reg, renderer) {
    var g = new T.Group(); parent.add(g);
    var W = 4.5, H = 2.0;
    pcb(g, W, H, '#1c4f9c', function (s) {
      s.text(0, 0.72, 'HC-SR04', 0.17, '#f2f2f2', 'center', 0, true);
      ['VCC', 'TRIG', 'ECHO', 'GND'].forEach(function (n, i) {
        var x = -0.38 + i * 0.254;
        s.pad(x, -0.88); s.text(x, -0.55, n, 0.085, '#fff', 'center', Math.PI / 2);
      });
      s.text(-2.05, -0.8, 'T', 0.13); s.text(2.05, -0.8, 'R', 0.13);
    }, renderer);
    [-1.3, 1.3].forEach(function (x) {
      cyl(g, 0.8, 1.2, METAL, x, 0, 0.6);
      cyl(g, 0.68, 0.02, std(0x2b2e33, 0.95), x, 0, 1.21);
      for (var r = 0.15; r < 0.68; r += 0.13) { var ring = new T.Mesh(new T.TorusGeometry(r, 0.012, 6, 28), std(0x8a8d92, 0.5, 0.6)); ring.position.set(x, 0, 1.225); g.add(ring); }
    });
    box(g, 0.75, 0.3, 0.3, METAL, 0, 0.45, 0.15);
    chip(g, 0.5, 0.35, 0, -0.1, 0.08, false, null, renderer);
    ['VCC', 'TRIG', 'ECHO', 'GND'].forEach(function (n, i) {
      reg.pin(g, 'R.' + n, -0.38 + i * 0.254, -0.88, [0, -1, 0], 0.6, n, { z: -0.1 });
    });
    return g;
  }

  /* ---------- 面包板、电平转换、分压电阻 ---------- */
  function breadboard(parent, reg, renderer) {
    var W = 8.3, H = 5.5, g = new T.Group(); parent.add(g);
    var body = std(0xf1eee4, 0.7);
    box(g, W, H, 0.85, body, 0, 0, -0.425);
    var top = new T.Mesh(new T.PlaneGeometry(W, H), silkMaterial(W, H, '#f1eee4', function (s, c) {
      var i, j;
      for (i = 0; i < 30; i++) for (j = 0; j < 10; j++) {
        var x = -3.7 + i * 0.254, y = (j < 5 ? -1.45 + j * 0.254 : 0.42 + (j - 5) * 0.254);
        c.fillStyle = '#2a2a2a'; c.fillRect(s.x(x) - 5, s.y(y) - 5, 10, 10);
      }
      s.trace([[-3.9, 2.45], [3.9, 2.45]], '#d22'); s.trace([[-3.9, 2.0], [3.9, 2.0]], '#26a');
      s.trace([[-3.9, -2.45], [3.9, -2.45]], '#d22'); s.trace([[-3.9, -2.0], [3.9, -2.0]], '#26a');
      for (i = 0; i < 24; i++) [2.3, 2.15, -2.15, -2.3].forEach(function (yy) { c.fillStyle = '#2a2a2a'; c.fillRect(s.x(-3.5 + i * 0.3) - 5, s.y(yy) - 5, 10, 10); });
      s.text(-4.0, 2.3, '+', 0.2, '#d22'); s.text(-4.0, 2.15, '−', 0.2, '#26a');
    }, renderer));
    top.position.z = 0.002; top.receiveShadow = true; g.add(top);
    // 电平转换模块：TXS0102（WIRING.md 第 1 节）
    var ls = new T.Group(); ls.position.set(-1.8, -0.3, 0.75); g.add(ls);
    var names = [['HV', 'HV1', 'HV2', 'GND'], ['LV', 'LV1', 'LV2', 'GND2']];
    pcb(ls, 1.6, 1.45, '#1a4fa0', function (s) {
      names[0].forEach(function (n, i) { var y = 0.38 - i * 0.254; s.pad(0.65, y); s.text(0.5, y, n.replace('2', n === 'GND2' ? '' : '2'), 0.085, '#fff', 'right'); });
      names[1].forEach(function (n, i) { var y = 0.38 - i * 0.254; s.pad(-0.65, y); s.text(-0.5, y, n === 'GND2' ? 'GND' : n, 0.085, '#fff', 'left'); });
      s.text(0, 0.6, '3.3V ⇄ 1.8V', 0.09, '#ffd54f');
    }, renderer);
    chip(ls, 0.5, 0.4, 0, -0.15, 0.08, false, null, renderer);
    names[0].forEach(function (n, i) { reg.pin(ls, 'LS.' + n, 0.65, 0.38 - i * 0.254, [0, 0, 1], 0.5, n, { note: '3.3 V 侧' }); });
    names[1].forEach(function (n, i) { reg.pin(ls, 'LS.' + n, -0.65, 0.38 - i * 0.254, [0, 0, 1], 0.5, n === 'GND2' ? 'GND' : n, { note: '1.8 V 侧（TXS0102 的 A 侧），接 J5' }); });
    for (var q = 0; q < 8; q++) box(g, 0.05, 0.05, 0.75, GOLD, (q < 4 ? 0.65 : -0.65) - 1.8, -0.3 + 0.38 - (q % 4) * 0.254, 0.37, false);

    // ECHO 分压：2.2 kΩ（红红红）+ 3.3 kΩ（橙橙红）
    function resistor(x, y, bands) {
      var r = new T.Group(); r.position.set(x, y, 0.18); g.add(r);
      var b = new T.Mesh(new T.CylinderGeometry(0.11, 0.11, 0.6, 12), std(0xd3b58a, 0.55)); r.add(b);
      var lead = new T.Mesh(new T.CylinderGeometry(0.018, 0.018, 1.27, 6), METAL); r.add(lead);
      bands.forEach(function (c, i) { var d = new T.Mesh(new T.CylinderGeometry(0.115, 0.115, 0.05, 12), std(c, 0.5)); d.position.y = -0.16 + i * 0.13; r.add(d); });
      return r;
    }
    resistor(1.55, 0.7, [0xcc2222, 0xcc2222, 0xcc2222]);
    resistor(2.45, 0.7, [0xe67a1e, 0xe67a1e, 0xcc2222]);
    var div = { z: 0.02 };
    reg.pin(g, 'BB.DIV_IN', 1.55, 1.35, [0, 0, 1], 0, '分压输入（ECHO，5 V）', { z: 0.02, noBase: true, note: '经 2.2 kΩ 到分压点' });
    reg.pin(g, 'BB.DIV_OUT', 2.0, 0.05, [0, 0, 1], 0, '分压点（约 3.0 V）', { z: 0.02, noBase: true, note: '2.2 kΩ 和 3.3 kΩ 的连接点，接 H7 PC6；3.3 kΩ 另一端接地' });

    var next = { G: 0, V: 0 };
    return { group: g, hole: function (rail) {
      var i = next[rail]++, id = 'BB.' + rail + i;
      var y = rail === 'V' ? 2.3 : 2.15;
      var pos = i < 24 ? [-3.5 + i * 0.3, y] : [-3.5 + (i - 24) * 0.3, rail === 'V' ? -2.3 : -2.15];
      reg.pin(g, id, pos[0], pos[1], [0, 0, 1], 0, rail === 'V' ? '舵机电源正轨' : '公共地轨', { noBase: true, z: 0.02 });
      return id;
    } };
  }

  function usbTtl(parent, reg, renderer) {
    var g = new T.Group(); parent.add(g);
    var names = ['GND', 'RXD', 'TXD', '3V3', '5V'];
    pcb(g, 4.4, 1.6, '#a81f2a', function (s) {
      s.text(0.1, 0.5, 'USB-TTL', 0.15, '#fff', 'center', 0, true);
      names.forEach(function (n, i) { var x = 1.9, y = 0.5 - i * 0.254; s.pad(x, y); s.text(x - 0.15, y, n, 0.09, '#fff', 'right'); });
    }, renderer);
    box(g, 1.2, 1.2, 0.42, METAL, -2.6, 0, 0.2);
    chip(g, 0.7, 0.5, -0.4, -0.15, 0.1, false, null, renderer);
    names.forEach(function (n, i) { reg.pin(g, 'TTL.' + n, 1.9, 0.5 - i * 0.254, [0, 0, 1], 0.6, n); });
    reg.pin(g, 'TTL.USB', -3.25, 0, [-1, 0, 0], 0.5, 'USB', { z: 0.2, noBase: true, note: '接电脑' });
    return g;
  }

  // 雷达旁的蓝色 USB 升压板（作品提交材料/概图.jpg）。外形按照片估计，型号和规格未知
  function psu(parent, reg, renderer) {
    var g = new T.Group(); parent.add(g);
    pcb(g, 5.2, 1.7, '#1d5fb0', function (s) {
      s.text(0.5, -0.62, '舵机电源 · 规格待实测', 0.11, '#fff');
      s.text(2.05, 0.55, 'OUT', 0.09, '#fff');
    }, renderer);
    box(g, 1.3, 1.25, 0.45, METAL, -3.1, 0, 0.1);      // USB-A 插头
    reg.pin(g, 'PSU.USB', -3.8, 0, [-1, 0, 0], 0.4, 'USB 输入', { z: 0.1, noBase: true, note: '插独立充电头，不接电脑' });
    box(g, 1.5, 0.85, 0.5, BLACK, 0.2, 0.15, 0.25);    // 数码管
    [-0.45, 0, 0.45].forEach(function (x) { box(g, 0.34, 0.6, 0.02, std(0xd93a2b, 0.4), 0.2 + x, 0.15, 0.51, false); });
    box(g, 0.5, 0.5, 0.25, std(0x2e7bd1, 0.5), -1.4, 0.2, 0.125);   // 调压电位器
    [['P', 0.2, '电源 +'], ['N', -0.2, '电源 −']].forEach(function (q) {
      reg.pin(g, 'PSU.' + q[0], 2.2, q[1], [0, 0, 1], 0.6, q[2], { note: '雷达旁的蓝色 USB 升压板。先调到 5.0~5.2 V 再接舵机，要求 ≥3 A，不达标就换 5 V 5 A 适配器' });
    });
    return g;
  }

  // USB 电源：开发板共用一个，舵机单独一个充电头，舵机启动电流不影响开发板。外形示意
  function usbSource(parent, reg, renderer, prefix, title, ports, note) {
    var g = new T.Group(); parent.add(g);
    var W = Math.max(3.4, ports.length * 1.0 + 0.6);
    box(g, W, 2.2, 1.1, std(0xe8e8e4, 0.8), 0, 0, 0.55);
    var top = new T.Group(); top.position.z = 1.1; g.add(top);
    pcb(top, W, 0.9, '#30343a', function (s) { s.text(0, 0, title, 0.13, '#fff', 'center', 0, true); }, renderer);
    ports.forEach(function (k, i) {
      var x = (i - (ports.length - 1) / 2) * 1.0;
      box(g, 0.6, 0.1, 0.3, BLACK, x, 1.1, 0.6, false);
      reg.pin(g, prefix + '.' + k, x, 1.15, [0, 1, 0], 0.3, k + ' 口', { z: 0.6, noBase: true, note: note });
    });
    return g;
  }

  // 自制舵机电源分配板：洞洞板加 7 组 3 针排针，舵机插头插这里。见 WIRING.md 第 7 节
  var SERVO_KEYS = ['SR', 'SAP', 'SAT', 'SBP', 'SBT', 'SEP', 'SET'];
  function driverBoard(parent, reg, renderer) {
    var g = new T.Group(); parent.add(g);
    var W = 5.6, H = 3.4, P = 0.3;
    pcb(g, W, H, '#20262c', function (s) {
      s.text(0, 1.45, '舵机电源分配板', 0.14, '#ffd54f', 'center', 0, true);
      SERVO_KEYS.forEach(function (k, i) {
        var x = -1.0 + i * P * 1.0;
        s.text(x, 0.95, k, 0.075, '#eee', 'center', Math.PI / 2);
      });
      s.text(-1.5, 0.5, 'S', 0.11, '#ffcc80'); s.text(-1.5, 0.15, 'V', 0.11, '#ff8a80'); s.text(-1.5, -0.2, 'G', 0.11, '#ddd');
      s.text(-2.2, -1.25, 'VIN  GND', 0.1, '#fff');
    }, renderer);
    SERVO_KEYS.forEach(function (k, i) {
      var x = -1.0 + i * P * 1.0;
      reg.pin(g, 'DRV.S_' + k, x, 0.5, [0, 0, 1], 0.6, k + ' 信号', { note: '舵机橙线插这里，各 S 针互不相连，用杜邦线接到 H7' });
      reg.pin(g, 'DRV.V_' + k, x, 0.15, [0, 0, 1], 0.6, k + ' 电源', { note: 'V 排连成一条，接 VIN' });
      reg.pin(g, 'DRV.G_' + k, x, -0.2, [0, 0, 1], 0.6, k + ' 地');
    });
    // 输入接线端子和 1000 µF 电容
    box(g, 1.5, 0.75, 0.75, std(0x2d7d46, 0.5), -2.0, -0.85, 0.38);
    reg.pin(g, 'DRV.VIN', -2.35, -0.85, [0, 0, 1], 0.1, '电源输入 +', { z: 0.8, noBase: true, note: '接电源，要用粗线' });
    reg.pin(g, 'DRV.GIN', -1.65, -0.85, [0, 0, 1], 0.1, '电源输入 −', { z: 0.8, noBase: true });
    cyl(g, 0.45, 1.0, std(0x15171a, 0.4), 1.9, -0.9, 0.5, 18);
    reg.pin(g, 'DRV.GND', 0.6, -1.1, [0, 0, 1], 0.6, '信号地', { note: '接 H7 的地，舵机信号才有参考' });
    return g;
  }

  /* ---------- 可转动的云台 ---------- */
  // 手切的泡沫块：顶点按位置做小扰动，棱边不至于太整齐
  function foamBlock(parent, w, d, h, x, y, z) {
    var geo = new T.BoxGeometry(w, d, h, 5, 6, 3), p = geo.attributes.position;
    for (var i = 0; i < p.count; i++) {
      var a = p.getX(i), b = p.getY(i), c = p.getZ(i), n = Math.sin(a * 7.1 + b * 3.3 + c * 5.7) * 0.06;
      p.setXYZ(i, a + n, b + Math.sin(a * 2.3 + c * 8.1) * 0.06, c + Math.sin(b * 6.1 + a * 4.9) * 0.06);
    }
    geo.computeVertexNormals();
    var m = new T.Mesh(geo, std(0xf4f3ee, 0.95));
    m.position.set(x, y, z); m.castShadow = true; m.receiveShadow = true; parent.add(m);
    return m;
  }
  // 结构按实物照片（作品提交材料/概图.jpg、ESP32.jpg、84E.jpg），尺寸是估计的：
  //   水平舵机竖放，转轴朝上，长边顺着泡沫块；泡沫块粘在舵盘上，随水平舵机一起转。
  //   俯仰舵机平躺在泡沫块上：贴纸朝上，导线朝后，转轴水平朝一侧，带动前面的泡沫和板子俯仰。
  // opt：rear/front 下层泡沫伸到轴后/轴前多远；blockH、blockW 下层泡沫的高和宽；
  //      neck 板背后那段泡沫 {w, z0, z1}（相对俯仰轴），要给排针和杜邦线留位置；mount 板子中心相对俯仰轴的位置
  function gimbal(parent, reg, key, name, renderer, opt) {
    var base = new T.Group(); parent.add(base);
    var pan = new T.Group(), tilt = new T.Group(), body = new T.Group();
    // body 以水平转轴为原点，安装朝向由 applyLayout 设置
    body.position.set(0.5, 0, 0); base.add(body);
    var ps = servo(body, reg, 'S' + key + 'P', name + ' 水平舵机');
    ps.rotation.z = Math.PI / 2; ps.position.set(0, -0.5, 0);
    pan.position.set(0.5, 0, 2.9); base.add(pan);
    box(pan, 0.5, 3.3, 0.12, WHITE, 0, 0, 0.06);     // 舵盘
    foamBlock(pan, opt.blockW, opt.rear + opt.front, opt.blockH, 0, (opt.front - opt.rear) / 2, 0.12 + opt.blockH / 2);
    // 俯仰舵机平躺：舵机 x → 前，舵机 y → 下（贴纸朝上），舵机 z（转轴）→ 左
    var SY = -0.2, SZ = 0.12 + opt.blockH + 0.61;
    var ts = servo(pan, reg, 'S' + key + 'T', name + ' 俯仰舵机');
    ts.quaternion.setFromRotationMatrix(new T.Matrix4().makeBasis(new T.Vector3(0, 1, 0), new T.Vector3(0, 0, -1), new T.Vector3(-1, 0, 0)));
    ts.position.set(1.1, SY, SZ);
    // 俯仰转轴穿过舵机输出轴，方向左右
    var TY = SY + 0.5;
    tilt.position.set(0, TY, SZ); pan.add(tilt);
    var my = opt.mount[1];
    box(tilt, 0.14, 3.2, 0.5, WHITE, -1.72, 1.6, 0);   // 舵臂
    foamBlock(tilt, 3.7, 1.55, 1.85, 0, 1.52, 0.38);
    foamBlock(tilt, opt.neck.w, my - 2.3 - 0.16, opt.neck.z1 - opt.neck.z0, 0, (2.3 + my - 0.16) / 2, (opt.neck.z0 + opt.neck.z1) / 2);
    // 板面朝前（本地 +y）：板 x → 世界 -x，板 y → 世界 +z，板 z → 世界 +y
    var mount = new T.Group();
    mount.quaternion.setFromRotationMatrix(new T.Matrix4().makeBasis(new T.Vector3(-1, 0, 0), new T.Vector3(0, 0, 1), new T.Vector3(0, 1, 0)));
    mount.position.set(opt.mount[0], my, opt.mount[2]);
    tilt.add(mount);
    return { base: base, body: body, pan: pan, tilt: tilt, mount: mount };
  }

  function build(scene, cfg, renderer) {
    var reg = new Registry(), R = { reg: reg, parts: {} };
    // 亚克力底板
    var plate = new T.Mesh(new T.BoxGeometry(cfg.plate, cfg.plate, 0.5), new T.MeshPhysicalMaterial({ color: 0xcfe8f5, transparent: true, opacity: 0.32, roughness: 0.08, metalness: 0, clearcoat: 1 }));
    // 实物 40 cm 见方（作品提交材料/概图.jpg）：两块 ESP32 在一条对角线上，雷达在另一条对角线上，所以板子相对坐标轴转 45°
    plate.rotation.z = Math.PI / 4;
    plate.position.z = -0.25; plate.receiveShadow = true; scene.add(plate);
    R.plate = plate;

    var bb = breadboard(scene, reg, renderer); bb.group.position.set(2, -7, 0.85);
    R.bb = bb; R.parts.BB = bb.group;
    var ped = new T.Mesh(new T.BoxGeometry(6.2, 3, 3), std(0xf1f0ea, 0.95)); ped.position.set(0, -17, 1.5); ped.castShadow = true; scene.add(ped);
    var h = h7(scene, reg, renderer); h.position.set(0, -17, 3.5); R.parts.H7 = h;
    var ttl = usbTtl(scene, reg, renderer); ttl.position.set(-12, -11, 0.3); R.parts.TTL = ttl;
    // 按照片放在雷达旁边，USB 插头朝外
    var ps = psu(scene, reg, renderer); ps.position.set(-10.5, 16, 0.3); R.parts.PSU = ps;
    var drv = driverBoard(scene, reg, renderer); drv.position.set(11, -11, 0.3); R.parts.DRV = drv;
    var hub = usbSource(scene, reg, renderer, 'HUB', '电脑 / 多口 USB 电源', ['H7', 'A', 'B', 'E', 'TTL'], '开发板供电，每口电流待实测'); hub.position.set(-7, -19.5, 0); R.parts.HUB = hub;
    var chg = usbSource(scene, reg, renderer, 'CHG', '舵机专用充电头', ['USB'], '≥5 V 3 A，不和开发板共用'); chg.position.set(-15, 10, 0); chg.rotation.z = Math.PI / 2; R.parts.CHG = chg;

    // ESP32-A / B
    // 板子下半段粘在泡沫上，摄像头那头朝下；排针朝后，所以板背后的泡沫要收窄
    var espOpt = { rear: 1.7, front: 2.2, blockH: 2.5, blockW: 3.9, neck: { w: 2.0, z0: -0.55, z1: 1.3 }, mount: [0, 4.5, 1.6] };
    var ga = gimbal(scene, reg, 'A', 'ESP32-A', renderer, espOpt), gb = gimbal(scene, reg, 'B', 'ESP32-B', renderer, espOpt);
    esp32(ga.mount, reg, 'A', renderer); esp32(gb.mount, reg, 'B', renderer);
    ga.cam = gb.cam = new T.Vector3(0, 5.4, -0.6);
    R.gimA = ga; R.gimB = gb;
    // 84E 立柱：泡沫块更长，向后伸得多；板子 USB 那头对着泡沫，另一头向外伸出
    var ge = gimbal(scene, reg, 'E', '84E', renderer, { rear: 5, front: 2.2, blockH: 3, blockW: 3.9, neck: { w: 3.6, z0: -3.6, z1: 0.2 }, mount: [-1.8, 4.3, -2.1] });
    var eb = e84(ge.mount, reg, renderer); R.gimE = ge;
    ge.cam = new T.Vector3(-1.75, 5.5, -0.85);
    ge.camRise = 2.9 + (0.12 + 3 + 0.61) - 0.85;   // 摄像头比云台底座高多少，和上面的尺寸对应
    var post = new T.MeshPhysicalMaterial({ color: 0xcfe8f5, transparent: true, opacity: 0.55, roughness: 0.1 });
    R.posts = [box(scene, 0.5, 0.6, 1, post, 0, 0, 0), box(scene, 0.5, 0.6, 1, post, 0, 0, 0)];
    // 雷达
    var gr = new T.Group(); scene.add(gr);
    servo(gr, reg, 'SR', '雷达水平舵机');
    var rp = new T.Group(); rp.position.set(0.5, 0, 2.9); gr.add(rp);
    var rm = new T.Group();
    rm.quaternion.setFromRotationMatrix(new T.Matrix4().makeBasis(new T.Vector3(-1, 0, 0), new T.Vector3(0, 0, 1), new T.Vector3(0, 1, 0)));
    rm.position.set(0, 0.3, 1.2); rp.add(rm);
    hcsr04(rm, reg, renderer);
    R.gimR = { base: gr, pan: rp };
    return R;
  }

  return { build: build, H7_PINS_VERIFIED: H7_PINS_VERIFIED, ESP_L: ESP_L, ESP_R: ESP_R };
})();
