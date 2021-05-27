/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_VCPCTL_H__
#define __VCP_VCPCTL_H__

#include <linux/types.h>

enum VCPCTL_TYPE_E {
	VCPCTL_TYPE_TMON,
	VCPCTL_STRESS_TEST,
};

enum VCPCTL_OP_E {
	VCPCTL_OP_INACTIVE = 0,
	VCPCTL_OP_ACTIVE,
};

struct vcpctl_cmd_s {
	uint32_t	type;
	uint32_t	op;
};

extern struct device_attribute dev_attr_vcpctl;

#endif /* __VCP_VCPCTL_H__ */


