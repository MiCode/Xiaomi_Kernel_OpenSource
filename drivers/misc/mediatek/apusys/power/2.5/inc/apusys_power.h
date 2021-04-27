/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APUSYS_POWER_H_
#define _APUSYS_POWER_H_

#include <linux/platform_device.h>
#include "apusys_power_user.h"

struct apu_dev_power_data {
	int dev_type;
	int dev_core;
	void *pdata;
};

/******************************************************
 * for apusys power platform device API
 ******************************************************/
extern int apu_power_device_register(enum DVFS_USER, struct platform_device *pdev);
extern void apu_power_device_unregister(enum DVFS_USER);
extern int apu_device_power_on(enum DVFS_USER);
extern int apu_device_power_off(enum DVFS_USER);
extern int apu_device_power_suspend(enum DVFS_USER user, int suspend);
extern void apu_device_set_opp(enum DVFS_USER user, uint8_t opp);
extern uint64_t apu_get_power_info(int force);
extern bool apu_get_power_on_status(enum DVFS_USER user);
extern int apu_power_callback_device_register(enum POWER_CALLBACK_USER user,
					void (*power_on_callback)(void *para),
					void (*power_off_callback)(void *para));
extern void apu_power_callback_device_unregister(enum POWER_CALLBACK_USER user);
extern uint8_t apusys_boost_value_to_opp
				(enum DVFS_USER user, uint8_t boost_value);
extern ulong apusys_opp_to_freq(enum DVFS_USER user, uint8_t opp);
extern int apusys_freq_to_opp(enum DVFS_USER buck_domain,
							uint32_t freq);
extern int8_t apusys_get_ceiling_opp(enum DVFS_USER user);
extern int8_t apusys_get_opp(enum DVFS_USER user);
extern bool apusys_power_check(void);
extern void apu_qos_set_vcore(int target_volt);
extern int apu_power_cb_register(enum POWER_CALLBACK_USER user,
					void (*power_on_callback)(void *para),
					void (*power_off_callback)(void *para));
extern void apu_power_cb_unregister(enum POWER_CALLBACK_USER user);

extern struct devfreq_governor agov_composite;
extern struct devfreq_governor agov_constrain;
extern struct devfreq_governor agov_userspace;
extern struct devfreq_governor agov_passive;
extern struct devfreq_governor agov_passive_pe;
extern struct platform_driver con_devfreq_driver;
extern struct platform_driver core_devfreq_driver;
extern struct platform_driver vpu_devfreq_driver;
extern struct platform_driver mdla_devfreq_driver;
extern struct platform_driver apu_rpc_driver;
extern struct platform_driver apu_cb_driver;
extern struct platform_driver iommu_devfreq_driver;

extern int fix_opp;
extern int bringup;
#endif
