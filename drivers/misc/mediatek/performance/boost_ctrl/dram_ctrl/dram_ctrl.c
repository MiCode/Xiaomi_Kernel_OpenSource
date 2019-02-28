/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "[dram_ctrl]"fmt
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/string.h>

/*if PM_DEVFREQ*/
#define MTK_QOS_SUPPORT
/*#endif # PM_DEVFREQ*/

#if defined(MTK_QOS_SUPPORT)
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#endif

#if defined(CONFIG_MTK_DRAMC)
#include "mtk_dramc.h"
#endif

#include "mtk_perfmgr_internal.h"
#include "boost_ctrl.h"
static int ddr_type;

#ifdef MTK_QOS_SUPPORT
/* QoS Method */
static int ddr_now;
static struct pm_qos_request emi_request;
static int emi_opp;
#endif


static int perfmgr_dram_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "DDR_TYPE: %d\n", ddr_type);
	return 0;
}
#ifdef MTK_QOS_SUPPORT
static ssize_t perfmgr_ddr_proc_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	int val;
	int ret;

	if (cnt >= sizeof(buf)) {
		pr_debug("ddr_write cnt >= sizeof\n");
		return -EINVAL;
	}
	if (copy_from_user(buf, ubuf, cnt)) {
		pr_debug("ddr_write copy_from_user\n");
		return -EFAULT;
	}
	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0) {
		pr_debug("ddr_write ret < 0\n");
		return ret;
	}
	if (val < -1 || val > emi_opp) {
		pr_debug("UNREQ\n");
		return -1;
	}

	pm_qos_update_request(&emi_request, val);

	ddr_now = val;

	return cnt;
}

static int perfmgr_ddr_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", ddr_now);
	return 0;
}
PROC_FOPS_RW(ddr);
#endif
PROC_FOPS_RO(dram);
/*--------------------INIT------------------------*/

int dram_ctrl_init(struct proc_dir_entry *parent)
{
	int i, ret = 0;
	struct proc_dir_entry *drams_dir;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(dram),
#ifdef MTK_QOS_SUPPORT
		PROC_ENTRY(ddr),
#endif
	};

	pr_debug("init dram driver start\n");
	drams_dir = proc_mkdir("dram_ctrl", parent);
	if (!drams_dir) {
		pr_debug("drams_dir not create success\n");
		return -ENOMEM;
	}

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
					drams_dir, entries[i].fops)) {
			pr_debug("%s(), create /dram_ctrl%s failed\n",
					__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

#ifdef MTK_QOS_SUPPORT
	/* QoS Method */
	ddr_now = -1;
	if (!pm_qos_request_active(&emi_request)) {
		pr_debug("hh: emi pm_qos_add_request\n");
		pm_qos_add_request(&emi_request, PM_QOS_DDR_OPP,
				PM_QOS_DDR_OPP_DEFAULT_VALUE);
	} else {
		pr_debug("hh: emi pm_qos already request\n");
	}
	emi_opp = DDR_OPP_NUM - 1;
#endif


#if defined(CONFIG_MTK_DRAMC)
	ddr_type = get_ddr_type();
#else
	ddr_type = -1;
#endif
	pr_debug("init dram driver done\n");
out:
	return ret;
}
