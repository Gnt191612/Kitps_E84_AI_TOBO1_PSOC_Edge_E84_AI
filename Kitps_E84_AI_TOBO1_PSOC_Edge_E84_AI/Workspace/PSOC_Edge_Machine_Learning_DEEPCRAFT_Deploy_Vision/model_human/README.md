# model_human：人体/背景二分类模型（CM55 + Ethos-U55）

本目录存放 ml-coretools 配置和重新生成说明，生成产物放在 `proj_cm55/mtb_ml_gen/`，由 `proj_cm55/Makefile` 中的 `NN_MODEL_NAME=HUMAN_DETECT`、`NN_MODEL_FOLDER=mtb_ml_gen` 引用。产物放在 CM55 工程目录内，是因为工程外的源文件会被编到 `build/Debug/external/<绝对路径>` 下，超出 Windows 260 字符的路径限制。

| 项 | 值 |
| --- | --- |
| 来源 | `code/PSE84E_Project/NPU/train_npu_model.py` 训练的 Keras 模型 |
| 输入 | `[1,120,160,1]` int8 灰度，scale 1.0，zero_point -128 → `q = gray - 128` |
| 输出 | `[1,2]` int8 softmax，scale 1/256，zero_point -128 → `p = (q + 128) / 256`；下标 0 = background，1 = human |
| Vela | `PSE8x_U55_400MHz_SOCMEM_300MHz_QUAD_XIP` / `Shared_Sram`，43 个算子全部在 NPU 上，估算 12.8 ms/帧 |
| 模型大小 | 127,616 B（`HUMAN_DETECT_MODEL_BIN_LEN`） |
| Tensor arena | 153,600 B（`HUMAN_DETECT_ARENA_SIZE`，实际使用 141,960 B） |

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

本地只有 `dataset/` 下 3 张样例图，原始训练集（`%USERPROFILE%\Desktop\infer_image_train\英飞凌训练集`）不在本机。用这 3 张图测原 uint8 模型，`background1.jpg` 被判为 human（0.988），`human1.jpg` 被判为 background（0.992），`human2.jpg` 判为 human（0.711）。样本太少，不能下结论，但上板前必须用真实 OV7675 画面重新评估精度和类别顺序。
