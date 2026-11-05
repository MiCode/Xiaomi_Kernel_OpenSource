// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/export.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>

#include "reset.h"
#include "trace.h"

static int
qcom_reset_runtime_get(struct qcom_reset_controller *rst)
{
	int ret;

	if (pm_runtime_enabled(rst->dev)) {
		ret = pm_runtime_resume_and_get(rst->dev);
		if (ret < 0) {
			WARN(1, "ret=%d\n", ret);
			return ret;
		}
	}

	return 0;
}

static void
qcom_reset_runtime_put(struct qcom_reset_controller *rst)
{
	int ret;

	if (pm_runtime_enabled(rst->dev)) {
		ret = pm_runtime_put_sync(rst->dev);
		if (ret < 0)
			WARN(1, "ret=%d\n", ret);
	}
}

static void qcom_reset_delay(const struct qcom_reset_map *map)
{
	/*
	 * XO div-4 is commonly used for the reset demets, so by default allow
	 * enough time for 4 demet cycles at 1.2MHz.
	 */
	fsleep(map->udelay ?: 4);
}

static int qcom_reset(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct qcom_reset_controller *rst = to_qcom_reset_controller(rcdev);

	rcdev->ops->assert(rcdev, id);
	qcom_reset_delay(&rst->reset_map[id]);
	rcdev->ops->deassert(rcdev, id);
	return 0;
}

static int qcom_reset_set_assert(struct reset_controller_dev *rcdev,
				 unsigned long id, bool assert)
{
	struct qcom_reset_controller *rst;
	const struct qcom_reset_map *map;
	u32 mask, val;
	int ret = 0;

	rst = to_qcom_reset_controller(rcdev);
	map = &rst->reset_map[id];
	mask = map->bitmask ? map->bitmask : BIT(map->bit);

	ret = qcom_reset_runtime_get(rst);
	if (ret < 0)
		return ret;

	trace_clk_reset(rst, id, assert);

	ret = regmap_update_bits(rst->regmap, map->reg, mask, assert ? mask : 0);
	if (ret)
		goto err;

	/* Ensure the write is fully propagated to the register. */
	ret = regmap_read(rst->regmap, map->reg, &val);
	if (ret)
		goto err;

	/* Give demets a chance to propagate the signal. */
	qcom_reset_delay(map);

err:
	qcom_reset_runtime_put(rst);

	return ret;
}

static int
qcom_reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return qcom_reset_set_assert(rcdev, id, true);
}

static int qcom_reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return qcom_reset_set_assert(rcdev, id, false);
}

static int
qcom_reset_status(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct qcom_reset_controller *rst;
	const struct qcom_reset_map *map;
	u32 mask, reg;
	int ret;

	rst = to_qcom_reset_controller(rcdev);
	map = &rst->reset_map[id];
	mask = map->bitmask ? map->bitmask : BIT(map->bit);

	ret = qcom_reset_runtime_get(rst);
	if (ret < 0)
		return ret;

	ret = regmap_read(rst->regmap, map->reg, &reg);
	if (ret) {
		qcom_reset_runtime_put(rst);
		return ret;
	}

	qcom_reset_runtime_put(rst);

	return (reg & mask);
}

const struct reset_control_ops qcom_reset_ops = {
	.reset = qcom_reset,
	.assert = qcom_reset_assert,
	.deassert = qcom_reset_deassert,
	.status = qcom_reset_status,
};
EXPORT_SYMBOL_GPL(qcom_reset_ops);
