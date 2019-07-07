/*
 * Copyright (C) 2018 MediaTek Inc.
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
