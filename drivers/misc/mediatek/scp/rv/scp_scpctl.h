/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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


