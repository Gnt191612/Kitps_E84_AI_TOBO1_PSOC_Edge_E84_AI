/**
 * @file    system.h
 * @brief   系统初始化接口
 *
 * 公开接口：
 *   System_Init() - 完成所有硬件和基础模块初始化
 */

#ifndef __SYSTEM_H
#define __SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

void System_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_H */