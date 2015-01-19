#ifndef __DW9714_H__
#define __DW9714_H__

#include <linux/atomisp_platform.h>
#include <linux/types.h>

#define DW9714_NAME		"dw9714"

enum dw9714_tok_type {
	DW9714_8BIT  = 0x0001,
	DW9714_16BIT = 0x0002,
};

struct dw9714_vcm_settings {
	u16 code;	/* bit[9:0]: Data[9:0] */
	u8 t_src;	/* bit[4:0]: T_SRC[4:0] */
	u8 step_setting;	/* bit[3:0]: S[3:0]/bit[5:4]: MCLK[1:0] */
	bool update;
};

enum dw9714_vcm_mode {
	DW9714_DIRECT = 0x1,	/* direct control */
	DW9714_LSC = 0x2,	/* linear slope control */
	DW9714_DLC = 0x4,	/* dual level control */
};

struct dw9714_device;

#define DW9714_NAME_LEN 10
struct dw9714_chip_features {
	char name[DW9714_NAME_LEN];
	int supported_modes;
	int (*set_vcm_mode)(struct dw9714_device *dev);
};

struct dw9714_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value,
		struct dw9714_device *dev);
	int (*tweak)(struct v4l2_subdev *sd, s32 value,
		struct dw9714_device *dev);
};

/* dw9714 device structure */
struct dw9714_device {
	struct i2c_client *client;
	struct mutex input_lock;

	struct camera_vcm_control vcm_ctrl;
	struct dw9714_vcm_settings vcm_settings;
	struct timespec timestamp_t_focus_abs;
	enum dw9714_vcm_mode vcm_mode;
	s16 number_of_steps;
	bool initialized;		/* true if dw9714 is detected */
	s32 focus;			/* Current focus value */
	const struct camera_af_platform_data *platform_data;

	int (*set_vcm_mode)(struct dw9714_device *dev);
};

#define DW9714_INVALID_CONFIG	0xffffffff
#define DW9714_MAX_FOCUS_NEG   -1023
#define DW9714_MAX_FOCUS_POS	1023
#define DW9714_SLEW_STEP_MAX	0x3f
#define DW9714_SLEW_TIME_MAX	0x1f

/* MCLK[1:0] = 01 T_SRC[4:0] = 00001 S[3:0] = 0111 */
#define DELAY_PER_STEP_NS	1000000
#define DELAY_MAX_PER_STEP_NS	(1000000 * 1023)

#define DLC_ENABLE 1
#define DLC_DISABLE 0
#define VCM_PROTECTION_OFF	0xeca3
#define VCM_PROTECTION_ON	0xdc51
#define VCM_DEFAULT_S 0x0

#define vcm_step_s(a) (u8)(a & 0xf)
#define vcm_step_mclk(a) (u8)((a >> 4) & 0x3)
#define vcm_dlc_mclk(dlc, mclk) (u16)((dlc << 3) | mclk | 0xa104)
#define vcm_tsrc(tsrc) (u16)(tsrc << 3 | 0xf200)
#define vcm_val(data, s) (u16)(data << 4 | s)
#define DIRECT_VCM vcm_dlc_mclk(0, 0)

#endif
