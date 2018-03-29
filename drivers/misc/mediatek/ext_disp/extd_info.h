/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __EXTD_INFO_H__
#define __EXTD_INFO_H__


enum EXTD_DEV_ID {
	DEV_MHL,
	DEV_EINK,
	DEV_WFD,
	DEV_MAX_NUM
};

enum EXTD_IOCTL_CMD {
	RECOMPUTE_BG_CMD,
	GET_DEV_TYPE_CMD,
	SET_LAYER_NUM_CMD
};

enum EXTD_GET_INFO_TYPE {
	AP_GET_INFO,
	SF_GET_INFO,
};

struct SWITCH_MODE_INFO_STRUCT {
	unsigned int old_session[DEV_MAX_NUM];
	unsigned int old_mode[DEV_MAX_NUM];
	unsigned int cur_mode;
	unsigned int switching;
	unsigned int ext_sid;
};

enum HDMI_FACTORY_TEST {
	STEP1_CHIP_INIT,
	STEP2_JUDGE_CALLBACK,
	STEP3_START_DPI_AND_CONFIG,
	STEP4_DPI_STOP_AND_POWER_OFF,
	STEP_FACTORY_MAX_NUM
};

struct EXTD_DRIVER {
	int (*init)(void);
	int (*post_init)(void);
	int (*deinit)(void);
	int (*enable)(int enable);
	int (*power_enable)(int enable);
	int (*set_audio_enable)(int enable);
	int (*set_audio_format)(int format);
	int (*set_resolution)(int resolution);
	int (*get_dev_info)(int is_sf, void *info);
	int (*get_capability)(void *info);
	int (*get_edid)(void *info);
	int (*wait_vsync)(void);
	int (*fake_connect)(int connect);
	int (*factory_mode_test)(enum HDMI_FACTORY_TEST test_step, void *info);
	int (*ioctl)(unsigned int ioctl_cmd, int param1, int param2, unsigned long *params);
	int (*is_force_awake)(void *argp);
};

/*get driver handle*/
const struct EXTD_DRIVER *EXTD_EPD_Driver(void);
const struct EXTD_DRIVER *EXTD_HDMI_Driver(void);

/*get driver handle for factory mode test*/
const struct EXTD_DRIVER *EXTD_Factory_HDMI_Driver(void);
const struct EXTD_DRIVER *EXTD_Factory_EPD_Driver(void);

#endif
