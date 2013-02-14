/* Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Qualcomm PMIC Multi-Purpose Pin Configurations */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/module.h>

#include <mach/mpp.h>
#include <mach/proc_comm.h>

int mpp_config_digital_out(unsigned mpp, unsigned config)
{
	int err;
	err = msm_proc_comm(PCOM_PM_MPP_CONFIG, &mpp, &config);
	if (err)
		pr_err("%s: msm_proc_comm(PCOM_PM_MPP_CONFIG) failed\n",
		       __func__);
	return err;
}
EXPORT_SYMBOL(mpp_config_digital_out);

int mpp_config_digital_in(unsigned mpp, unsigned config)
{
	int err;
	err = msm_proc_comm(PCOM_PM_MPP_CONFIG_DIGITAL_INPUT, &mpp, &config);
	if (err)
		pr_err("%s: msm_proc_comm(PCOM_PM_MPP_CONFIG) failed\n",
		       __func__);
	return err;
}
EXPORT_SYMBOL(mpp_config_digital_in);

#if defined(CONFIG_DEBUG_FS)
static int test_result;

static int mpp_debug_set(void *data, u64 val)
{
	unsigned mpp = (unsigned) data;

	test_result = mpp_config_digital_out(mpp, (unsigned)val);
	if (test_result) {
		printk(KERN_ERR
			   "%s: mpp_config_digital_out \
			   [mpp(%d) = 0x%x] failed (err=%d)\n",
			   __func__, mpp, (unsigned)val, test_result);
	}
	return 0;
}

static int mpp_debug_get(void *data, u64 *val)
{
	if (!test_result)
		*val = 0;
	else
		*val = 1;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mpp_fops, mpp_debug_get, mpp_debug_set, "%llu\n");

static int __init mpp_debug_init(void)
{
	struct dentry *dent;
	int n;
	char	file_name[16];

	dent = debugfs_create_dir("mpp", 0);
	if (IS_ERR(dent))
		return 0;

	for (n = 0; n < MPPS; n++) {
		snprintf(file_name, sizeof(file_name), "mpp%d", n + 1);
		debugfs_create_file(file_name, 0644, dent,
				    (void *)n, &mpp_fops);
	}

	return 0;
}

device_initcall(mpp_debug_init);
#endif

