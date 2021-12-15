// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "mdla.h"
#include <linux/io.h>

static const char *reason_str[REASON_MAX+1] = {
	"others",
	"driver_init",
	"command_timeout",
	"power_on",
	"preemption",
	"-"
};

const char *mdla_get_reason_str(int res)
{
	if ((res < 0) || (res > REASON_MAX))
		res = REASON_MAX;

	return reason_str[res];
}

unsigned int mdla_reg_read_with_mdlaid(u32 mdlaid, u32 offset)
{
	return ioread32(mdla_reg_control[mdlaid].apu_mdla_cmde_mreg_top
		+ offset);
}

unsigned int mdla_cfg_read_with_mdlaid(u32 mdlaid, u32 offset)
{
	return ioread32(mdla_reg_control[mdlaid].apu_mdla_config_top + offset);
}
