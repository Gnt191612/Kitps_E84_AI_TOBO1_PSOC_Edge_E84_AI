/**
 * @file    config_mount.h
 * @brief   物理安装标定参数 — 三维坐标系下的各器件位置与朝向
 *
 * 根据实际安装修改此文件，使系统空间解算与实际物理布局一致。
 *
 * ============================================================================
 * 三维坐标系约定
 * ============================================================================
 *
 *   中心点 (Center): 系统几何原点 (0, 0, 0)
 *   基平面 (Base Plane): 两个跟踪点与雷达点构成的平面 (XY 平面, Z=0)
 *
 *                      Z (指向识别点)
 *                      |
 *                  识别点(PSE84E)
 *                      |
 *                      | 0.2m
 *                      |
 *      跟踪点-A  ── 中心点 ── 跟踪点-B ────→ X (沿跟踪点连线方向)
 *     (ESP32-A) 0.2m |   0.2m (ESP32-B)
 *                     |
 *                   0.2m
 *                     |
 *                   雷达点(HC-SR04+270°位置舵机)
 *                    ↓
 *                   Y (从中心指向雷达点方向)
 *
 * ----------------------------------------------------------------------------
 * 各器件精确三维坐标（单位：m）
 * ----------------------------------------------------------------------------
 *
 *   中心点 (Origin / STM32H7参考点):          ( 0.0,  0.0,  0.0)
 *   跟踪点-A (ESP32-A + OV2640 + Pan/Tilt):  (-0.2,  0.0,  0.0)
 *   跟踪点-B (ESP32-B + OV2640 + Pan/Tilt):  ( 0.2,  0.0,  0.0)
 *   雷达点  (HC-SR04 + 270°位置舵机):          ( 0.0,  0.2,  0.0)
 *   识别点  (PSE84E + OV7675 + Pan/Tilt):    ( 0.0,  0.0,  0.2)
 *
 *   注意：所有坐标以中心点为原点，基平面为 XY 平面 (Z=0)。
 *        识别点沿 Z 轴垂直于基平面。
 *
 * ----------------------------------------------------------------------------
 * 坐标系关系
 * ----------------------------------------------------------------------------
 *
 *   + X 轴: 沿两个跟踪点连线方向 (A→B)
 *   + Y 轴: 从中心指向雷达点方向，⊥ X 轴
 *   + Z 轴: 从中心指向识别点方向，⊥ 基平面 (XY 平面)
 *
 *   雷达舵机：0° = 左135°, 135° = 正前 (+Y), 270° = 右135°
 *   云台舵机：Pan=135° 为相机初始朝向（由安装决定，见各偏置定义）
 *
 * ----------------------------------------------------------------------------
 * 自然朝向（Pan=135°, Tilt=90° 时相机的物理指向）
 * ----------------------------------------------------------------------------
 *
 *   Camera0 (ESP32-A): 朝左  (指向 -X 方向)
 *   Camera1 (ESP32-B): 朝右  (指向 +X 方向)
 *   Camera2 (PSE84E):  朝前  (指向 +Y 方向)
 * ============================================================================
 */

#ifndef __CONFIG_MOUNT_H
#define __CONFIG_MOUNT_H

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * 三维空间位置常量（单位：m）
 * 定义各器件相对于中心点 (0,0,0) 的精确位置
 *----------------------------------------------------------------------------*/
#define DEVICE_CENTER_X               0.0f
#define DEVICE_CENTER_Y               0.0f
#define DEVICE_CENTER_Z               0.0f

#define DEVICE_TRACK_A_X             -0.2f   /* ESP32-A: 中心点左侧 0.2m */
#define DEVICE_TRACK_A_Y              0.0f
#define DEVICE_TRACK_A_Z              0.0f

#define DEVICE_TRACK_B_X              0.2f   /* ESP32-B: 中心点右侧 0.2m */
#define DEVICE_TRACK_B_Y              0.0f
#define DEVICE_TRACK_B_Z              0.0f

#define DEVICE_RADAR_X                0.0f   /* 雷达点: 中心点上侧 0.2m (Y轴) */
#define DEVICE_RADAR_Y                0.2f
#define DEVICE_RADAR_Z                0.0f

#define DEVICE_RECOG_X                0.0f   /* 识别点: 中心点上方 0.2m (Z轴) */
#define DEVICE_RECOG_Y                0.0f
#define DEVICE_RECOG_Z                0.2f

/* 跟踪点间基线距离 (用于双ESP32三角定位) */
#define DEVICE_TRACK_BASE_DIST_M      0.4f   /* 两跟踪点间距离 = 0.4m */
#define DEVICE_TRACK_BASE_DIST_CM    40.0f   /* 同上，单位 cm */
#define DEVICE_TRACK_BASE_DIST_MM   400.0f   /* 同上，单位 mm */

/* 平面坐标（cm），供 geometry.c 融合与三角定位使用 */
#define DEVICE_TRACK_A_X_CM         (DEVICE_TRACK_A_X * 100.0f)
#define DEVICE_TRACK_A_Y_CM         (DEVICE_TRACK_A_Y * 100.0f)
#define DEVICE_TRACK_B_X_CM         (DEVICE_TRACK_B_X * 100.0f)
#define DEVICE_TRACK_B_Y_CM         (DEVICE_TRACK_B_Y * 100.0f)
#define DEVICE_RADAR_X_CM           (DEVICE_RADAR_X * 100.0f)
#define DEVICE_RADAR_Y_CM           (DEVICE_RADAR_Y * 100.0f)
#define DEVICE_RECOG_X_CM           (DEVICE_RECOG_X * 100.0f)
#define DEVICE_RECOG_Y_CM           (DEVICE_RECOG_Y * 100.0f)

/*----------------------------------------------------------------------------
 * 云台 Pan 偏置：云台 Pan=135° 时相机相对 H7 正前方的偏离角(°)
 * 正值 = 顺时针偏转，负值 = 逆时针偏转
 *
 * 调度器按此换算：Pan = 135 + 方位角 - 偏置；方位角 = Pan - 135 + 偏置。
 * 偏置决定每块 ESP32 的可视方位范围（Pan 5°~265°），边界接力依赖两块板
 * 范围不同。上板前必须按实物安装朝向核对；若三台相机都朝前，全部改为 0。
 *----------------------------------------------------------------------------*/
#define MOUNT_CAM0_PAN_OFFSET      -90.0f   /* ESP32-A 自然朝左 → 偏 H7 正向 -90° */
#define MOUNT_CAM1_PAN_OFFSET       90.0f   /* ESP32-B 自然朝右 → 偏 H7 正向 +90° */
#define MOUNT_CAM2_PAN_OFFSET        0.0f   /* PSE84E  自然朝前 → 与 H7 同向 */

/*----------------------------------------------------------------------------
 * 云台 Tilt 偏置：相机自然俯仰角(°)
 * 90° = 水平，0° = 朝天，180° = 朝地
 *----------------------------------------------------------------------------*/
#define MOUNT_CAM0_TILT_OFFSET      90.0f   /* ESP32-A 安装后水平 */
#define MOUNT_CAM1_TILT_OFFSET      90.0f   /* ESP32-B 安装后水平 */
#define MOUNT_CAM2_TILT_OFFSET      90.0f   /* PSE84E  安装后水平 */

/*----------------------------------------------------------------------------
 * 垂直高度（用于俯仰角补偿计算，单位 cm）
 * 新三维布局中高度已由 Z 坐标精确表示，此偏置主要用于保持原有接口兼容。
 *----------------------------------------------------------------------------*/
#define MOUNT_CAM0_HEIGHT_CM        0.0f    /* ESP32-A 位于基平面 (Z=0) */
#define MOUNT_CAM1_HEIGHT_CM        0.0f    /* ESP32-B 位于基平面 (Z=0) */
#define MOUNT_CAM2_HEIGHT_CM       20.0f    /* PSE84E 距基平面 20cm 沿 Z 轴正方向 */

/*----------------------------------------------------------------------------
 * ESP32 图像 → 方位偏差换算（需与 ESP32_ov2640 中的常量一致）
 * ESP32 回传的 x 是像素 × ESP32_PIXEL_TO_MM_X 得到的“伪 mm”，不是真实尺寸。
 *----------------------------------------------------------------------------*/
#define ESP32_IMG_WIDTH_PX          160.0f
#define ESP32_PIXEL_TO_MM_X         1.875f   /* algorithm/track_algorithm.c PIXEL_TO_MM_X */
#define ESP32_CAM_FOV_H_DEG         50.0f    /* Core/system.c CAM_FOV_H_DEG */
/* 图像 x 增大对应方位角增大(+1) 还是减小(-1)，取决于相机安装，需上板确认 */
#define ESP32_IMG_X_BEARING_SIGN    1.0f

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_MOUNT_H */
