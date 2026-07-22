/**
 * @file camera_io.c
 * @brief OV2640 SCCB 寄存器读写实现 (基于 ESP-IDF I2C)
 */

#include "camera_io.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CAM_IO";

/* I2C 配置 */
#define I2C_MASTER_PORT   I2C_NUM_0
#define I2C_MASTER_TX_BUF 0
#define I2C_MASTER_RX_BUF 0
#define I2C_ACK_CHECK     true

/* ──── 初始化 ──── */
int Cam_Init(int sda_pin, int scl_pin, uint32_t clk_hz)
{
    if (clk_hz == 0) clk_hz = 100000;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = clk_hz,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %d", ret);
        return -1;
    }

    ret = i2c_driver_install(I2C_MASTER_PORT, I2C_MODE_MASTER,
                             I2C_MASTER_RX_BUF, I2C_MASTER_TX_BUF, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %d", ret);
        return -1;
    }

    ESP_LOGI(TAG, "SCCB init OK, SDA=%d, SCL=%d, clk=%" PRIu32 "Hz", sda_pin, scl_pin, clk_hz);
    return 0;
}

/* ──── 写寄存器 ──── */
int Cam_WriteReg(uint8_t slave_id, uint8_t reg_addr, uint8_t value)
{
    uint8_t data[2] = { reg_addr, value };
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_PORT,
                                                slave_id,
                                                data, 2,
                                                pdMS_TO_TICKS(20));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cam_WriteReg(0x%02X, 0x%02X) failed: %d", reg_addr, value, ret);
        return -1;
    }
    return 0;
}

/* ──── 读寄存器 ──── */
int Cam_ReadReg(uint8_t slave_id, uint8_t reg_addr, uint8_t *value)
{
    if (!value) return -1;

    /* 写寄存器地址 */
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_PORT,
                                                slave_id,
                                                &reg_addr, 1,
                                                pdMS_TO_TICKS(20));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cam_ReadReg addr write failed: %d", ret);
        return -1;
    }

    /* 读寄存器值 */
    ret = i2c_master_read_from_device(I2C_MASTER_PORT,
                                       slave_id,
                                       value, 1,
                                       pdMS_TO_TICKS(20));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cam_ReadReg read failed: %d", ret);
        return -1;
    }
    return 0;
}

/* ──── 多字节写 ──── */
int Cam_WriteMulti(uint8_t slave_id, uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) return -1;

    uint8_t *buf = malloc(len + 1);
    if (!buf) return -1;

    buf[0] = reg_addr;
    memcpy(buf + 1, data, len);

    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_PORT,
                                                slave_id,
                                                buf, len + 1,
                                                pdMS_TO_TICKS(50));
    free(buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cam_WriteMulti failed: %d", ret);
        return -1;
    }
    return 0;
}
