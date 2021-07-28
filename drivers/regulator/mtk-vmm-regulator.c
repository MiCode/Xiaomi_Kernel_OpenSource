// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk-vmm-regulator.h"

#define CREATE_TRACE_POINTS
#include "mtk-vmm-trace.h"

#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/printk.h>
#include <linux/pm_opp.h>
#include <linux/clk.h>
#include <linux/debugfs.h>

static struct dentry *ispdvfs_debugfs_dir;
static struct regulator *vmm_reg;

#define REGULATOR_ID_VMM 0

int mtk_ispdvfs_dbg_level;
EXPORT_SYMBOL(mtk_ispdvfs_dbg_level);
module_param(mtk_ispdvfs_dbg_level, int, 0644);
#define ISPDVFS_DBG
#ifdef ISPDVFS_DBG
#define ISP_LOGD(fmt, args...) \
	do { \
		if (mtk_ispdvfs_dbg_level) \
			pr_debug("[ISPDVFS] %s(): " fmt "\n",\
				__func__, ##args); \
	} while (0)
#else
#define ISPDVFS_DBG(fmt, args...)
#endif
#define ISP_LOGI(fmt, args...) \
	pr_notice("[ISPDVFS] %s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("[ISPDVFS] error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("[ISPDVFS] fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

#define CREATE_REGULATOR(match, _name)	\
	.desc = {					\
		.name = match,				\
		.of_match = of_match_ptr(match),	\
		.ops = &vmm_apmcu_ops,		\
		.type = REGULATOR_VOLTAGE,		\
		.id = REGULATOR_ID_##_name,		\
		.owner = THIS_MODULE,			\
	}

struct vmm_regulator {
	struct regulator_desc	desc;
	struct dvfs_driver_data *dvfs_data;
	int is_enable;
};

static int show_setting(struct seq_file *s, void *data)
{
	struct dvfs_driver_data *drv_data = s->private;
	struct dvfs_table *table = &(drv_data->opp_table);
	struct dvfs_info *cur_dvfs = &(drv_data->current_dvfs);
	u32 i;

	seq_printf(s, "mux number:%d\n", drv_data->num_muxes);
	seq_puts(s, "mux:");
	for (i = 0; i < drv_data->num_muxes; i++) {
		seq_printf(s,
			"%s ", drv_data->muxes[i].mux_name);
	}
	seq_puts(s, "\n");
	seq_puts(s, "Support voltage:");
	for (i = 0; i < table->opp_num; i++)
		seq_printf(s, "%d ", table->voltage[i]);
	seq_puts(s, "\n");
	seq_printf(s, "Current voltage:%d\n", cur_dvfs->voltage_target);
	return 0;
}

static int open_setting(struct inode *inode, struct file *file)
{
	return single_open(file, show_setting, inode->i_private);
}

static const struct file_operations ispdvfs_setting_fops = {
	.open = open_setting,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

struct ispdvfs_dbg_data {
	struct dvfs_driver_data *drv_data;
	struct regulator *reg;
	int max_voltage;
};

static int force_dvfs_set(void *data, u64 val)
{
	struct ispdvfs_dbg_data *dbg_data = (struct ispdvfs_dbg_data *)data;

	ISP_LOGI("Force max dvfs control(%d)", val);
	if (IS_ERR(dbg_data->reg)) {
		ISP_LOGE("can't get dvfs regulator\n");
		return PTR_ERR(dbg_data->reg);
	}

	if (val == 0) {
		regulator_set_voltage(dbg_data->reg, 0, INT_MAX);
	} else {
		regulator_set_voltage(dbg_data->reg,
				dbg_data->max_voltage, INT_MAX);
	}

	return 0;
}

static int force_opp_level(void *data, u64 val)
{
	struct ispdvfs_dbg_data *dbg_data = (struct ispdvfs_dbg_data *)data;
	struct dvfs_driver_data *drv_data;
	struct dvfs_table *opp_table;
	int voltage = 0;

	ISP_LOGI("Force opp level(%d)", val);

	if (IS_ERR(dbg_data)) {
		ISP_LOGE("dbg_data is NULL\n");
		return PTR_ERR(dbg_data);
	}

	if (IS_ERR(dbg_data->reg)) {
		ISP_LOGE("can't get dvfs regulator\n");
		return PTR_ERR(dbg_data->reg);
	}

	drv_data = dbg_data->drv_data;
	if (IS_ERR(drv_data)) {
		ISP_LOGE("dbg_data is NULL\n");
		return PTR_ERR(drv_data);
	}
	opp_table = &(drv_data->opp_table);

	if (val < opp_table->opp_num)
		voltage = opp_table->voltage[val];
	else
		ISP_LOGI("Opp level is not in range. Use default level\n");

	regulator_set_voltage(dbg_data->reg, voltage, INT_MAX);

	return 0;
}

static inline struct device *to_vmm_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent;
}

static void set_all_muxes(struct dvfs_driver_data *drv_data, u32 opp_level)
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
			ISP_LOGE("prepare mux(%s) fail:%d opp_level:%d\n",
			drv_data->muxes[i].mux_name, err, opp_level);
			continue;
		}
		err = clk_set_parent(mux, clk_src);
		if (err)
			ISP_LOGE("set parent(%s) fail:%d opp_level:%d\n",
					drv_data->muxes[i].mux_name, err, opp_level);
		clk_disable_unprepare(mux);
	}
}

static int apmcu_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned int *selector)
{
	int ret = 0;
	struct vmm_regulator *regulator;
	struct dvfs_driver_data *drv_data;
	struct dvfs_info *current_info;
	struct dvfs_table *table;
	u32 cur_opp_idx = 0, prev_opp_idx = 0;
	u32 i;

	trace_vmm__update_voltage(min_uV);

	regulator = rdev_get_drvdata(rdev);
	if (!regulator) {
		ISP_LOGE("rdev_get_drvdata ptr is null");
		return PTR_ERR(regulator);
	}

	drv_data = regulator->dvfs_data;
	if (!drv_data) {
		ISP_LOGE("drv_data ptr is null");
		return PTR_ERR(drv_data);
	}
	table = &(drv_data->opp_table);

	current_info = &(drv_data->current_dvfs);
	mutex_lock(&current_info->voltage_mutex);

	for (i = 0; i < table->opp_num; i++) {
		if (min_uV <= table->voltage[i]) {
			cur_opp_idx = i;
			break;
		}
	}
	prev_opp_idx = current_info->opp_level;

	if (cur_opp_idx < prev_opp_idx) {
		/* Upldate Frequency firstly */
		set_all_muxes(drv_data, cur_opp_idx);

		/* Then update voltage */
		regulator_set_voltage(vmm_reg, min_uV, INT_MAX);
		current_info->voltage_target = min_uV;
		current_info->opp_level = cur_opp_idx;
		mutex_unlock(&current_info->voltage_mutex);
	} else {
		/* Update voltage firstly */
		regulator_set_voltage(vmm_reg, min_uV, INT_MAX);
		current_info->voltage_target = min_uV;
		current_info->opp_level = cur_opp_idx;
		mutex_unlock(&current_info->voltage_mutex);

		/* Then update frequency */
		set_all_muxes(drv_data, cur_opp_idx);
	}

	return ret;
}

static int vmm_get_voltage(struct regulator_dev *rdev)
{
	struct vmm_regulator *regulator;
	struct dvfs_driver_data *drv_data;
	struct dvfs_info *current_info;
	int current_voltage = DEFAULT_VOLTAGE;

	regulator = rdev_get_drvdata(rdev);
	if (!regulator) {
		ISP_LOGE("rdev_get_drvdata ptr is null");
		goto out;
	}

	drv_data = regulator->dvfs_data;
	if (!drv_data) {
		ISP_LOGE("drv_data ptr is null");
		goto out;
	}

	current_info = &(drv_data->current_dvfs);
	mutex_lock(&current_info->voltage_mutex);
	current_voltage = current_info->voltage_target;
	mutex_unlock(&current_info->voltage_mutex);

out:
	return current_voltage;
}

static void vmm_init_opp_table(struct dvfs_driver_data *data)
{
	struct dvfs_table *opp_table = &(data->opp_table);
	struct dev_pm_opp *opp;
	unsigned long freq = 0;
	int clk_num = 0, i = 0;

	dev_pm_opp_of_add_table(data->dev);
	clk_num = dev_pm_opp_get_opp_count(data->dev);
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(data->dev, &freq))) {
		opp_table->frequency[i] = freq;
		opp_table->voltage[i] = dev_pm_opp_get_voltage(opp);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
	opp_table->opp_num = clk_num;
	for (i = 0; i < opp_table->opp_num; i++) {
		ISP_LOGD("Opp table: idx=%d, clk=%d volt=%d\n",
				i, opp_table->frequency[i], opp_table->voltage[i]);
	}
}

static int vmm_enable_regulator(struct regulator_dev *rdev)
{
	struct vmm_regulator *regulator;

	ISP_LOGI("Enable vmm regulator");
	regulator = rdev_get_drvdata(rdev);
	if (!regulator) {
		ISP_LOGE("rdev_get_drvdata ptr is null");
		return PTR_ERR(regulator);
	}

	regulator->is_enable = 1;

	return 0;
}

static int vmm_disable_regulator(struct regulator_dev *rdev)
{
	struct vmm_regulator *regulator;

	ISP_LOGI("Disable vmm regulator");
	regulator = rdev_get_drvdata(rdev);
	if (!regulator) {
		ISP_LOGE("rdev_get_drvdata ptr is null");
		return PTR_ERR(regulator);
	}

	regulator->is_enable = 0;
	return 0;
}

static int vmm_is_enabled(struct regulator_dev *rdev)
{
	struct vmm_regulator *regulator = rdev_get_drvdata(rdev);

	return regulator->is_enable;
}

static const struct regulator_ops vmm_apmcu_ops = {
	.list_voltage = regulator_list_voltage_table,
	.set_voltage = apmcu_set_voltage,
	.get_voltage = vmm_get_voltage,
	.enable = vmm_enable_regulator,
	.disable = vmm_disable_regulator,
	.is_enabled = vmm_is_enabled,
};

static struct vmm_regulator platform_regulators = {
	CREATE_REGULATOR("vmm-proxy", VMM),
};

static const struct of_device_id mtk_vmm_regulator_match[] = {
	{
		.compatible = "mediatek,ispdvfs",
		.data = &platform_regulators,
	}, {
		/* sentinel */
	},
};

DEFINE_SIMPLE_ATTRIBUTE(force_dvfs_ops, NULL, force_dvfs_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(force_opp_level_ops, NULL, force_opp_level, "%llu\n");
static int vmm_regulator_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	struct vmm_regulator *regulator;
	struct dvfs_driver_data *dvfs_data;
	struct dvfs_table *opp_table;
	struct dvfs_info *current_dvfs;
	int support_micro_processor = 0;
	struct property *mux_prop, *clksrc_prop;
	const char *mux_name, *clksrc_name;
	u32 num_mux = 0;
	u32 num_clksrc;
	char prop_name[32];
	struct dentry *dentry;
	struct ispdvfs_dbg_data *dbg_data;
	u32 opp_level = 0;

	match = of_match_node(mtk_vmm_regulator_match, dev->of_node);
	if (!match) {
		ISP_LOGE("invalid compatible string\n");
		return -ENODEV;
	}

	dvfs_data = devm_kzalloc(dev, sizeof(*dvfs_data), GFP_KERNEL);
	if (!dvfs_data) {
		ISP_LOGE("devm_kzalloc dvfs_data fail\n");
		return -ENODEV;
	}
	dvfs_data->dev = dev;
	dev_set_drvdata(dev, dvfs_data);
	vmm_init_opp_table(dvfs_data);

	current_dvfs = &(dvfs_data->current_dvfs);
	mutex_init(&current_dvfs->voltage_mutex);
	opp_table = &(dvfs_data->opp_table);
	current_dvfs->voltage_target = opp_table->voltage[opp_level];
	current_dvfs->opp_level = opp_level;

	of_property_for_each_string(
		dev->of_node, "mediatek,support_mux", mux_prop, mux_name) {
		if (num_mux >= MAX_MUX_NUM) {
			ISP_LOGE("Too many items in support_mux\n");
			return -EINVAL;
		}
		dvfs_data->muxes[num_mux].mux = devm_clk_get(dev, mux_name);
		dvfs_data->muxes[num_mux].mux_name = mux_name;
		snprintf(prop_name, sizeof(prop_name)-1,
			"mediatek,mux_%s", mux_name);
		num_clksrc = 0;
		of_property_for_each_string(
			dev->of_node, prop_name, clksrc_prop, clksrc_name) {
			if (num_clksrc >= MAX_OPP_STEP) {
				ISP_LOGE("Too many items in %s\n", prop_name);
				return -EINVAL;
			}
			dvfs_data->muxes[num_mux].clk_src[num_clksrc] =
				devm_clk_get(dev, clksrc_name);
			num_clksrc++;
		}
		num_mux++;
	}
	dvfs_data->num_muxes = num_mux;

	/* Real regualtor instance which controls vmm directly */
	vmm_reg = devm_regulator_get(dev, "buck-vmm");
	if (IS_ERR(vmm_reg))
		ISP_LOGE("could not get buck-vmm regulator\n");

	of_property_read_u32(dev->of_node,
		"mediatek,support_micro_processor",
		&support_micro_processor);

	regulator = (struct vmm_regulator *)(match->data);
	regulator->dvfs_data = dvfs_data;
	regulator->desc.n_voltages = ARRAY_SIZE(opp_table->voltage);
	regulator->desc.volt_table = opp_table->voltage;
	regulator->is_enable = 0;

	config.dev = dev;
	config.driver_data = regulator;
	rdev = devm_regulator_register(dev,
								&regulator->desc,
								&config);
	if (IS_ERR(rdev)) {
		ISP_LOGE("failed to register %s\n",
			regulator->desc.name);
		goto fail_destroy_mutex;
	}

	ispdvfs_debugfs_dir = debugfs_create_dir("ispdvfs", NULL);
	if (IS_ERR(ispdvfs_debugfs_dir))
		ISP_LOGE("Failed to create debugfs dir ispdvfs: %ld\n",
			PTR_ERR(ispdvfs_debugfs_dir));
	dentry = debugfs_create_file("setting", 0444,
			ispdvfs_debugfs_dir, dvfs_data, &ispdvfs_setting_fops);
	if (IS_ERR(dentry))
		ISP_LOGE("Failed to create debugfs setting: %ld\n",
			PTR_ERR(dentry));

	dbg_data = devm_kzalloc(dev, sizeof(*dbg_data), GFP_KERNEL);
	if (!dbg_data) {
		ISP_LOGE("devm_kzalloc fail: %ld\n",
			PTR_ERR(dbg_data));
		return -ENOMEM;
	}
	dbg_data->drv_data = dvfs_data;
	dbg_data->reg = devm_regulator_get(dev, "dvfs-vmm");
	dbg_data->max_voltage = opp_table->voltage[opp_table->opp_num - 1];
	dentry = debugfs_create_file("force_max_freq", 0200,
			ispdvfs_debugfs_dir, dbg_data, &force_dvfs_ops);
	if (IS_ERR(dentry))
		ISP_LOGE("Failed to create debugfs force_max_freq: %ld\n",
			PTR_ERR(dentry));

	/* Need to remove. We do not allow select operating point level */
	dentry = debugfs_create_file("force_opp_level", 0200,
			ispdvfs_debugfs_dir, dbg_data, &force_opp_level_ops);
	if (IS_ERR(dentry))
		ISP_LOGE("Failed to create debugfs force_opp_level: %ld\n",
			PTR_ERR(dentry));

	return 0;

fail_destroy_mutex:
	mutex_destroy(&current_dvfs->voltage_mutex);
	return -ENODEV;
}

static struct platform_driver mtk_vmm_regulator_driver = {
	.driver = {
		.name  = "mtk-vmm-regulator",
		.owner = THIS_MODULE,
		.of_match_table = mtk_vmm_regulator_match,
	},
	.probe = vmm_regulator_probe,
};

static int __init mtk_vmm_regulator_init(void)
{
	s32 status;

	status = platform_driver_register(&mtk_vmm_regulator_driver);
	if (status) {
		ISP_LOGE("Failed to register VMM driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}
subsys_initcall(mtk_vmm_regulator_init);

static void __exit mtk_vmm_regulator_exit(void)
{
	platform_driver_unregister(&mtk_vmm_regulator_driver);
}
module_exit(mtk_vmm_regulator_exit);

MODULE_AUTHOR("Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>");
MODULE_DESCRIPTION("VMM regulator driver");
MODULE_LICENSE("GPL v2");
