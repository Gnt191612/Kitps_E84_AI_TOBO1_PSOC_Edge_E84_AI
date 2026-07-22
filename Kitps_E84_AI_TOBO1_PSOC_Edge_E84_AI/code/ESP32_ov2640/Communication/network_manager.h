/**
 * @file network_manager.h
 * @brief WiFi 连接 + WebSocket 服务端/客户端
 *
 * ESP32-A (CONFIG_ESP32_IS_SERVER=y): 运行 WS 服务器 + 连 H7
 * ESP32-B (!CONFIG_ESP32_IS_SERVER):  运行 WS 客户端连 A
 *
 * 指令格式 (浏览器 → WS → ESP32-A):
 *   "TRACK:<target_id>:<angle>:<dist>"  开始跟踪
 *   "RELEASE"                           停止跟踪
 *   "RELOAD"                            复锁
 *   "SWITCH:<target_id>"                切换目标
 *
 * 数据格式 (ESP32-B → WS → ESP32-A → 浏览器):
 *   {"s":"B","t":1,"x":150,"y":200,"l":0}
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WebSocket 命令回调
 * @param cmd    命令名 (如 "TRACK", "RELEASE", "RELOAD")
 * @param params 参数字符串
 */
typedef void (*Network_CommandCallback_t)(const char *cmd, const char *params);

/* ──── 服务端接口 (ESP32-A) ──── */

/**
 * @brief 初始化 WiFi 并启动 WS 服务器
 * @return 0=成功, -1=失败
 */
int Network_Init(void);

/**
 * @brief WebSocket 广播（向所有已连接的客户端发送文本）
 * @param data 文本数据
 * @param len  数据长度 (<=0 自动 strlen)
 * @return 0=成功
 */
int Network_BroadcastText(const char *data, int len);

/**
 * @brief 检查 WebSocket 是否有客户端在线
 * @return 在线客户端数
 */
int Network_ClientCount(void);

/**
 * @brief 注册 WebSocket 命令回调 (浏览器发来的指令)
 * @param cb 回调函数
 */
void Network_RegisterCommandCallback(Network_CommandCallback_t cb);

/* ──── 客户端接口 (ESP32-B) ──── */

/**
 * @brief 初始化 WiFi 并作为 WS 客户端连到 ESP32-A
 * @return 0=成功, -1=失败
 */
int Network_InitClient(void);

/**
 * @brief 发送 JSON 跟踪数据到 ESP32-A (由 A 转发给浏览器)
 * @param json JSON 字符串
 * @param len  长度
 * @return 0=成功
 */
int Network_SendJSON(const char *json, int len);

/**
 * @brief 注册 WS 客户端接收到的转发指令回调 (ESP32-B)
 * @param cb 回调函数
 */
void Network_RegisterRelayCallback(Network_CommandCallback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_MANAGER_H */
