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

#ifndef __SCP_SCPCTL_H__
#define __SCP_SCPCTL_H__

#include <linux/types.h>

enum SCPCTL_TYPE_E {
	SCPCTL_TYPE_TMON,
	SCPCTL_STRESS_TEST,
};

enum SCPCTL_OP_E {
	SCPCTL_OP_INACTIVE = 0,
	SCPCTL_OP_ACTIVE,
};

struct scpctl_cmd_s {
	uint32_t	type;
	uint32_t	op;
};

extern struct device_attribute dev_attr_scpctl;

#endif /* __SCP_SCPCTL_H__ */


