#!/usr/bin/env python3
"""
人体/背景二分类模型 v2 训练（没有新实拍数据，只改训练方法和数据增强）
=====================================================================
与 train_npu_model.py（v1，保留不动）相比：
  - 评判标准只有一个：按场景留出的 4 折交叉验证（与 eval_human_model.py --cv 相同的折）
  - 测试集统一使用板端预处理（320x240 → RGB565 截位 → 整数 luma → 2x2 平均，
    逐位复现 proj_cm55/source/inference_task.c），训练增强后也走同一条链
  - 每折内部再按场景留出 3 个场景做验证（早停、选阈值），不再按图片随机划分
  - 可选：BatchNorm、类别均衡采样、label smoothing、val_loss 早停、摄像头仿真增强、
    尺度增强（缩小+补边模拟远处小人）、背景图大幅裁剪（只对 scene 图做，安全）

用法:
  python train_v2.py --cv v1 stab cam cam_scale full      # 消融，每个配置跑 4 折
  python train_v2.py --cv full --quant-check               # 额外检查每折 INT8 模型
  python train_v2.py --final full                          # 用全部 16 个场景训练并导出到 output/
"""

import os, sys, glob, json, argparse, time, shutil
import numpy as np
from PIL import Image
import cv2

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(SCRIPT_DIR, 'output')
NPU_W, NPU_H = 160, 120
CAM_W, CAM_H = 320, 240
CLASS_NAMES = ['background', 'human']
DEFAULT_DS = [
    os.path.expandvars(r'%USERPROFILE%\Desktop\infer_image_train\英飞凌训练集'),
    os.path.expandvars(r'%USERPROFILE%\Desktop\infer\infer_image_train\英飞凌训练集'),
]
TARGET_FPR = 0.05

# ---------------- 配置（消融用，逐项叠加） ----------------
BASE = dict(arch='v1', protocol='v1', balance=False, ls=0.0, photo=False, scale=False, bg_crop=False,
            geo=False, epochs=100, width=1.0)
CONFIGS = {
    # v1：与 train_npu_model.py 相同的网络和训练方式（随机 85/15 验证、val_accuracy 早停）
    'v1':        dict(BASE),
    # 训练稳定性：BN + 均衡采样 + label smoothing + val_loss 早停 + 按场景验证 + 翻转/小幅几何
    'stab':      dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, epochs=150),
    # + 摄像头仿真（亮度/对比度/gamma、暗光噪声、模糊、运动模糊、暗角、降采样、JPEG）
    'cam':       dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, photo=True, epochs=150),
    # + 尺度增强（人物图缩小补边模拟远处，轻微放大）
    'cam_scale': dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, photo=True, scale=True, epochs=150),
    # + 背景图大幅随机裁剪/放大（只对 scene 图，生成更多样的背景）
    'full':      dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, photo=True, scale=True,
                      bg_crop=True, epochs=150),
    # full 但网络加宽 1.5 倍（检查容量是否是瓶颈）
    'full_w15':  dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, photo=True, scale=True,
                      bg_crop=True, epochs=150, width=1.5),
    # 稳定性 + 尺度增强（不加摄像头仿真）
    'stab_scale': dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, scale=True, epochs=150),
    # 稳定性 + 尺度增强 + 背景大幅裁剪
    'stab_bg':   dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, scale=True, bg_crop=True,
                      epochs=150),
    # 稳定性 + 轻度摄像头仿真（只有 40% 的样本做光照/噪声/模糊）
    'cam_lite':  dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, photo=True, photo_p=0.4,
                      epochs=150),
    # stab_bg + 轻度摄像头仿真（候选最终配置）
    'bg_lite':   dict(BASE, arch='v2', protocol='v2', balance=True, ls=0.05, geo=True, scale=True, bg_crop=True,
                      photo=True, photo_p=0.4, epochs=150),
}
# ---------------- 数据 ----------------
def load_dataset(ds_dir):
    files, labels, scenes = [], [], []
    for s in sorted(glob.glob(os.path.join(ds_dir, '场景*'))):
        for sub, label in [('person', 1), ('scene', 0)]:
            d = os.path.join(s, sub)
            if os.path.isdir(d):
                for f in sorted(glob.glob(os.path.join(d, '*.*'))):
                    if f.lower().endswith(('.jpg', '.jpeg', '.png')):
                        files.append(f); labels.append(label); scenes.append(os.path.basename(s))
    return np.array(files), np.array(labels), np.array(scenes)


def load_cache(files, cache_path):
    """g320: 320x240 灰度（模拟 OV7675 帧）；lz160: v1 训练预处理（LANCZOS 160x120）。"""
    if os.path.exists(cache_path):
        z = np.load(cache_path)
        if len(z['g320']) == len(files):
            return z['g320'], z['lz160']
    g320 = np.zeros((len(files), CAM_H, CAM_W), np.uint8)
    lz160 = np.zeros((len(files), NPU_H, NPU_W), np.uint8)
    for i, f in enumerate(files):
        im = Image.open(f)
        g320[i] = np.asarray(im.convert('L').resize((CAM_W, CAM_H), Image.BILINEAR))
        lz160[i] = np.asarray(im.convert('L').resize((NPU_W, NPU_H), Image.LANCZOS))
    np.savez(cache_path, g320=g320, lz160=lz160)
    return g320, lz160


def board_chain(g320):
    """320x240 uint8 灰度 → 160x120 uint8，逐位复现 preprocess_bgr565_to_gray_i8()（R=G=B 时）。
    板端 int8 输入 = 返回值 - 128。"""
    g = g320.astype(np.uint32)
    luma = (306 * (g & 0xF8) + 601 * (g & 0xFC) + 117 * (g & 0xF8)) >> 10
    s = luma[0::2, 0::2] + luma[0::2, 1::2] + luma[1::2, 0::2] + luma[1::2, 1::2]
    return ((s + 2) >> 2).astype(np.uint8)


# ---------------- 增强 ----------------
_yy, _xx = np.mgrid[0:CAM_H, 0:CAM_W]
_R2 = (((_xx - CAM_W / 2) / (CAM_W / 2)) ** 2 + ((_yy - CAM_H / 2) / (CAM_H / 2)) ** 2).astype(np.float32)

def augment(img, label, rng, cfg):
    """img: 240x320 uint8 → 增强后 240x320 uint8（再交给 board_chain）。"""
    h, w = img.shape
    x = img.astype(np.float32)
    if cfg['geo'] or cfg['scale'] or cfg['bg_crop']:
        if rng.random() < 0.5:
            x = x[:, ::-1]
        ang = rng.uniform(-5, 5) if cfg['geo'] else 0.0
        s = rng.uniform(0.95, 1.05)
        if label == 1 and cfg['scale'] and rng.random() < 0.6:
            s = rng.uniform(0.55, 1.1)           # 缩小：人变小（模拟 4~10 m）；放大最多 1.1，避免把人裁掉
        if label == 0 and cfg['bg_crop'] and rng.random() < 0.6:
            s = rng.uniform(1.0, 2.2)            # 背景图大幅放大裁剪，安全地产生新背景
        elif label == 0 and cfg['scale'] and rng.random() < 0.4:
            s = rng.uniform(0.55, 1.0)           # 背景也做同样的缩小补边，避免"补边=有人"的捷径
        tx, ty = rng.uniform(-0.06, 0.06) * w, rng.uniform(-0.06, 0.06) * h
        if label == 0 and s > 1.05:              # 放大裁剪时允许平移到画面任意位置
            m = (s - 1) / (2 * s)
            tx, ty = rng.uniform(-m, m) * w * s, rng.uniform(-m, m) * h * s
        M = cv2.getRotationMatrix2D((w / 2, h / 2), ang, s)
        M[0, 2] += tx; M[1, 2] += ty
        x = cv2.warpAffine(np.ascontiguousarray(x), M, (w, h), flags=cv2.INTER_LINEAR,
                           borderMode=cv2.BORDER_REFLECT_101)
    if cfg['photo'] and rng.random() < cfg.get('photo_p', 0.85):
        x = x / 255.0
        x = np.clip(x, 0, 1) ** np.exp(rng.uniform(np.log(0.7), np.log(2.0)))       # gamma，偏向变暗
        mu = x.mean()
        x = (x - mu) * rng.uniform(0.6, 1.3) + mu + rng.uniform(-0.12, 0.10)        # 对比度、亮度
        if rng.random() < 0.5:                                                      # 暗角
            x = x * (1 - rng.uniform(0.1, 0.5) * _R2 / 2)
        x = np.clip(x, 0, 1).astype(np.float32)
        if rng.random() < 0.4:                                                      # 降采样再放大（镜头/传感器偏软）
            f = rng.choice([2, 3])
            x = cv2.resize(cv2.resize(x, (w // f, h // f), interpolation=cv2.INTER_AREA), (w, h),
                           interpolation=cv2.INTER_LINEAR)
        sig = rng.uniform(0, 1.0)
        if sig > 0.3:
            x = cv2.GaussianBlur(x, (0, 0), sig)
        if rng.random() < 0.2:                                                      # 运动模糊
            L = int(rng.integers(3, 10)); k = np.zeros((L, L), np.float32); k[L // 2] = 1.0 / L
            k = cv2.warpAffine(k, cv2.getRotationMatrix2D((L / 2 - .5, L / 2 - .5), rng.uniform(0, 180), 1), (L, L))
            x = cv2.filter2D(x, -1, k / max(k.sum(), 1e-6))
        # 传感器噪声：光子数随亮度下降（暗光增益高 → 噪声大）+ 读出噪声
        photons = rng.uniform(150, 1500) * np.clip(x.mean() / 0.35, 0.3, 1.0)
        x = rng.poisson(np.clip(x, 0, 1) * photons) / photons + rng.normal(0, rng.uniform(0, 0.012), x.shape)
        x = np.clip(x * 255, 0, 255)
        if rng.random() < 0.3:                                                      # JPEG 压缩
            ok, enc = cv2.imencode('.jpg', x.astype(np.uint8), [cv2.IMWRITE_JPEG_QUALITY, int(rng.integers(30, 90))])
            x = cv2.imdecode(enc, cv2.IMREAD_GRAYSCALE).astype(np.float32)
    return np.clip(np.round(x), 0, 255).astype(np.uint8)


# ---------------- 模型 ----------------
def build_v1():
    sys.path.insert(0, SCRIPT_DIR)
    import train_npu_model as T
    return T.build_model()


def build_v2(width=1.0, ls=0.05):
    import keras
    from keras import layers
    c = lambda n: int(round(n * width / 8.0)) * 8

    def cbr(x, n):
        x = layers.Conv2D(n, 3, padding='same', use_bias=False)(x)
        return layers.ReLU()(layers.BatchNormalization()(x))

    def dbr(x):
        x = layers.DepthwiseConv2D(3, padding='same', use_bias=False)(x)
        return layers.ReLU()(layers.BatchNormalization()(x))

    inp = keras.Input(shape=(NPU_H, NPU_W, 1), name='image_input')
    x = layers.Rescaling(1. / 127.5, offset=-1.0)(inp)
    x = layers.MaxPooling2D(2)(dbr(cbr(x, c(16))))      # 80x60
    x = layers.MaxPooling2D(2)(dbr(cbr(x, c(32))))      # 40x30
    x = layers.MaxPooling2D(2)(dbr(cbr(x, c(48))))      # 20x15
    x = layers.MaxPooling2D(2)(cbr(x, c(64)))           # 10x7
    x = cbr(x, c(96))
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.3)(x)
    x = layers.Dense(32, activation='relu')(x)
    out = layers.Dense(2, activation='softmax', name='output')(x)
    m = keras.Model(inp, out, name='human_detector_v2')
    m.compile(optimizer=keras.optimizers.Adam(1e-3),
              loss=keras.losses.CategoricalCrossentropy(label_smoothing=ls), metrics=['accuracy'])
    return m


# ---------------- 数据流 ----------------
def make_seq(g320, y, idx, cfg, batch, steps, seed):
    import keras

    class Seq(keras.utils.PyDataset):
        def __init__(self):
            super().__init__(workers=8, use_multiprocessing=False)
            self.epoch = 0
            self.pos, self.neg = idx[y[idx] == 1], idx[y[idx] == 0]
            self._resample()

        def _resample(self):
            r = np.random.default_rng(seed * 1000 + self.epoch)
            n = steps * batch
            if cfg['balance']:
                self.order = np.concatenate([r.choice(self.pos, n // 2), r.choice(self.neg, n - n // 2)])
            else:
                self.order = r.choice(idx, n)
            r.shuffle(self.order)

        def __len__(self):
            return steps

        def __getitem__(self, i):
            r = np.random.default_rng([seed, self.epoch, i])
            b = self.order[i * batch:(i + 1) * batch]
            X = np.stack([board_chain(augment(g320[j], y[j], r, cfg)) for j in b])[..., None].astype(np.float32)
            Y = np.eye(2, dtype=np.float32)[y[b]]
            return X, Y

        def on_epoch_end(self):
            self.epoch += 1
            self._resample()

    return Seq()


def train_one(cfg, g320, lz160, y, tr, va, seed, epochs=None, verbose=0):
    """返回 (model, 实际 epoch 数, 最佳 epoch)。va 为空时按固定 epoch 训练（--final）。"""
    import keras
    keras.utils.set_random_seed(seed)
    if cfg['protocol'] == 'v1':
        m = build_v1()
        X = lz160[..., None].astype(np.float32)
        cb = [keras.callbacks.EarlyStopping(monitor='val_accuracy', patience=15, restore_best_weights=True,
                                            start_from_epoch=cfg.get('_start', 0)),
              keras.callbacks.ReduceLROnPlateau(monitor='val_loss', factor=0.5, patience=5, min_lr=1e-6)]
        h = m.fit(X[tr], y[tr], validation_data=(X[va], y[va]), batch_size=16, epochs=cfg['epochs'],
                  callbacks=cb, verbose=verbose)
        return m, len(h.history['loss']), int(np.argmax(h.history['val_accuracy'])) + 1
    m = build_v2(cfg['width'], cfg['ls'])
    seq = make_seq(g320, y, tr, cfg, 16, max(20, len(tr) // 16), seed)
    if len(va):
        Xv = np.stack([board_chain(g320[j]) for j in va])[..., None].astype(np.float32)
        Yv = np.eye(2, dtype=np.float32)[y[va]]
        cb = [keras.callbacks.EarlyStopping(monitor='val_loss', patience=25, restore_best_weights=True,
                                            start_from_epoch=10),
              keras.callbacks.ReduceLROnPlateau(monitor='val_loss', factor=0.5, patience=8, min_lr=1e-5)]
        h = m.fit(seq, validation_data=(Xv, Yv), epochs=epochs or cfg['epochs'], callbacks=cb, verbose=verbose)
        return m, len(h.history['loss']), int(np.argmin(h.history['val_loss'])) + 1
    # 固定 epoch：学习率按余弦退火
    sched = keras.callbacks.LearningRateScheduler(lambda e: 1e-3 * 0.5 * (1 + np.cos(np.pi * e / epochs)) + 1e-5)
    h = m.fit(seq, epochs=epochs, callbacks=[sched], verbose=verbose)
    return m, len(h.history['loss']), epochs


# ---------------- 指标 ----------------
def cmetrics(y, p_human, thr=0.5):
    pred = (p_human >= thr).astype(int)
    cm = np.zeros((2, 2), int)
    for t, p in zip(y, pred):
        cm[t, p] += 1
    tn, fp, fn, tp = cm.ravel()
    return dict(n=int(len(y)), acc=float((tn + tp) / len(y)), fpr=float(fp / max(1, tn + fp)),
                fnr=float(fn / max(1, fn + tp)), cm=cm.tolist())


def fmt(name, m):
    c = m['cm']
    return (f"{name:<34} n={m['n']:<4} acc={m['acc']*100:6.2f}%  FPR={m['fpr']*100:6.2f}%  FNR={m['fnr']*100:6.2f}%  "
            f"[[{c[0][0]}, {c[0][1]}], [{c[1][0]}, {c[1][1]}]]")


def pick_threshold(y, p, target=TARGET_FPR):
    """在满足 FPR<=target 的阈值中取漏检最少的；都不满足时取 FPR 最小的。"""
    cands = np.unique(np.concatenate([[0.5], p, np.linspace(0.05, 0.995, 190)]))
    best = None
    for t in cands:
        m = cmetrics(y, p, t)
        key = (m['fpr'] <= target, -m['fnr'] if m['fpr'] <= target else -m['fpr'])
        if best is None or key > best[0]:
            best = (key, float(t))
    return best[1]


def predict(m, X):
    return m.predict(X.astype(np.float32), batch_size=64, verbose=0)[:, 1]


def quant_predict(m, calib, X):
    """与 train_npu_model.export_int8 相同的全整数量化（uint8 I/O），返回 p_human。"""
    import tensorflow as tf
    tfl = to_tflite(m, calib)
    it = tf.lite.Interpreter(model_content=tfl); it.allocate_tensors()
    di, do = it.get_input_details()[0], it.get_output_details()[0]
    check_input_quant(di)
    so, zo = do['quantization']; out = np.zeros(len(X), np.float32)
    for i in range(len(X)):
        it.set_tensor(di['index'], X[i:i + 1].astype(np.uint8)); it.invoke()
        out[i] = (it.get_tensor(do['index'])[0, 1].astype(np.float32) - zo) * so
    return out, len(tfl)


def check_input_quant(di):
    """板端写死 q = gray - 128，所以 uint8 输入必须是 scale 1.0、zero_point 0。"""
    s, z = di['quantization']
    if abs(s - 1.0) > 1e-6 or z != 0:
        raise RuntimeError(f'输入量化参数 scale={s} zp={z}，与 inference_task.c 的 gray-128 不一致')


def to_tflite(m, calib):
    import tensorflow as tf

    def rep():
        # 板端灰度最大只有 250（RGB565 截位），不加全黑、全白帧时输入 scale 会偏离 1.0
        for v in (0.0, 255.0):
            yield [np.full((1, NPU_H, NPU_W, 1), v, np.float32)]
        for i in range(len(calib)):
            yield [calib[i:i + 1].astype(np.float32)]
    cv = tf.lite.TFLiteConverter.from_keras_model(m)
    cv.optimizations = [tf.lite.Optimize.DEFAULT]
    cv.representative_dataset = rep
    cv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    cv.inference_input_type = tf.uint8
    cv.inference_output_type = tf.uint8
    return cv.convert()


# ---------------- 交叉验证 ----------------
def scene_folds(scenes, k=4):
    uniq = sorted(set(scenes), key=lambda s: int(s.replace('场景', '')))
    return [uniq[i::k] for i in range(k)]


def run_cv(name, cfg, g320, lz160, y, scenes, seed, quant_check, log):
    folds = scene_folds(scenes)
    G = np.stack([board_chain(g) for g in g320])[..., None]          # 测试集：板端预处理
    P_test, Y_test, P_q, rows, collapses, best_epochs, thr_list = [], [], [], [], 0, [], []
    P_val, Y_val = [], []
    t0 = time.time()
    for fi, held in enumerate(folds):
        te = np.where(np.isin(scenes, held))[0]
        rest = np.where(~np.isin(scenes, held))[0]
        r = np.random.RandomState(seed * 100 + fi)
        if cfg['protocol'] == 'v1':
            r.shuffle(rest); sp = int(len(rest) * 0.85); tr, va = rest[:sp], rest[sp:]
        else:
            rs = sorted(set(scenes[rest])); vs = list(r.choice(rs, 3, replace=False))
            va = rest[np.isin(scenes[rest], vs)]; tr = rest[~np.isin(scenes[rest], vs)]
        for attempt in range(4):
            c = dict(cfg, _start=0 if attempt == 0 else 30)
            m, ne, be = train_one(c, g320, lz160, y, tr, va, seed * 1000 + fi * 10 + attempt)
            Xtr = (lz160[tr][..., None] if cfg['protocol'] == 'v1' else G[tr])
            collapsed = len(set((predict(m, Xtr) >= 0.5).astype(int))) < 2
            if not collapsed:
                break
            collapses += 1
            log(f"  {name} fold{fi} attempt{attempt}: 塌缩（训练集只输出一个类别），重训")
        best_epochs.append(be)
        pv = predict(m, (lz160[va][..., None] if cfg['protocol'] == 'v1' else G[va]))
        thr = pick_threshold(y[va], pv); thr_list.append(thr)
        pt = predict(m, G[te])
        P_val.append(pv); Y_val.append(y[va]); P_test.append(pt); Y_test.append(y[te])
        m05, mt = cmetrics(y[te], pt), cmetrics(y[te], pt, thr)
        line = fmt(f"{name} fold{fi} ({','.join(s.replace('场景','') for s in held)})", m05)
        line += f"  | thr={thr:.3f}: acc={mt['acc']*100:.2f}% FPR={mt['fpr']*100:.2f}% FNR={mt['fnr']*100:.2f}%"
        line += f"  | {ne} ep (best {be})"
        if quant_check and cfg['protocol'] != 'v1':
            calib = G[tr][np.random.RandomState(0).permutation(len(tr))[:100]]
            pq, sz = quant_predict(m, calib, G[te]); P_q.append(pq)
            line += f"  | int8 acc={cmetrics(y[te], pq)['acc']*100:.2f}% ({sz/1024:.0f} KB)"
        log(line)
        rows.append(dict(fold=fi, held=held, m05=m05, thr=thr, mthr=mt, epochs=ne, best_epoch=be))
    yt, pt = np.concatenate(Y_test), np.concatenate(P_test)
    res = dict(name=name, cfg=cfg, seed=seed, folds=rows, collapses=collapses,
               pooled05=cmetrics(yt, pt), pooled_fold_thr=None, minutes=(time.time() - t0) / 60,
               best_epochs=best_epochs)
    # 每折用自己验证集选出的阈值后合并
    cm = np.sum([np.array(r['mthr']['cm']) for r in rows], axis=0)
    tn, fp, fn, tp = cm.ravel()
    res['pooled_fold_thr'] = dict(n=int(cm.sum()), acc=float((tn + tp) / cm.sum()), fpr=float(fp / (tn + fp)),
                                  fnr=float(fn / (fn + tp)), cm=cm.tolist())
    res['thr_val_pooled'] = pick_threshold(np.concatenate(Y_val), np.concatenate(P_val))
    res['pooled_at_val_thr'] = cmetrics(yt, pt, res['thr_val_pooled'])
    if P_q:
        res['pooled_int8'] = cmetrics(yt, np.concatenate(P_q))
        res['int8_float_agree'] = float(np.mean((np.concatenate(P_q) >= 0.5) == (pt >= 0.5)))
    log(fmt(f"{name} pooled thr=0.5", res['pooled05']))
    log(fmt(f"{name} pooled 每折验证阈值", res['pooled_fold_thr']))
    log(fmt(f"{name} pooled 合并验证阈值 {res['thr_val_pooled']:.3f}", res['pooled_at_val_thr']))
    if P_q:
        log(fmt(f"{name} pooled int8 thr=0.5", res['pooled_int8']) + f"  与 float 一致率 {res['int8_float_agree']*100:.2f}%")
    log(f"{name} 塌缩次数 {collapses}，用时 {res['minutes']:.1f} min，best epochs {best_epochs}")
    res['_pt'] = pt.tolist(); res['_yt'] = yt.tolist()
    res['_pv'] = np.concatenate(P_val).tolist(); res['_yv'] = np.concatenate(Y_val).tolist()
    if P_q:
        res['_pq'] = np.concatenate(P_q).tolist()
    return res


# ---------------- 最终模型 ----------------
def run_final(name, cfg, g320, lz160, y, epochs, seed, log):
    G = np.stack([board_chain(g) for g in g320])[..., None]
    allidx = np.arange(len(y))
    m, _, _ = train_one(cfg, g320, lz160, y, allidx, np.array([], int), seed, epochs=epochs, verbose=2)
    p = predict(m, G)
    log(fmt(f"final {name} 训练集(全部, 板端预处理)", cmetrics(y, p)))
    # 量化校准：板端预处理的原图 + 增强图各 100 张
    r = np.random.default_rng(0)
    calib = np.concatenate([G[r.permutation(len(y))[:100]],
                            np.stack([board_chain(augment(g320[j], y[j], r, cfg))
                                      for j in r.permutation(len(y))[:100]])[..., None]])
    if len(set((p >= 0.5).astype(int))) < 2:
        raise RuntimeError('最终模型塌缩，未导出')
    tfl = to_tflite(m, calib)
    # 归档 v1 产物（只在第一次导出时）
    for f in ['model.h5', 'model_quantized.tflite', 'model_quantized_int8io.tflite']:
        src = os.path.join(OUT_DIR, f); b, e = os.path.splitext(f); dst = os.path.join(OUT_DIR, f'{b}_v1{e}')
        if os.path.exists(src) and not os.path.exists(dst):
            shutil.copy2(src, dst); log(f"归档 {f} → {os.path.basename(dst)}")
    m.save(os.path.join(OUT_DIR, 'model.h5'))
    with open(os.path.join(OUT_DIR, 'model_quantized.tflite'), 'wb') as fh:
        fh.write(tfl)
    log(f"导出 model.h5、model_quantized.tflite ({len(tfl)/1024:.1f} KB)；"
        f"int8 I/O 版请运行 convert_uint8io_to_int8io.py")
    import tensorflow as tf
    it = tf.lite.Interpreter(model_content=tfl); it.allocate_tensors()
    di, do = it.get_input_details()[0], it.get_output_details()[0]
    check_input_quant(di)
    so, zo = do['quantization']; pq = np.zeros(len(y), np.float32)
    for i in range(len(y)):
        it.set_tensor(di['index'], G[i:i + 1].astype(np.uint8)); it.invoke()
        pq[i] = (it.get_tensor(do['index'])[0, 1].astype(np.float32) - zo) * so
    log(fmt(f"final {name} int8 训练集", cmetrics(y, pq)) +
        f"  与 float 一致率 {np.mean((pq >= .5) == (p >= .5))*100:.2f}%")


def summarize(paths):
    """把若干个 --results json 汇总成 markdown 表格。"""
    allres = {}
    for p in paths:
        allres.update(json.load(open(p, encoding='utf-8')))
    pc = lambda v: f"{v*100:.1f}"
    print("| 配置 | seed | 折0 acc/FPR/FNR | 折1 | 折2 | 折3 | 合并 acc | 合并 FPR | 合并 FNR | 塌缩 |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    by = {}
    for k in sorted(allres, key=lambda k: (list(CONFIGS).index(k.split('/')[0]), k)):
        r = allres[k]; name = k.split('/')[0]
        f = ' | '.join(f"{pc(x['m05']['acc'])}/{pc(x['m05']['fpr'])}/{pc(x['m05']['fnr'])}" for x in r['folds'])
        p = r['pooled05']
        print(f"| {name} | {r['seed']} | {f} | {pc(p['acc'])} | {pc(p['fpr'])} | {pc(p['fnr'])} | {r['collapses']} |")
        by.setdefault(name, []).append(r)
    print("\n| 配置 | seed 数 | 合并 acc 均值±std | FPR 均值 | FNR 均值 | int8 合并 acc | 每折验证阈值后 acc/FPR/FNR | "
          "CV 预测上 FPR≤5% 的阈值 → acc/FNR |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- |")
    for name, rs in by.items():
        acc = np.array([r['pooled05']['acc'] for r in rs])
        fpr = np.mean([r['pooled05']['fpr'] for r in rs]); fnr = np.mean([r['pooled05']['fnr'] for r in rs])
        q = [r['pooled_int8']['acc'] for r in rs if 'pooled_int8' in r]
        ft = [r['pooled_fold_thr'] for r in rs]
        o = []
        for r in rs:
            yt, pt = np.array(r['_yt']), np.array(r['_pt'])
            t = pick_threshold(yt, pt); m = cmetrics(yt, pt, t); o.append((t, m['acc'], m['fnr']))
        print(f"| {name} | {len(rs)} | {pc(acc.mean())}±{pc(acc.std())} | {pc(fpr)} | {pc(fnr)} | "
              f"{pc(np.mean(q)) if q else '-'} | "
              f"{pc(np.mean([x['acc'] for x in ft]))}/{pc(np.mean([x['fpr'] for x in ft]))}/"
              f"{pc(np.mean([x['fnr'] for x in ft]))} | "
              f"t≈{np.mean([x[0] for x in o]):.2f} → {pc(np.mean([x[1] for x in o]))}/{pc(np.mean([x[2] for x in o]))} |")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dataset')
    ap.add_argument('--cv', nargs='*', default=[])
    ap.add_argument('--seeds', type=int, nargs='*', default=[0])
    ap.add_argument('--quant-check', action='store_true')
    ap.add_argument('--final')
    ap.add_argument('--final-epochs', type=int, default=0, help='0 = 取 CV 结果中 best epoch 的中位数')
    ap.add_argument('--results', default=os.path.join(SCRIPT_DIR, 'output', 'cv_v2_results.json'))
    ap.add_argument('--cache', default=os.path.join(os.environ.get('TEMP', '.'), 'human_ds_cache.npz'))
    ap.add_argument('--summarize', nargs='*', help='只汇总这些 results json，不训练')
    a = ap.parse_args()
    os.environ.setdefault('TF_CPP_MIN_LOG_LEVEL', '2')
    if a.summarize:
        return summarize(a.summarize)

    ds = a.dataset or next((d for d in DEFAULT_DS if os.path.isdir(d)), None)
    if not ds:
        sys.exit('数据集不存在，请用 --dataset 指定')
    files, y, scenes = load_dataset(ds)
    g320, lz160 = load_cache(files, a.cache)
    print(f"数据集 {ds}: {len(y)} 张 (human {y.sum()}, background {(y == 0).sum()})", flush=True)

    allres = json.load(open(a.results, encoding='utf-8')) if os.path.exists(a.results) else {}

    def log(s):
        print(s, flush=True)

    for name in a.cv:
        for seed in a.seeds:
            res = run_cv(name, CONFIGS[name], g320, lz160, y, scenes, seed, a.quant_check, log)
            allres[f"{name}/seed{seed}"] = res
            with open(a.results, 'w', encoding='utf-8') as fh:
                json.dump(allres, fh, ensure_ascii=False, indent=1)
    if a.final:
        ep = a.final_epochs
        if not ep:
            be = [e for k, v in allres.items() if k.startswith(a.final + '/') for e in v['best_epochs']]
            ep = int(np.median(be)) if be else 80
        log(f"最终训练 {a.final}: {ep} epochs，全部 16 个场景")
        run_final(a.final, CONFIGS[a.final], g320, lz160, y, ep, 0, log)


if __name__ == '__main__':
    main()
