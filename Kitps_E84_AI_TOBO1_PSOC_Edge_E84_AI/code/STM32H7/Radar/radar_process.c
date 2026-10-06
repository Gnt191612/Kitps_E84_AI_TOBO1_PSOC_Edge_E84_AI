/**
 * @file    radar_process.c
 * @brief   雷达点云解析与唤醒判断实现
 * 
 * 逻辑：
 *   1. 触发传感器采集 → 等待/检查数据就绪 → 读取脉冲宽度 → 换算距离 → 构造点云
 *   2. 判断是否存在0.2m外的目标，累积多帧去抖后输出唤醒信号
 *   3. 唤醒H7调度器，通知84E启动NPU识别
 */

#include "radar_process.h"
#include "servo.h"
#include <string.h>

/*----------------------------------------------------------------------------
 * 内部状态
 *----------------------------------------------------------------------------*/
static uint8_t s_initialized    = 0;
static uint8_t s_wake_buffer[RADAR_SAMPLE_COUNT] = {0};
static uint8_t s_wake_buf_index = 0;
static uint8_t s_wake_buf_full  = 0;

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void Radar_Process_Init(void)
{
    Radar_Driver_Init();
    memset(s_wake_buffer, 0, sizeof(s_wake_buffer));
    s_wake_buf_index = 0;
    s_wake_buf_full  = 0;
    s_initialized    = 1;
}


/*----------------------------------------------------------------------------
 * 单次扫描
 * 触发 HC-SR04 (PC0 TRIG → PC6 ECHO/TIM3_CH1)，等待数据就绪，
 * 构造点云并返回有效点数量
 *----------------------------------------------------------------------------*/
uint8_t Radar_Process_Scan(RawPoint_t *out_points, uint8_t max_num)
{
    if (!s_initialized) return 0;
    if (!out_points || max_num == 0) return 0;

    uint8_t count = 0;
    RawPoint_t temp;

    /* 1. 触发传感器 (单路 HC-SR04) */
    RadarStatus_t status = Radar_Trigger();
    if (status != RADAR_OK) return 0;

    /* 等待数据就绪（超时保护：最长等待65ms） */
    uint32_t start = HAL_GetTick();
    while (!Radar_Is_DataReady()) {
        if ((HAL_GetTick() - start) > 65) {
            Radar_Clear_DataReady();
            return 0;
        }
    }

    /* 2. 读取脉冲宽度并换算距离 */
    uint32_t pw   = Radar_Get_PulseWidth_us();
    float    dist = Radar_UltrasonicToDistance(pw);
    Radar_Clear_DataReady();

    /* 3. 构造点云（angle_deg = 当前舵机指向角度） */
    if (dist >= RADAR_MIN_DISTANCE_CM && dist <= RADAR_MAX_DISTANCE_CM) {
        temp.distance_cm = dist;
        /* 对外统一使用相对正前方方位：左负、右正，范围-135°~+135°。 */
        temp.angle_deg   = Servo_GetAngle() - SERVO_ANGLE_CENTER;
        temp.sensor_id   = 0;
        temp.valid       = 1;
        out_points[count++] = temp;
    }

    /* 4. 唤醒预检：是否存在0.2m外的目标 */
    uint8_t has_target = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (out_points[i].distance_cm > WAKE_DISTANCE_CM_MIN) {
            has_target = 1;
            break;
        }
    }
    Radar_Update_WakeBuffer(has_target);

    return count;
}


/*----------------------------------------------------------------------------
 * 0.2m 目标唤醒判断（去抖处理）
 * 连续 RADAR_SAMPLE_COUNT 帧中 ≥3 帧存在目标 → 唤醒
 *----------------------------------------------------------------------------*/
uint8_t Radar_Is_Target_Wake(void)
{
    if (!s_wake_buf_full) return 0;

    uint8_t positive = 0;
    for (int i = 0; i < RADAR_SAMPLE_COUNT; i++) {
        positive += s_wake_buffer[i];
    }

    return (positive >= 3) ? 1 : 0;
}

void Radar_Update_WakeBuffer(uint8_t has_target)
{
    s_wake_buffer[s_wake_buf_index] = (has_target ? 1 : 0);

    if (s_wake_buf_index == (RADAR_SAMPLE_COUNT - 1)) {
        s_wake_buf_full = 1;
    }
    s_wake_buf_index = (s_wake_buf_index + 1) % RADAR_SAMPLE_COUNT;
}

uint8_t Radar_Is_Target_Within(float min_cm, float max_cm, RawPoint_t *points, uint8_t cnt)
{
    for (uint8_t i = 0; i < cnt; i++) {
        if (points[i].distance_cm >= min_cm &&
            points[i].distance_cm <= max_cm) {
            return 1;
        }
    }
    return 0;
}
