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
		goto out_sensor_comm;
	}
	ret = sensor_list_init();
	if (ret < 0) {
		pr_err("Failed sensor list init, ret=%d\n", ret);
		goto out_ready;
	}
	return 0;
out_ready:
	host_ready_exit();
out_sensor_comm:
	sensor_comm_exit();
	return ret;
}

static void __exit transceiver_exit(void)
{
	host_ready_exit();
	sensor_comm_exit();
	sensor_list_exit();
}

module_init(transceiver_init);
module_exit(transceiver_exit);
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("transceiver driver");
MODULE_LICENSE("GPL");
