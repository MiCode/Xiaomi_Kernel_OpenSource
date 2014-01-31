/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
 *
 * RMNET Data generic framework
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include "rmnet_data_private.h"
#include "rmnet_data_config.h"
#include "rmnet_data_vnd.h"

/* ***************** Trace Points ******************************************* */
#define CREATE_TRACE_POINTS
#include "rmnet_data_trace.h"

/* ***************** Module Parameters ************************************** */
unsigned int rmnet_data_log_level = RMNET_LOG_LVL_ERR | RMNET_LOG_LVL_HI;
module_param(rmnet_data_log_level, uint,  S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(log_level, "Logging level");

unsigned int rmnet_data_log_module_mask;
module_param(rmnet_data_log_module_mask, uint,  S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rmnet_data_log_module_mask, "Logging module mask");

/* ***************** Startup/Shutdown *************************************** */

/**
 * rmnet_init() - Module initialization
 *
 * todo: check for (and init) startup errors
 */
static int __init rmnet_init(void)
{
	rmnet_config_init();
	rmnet_vnd_init();

	LOGL("%s", "RMNET Data driver loaded successfully");
	return 0;
}

static void __exit rmnet_exit(void)
{
	rmnet_config_exit();
	rmnet_vnd_exit();
}

module_init(rmnet_init)
module_exit(rmnet_exit)
MODULE_LICENSE("GPL v2");
