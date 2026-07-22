#!/usr/bin/env python3
"""
英飞凌 NPU 模型训练流水线
=========================
针对 KITPSE84ETOBO1 NNLite NPU 的二分类（背景/人体）模型训练。

用法:
  python train_npu_model.py

输出:
  - output/model.h5            ← Keras 模型（可被 imc.exe 编译）
  - output/model_quantized.tflite  ← INT8 量化 TFLite
  - model/model_weights.h      ← 替换项目中的占位权重！！！
  - output/model_weights.h     ← 备份副本

环境: Python 3.11+ numpy pillow tensorflow keras
"""

import os, sys, glob, subprocess
import numpy as np
from PIL import Image

# ===== 配置 =====
NPU_W, NPU_H = 160, 120   # NPU 输入尺寸
NUM_CLASSES  = 2
CLASS_NAMES  = ['background', 'human']

DATASET_DIR  = os.path.expandvars(
    r'%USERPROFILE%\Desktop\infer_image_train\英飞凌训练集'
)
SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR   = os.path.join(SCRIPT_DIR, 'output')
WEIGHTS_HDR  = os.path.join(SCRIPT_DIR, 'model', 'model_weights.h')
IMC_EXE      = os.path.expandvars(
    r'%LOCALAPPDATA%\Programs\Imagimob Studio\imc.exe'
)

os.makedirs(OUTPUT_DIR, exist_ok=True)

# ===== 1. 加载数据 =====
def load_dataset(ds_dir):
    files, labels = [], []
    scenes = sorted(glob.glob(os.path.join(ds_dir, '场景*')))
    if not scenes:
        raise FileNotFoundError(f"未找到场景目录: {ds_dir}")
    print(f"找到 {len(scenes)} 个场景")

    for s in scenes:
        for sub, label in [('person', 1), ('scene', 0)]:
            d = os.path.join(s, sub)
            if os.path.isdir(d):
                for f in sorted(glob.glob(os.path.join(d, '*.*'))):
                    if f.lower().endswith(('.jpg','.jpeg','.png')):
                        files.append(f); labels.append(label)
    return np.array(files), np.array(labels)

# ===== 2. 预处理 =====
def preprocess(fp):
    return np.array(Image.open(fp).convert('L').resize((NPU_W, NPU_H), Image.LANCZOS), dtype=np.uint8)

def load_all(files, labels):
    X = np.zeros((len(files), NPU_H, NPU_W, 1), dtype=np.uint8)
    for i, f in enumerate(files):
        X[i,:,:,0] = preprocess(f)
    return X, labels.copy()

# ===== 3. 构建模型 =====
def build_model():
    import keras
    from keras import layers, Model, Input

    inp = Input(shape=(NPU_H, NPU_W, 1), name='image_input')
    x = layers.Rescaling(scale=1./127.5, offset=-1.0)(inp)

    x = layers.Conv2D(16, (3,3), padding='same', activation='relu')(x)
    x = layers.DepthwiseConv2D((3,3), padding='same', activation='relu')(x)
    x = layers.MaxPooling2D((2,2))(x)           # 80x60

    x = layers.Conv2D(32, (3,3), padding='same', activation='relu')(x)
    x = layers.DepthwiseConv2D((3,3), padding='same', activation='relu')(x)
    x = layers.MaxPooling2D((2,2))(x)           # 40x30

    x = layers.Conv2D(48, (3,3), padding='same', activation='relu')(x)
    x = layers.MaxPooling2D((2,2))(x)           # 20x15

    x = layers.Conv2D(64, (3,3), padding='same', activation='relu')(x)
    x = layers.MaxPooling2D((2,2))(x)           # 10x7

    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.2)(x)
    x = layers.Dense(32, activation='relu')(x)
    out = layers.Dense(NUM_CLASSES, activation='softmax', name='output')(x)

    model = Model(inp, out, name='human_detector')
    model.compile(
        optimizer=keras.optimizers.Adam(0.001),
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    return model

# ===== 4. 训练 =====
def train(model, X_tr, y_tr, X_va, y_va):
    import tensorflow as tf
    import keras
    from keras.callbacks import EarlyStopping, ReduceLROnPlateau, ModelCheckpoint

    cb = [
        EarlyStopping(monitor='val_accuracy', patience=15, restore_best_weights=True),
        ReduceLROnPlateau(monitor='val_loss', factor=0.5, patience=5, min_lr=1e-6),
        ModelCheckpoint(os.path.join(OUTPUT_DIR, 'best.keras'), monitor='val_accuracy',
                        save_best_only=True)
    ]

    X_tr_f = X_tr.astype(np.float32)
    X_va_f = X_va.astype(np.float32)

    val_ds = tf.data.Dataset.from_tensor_slices((X_va_f, y_va)).batch(16)

    h = model.fit(X_tr_f, y_tr, validation_data=(X_va_f, y_va),
                  batch_size=16, epochs=100,
                  callbacks=cb, verbose=1)
    va = max(h.history['val_accuracy'])
    print(f"\n最佳验证准确率: {va:.3f} ({va*100:.1f}%)")
    return h, va

# ===== 5. INT8 量化导出 =====
def export_int8(model, calib_data):
    import tensorflow as tf

    def rep_ds():
        for i in range(min(100, len(calib_data))):
            yield [calib_data[i:i+1].astype(np.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = rep_ds
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.uint8
    converter.inference_output_type = tf.uint8

    tflite = converter.convert()
    tflite_path = os.path.join(OUTPUT_DIR, 'model_quantized.tflite')
    with open(tflite_path, 'wb') as f:
        f.write(tflite)
    print(f"INT8 量化模型: {tflite_path} ({len(tflite)/1024:.1f} KB)")
    return tflite, tflite_path

def extract_weights(tflite_data, keras_model_path):
    """从 Keras 模型提取权重并 int8 量化。"""
    import keras
    model = keras.models.load_model(keras_model_path)
    buf = bytearray()
    for layer in model.layers:
        for w in layer.get_weights():
            wf = w.flatten()
            scale = max(abs(wf.min()), abs(wf.max())) or 1.0
            q = np.clip(np.round(wf / scale * 127), -128, 127).astype(np.int8)
            buf.extend(q.view(np.uint8).tobytes())
    return bytes(buf)

# ===== 6. 生成 C 头文件 =====
def gen_header(weights, path):
    ts = __import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    lines = [f'/** @file model_weights.h\n *  生成: {ts}\n */\n']
    lines.append('#ifndef MODEL_WEIGHTS_H\n#define MODEL_WEIGHTS_H\n#include <stdint.h>\n')
    lines.append(f'#define MODEL_INPUT_W  {NPU_W}\n#define MODEL_INPUT_H  {NPU_H}\n'
                 f'#define MODEL_OUTPUT_CLASSES  {NUM_CLASSES}\n\n')
    lines.append('static const uint8_t model_weights[] = {\n')
    row = '    '
    for i, b in enumerate(weights):
        row += f'0x{b:02x}, '
        if (i + 1) % 16 == 0:
            lines.append(row + '\n'); row = '    '
    if row.strip():
        lines.append(row.rstrip(', ') + '\n')
    lines.append('};\n')
    lines.append(f'#define WEIGHT_SIZE  sizeof(model_weights)  /* {len(weights)} B */\n#endif\n')

    with open(path, 'w', encoding='utf-8') as f:
        f.writelines(lines)
    print(f"权重头文件: {path} ({len(weights)} B / {len(weights)/1024:.1f} KB)")

# ===== 7. 训练后调用 imc.exe =====
def run_imc(h5_path):
    if not os.path.exists(IMC_EXE):
        print(f"imc.exe 未找到: {IMC_EXE} (跳过)")
        return
    imc_out = os.path.join(OUTPUT_DIR, 'imc_output')
    os.makedirs(imc_out, exist_ok=True)
    c_file = os.path.join(imc_out, 'model.c')
    h_file = os.path.join(imc_out, 'model.h')

    r = subprocess.run([IMC_EXE, '-p', 'NPU_', '-ns',
                        '-oc', c_file, '-oh', h_file, h5_path],
                       capture_output=True, text=True, timeout=120)
    if r.returncode == 0:
        print(f"imc.exe 编译成功: {c_file}")
    else:
        print(f"imc.exe 返回 {r.returncode}: {r.stderr[:200]}")

# ===== 主流程 =====
def main():
    print("正在导入 TensorFlow（首次加载约 30-60 秒）...", flush=True)
    import tensorflow as tf
    from keras import __version__ as kv

    os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'
    print(f"TF {tf.__version__} | Keras {kv} | numpy {np.__version__}")
    print("=" * 55)

    # 1-2. 加载 + 预处理
    print("[1/6] 加载数据集...")
    fp, lb = load_dataset(DATASET_DIR)
    print("[2/6] 预处理 160x120 灰度...")
    X, y = load_all(fp, lb)

    # 打乱 / 分割
    np.random.seed(42)
    idx = np.random.permutation(len(X))
    sp = int(len(X) * 0.8)
    X_tr, y_tr = X[idx[:sp]], y[idx[:sp]]
    X_va, y_va = X[idx[sp:]], y[idx[sp:]]
    print(f"  训练: {len(X_tr)}  验证: {len(X_va)}")

    # 3-4. 构建 + 训练
    print("[3/6] 构建模型...")
    model = build_model()
    model.summary()

    print("[4/6] 训练中...")
    _, acc = train(model, X_tr, y_tr, X_va, y_va)

    # 保存 .h5
    h5 = os.path.join(OUTPUT_DIR, 'model.h5')
    model.save(h5)
    print(f"模型保存: {h5}")

    # 5. INT8 量化
    print("[5/6] INT8 量化 + 权重提取...")
    tflite, _ = export_int8(model, X_tr)
    w = extract_weights(tflite, h5)

    # 6. 生成头文件
    print("[6/6] 生成 model_weights.h...")
    gen_header(w, WEIGHTS_HDR)
    gen_header(w, os.path.join(OUTPUT_DIR, 'model_weights.h'))

    # 可选: imc.exe
    run_imc(h5)

    print(f"\n=== 完成! 验证准确率: {acc:.3f} ({acc*100:.1f}%) ===")
    print(f"权重: {len(w)} B ({len(w)/1024:.1f} KB)")
    print(f"下一步: 在 VSCode 重新编译 PSE84E_Project 即可")

if __name__ == '__main__':
    main()
