// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

static struct dentry *mmdvfs_debugfs_dir;


#define MAX_OPP_NUM (6)
#define MAX_MUX_NUM (10)

struct mmdvfs_mux_data {
	const char *mux_name;
	struct clk *mux;
	struct clk *clk_src[MAX_OPP_NUM];
};

struct mmdvfs_drv_data {
	bool need_change_voltage;
	u32 request_voltage;
	u32 num_muxes;
	struct mmdvfs_mux_data muxes[MAX_MUX_NUM];
	struct notifier_block nb;
	u32 voltages[MAX_OPP_NUM];
};

static void set_mux_clk(struct mmdvfs_mux_data *mux_data, u32 opp_level)
{
	struct clk *mux = mux_data->mux;
	struct clk *clk_src = mux_data->clk_src[opp_level];
	s32 err;

	err = clk_prepare_enable(mux);

	if (err) {
		pr_notice("prepare mux fail:%d opp_level:%d\n", err, opp_level);
		return;
	}

	err = clk_set_parent(mux, clk_src);

	if (err)
		pr_notice("set parent fail:%d opp_level:%d\n", err, opp_level);

	clk_disable_unprepare(mux);
}

static void set_all_muxes(struct mmdvfs_drv_data *drv_data, u32 voltage)
{
	u32 num_muxes = drv_data->num_muxes;
	u32 i;
	u32 opp_level;

	for (i = 0; i < MAX_OPP_NUM; i++) {
		if (drv_data->voltages[i] == voltage) {
			opp_level = i;
			break;
		}
	}
	if (i == MAX_OPP_NUM) {
		pr_notice("voltage(%d) is not found\n", voltage);
		return;
	}

	for (i = 0; i < num_muxes; i++)
		set_mux_clk(&drv_data->muxes[i], opp_level);

	pr_notice("set clk to opp level:%d\n", opp_level);
}

static int regulator_event_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	int uV;
	struct mmdvfs_drv_data *drv_data;
	struct pre_voltage_change_data *pvc_data;

	drv_data = container_of(nb, struct mmdvfs_drv_data, nb);

	if (event == REGULATOR_EVENT_PRE_VOLTAGE_CHANGE) {
		pvc_data = data;
		uV = pvc_data->min_uV;

		if (uV < pvc_data->old_uV) {
			set_all_muxes(drv_data, uV);
			drv_data->request_voltage = uV;
		} else if (uV > pvc_data->old_uV) {
			drv_data->need_change_voltage = true;
		}
		pr_debug("regulator event=PRE_VOLTAGE_CHANGE old=%d new=%d\n",
			pvc_data->old_uV, pvc_data->min_uV);
	} else if (event == REGULATOR_EVENT_VOLTAGE_CHANGE) {
		uV = (unsigned long)data;
		if (drv_data->need_change_voltage == true) {
			set_all_muxes(drv_data, uV);
			drv_data->need_change_voltage = false;
			drv_data->request_voltage = uV;
		}
		pr_debug("regulator event=VOLTAGE_CHANGE voltage=%d\n", uV);
	} else if (event == REGULATOR_EVENT_ABORT_VOLTAGE_CHANGE) {
		uV = (unsigned long) data;
		/* If clk was changed, restore to previous setting */
		if (uV != drv_data->request_voltage) {
			set_all_muxes(drv_data, uV);
			drv_data->need_change_voltage = false;
			drv_data->request_voltage = uV;
		}
		pr_info("regulator event=ABORT_VOLTAGE_CHANGE voltage=%d\n",
			uV);
	}
	return 0;
}


/*
 * mmdvfs driver init
 */

static const struct of_device_id of_mmdvfs_match_tbl[] = {
	{
		.compatible = "mediatek,mmdvfs",
	},
	{}
};

static int setting_show(struct seq_file *s, void *data)
{
	struct mmdvfs_drv_data *drv_data = s->private;
	u32 i;

	seq_printf(s, "mux number:%d\n", drv_data->num_muxes);
	seq_puts(s, "mux:");
	for (i = 0; i < drv_data->num_muxes; i++) {
		seq_printf(s,
			"%s ", drv_data->muxes[i].mux_name);
	}
	seq_puts(s, "\n");
	seq_puts(s, "voltage level:");
	for (i = 0; i < MAX_OPP_NUM; i++) {
		if (!drv_data->voltages[i])
			break;
		seq_printf(s, "%d ", drv_data->voltages[i]);
	}
	seq_puts(s, "\n");
	seq_printf(s, "request voltage:%d\n", drv_data->request_voltage);
	return 0;
}

static int mmdvfs_setting_open(struct inode *inode, struct file *file)
{
	return single_open(file, setting_show, inode->i_private);
}

static const struct file_operations mmdvfs_setting_fops = {
	.open = mmdvfs_setting_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int mmdvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mmdvfs_drv_data *drv_data;
	struct regulator *reg;
	u32 num_mux = 0;
	u32 num_clksrc, index;
	struct property *mux_prop, *clksrc_prop;
	const char *mux_name, *clksrc_name;
	char prop_name[32];
	s32 ret;
	unsigned long freq;
	struct dev_pm_opp *opp;
	struct dentry *dentry;

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;
	dentry = debugfs_create_file("setting", 0444,
			    mmdvfs_debugfs_dir, drv_data, &mmdvfs_setting_fops);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	of_property_for_each_string(
		dev->of_node, "mediatek,support_mux", mux_prop, mux_name) {
		drv_data->muxes[num_mux].mux = devm_clk_get(dev, mux_name);
		drv_data->muxes[num_mux].mux_name = mux_name;
		snprintf(prop_name, sizeof(prop_name)-1,
			"mediatek,mux_%s", mux_name);
		num_clksrc = 0;
		of_property_for_each_string(
			dev->of_node, prop_name, clksrc_prop, clksrc_name) {
			drv_data->muxes[num_mux].clk_src[num_clksrc] =
				devm_clk_get(dev, clksrc_name);
			num_clksrc++;
		}
		num_mux++;
	}
	drv_data->num_muxes = num_mux;

	/* Get voltage info from opp table */
	dev_pm_opp_of_add_table(dev);
	freq = 0;
	index = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		drv_data->voltages[index] = dev_pm_opp_get_voltage(opp);
		freq++;
		index++;
		dev_pm_opp_put(opp);
	}

	reg = devm_regulator_get(dev, "dvfsrc-vcore");
	if (IS_ERR(reg))
		return PTR_ERR(reg);
	drv_data->nb.notifier_call = regulator_event_notify;
	ret = devm_regulator_register_notifier(reg, &drv_data->nb);
	if (ret)
		pr_notice("Failed to register notifier: %d\n", ret);

	return ret;
}

static struct platform_driver mmdvfs_drv = {
	.probe = mmdvfs_probe,
	.driver = {
		.name = "mtk-mmdvfs",
		.owner = THIS_MODULE,
		.of_match_table = of_mmdvfs_match_tbl,
	},
};

static int __init mtk_mmdvfs_init(void)
{
	s32 status;

	mmdvfs_debugfs_dir = debugfs_create_dir("mmdvfs", NULL);
	if (IS_ERR(mmdvfs_debugfs_dir))
		return PTR_ERR(mmdvfs_debugfs_dir);

	status = platform_driver_register(&mmdvfs_drv);
	if (status) {
		pr_notice("Failed to register MMDVFS driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mmdvfs_exit(void)
{
	platform_driver_unregister(&mmdvfs_drv);
	debugfs_remove_recursive(mmdvfs_debugfs_dir);
}

module_init(mtk_mmdvfs_init);
module_exit(mtk_mmdvfs_exit);

MODULE_DESCRIPTION("MTK MMDVFS driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
