#ifndef __VM149_H__
#define __VM149_H__

#include <linux/atomisp_platform.h>
#include <linux/types.h>


#define VM149_VCM_ADDR	0x0c

enum vm149_tok_type {
	VM149_8BIT  = 0x0001,
	VM149_16BIT = 0x0002,
};

struct vm149_vcm_settings {
	u16 code;	/* bit[9:0]: Data[9:0] */
	u8 step_setting;	/* bit[3:0]: S[3:0]/bit[5:4]: MCLK[1:0] */
	bool update;
};

enum vm149_vcm_mode {
	VM149_DIRECT = 0x1,	/* direct control */
	VM149_LSC = 0x2,	/* linear slope control */
	VM149_DLC = 0x3,	/* dual level control */
};

/* vm149 device structure */
struct vm149_device {
	struct vm149_vcm_settings vcm_settings;
	struct timespec timestamp_t_focus_abs;
	s16 number_of_steps;
	bool initialized;		/* true if vm149 is detected */
	s32 focus;			/* Current focus value */
	struct timespec focus_time;	/* Time when focus was last time set */
	__u8 buffer[4];			/* Used for i2c transactions */
	const struct camera_af_platform_data *platform_data;
};

#define VM149_INVALID_CONFIG	0xffffffff
#define VM149_MAX_FOCUS_POS	1023


/* MCLK[1:0] = 01 T_SRC[4:0] = 00001 S[3:0] = 0111 */
#define DELAY_PER_STEP_NS	1000000
#define DELAY_MAX_PER_STEP_NS	(1000000 * 1023)

#define vm149_vcm_step_s(a) (u8)(a & 0xf)
#define vm149_vcm_val(data, s) (u16)(data << 4 | s)


#endif
