/*
 * 实时仿真的外观：房间环境、人和物品。只管好不好看，不参与计算。
 * 雷达和相机算的是 world.js 里的房间和柜子。靠墙的设备不单独算回波，所以不能挡住场景路线，
 * 占用的深度见 world.js 的 ENV_CLEAR，test_live_sim.js 会检查路线有没有穿过它们。
 */
window.Sim3DScenery = (function () {
  var T = window.THREE;
  var CLEAR = window.Sim3DWorld.ENV_CLEAR, DOORS = window.Sim3DWorld.ENV_DOORS;
  var WALL_H = 260, WALL_T = 8, DOOR_H = 210;

  function std(color, rough, metal) { return new T.MeshStandardMaterial({ color: color, roughness: rough === undefined ? 0.8 : rough, metalness: metal || 0, envMapIntensity: 0.5 }); }
  function tex(w, h, draw, rx, ry) {
    var cv = document.createElement('canvas'); cv.width = w; cv.height = h;
    draw(cv.getContext('2d'), w, h);
    var t = new T.CanvasTexture(cv); t.encoding = T.sRGBEncoding; t.anisotropy = 8;
    if (rx) { t.wrapS = t.wrapT = T.RepeatWrapping; t.repeat.set(rx, ry || rx); }
    return t;
  }
  function texMat(t, rough) { return new T.MeshStandardMaterial({ map: t, roughness: rough === undefined ? 0.8 : rough, envMapIntensity: 0.4 }); }
  // 可复现的伪随机：每次重建环境长得一样
  function rnd(seed) { var s = seed * 9301 + 49297; return function () { s = (s * 9301 + 49297) % 233280; return s / 233280; }; }
  function box(parent, w, d, h, mat, x, y, z) {
    var m = new T.Mesh(new T.BoxGeometry(w, d, h), mat); m.position.set(x, y, z); parent.add(m); return m;
  }
  function flat(parent, w, h, mat, x, y, z, rotZ) {
    var m = new T.Mesh(new T.PlaneGeometry(w, h), mat);
    m.rotation.order = 'ZXY'; m.rotation.x = Math.PI / 2; m.rotation.z = rotZ || 0;
    m.position.set(x, y, z); parent.add(m); return m;
  }

  /* ---------- 贴图 ---------- */
  var TEX = {};
  function once(key, make) { return TEX[key] || (TEX[key] = make()); }
  function rackTex() {
    return once('rack', function () {
      return tex(256, 800, function (c, w, h) {
        var r = rnd(3);
        c.fillStyle = '#16181c'; c.fillRect(0, 0, w, h);
        c.fillStyle = '#2a2e35'; c.fillRect(0, 0, 14, h); c.fillRect(w - 14, 0, 14, h);
        for (var y = 18; y < h - 30; y += 62) {
          var dark = r() > 0.8;
          c.fillStyle = dark ? '#0c0d10' : '#30353d'; c.fillRect(22, y, w - 44, 54);
          if (dark) continue;
          c.fillStyle = '#20242a';
          for (var i = 0; i < 9; i++) c.fillRect(32 + i * 16, y + 8, 10, 38);
          ['#37d67a', '#37d67a', '#f2b33d', '#37d67a'].forEach(function (col, k) {
            c.fillStyle = r() > 0.25 ? col : '#3a3f46';
            c.fillRect(w - 62 + k * 10, y + 22, 6, 6);
          });
        }
      });
    });
  }
  function cartonTex() {
    return once('carton', function () {
      return tex(128, 128, function (c, w, h) {
        c.fillStyle = '#b98f5e'; c.fillRect(0, 0, w, h);
        c.fillStyle = '#c9a678'; c.fillRect(0, h * 0.44, w, h * 0.12);
        c.strokeStyle = '#8f6c44'; c.lineWidth = 3; c.strokeRect(2, 2, w - 4, h - 4);
        c.fillStyle = '#efe6d6'; c.fillRect(w * 0.62, h * 0.66, w * 0.28, h * 0.2);
      });
    });
  }
  function floorTex(kind) {
    return once('floor' + kind, function () {
      if (kind === 'lab') return tex(256, 256, function (c, w, h) {
        c.fillStyle = '#8f979f'; c.fillRect(0, 0, w, h);
        var r = rnd(5);
        for (var i = 0; i < 500; i++) { c.fillStyle = 'rgba(70,80,90,' + (r() * 0.12) + ')'; c.fillRect(r() * w, r() * h, 2, 2); }
        c.strokeStyle = '#6c737b'; c.lineWidth = 4; c.strokeRect(0, 0, w, h);
      }, 11, 9);
      if (kind === 'store') return tex(512, 512, function (c, w, h) {
        c.fillStyle = '#9d9d97'; c.fillRect(0, 0, w, h);
        var r = rnd(7);
        for (var i = 0; i < 1600; i++) { c.fillStyle = r() > 0.5 ? 'rgba(255,255,255,0.03)' : 'rgba(0,0,0,0.04)'; c.fillRect(r() * w, r() * h, 3 + r() * 5, 3 + r() * 5); }
        c.strokeStyle = 'rgba(60,60,58,0.5)'; c.lineWidth = 2; c.strokeRect(0, 0, w, h);
      }, 2, 2);
      return tex(512, 256, function (c, w, h) {
        var r = rnd(9);
        for (var y = 0; y < 8; y++) for (var x = 0; x < 4; x++) {
          var v = 150 + r() * 34;
          c.fillStyle = 'rgb(' + (v + 26 | 0) + ',' + (v - 8 | 0) + ',' + (v - 52 | 0) + ')';
          var off = (y % 2) * 64;
          c.fillRect(((x * 128 + off) % w), y * 32, 127, 31);
          if (x === 3 && off) c.fillRect(0, y * 32, off - 1, 31);
        }
      }, 6, 8);
    });
  }
  function boardTex() {
    return once('board', function () {
      return tex(512, 256, function (c, w, h) {
        c.fillStyle = '#f3f5f4'; c.fillRect(0, 0, w, h);
        c.fillStyle = '#20457a'; c.font = 'bold 26px sans-serif'; c.fillText('多目标跟踪 教学平台', 24, 44);
        c.strokeStyle = '#20457a'; c.lineWidth = 3;
        [[40, 90, '雷达'], [190, 90, '84E'], [340, 90, 'ESP32']].forEach(function (q) {
          c.strokeRect(q[0], q[1], 110, 50); c.fillStyle = '#20457a'; c.font = '20px sans-serif'; c.fillText(q[2], q[0] + 22, q[1] + 33);
        });
        c.beginPath(); c.moveTo(150, 115); c.lineTo(190, 115); c.moveTo(300, 115); c.lineTo(340, 115); c.stroke();
        c.strokeStyle = '#b33a3a'; c.beginPath(); c.moveTo(60, 200); c.bezierCurveTo(180, 150, 300, 240, 440, 180); c.stroke();
      });
    });
  }
  function signTex(text, bg, fg) {
    return once('sign' + text, function () {
      return tex(256, 96, function (c, w, h) {
        c.fillStyle = bg; c.fillRect(0, 0, w, h);
        c.fillStyle = fg; c.font = 'bold 40px sans-serif'; c.textAlign = 'center'; c.textBaseline = 'middle'; c.fillText(text, w / 2, h / 2);
      });
    });
  }
  function windowTex() {
    return once('window', function () {
      return tex(256, 256, function (c, w, h) {
        var g = c.createLinearGradient(0, 0, 0, h); g.addColorStop(0, '#9cc4e4'); g.addColorStop(1, '#dfeaf2');
        c.fillStyle = g; c.fillRect(0, 0, w, h);
        c.fillStyle = '#8d9aa5'; c.fillRect(0, h * 0.78, w, h * 0.22);
        c.strokeStyle = '#f2f2ee'; c.lineWidth = 12; c.strokeRect(0, 0, w, h);
        c.beginPath(); c.moveTo(w / 2, 0); c.lineTo(w / 2, h); c.stroke();
      });
    });
  }
  function cabinetTex(theme) {
    return once('cab' + theme, function () {
      return tex(256, 512, function (c, w, h) {
        if (theme === 'lab') {
          c.drawImage(rackTex().image, 0, 0, w, h);
          return;
        }
        c.fillStyle = theme === 'store' ? '#5d7f94' : '#c9c3b4'; c.fillRect(0, 0, w, h);
        c.strokeStyle = 'rgba(0,0,0,0.4)'; c.lineWidth = 3;
        c.strokeRect(10, 10, w / 2 - 14, h - 20); c.strokeRect(w / 2 + 4, 10, w / 2 - 14, h - 20);
        c.fillStyle = '#2d3136'; c.fillRect(w / 2 - 16, h * 0.46, 6, 44); c.fillRect(w / 2 + 10, h * 0.46, 6, 44);
      });
    });
  }

  /* ---------- 靠墙设备 ---------- */
  function rackRow(parent, x0, x1, yBack, seedless) {
    var frame = std(0x111317, 0.5, 0.4), face = texMat(rackTex(), 0.45), W = 60, D = CLEAR.n - 4, H = 205;
    var n = Math.floor((x1 - x0) / W);
    var start = (x0 + x1) / 2 - n * W / 2;
    var mats = [frame, frame, frame, face, frame, frame];   // 机柜正面朝 -y，对着屋里
    for (var i = 0; i < n; i++) {
      var m = new T.Mesh(new T.BoxGeometry(W - 2, D, H), mats);
      m.position.set(start + (i + 0.5) * W, yBack - D / 2, H / 2); parent.add(m);
    }
    box(parent, n * W, 22, 5, std(0x6d7782, 0.5, 0.6), (x0 + x1) / 2, yBack - 14, 232);   // 线槽
  }
  function palletRack(parent, x0, x1, yBack, depth) {
    var up = std(0x1f5ea8, 0.5, 0.2), beam = std(0xe07b1f, 0.5, 0.2), carton = texMat(cartonTex(), 0.9);
    var D = depth, bay = 130, n = Math.max(1, Math.floor((x1 - x0) / bay)), start = (x0 + x1) / 2 - n * bay / 2, r = rnd(11);
    var boxGeo = new T.BoxGeometry(1, 1, 1);
    for (var i = 0; i <= n; i++) {
      var x = start + i * bay;
      box(parent, 6, 5, 220, up, x, yBack - 3, 110); box(parent, 6, 5, 220, up, x, yBack - D + 3, 110);
    }
    [14, 92, 170].forEach(function (z) {
      box(parent, n * bay, 5, 7, beam, (x0 + x1) / 2, yBack - 3, z); box(parent, n * bay, 5, 7, beam, (x0 + x1) / 2, yBack - D + 3, z);
      for (var b = 0; b < n; b++) {
        var left = start + b * bay + 10, right = left + bay - 20, cx = left;
        while (cx < right - 24) {
          var w = 28 + r() * 22, h = 26 + r() * 30, d = D - 10;
          if (cx + w > right) break;
          if (r() > 0.14) {
            var m = new T.Mesh(boxGeo, carton); m.scale.set(w, d, h); m.position.set(cx + w / 2, yBack - D / 2, z + 3.5 + h / 2); parent.add(m);
          }
          cx += w + 3;
        }
      }
    });
  }
  function workBench(parent, xFace, y0, y1, withScreens) {
    // 沿 x = xFace 的墙摆的长桌，深度不超过 ENV_CLEAR.e
    var top = std(0xd6d2c8, 0.6), leg = std(0x4a4f57, 0.5, 0.5), D = CLEAR.e - 4, dir = xFace > 0 ? -1 : 1;
    var cx = xFace + dir * D / 2;
    box(parent, D, y1 - y0, 4, top, cx, (y0 + y1) / 2, 76);
    [y0 + 6, y1 - 6].forEach(function (y) { box(parent, D - 4, 4, 74, leg, cx, y, 37); });
    if (!withScreens) return;
    var scr = new T.MeshStandardMaterial({ color: 0x0d1b2a, emissive: 0x1d4f7a, emissiveIntensity: 0.55, roughness: 0.35 });
    for (var y = y0 + 34; y < y1 - 20; y += 70) {
      box(parent, 3, 46, 28, scr, xFace + dir * 8, y, 104);
      box(parent, 8, 4, 12, leg, xFace + dir * 8, y, 84);
      box(parent, 14, 34, 2, std(0x24272c, 0.6), xFace + dir * 22, y, 79);
    }
  }

  /* ---------- 三种应用环境 ---------- */
  var THEMES = {
    lab: { label: '实验室机房', wall: 0xc9d0d6, trim: 0x59626c, floor: 'lab', door: 0x4d5a68 },
    store: { label: '小型库房', wall: 0xc6c2b6, trim: 0xd8a425, floor: 'store', door: 0x6a747c },
    class: { label: '教学实验室', wall: 0xe3e0d6, trim: 0x8d7a5e, floor: 'class', door: 0x8a5f3a }
  };
  function decorate(env, theme, r) {
    var N = env.walls.n, S = env.walls.s, E = env.walls.e, W = env.walls.w;
    var eMax = Math.min(CLEAR.eMaxY, r.y1 - 40), hasE = eMax - r.y0 > 80;
    if (theme === 'lab') {
      rackRow(N, r.x0 + 14, r.x1 - 14, r.y1);
      if (hasE) workBench(E, r.x1, r.y0 + 10, eMax, true);
      box(W, CLEAR.w - 4, 60, 90, std(0x8d9299, 0.5, 0.5), r.x0 + (CLEAR.w - 4) / 2, Math.max(r.y0 + 60, r.y0 + 50), 140);       // 配电箱
      flat(S, 90, 60, texMat(signTex('机房重地 闲人免进', '#23476f', '#ffffff')), 0, r.y0 + 1, 170);
    } else if (theme === 'store') {
      palletRack(N, r.x0 + 10, r.x1 - 10, r.y1, CLEAR.n - 5);
      if (hasE) palletRack(env.makeTurned(E, r, eMax - 4), 0, eMax - r.y0 - 10, 0, CLEAR.e - 2);
      flat(S, 90, 34, texMat(signTex('货物请勿堆放', '#d8a425', '#1c1c1c')), 0, r.y0 + 1, 170);
      // 地面黄线：通道边界
      var tape = new T.MeshBasicMaterial({ color: 0xe6b422 });
      var yl = r.y1 - CLEAR.n - 8;
      box(env.floorGroup, r.x1 - r.x0 - 30, 6, 0.6, tape, (r.x0 + r.x1) / 2, yl, 0.4);
    } else {
      if (r.x1 - r.x0 > 300) flat(N, 240, 120, texMat(boardTex(), 0.5), (r.x0 + r.x1) / 2, r.y1 - 1, 150);
      box(N, r.x1 - r.x0 - 40, CLEAR.n - 8, 78, std(0xb7ac98, 0.7), (r.x0 + r.x1) / 2, r.y1 - (CLEAR.n - 8) / 2, 39);          // 矮柜
      if (hasE) workBench(E, r.x1, r.y0 + 10, eMax, true);
      var win = texMat(windowTex(), 0.25), wy = r.y0 + 60;
      for (var i = 0; i < 2 && wy < r.y1 - 300; i++, wy += 130) flat(W, 110, 120, win, r.x0 + 1, wy, 150, -Math.PI / 2);
    }
    // 门上方的出口标志
    DOORS.forEach(function (d) {
      if (d.y0 < r.y0 || d.y1 > r.y1) return;
      var g = d.side === 'w' ? W : E, x = d.side === 'w' ? r.x0 + 1 : r.x1 - 1;
      flat(g, 40, 15, new T.MeshBasicMaterial({ map: signTex('安全出口', '#128a4a', '#ffffff') }), x, (d.y0 + d.y1) / 2, DOOR_H + 18, d.side === 'w' ? -Math.PI / 2 : Math.PI / 2);
    });
  }

  function createEnv(scene) {
    var env = { group: new T.Group(), walls: {}, theme: 'lab', room: null };
    env.group.visible = false; scene.add(env.group);
    function disposeAll(o) {
      while (o.children.length) { var c = o.children.pop(); disposeAll(c); if (c.geometry) c.geometry.dispose(); }
    }
    env.makeTurned = function (parent, r, yStart) {
      // 把沿 x 摆的货架转成沿东墙摆：本地 +x 朝南，本地 -y 朝屋里
      var g = new T.Group(); g.rotation.z = -Math.PI / 2; g.position.set(r.x1, yStart, 0); parent.add(g); return g;
    };
    env.build = function (theme, r) {
      env.theme = theme; env.room = r;
      disposeAll(env.group); env.walls = {};
      var t = THEMES[theme], w = r.x1 - r.x0, d = r.y1 - r.y0, cx = (r.x0 + r.x1) / 2, cy = (r.y0 + r.y1) / 2;
      var wallMat = std(t.wall, 0.9), trim = std(t.trim, 0.6);
      env.floorGroup = new T.Group(); env.group.add(env.floorGroup);
      var ft = floorTex(t.floor);
      var fl = new T.Mesh(new T.PlaneGeometry(w + WALL_T * 2, d + WALL_T * 2), new T.MeshStandardMaterial({ map: ft, roughness: 0.72, envMapIntensity: 0.2 }));
      fl.position.set(cx, cy, -0.48); fl.receiveShadow = true; env.floorGroup.add(fl);
      // 墙按方向分组：朝相机那面整面隐藏，看得到屋里
      ['n', 's', 'e', 'w'].forEach(function (k) { env.walls[k] = new T.Group(); env.group.add(env.walls[k]); });
      box(env.walls.n, w + WALL_T * 2, WALL_T, WALL_H, wallMat, cx, r.y1 + WALL_T / 2, WALL_H / 2);
      box(env.walls.s, w + WALL_T * 2, WALL_T, WALL_H, wallMat, cx, r.y0 - WALL_T / 2, WALL_H / 2);
      [['w', r.x0 - WALL_T / 2], ['e', r.x1 + WALL_T / 2]].forEach(function (s) {
        var g = env.walls[s[0]], y = r.y0;
        var door = DOORS.filter(function (q) { return q.side === s[0] && q.y0 >= r.y0 && q.y1 <= r.y1; })[0];
        if (door) {
          box(g, WALL_T, door.y0 - r.y0, WALL_H, wallMat, s[1], (r.y0 + door.y0) / 2, WALL_H / 2);
          box(g, WALL_T, r.y1 - door.y1, WALL_H, wallMat, s[1], (door.y1 + r.y1) / 2, WALL_H / 2);
          box(g, WALL_T, door.y1 - door.y0, WALL_H - DOOR_H, wallMat, s[1], (door.y0 + door.y1) / 2, (WALL_H + DOOR_H) / 2);
          var frame = std(t.door, 0.5);
          box(g, WALL_T + 2, 5, DOOR_H, frame, s[1], door.y0, DOOR_H / 2); box(g, WALL_T + 2, 5, DOOR_H, frame, s[1], door.y1, DOOR_H / 2);
          box(g, WALL_T + 2, door.y1 - door.y0 + 5, 5, frame, s[1], (door.y0 + door.y1) / 2, DOOR_H);
          // 门扇开向屋外
          var dir = s[0] === 'w' ? -1 : 1, lw = door.y1 - door.y0;
          box(g, lw * 0.5, 4, DOOR_H - 6, frame, s[1] + dir * (WALL_T / 2 + lw * 0.25), door.y0 - 2, DOOR_H / 2);
          box(g, lw * 0.5, 4, DOOR_H - 6, frame, s[1] + dir * (WALL_T / 2 + lw * 0.25), door.y1 + 2, DOOR_H / 2);
        } else box(g, WALL_T, d, WALL_H, wallMat, s[1], cy, WALL_H / 2);
      });
      // 墙隐藏后地上留一圈踢脚线，看得出房间边界
      env.curb = new T.Group(); env.group.add(env.curb);
      box(env.curb, w + WALL_T * 2, WALL_T, 5, trim, cx, r.y1 + WALL_T / 2, 2.5); box(env.curb, w + WALL_T * 2, WALL_T, 5, trim, cx, r.y0 - WALL_T / 2, 2.5);
      box(env.curb, WALL_T, d, 5, trim, r.x0 - WALL_T / 2, cy, 2.5); box(env.curb, WALL_T, d, 5, trim, r.x1 + WALL_T / 2, cy, 2.5);
      decorate(env, theme, r);
      // 天花板和灯：只有相机画面里看得到
      env.ceiling = new T.Group(); env.group.add(env.ceiling); env.ceiling.visible = false;
      var ce = new T.Mesh(new T.PlaneGeometry(w + WALL_T * 2, d + WALL_T * 2), std(0xe9e9e4, 0.95)); ce.rotation.x = Math.PI; ce.position.set(cx, cy, WALL_H); env.ceiling.add(ce);
      var lamp = new T.MeshBasicMaterial({ color: 0xfafaf2 });
      for (var lx = r.x0 + 120; lx < r.x1 - 60; lx += 200) for (var ly = r.y0 + 100; ly < r.y1 - 60; ly += 170) {
        var l = new T.Mesh(new T.PlaneGeometry(90, 26), lamp); l.rotation.x = Math.PI; l.position.set(lx, ly, WALL_H - 0.5); env.ceiling.add(l);
      }
    };
    // 主视图：相机在哪一面墙外，就隐藏那面墙
    env.cutaway = function (cam) {
      var r = env.room; if (!r) return;
      env.walls.w.visible = cam.x > r.x0; env.walls.e.visible = cam.x < r.x1;
      env.walls.s.visible = cam.y > r.y0; env.walls.n.visible = cam.y < r.y1;
      env.ceiling.visible = false;
    };
    // 相机画面：屋子是完整的
    env.showAll = function () {
      ['n', 's', 'e', 'w'].forEach(function (k) { env.walls[k].visible = true; });
      env.ceiling.visible = true;
    };
    // 柜子：一个方盒子贴上对应环境的柜面，方便拖动和判断遮挡
    env.cabinetMats = function () {
      var side = std(env.theme === 'store' ? 0x4d6a7c : (env.theme === 'lab' ? 0x15171b : 0xb9b3a4), 0.6, 0.15);
      var f = texMat(cabinetTex(env.theme), 0.5);
      return [f, f, f, f, side, side];
    };
    return env;
  }

  /* ---------- 人 ---------- */
  var OUTFITS = [
    { top: 0x2f3e57, legs: 0x22252b, hair: 0x1b1816, skin: 0xd8ad8b, shoe: 0xe8e8e4 },
    { top: 0x7c3a2e, legs: 0x38465a, hair: 0x2c2019, skin: 0xcf9f7d, shoe: 0x2a2a2c },
    { top: 0x41684f, legs: 0x2b2d33, hair: 0x15130f, skin: 0xe2b896, shoe: 0x6c5a48 },
    { top: 0x5a4a7a, legs: 0x3a3b40, hair: 0x332319, skin: 0xd6a988, shoe: 0xdedad2 }
  ];
  var G = {};
  function limbGeo(len, r0, r1) {
    var key = len + '_' + r0 + '_' + r1;
    if (G[key]) return G[key];
    // 从原点向下（-z）伸出，转轴在原点
    var g = new T.CylinderGeometry(r0, r1, len, 10); g.rotateX(Math.PI / 2); g.translate(0, 0, -len / 2);
    return (G[key] = g);
  }
  // 身高 170 cm，脚底在 z=0，面朝 +y。mono 时全身一种材质，用来画半透明的固件估计位置
  function person(outfitIndex, mono) {
    var o = OUTFITS[outfitIndex % OUTFITS.length], g = new T.Group();
    var one = mono ? std(0xd8dee4, 0.8) : null;
    var top = one || std(o.top, 0.85), legs = one || std(o.legs, 0.8), skin = one || std(o.skin, 0.65), hair = one || std(o.hair, 0.9), shoe = one || std(o.shoe, 0.6);
    function add(parent, geo, mat, x, y, z) { var m = new T.Mesh(geo, mat); m.position.set(x || 0, y || 0, z || 0); m.castShadow = true; parent.add(m); return m; }
    var body = new T.Group(); g.add(body);
    // 躯干：上宽下窄，前后扁
    var torso = new T.CylinderGeometry(17.5, 14.5, 52, 14); torso.rotateX(Math.PI / 2); torso.scale(1, 0.62, 1);
    add(body, torso, top, 0, 0, 116);
    var hips = new T.SphereGeometry(15, 14, 10); hips.scale(1, 0.68, 0.55);
    add(body, hips, legs, 0, 0, 91);
    var shoulders = new T.SphereGeometry(18, 14, 10); shoulders.scale(1.02, 0.6, 0.34);
    add(body, shoulders, top, 0, 0, 141);
    add(body, new T.CylinderGeometry(4.6, 5.2, 8, 10).rotateX(Math.PI / 2), skin, 0, 0, 148);
    var head = new T.SphereGeometry(10.5, 18, 14); head.scale(0.92, 1, 1.14);
    add(body, head, skin, 0, 0.8, 160);
    var cap = new T.SphereGeometry(11, 18, 10, 0, Math.PI * 2, 0, Math.PI * 0.56); cap.rotateX(Math.PI / 2); cap.scale(0.96, 1.02, 1.12);
    add(body, cap, hair, 0, -0.8, 161);
    var legL = [], armL = [];
    [-1, 1].forEach(function (s) {
      var hip = new T.Group(); hip.position.set(s * 8.2, 0, 90); g.add(hip);
      add(hip, limbGeo(42, 7.4, 5.8), legs);
      var knee = new T.Group(); knee.position.z = -42; hip.add(knee);
      add(knee, limbGeo(42, 5.6, 4.2), legs);
      var foot = new T.SphereGeometry(1, 12, 8); foot.scale(4.6, 12.5, 3.6);
      add(knee, foot, shoe, 0, 5, -42 - 2.8);
      legL.push({ hip: hip, knee: knee });
      var sh = new T.Group(); sh.position.set(s * 20.5, 0, 140); sh.rotation.y = s * 0.09; g.add(sh);
      add(sh, limbGeo(29, 4.9, 4), top);
      var el = new T.Group(); el.position.z = -29; sh.add(el);
      add(el, limbGeo(26, 3.9, 3.1), top);
      add(el, new T.SphereGeometry(3.9, 10, 8), skin, 0, 0, -28);
      armL.push({ sh: sh, el: el });
    });
    g.rig = { legs: legL, arms: armL, phase: 0, amp: 0, lastX: null, lastY: null, heading: 0 };
    g.mat = one || top;
    return g;
  }
  // 按移动距离摆腿，脚步和地面对得上；站住时收回成站姿
  function walk(p, x, y, speedKnown) {
    var r = p.rig;
    if (r.lastX === null) { r.lastX = x; r.lastY = y; }
    var dx = x - r.lastX, dy = y - r.lastY, d = Math.hypot(dx, dy);
    r.lastX = x; r.lastY = y;
    if (d > 60) d = 0;       // 被拖动或换场景，不是走过去的
    if (d > 0.2) {
      var want = Math.atan2(dx, dy), diff = want - r.heading;
      while (diff > Math.PI) diff -= 2 * Math.PI;
      while (diff < -Math.PI) diff += 2 * Math.PI;
      r.heading += diff * 0.18;
    }
    var target = d > 0.02 ? Math.min(1, Math.max(0.4, (speedKnown || 90) / 90)) : 0;
    r.amp += (target - r.amp) * 0.14;
    r.phase += d / 135 * Math.PI * 2;
    p.rotation.z = -r.heading;
    r.legs.forEach(function (q, i) {
      var ph = r.phase + (i ? Math.PI : 0), s = Math.sin(ph);
      q.hip.rotation.x = 0.46 * r.amp * s;
      q.knee.rotation.x = -(0.12 + 0.72 * Math.max(0, -Math.cos(ph))) * r.amp;
    });
    r.arms.forEach(function (q, i) {
      var ph = r.phase + (i ? 0 : Math.PI);
      q.sh.rotation.x = 0.36 * r.amp * Math.sin(ph);
      q.el.rotation.x = 0.28 + 0.2 * r.amp;
    });
  }
  // 地上的光圈和头顶的小箭头：这个人被谁跟着。相机画面里隐藏
  function marker(parent) {
    var g = new T.Group(), mat = new T.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.9, depthWrite: false });
    var ring = new T.Mesh(new T.RingGeometry(27, 33, 40), mat); ring.position.z = 1.2; g.add(ring);
    var cone = new T.Mesh(new T.ConeGeometry(7, 13, 4), mat); cone.rotation.x = -Math.PI / 2; cone.position.z = 190; g.add(cone);
    parent.add(g); g.mat = mat;
    return g;
  }
  // 脚下的柔和阴影：太阳阴影只覆盖装置附近
  var BLOB = null;
  function blob(parent) {
    if (!BLOB) BLOB = tex(64, 64, function (c, w) {
      var gr = c.createRadialGradient(w / 2, w / 2, 2, w / 2, w / 2, w / 2);
      gr.addColorStop(0, 'rgba(0,0,0,0.42)'); gr.addColorStop(1, 'rgba(0,0,0,0)');
      c.fillStyle = gr; c.fillRect(0, 0, w, w);
    });
    var m = new T.Mesh(new T.PlaneGeometry(70, 70), new T.MeshBasicMaterial({ map: BLOB, transparent: true, depthWrite: false }));
    m.position.z = 0.8; parent.add(m); return m;
  }
  // 放进房间的物品：办公椅，高度和 world.js 的 actorH 一致
  function chair() {
    var g = new T.Group(), fab = std(0x2b3036, 0.9), metal = std(0x83888f, 0.35, 0.8);
    function add(geo, mat, x, y, z) { var m = new T.Mesh(geo, mat); m.position.set(x, y, z); m.castShadow = true; g.add(m); return m; }
    var seat = new T.SphereGeometry(24, 18, 10); seat.scale(1, 1, 0.2); add(seat, fab, 0, 0, 46);
    var back = new T.SphereGeometry(21, 18, 12); back.scale(1, 0.18, 1.05); add(back, fab, 0, 21, 72);
    add(new T.BoxGeometry(4, 3, 26), metal, 0, 22, 52);
    add(new T.CylinderGeometry(2.6, 2.6, 34, 10).rotateX(Math.PI / 2), metal, 0, 0, 27);
    for (var i = 0; i < 5; i++) {
      var a = i / 5 * Math.PI * 2, leg = add(new T.BoxGeometry(3.2, 26, 2.2), metal, Math.sin(a) * 13, Math.cos(a) * 13, 9);
      leg.rotation.z = -a;
      add(new T.SphereGeometry(3, 10, 8), fab, Math.sin(a) * 25, Math.cos(a) * 25, 3);
    }
    return g;
  }

  return { createEnv: createEnv, THEMES: THEMES, person: person, walk: walk, marker: marker, blob: blob, chair: chair };
})();
