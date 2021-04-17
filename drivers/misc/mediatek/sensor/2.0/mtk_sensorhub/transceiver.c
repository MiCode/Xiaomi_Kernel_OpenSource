// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[transceiver] " fmt

#include <linux/module.h>

#include "ready.h"
#include "sensor_comm.h"
#include "sensor_list.h"

static int __init transceiver_init(void)
{
	int ret = 0;

	ret = sensor_comm_init();
	if (ret < 0) {
		pr_err("Failed sensor comm init, ret=%d\n", ret);
		return ret;
	}
	ret = host_ready_init();
	if (ret < 0) {
		pr_err("Failed host ready init, ret=%d\n", ret);
		return ret;
	}
	return 0;
}

static void __exit transceiver_exit(void)
{
	host_ready_exit();
	sensor_comm_exit();
}

module_init(transceiver_init);
module_exit(transceiver_exit);
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("transceiver driver");
MODULE_LICENSE("GPL");
