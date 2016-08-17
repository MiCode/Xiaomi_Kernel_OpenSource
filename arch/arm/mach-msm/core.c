/*
 *  Copyright (C) 2011 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include "core.h"

struct arm_soc_desc msm_soc_desc __initdata = {
	.name		= "Qualcomm MSM",
	soc_smp_init_ops(msm_soc_smp_init_ops)
	soc_smp_ops(msm_soc_smp_ops)
};
