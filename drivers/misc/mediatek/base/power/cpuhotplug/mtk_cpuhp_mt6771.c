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

#define pr_fmt(fmt) "cpuhp: " fmt

#include <linux/cpu.h>
#include <linux/regulator/consumer.h>

/* TODO: wait for FH ready #include <mt_freqhopping.h> */
#define mt_pause_armpll(X, Y) /* remove when FH driver is ready */

#include "mtk_cpuhp_private.h"

/* cluster1's buck */
static struct regulator *regulator_vproc11;
static struct regulator *regulator_vsram11;

static void prepare_poweron_cluster0(void)
{
	/*1. Turn on ARM PLL*/
	armpll_control(1, 1);

	/*2. Non-pause FQHP function*/
	mt_pause_armpll(FH_PLL0, 0);

	/*3. Switch to HW mode*/
	mp_enter_suspend(0, 1);
}

static void finish_poweroff_cluster0(int action)
{
	/*1. Switch to SW mode*/
	mp_enter_suspend(0, 0);

	/*2. Pause FQHP function*/
	if (action == CPU_DEAD_FROZEN)
		mt_pause_armpll(FH_PLL1, 0x11);
	else
		mt_pause_armpll(FH_PLL1, 0x01);

	/*3. Turn off ARM PLL*/
	armpll_control(1, 0);
}

static void prepare_poweron_cluster1(void)
{
	int rc;

	rc = regulator_enable(regulator_vproc11);
	if (rc) {
		pr_notice("failed to enable regulator: vproc11\n");
		return;
	}

	rc = regulator_enable(regulator_vsram11);
	if (rc) {
		pr_notice("failed to enable regulator: vsram11\n");
		return;
	}

	/*4. Turn on ARM PLL*/
	armpll_control(2, 1);

	/*5. Non-pause FQHP function*/
	mt_pause_armpll(FH_PLL2, 0);

	/*6. Switch to HW mode*/
	mp_enter_suspend(1, 1);
}

static void finish_poweroff_cluster1(int action)
{
	/*1. Switch to SW mode*/
	mp_enter_suspend(1, 0);

	/*2. Pause FQHP function*/
	if (action == CPU_DEAD_FROZEN)
		mt_pause_armpll(FH_PLL2, 0x11);
	else
		mt_pause_armpll(FH_PLL2, 0x01);

	/*3. Turn off ARM PLL*/
	armpll_control(2, 0);

	regulator_disable(regulator_vproc11);
	regulator_disable(regulator_vsram11);
}

int cpuhp_platform_cpuon(int cluster, int cpu, int isalone, int action)
{
	/* before powering on CPU, enable the regulator and its clock first */
	if (cluster == 0 && isalone) {
		prepare_poweron_cluster0();
		return 0;
	}

	if (cluster && isalone)
		prepare_poweron_cluster1();

	return 0;
}

int cpuhp_platform_cpuoff(int cluster, int cpu, int isalone, int action)
{
	poweroff_cpu(cpu);

	if (isalone)
		poweroff_cluster(cluster);

	/*
	 * after powering down cluster,
	 * shutdown the regulator and its clock
	 */
	if (cluster == 0 && isalone) {
		finish_poweroff_cluster0(action);
		return 0;
	}

	if (cluster && isalone)
		finish_poweroff_cluster1(action);

	return 0;
}


int cpuhp_platform_init(void)
{
	struct device *cpu_dev;
	int rc;

	cpu_dev = get_cpu_device(0);
	if (!cpu_dev) {
		pr_notice("failed to get cpu0 device\n");
		return -ENODEV;
	}

	regulator_vproc11 = regulator_get(cpu_dev, "vproc11");
	if (IS_ERR(regulator_vproc11)) {
		pr_notice("failed to get regulator: vproc11\n");
		goto vproc11_fail;
	}

	regulator_vsram11 = regulator_get(cpu_dev, "vsram_proc11");
	if (IS_ERR(regulator_vsram11)) {
		pr_notice("failed to get regulator: vsram_proc11\n");
		goto vsram11_fail;
	}

	rc = regulator_enable(regulator_vproc11);
	if (rc) {
		pr_notice("failed to enable regulator: vproc11\n");
		goto enable_fail;
	}

	rc = regulator_enable(regulator_vsram11);
	if (rc) {
		pr_notice("failed to enable regulator: vsram11\n");
		goto enable_fail;
	}

	mp_enter_suspend(0, 1);/*Switch LL cluster to HW mode*/
	mp_enter_suspend(1, 1);/*Switch L cluster to HW mode*/
	return 0;

enable_fail:
	regulator_put(regulator_vsram11);
vsram11_fail:
	regulator_put(regulator_vproc11);
vproc11_fail:

	return -ENOENT;

}
