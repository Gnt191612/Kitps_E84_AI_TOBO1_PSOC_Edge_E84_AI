# model_human：人体/背景二分类模型（CM55 + Ethos-U55）

本目录存放 ml-coretools 配置和重新生成说明，生成产物放在 `proj_cm55/mtb_ml_gen/`，由 `proj_cm55/Makefile` 中的 `NN_MODEL_NAME=HUMAN_DETECT`、`NN_MODEL_FOLDER=mtb_ml_gen` 引用。产物放在 CM55 工程目录内，是因为工程外的源文件会被编到 `build/Debug/external/<绝对路径>` 下，超出 Windows 260 字符的路径限制。

| 项 | 值 |
| --- | --- |
| 来源 | `code/PSE84E_Project/NPU/train_v2.py --final stab_bg` 训练的 Keras 模型（v1 保留为 `output/*_v1`，精度见 `EVAL_REPORT.md`） |
| 输入 | `[1,120,160,1]` int8 灰度，scale 1.0，zero_point -128 → `q = gray - 128` |
| 输出 | `[1,2]` int8 softmax，scale 1/256，zero_point -128 → `p = (q + 128) / 256`；下标 0 = background，1 = human |
| Vela | `PSE8x_U55_400MHz_SOCMEM_300MHz_QUAD_XIP` / `Shared_Sram`，45 个算子全部在 NPU 上，估算 13.0 ms/帧 |
| 模型大小 | 183,088 B（`HUMAN_DETECT_MODEL_BIN_LEN`） |
| Tensor arena | 160,768 B（`HUMAN_DETECT_ARENA_SIZE`，实际使用 148,040 B） |

## 为什么要先转成 int8 I/O

`train_npu_model.py` 导出的 `model_quantized.tflite` 输入/输出是 **uint8**，ml-coretools 3.1 对 `PSE84_M55_U55 + int8x8` 只接受 int8 I/O，否则报错：

```text
Unsupported data types: input=3, output=3
```

`code/PSE84E_Project/NPU/convert_uint8io_to_int8io.py` 删掉首尾两个 uint8↔int8 `QUANTIZE` 算子，生成 `model_quantized_int8io.tflite`。权重和内部量化参数不变；用 TFLite 解释器对 3 张样例图和 20 张随机图对比，两者输出逐位一致（最大差 0）。

## coretools.json 的必填字段（ml-coretools 3.1.0.9404）

之前两次失败的原因和修复：

1. `The Vela configuration options required for PSE84_M55_U55 target, but not available` → 必须有 `optimization_settings.ethos_u_npu_vela_options`，其中 `system_config` 和 `memory_mode` 要与工具自带 `vela.ini` 的段名一致（`PSE8x_U55_400MHz_SOCMEM_300MHz_QUAD_XIP`、`Shared_Sram`）。
2. `The arena_cache_size key is required` → schema 写的是可选，但 3.1 实际要求必填。`Shared_Sram` 模式下不使用 arena cache，填 `0` 即可。

之后还依次需要：

3. 顶层 `cdv_requirements.tflm`，五个字段都必须写：`accuracy`、`mae`、`scratch_mem_kb`、`scratch_mem_opt_kb` 用浮点（`90.0`，不能写成 `90`），`cpu_cycles` 用整数。
4. `data.validation_data` 不能为空。这里用 `data_mode: "random"`，`max_samples` 写在 `validation_data` 这一层。随机数据只用于生成回归数据和估算 arena，**不代表模型精度**。

## 重新生成

```powershell
# 1) uint8 I/O -> int8 I/O（只在重新训练后需要）
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\code\PSE84E_Project\NPU
python convert_uint8io_to_int8io.py output\model_quantized.tflite output\model_quantized_int8io.tflite

# 2) 在短路径下运行 coretools（见下方“路径长度”）
$t = "$env:TEMP\mlh"; New-Item -ItemType Directory -Force $t | Out-Null
Copy-Item output\model_quantized_int8io.tflite "$t\human.tflite"
(Get-Content ..\..\..\Workspace\PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision\model_human\coretools.json -Raw).Replace('../../../code/PSE84E_Project/NPU/output/model_quantized_int8io.tflite','human.tflite') | Set-Content "$t\coretools.json" -Encoding ascii
cd $t
& "$env:LOCALAPPDATA\Programs\Imagimob Studio\mtb\tools\ml-coretools\ml-coretools.exe" --config coretools.json --mode deploy --output "$t\mtb_ml_gen"

# 3) 把 mtb_ml_gen\{mtb_ml_models,info,model_gen_dir} 复制到 proj_cm55\mtb_ml_gen\
```

本机没有单独安装 ML Pack，使用的是 DEEPCRAFT Studio 自带的 ml-coretools 3.1。安装了 ModusToolbox ML Pack 的机器上，把 exe 路径换成 ML Pack 里的 `ml-coretools` 即可。

### 已知坑

- **路径长度**：coretools 用 QEMU（`tflm-qemu-model-validation-cm55`）估算 arena，在本工程约 220 字符的路径下会报 `Can't open the file ...HUMAN_DETECT_vela_int8x8.tflite` 并退出（返回码 4294967294）。放到短路径（如 `%TEMP%\mlh`）下即可成功。
- **GBK 控制台**：`ml-coretools.exe --help` 在中文 Windows 控制台下会因 `™` 字符抛 `UnicodeEncodeError`。可用参数为 `--config`、`--mode {deploy,deploy_validate_desktop,validate_target}`、`--output`、`--logfile`。

## 精度提醒

v2 模型在没见过的场景上约为准确率 92.5%、误报率 7.6%（阈值 0.5），详见 `code/PSE84E_Project/NPU/EVAL_REPORT.md`。这些数字只来自 AI 生成的同类场馆图，尚未上板，也没有用 OV7675 实拍画面验证，必须用实拍数据重新评估。

## OV7675 实拍数据采集（DATASET_CAPTURE_MODE）

模型评估（[`EVAL_REPORT.md`](../../../code/PSE84E_Project/NPU/EVAL_REPORT.md)）表明，现有模型只用 496 张 AI 生成图训练，在未见过的场景上误报率约 29%。因此每类至少要采集 1500 张 OV7675 实拍图，场景、距离、光照和每档张数按报告中的[采集方案](../../../code/PSE84E_Project/NPU/EVAL_REPORT.md#ov7675-实拍补充数据采集方案)执行。

采集模式导出的图片，就是推理时 `preprocess_bgr565_to_gray_i8()` 的输出（`gray = q + 128`，160×120），训练数据与部署时的像素流水线逐位一致。

**1. 烧录采集固件**（默认构建为 0，不包含任何采集代码，H7 串口链路不受影响）：

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\Workspace\PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision
make build TARGET=APP_KIT_PSE84_AI TOOLCHAIN=GCC_ARM DATASET_CAPTURE_MODE=1
make program TARGET=APP_KIT_PSE84_AI TOOLCHAIN=GCC_ARM DATASET_CAPTURE_MODE=1
```

可选 make 变量：

- `DATASET_CAPTURE_BAUD`：默认 921600。PC 端收到 CRC 错误时改为 460800 或 115200，并给脚本传相同的 `--baud`。
- `DATASET_CAPTURE_INTERVAL_MS`：连续采集间隔，默认 500，即 2 fps。
- `DATASET_CAPTURE_AUTOSTART=1`：上电直接开始连续采集。

**2. PC 端接收**（只需要 `pip install pyserial pillow`）。串口选 KitProg3 的 USB-UART，一次运行只采一个“类别 × 场景 × 距离 × 光照”组合：

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\code\PSE84E_Project\NPU
python capture_dataset.py --port COM7 --label human --scene lab-1104 --distance 2m --light day --count 30
python capture_dataset.py --port COM7 --label background --scene lab-1104 --distance 2m --light day --count 30
python capture_dataset.py --selftest   # 不接板子，检查帧解析
```

图片保存为 `dataset/<label>/<scene>_<distance>_<light>_<n>.png`。同名前缀会接着已有的最大序号继续编号，不会覆盖旧文件。参数里不能用 `_`（`_` 是文件名分隔符），建议把日期写进场景名。终端会实时显示已保存张数、模型对该帧的 `p(human)`、CRC 错误数和丢帧数。

**3. 板上操作：**

- **SW1 短按**：采集 1 帧。
- **SW1 长按 ≥1 s**：开始或停止连续采集。
- **LED1（绿）**：每发出一帧闪一下。
- **LED2（蓝）**：亮着表示正在连续采集。

同一个场景采 human 和 background 时，机位要保持不变：先采有人，人离开画面后再采无人。

**注意：**

- 采集模式下，调试串口改为 `DATASET_CAPTURE_BAUD`，串口终端也必须用这个波特率。发送一帧（19.2 kB，921600 下约 210 ms）期间会暂停 FreeRTOS 调度，以免 `printf` 插入二进制帧中，因此此时 LCD 和 H7 链路会短暂停顿。这只发生在采集模式中。
- 帧格式见 `proj_cm55/source/dataset_capture.h`：`"DSF1"` + 帧计数 + 时间戳 + 宽高 + 长度 + `p(human)×1000` + 灰度数据 + CRC-32（与 zlib 相同）。
- 训练和评估脚本使用的目录结构是 `场景*/person`、`场景*/scene`。整理时 `human` 对应 `person`，`background` 对应 `scene`，场景名用作场景目录。测试集要使用训练时没出现过的场景或日期（见报告“下一步建议”第 2 条）。