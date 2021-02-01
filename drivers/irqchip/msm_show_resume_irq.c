/* Copyright (c) 2011, 2014-2016 The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

/*Modify-begin
**JIRA-id:HMI_M516_A01-51
**Author:lijiang@longcheer.com
**Date:2019-5-13
**Comment:change msm_show_resume_irq_mask 0 to 1
*/
int msm_show_resume_irq_mask=1;
/*Modify-end*/

module_param_named(
	debug_mask, msm_show_resume_irq_mask, int, 0664);
