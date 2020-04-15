// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011, 2014-2016, 2018, 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

int msm_show_resume_irq_mask;

module_param_named(
	debug_mask, msm_show_resume_irq_mask, int, 0664);
