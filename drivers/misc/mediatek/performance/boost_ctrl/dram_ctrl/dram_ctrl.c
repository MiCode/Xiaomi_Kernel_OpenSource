// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>

#ifdef MTK_K14_DRM_BOOST
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
char qos_has_dts_support;
#ifdef MTK_QOS_SUPPORT
/* QoS Method */
static int ddr_now;
static int ddr_lp5_now;
static int ddr_lp5_hfr_now;
static struct mtk_pm_qos_request emi_request;
static struct mtk_pm_qos_request emi_request_lp5;
static struct mtk_pm_qos_request emi_request_lp5_hfr;
static int emi_opp;
static int dfps;
static int hfr_fps;
#endif

#ifdef VCORE_DVFS_OPP_SUPPORT
static int vcore_now;
#endif
#endif

struct mtk_dram_ctrl {
	struct device *dev;
	struct proc_dir_entry *proc_dir;
	int num_perf;
	int *perfs;
	int curr_opp;
	int qos_ctrl;
};

struct proc_dir_entry *dram_proc_parent;
static struct mtk_pm_qos_request ddr_request;

void dram_ctl_update_dfrc_fps(int fps)
{
#ifdef MTK_K14_DRM_BOOST
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
		mtk_pm_qos_update_request(&emi_request_lp5, ddr_lp5_now);
	else if (ddr_lp5_hfr_now != -1 && dfps > hfr_fps)
		mtk_pm_qos_update_request(&emi_request_lp5_hfr, ddr_lp5_hfr_now);
	else if (ddr_lp5_now != -1)
		mtk_pm_qos_update_request(&emi_request_lp5, -1);
	else
		mtk_pm_qos_update_request(&emi_request_lp5_hfr, -1);
#endif

#endif

#endif
}

static ssize_t ddr_ctrl_proc_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	struct mtk_dram_ctrl *dram_ctrl = PDE_DATA(file_inode(filp));

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

	if (dram_ctrl->qos_ctrl) {
		if ((val > -1) && (val < dram_ctrl->num_perf))
			mtk_pm_qos_update_request(&ddr_request,
				dram_ctrl->perfs[val]);
		else
			mtk_pm_qos_update_request(&ddr_request,
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
	} else {
		if ((val > -1) && (val < dram_ctrl->num_perf))
			dev_pm_genpd_set_performance_state(
				dram_ctrl->dev,
				dram_ctrl->perfs[val]);
		else
			dev_pm_genpd_set_performance_state(
				dram_ctrl->dev, 0);
	}

	dram_ctrl->curr_opp = val;

	return cnt;
}

static int ddr_ctrl_proc_show(struct seq_file *m, void *v)
{
	struct mtk_dram_ctrl *dram_ctrl = m->private;

	seq_printf(m, "%d\n", dram_ctrl->curr_opp);

	return 0;
}

static int ddr_ctrl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ddr_ctrl_proc_show, PDE_DATA(inode));
}

static const struct file_operations ddr_ctrl_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= ddr_ctrl_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= ddr_ctrl_proc_write,
};

static int mtk_dram_ctrl_probe(struct platform_device *pdev)
{
	struct mtk_dram_ctrl *dram_ctrl;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	int i;

	dram_ctrl = devm_kzalloc(dev, sizeof(*dram_ctrl), GFP_KERNEL);
	if (!dram_ctrl)
		return -ENOMEM;

	dram_ctrl->num_perf = of_count_phandle_with_args(node,
		   "required-opps", NULL);

	if (dram_ctrl->num_perf > 0) {
		dram_ctrl->perfs = devm_kzalloc(&pdev->dev,
			 dram_ctrl->num_perf * sizeof(int),
			GFP_KERNEL);

		if (!dram_ctrl->perfs)
			return -ENOMEM;

		for (i = 0; i < dram_ctrl->num_perf; i++) {
			dram_ctrl->perfs[i] =
				dvfsrc_get_required_opp_performance_state(node, i);
		}
	} else
		dram_ctrl->num_perf = 0;

	if (of_device_is_compatible(node, "mediatek,dram-qosctrl")) {
		mtk_pm_qos_add_request(&ddr_request, MTK_PM_QOS_DDR_OPP,
			MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
		dram_ctrl->qos_ctrl = 1;
	}

	dram_ctrl->curr_opp = dram_ctrl->num_perf;
	dram_ctrl->dev = dev;
	platform_set_drvdata(pdev, dram_ctrl);

	dram_ctrl->proc_dir = proc_mkdir("dram_ctrl", dram_proc_parent);
	if (!dram_ctrl->proc_dir) {
		pr_debug("drams_dir not create success\n");
		return -ENOMEM;
	}

	proc_create_data("ddr", 0644,
		dram_ctrl->proc_dir, &ddr_ctrl_proc_fops, dram_ctrl);

#ifdef MTK_K14_DRM_BOOST
	qos_has_dts_support = 1;
#endif

	return 0;
}

static int mtk_dram_ctrl_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id mtk_dram_ctrl_of_match[] = {
	{
		.compatible = "mediatek,dram-ctrl",
	}, {
		.compatible = "mediatek,dram-qosctrl",
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dram_ctrl_driver = {
	.probe	= mtk_dram_ctrl_probe,
	.remove	= mtk_dram_ctrl_remove,
	.driver = {
		.name = "mtk-dram_ctrl",
		.of_match_table = of_match_ptr(mtk_dram_ctrl_of_match),
	},
};

#ifdef MTK_K14_DRM_BOOST
#ifdef MTK_QOS_SUPPORT
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
		mtk_pm_qos_update_request(&emi_request_lp5_hfr, val);
#endif

	return cnt;
}

static int perfmgr_ddr_lp5_hfr_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", ddr_lp5_hfr_now);
	return 0;
}
PROC_FOPS_RW(ddr_lp5_hfr);

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
		mtk_pm_qos_update_request(&emi_request_lp5, val);
#endif

	return cnt;
}

static int perfmgr_ddr_lp5_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", ddr_lp5_now);
	return 0;
}
PROC_FOPS_RW(ddr_lp5);

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

	mtk_pm_qos_update_request(&emi_request, val);

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

#ifdef MTK_K14_DRM_BOOST
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
#endif

static int perfmgr_dram_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "DDR_TYPE: %d\n", ddr_type);
	return 0;
}
PROC_FOPS_RO(dram);

int dram_ctrl_init_with_no_dts_support(struct proc_dir_entry *parent)
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
	if (!mtk_pm_qos_request_active(&emi_request) ||
		!mtk_pm_qos_request_active(&emi_request_lp5) ||
		!mtk_pm_qos_request_active(&emi_request_lp5_hfr)) {
		pr_debug("hh: emi pm_qos_add_request\n");
		mtk_pm_qos_add_request(&emi_request, MTK_PM_QOS_DDR_OPP,
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
		mtk_pm_qos_add_request(&emi_request_lp5, MTK_PM_QOS_DDR_OPP,
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
		strncpy(emi_request_lp5.owner, owner,
			sizeof(emi_request_lp5.owner) - 1);
		mtk_pm_qos_add_request(&emi_request_lp5_hfr, MTK_PM_QOS_DDR_OPP,
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
		strncpy(emi_request_lp5_hfr.owner, owner_hfr,
			sizeof(emi_request_lp5_hfr.owner) - 1);
	} else {
		pr_debug("hh: emi pm_qos already request\n");
	}
	emi_opp = DDR_OPP_NUM - 1;
#endif

#ifdef VCORE_DVFS_OPP_SUPPORT
	vcore_now = -1;
#endif

#ifdef CONFIG_MEDIATEK_DRAMC
	ddr_type = mtk_dramc_get_ddr_type();
#else
	ddr_type = -1;
#endif
	pr_debug("init dram driver done\n");
out:
	return ret;
}
#endif

int dram_ctrl_init(struct proc_dir_entry *parent)
{
	int ret = 0;

	dram_proc_parent = parent;
	ret = platform_driver_register(&mtk_dram_ctrl_driver);

#ifdef MTK_K14_DRM_BOOST
	if (!qos_has_dts_support)
		dram_ctrl_init_with_no_dts_support(parent);
#endif

	return ret;
}

void dram_ctrl_exit(void)
{
	platform_driver_unregister(&mtk_dram_ctrl_driver);
}


