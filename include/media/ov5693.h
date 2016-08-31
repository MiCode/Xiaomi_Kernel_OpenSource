/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OV5693_H__
#define __OV5693_H__

#include <media/nvc.h>
#include <media/nvc_image.h>

#define OV5693_IOCTL_SET_MODE               _IOW('o', 1, struct ov5693_mode)
#define OV5693_IOCTL_SET_FRAME_LENGTH       _IOW('o', 2, __u32)
#define OV5693_IOCTL_SET_COARSE_TIME        _IOW('o', 3, __u32)
#define OV5693_IOCTL_SET_GAIN               _IOW('o', 4, __u16)
#define OV5693_IOCTL_GET_STATUS             _IOR('o', 5, __u8)
#define OV5693_IOCTL_SET_BINNING            _IOW('o', 6, __u8)
#define OV5693_IOCTL_TEST_PATTERN           _IOW('o', 7, \
						 enum ov5693_test_pattern)
#define OV5693_IOCTL_SET_GROUP_HOLD         _IOW('o', 8, struct ov5693_ae)
/* IOCTL to set the operating mode of camera.
 * This can be either stereo , leftOnly or rightOnly */
#define OV5693_IOCTL_SET_CAMERA_MODE        _IOW('o', 10, __u32)
#define OV5693_IOCTL_SYNC_SENSORS           _IOW('o', 11, __u32)
#define OV5693_IOCTL_GET_FUSEID             _IOR('o', 12, struct nvc_fuseid)
#define OV5693_IOCTL_SET_HDR_COARSE_TIME    _IOW('o', 13, struct ov5693_hdr)
#define OV5693_IOCTL_READ_OTP_BANK          _IOWR('o', 14, \
						struct ov5693_otp_bank)
#define OV5693_IOCTL_SET_CAL_DATA           _IOW('o', 15, \
						struct ov5693_cal_data)
#define OV5693_IOCTL_GET_EEPROM_DATA        _IOR('o', 20, __u8 *)
#define OV5693_IOCTL_SET_EEPROM_DATA        _IOW('o', 21, __u8 *)
#define OV5693_IOCTL_GET_CAPS               _IOR('o', 22, struct nvc_imager_cap)
#define OV5693_IOCTL_SET_POWER              _IOW('o', 23, __u32)

#define OV5693_INVALID_COARSE_TIME  -1

#define OV5693_EEPROM_ADDRESS	0x50
#define OV5693_EEPROM_SIZE	1024
#define OV5693_EEPROM_BLOCK_SIZE	(1 << 8)
#define OV5693_EEPROM_NUM_BLOCKS \
	(OV5693_EEPROM_SIZE / OV5693_EEPROM_BLOCK_SIZE)

struct ov5693_mode {
	int res_x;
	int res_y;
	int fps;
	__u32 frame_length;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u16 gain;
	__u8 hdr_en;
};

struct ov5693_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u8  coarse_time_enable;
	__s32 gain;
	__u8  gain_enable;
};

struct ov5693_fuseid {
	__u32 size;
	__u8  id[16];
};

struct ov5693_hdr {
	__u32 coarse_time_long;
	__u32 coarse_time_short;
};

struct ov5693_otp_bank {
	__u32 id;
	__u8  buf[16];
};

struct ov5693_cal_data {
	int loaded;
	int rg_ratio;
	int bg_ratio;
	int rg_ratio_typical;
	int bg_ratio_typical;
	__u8 lenc[62];
};

/* See notes in the nvc.h file on the GPIO usage */
enum ov5693_gpio_type {
	OV5693_GPIO_TYPE_PWRDN = 0,
};

struct ov5693_eeprom_data {
	struct i2c_client *i2c_client;
	struct i2c_adapter *adap;
	struct i2c_board_info brd;
	struct regmap *regmap;
};

struct ov5693_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *dovdd;
};

struct ov5693_regulators {
	char *avdd;
	char *dvdd;
	char *dovdd;
};

struct ov5693_platform_data {
	unsigned cfg;
	unsigned num;
	const char *dev_name;
	unsigned gpio_count; /* see nvc.h GPIO notes */
	struct nvc_gpio_pdata *gpio; /* see nvc.h GPIO notes */
	unsigned lens_focal_length; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_max_aperture; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_fnumber; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_h; /* / _INT2FLOAT_DIVISOR */
	unsigned lens_view_angle_v; /* / _INT2FLOAT_DIVISOR */
	bool use_vcm_vdd;
	int (*probe_clock)(unsigned long);
	int (*power_on)(struct ov5693_power_rail *);
	int (*power_off)(struct ov5693_power_rail *);
	const char *mclk_name;
	struct nvc_imager_cap *cap;
	struct ov5693_regulators regulators;
	bool has_eeprom;
};

#endif  /* __OV5693_H__ */
