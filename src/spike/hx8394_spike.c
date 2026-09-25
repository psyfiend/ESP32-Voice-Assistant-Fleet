// SPIKE: compiles Waveshare's HX8394 driver straight from reference/ (MIT,
// gitignored), with its board-specific legacy-I2C sequence switched off - the
// same choice Waveshare's own P4_5 BSP makes (its README, "HX8394
// initialization"). See esplcd_spike.c for what the spike is for.
#if defined(DISPLAY_ESPLCD_SPIKE)
#define CONFIG_ESP_LCD_HX8394_SKIP_I2C_INIT 1
// IDF's component build injects these from idf_component.yml; PlatformIO does not.
#define ESP_LCD_HX8394_VER_MAJOR 2
#define ESP_LCD_HX8394_VER_MINOR 1
#define ESP_LCD_HX8394_VER_PATCH 0
#include "../../reference/esp-registry/waveshare__esp_lcd_hx8394-v2.1.0/esp_lcd_hx8394.c"
#endif
