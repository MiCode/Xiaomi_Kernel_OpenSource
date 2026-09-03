// SPDX-License-Identifier: GPL-2.0-only
//
// Spreadtrum reset clock driver
//
// Copyright (C) 2022 Spreadtrum, Inc.
// Author: Zhifeng Tang <zhifeng.tang@unisoc.com>

#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include "reset.h"

static inline struct sprd_reset *to_sprd_reset(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct sprd_reset, rcdev);
}

static int sprd_reset_assert(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	struct sprd_reset *reset = to_sprd_reset(rcdev);
	const struct sprd_reset_map *map = &reset->reset_map[id];
	unsigned long flags;
	unsigned int val = 0;

	spin_lock_irqsave(&reset->lock, flags);
	regmap_read(reset->regmap, map->reg, &val);
	val |= map->mask;
	regmap_write(reset->regmap, map->reg, val);
	spin_unlock_irqrestore(&reset->lock, flags);

	return 0;
}

static int sprd_reset_deassert(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	struct sprd_reset *reset = to_sprd_reset(rcdev);
	const struct sprd_reset_map *map = &reset->reset_map[id];
	unsigned long flags;
	unsigned int val = 0;

	spin_lock_irqsave(&reset->lock, flags);
	regmap_read(reset->regmap, map->reg, &val);
	val &= ~map->mask;
	regmap_write(reset->regmap, map->reg, val);
	spin_unlock_irqrestore(&reset->lock, flags);

	return 0;
}

static int sprd_reset_reset(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	sprd_reset_assert(rcdev, id);
	udelay(1);
	sprd_reset_deassert(rcdev, id);

	return 0;
}

static int sprd_sc_reset_assert(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	struct sprd_reset *reset = to_sprd_reset(rcdev);
	const struct sprd_reset_map *map = &reset->reset_map[id];
	unsigned int offset = map->sc_offset;

	return regmap_write(reset->regmap, map->reg + offset, map->mask);
}

static int sprd_sc_reset_deassert(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	struct sprd_reset *reset = to_sprd_reset(rcdev);
	const struct sprd_reset_map *map = &reset->reset_map[id];
	unsigned int offset = map->sc_offset * 2;

	return regmap_write(reset->regmap, map->reg + offset, map->mask);
}

static int sprd_sc_reset_reset(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	sprd_sc_reset_assert(rcdev, id);
	udelay(1);
	sprd_sc_reset_deassert(rcdev, id);

	return 0;
}

const struct reset_control_ops sprd_reset_ops = {
	.assert		= sprd_reset_assert,
	.deassert	= sprd_reset_deassert,
	.reset		= sprd_reset_reset,
};
EXPORT_SYMBOL_GPL(sprd_reset_ops);

const struct reset_control_ops sprd_sc_reset_ops = {
	.assert		= sprd_sc_reset_assert,
	.deassert	= sprd_sc_reset_deassert,
	.reset		= sprd_sc_reset_reset,
};
EXPORT_SYMBOL_GPL(sprd_sc_reset_ops);
