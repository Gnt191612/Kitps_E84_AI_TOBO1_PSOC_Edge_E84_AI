/**
 * @file    exp_config.h
 * @brief   对比实验编译开关（开题报告第九节三组对照）
 *
 * 每组实验各编译两版固件，只改对应开关，其余保持一致：
 *   对照①（三高特征区域 vs 全图匹配）：在 ESP32 工程 Core/system.h 的
 *         EXP_TRACK_FULL_FRAME 切换，H7 不需要改。
 *   对照②（有 / 无历史波动预判扫描）：EXP_PREDICTIVE_AIM
 *   对照③（单目标双目互辅 vs 单目跟踪）：EXP_ASSIST_ENABLE
 *
 * 也可在 CMake 中传 -DEXP_PREDICTIVE_AIM=0 等覆盖。
 */
#ifndef __EXP_CONFIG_H
#define __EXP_CONFIG_H

/* 1 = 请求 84E 识别/复锁时按卡尔曼外推方位预转云台，并按方位波动放宽扫描窗；
 * 0 = 直接使用最近一次观测方位、固定窗宽 */
#ifndef EXP_PREDICTIVE_AIM
#define EXP_PREDICTIVE_AIM   1
#endif

/* 1 = 84E 边扫描边跟踪：雷达发现后 84E 云台转过去，84E 连续识别并回传目标在画面中的位置，
 *     H7 据此转云台跟着目标；认定是人后才把坐标交给 ESP32，之后 84E 停下待命。
 * 0 = 84E 只拍一帧判定，H7 不跟随（对照组） */
#ifndef EXP_RECOG_WHILE_TRACK
#define EXP_RECOG_WHILE_TRACK   1
#endif

/* 1 = 单目标时另一块空闲 ESP32 辅助观察并做三角定位；0 = 只用主跟踪板 */
#ifndef EXP_ASSIST_ENABLE
#define EXP_ASSIST_ENABLE    1
#endif

/* 1 = 在 UART4 输出 $E 实验事件行（tools/exp_stats.py 解析） */
#ifndef EXP_LOG_ENABLE
#define EXP_LOG_ENABLE       1
#endif

#endif /* __EXP_CONFIG_H */
