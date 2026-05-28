// Target definition
#define RG_TARGET_NAME             "ESP32-P4-DEVKIT"

// Storage
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDMMC_HOST       SDMMC_HOST_SLOT_1
#define RG_STORAGE_SDMMC_SPEED      SDMMC_FREQ_DEFAULT
#define RG_STORAGE_SDMMC_LDO_CHAN   4

#define RG_GPIO_SDSPI_CMD           GPIO_NUM_44
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_43
#define RG_GPIO_SDSPI_D0            GPIO_NUM_39
#define RG_GPIO_SDSPI_D1            GPIO_NUM_40
#define RG_GPIO_SDSPI_D2            GPIO_NUM_41
#define RG_GPIO_SDSPI_D3            GPIO_NUM_42

// Audio (disabled - I2S legacy driver panics on ESP32-P4 HW v2)
#define RG_AUDIO_USE_INT_DAC        0
#define RG_AUDIO_USE_EXT_DAC        0
#define RG_GPIO_SND_I2S_BCK         47
#define RG_GPIO_SND_I2S_WS          46
#define RG_GPIO_SND_I2S_DATA        48

// Video
#define RG_SCREEN_DRIVER            2
#define RG_SCREEN_WIDTH             640
#define RG_SCREEN_HEIGHT            480
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_ROTATE            1
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}

// MIPI-DSI
#define RG_MIPI_DSI_PHY_LDO_CHAN        3
#define RG_MIPI_DSI_PHY_VOLTAGE_MV      2500
#define RG_MIPI_DSI_LANE_BITRATE_MBPS   700
#define RG_MIPI_DSI_LCD_H_RES           480
#define RG_MIPI_DSI_LCD_V_RES           640


#define RG_GPIO_I2C_SDA             GPIO_NUM_22
#define RG_GPIO_I2C_SCL             GPIO_NUM_21

// Input
// All buttons via GPIO (active low with pullup). D-pad on 26-30, action on 11-50.
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,     .num = GPIO_NUM_28, .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_26, .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = GPIO_NUM_27, .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_30, .pullup = 1, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_11, .pullup = 1, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_12,  .pullup = 1, .level = 0},\
    {RG_KEY_X,      .num = GPIO_NUM_50, .pullup = 1, .level = 0},\
    {RG_KEY_Y,      .num = GPIO_NUM_49,  .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_32, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_34, .pullup = 1, .level = 0},\
    {RG_KEY_MENU,   .num = GPIO_NUM_33, .pullup = 1, .level = 0},\
    {RG_KEY_OPTION, .num = GPIO_NUM_31,  .pullup = 1, .level = 0},\
}

// Battery
#define RG_BATTERY_DRIVER           0

#define RG_RECOVERY_BTN             RG_KEY_MENU
