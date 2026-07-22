/**
 * @file    tcp_client.c
 * @brief   TCP客户端实现（当前未编译/未使用，保留做参考）
 *
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║ ⚠ 此文件不在 CMakeLists.txt 中编译，也未被任何模块调用。       ║
 * ║   作为未来扩展的参考代码保留。                                  ║
 * ║                                                               ║
 * ║ 原因：H7 不直接连网络，所有上位机通信通过 ESP32-A 的           ║
 * ║       WiFi WebSocket 中继（H7→UART→ESP32-A→WiFi→浏览器）。    ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include "tcp_client.h"
#include "logger.h"
#include <string.h>

/* 当前未编译/未使用 — 参见文件头注释 */

#if 0
#include "lwip/netconn.h"
#include "lwip/ip_addr.h"

static struct netconn *client_conn = NULL;
static ip_addr_t server_ip;

void TCP_Client_Init(void)
{
    /* 设置上位机IP（根据实际修改） */
    IP4_ADDR(&server_ip, 192, 168, 1, 100);

    client_conn = netconn_new(NETCONN_TCP);
    if (client_conn == NULL) {
        Logger_Print(LOG_ERROR, "TCP: Failed to create netconn");
        return;
    }

    err_t err = netconn_connect(client_conn, &server_ip, 8888);
    if (err != ERR_OK) {
        Logger_Print(LOG_ERROR, "TCP: Connect failed (%d)", err);
        netconn_delete(client_conn);
        client_conn = NULL;
    } else {
        Logger_Print(LOG_INFO, "TCP: Connected to server");
    }
}

int TCP_Client_IsConnected(void)
{
    if (client_conn == NULL) return 0;
    return 1;
}

void TCP_Client_Send(uint8_t *data, uint16_t len)
{
    if (client_conn == NULL || !TCP_Client_IsConnected()) {
        TCP_Client_Init();
        if (client_conn == NULL) return;
    }

    err_t err = netconn_write(client_conn, data, len, NETCONN_COPY);
    if (err != ERR_OK) {
        Logger_Print(LOG_ERROR, "TCP: Send error %d", err);
        netconn_close(client_conn);
        netconn_delete(client_conn);
        client_conn = NULL;
    }
}
#endif