/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VTSKIN_TEMP_H__
#define __VTSKIN_TEMP_H__

#define MAX_VTSKIN_REF_NUM 10

enum vtskin_operation {
	OP_COEF,
	OP_MAX,
	OP_NUM,
};

struct vtskin_coef {
	char sensor_name[THERMAL_NAME_LENGTH + 1];
	long long sensor_coef;
};

struct vtskin_tz_param {
	char tz_name[THERMAL_NAME_LENGTH + 1];
	enum vtskin_operation operation;
	unsigned int ref_num;
	struct vtskin_coef vtskin_ref[MAX_VTSKIN_REF_NUM];
};

struct vtskin_data {
	struct device *dev;
	struct mutex lock;
	int num_sensor;
	struct vtskin_tz_param *params;
};

struct vtskin_temp_tz {
	unsigned int id;
	struct vtskin_data *skin_data;
	struct vtskin_tz_param *skin_param;
};

extern struct vtskin_data *plat_vtskin_info;

#endif
