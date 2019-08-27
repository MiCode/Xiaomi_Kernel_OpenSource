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

#define MMDVFS_DBG

#define MAX_OPP_NUM (6)
#define MAX_MUX_NUM (10)
#define MAX_HOPPING_CLK_NUM (2)

enum {
	ACTION_DEFAULT,
	ACTION_IHDM, /* Voltage Increase: Hopping First, Decrease: MUX First*/
};

struct mmdvfs_mux_data {
	const char *mux_name;
	struct clk *mux;
	struct clk *clk_src[MAX_OPP_NUM];
};

struct mmdvfs_hopping_data {
	const char *hopping_name;
	struct clk *hopping_clk;
	u32 hopping_rate[MAX_OPP_NUM];
};

struct mmdvfs_drv_data {
	bool need_change_voltage;
	u32 request_voltage;
	u32 num_muxes;
	struct mmdvfs_mux_data muxes[MAX_MUX_NUM];
	u32 num_hoppings;
	struct mmdvfs_hopping_data hoppings[MAX_HOPPING_CLK_NUM];
	u32 action;
	struct notifier_block nb;
	u32 voltages[MAX_OPP_NUM];
};

static BLOCKING_NOTIFIER_HEAD(mmdvfs_notifier_list);

/**
 * register_mmdvfs_notifier - register multimedia clk changing notifier
 * @nb: notifier block
 *
 * Register notifier block to receive clk changing  notification.
 */
int register_mmdvfs_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mmdvfs_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(register_mmdvfs_notifier);

/**
 * unregister_mmdvfs_notifier - unregister multimedia clk changing notifier
 * @nb: notifier block
 *
 * Unregister clk changing notifier block.
 */
int unregister_mmdvfs_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mmdvfs_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_mmdvfs_notifier);

static void set_all_muxes(struct mmdvfs_drv_data *drv_data, u32 opp_level)
{
	u32 num_muxes = drv_data->num_muxes;
	u32 i;
	struct clk *mux, *clk_src;
	s32 err;

	for (i = 0; i < num_muxes; i++) {
		mux = drv_data->muxes[i].mux;
		clk_src = drv_data->muxes[i].clk_src[opp_level];
		err = clk_prepare_enable(mux);

		if (err) {
			pr_notice("prepare mux(%s) fail:%d opp_level:%d\n",
				drv_data->muxes[i].mux_name, err, opp_level);
			continue;
		}
		err = clk_set_parent(mux, clk_src);
		if (err)
			pr_notice("set parent(%s) fail:%d opp_level:%d\n",
				drv_data->muxes[i].mux_name, err, opp_level);
		clk_disable_unprepare(mux);
	}
}

static void set_all_hoppings(struct mmdvfs_drv_data *drv_data, u32 opp_level)
{
	u32 num_hoppings = drv_data->num_hoppings;
	u32 i, hopping_rate;
	struct clk *hopping;
	s32 err;

	for (i = 0; i < num_hoppings; i++) {
		hopping = drv_data->hoppings[i].hopping_clk;
		hopping_rate = drv_data->hoppings[i].hopping_rate[opp_level];
		err = clk_prepare_enable(hopping);

		if (err) {
			pr_notice("prepare hopping(%s) fail:%d opp_level:%d\n",
				drv_data->hoppings[i].hopping_name,
				err, opp_level);
			continue;
		}
		err = clk_set_rate(hopping, hopping_rate);
		if (err)
			pr_notice("set %s rate(%u) fail:%d opp_level:%d\n",
				drv_data->hoppings[i].hopping_name,
				hopping_rate, err, opp_level);
		clk_disable_unprepare(hopping);
	}
}

static void set_all_clk(
	struct mmdvfs_drv_data *drv_data, u32 voltage, bool vol_inc)
{
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

	switch (drv_data->action) {
	/* Voltage Increase: Hopping First, Decrease: MUX First*/
	case ACTION_IHDM:
		if (vol_inc) {
			set_all_hoppings(drv_data, opp_level);
			set_all_muxes(drv_data, opp_level);
		} else {
			set_all_muxes(drv_data, opp_level);
			set_all_hoppings(drv_data, opp_level);
		}
		break;
	default:
		set_all_muxes(drv_data, opp_level);
		break;
	}
	blocking_notifier_call_chain(&mmdvfs_notifier_list, opp_level, NULL);
	pr_debug("set clk to opp level:%d\n", opp_level);
}

static int regulator_event_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	unsigned long uV;
	struct mmdvfs_drv_data *drv_data;
	struct pre_voltage_change_data *pvc_data;

	drv_data = container_of(nb, struct mmdvfs_drv_data, nb);

	if (event == REGULATOR_EVENT_PRE_VOLTAGE_CHANGE) {
		pvc_data = data;
		uV = pvc_data->min_uV;

		if (uV < pvc_data->old_uV) {
			set_all_clk(drv_data, uV, false);
			drv_data->request_voltage = uV;
		} else if (uV > pvc_data->old_uV) {
			drv_data->need_change_voltage = true;
		}
		pr_debug("regulator event=PRE_VOLTAGE_CHANGE old=%lu new=%lu\n",
			pvc_data->old_uV, pvc_data->min_uV);
	} else if (event == REGULATOR_EVENT_VOLTAGE_CHANGE) {
		uV = (unsigned long)data;
		if (drv_data->need_change_voltage == true) {
			set_all_clk(drv_data, uV, true);
			drv_data->need_change_voltage = false;
			drv_data->request_voltage = uV;
		}
		pr_debug("regulator event=VOLTAGE_CHANGE voltage=%lu\n", uV);
	} else if (event == REGULATOR_EVENT_ABORT_VOLTAGE_CHANGE) {
		uV = (unsigned long) data;
		/* If clk was changed, restore to previous setting */
		if (uV != drv_data->request_voltage) {
			set_all_clk(drv_data, uV,
				uV > drv_data->request_voltage);
			drv_data->need_change_voltage = false;
			drv_data->request_voltage = uV;
		}
		pr_info("regulator event=ABORT_VOLTAGE_CHANGE voltage=%lu\n",
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
	u32 i, j;

	seq_printf(s, "mux number:%d\n", drv_data->num_muxes);
	seq_puts(s, "mux:");
	for (i = 0; i < drv_data->num_muxes; i++) {
		seq_printf(s,
			"%s ", drv_data->muxes[i].mux_name);
	}
	seq_puts(s, "\n");
	seq_printf(s, "hopping number:%d\n", drv_data->num_hoppings);
	for (i = 0; i < drv_data->num_hoppings; i++) {
		seq_printf(s,
			"%s: ", drv_data->hoppings[i].hopping_name);
		for (j = 0; j < MAX_OPP_NUM; j++) {
			if (!drv_data->hoppings[i].hopping_rate[j])
				break;
			seq_printf(s, "%d ",
				drv_data->hoppings[i].hopping_rate[j]);
		}
		seq_puts(s, "\n");
	}
	seq_printf(s, "action: %d\n", drv_data->action);
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

#ifdef MMDVFS_DBG
struct mmdvfs_dbg_data {
	struct mmdvfs_drv_data *drv_data;
	struct regulator *reg;
	u32 max_voltage;
};
static int force_clk_set(void *data, u64 val)
{
	struct mmdvfs_dbg_data *dbg_data = (struct mmdvfs_dbg_data *)data;
	struct mmdvfs_drv_data *drv_data = dbg_data->drv_data;
	s32 ret;

	if (val == 0) {
		ret = devm_regulator_register_notifier(
				dbg_data->reg, &drv_data->nb);
		if (ret)
			pr_notice("Failed to register notifier: %d\n", ret);
		regulator_set_voltage(dbg_data->reg, 0, dbg_data->max_voltage);
	} else {
		devm_regulator_unregister_notifier(
				dbg_data->reg, &drv_data->nb);
		if (val > drv_data->request_voltage) {
			regulator_set_voltage(
				dbg_data->reg, val, dbg_data->max_voltage);
			set_all_clk(drv_data, val, true);
		} else {
			set_all_clk(drv_data, val, false);
			regulator_set_voltage(
				dbg_data->reg, val, dbg_data->max_voltage);
		}
	}

	pr_notice("%s: val=%llu\n", __func__, val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(force_clk_ops, NULL, force_clk_set, "%llu\n");
#endif /* MMDVFS_DBG */

static int mmdvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mmdvfs_drv_data *drv_data;
#ifdef MMDVFS_DBG
	struct mmdvfs_dbg_data *dbg_data;
#endif
	struct regulator *reg;
	u32 num_mux = 0, num_hopping = 0;
	u32 num_clksrc, index, hopping_rate, num_hopping_rate;
	struct property *mux_prop, *clksrc_prop;
	struct property *hopping_prop, *hopping_rate_prop;
	const char *mux_name, *clksrc_name, *hopping_name;
	char prop_name[32];
	const __be32 *p;
	s32 ret;
	unsigned long freq;
	struct dev_pm_opp *opp;
	struct dentry *dentry;

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	of_property_for_each_string(
		dev->of_node, "mediatek,support_mux", mux_prop, mux_name) {
		if (num_mux >= MAX_MUX_NUM) {
			pr_notice("Too many items in support_mux\n");
			return -EINVAL;
		}
		drv_data->muxes[num_mux].mux = devm_clk_get(dev, mux_name);
		drv_data->muxes[num_mux].mux_name = mux_name;
		snprintf(prop_name, sizeof(prop_name)-1,
			"mediatek,mux_%s", mux_name);
		num_clksrc = 0;
		of_property_for_each_string(
			dev->of_node, prop_name, clksrc_prop, clksrc_name) {
			if (num_clksrc >= MAX_OPP_NUM) {
				pr_notice("Too many items in %s\n", prop_name);
				return -EINVAL;
			}
			drv_data->muxes[num_mux].clk_src[num_clksrc] =
				devm_clk_get(dev, clksrc_name);
			num_clksrc++;
		}
		num_mux++;
	}
	drv_data->num_muxes = num_mux;

	of_property_for_each_string(dev->of_node, "mediatek,support_hopping",
		hopping_prop, hopping_name) {
		if (num_hopping >= MAX_HOPPING_CLK_NUM) {
			pr_notice("Too many items in support_hopping\n");
			return -EINVAL;
		}
		drv_data->hoppings[num_hopping].hopping_clk =
					devm_clk_get(dev, hopping_name);
		drv_data->hoppings[num_hopping].hopping_name = hopping_name;
		snprintf(prop_name, sizeof(prop_name)-1,
			"mediatek,hopping_%s", hopping_name);
		num_hopping_rate = 0;
		of_property_for_each_u32(dev->of_node, prop_name,
				hopping_rate_prop, p, hopping_rate) {
			if (num_hopping_rate >= MAX_OPP_NUM) {
				pr_notice("Too many items in %s\n", prop_name);
				return -EINVAL;
			}
			drv_data->hoppings[num_hopping].hopping_rate[
					num_hopping_rate] = hopping_rate;
			num_hopping_rate++;
		}
		num_hopping++;
	}
	drv_data->num_hoppings = num_hopping;

	of_property_read_u32(dev->of_node,
		"mediatek,action", &drv_data->action);

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

	mmdvfs_debugfs_dir = debugfs_create_dir("mmdvfs", NULL);
	if (IS_ERR(mmdvfs_debugfs_dir))
		pr_notice("Failed to create debugfs dir mmdvfs: %ld\n",
			PTR_ERR(mmdvfs_debugfs_dir));
	dentry = debugfs_create_file("setting", 0444,
			mmdvfs_debugfs_dir, drv_data, &mmdvfs_setting_fops);
	if (IS_ERR(dentry))
		pr_notice("Failed to create debugfs setting: %ld\n",
			PTR_ERR(dentry));
#ifdef MMDVFS_DBG
	dbg_data = devm_kzalloc(dev, sizeof(*dbg_data), GFP_KERNEL);
	if (!dbg_data)
		return -ENOMEM;
	dbg_data->drv_data = drv_data;
	dbg_data->reg = reg;
	dbg_data->max_voltage = drv_data->voltages[index-1];
	dentry = debugfs_create_file("force_clk", 0200,
			mmdvfs_debugfs_dir, dbg_data, &force_clk_ops);
	if (IS_ERR(dentry))
		pr_notice("Failed to create debugfs force_clk: %ld\n",
			PTR_ERR(dentry));
#endif

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
