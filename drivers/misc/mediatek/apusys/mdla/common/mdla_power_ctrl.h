/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_POWER_CTRL_H__
#define __MDLA_POWER_CTRL_H__

#include <linux/platform_device.h>

struct mdla_pwr_ops {
	int (*on)(int core_id, bool force);
	int (*off)(int core_id, int suspend, bool force);
	void (*off_timer_start)(int core_id);
	void (*off_timer_cancel)(int core_id);
	void (*set_opp)(int core_id, int opp);
	void (*set_opp_by_bootst)(int core_id, int bootst_val);
	void (*switch_off_on)(int core_id);
	void (*hw_reset)(int core_id, const char *str);
	void (*lock)(int core_id);/* TODO: remove */
	void (*unlock)(int core_id);/* TODO: remove */
	void (*wake_lock)(int core_id);/* TODO: remove */
	void (*wake_unlock)(int core_id);/* TODO: remove */
};

const struct mdla_pwr_ops *mdla_pwr_ops_get(void);

bool mdla_power_check(void);

int mdla_pwr_device_register(struct platform_device *pdev,
			int (*on)(int core_id, bool force),
			int (*off)(int core_id, int suspend, bool force),
			void (*hw_reset)(int core_id, const char *str));
int mdla_pwr_device_unregister(struct platform_device *pdev);

#endif /* __MDLA_POWER_CTRL_H__ */
