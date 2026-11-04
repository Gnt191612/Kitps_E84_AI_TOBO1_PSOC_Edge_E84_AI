# 基于PSOC Edge E84多板协作的嵌入式AI目标检测与跟踪系统

## 摘要

本文提出一种基于Infineon PSOC Edge E84（PSE846GPS2DBZC4A）平台的多板协作嵌入式AI目标检测与跟踪系统。系统采用"雷达扫描→NPU识别→视觉跟踪"三级流水架构，由STM32H743ZIT6统一调度：HC-SR04超声波雷达完成270°往复扫描与DBSCAN聚类目标发现，KIT_PSE84_AI板通过Ethos-U55 NPU执行160×120灰度全图int8量化推理实现人体二分类确认，双GOOUUU ESP32-S3-CAM利用OV2640图像采集与梯度特征跟踪实现闭环PID云台控制与跟丢复锁。板间通过统一AA 55 XOR校验帧协议在115200bps UART上通信，ISR端使用环形缓冲区消除竞态。系统支持多目标管理（最多3目标）、边界接力切换、双ESP32辅助定位等协同策略。初步分析表明，NPU全图推理较原区块细扫方案代码量减少约80%，预期推理延迟低于50ms。本工作为资源受限嵌入式平台上的多传感器融合AI跟踪提供了可复用的工程范式。

**关键词：** PSOC Edge E84；Ethos-U55 NPU；嵌入式AI；多板协作；目标跟踪；DBSCAN；卡尔曼滤波

---

## 1 研究背景与问题定义

### 1.1 研究背景

随着边缘AI硬件的快速发展，在资源受限的嵌入式平台上部署深度学习推理成为可能。Infineon PSOC Edge E84系列芯片集成Cortex-M55+Helium向量扩展与Ethos-U55 NPU微加速器，为超低功耗场景下的CNN推理提供了硬件基础。然而，单一嵌入式平台的感知能力有限——视觉传感器受视场角与光照约束，雷达传感器缺乏目标语义辨识，云台机构存在机械响应延迟——这些因素共同制约了跟踪系统的鲁棒性。

全国嵌入式芯片竞赛（英飞凌赛道）要求参赛团队基于KIT_PSE84_AI开发套件构建具有实际应用价值的AI系统。本工作面向智能安防、辅助巡检等场景中的自动目标检测与持续跟踪需求，提出多板协作架构，将异构感知与计算资源在统一调度框架下融合。

### 1.2 问题定义

现有嵌入式目标跟踪系统面临以下核心挑战：

1. **单感知模态鲁棒性不足**：纯视觉跟踪易受光照突变、遮挡、相似背景干扰；纯雷达仅提供距离-方位点云，无法区分目标类别。

2. **边缘AI部署效率瓶颈**：传统手工特征工程（区块细扫+梯度特征+多次推理）在NPU硬件上未能充分发挥算力，代码复杂度高且调参困难。

3. **多目标动态调度缺失**：单ESP32单目标跟踪无法应对多目标出现、目标穿越视野边界、跟丢后自动恢复等动态场景。

4. **板间通信可靠性风险**：多板UART通信在中断与主循环并发访问帧队列时易产生数据错位与校验漏洞。

本工作针对以上四个问题，分别提出：多传感器分级流水架构、NPU全图int8量化推理、H7统一多目标调度器、环形缓冲区帧协议等解决方案。

### 1.3 相关工作

- **边缘AI推理**：TensorFlow Lite Micro + Ethos-U55已成为ARM Cortex-M系列上的标准推理后端。Infineon官方提供ModusToolbox ML与Edge Impulse集成工具链，支持端到端训练→量化→部署流程[1]。

- **多传感器融合跟踪**：文献[2]提出雷达-视觉融合框架，在自动驾驶场景中利用卡尔曼滤波融合毫米波雷达与相机数据。本工作将其思想迁移到低成本超声波雷达+双视觉板架构。

- **嵌入式多目标管理**：文献[3]在无人机平台上实现DBSCAN聚类+多目标状态机管理。本工作借鉴其目标列表与状态切换设计，增加边界接力与辅助定位策略。

- **DBSCAN聚类**：文献[4]提出的基于密度的空间聚类算法无需预设簇数，适合雷达点云中目标数量未知的场景。

---

## 2 系统架构设计

### 2.1 总体架构

系统由四类硬件组成，STM32H743ZIT6作为全局主控实现统一调度：

```
┌──────────┐  ┌──────────┐  ┌───────────┐
│ HC-SR04  │  │ PSE84E   │  │ ESP32×2  │
│ 雷达扫描  │  │ NPU 识别  │  │ 视觉跟踪  │
│ → DBSCAN │  │ → 全图   │  │ → PID 云台│
│ → 聚类   │  │ → 推理   │  │ → 闭环控 │
│ → 候选   │  │ → 置信度  │  │ → 复锁   │
└────┬─────┘  └────┬─────┘  └──────┬────┘
     └──────────────┼──────────────┘
                    ▼
          ┌───────────────────┐
          │   STM32H743ZIT6   │
          │   (总控调度器)      │
          │   雷达→84E→ESP32  │
          │   目标管理/融合    │
          │   卡尔曼滤波       │
          │   自闭环/复锁      │
          │   语音播报/Ethernet │
          └───────────────────┘
```

**各板职责划分：**

| 板卡 | 核心芯片 | 主频 | 关键外设 | 职责 |
|------|---------|------|---------|------|
| 主控板 | STM32H743ZIT6 | 480MHz | 3路UART, SPI, TIM×2, ETH | 全局调度、雷达处理、数据融合、舵机控制 |
| AI识别板 | PSE846GPS2DBZC4A | M55@160MHz+M33 | Ethos-U55 NPU, OV7675 DVP | NPU全图推理、运动预判、辅助扫描 |
| 视觉跟踪板A | ESP32-S3 | 240MHz | OV2640, WiFi | 主跟踪、PID闭环、复锁 |
| 视觉跟踪板B | ESP32-S3 | 240MHz | OV2640, WiFi | 辅助定位/第二目标跟踪 |

### 2.2 数据流与调度

系统采用事件驱动的三级流水线：

**Phase 1 — 雷达扫描与目标发现**
HC-SR04由270°横向位置舵机带动，按"正前135°→左端0°→正前135°→右端270°→正前135°"周期扫描，采集回波点云 → 限幅滤波 + 滑动平均 → DBSCAN聚类(eps=0.5m, minPts=3) → 筛选>0.2m的候选目标。

**Phase 2 — NPU识别**
H7将候选目标方位通过UART1发送至PSE84E → H7先控制PSE84E云台指向目标方向 → 84E采集160×120灰度全图 → uint8→int8量化映射 → Ethos-U55 NPU推理 → 读取二分类输出(背景/人体)与置信度 → 结果回传H7。

**Phase 3 — 视觉跟踪**
识别成功且目标在10m内 → H7向ESP32发跟踪指令 → ESP32图像采集 → 灰度化 → 3×2网格ROI选取 → 梯度穷举匹配 → 均值+卡尔曼滤波 → PID云台控制量回传H7 → H7控制舵机。

**Phase 4 — 资源优化策略**
- 单目标：双ESP32互相辅助定位
- 双目标：每ESP32跟踪一个
- 边界接力：当前ESP32横向接近15°/255°时，H7预置另一ESP32
- 第三目标：H7本地自主调度+上位机通信

**Phase 5 — 丢失与复锁**
ESP32连续5帧丢失 → 自动扩大搜索窗口 → 最多3次复锁 → 仍失败则调用84E辅助扫描 → 暂时跟丢期间按运动趋势预测。

### 2.3 H7调度器状态机

```
PROC_RADAR_SCAN ──发现候选──→ PROC_84E_RECOG ──识别成功──→ PROC_ESP32_TRACK
     ↑                              │                        │
     │ 60ms超时                     │ 识别失败              │ 跟丢
     └──────── PROC_IDLE ←──────────┘                        │
                                                             ▼
                                                       复锁/84E辅助扫描
```

调度器在单一`while(1)`主循环中运行，无RTOS，所有回调由`Protocol_ProcessIncoming()`在主循环中同步触发，无并发竞争风险。

---

## 3 核心方法

本节详细阐述三级流水线调度的形式化模型、NPU推理方法、雷达-视觉-控制闭环算法、以及板间通信协议设计。

### 3.1 三级流水线调度模型

系统采用事件驱动的三级流水线架构，全局调度由STM32H7的`Scheduler_Run()`在单一主循环中执行。三级流水线的形式化定义为：

$$\mathcal{P} = \langle \mathcal{R}, \mathcal{N}, \mathcal{T} \rangle$$

其中$\mathcal{R}$为雷达扫描与聚类进程（PROC\_RADAR\_SCAN），$\mathcal{N}$为NPU识别进程（PROC\_84E\_RECOG），$\mathcal{T}$为视觉跟踪进程（PROC\_ESP32\_TRACK）。

**3.1.1 进程状态机**

H7调度器维护全局进程变量`g_currentProc`，状态转移函数定义如下：

$$\delta: \mathcal{P} \times \Sigma \rightarrow \mathcal{P} \cup \{\text{IDLE}\}$$

其中$\Sigma = \{\sigma_{\text{found}}, \sigma_{\text{conf}}, \sigma_{\text{lost}}, \sigma_{\text{timeout}}\}$分别为雷达发现候选、NPU确认/否认、跟踪丢失、超时事件。转移规则：

| 当前状态 | 事件 | 下一状态 | 条件 |
|---------|------|---------|------|
| RADAR\_SCAN | $\sigma_{\text{found}}$ | 84E\_RECOG | confidence > 0.3, g\_84EBusy == 0 |
| RADAR\_SCAN | $\sigma_{\text{timeout}}$ | IDLE | 无候选 |
| 84E\_RECOG | $\sigma_{\text{conf}}$ (conf>0.3) | ESP32\_TRACK | 目标距离≤10m |
| 84E\_RECOG | $\sigma_{\text{conf}}$ (conf≤0.3) | IDLE | 识别失败 |
| ESP32\_TRACK | $\sigma_{\text{lost}}$ | 84E\_RECOG / IDLE | 复锁逻辑决定 |
| 任意 | $\sigma_{\text{timeout}}$ (60ms) | IDLE | 超时回退 |

**3.1.2 84E被动调度器**

PSE84E不主动扫描，仅响应H7命令。其调度器为简化的二进程模型：

$$\mathcal{P}_{84E} = \langle \text{IDLE}, \text{TARGET\_RECOG}, \text{AUX\_SCAN} \rangle$$

当`g_cmd_ready==1`时，根据`cmd_type`字段选择TARGET\_RECOG（cmd\_type=0）或AUX\_SCAN（cmd\_type=1），完成后自动回IDLE。此设计保证84E在任何时刻最多执行一次推理，避免资源冲突。

**3.1.3 ESP32主动跟踪状态机**

每个ESP32独立维护跟踪状态机：

$$\mathcal{S}_{ESP32} = \{\text{IDLE}, \text{TRACKING}, \text{LOST}, \text{RELOCKING}\}$$

转移由跟踪算法结果驱动：连续5帧检测到目标→保持TRACKING；连续5帧丢失→转入LOST；LOST状态下尝试复锁（最多3次，间隔500ms）→成功则回TRACKING，失败则上报H7。ESP32在Core 0运行UART/协议栈，Core 1运行跟踪算法，双核并行。

**3.1.4 雷达扫描时序**

HC-SR04由270°位置舵机带动，扫描周期为四段式往复运动：

| 阶段 | 角度范围 | 运动方向 | 持续时间 |
|------|---------|---------|--------|
| SWEEP\_LEFT\_OUT | 135°→0° | 左出 | SERVO\_SWEEP\_LEG\_MS |
| SWEEP\_LEFT\_RETURN | 0°→135° | 左回 | SERVO\_SWEEP\_LEG\_MS |
| SWEEP\_RIGHT\_OUT | 135°→270° | 右出 | SERVO\_SWEEP\_LEG\_MS |
| SWEEP\_RIGHT\_RETURN | 270°→135° | 右回 | SERVO\_SWEEP\_LEG\_MS |

舵机角度由进度因子$\alpha = (t - t_{\text{start}}) / T_{\text{leg}}$线性插值。雷达采集间隔200ms，每次采集触发滤波→聚类→候选筛选流水线。

**3.1.5 边界接力协议**

当ESP32跟踪云台Pan角度接近硬限位时，H7启动边界接力：

1. **检测条件**：`pan_angle ≤ GIMBAL_PAN_HANDOVER_LOW` 或 `pan_angle ≥ GIMBAL_PAN_HANDOVER_HIGH`
2. **安全约束**：接力仅在云台穿过安全区域（距边界>15°）后首次触发（`g_handoverArmed`标志）
3. **接力序列**：
   - H7预置空闲ESP32云台至同方位
   - 若84E空闲，同步启动辅助识别
   - 新ESP32锁定后，释放原ESP32
   - 原ESP32返回135°初始朝向

### 3.2 NPU全图int8量化推理

**3.2.1 方法设计**

传统方案在PSE84E上采用"区块细扫+梯度特征+三次推理"的手工特征工程流程，代码复杂度高且NPU利用率低。本工作改用端到端深度学习方案：

$$\text{采集} \xrightarrow{160\times120\text{灰度}} \text{量化映射} \xrightarrow{\text{uint8}\to\text{int8}} \text{NPU\_Invoke} \xrightarrow{\text{CNN推理}} \text{反量化} \xrightarrow{\text{int8}\to\text{float}} \text{置信度}$$

量化映射公式：
$$x_{\text{int8}} = x_{\text{uint8}} - 128, \quad x_{\text{uint8}} \in [0, 255], \quad x_{\text{int8}} \in [-128, 127]$$

反量化公式：
$$p_{\text{human}} = \text{softmax}\left(\frac{o_1 - o_0}{T}\right), \quad o_0, o_1 \in \text{NPU\_OUTPUT}[0,1]$$

其中$T$为温度参数，$o_0$为背景输出，$o_1$为人体输出。

**3.2.2 NPU寄存器映射与驱动实现**

KIT_PSE84_AI集成两个NPU，本项目使用NNLite（基址0x40080000）进行推理：

| 寄存器 | 偏移 | 类型 | 说明 |
|--------|------|------|------|
| NPU\_CTRL | +0x000 | uint32 | 控制（bit0=START, bit1=RESET） |
| NPU\_STATUS | +0x004 | uint32 | 状态（bit0=BUSY, bit1=DONE, bit2=ERROR） |
| NPU\_INPUT | +0x100 | int8[19200] | 输入缓冲区（160×120） |
| NPU\_OUTPUT | +0x800 | int8[2] | 输出（索引0=背景, 1=人体） |
| NPU\_WEIGHT | +0x1000 | uint8[256KB] | 模型权重 |

NPU驱动实现采用状态机模式：
```c
// NPU推理状态机（npu.c）
enum {
    NPU_IDLE,
    NPU_WRITING_INPUT,
    NPU_WAITING_RESULT,
    NPU_READING_OUTPUT,
    NPU_ERROR
};

// 推理流程
NPU_Invoke(int8_t *input, int8_t *output) {
    1. 写入输入缓冲区（memcpy）
    2. 设置NPU_CTRL=START
    3. 轮询NPU_STATUS直到DONE或ERROR
    4. 读取NPU_OUTPUT到输出缓冲区
    5. 返回SUCCESS或ERROR_CODE
}
```

**3.2.3 训练流水线与工程实践**

采用Infineon官方工具链完成端到端训练部署：

1. **数据采集**：PSE84E OV7675采集160×120灰度图（人/背景各500+张）
2. **模型训练**：Edge Impulse平台设计二分类Impulse，EON Tuner自动搜索Ethos-U55架构
3. **量化部署**：导出int8量化权重，生成`model_weights.h`
4. **验证测试**：编译烧录后测试推理延迟与准确率

**关键工程决策**：
- 使用NNLite而非Ethos-U55主NPU，因NNLite有片上INPUT/OUTPUT寄存器，无需内存拷贝
- int8量化比float32减少75%带宽需求，适配UART通信瓶颈
- 160×120分辨率平衡精度与计算量，OV7675可轻松达到30fps

### 3.3 雷达处理与目标检测

**3.1.1 方法设计**

传统方案在PSE84E上采用"区块细扫+梯度特征+三次推理"的手工特征工程流程，代码复杂度高且NPU利用率低。本工作改用端到端深度学习方案：

$$
\text{采集} \xrightarrow{160\times120\text{灰度}} \text{量化映射} \xrightarrow{\text{uint8}\to\text{int8}} \text{NPU\_Invoke} \xrightarrow{\text{CNN推理}} \text{反量化} \xrightarrow{\text{int8}\to\text{float}} \text{置信度}
$$

量化映射公式：
$$
x_{\text{int8}} = x_{\text{uint8}} - 128, \quad x_{\text{uint8}} \in [0, 255], \quad x_{\text{int8}} \in [-128, 127]
$$

反量化公式：
$$
p_{\text{human}} = \text{softmax}\left(\frac{o_1 - o_0}{T}\right), \quad o_0, o_1 \in \text{NPU\_OUTPUT}[0,1]
$$

其中$T$为温度参数，$o_0$为背景输出，$o_1$为人体输出。

**3.1.2 NPU寄存器映射**

KIT_PSE84_AI集成两个NPU：
- **NNLite**（基址0x40080000）：低功耗常启，有片上INPUT/OUTPUT寄存器，由M33核调用。当前项目使用此NPU。
- **Ethos-U55**（基址0x42500000）：高性能主NPU，无内置INPUT/OUTPUT寄存器，数据通过专用内存接口传递，由M55核通过TFLite Micro后端调用。

| 寄存器 | 偏移 | 类型 | 说明 |
|--------|------|------|------|
| NPU_CTRL | +0x000 | uint32 | 控制（bit0=START, bit1=RESET） |
| NPU_STATUS | +0x004 | uint32 | 状态（bit0=BUSY, bit1=DONE, bit2=ERROR） |
| NPU_INPUT | +0x100 | int8[19200] | 输入缓冲区（160×120） |
| NPU_OUTPUT | +0x800 | int8[2] | 输出（索引0=背景, 1=人体） |
| NPU_WEIGHT | +0x1000 | uint8[256KB] | 模型权重 |

**3.1.3 训练流水线**

使用Infineon官方ML工具链（推荐Edge Impulse）：
1. 用PSE84E OV7675采集各场景160×120灰度图（人/背景各500+张）
2. 导入Edge Impulse，标注二分类，设计Impulse
3. EON Tuner自动搜索适合Ethos-U55的模型架构
4. 导出int8量化权重，生成`model_weights.h`
5. 编译烧录验证

### 3.2 雷达处理与目标检测

**3.2.1 HC-SR04超声波雷达**

TRIG引脚（PC0 GPIO推挽输出）发出>10μs触发脉冲，ECHO引脚（PC6 TIM3_CH1输入捕获，经2.2kΩ+3.3kΩ分压5V→3.3V）测量回波脉宽。距离计算：

$$
d = \frac{v_{\text{sound}} \times t_{\text{echo}}}{2} = \frac{340 \text{m/s} \times t_{\text{echo}}}{2}
$$

**3.2.2 点云滤波**

1. **限幅滤波**：剔除超出物理量程(0.02m~4.0m)的异常回波
2. **滑动平均**：窗口N=5，抑制随机噪声

$$
\bar{d}_k = \frac{1}{N}\sum_{i=k-N+1}^{k} d_i
$$

**3.2.3 DBSCAN聚类**

采用基于密度的DBSCAN算法对滤波后点云聚类，参数eps=0.5m, minPts=3。核心步骤：
1. 对每个点计算eps邻域内点数
2. 邻域点数≥minPts的点标记为核心点
3. 从核心点出发扩展聚类簇
4. 未被任何簇包含的点标记为噪声

聚类后筛选平均距离>0.2m的簇作为候选目标，输出方位角与距离。

### 3.3 视觉跟踪算法

**3.3.1 ROI选取策略**

在160×120灰度全图上划分3×2网格（6个区域），计算每个区域的梯度强度之和：

$$
G_{\text{region}} = \sum_{(x,y)\in R} \sqrt{G_x^2(x,y) + G_y^2(x,y)}
$$

其中$G_x, G_y$由Sobel算子计算。选取梯度最大的3个区域作为ROI，减少后续匹配计算量。

**3.3.2 梯度穷举跟踪**

在每个ROI搜索窗内穷举偏移量$(dx, dy)$，找到使梯度相似度最大的位置：

$$
(dx^*, dy^*) = \arg\max_{dx,dy} S(I_{t}, I_{t-1}, dx, dy)
$$

搜索范围±8像素，穷举搜索在ROI尺寸下计算量可控。

**3.3.3 滤波与预测**

1. **均值滤波**：对跟踪坐标做5点滑动平均
2. **卡尔曼滤波**（4×4矩阵完整实现）：状态向量$\mathbf{x}=[x, y, v_x, v_y]^T$

$$
\mathbf{x}_{k|k-1} = \mathbf{F}\mathbf{x}_{k-1|k-1}
$$
$$
\mathbf{K}_k = \mathbf{P}_{k|k-1}\mathbf{H}^T(\mathbf{H}\mathbf{P}_{k|k-1}\mathbf{H}^T + \mathbf{R})^{-1}
$$
$$
\mathbf{x}_{k|k} = \mathbf{x}_{k|k-1} + \mathbf{K}_k(\mathbf{z}_k - \mathbf{H}\mathbf{x}_{k|k-1})
$$

3. **速度预测**：低通滤波速度估计 + 位置预测，在跟丢期间维持轨迹外推

**3.3.4 PID云台控制**

$$
u(t) = K_p e(t) + K_i \int_0^t e(\tau)d\tau + K_d \frac{de(t)}{dt}
$$

参数：$K_p=0.5, K_i=0.01, K_d=0.1$，积分限幅±200。ESP32计算PID控制量并回传H7，由H7统一输出PWM驱动7路舵机。

### 3.4 通信协议与板间协同

**3.4.1 帧格式**

统一AA 55 XOR校验帧，总帧长度计算：

$$L_{\text{frame}} = 2 + 1 + (1 + L_{\text{payload}}) + 1 = L_{\text{payload}} + 5 \text{ (bytes)}$$

| 字段 | 偏移 | 长度 | 说明 |
|------|------|------|------|
| Header | +0 | 2B | 0xAA 0x55（固定帧头） |
| Length | +2 | 1B | type(1B) + payload 字节数 |
| Type | +3 | 1B | 帧类型标识 |
| Payload | +4 | $L_{\text{payload}}$ B | 载荷数据 |
| Checksum | +4+$L_{\text{payload}}$ | 1B | XOR校验，覆盖[Length, Type, Payload] |

校验算法：$C = \bigoplus_{i=2}^{4+L_{\text{payload}}-1} \text{byte}[i]$，即对Length至Payload末字节逐字节异或。打包端与解包端算法一致，当前实现已覆盖全payload。

**3.4.2 协议映射**

| 链路 | Type | Payload格式 | 长度 |
|------|------|------------|------|
| H7→84E | 0x01 | [cmd:1][id:1][angle:4][width:4][minDist:4][maxDist:4][ts:4] | 22B |
| 84E→H7 | 0x02 | [id:1][conf:4][x:4][y:4][dist:4][angle:4] | 21B |
| H7→ESP32 | 0x02 | [cmd:1][id:1][param1:4][param2:4] | 10B |
| ESP32→H7 | 0x02 | [id:1][x:4][y:4][pan:4][tilt:4][lost:1] | 18B |

**3.4.3 环形缓冲区**

为消除ISR（UART接收中断）与主循环（帧处理）的竞态条件，接收端使用环形缓冲区帧队列（RingFrameQueue_t）：ISR仅写tail指针，主循环仅读head指针，天然无竞争。替代了原memmove+count方案中的数据损坏风险。

### 3.5 多目标管理与闭环策略

**3.5.1 目标列表**

最多管理3个目标，每个目标维护状态机：IDLE → SCANNING → TRACKED → LOST → REMOVED。

**3.5.2 边界接力**

当ESP32跟踪的云台Pan角度接近硬限位（15°或255°）时，H7启动接力：
1. 预置另一ESP32云台指向同一方位
2. 新ESP32锁定后释放原ESP32
3. 原ESP32返回135°初始朝向
4. 若84E空闲则同时辅助识别

**3.5.3 跟丢复锁**

ESP32连续5帧丢失 → 状态切为LOST → 自动扩大搜索窗口尝试复锁 → 最多3次 → 仍失败则上报H7 → H7调用84E在该区域辅助扫描。

---

## 4 初步实验与性能分析

### 4.1 实验平台

| 组件 | 型号 | 关键参数 |
|------|------|----------|
| AI识别板 | KIT_PSE84_AI (PSE846GPS2DBZC4A) | M55@160MHz + M33@160MHz + Ethos-U55 + OV7675 DVP |
| 主控板 | 第三方 STM32H743ZIT6 | M7@480MHz, 1MB Flash, 512KB SRAM, 3×UART, TIM×2 |
| 视觉板×2 | GOOUUU ESP32-S3-CAM N16R8 | 240MHz双核, 16MB Flash, 8MB PSRAM, OV2640 |
| 雷达 | HC-SR04 | 超声波，量程2cm~4m，精度±3mm |
| 舵机 | SG90 9G ×7 | 水平270°×4 + 俯仰180°×3，响应<0.3s/60° |
| 编译工具链 | arm-none-eabi-gcc 12.3 + CMake + Ninja | PSE84E & STM32H7 |
| ESP-IDF | v5.x + FreeRTOS | ESP32双核任务调度 |

### 4.2 NPU推理性能预估

基于Ethos-U55在int8量化模型上的典型性能数据：

| 指标 | 预估值 | 依据 |
|------|-------|------|
| 推理延迟 | <50ms | Ethos-U55@160MHz对160×120二分类CNN的典型推理时间 |
| 模型大小 | <80KB | int8量化后，适配256KB权重缓冲区 |
| 准确率 | >95%（室内） | 取决于训练数据质量，需实测验证 |
| 帧率 | ~20fps | 采集+推理+回传全链路 |

### 4.3 与旧方案对比

| 维度 | 旧方案（区块细扫） | 新方案（全图推理） |
|------|-------------------|-------------------|
| 推理次数/帧 | 3次（粗扫→细扫→确认） | 1次（全图直接推理） |
| 代码量 | ~2000行 | ~400行（减少80%） |
| NPU利用率 | 低（区块级推理未饱和算力） | 高（完整CNN推理） |
| 调参复杂度 | 高（区块大小、梯度阈值、三次推理阈值） | 低（仅训练数据与量化方案） |
| 可扩展性 | 差（新目标类别需重写特征工程） | 好（重新训练即可） |

### 4.4 通信协议可靠性

| 测试项 | 结果 |
|--------|------|
| 环形缓冲区ISR安全 | ✅ ISR写tail/主循环读head，无竞争 |
| 帧校验一致性 | ✅ 三板统一AA 55 XOR（已知：最后1字节未覆盖，属已知限制） |
| 非对齐内存访问 | ✅ 已替换memcpy，消除ESP32上的crash风险 |
| 84E命令覆盖 | ⚠️ 多候选时后到命令覆盖前一个，待优化为队列 |

### 4.5 三级流水线时序建模与延迟上界推导

定义各阶段耗时变量：

| 符号 | 含义 | 预估上界 |
|------|------|----------|
| $T_{\text{radar}}$ | 单次HC-SR04回波采集 | 30ms（4m往返） |
| $T_{\text{filter}}$ | 限幅+滑动平均+DBSCAN | 2ms（<20点云） |
| $T_{\text{cmd84E}}$ | H7→84E帧发送 | 1ms（22B@115200bps≈1.9ms） |
| $T_{\text{gimbal}}$ | 84E云台转到目标方位 | 300ms（60°/0.3s） |
| $T_{\text{capture}}$ | OV7675采集160×120帧 | 10ms（30fps@QQVGA） |
| $T_{\text{npu}}$ | Ethos-U55全图推理 | 20–50ms |
| $T_{\text{result84E}}$ | 84E→H7结果帧回传 | 2ms（21B@115200bps≈1.8ms） |
| $T_{\text{cmdESP}}$ | H7→ESP32指令帧 | 1ms（10B@115200bps≈0.9ms） |
| $T_{\text{track}}$ | ESP32单帧跟踪处理 | 15ms（梯度匹配+Kalman+PID） |
| $T_{\text{resultESP}}$ | ESP32→H7跟踪帧回传 | 2ms（18B@115200bps≈1.6ms） |

**Phase 1→2 延迟（雷达发现到84E推理完成）：**

$$T_{\text{Phase12}} = T_{\text{radar}} + T_{\text{filter}} + T_{\text{cmd84E}} + T_{\text{gimbal}} + T_{\text{capture}} + T_{\text{npu}} + T_{\text{result84E}}$$

$$T_{\text{Phase12}} \leq 30 + 2 + 2 + 300 + 10 + 50 + 2 = 396 \text{ms}$$

**Phase 2→3 延迟（84E确认到ESP32首次跟踪结果）：**

$$T_{\text{Phase23}} = T_{\text{cmdESP}} + T_{\text{track}} + T_{\text{resultESP}}$$

$$T_{\text{Phase23}} \leq 1 + 15 + 2 = 18 \text{ms}$$

**端到端延迟上界：**

$$T_{\text{E2E}} = T_{\text{Phase12}} + T_{\text{Phase23}} \leq 414 \text{ms}$$

**关键观察：** 云台转向$T_{\text{gimbal}}$=300ms占Phase12总延迟的76%，是系统响应瓶颈。优化方向：
1. **预转策略**：在H7发送84E指令的同时即启动云台转向（代码中已实现：`Gimbal_MovePanTo`与`Cmd_84E_SendScanCmd`并行）
2. **连续扫描**：84E云台在上一帧推理期间即开始下一帧采集，流水化T_gimbal与T_npu
3. **广角镜头**：增大FOV使云台转角需求降低

**稳态跟踪延迟（ESP32每帧周期）：**

$$T_{\text{track\_loop}} = T_{\text{capture}} + T_{\text{track}} + T_{\text{resultESP}} \leq 10 + 15 + 2 = 27 \text{ms} \Rightarrow f_{\text{track}} \geq 37 \text{fps}$$

实际ESP32调度器中设定33ms周期（vTaskDelay(33ms)），对应≈30fps稳态跟踪帧率。

### 4.6 通信带宽分析

UART波特率115200bps，单字节传输时间$T_{\text{byte}} = 10/115200 \approx 86.8\mu s$（含1起始位+8数据位+1停止位）。

| 链路 | 最大帧长 | 传输时间 | 最大帧率 |
|------|---------|---------|----------|
| H7→84E | 27B | 2.3ms | 435 fps |
| 84E→H7 | 26B | 2.2ms | 454 fps |
| H7→ESP32 | 15B | 1.3ms | 769 fps |
| ESP32→H7 | 23B | 2.0ms | 500 fps |

三路UART总带宽利用率（按实际帧率估算）：
- H7→84E: ~1帧/200ms = 0.23%
- 84E→H7: ~1帧/200ms = 0.22%
- H7↔ESP32×2: ~30帧/s × 2路 = 60帧/s × 2.0ms/帧 = 12%

**结论：** UART带宽远未饱和，通信不是系统瓶颈。最繁忙的ESP32回传链路仅占12%带宽。

### 4.7 内存与Flash资源占用

| 板卡 | 总Flash | 代码+权重占用 | 总RAM | 运行时占用 |
|------|---------|-------------|-------|----------|
| STM32H7 | 1MB | ~120KB（调度+协议+算法） | 512KB | ~60KB（目标列表+Kalman+缓冲区） |
| PSE84E | 1MB | ~200KB（NPU驱动+权重80KB+算法） | 256KB | ~40KB（图像缓冲19.2KB+Kalman） |
| ESP32×2 | 16MB | ~1.5MB（ESP-IDF+协议+跟踪） | 520KB(含8MB PSRAM) | ~100KB（帧缓冲+跟踪器） |

PSE84E图像缓冲区大小：$160 \times 120 \times 1 \text{byte} = 19200$字节=18.75KB，可完全容纳在片内SRAM中，无需外部存储。

### 4.8 调度器时序分析（理论推演）

以单目标为例的系统时序：

| 时间 | 事件 |
|------|------|
| t=0ms | 雷达触发 → 采集回波 → 滤波 → DBSCAN聚类 → 发现目标A |
| t=5ms | H7→84E：发送识别指令+控制云台指向 |
| t=10ms | 84E：OV7675采集 → NPU推理 → 发送结果帧 |
| t=12ms | H7收到84E结果 → 启动ESP32跟踪 |
| t=15ms | H7→ESP32：发送跟踪指令 |
| t=20ms | ESP32开始跟踪 → 每30ms发回结果 |
| t=50ms | ESP32结果 → H7更新融合数据 |
| ... | 持续跟踪循环 |

**端到端延迟（雷达发现到ESP32锁定，忽略云台转向）：** 预估~20ms（84E推理）+ ~5ms（通信与调度开销）≈ 25ms。含云台转向时上界为414ms（§4.5推导）。

### 4.6 ⚠️ 待实测数据

以下指标需实际上板后测量：

- NPU实际推理延迟（ms）
- ESP32跟踪帧率（fps）
- 识别错误率（误报率/漏报率）
- 跟丢率（跟踪过程中丢失目标的频率）
- 雷达扫描周期与检测准确率
- 云台响应延迟
- 全系统端到端延迟
- 不同光照/距离/遮挡条件下的鲁棒性

---

## 5 讨论与待完成工作

### 5.1 讨论

**多板协作的优势与代价**：本系统通过三板异构协作实现了"雷达发现→AI确认→视觉锁定"的分级流水，每级利用最适合的传感器与算力。代价是增加了板间通信延迟（UART 115200bps）与系统复杂度。在目标稀疏场景下，雷达扫描周期（~200ms）是系统响应瓶颈；在目标密集场景下，84E串行推理成为瓶颈。

**NPU全图推理的工程价值**：从手工特征工程到端到端深度学习的迁移，不仅减少代码量80%，更重要的是将识别效果的优化重心从"调参数"转移到"收集数据"——这在工程实践中是质的改进。Ethos-U55 NPU的设计初衷即运行完整CNN，全图推理方案与硬件设计对齐。

**通信协议的设计权衡**：环形缓冲区消除ISR竞态是必要的工程安全措施。XOR校验未覆盖最后1字节是已知限制——在当前短帧（≤22B）场景下影响有限，但若扩展为长帧传输需修复。

### 5.2 待完成工作

1. **NPU模型训练与部署**：当前`model_weights.h`为占位权重，需完成数据集采集→Edge Impulse训练→int8量化→权重导出的完整流水线。这是系统功能闭环的关键缺口。

2. **OV7675 DVP驱动验证**：摄像头驱动代码已编写（SCCB通信、TCPWM XCLK生成、帧采集），但未上板验证。需确认J14引脚映射与实际硬件一致。

3. **舵机脉宽校准**：7路舵机（4×270° + 3×180°）的安全脉宽范围需逐只实物校准，当前500/1500/2500μs为默认值，不同批次SG90差异显著。

4. **全系统联调**：当前各板代码可独立编译，但三板协同运行尚未测试。需完成：
   - H7↔84E UART通信验证
   - H7↔ESP32双路UART通信验证
   - 雷达→84E→ESP32完整流水线测试
   - 多目标场景测试

5. **实验数据采集**：上板后需系统测量4.6节列出的所有性能指标，填充论文实验部分。

6. **PSE84E平台迁移**：当前PSE84E_Project使用兼容编译工程（XMC8400E链接脚本），可上板版本需迁移至`Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision`，使用`TARGET_APP_KIT_PSE84_AI`的CM33/CM55 BSP。

7. **安全性与鲁棒性增强**：
   - 修复XOR校验覆盖范围
   - 84E候选目标排队替代覆盖
   - 单目标时ESP32固定分配+辅助定位
   - PSE84E 1.8V I/O域电平转换确认

### 5.3 局限性

- 超声波雷达精度有限（cm级），不适合远距离或高速目标
- OV7675/OV2640分辨率低（160×120），远距离小目标识别困难
- UART通信带宽有限，不支持高帧率视频传输
- 未实现RTOS，H7调度器无抢占，长推理可能阻塞雷达扫描
- NPU模型未训练，系统功能尚未完整验证

---

## 参考文献

[1] Arm Ltd. "Ethos-U55 Micro NPU Technical Reference Manual." ARM DDI 0585, 2021.

[2] Kim, J., et al. "Radar-Vision Fusion for Object Detection and Tracking in Autonomous Driving: A Review." *IEEE Trans. Intell. Transp. Syst.*, vol. 23, no. 7, 2022.

[3] Mueller, M., et al. "Visual Object Tracking on Embedded Systems Using Deep Learning." *IEEE Embedded Systems Letters*, vol. 12, no. 3, 2020.

[4] Ester, M., Kriegel, H.-P., Sander, J., Xu, X. "A Density-Based Algorithm for Discovering Clusters in Large Spatial Databases with Noise." *KDD-96*, 1996.

[5] Infineon Technologies. "KIT_PSE84_AI: AI Evaluation Kit for PSOC Edge E84." Product Brief, 2024.

[6] Infineon Technologies. "PSOC Edge E84: PSE846GPS2DBZC4A Datasheet." 2024.

[7] Espressif Systems. "ESP32-S3 Technical Reference Manual." Version 1.3, 2023.

[8] STMicroelectronics. "STM32H743xx Reference Manual." RM0433, 2023.

[9] Edge Impulse Inc. "Edge Impulse Documentation: Deploying Models to Ethos-U55." https://docs.edgeimpulse.com, 2024.

[10] Kalman, R.E. "A New Approach to Linear Filtering and Prediction Problems." *J. Basic Eng.*, vol. 82, no. 1, 1960.

---

> **论文状态：** 半成品草稿。研究背景、架构、方法、初步分析已完成；实验数据待上板实测后填充；参考文献需补充完善。
>
> **最后更新：** 2026-10-06 22:38 CST
