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

int dram_ctrl_init(struct proc_dir_entry *parent)
{
	int ret = 0;

	dram_proc_parent = parent;
	ret = platform_driver_register(&mtk_dram_ctrl_driver);


	return ret;
}

void dram_ctrl_exit(void)
{
	platform_driver_unregister(&mtk_dram_ctrl_driver);
}


