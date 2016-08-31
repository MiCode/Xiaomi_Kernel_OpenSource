/* Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVC_H__
#define __NVC_H__

#include <linux/ioctl.h>
#include <linux/regulator/consumer.h>
#include <mach/pinmux.h>

#define NVC_INT2FLOAT_DIVISOR_1K	1000
#define NVC_INT2FLOAT_DIVISOR_1M	1000000
#define NVC_INT2FLOAT_DIVISOR		1000

struct nvc_param {
	__u32 param;
	__u32 sizeofvalue;
	__u32 variant;
	__u32 p_value;
} __packed;

enum nvc_params {
	NVC_PARAM_EXPOSURE = 0,
	NVC_PARAM_GAIN,
	NVC_PARAM_FRAMERATE,
	NVC_PARAM_MAX_FRAMERATE,
	NVC_PARAM_INPUT_CLOCK,
	NVC_PARAM_LOCUS,
	NVC_PARAM_FLASH_CAPS,
	NVC_PARAM_FLASH_LEVEL,
	NVC_PARAM_FLASH_PIN_STATE,
	NVC_PARAM_TORCH_CAPS,
	NVC_PARAM_TORCH_LEVEL,
	NVC_PARAM_FOCAL_LEN,
	NVC_PARAM_MAX_APERTURE,
	NVC_PARAM_FNUMBER,
	NVC_PARAM_EXPOSURE_LIMITS,
	NVC_PARAM_GAIN_LIMITS,
	NVC_PARAM_FRAMERATE_LIMITS,
	NVC_PARAM_FRAME_RATES,
	NVC_PARAM_CLOCK_LIMITS,
	NVC_PARAM_EXP_LATCH_TIME,
	NVC_PARAM_REGION_USED,
	NVC_PARAM_CALIBRATION_DATA,
	NVC_PARAM_CALIBRATION_OVERRIDES,
	NVC_PARAM_SELF_TEST,
	NVC_PARAM_STS,
	NVC_PARAM_TESTMODE,
	NVC_PARAM_EXPECTED_VALUES,
	NVC_PARAM_RESET,
	NVC_PARAM_OPTIMIZE_RES,
	NVC_PARAM_DETECT_COLOR_TEMP,
	NVC_PARAM_LINES_PER_SEC,
	NVC_PARAM_CAPS,
	NVC_PARAM_CUSTOM_BLOCK_INFO,
	NVC_PARAM_STEREO_CAP,
	NVC_PARAM_FOCUS_STEREO,
	NVC_PARAM_STEREO,
	NVC_PARAM_INHERENT_GAIN,
	NVC_PARAM_VIEW_ANGLE_H,
	NVC_PARAM_VIEW_ANGLE_V,
	NVC_PARAM_ISP_SETTING,
	NVC_PARAM_OPERATION_MODE,
	NVC_PARAM_SUPPORT_ISP,
	NVC_PARAM_AWB_LOCK,
	NVC_PARAM_AE_LOCK,
	NVC_PARAM_RES_CHANGE_WAIT_TIME,
	NVC_PARAM_FACTORY_CALIBRATION_DATA,
	NVC_PARAM_DEV_ID,
	NVC_PARAM_GROUP_HOLD,
	NVC_PARAM_SET_SENSOR_FLASH_MODE,
	NVC_PARAM_TORCH_QUERY,
	NVC_PARAM_FLASH_EXT_CAPS,
	NVC_PARAM_TORCH_EXT_CAPS,
	NVC_PARAM_BEGIN_VENDOR_EXTENSIONS = 0x10000000,
	NVC_PARAM_CALIBRATION_STATUS,
	NVC_PARAM_TEST_PATTERN,
	NVC_PARAM_MODULE_INFO,
	NVC_PARAM_FLASH_MAX_POWER,
	NVC_PARAM_DIRECTION,
	NVC_PARAM_SENSOR_TYPE,
	NVC_PARAM_DLI_CHECK,
	NVC_PARAM_PARALLEL_DLI_CHECK,
	NVC_PARAM_BRACKET_CAPS,
	NVC_PARAM_NUM,
	NVC_PARAM_I2C,
	NVC_PARAM_FORCE32 = 0x7FFFFFFF
};

/* sync off */
#define NVC_SYNC_OFF			0
/* use only this device (the one receiving the call) */
#define NVC_SYNC_MASTER			1
/* use only the synced device (the "other" device) */
#define NVC_SYNC_SLAVE			2
/* use both synced devices at the same time */
#define NVC_SYNC_STEREO			3

#define NVC_RESET_HARD			0
#define NVC_RESET_SOFT			1

struct nvc_param_isp {
	int attr;
	void *p_data;
	__u32 data_size;
} __packed;

struct nvc_isp_focus_param {
	__s32 min_pos;
	__s32 max_pos;
	__s32 hyperfocal;
	__s32 macro;
	__s32 powersave;
} __packed;

struct nvc_isp_focus_pos {
	__u32 is_auto;
	__s32 value;
} __packed;

struct nvc_isp_focus_region {
	__u32 num_region;
	__s32 value;
} __packed;

enum nvc_params_isp {
	NVC_PARAM_ISP_FOCUS_CAF = 16389,
	NVC_PARAM_ISP_FOCUS_CAF_PAUSE,
	NVC_PARAM_ISP_FOCUS_CAF_STS,
	NVC_PARAM_ISP_FOCUS_POS = 16407,
	NVC_PARAM_ISP_FOCUS_RANGE,
	NVC_PARAM_ISP_FOCUS_AF_RGN = 16413,
	NVC_PARAM_ISP_FOCUS_AF_RGN_MASK,
	NVC_PARAM_ISP_FOCUS_AF_RGN_STS,
	NVC_PARAM_ISP_FOCUS_CTRL = 16424,
	NVC_PARAM_ISP_FOCUS_TRGR,
	NVC_PARAM_ISP_FOCUS_STS,
};

#define NVC_PARAM_ISP_FOCUS_STS_BUSY	0
#define NVC_PARAM_ISP_FOCUS_STS_LOCKD	1
#define NVC_PARAM_ISP_FOCUS_STS_FAILD	2
#define NVC_PARAM_ISP_FOCUS_STS_ERR	3

#define NVC_PARAM_ISP_FOCUS_CTRL_ON	0
#define NVC_PARAM_ISP_FOCUS_CTRL_OFF	1
#define NVC_PARAM_ISP_FOCUS_CTRL_AUTO	2
#define NVC_PARAM_ISP_FOCUS_CTRL_ALOCK	3

#define NVC_PARAM_ISP_FOCUS_CAF_CONVRG	1
#define NVC_PARAM_ISP_FOCUS_CAF_SEARCH	2

#define NVC_PARAM_ISP_FOCUS_POS_INF	0


#define NVC_IOCTL_PWR_WR		_IOW('o', 102, int)
#define NVC_IOCTL_PWR_RD		_IOW('o', 103, int)
#define NVC_IOCTL_PARAM_WR		_IOW('o', 104, struct nvc_param)
#define NVC_IOCTL_PARAM_RD		_IOWR('o', 105, struct nvc_param)
#define NVC_IOCTL_PARAM_ISP_RD		_IOWR('o', 200, struct nvc_param_isp)
#define NVC_IOCTL_PARAM_ISP_WR		_IOWR('o', 201, struct nvc_param_isp)
#define NVC_IOCTL_FUSE_ID		_IOWR('o', 202, struct nvc_fuseid)

/* Expected higher level power calls are:
 * 1 = OFF
 * 2 = STANDBY
 * 3 = ON
 * These will be multiplied by 2 before given to the driver's PM code that
 * uses the _PWR_ defines. This allows us to insert defines to give more power
 * granularity and still remain linear with regards to the power usage and
 * full power state transition latency for easy implementation of PM
 * algorithms.
 * The PM actions:
 * _PWR_ERR = Non-valid state.
 * _PWR_OFF_FORCE = _PWR_OFF is forced regardless of standby mechanisms.
 * _PWR_OFF = Device, regulators, clocks, etc is turned off.  The longest
 *            transition time to _PWR_ON is from this state.
 * _PWR_STDBY_OFF = Device is useless but powered.  No communication possible.
 *                  Device does not retain programming.  Main purpose is for
 *                  faster return to _PWR_ON without regulator delays.
 * _PWR_STDBY = Device is in standby.  Device retains programming.
 * _PWR_COMM = Device is powered enough to communicate with the device.
 * _PWR_ON = Device is at full power with active output.
 *
 * The kernel drivers treat these calls as Guaranteed Level Of Service.
 */

#define NVC_PWR_ERR			0
#define NVC_PWR_OFF_FORCE		1
#define NVC_PWR_OFF			2
#define NVC_PWR_STDBY_OFF		3
#define NVC_PWR_STDBY			4
#define NVC_PWR_COMM			5
#define NVC_PWR_ON			6


#ifdef __KERNEL__

#include <linux/regulator/consumer.h>

/* The NVC_CFG_ defines are for the .cfg entry in the
 * platform data structure.
 */
/* Device not registered if not found */
#define NVC_CFG_NODEV			(1 << 0)
/* Don't return errors */
#define NVC_CFG_NOERR			(1 << 1)
/* Always go to _PWR_STDBY instead of _PWR_OFF */
#define NVC_CFG_OFF2STDBY		(1 << 2)
/* Init device at sys boot */
#define NVC_CFG_BOOT_INIT		(1 << 3)
/* Sync mode uses an I2C MUX to send at same time */
#define NVC_CFG_SYNC_I2C_MUX		(1 << 4)

struct nvc_regulator_init {
	unsigned vreg_num;
	const char *vreg_name;
};

struct nvc_regulator {
	bool vreg_flag;
	struct regulator *vreg;
	const char *vreg_name;
};

/* The GPIO mechanism uses the _gpio_type in the device's header file as a key
 * to define all the possible GPIO's the device will need.  The key is used to
 * combine the GPIO's defined in the platform board file using the
 * nvc_gpio_pdata structure with the nvc_gpio structure in the nvc kernel
 * driver.
 */
struct nvc_gpio_pdata {
	/* use a _gpio_type enum from the device's header file */
	unsigned gpio_type;
	/* the GPIO system number */
	unsigned gpio;
	/* init_en is typically set to true for all GPIO's used by the driver.
	 * However, some GPIO's are used by multiple drivers (CSI MUX, reset,
	 * etc.).  In this case, this is set true for only one of the drivers
	 * that uses the GPIO and false for the others.  If the platform board
	 * file initializes the GPIO, then this is false for all of the drivers
	 * using the GPIO.
	 */
	bool init_en;
	/* this defines the assert level for the general purpose GPIO's
	 * (_GPIO_TYPE_GPx, etc.).  The _GPIO_TYPE_GPx can be used for a GPIO
	 * that the driver doesn't know about but is needed in order for the
	 * device to work (CSI select, regulator, etc.).  The driver will
	 * blindly assert the GPIO when the device is operational and deassert
	 * when the device is turned off.
	 */
	bool active_high;
};

struct nvc_gpio_init {
	/* key to match in nvc_gpio_pdata */
	unsigned gpio_type;
	/* same as in gpio.h */
	unsigned long flags;
	/* same as in gpio.h */
	const char *label;
	/* used instead of nvc_gpio_pdata.active_high if use_flags true */
	bool active_high;
	/* false if nvc_gpio_pdata.active_high used else flags is used */
	bool use_flags;
};

struct nvc_gpio {
	unsigned gpio; /* system GPIO number */
	bool own; /* gets set if driver initializes */
	bool active_high; /* used for GP GPIOs */
	bool valid; /* set if struct data is valid */
	bool flag; /* scratch flag for driver implementation */
};

struct nvc_fuseid {
	__u32 size;
	__u8 data[16];
};

struct nvc_pinmux {
	struct tegra_pingroup_config pcfg;
	bool valid; /* set if struct data is valid */
	bool own; /* gets set if driver initializes */
	bool flag; /* scratch flag for driver implementation */
};

#endif /* __KERNEL__ */

#endif /* __NVC_H__ */

