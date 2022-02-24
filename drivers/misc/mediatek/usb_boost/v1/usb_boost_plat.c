// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include "mtk_ppm_api.h"
#include "cpu_ctrl.h"
#include "usb_boost.h"
#include <linux/plist.h>
#if defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6771) \
	|| defined(CONFIG_MACH_MT6768) || defined(CONFIG_MACH_MT6781) \
	|| defined(CONFIG_MACH_MT6785) || defined(CONFIG_MACH_MT6833) \
	|| defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6873) \
	|| defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6885) \
	|| defined(CONFIG_MACH_MT6893)
#include <helio-dvfsrc-opp.h>
#else
#include <linux/soc/mediatek/mtk-pm-qos.h>
#endif
#include <linux/pm_qos.h>
#include <linux/topology.h>

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
#elif defined(CONFIG_MACH_MT6761)
static int cpu_freq_test_para[] = {1, 5, 500, 0};
static int cpu_core_test_para[] = {1, 5, 500, 0};
static int dram_vcore_test_para[] = {1, 5, 500, 0};

/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {2500000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
struct act_arg_obj dram_vcore_test_arg = {DDR_OPP_0, -1, -1};
#elif defined(CONFIG_MACH_MT6779)
static int cpu_freq_test_para[] = {1, 5, 500, 0};
static int cpu_core_test_para[] = {1, 5, 500, 0};
static int dram_vcore_test_para[] = {1, 5, 500, 0};

/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {2500000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
struct act_arg_obj dram_vcore_test_arg = {DDR_OPP_0, -1, -1};
#elif defined(CONFIG_MACH_MT6763)
static int cpu_freq_test_para[] = {1, 5, 500, 0};
static int cpu_core_test_para[] = {1, 5, 500, 0};
static int dram_vcore_test_para[] = {1, 5, 500, 0};

/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {2500000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
struct act_arg_obj dram_vcore_test_arg = {DDR_OPP_0, -1, -1};
#elif defined(CONFIG_MACH_MT6771) || defined(CONFIG_MACH_MT6768) \
		|| defined(CONFIG_MACH_MT6781) || defined(CONFIG_MACH_MT6785) \
		|| defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6853) \
		|| defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6877) \
		|| defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893) \

static int cpu_freq_test_para[] = {1, 5, 500, 0};
static int cpu_core_test_para[] = {1, 5, 500, 0};
static int dram_vcore_test_para[] = {1, 5, 500, 0};

/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {2500000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
struct act_arg_obj dram_vcore_test_arg = {DDR_OPP_0, -1, -1};
#elif defined(CONFIG_MACH_MT6739)
static int cpu_freq_test_para[] = {1, 5, 500, 0};
static int cpu_core_test_para[] = {1, 5, 500, 0};
static int dram_vcore_test_para[] = {1, 5, 500, 0};

/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {1500000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
struct act_arg_obj dram_vcore_test_arg = {DDR_OPP_0, -1, -1};
#elif defined(CONFIG_ARCH_MT6XXX)
/* add new here */
#endif

static struct mtk_pm_qos_request pm_qos_req;
static struct mtk_pm_qos_request pm_qos_emi_req;
static struct cpu_ctrl_data *freq_to_set;
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

	mtk_pm_qos_update_request(&pm_qos_req, 50);
	return 0;
}

static int core_release(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");

	/*Enable MCDI*/
	mtk_pm_qos_update_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);
	return 0;
}

static int vcorefs_hold(struct act_arg_obj *arg)
{
	mtk_pm_qos_update_request(&pm_qos_emi_req, DDR_OPP_0);
	return 0;
}

static int vcorefs_release(struct act_arg_obj *arg)
{
	mtk_pm_qos_update_request(&pm_qos_emi_req, DDR_OPP_UNREQ);
	return 0;
}

static int __init usbboost(void)
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

	mtk_pm_qos_add_request(&pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
		PM_QOS_DEFAULT_VALUE);

	mtk_pm_qos_add_request(&pm_qos_emi_req, MTK_PM_QOS_DDR_OPP,
		MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

	/* init freq ppm data */
	cluster_num = arch_nr_clusters();
	USB_BOOST_DBG("cluster_num=%d\n", cluster_num);

	freq_to_set = kcalloc(cluster_num,
				sizeof(struct cpu_ctrl_data), GFP_KERNEL);

	if (!freq_to_set) {
		USB_BOOST_DBG("kcalloc freq_to_set fail\n");
		return -1;
	}

	return 0;
}
module_init(usbboost);

static void __exit clean(void)
{
	kfree(freq_to_set);
}
module_exit(clean);
