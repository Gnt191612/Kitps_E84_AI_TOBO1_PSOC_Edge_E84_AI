# PSE84E_Project —— 旧工程，禁止烧录

> **本工程的固件不得烧录到 KIT_PSE84_AI。** 它使用的是错误的兼容设备层（`Device/XMC8400E*`），链接地址、外设寄存器和启动文件都与 PSE846GPS2DBZC4A 不符。

KIT_PSE84_AI 上实际运行的是官方多核工程：
`Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision`。
人体模型推理、与 H7 的串口协议（`proj_cm55/source/h7_link.c`）都在那里。

## 本目录还在用的部分

| 路径 | 用途 |
|---|---|
| `NPU/train_npu_model.py` | 人体/背景二分类模型训练脚本 |
| `NPU/output/` | 训练产物（`model.h5`、`model_quantized*.tflite`）；官方工程的 `model_human/coretools.json` 按此路径引用 |
| `NPU/EVAL_REPORT.md`、`NPU/eval_human_model.py` | 模型精度评估报告与脚本 |
| `Communication/protocol.h` | H7↔84E 帧格式的原始定义（官方工程已按字节兼容移植） |

其余 `Core/`、`Algorithm/`、`Drivers/`、`Device/` 等只作为应用逻辑参考保留，不再维护。

因为 `model_human/coretools.json` 和训练、评估脚本都按当前路径互相引用，本目录暂不改名或移动。
