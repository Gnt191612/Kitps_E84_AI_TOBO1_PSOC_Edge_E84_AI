#!/usr/bin/env python3
"""
人体/背景二分类模型精度评估（不修改任何模型产物）
================================================
评估对象（都在 output/ 下）：
  float   : model.h5                      Keras 浮点模型
  uint8io : model_quantized.tflite        训练脚本导出的 INT8 模型（uint8 I/O）
  int8io  : model_quantized_int8io.tflite convert_uint8io_to_int8io.py 生成、送进 ml-coretools 的版本

数据划分：完全复现 train_npu_model.py（np.random.seed(42) 打乱，前 80% 训练，后 20% 验证）。
验证集没参与梯度更新，但 EarlyStopping/ModelCheckpoint 依据它挑选，而且与训练集来自同一批场景，
所以它只能给出偏乐观的估计。真正的跨场景泛化用 --cv（按场景留出重训，产物只写到 --cv-out，
不覆盖 output/）。

预处理：
  train : 与训练一致，PIL convert('L') + LANCZOS 缩放到 160x120
  board : 模拟 CM55 端 inference_task.c：先缩到 320x240（模拟 OV7675 帧）→ RGB565 截位 →
          整数 luma(306/601/117 >>10) → 2x2 盒式平均 → q = gray - 128
  board_rb: 同 board，但 R/B 对调（模拟摄像头字节序/颜色顺序接错）

用法:
  python eval_human_model.py                      # 三个模型 × 训练预处理 + 板端预处理
  python eval_human_model.py --cv 4 --cv-out <dir> # 额外做 4 折按场景留出交叉验证（会重训）
"""

import os, sys, glob, argparse, json
import numpy as np
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(SCRIPT_DIR, 'output')
NPU_W, NPU_H = 160, 120
CAM_W, CAM_H = 320, 240
CLASS_NAMES = ['background', 'human']

DEFAULT_DS = [
    os.path.expandvars(r'%USERPROFILE%\Desktop\infer_image_train\英飞凌训练集'),   # 训练脚本里的默认路径
    os.path.expandvars(r'%USERPROFILE%\Desktop\infer\infer_image_train\英飞凌训练集'),  # 本机实际位置
]


# ---------- 数据（与 train_npu_model.load_dataset 完全一致的顺序） ----------
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


def prep_train(fp):
    return np.array(Image.open(fp).convert('L').resize((NPU_W, NPU_H), Image.LANCZOS), dtype=np.uint8)


def prep_board(fp, swap_rb=False):
    """返回 uint8 灰度（= int8 输入 + 128），逐位复现 preprocess_bgr565_to_gray_i8。"""
    rgb = np.asarray(Image.open(fp).convert('RGB').resize((CAM_W, CAM_H), Image.BILINEAR), dtype=np.uint32)
    r, g, b = rgb[..., 0] & 0xF8, rgb[..., 1] & 0xFC, rgb[..., 2] & 0xF8   # RGB565 截位后再左移回 8 位
    if swap_rb:
        r, b = b, r
    luma = (306 * r + 601 * g + 117 * b) >> 10
    s = luma[0::2, 0::2] + luma[0::2, 1::2] + luma[1::2, 0::2] + luma[1::2, 1::2]
    return ((s + 2) >> 2).astype(np.uint8)


def load_X(files, fn):
    X = np.zeros((len(files), NPU_H, NPU_W, 1), dtype=np.uint8)
    for i, f in enumerate(files):
        X[i, :, :, 0] = fn(f)
    return X


# ---------- 推理 ----------
def run_keras(path, X):
    import keras
    m = keras.models.load_model(path, compile=False)
    return m.predict(X.astype(np.float32), batch_size=32, verbose=0)


def run_tflite(path, X):
    import tensorflow as tf
    it = tf.lite.Interpreter(model_path=path)
    it.allocate_tensors()
    di, do = it.get_input_details()[0], it.get_output_details()[0]
    s_in, z_in = di['quantization']; s_out, z_out = do['quantization']
    P = np.zeros((len(X), 2), np.float32)
    for i in range(len(X)):
        x = X[i:i + 1].astype(np.float32)
        q = np.clip(np.round(x / s_in + z_in), np.iinfo(di['dtype']).min, np.iinfo(di['dtype']).max).astype(di['dtype'])
        it.set_tensor(di['index'], q); it.invoke()
        P[i] = (it.get_tensor(do['index']).astype(np.float32)[0] - z_out) * s_out
    return P, dict(in_dtype=str(di['dtype'].__name__), in_scale=s_in, in_zp=z_in,
                   out_dtype=str(do['dtype'].__name__), out_scale=s_out, out_zp=z_out)


# ---------- 指标 ----------
def metrics(y, P):
    pred = P.argmax(1)            # 与板端一致：conf_human > conf_background 判为 human
    cm = np.zeros((2, 2), int)
    for t, p in zip(y, pred):
        cm[t, p] += 1
    tn, fp, fn, tp = cm[0, 0], cm[0, 1], cm[1, 0], cm[1, 1]
    return dict(n=int(len(y)), acc=(tn + tp) / max(1, len(y)),
                fpr=fp / max(1, tn + fp),          # 误报率：背景被判成人
                fnr=fn / max(1, fn + tp),          # 漏检率
                cm=cm.tolist(), pred=pred)


def fmt(name, m):
    cm = m['cm']
    return (f"{name:<28} n={m['n']:<4} acc={m['acc']*100:6.2f}%  FPR={m['fpr']*100:6.2f}%  FNR={m['fnr']*100:6.2f}%  "
            f"CM[[TN {cm[0][0]}, FP {cm[0][1]}], [FN {cm[1][0]}, TP {cm[1][1]}]]")


# ---------- 按场景留出交叉验证（重训，仅写到 cv_out） ----------
def scene_cv(files, y, scenes, k, cv_out, epochs, retries=3):
    import tensorflow as tf, keras
    sys.path.insert(0, SCRIPT_DIR)
    import train_npu_model as T
    os.makedirs(cv_out, exist_ok=True)
    X = load_X(files, prep_train)
    uniq = sorted(set(scenes), key=lambda s: int(s.replace('场景', '')))
    folds = [uniq[i::k] for i in range(k)]
    allP, allY, rows = [], [], []
    n_collapse = 0
    for fi, held in enumerate(folds):
        te = np.isin(scenes, held); tr_all = np.where(~te)[0]
        rng = np.random.RandomState(42 + fi); rng.shuffle(tr_all)
        sp = int(len(tr_all) * 0.85)
        tr, va = tr_all[:sp], tr_all[sp:]
        # 原脚本的训练偶尔会塌缩成"全判 human"（训练集上只输出一个类别）。
        # 塌缩时换随机种子重训，最多 retries 次，并记录塌缩次数
        for attempt in range(retries + 1):
            keras.utils.set_random_seed(42 + fi + 100 * attempt)
            m = T.build_model()
            # attempt0 与训练脚本完全相同；重试时推迟 EarlyStopping（start_from_epoch=30），
            # 因为塌缩通常是 val_accuracy 停在多数类比例 15 个 epoch 后被提前停掉
            cb = [keras.callbacks.EarlyStopping(monitor='val_accuracy', patience=15, restore_best_weights=True,
                                                start_from_epoch=0 if attempt == 0 else 30),
                  keras.callbacks.ReduceLROnPlateau(monitor='val_loss', factor=0.5, patience=5, min_lr=1e-6)]
            h = m.fit(X[tr].astype(np.float32), y[tr], validation_data=(X[va].astype(np.float32), y[va]),
                      batch_size=16, epochs=epochs, callbacks=cb, verbose=0)
            print(f"  fold{fi} attempt{attempt}: {len(h.history['loss'])} epochs, "
                  f"train_acc_max {max(h.history['accuracy']):.3f}, val_acc_max {max(h.history['val_accuracy']):.3f}",
                  flush=True)
            collapsed = len(set(m.predict(X[tr].astype(np.float32), verbose=0).argmax(1))) < 2
            if not collapsed:
                break
            print(f"  fold{fi} attempt{attempt}: 训练塌缩（只输出一个类别）", flush=True)
            n_collapse += 1
        P = m.predict(X[te].astype(np.float32), verbose=0)
        mt = metrics(y[te], P)
        line = fmt(f"fold{fi} held={','.join(held)}" + (" [塌缩]" if collapsed else ""), mt)
        print(line, flush=True); rows.append(line)
        m.save(os.path.join(cv_out, f'cv_fold{fi}.keras'))
        allP.append(P); allY.append(y[te])
    mt = metrics(np.concatenate(allY), np.concatenate(allP))
    line = fmt(f"scene-CV pooled (k={k})", mt)
    print(line); rows.append(line)
    line = f"训练塌缩次数: {n_collapse}"
    print(line); rows.append(line)
    return rows, mt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dataset', default=None)
    ap.add_argument('--cv', type=int, default=0, help='按场景留出 k 折交叉验证（会重训）')
    ap.add_argument('--cv-out', default=None, help='CV 模型输出目录（不要指向 output/）')
    ap.add_argument('--epochs', type=int, default=100)
    ap.add_argument('--json', default=None, help='把结果写成 json')
    a = ap.parse_args()

    ds = a.dataset or next((d for d in DEFAULT_DS if os.path.isdir(d)), None)
    if not ds:
        sys.exit('数据集不存在，请用 --dataset 指定')
    files, y, scenes = load_dataset(ds)
    print(f"数据集: {ds}\n  共 {len(files)} 张: human {int(y.sum())} / background {int((y == 0).sum())}, "
          f"{len(set(scenes))} 个场景")

    # 复现训练脚本划分
    np.random.seed(42)
    idx = np.random.permutation(len(files))
    sp = int(len(files) * 0.8)
    split = {'train(80%)': idx[:sp], 'val(20%)': idx[sp:], 'all': np.arange(len(files))}
    va = idx[sp:]
    print(f"  复现划分: train {sp} / val {len(va)} (val 中 human {int(y[va].sum())}, background {int((y[va] == 0).sum())})")

    X = {'train': load_X(files, prep_train)}
    X['board'] = load_X(files, prep_board)
    X['board_rb'] = load_X(files, lambda f: prep_board(f, swap_rb=True))
    d = np.abs(X['train'].astype(int) - X['board'].astype(int))
    print(f"  训练预处理 vs 板端预处理: 像素平均差 {d.mean():.2f}，最大 {d.max()}，"
          f"平均亮度 {X['train'].mean():.1f} vs {X['board'].mean():.1f}")

    results, lines, qinfo, preds = {}, [], {}, {}
    for pp in ['train', 'board', 'board_rb']:
        for mname, path in [('float', 'model.h5'), ('uint8io', 'model_quantized.tflite'),
                            ('int8io', 'model_quantized_int8io.tflite')]:
            p = os.path.join(OUT_DIR, path)
            if mname == 'float':
                P = run_keras(p, X[pp])
            else:
                P, qi = run_tflite(p, X[pp]); qinfo[mname] = qi
            preds[(mname, pp)] = P
            for sn, si in split.items():
                if pp != 'train' and sn == 'train(80%)':
                    continue
                m = metrics(y[si], P[si])
                key = f"{mname}/{pp}/{sn}"
                results[key] = {k: v for k, v in m.items() if k != 'pred'}
                lines.append(fmt(key, m))
                print(lines[-1], flush=True)

    print("\n量化参数:", json.dumps(qinfo, default=float))
    # 版本间一致性
    for pp in ['train', 'board']:
        f, u, i8 = (preds[(n, pp)] for n in ['float', 'uint8io', 'int8io'])
        print(f"[{pp}] argmax 一致率 float-uint8io {np.mean(f.argmax(1) == u.argmax(1))*100:.2f}%  "
              f"uint8io-int8io {np.mean(u.argmax(1) == i8.argmax(1))*100:.2f}%  "
              f"|p_human| 最大差 float-uint8io {np.abs(f[:, 1] - u[:, 1]).max():.4f}  "
              f"uint8io-int8io {np.abs(u[:, 1] - i8[:, 1]).max():.4f}")
    # 类别顺序检验：若把下标对调，准确率会变成多少
    P = preds[('int8io', 'train')]
    print(f"类别顺序检验(int8io/val): 当前顺序 acc {np.mean(P[va].argmax(1) == y[va])*100:.2f}%  "
          f"对调后 acc {np.mean((1 - P[va].argmax(1)) == y[va])*100:.2f}%")
    # 验证集逐场景错误
    wrong = [os.path.relpath(files[j], ds) + f" (p_human={P[j,1]:.3f})" for j in va if P[j].argmax() != y[j]]
    print(f"int8io/train 预处理 val 错例 {len(wrong)} 个:", *wrong, sep='\n  ')

    cv_rows = None
    if a.cv:
        out = a.cv_out or os.path.join(os.environ.get('TEMP', '.'), 'human_scene_cv')
        if os.path.abspath(out).startswith(os.path.abspath(OUT_DIR)):
            sys.exit('--cv-out 不能放在 output/ 下')
        print(f"\n按场景留出 {a.cv} 折交叉验证（重训，模型写到 {out}）")
        cv_rows, cvm = scene_cv(files, y, scenes, a.cv, out, a.epochs)
        results['scene_cv'] = {k: v for k, v in cvm.items() if k != 'pred'}

    if a.json:
        with open(a.json, 'w', encoding='utf-8') as fh:
            json.dump(dict(dataset=ds, results=results, qinfo=qinfo, val_wrong=wrong, cv=cv_rows),
                      fh, ensure_ascii=False, indent=1, default=float)


if __name__ == '__main__':
    main()
