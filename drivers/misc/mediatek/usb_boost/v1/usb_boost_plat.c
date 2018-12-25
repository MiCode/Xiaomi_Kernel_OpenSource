/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include "mtk_ppm_api.h"
#include "cpu_ctrl.h"
#include "usb_boost.h"
#include <mtk_vcorefs_manager.h>
#include <helio-dvfsrc-opp.h>

/* platform specific parameter here */
#if defined(CONFIG_MACH_MT6758)
static int cpu_freq_test_para[] = {1, 5, 500, 0};
static int cpu_core_test_para[] = {1, 5, 500, 0};
static int dram_vcore_test_para[] = {1, 5, 500, 0};

/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {2500000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
struct act_arg_obj dram_vcore_test_arg = {DDR_OPP_0, -1, -1};
#elif defined(CONFIG_MACH_MT6765)
static int cpu_freq_test_para[] = {1, 5, 500, 0};
static int cpu_core_test_para[] = {1, 5, 500, 0};
static int dram_vcore_test_para[] = {1, 5, 500, 0};

/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {2500000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
struct act_arg_obj dram_vcore_test_arg = {DDR_OPP_0, -1, -1};
#elif defined(CONFIG_ARCH_MT6XXX)
/* add new here */
#endif

static struct pm_qos_request pm_qos_req;
static struct pm_qos_request pm_qos_emi_req;
static struct ppm_limit_data *freq_to_set;
static int cluster_num;

static int freq_hold(struct act_arg_obj *arg)
{
	int i;

	USB_BOOST_DBG("\n");

	for (i = 0; i < cluster_num; i++) {
		freq_to_set[i].min = arg->arg1;
		freq_to_set[i].max = -1;
	}

	update_userlimit_cpu_freq(CPU_KIR_USB, cluster_num, freq_to_set);
	return 0;
}

static int freq_release(struct act_arg_obj *arg)
{
	int i;

	USB_BOOST_DBG("\n");

	for (i = 0; i < cluster_num; i++) {
		freq_to_set[i].min = -1;
		freq_to_set[i].max = -1;
	}

	update_userlimit_cpu_freq(CPU_KIR_USB, cluster_num, freq_to_set);
	return 0;
}

static int core_hold(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");

	/*Disable MCDI to save around 100us
	 *"Power ON CPU -> CPU context restore"
	 */

	pm_qos_update_request(&pm_qos_req, 50);
	return 0;
}

static int core_release(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");

	/*Enable MCDI*/
	pm_qos_update_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);
	return 0;
}

static int vcorefs_hold(struct act_arg_obj *arg)
{
	pm_qos_update_request(&pm_qos_emi_req, DDR_OPP_0);
	return 0;
}

static int vcorefs_release(struct act_arg_obj *arg)
{
	pm_qos_update_request(&pm_qos_emi_req, DDR_OPP_UNREQ);
	return 0;
}

static int __init init(void)
{
	/* mandatory, related resource inited*/
	usb_boost_init();

	/* optional, change setting for alorithm other than default*/
	usb_boost_set_para_and_arg(TYPE_CPU_FREQ, cpu_freq_test_para,
			ARRAY_SIZE(cpu_freq_test_para), &cpu_freq_test_arg);
	usb_boost_set_para_and_arg(TYPE_CPU_CORE, cpu_core_test_para,
			ARRAY_SIZE(cpu_core_test_para), &cpu_core_test_arg);
	usb_boost_set_para_and_arg(TYPE_DRAM_VCORE, dram_vcore_test_para,
			ARRAY_SIZE(dram_vcore_test_para), &dram_vcore_test_arg);

	/* mandatory, hook callback depends on platform */
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_HOLD, freq_hold);
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_RELEASE, freq_release);
	register_usb_boost_act(TYPE_CPU_CORE, ACT_HOLD, core_hold);
	register_usb_boost_act(TYPE_CPU_CORE, ACT_RELEASE, core_release);
	register_usb_boost_act(TYPE_DRAM_VCORE, ACT_HOLD, vcorefs_hold);
	register_usb_boost_act(TYPE_DRAM_VCORE, ACT_RELEASE, vcorefs_release);

	pm_qos_add_request(&pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
		PM_QOS_DEFAULT_VALUE);

	pm_qos_add_request(&pm_qos_emi_req, PM_QOS_DDR_OPP,
		PM_QOS_DDR_OPP_DEFAULT_VALUE);

	/* init freq ppm data */
	cluster_num = arch_get_nr_clusters();

	freq_to_set = kcalloc(cluster_num,
				sizeof(struct ppm_limit_data), GFP_KERNEL);

	if (!freq_to_set) {
		USB_BOOST_DBG("kcalloc freq_to_set fail\n");
		return -1;
	}

	return 0;
}
module_init(init);

static void __exit clean(void)
{
	kfree(freq_to_set);
}
module_exit(clean);
