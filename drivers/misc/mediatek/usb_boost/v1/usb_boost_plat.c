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
#include "mt_ppm_api.h"
#include "usb_boost.h"

/* platform specific parameter here */
#ifdef CONFIG_ARCH_MT6755
static int cpu_freq_test_para[] = {1, 5, 500, 0};
static int cpu_core_test_para[] = {1, 5, 500, 0};
/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {2000000000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
#elif defined(CONFIG_ARCH_MT6797)
static int cpu_freq_test_para[] = {1, 2, 800, 0};
static int cpu_core_test_para[] = {1, 2, 800, 0};
/* -1 denote not used*/
struct act_arg_obj cpu_freq_test_arg = {1500000000, -1, -1};
struct act_arg_obj cpu_core_test_arg = {4, -1, -1};
#endif

static int freq_hold(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");
	mt_ppm_sysboost_freq(BOOST_BY_USB, arg->arg1);
	return 0;
}
static int freq_release(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");
	mt_ppm_sysboost_freq(BOOST_BY_USB, 0);
	return 0;
}
static int core_hold(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");
	mt_ppm_sysboost_core(BOOST_BY_USB, arg->arg1);
	return 0;
}
static int core_release(struct act_arg_obj *arg)
{
	USB_BOOST_DBG("\n");
	mt_ppm_sysboost_core(BOOST_BY_USB, 0);
	return 0;
}

static int __init init(void)
{

	/* mandatory, related resource inited*/
	usb_boost_init();

	/* optional, change setting for alorithm other than default*/
	usb_boost_set_para_and_arg(TYPE_CPU_FREQ, cpu_freq_test_para,
			sizeof(cpu_freq_test_para)/sizeof(int), &cpu_freq_test_arg);
	usb_boost_set_para_and_arg(TYPE_CPU_CORE, cpu_core_test_para,
			sizeof(cpu_core_test_para)/sizeof(int), &cpu_core_test_arg);

	/* mandatory, hook callback depends on platform */
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_HOLD, freq_hold);
	register_usb_boost_act(TYPE_CPU_FREQ, ACT_RELEASE, freq_release);
	register_usb_boost_act(TYPE_CPU_CORE, ACT_HOLD, core_hold);
	register_usb_boost_act(TYPE_CPU_CORE, ACT_RELEASE, core_release);
	return 0;
}
module_init(init);
