#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <driver/ppa.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_commands.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_ldo_regulator.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include "lcd_ppa.h"

static const char *TAG_ST7701 = "st7701_drv";

// Panel handle
static esp_lcd_panel_handle_t mipi_dpi_panel = NULL;
static esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
static esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;

// PPA rotation
static lvgl_port_ppa_handle_t ppa_handle = NULL;

// Framebuffer (640x480 RGB565, landscape)
static uint16_t *lcd_framebuffer = NULL;

// Buffer pool for lcd_get_buffer / lcd_send_buffer
#define ST7701_BUFFER_COUNT 3
#define ST7701_BUFFER_LENGTH (LCD_BUFFER_LENGTH * 2) // bytes
static QueueHandle_t buffer_queue;

// Current window state
static int window_left, window_top, window_width, window_height;
static int window_current_y;

// Sync semaphore for DPI transfers
static SemaphoreHandle_t dpi_sync_sem = NULL;

// --- I2C / PCA9536 helpers (for backlight and reset) ---

#define PCA9536_ADDR 0x41
#define PCA9536_OUTPUT_REG 0x01
#define PCA9536_CONFIG_REG 0x03
#define PCA9536_PIN_RESET (1 << 0) // PIN0: Reset
#define PCA9536_PIN_BCKL (1 << 1) // PIN1: Backlight

static esp_err_t pca9536_write_reg(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCA9536_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t pca9536_read_reg(uint8_t reg, uint8_t *data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCA9536_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCA9536_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t pca9536_set_pin(uint8_t pin_mask, bool high)
{
    uint8_t current = 0;
    esp_err_t ret = pca9536_read_reg(PCA9536_OUTPUT_REG, &current);
    if (ret != ESP_OK) return ret;
    if (high)
    {
        current |= pin_mask;
    }
    else
    {
        current &= ~pin_mask;
    }
    return pca9536_write_reg(PCA9536_OUTPUT_REG, current);
}

// --- Vendor init commands (from GB-Drone st7701.c, 2.8" panel) ---

typedef struct {
    uint8_t cmd;
    const uint8_t *data;
    uint8_t data_len;
    uint8_t delay_ms;
} st7701_init_cmd_t;

static const st7701_init_cmd_t vendor_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xFF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x4f, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x10, 0x00}, 2, 0},
    {0xC2, (uint8_t[]){0x07, 0x14}, 2, 0},
    {0xC3, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0xa0, 0x18, 0xe1, 0x12, 0x16, 0x0c, 0x0e, 0x0d, 0x0b, 0x09, 0x14, 0x13, 0x29, 0x33, 0x1c}, 16, 0},
    {0xB1, (uint8_t[]){0xa0, 0x19, 0x21, 0xa0, 0x0c, 0x0e, 0x0d, 0x0b, 0x09, 0x14, 0x13, 0x29, 0x27, 0x2b, 0x1c}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5d}, 1, 0},
    {0xB1, (uint8_t[]){0x61}, 1, 0},
    {0xB2, (uint8_t[]){0x84}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4d}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x20}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x06, 0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x06, 0xa0, 0x08, 0xa0, 0x05, 0xa0, 0x07, 0xa0, 0x04, 0x44, 0x44}, 11, 0},
    {0xE2, (uint8_t[]){0x20, 0x24, 0x44, 0x44, 0x94, 0x96, 0x90, 0x90, 0x90, 0x90, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x22, 0x22}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0xd0, 0x91, 0xa0, 0xa0, 0xf9, 0x93, 0xa0, 0xa0, 0x89, 0x8d, 0xa0, 0xa0, 0xb8, 0xf8, 0xa0, 0xa0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x22, 0x22}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0xc0, 0x90, 0xa0, 0xa0, 0xe0, 0x92, 0xa0, 0xa0, 0x88, 0xc8, 0xa0, 0xa0, 0x8a, 0xe0, 0xa0, 0xa0}, 16, 0},
    {0xE9, (uint8_t[]){0x36, 0x00}, 2, 0},
    {0xEB, (uint8_t[]){0x00, 0x01, 0xe4, 0xe4, 0x44, 0x88, 0x40}, 7, 0},
    {0xED, (uint8_t[]){0xff, 0x45, 0x67, 0xfa, 0x01, 0x2b, 0xcf, 0xff, 0xff, 0xfc, 0xb2, 0x10, 0xaf, 0x76, 0x54, 0xff}, 16, 0},
    {0xEF, (uint8_t[]){0x10, 0x0d, 0x0d, 0x08, 0x3f, 0x1f}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120}, // Sleep out
    {0x29, (uint8_t[]){0x00}, 1, 20}, // Display on
};

static bool dpi_flush_ready_cb(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (dpi_sync_sem) {
        xSemaphoreGiveFromISR(dpi_sync_sem, &xHigherPriorityTaskWoken);
    }
    return xHigherPriorityTaskWoken == pdTRUE;
}

// --- lcd* interface implementation ---

static void lcd_set_rotation(int rotation)
{
    // Rotation is handled by PPA hardware, no LCD command needed
    (void)rotation;
}

static void lcd_set_backlight(float percent)
{
    bool on = (percent > 0);
    pca9536_set_pin(PCA9536_PIN_BCKL, on);
    ESP_LOGI(TAG_ST7701, "backlight %s", on ? "on" : "off");
}

static void lcd_set_window(int left, int top, int width, int height)
{
    window_left = left;
    window_top = top;
    window_width = width;
    window_height = height;
    window_current_y = 0;
}

static inline uint16_t *lcd_get_buffer(size_t length)
{
    uint16_t *buffer;
    if (xQueueReceive(buffer_queue, &buffer, pdMS_TO_TICKS(2500)) != pdTRUE)
        RG_PANIC("display");
    return buffer;
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    if (length == 0) {
        xQueueSend(buffer_queue, &buffer, portMAX_DELAY);
        return;
    }

    // Copy pixel data into the landscape framebuffer
    int pixels_per_line = window_width;
    int lines = length / pixels_per_line;

    for (int line = 0; line < lines && (window_current_y + line) < window_height; line++) {
        int fb_y = window_top + window_current_y + line;
        int fb_x = window_left;
        if (fb_y >= 0 && fb_y < RG_SCREEN_HEIGHT && fb_x >= 0) {
            int copy_width = RG_MIN(pixels_per_line, RG_SCREEN_WIDTH - fb_x);
            if (copy_width > 0) {
                memcpy(&lcd_framebuffer[fb_y * RG_SCREEN_WIDTH + fb_x],
                    &buffer[line * pixels_per_line],
                    copy_width * sizeof(uint16_t));
            }
        }
    }

    // PPA rotate the chunk and send to panel
    int chunk_top = window_top + window_current_y;
    int chunk_lines = lines;

    if (chunk_top < 0) { chunk_lines += chunk_top; chunk_top = 0; }
    if (chunk_top + chunk_lines > RG_SCREEN_HEIGHT) chunk_lines = RG_SCREEN_HEIGHT - chunk_top;

    if (chunk_lines > 0 && window_width > 0) {
        int chunk_width = RG_MIN(window_width, RG_SCREEN_WIDTH - window_left);
        if (chunk_width > 0) {
            // Copy chunk from framebuffer into contiguous buffer for PPA input
            for (int line = 0; line < chunk_lines; line++) {
                int fb_y = chunk_top + line;
                memcpy(&buffer[line * chunk_width],
                    &lcd_framebuffer[fb_y * RG_SCREEN_WIDTH + window_left],
                    chunk_width * sizeof(uint16_t));
            }
        }
        lvgl_port_ppa_disp_rotate_t rotate_cfg = {
            .in_buff = (uint8_t *)buffer,
            .area = {
                .x1 = window_left,
                .x2 = window_left + chunk_width - 1,
                .y1 = chunk_top,
                .y2 = chunk_top + chunk_lines - 1,
            },
            .disp_size = {
                .hres = RG_SCREEN_WIDTH, // 640 landscape
                .vres = RG_SCREEN_HEIGHT, // 480 landscape
            },
            .rotation = PPA_SRM_ROTATION_ANGLE_90,
            .ppa_mode = PPA_TRANS_MODE_BLOCKING,
            .swap_bytes = true, // retro-go sends BE RGB565, MIPI-DSI expects LE
            .user_data = NULL,
        };
        esp_err_t err = lvgl_port_ppa_rotate(ppa_handle, &rotate_cfg);
        if (err == ESP_OK) {
            uint8_t *rotated = lvgl_port_ppa_get_output_buffer(ppa_handle);
            int rx1 = rotate_cfg.area.x1;
            int ry1 = rotate_cfg.area.y1;
            int rx2 = rotate_cfg.area.x2;
            int ry2 = rotate_cfg.area.y2;

            esp_lcd_panel_draw_bitmap(mipi_dpi_panel, rx1, ry1, rx2 + 1, ry2 + 1, rotated);
        } else {
            ESP_LOGE(TAG_ST7701, "PPA rotate failed: %s", esp_err_to_name(err));
        }
    }

    window_current_y += lines;

    // Return the buffer to pool
    xQueueSend(buffer_queue, &buffer, portMAX_DELAY);
}

static void lcd_sync(void)
{
    if (dpi_sync_sem) {
        xSemaphoreTake(dpi_sync_sem, pdMS_TO_TICKS(100));
    }
}

static void lcd_init(void)
{
    ESP_LOGI(TAG_ST7701, "Initializing ST7701 MIPI-DSI display (GB-Drone remote)");

    // 1. Init I2C for PCA9536
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = RG_GPIO_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = RG_GPIO_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    // Configure PCA9536 all pins as outputs
    ESP_ERROR_CHECK(pca9536_write_reg(PCA9536_CONFIG_REG, 0x00));

    // 2. Enable MIPI DSI PHY LDO
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_config = {
        .chan_id = RG_MIPI_DSI_PHY_LDO_CHAN,
        .voltage_mv = RG_MIPI_DSI_PHY_VOLTAGE_MV,
    };

    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_config, &ldo_mipi_phy));
    ESP_LOGI(TAG_ST7701, "MIPI DSI PHY LDO powered on");

    // 3. Create MIPI DSI bus
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 1,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = RG_MIPI_DSI_LANE_BITRATE_MBPS,
    };

    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    // 4. Create DBI IO for commands
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    // 5. Create DPI panel (480x640, RGB565)
    esp_lcd_dpi_panel_config_t dpi_config = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 25,
        .virtual_channel = 0,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = RG_MIPI_DSI_LCD_H_RES, // 480
            .v_size = RG_MIPI_DSI_LCD_V_RES, // 640
            .hsync_back_porch = 30,
            .hsync_pulse_width = 10,
            .hsync_front_porch = 30,
            .vsync_back_porch = 50,
            .vsync_pulse_width = 8,
            .vsync_front_porch = 50,
        },
        .flags.use_dma2d = true,
    };

    esp_lcd_dsi_bus_handle_t dsi_bus = mipi_dsi_bus;
    ESP_ERROR_CHECK(esp_lcd_new_panel_dpi(dsi_bus, &dpi_config, &mipi_dpi_panel));
    ESP_LOGI(TAG_ST7701, "DPI panel created (480x640 RGB565)");

    // 6. Software reset
    esp_lcd_panel_io_tx_param(mipi_dbi_io, LCD_CMD_SWRESET, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(200));

    // 7. Hardware reset via PCA9536 PIN0
    pca9536_set_pin(PCA9536_PIN_RESET, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    pca9536_set_pin(PCA9536_PIN_RESET, false);
    vTaskDelay(pdMS_TO_TICKS(30));
    pca9536_set_pin(PCA9536_PIN_RESET, true);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 8. Display connection check (diagnostic)
    {
        esp_lcd_panel_io_tx_param(mipi_dbi_io, LCD_CMD_SWRESET, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(150));
        esp_lcd_panel_io_tx_param(mipi_dbi_io, LCD_CMD_SLPOUT, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));

        uint8_t id_data[4] = {0};
        esp_lcd_panel_io_rx_param(mipi_dbi_io, 0xA1, id_data, 3);
        uint8_t power_mode[2] = {0};
        esp_lcd_panel_io_rx_param(mipi_dbi_io, 0x0A, power_mode, 1);
        ESP_LOGI(TAG_ST7701, "Display ID: 0x%02x 0x%02x 0x%02x, Power: 0x%02x",
            id_data[0], id_data[1], id_data[2], power_mode[0]);
    }

    // 9. Send vendor init commands
    const int cmd_count = sizeof(vendor_init_cmds) / sizeof(st7701_init_cmd_t);
    ESP_LOGI(TAG_ST7701, "Sending %d init commands...", cmd_count);
    for (int i = 0; i < cmd_count; i++) {
        const st7701_init_cmd_t *cmd = &vendor_init_cmds[i];
        esp_lcd_panel_io_tx_param(mipi_dbi_io, cmd->cmd, cmd->data, cmd->data_len);
        if (cmd->delay_ms > 0)
            vTaskDelay(pdMS_TO_TICKS(cmd->delay_ms));
    }

    // 10. Initialize DPI panel (starts video stream)
    ESP_ERROR_CHECK(esp_lcd_panel_init(mipi_dpi_panel));
    ESP_LOGI(TAG_ST7701, "DPI panel initialized - video stream active");

    // 11. Enable backlight
    pca9536_set_pin(PCA9536_PIN_BCKL, true);

    // 12. Register DPI event callback for sync
    dpi_sync_sem = xSemaphoreCreateBinary();
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = dpi_flush_ready_cb,
    };

    esp_lcd_dpi_panel_register_event_callbacks(mipi_dpi_panel, &cbs, NULL);

    // 13. Init PPA rotation
    uint32_t ppa_buf_size = RG_SCREEN_WIDTH * RG_SCREEN_HEIGHT * 2;
    lvgl_port_ppa_cfg_t ppa_cfg = {
        .buffer_size = ppa_buf_size,
        .color_mode = PPA_SRM_COLOR_MODE_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 1,
        },
    };

    ppa_handle = lvgl_port_ppa_create(&ppa_cfg);
    assert(ppa_handle != NULL);
    ESP_LOGI(TAG_ST7701, "PPA SRM client initialized");

    // 14. Allocate landscape framebuffer (640x480 RGB565)
    lcd_framebuffer = heap_caps_calloc(RG_SCREEN_WIDTH * RG_SCREEN_HEIGHT, sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

    assert(lcd_framebuffer != NULL);
    ESP_LOGI(TAG_ST7701, "Framebuffer allocated: %dx%d RGB565 (%d bytes)",
        RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT, RG_SCREEN_WIDTH * RG_SCREEN_HEIGHT * 2);

    // 15. Allocate buffer pool
    buffer_queue = xQueueCreate(ST7701_BUFFER_COUNT, sizeof(uint16_t *));
    for (int i = 0; i < ST7701_BUFFER_COUNT; i++) {
        uint16_t *buf = heap_caps_aligned_alloc(64, ST7701_BUFFER_LENGTH,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

        assert(buf != NULL);
        xQueueSend(buffer_queue, &buf, portMAX_DELAY);
    }

    ESP_LOGI(TAG_ST7701, "ST7701 MIPI-DSI display initialized (logical %dx%d, physical %dx%d)",
        RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT, RG_MIPI_DSI_LCD_H_RES, RG_MIPI_DSI_LCD_V_RES);
}

static void lcd_deinit(void)
{
    if (ppa_handle) {
        lvgl_port_ppa_delete(ppa_handle);
        ppa_handle = NULL;
    }
    if (lcd_framebuffer) {
        free(lcd_framebuffer);
        lcd_framebuffer = NULL;
    }
    if (dpi_sync_sem) {
        vSemaphoreDelete(dpi_sync_sem);
        dpi_sync_sem = NULL;
    }
}

const rg_display_driver_t rg_display_driver_st7701 = {
    .name = "st7701",
};
