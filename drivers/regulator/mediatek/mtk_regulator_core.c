/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/regulator/mediatek/mtk_regulator_core.h>
#include <linux/regulator/mediatek/mtk_regulator_class.h>
#include <linux/regulator/mediatek/mtk_regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>

static int mtk_simple_regulator_list_voltage(struct regulator_dev *rdev,
	unsigned int index)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	if (mreg_desc->list_voltage)
		return mreg_desc->list_voltage(mreg_desc, index);
	return (index >= rdev->desc->n_voltages) ?
		-EINVAL : mreg_desc->output_list[index];
}

static int mtk_simple_regulator_set_voltage_sel(struct regulator_dev *rdev,
	unsigned int selector)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);
	uint32_t data;
	int ret;

	pr_debug("%s: (%s) selector = %d, output list count = %d\n", __func__,
		rdev->desc->name, selector, rdev->desc->n_voltages);

	if (selector >= rdev->desc->n_voltages)
		return -EINVAL;

	pr_info("%s: (%s) Vout = %d\n", __func__, rdev->desc->name,
		mtk_simple_regulator_list_voltage(rdev, selector));
	data = selector;
	data <<= mreg_desc->vol_shift;
	ret = mreg_desc->mreg_ctrl_ops->register_update_bits(mreg_desc->client,
		mreg_desc->vol_reg, mreg_desc->vol_mask, data);

	pr_debug("%s: (%s), ret = %d\n", __func__, rdev->desc->name, ret);
	return ret;
}

static int mtk_simple_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);
	int ret;
	uint32_t data;

	ret = mreg_desc->mreg_ctrl_ops->register_read(mreg_desc->client,
		mreg_desc->vol_reg, &data);
	if (ret < 0)
		return ret;
	return (data & mreg_desc->vol_mask) >> mreg_desc->vol_shift;
}

static int mtk_simple_regulator_enable(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	pr_debug_ratelimited("%s: (%s) enable regulator\n", __func__,
		rdev->desc->name);
	return mreg_desc->mreg_ctrl_ops->register_update_bits(mreg_desc->client,
		mreg_desc->enable_reg, mreg_desc->enable_bit,
		mreg_desc->enable_bit);
}

static int mtk_simple_regulator_disable(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	pr_debug_ratelimited("%s: (%s) disable regulator\n", __func__,
		rdev->desc->name);
	return mreg_desc->mreg_ctrl_ops->register_update_bits(mreg_desc->client,
		mreg_desc->enable_reg, mreg_desc->enable_bit, 0);
}

static int mtk_simple_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);
	int ret;
	uint32_t data;

	ret = mreg_desc->mreg_ctrl_ops->register_read(mreg_desc->client,
		mreg_desc->enable_reg, &data);
	if (ret < 0)
		return ret;

	ret = (data & (mreg_desc->enable_bit)) ? 1 : 0;
	pr_debug_ratelimited("%s: (%s), enabled = %d\n", __func__,
		rdev->desc->name, ret);
	return ret;
}

static int mtk_simple_regulator_set_mode(struct regulator_dev *rdev,
	unsigned int mode)
{
	return 0;
}

static unsigned int mtk_simple_regulator_get_mode(struct regulator_dev *rdev)
{
	return 0;
}

static int mtk_simple_regulator_get_status(struct regulator_dev *rdev)
{
	return mtk_simple_regulator_is_enabled(rdev) ? REGULATOR_STATUS_ON
		: REGULATOR_STATUS_OFF;
}

static int mtk_simple_regulator_set_suspend_voltage(struct regulator_dev *rdev,
	int uV)
{
	return 0;
}

static int mtk_simple_regulator_set_suspend_enable(struct regulator_dev *rdev)
{
	return 0;
}

static int mtk_simple_regulator_set_suspend_disable(struct regulator_dev *rdev)
{
	return 0;
}

/* =========================================================== */
/* The following is stub function for replacing default ops    */
/* =========================================================== */

static int mtk_simple_regulator_set_voltage_sel_stub(struct regulator_dev *rdev,
	unsigned int selector)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->set_voltage_sel(mreg_desc, selector);
}

static int mtk_simple_regulator_get_voltage_sel_stub(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->get_voltage_sel(mreg_desc);
}

static int mtk_simple_regulator_enable_stub(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->enable(mreg_desc);
}

static int mtk_simple_regulator_disable_stub(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->disable(mreg_desc);
}

static int mtk_simple_regulator_is_enabled_stub(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->is_enabled(mreg_desc);
}

static int mtk_simple_regulator_set_mode_stub(struct regulator_dev *rdev,
	unsigned int mode)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->set_mode(mreg_desc, mode);
}

static unsigned int mtk_simple_regulator_get_mode_stub(
	struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->get_mode(mreg_desc);
}

static int mtk_simple_regulator_get_status_stub(struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->get_status(mreg_desc);
}

static int mtk_simple_regulator_set_suspend_voltage_stub(
	struct regulator_dev *rdev, int uV)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->set_suspend_voltage(mreg_desc, uV);
}

static int mtk_simple_regulator_set_suspend_enable_stub(
	struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->set_suspend_enable(mreg_desc);
}

static int mtk_simple_regulator_set_suspend_disable_stub(
	struct regulator_dev *rdev)
{
	struct mtk_simple_regulator_desc *mreg_desc = rdev_get_drvdata(rdev);

	return mreg_desc->mreg_ext_ops->set_suspend_disable(mreg_desc);
}


static const struct regulator_ops mtk_simple_regulator_ops = {
	.list_voltage		= mtk_simple_regulator_list_voltage,
	.set_voltage_sel	= mtk_simple_regulator_set_voltage_sel,
	.get_voltage_sel	= mtk_simple_regulator_get_voltage_sel,
	.enable			= mtk_simple_regulator_enable,
	.disable		= mtk_simple_regulator_disable,
	.is_enabled		= mtk_simple_regulator_is_enabled,
	.set_mode		= mtk_simple_regulator_set_mode,
	.get_mode		= mtk_simple_regulator_get_mode,
	.get_status		= mtk_simple_regulator_get_status,
	/* for standby/hibernate mode */
	.set_suspend_voltage = mtk_simple_regulator_set_suspend_voltage,
	.set_suspend_enable = mtk_simple_regulator_set_suspend_enable,
	.set_suspend_disable = mtk_simple_regulator_set_suspend_disable,
};

#if 0
struct regulator_ops mtk_simple_regulator_stub_ops = {
	.set_voltage_sel	= mtk_simple_regulator_set_voltage_sel_stub,
	.get_voltage_sel	= mtk_simple_regulator_get_voltage_sel_stub,
	.enable			= mtk_simple_regulator_enable_stub,
	.disable		= mtk_simple_regulator_disable_stub,
	.is_enabled		= mtk_simple_regulator_is_enabled_stub,
	.set_mode		= mtk_simple_regulator_set_mode_stub,
	.get_mode		= mtk_simple_regulator_get_mode_stub,
	.get_status		= mtk_simple_regulator_get_status_stub,
	/* for standby/hibernate mode */
	.set_suspend_voltage = mtk_simple_regulator_set_suspend_voltage_stub,
	.set_suspend_enable = mtk_simple_regulator_set_suspend_enable_stub,
	.set_suspend_disable = mtk_simple_regulator_set_suspend_disable_stub,
};
#endif

static inline struct regulator_dev *_mtk_simple_regulator_register(
	struct regulator_desc *regulator_desc, struct device *dev,
	struct regulator_init_data *init_data, void *driver_data,
	struct device_node *of_node)
{
	struct regulator_config config = {
		.dev = dev,
		.init_data = init_data,
		.driver_data = driver_data,
		.of_node = of_node,
	};

	return regulator_register(regulator_desc, &config);
}

static inline void _mtk_simple_regulator_init_ops(struct regulator_ops *rops,
	 const struct mtk_simple_regulator_ext_ops *mreg_ext_ops)
{
	*rops = mtk_simple_regulator_ops;
	if (!mreg_ext_ops)
		return;

	if (mreg_ext_ops->set_voltage_sel)
		rops->set_voltage_sel =
			mtk_simple_regulator_set_voltage_sel_stub;
	if (mreg_ext_ops->get_voltage_sel)
		rops->get_voltage_sel =
			mtk_simple_regulator_get_voltage_sel_stub;
	if (mreg_ext_ops->enable)
		rops->enable = mtk_simple_regulator_enable_stub;
	if (mreg_ext_ops->disable)
		rops->disable = mtk_simple_regulator_disable_stub;
	if (mreg_ext_ops->is_enabled)
		rops->is_enabled = mtk_simple_regulator_is_enabled_stub;
	if (mreg_ext_ops->set_mode)
		rops->set_mode = mtk_simple_regulator_set_mode_stub;
	if (mreg_ext_ops->get_mode)
		rops->get_mode = mtk_simple_regulator_get_mode_stub;
	if (mreg_ext_ops->get_status)
		rops->get_status = mtk_simple_regulator_get_status_stub;
	if (mreg_ext_ops->set_suspend_voltage)
		rops->set_suspend_voltage =
			mtk_simple_regulator_set_suspend_voltage_stub;
	if (mreg_ext_ops->set_suspend_enable)
		rops->set_suspend_enable =
			mtk_simple_regulator_set_suspend_enable_stub;
	if (mreg_ext_ops->set_suspend_disable)
		rops->set_suspend_disable =
			mtk_simple_regulator_set_suspend_disable_stub;
}

/* #ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#if 0
int mtk_simple_regulator_register(struct mtk_simple_regulator_desc *mreg_desc,
	struct device *dev,
	const struct mtk_simple_regulator_ext_ops *mreg_ext_ops,
	struct mtk_simple_regulator_adv_ops *mreg_adv_ops)
{
	return 0;
}

int mtk_simple_regulator_unregister(struct mtk_simple_regulator_desc *mreg_desc)
{
	return 0;
}

#else

int mtk_simple_regulator_register(struct mtk_simple_regulator_desc *mreg_desc,
	struct device *dev,
	const struct mtk_simple_regulator_ext_ops *mreg_ext_ops,
	struct mtk_simple_regulator_adv_ops *mreg_adv_ops)
{
	struct regulator_init_data *init_data;
	struct regulator_dev *rdev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;

	WARN_ON(mreg_desc->list_voltage == 0 && mreg_desc->output_list == 0);

	/* Parse regulator init data
	 * if we can find child node, use it instead of the original one
	 */
	child_np = of_get_child_by_name(np, mreg_desc->rdesc.name);

	if (child_np)
		init_data = of_get_regulator_init_data(dev, child_np,
			&mreg_desc->rdesc);
	else {
		init_data = of_get_regulator_init_data(dev, np,
			&mreg_desc->rdesc);
		child_np = np;
	}

	if (init_data->constraints.name == NULL) {
		init_data = mreg_desc->def_init_data;
		if (init_data == NULL) {
			pr_notice("%s: (%s) no init data specified\n", __func__,
					mreg_desc->rdesc.name);
			return -ENODEV;
		}
		if (init_data->constraints.name == NULL) {
			init_data->constraints.name = mreg_desc->rdesc.name;
			pr_info("%s: (%s) init_data without name, use name from rdesc\n"
				, __func__, init_data->constraints.name);
		}
		pr_info("%s: (%s) use default init_data\n", __func__,
				init_data->constraints.name);
	}

	/* Set default & extended ops */
	mreg_desc->rdesc.ops = &mreg_desc->rops;
	mreg_desc->mreg_ext_ops = mreg_ext_ops;
	mreg_desc->mreg_adv_ops = mreg_adv_ops;
	_mtk_simple_regulator_init_ops(&mreg_desc->rops, mreg_ext_ops);

	/* Register regulator */
	rdev = _mtk_simple_regulator_register(&mreg_desc->rdesc, dev, init_data,
		mreg_desc, child_np);
	mreg_desc->rdev = rdev;

	/* Register MTK regulator device */
	if (rdev) {
		mreg_desc->mreg_dev = mtk_simple_regulator_device_register(
			init_data->constraints.name, dev, mreg_desc);
		if (IS_ERR(mreg_desc->mreg_dev)) {
			pr_info("%s: (%s) unable to register mreg device\n",
				__func__, init_data->constraints.name);
			mreg_desc->mreg_dev = NULL;
		}
	}

	return (rdev) ? 0 : -ENOMEM;
}

int mtk_simple_regulator_unregister(struct mtk_simple_regulator_desc *mreg_desc)
{
	regulator_unregister(mreg_desc->rdev);
	mtk_simple_regulator_device_unregister(mreg_desc->mreg_dev);
	return 0;
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
