#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "display_manager.h"
#include "esp_log.h"

#if CONFIG_DISPLAY_ENABLE
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "u8g2.h"

static const char *TAG = "display";

static i2c_master_bus_handle_t s_i2c_bus_handle;
static i2c_master_dev_handle_t s_display_dev_handle;
static u8g2_t s_u8g2;
static bool s_display_ready;

static uint8_t u8x8_byte_i2c_cb(u8x8_t *u8x8, uint8_t msg,
                                uint8_t arg_int, void *arg_ptr)
{
    static uint8_t buffer[132];
    static uint8_t buf_idx;

    switch (msg) {
    case U8X8_MSG_BYTE_INIT: {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = CONFIG_DISPLAY_I2C_ADDR,
            .scl_speed_hz = CONFIG_DISPLAY_I2C_FREQ_HZ,
            .scl_wait_us = 0,
            .flags.disable_ack_check = false,
        };
        esp_err_t ret = i2c_master_bus_add_device(s_i2c_bus_handle, &dev_config, &s_display_dev_handle);
        return ret == ESP_OK ? 1 : 0;
    }
    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        break;
    case U8X8_MSG_BYTE_SET_DC:
        break;
    case U8X8_MSG_BYTE_SEND:
        for (size_t i = 0; i < arg_int; ++i) {
            buffer[buf_idx++] = *((uint8_t *)arg_ptr + i);
        }
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        if (buf_idx > 0 && s_display_dev_handle != NULL) {
            esp_err_t ret = i2c_master_transmit(s_display_dev_handle, buffer, buf_idx, 1000);
            return ret == ESP_OK ? 1 : 0;
        }
        break;
    default:
        return 0;
    }

    return 1;
}

static uint8_t u8x8_gpio_delay_cb(u8x8_t *u8x8, uint8_t msg,
                                  uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;
    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(arg_int * 10);
        break;
    case U8X8_MSG_DELAY_100NANO:
        __asm__ __volatile__("nop");
        break;
    case U8X8_MSG_DELAY_I2C:
        esp_rom_delay_us(5 / arg_int);
        break;
    case U8X8_MSG_GPIO_RESET:
        break;
    default:
        return 0;
    }
    return 1;
}

static void draw_lines(const char *line1, const char *line2, const char *line3, const char *line4)
{
    if (!s_display_ready) {
        return;
    }

    u8g2_ClearBuffer(&s_u8g2);
    u8g2_SetFont(&s_u8g2, u8g2_font_6x13_tr);
    u8g2_DrawStr(&s_u8g2, 0, 12, line1);
    u8g2_DrawStr(&s_u8g2, 0, 26, line2);
    u8g2_DrawStr(&s_u8g2, 0, 40, line3);
    u8g2_DrawStr(&s_u8g2, 0, 54, line4);
    u8g2_SendBuffer(&s_u8g2);
}

esp_err_t display_manager_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = CONFIG_DISPLAY_I2C_PORT,
        .sda_io_num = CONFIG_DISPLAY_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_DISPLAY_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar I2C do display: %s", esp_err_to_name(err));
        return err;
    }

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &s_u8g2, U8G2_R0, u8x8_byte_i2c_cb, u8x8_gpio_delay_cb);

    u8g2_InitDisplay(&s_u8g2);
    u8g2_SetPowerSave(&s_u8g2, 0);

    s_display_ready = true;
    draw_lines("Klipper WiFi", "Display OK", "", "");
    return ESP_OK;
}

void display_manager_show_status(const char *message, float progress_percent,
                                 float extruder_temp, float bed_temp)
{
    char line1[22];
    char line2[22];
    char line3[22];
    char line4[22];

    snprintf(line1, sizeof(line1), "MSG: %.16s", message ? message : "");
    snprintf(line2, sizeof(line2), "PRG: %5.1f%%", progress_percent);
    snprintf(line3, sizeof(line3), "EXT: %5.1f C", extruder_temp);
    snprintf(line4, sizeof(line4), "BED: %5.1f C", bed_temp);

    draw_lines(line1, line2, line3, line4);
}

void display_manager_show_error(const char *line1, const char *line2)
{
    draw_lines("ERRO", line1 ? line1 : "", line2 ? line2 : "", "");
}

#else
esp_err_t display_manager_init(void)
{
    return ESP_OK;
}

void display_manager_show_status(const char *message, float progress_percent,
                                 float extruder_temp, float bed_temp)
{
    (void)message;
    (void)progress_percent;
    (void)extruder_temp;
    (void)bed_temp;
}

void display_manager_show_error(const char *line1, const char *line2)
{
    (void)line1;
    (void)line2;
}
#endif
