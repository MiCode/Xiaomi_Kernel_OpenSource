/*
 * arch/arm/mach-tegra/therm-dram.c
 *
 * Copyright (C) 2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/seq_file.h>
#include <linux/thermal.h>
#include <linux/timer.h>
#include <linux/atomic.h>

#include "tegra_emc.h"

#define TEGRA_DRAM_THERM_MAX_STATE     1

/* In ms - time between taking MR4 samples. */
static unsigned long emc_mr4_sample_interval = 1000;
static atomic_t do_poll;
static int prev_temp = -1;

static struct timer_list emc_mr4_timer;

/*
 * Set to 1 to allow debugfs to control the mr4 read value.
 */
static int test_mode;
static int dram_temp_override;

static void emc_mr4_poll(unsigned long nothing)
{
	int dram_temp;

	if (!test_mode)
		dram_temp = tegra_emc_get_dram_temperature();
	else
		dram_temp = dram_temp_override;

	if (prev_temp == dram_temp)
		goto reset;

	switch (dram_temp) {
	case 0:
	case 1:
	case 2:
	case 3:
		/*
		 * Temp is fine - go back to regular refresh.
		 */
		pr_info("[dram-therm] Setting nominal refresh + timings.\n");
		tegra_emc_set_over_temp_state(DRAM_OVER_TEMP_NONE);
		break;
	case 4:
		pr_info("[dram-therm] Enabling 2x refresh.\n");
		tegra_emc_set_over_temp_state(DRAM_OVER_TEMP_REFRESH_X2);
		break;
	case 5:
		pr_info("[dram-therm] Enabling 4x refresh.\n");
		tegra_emc_set_over_temp_state(DRAM_OVER_TEMP_REFRESH_X4);
		break;
	case 6:
		pr_info("[dram-therm] Enabling 4x refresh + derating.\n");
		tegra_emc_set_over_temp_state(DRAM_OVER_TEMP_THROTTLE);
		break;
	default:
		WARN(1, "%s: Invalid DRAM temp state %d\n",
		     __func__, dram_temp);
		break;
	}
	prev_temp = dram_temp;

reset:
	if (atomic_read(&do_poll) == 0)
		return;

	if (mod_timer(&emc_mr4_timer,
		      jiffies + msecs_to_jiffies(emc_mr4_sample_interval)))
		pr_err("[dram-therm] Failed to restart timer!!!\n");
}

/*
 * Tell the dram thermal driver to start polling for the DRAM temperature. This
 * should be invoked when there is reason to believe the DRAM temperature is
 * high.
 */
static int tegra_dram_temp_start(void)
{
	int err;

	pr_info("[dram-therm] Starting DRAM temperature polling.\n");

	err = mod_timer(&emc_mr4_timer,
			jiffies + msecs_to_jiffies(emc_mr4_sample_interval));
	if (err)
		return err;

	atomic_set(&do_poll, 1);
	return mod_timer(&emc_mr4_timer,
			 jiffies + msecs_to_jiffies(emc_mr4_sample_interval));
}

/*
 * Stop the DRAM thermal driver from polling for the DRAM temperature. If there
 * is no reason to expect the DRAM to be very hot then there is no reason to
 * poll for the DRAM's temperature.
 */
static void tegra_dram_temp_stop(void)
{
	pr_info("[dram-therm] Stopping DRAM temperature polling.\n");
	atomic_set(&do_poll, 0);
}

static int tegra_dram_cd_max_state(struct thermal_cooling_device *tcd,
				   unsigned long *state)
{
	*state = TEGRA_DRAM_THERM_MAX_STATE;
	return 0;
}

static int tegra_dram_cd_cur_state(struct thermal_cooling_device *tcd,
				   unsigned long *state)
{
	*state = (unsigned long)atomic_read(&do_poll);
	return 0;
}

static int tegra_dram_cd_set_state(struct thermal_cooling_device *tcd,
				   unsigned long state)
{
	if (state == (unsigned long)atomic_read(&do_poll))
		return 0;

	if (state)
		tegra_dram_temp_start();
	else
		tegra_dram_temp_stop();
	return 0;
}

/*
 * Cooling device support.
 */
static struct thermal_cooling_device_ops emc_dram_cd_ops = {
	.get_max_state = tegra_dram_cd_max_state,
	.get_cur_state = tegra_dram_cd_cur_state,
	.set_cur_state = tegra_dram_cd_set_state,
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *dram_therm_debugfs;

static int __get_sample_interval(void *data, u64 *val)
{
	*val = emc_mr4_sample_interval;
	return 0;
}
static int __set_sample_interval(void *data, u64 val)
{
	emc_mr4_sample_interval = (unsigned long) val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(sample_interval_fops, __get_sample_interval,
			__set_sample_interval, "%llu\n");

static int __get_test_mode(void *data, u64 *val)
{
	*val = test_mode;
	return 0;
}
static int __set_test_mode(void *data, u64 val)
{
	test_mode = !!val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(test_mode_fops, __get_test_mode,
			__set_test_mode, "%llu\n");

static int __get_dram_temp_override(void *data, u64 *val)
{
	*val = dram_temp_override;
	return 0;
}
static int __set_dram_temp_override(void *data, u64 val)
{
	dram_temp_override = (unsigned int) val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(dram_temp_override_fops, __get_dram_temp_override,
			__set_dram_temp_override, "%llu\n");

static int __get_do_poll(void *data, u64 *val)
{
	*val = atomic_read(&do_poll);

	return 0;
}
static int __set_do_poll(void *data, u64 val)
{
	atomic_set(&do_poll, (unsigned int)val);

	/* Explicitly wake up the DRAM monitoring thread. */
	if (atomic_read(&do_poll))
		tegra_dram_temp_start();

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(do_poll_fops, __get_do_poll, __set_do_poll, "%llu\n");
#endif

static int __init tegra_dram_therm_init(void)
{
	void *ret;

	ret = thermal_cooling_device_register("tegra-dram", NULL,
					      &emc_dram_cd_ops);
	if (IS_ERR(ret))
		return PTR_ERR(ret);
	if (ret == NULL)
		return -ENODEV;

	setup_timer(&emc_mr4_timer, emc_mr4_poll, 0);

#ifdef CONFIG_DEBUG_FS
	dram_therm_debugfs = debugfs_create_dir("dram-therm", NULL);
	if (!dram_therm_debugfs)
		return -ENOMEM;

	if (!debugfs_create_file("sample_interval", S_IRUGO | S_IWUSR,
				 dram_therm_debugfs, NULL,
				 &sample_interval_fops))
		return -ENOMEM;
	if (!debugfs_create_file("test_mode", S_IRUGO | S_IWUSR,
				 dram_therm_debugfs, NULL, &test_mode_fops))
		return -ENOMEM;
	if (!debugfs_create_file("dram_temp_override", S_IRUGO | S_IWUSR,
				 dram_therm_debugfs, NULL,
				 &dram_temp_override_fops))
		return -ENOMEM;
	if (!debugfs_create_file("do_poll", S_IRUGO | S_IWUSR,
				 dram_therm_debugfs, NULL, &do_poll_fops))
		return -ENOMEM;
#endif

	return 0;
}
late_initcall(tegra_dram_therm_init);
