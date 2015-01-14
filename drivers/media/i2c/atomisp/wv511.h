#ifndef __WV511_H__
#define __WV511_H__

#include <linux/atomisp_platform.h>
#include <linux/types.h>


#define wv511_VCM_ADDR	0x0c

enum wv511_tok_type {
	wv511_8BIT  = 0x0001,
	wv511_16BIT = 0x0002,
};

struct wv511_vcm_settings {
	u16 dac_code;
	u8 slew_rate_setting;	/* slew rate 3:0] */
};

enum wv511_vcm_mode {
	wv511_DIRECT = 0x1,	/* direct control */
	wv511_LSC = 0x2,	/* linear slope control */
};

/* wv511 device structure */
struct wv511_device {
	struct wv511_vcm_settings vcm_settings;
	struct timespec timestamp_t_focus_abs;
	enum wv511_vcm_mode vcm_mode;
	s16 number_of_steps;
	s32 focus;			/* Current focus value */
	struct timespec focus_time;	/* Time when focus was last time set */
	__u8 buffer[4];			/* Used for i2c transactions */
	const struct camera_af_platform_data *platform_data;
};

#define wv511_INVALID_CONFIG	0xffffffff
#define wv511_MAX_FOCUS_POS	1023


/* MCLK[1:0] = 01 T_SRC[4:0] = 00001 S[3:0] = 0111 */
#define DELAY_PER_STEP_NS	1000000
#define DELAY_MAX_PER_STEP_NS	(1000000 * 1023)
#define VCM_DEFAULT_S 0x0
#define vcm_step_s(a) (u8)(a & 0xf)
#define vcm_val(data, s) (u16)(data << 4 | s)




#endif
