/*
 * Chu Controller Board Definitions
 * WHowe <github.com/whowechina>
 */

#if defined BOARD_CHU_PICO

#define MPR121_I2C i2c0
#define MPR121_SDA 16
#define MPR121_SCL 17

// if TOF_I2C is same as MPR121_I2C, TOF_SCL and TOF_SDA will be ignored
#define TOF_I2C i2c0
#define TOF_SDA 16
#define TOF_SCL 17
#define TOF_I2C_HUB 19

#define TOF_MUX_LIST { 1, 2, 0, 4, 5 }

#define RGB_PIN 2
#define RGB_ORDER GRB // or RGB

#define NKRO_KEYMAP "1aqz2swx3dec4frv5gtb6hyn7jum8ki90olp,."
#else

#endif
