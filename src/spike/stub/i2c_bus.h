// SPIKE stand-in. esp_lcd_hx8394.c includes "i2c_bus.h" unconditionally, but
// with CONFIG_ESP_LCD_HX8394_SKIP_I2C_INIT set it uses nothing from it. The
// real vendored copy should guard that include instead of needing this file.
#pragma once
