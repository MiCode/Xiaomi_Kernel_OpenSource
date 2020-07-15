/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_POWER_CTRL_H__
#define __MDLA_POWER_CTRL_H__

#include <linux/types.h>
#include <linux/platform_device.h>

#define DBGFS_PWR_NAME      "pwr_dbg"

struct mdla_pwr_ops {
	int (*on)(u32 core_id, bool force);
	int (*off)(u32 core_id, int suspend, bool force);
	void (*off_timer_start)(u32 core_id);
	void (*off_timer_cancel)(u32 core_id);
	void (*set_opp)(u32 core_id, int opp);
	void (*set_opp_by_bootst)(u32 core_id, int bootst_val);
	void (*switch_off_on)(u32 core_id);
	void (*hw_reset)(u32 core_id, const char *str);
	void (*lock)(u32 core_id);
	void (*unlock)(u32 core_id);
	void (*wake_lock)(u32 core_id);
	void (*wake_unlock)(u32 core_id);
};

const struct mdla_pwr_ops *mdla_pwr_ops_get(void);

void mdla_power_set_random_opp(u32 core_id);
bool mdla_power_check(void);

void mdla_pwr_reset_setup(void (*hw_reset)(u32 core_id, const char *str));
int mdla_pwr_device_register(struct platform_device *pdev,
			int (*on)(u32 core_id, bool force),
			int (*off)(u32 core_id, int suspend, bool force));
int mdla_pwr_device_unregister(struct platform_device *pdev);

#endif /* __MDLA_POWER_CTRL_H__ */
