/*
 * Intel SOC North Complex devices state Driver
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/suspend.h>

#include <asm/cpu_device_id.h>
#include <asm/intel_mid_pcihelpers.h>

#define	DRIVER_NAME	KBUILD_MODNAME

#define PUNIT_PORT		0x04
#define PWRGT_CNT		0x60
#define PWRGT_STATUS		0x61
#define VED_SS_PM0		0x32
#define ISP_SS_PM0		0x39
#define MIO_SS_PM		0x3B
#define SSS_SHIFT		24
#define RENDER_POS		0
#define MEDIA_POS		2
#define DISPLAY_POS		6
#define DSP_SSS_CHT		0x36
#define DSP_SSS_POS_CHT		16

#define PMC_D0I3_MASK		3

static char *dstates[] = {"D0", "D0i1", "D0i2", "D0i3"};

struct nc_device {
	char *name;
	int reg;
	int sss_pos;
};

struct nc_device nc_devices_vlv[] = {
	{ "GFX RENDER", PWRGT_STATUS,  RENDER_POS },
	{ "GFX MEDIA", PWRGT_STATUS, MEDIA_POS },
	{ "DISPLAY", PWRGT_STATUS,  DISPLAY_POS },
	{ "VED", VED_SS_PM0, SSS_SHIFT},
	{ "ISP", ISP_SS_PM0, SSS_SHIFT},
};

struct nc_device nc_devices_cht[] = {
	{ "GFX RENDER", PWRGT_STATUS,  RENDER_POS },
	{ "GFX MEDIA", PWRGT_STATUS, MEDIA_POS },
	{ "DSP", DSP_SSS_CHT,  DSP_SSS_POS_CHT },
	{ "VED", VED_SS_PM0, SSS_SHIFT},
	{ "ISP", ISP_SS_PM0, SSS_SHIFT},
};
#define NC_NUM_DEVICES 5

#define ICPU(model, cpu) \
	{ X86_VENDOR_INTEL, 6, model, X86_FEATURE_MWAIT, (unsigned long)&cpu }


MODULE_DEVICE_TABLE(x86cpu, intel_pmc_cpu_ids);

struct pmc_config {
	struct nc_device *nc_table;
	u32 num_nc;
};
static const struct pmc_config vlv_config = {
	.nc_table = nc_devices_vlv,
	.num_nc = NC_NUM_DEVICES,
};

static const struct pmc_config cht_config = {
	.nc_table = nc_devices_cht,
	.num_nc = NC_NUM_DEVICES,
};

static const struct pmc_config *icpu;

static const struct x86_cpu_id intel_pmc_cpu_ids[] = {
	ICPU(0x4c, cht_config),
	ICPU(0x37, vlv_config),
	{}
};

static void nc_dev_state_list(void *seq_file)
{
	struct seq_file *s = (struct seq_file *)seq_file;
	u32 nc_pwr_sts, nc_reg, nc_val;
	const struct x86_cpu_id *id;
	int dev_num, dev_index;
	static struct nc_device *nc_dev;

	id = x86_match_cpu(intel_pmc_cpu_ids);
	if (!id)
		return;
	icpu = (const struct pmc_config *)id->driver_data;

	if (!icpu->nc_table)
		return;
	nc_dev = icpu->nc_table;
	dev_num = icpu->num_nc;

	if (s)
		seq_puts(s, "\n\nNORTH COMPLEX DEVICES :\n");
	for (dev_index = 0; dev_index < dev_num; dev_index++) {
		nc_reg = nc_dev[dev_index].reg;
		nc_pwr_sts = intel_mid_msgbus_read32(PUNIT_PORT, nc_reg);
		nc_pwr_sts >>= nc_dev[dev_index].sss_pos;
		nc_val = nc_pwr_sts & PMC_D0I3_MASK;

		if ((pm_suspend_debug) && (s == NULL)) {
			if (!nc_val)
				pr_info("%s in NC is in D0 prior to suspend\n",
				nc_dev[dev_index].name);
		} else
			seq_printf(s, "%9s : %s\n", nc_dev[dev_index].name,
				dstates[nc_val]);
	}
	return;
}

static int nc_dev_state_show(struct seq_file *s, void *unused)
{
	nc_dev_state_list(s);
	return 0;
}

static int nc_dev_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, nc_dev_state_show, inode->i_private);
}

static const struct file_operations nc_dev_state_ops = {
	.open		= nc_dev_state_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *dbg_file;

static int nc_dbgfs_register(void)
{

	dbg_file = debugfs_create_dir("nc_atom", NULL);
	if (!dbg_file)
		return -ENOMEM;

	dbg_file = debugfs_create_file("nc_dev_state", S_IFREG | S_IRUGO,
				dbg_file, NULL, &nc_dev_state_ops);
	if (!dbg_file) {
		pr_err("nc_dev_state register failed\n");
		return -ENODEV;
	}
	return 0;
}

static void nc_dbgfs_unregister(void)
{
	if (!dbg_file)
		return;

	debugfs_remove_recursive(dbg_file);
	dbg_file = NULL;
}

static int __init nc_dev_state_module_init(void)
{
#ifdef CONFIG_DEBUG_FS
	int ret = nc_dbgfs_register();
	if (ret < 0)
		return ret;
	nc_dev_state = nc_dev_state_list;
#endif /* CONFIG_DEBUG_FS */
	return 0;
}

static void __exit nc_dev_state_module_exit(void)
{
	nc_dbgfs_unregister();
}

module_init(nc_dev_state_module_init);
module_exit(nc_dev_state_module_exit);

MODULE_AUTHOR("Kumar P, Mahesh <mahesh.kumar.p.intel.com>");
MODULE_DESCRIPTION("Driver for get North Complex devices state");
MODULE_LICENSE("GPL v2");
