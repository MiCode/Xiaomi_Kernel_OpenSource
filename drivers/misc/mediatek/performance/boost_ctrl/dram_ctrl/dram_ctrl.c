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
/* config: MTK_QOS_SUPPORT or VCORE_DVFS_OPP_SUPPORT */
/* define in Makefile */
/*#endif # PM_DEVFREQ*/

#if defined(MTK_QOS_SUPPORT)
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#endif

#if defined(CONFIG_MTK_DRAMC)
#include "mtk_dramc.h"
#endif

#if defined(VCORE_DVFS_OPP_SUPPORT)
#include <mtk_vcorefs_governor.h>
#include <mtk_vcorefs_manager.h>
#endif

#include "mtk_perfmgr_internal.h"
#include "boost_ctrl.h"

#ifdef CONFIG_MEDIATEK_DRAMC
#include <dramc.h>
#endif

static int ddr_type;

#ifdef MTK_QOS_SUPPORT
/* QoS Method */
static int ddr_now;
static int ddr_lp5_now;
static int ddr_lp5_hfr_now;
static struct pm_qos_request emi_request;
static struct pm_qos_request emi_request_lp5;
static struct pm_qos_request emi_request_lp5_hfr;
static int emi_opp;
static int dfps;
static int hfr_fps;
#endif

#ifdef VCORE_DVFS_OPP_SUPPORT
static int vcore_now;
#endif

static int perfmgr_dram_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "DDR_TYPE: %d\n", ddr_type);
	return 0;
}

void dram_ctl_update_dfrc_fps(int fps)
{
#ifdef MTK_QOS_SUPPORT
	if (fps == dfps || fps <= 0)
		return;

	dfps = fps;

#ifdef CONFIG_MEDIATEK_DRAMC
	if (ddr_lp5_now == -1 && ddr_lp5_hfr_now == -1)
		return;

	if (mtk_dramc_get_ddr_type() != TYPE_LPDDR5)
		return;

	if (ddr_lp5_now != -1 && dfps <= hfr_fps)
		pm_qos_update_request(&emi_request_lp5, ddr_lp5_now);
	else if (ddr_lp5_hfr_now != -1 && dfps > hfr_fps)
		pm_qos_update_request(&emi_request_lp5_hfr, ddr_lp5_hfr_now);
	else if (ddr_lp5_now != -1)
		pm_qos_update_request(&emi_request_lp5, -1);
	else
		pm_qos_update_request(&emi_request_lp5_hfr, -1);
#endif

#endif
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

static ssize_t perfmgr_ddr_lp5_proc_write(struct file *filp, const char *ubuf,
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

	ddr_lp5_now = val;

#ifdef CONFIG_MEDIATEK_DRAMC
	if (mtk_dramc_get_ddr_type() == TYPE_LPDDR5
		&& dfps <= hfr_fps)
		pm_qos_update_request(&emi_request_lp5, val);
#endif

	return cnt;
}

static int perfmgr_ddr_lp5_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", ddr_lp5_now);
	return 0;
}
PROC_FOPS_RW(ddr_lp5);

static ssize_t perfmgr_ddr_lp5_hfr_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
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

	ddr_lp5_hfr_now = val;

#ifdef CONFIG_MEDIATEK_DRAMC
	if (mtk_dramc_get_ddr_type() == TYPE_LPDDR5
		&& dfps > hfr_fps)
		pm_qos_update_request(&emi_request_lp5_hfr, val);
#endif

	return cnt;
}

static int perfmgr_ddr_lp5_hfr_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", ddr_lp5_hfr_now);
	return 0;
}
PROC_FOPS_RW(ddr_lp5_hfr);

#endif

#ifdef VCORE_DVFS_OPP_SUPPORT
static ssize_t perfmgr_vcore_proc_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	int val;
	int ret;

	if (cnt >= sizeof(buf)) {
		pr_debug("vcore_write cnt >= sizeof\n");
		return -EINVAL;
	}
	if (copy_from_user(buf, ubuf, cnt)) {
		pr_debug("vcore_write copy_from_user\n");
		return -EFAULT;
	}
	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0) {
		pr_debug("vcore_write ret < 0\n");
		return ret;
	}
	if (val < -1 || val > 4) {
		pr_debug("UNREQ\n");
		return -1;
	}

	ret = vcorefs_request_dvfs_opp(KIR_PERF, val);

	if (ret == 0)
		vcore_now = val;

	return cnt;
}

static int perfmgr_vcore_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", vcore_now);
	return 0;
}
PROC_FOPS_RW(vcore);
#endif


PROC_FOPS_RO(dram);
/*--------------------INIT------------------------*/

int dram_ctrl_init(struct proc_dir_entry *parent)
{
	int ret = 0;
	size_t i;
#ifdef MTK_QOS_SUPPORT
	char owner[20] = "dram_ctrl_lp5";
	char owner_hfr[20] = "dram_ctrl_lp5_hfr";
#endif
	struct proc_dir_entry *drams_dir;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(dram),
#ifdef MTK_QOS_SUPPORT
		PROC_ENTRY(ddr),
		PROC_ENTRY(ddr_lp5),
		PROC_ENTRY(ddr_lp5_hfr),
#endif
#ifdef VCORE_DVFS_OPP_SUPPORT
		PROC_ENTRY(vcore),
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
	hfr_fps = 66;
	ddr_now = -1;
	ddr_lp5_now = -1;
	ddr_lp5_hfr_now = -1;
	if (!pm_qos_request_active(&emi_request) ||
		!pm_qos_request_active(&emi_request_lp5) ||
		!pm_qos_request_active(&emi_request_lp5_hfr)) {

		pr_debug("hh: emi pm_qos_add_request\n");
#if defined(MTK_QOS_EMI_OPP)
		pm_qos_add_request(&emi_request, PM_QOS_EMI_OPP,
				PM_QOS_EMI_OPP_DEFAULT_VALUE);
		pm_qos_add_request(&emi_request_lp5, PM_QOS_EMI_OPP,
				PM_QOS_EMI_OPP_DEFAULT_VALUE);
		strncpy(emi_request_lp5.owner, owner,
			sizeof(emi_request_lp5.owner) - 1);
		pm_qos_add_request(&emi_request_lp5_hfr, PM_QOS_EMI_OPP,
				PM_QOS_EMI_OPP_DEFAULT_VALUE);
		strncpy(emi_request_lp5_hfr.owner, owner_hfr,
			sizeof(emi_request_lp5_hfr.owner) - 1);
#else
		pm_qos_add_request(&emi_request, PM_QOS_DDR_OPP,
				PM_QOS_DDR_OPP_DEFAULT_VALUE);
		pm_qos_add_request(&emi_request_lp5, PM_QOS_DDR_OPP,
				PM_QOS_DDR_OPP_DEFAULT_VALUE);
		strncpy(emi_request_lp5.owner, owner,
			sizeof(emi_request_lp5.owner) - 1);
		pm_qos_add_request(&emi_request_lp5_hfr, PM_QOS_DDR_OPP,
				PM_QOS_DDR_OPP_DEFAULT_VALUE);
		strncpy(emi_request_lp5_hfr.owner, owner_hfr,
			sizeof(emi_request_lp5_hfr.owner) - 1);
#endif
	} else {
		pr_debug("hh: emi pm_qos already request\n");
	}
	emi_opp = DDR_OPP_NUM - 1;
#endif

#ifdef VCORE_DVFS_OPP_SUPPORT
	vcore_now = -1;
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

