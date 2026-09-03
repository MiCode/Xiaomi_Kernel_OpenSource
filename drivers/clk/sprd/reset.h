/* SPDX-License-Identifier: GPL-2.0-only */
//
// Spreadtrum reset clock driver
//
// Copyright (C) 2022 Spreadtrum, Inc.
// Author: Zhifeng Tang <zhifeng.tang@unisoc.com>

#ifndef _SPRD_RESET_H_
#define _SPRD_RESET_H_

#include <linux/reset-controller.h>
#include <linux/spinlock.h>

struct sprd_reset_map {
	u32	reg;
	u32	mask;
	u32	sc_offset;
};

struct sprd_reset {
	struct reset_controller_dev	rcdev;
	const struct sprd_reset_map	*reset_map;
	struct regmap			*regmap;
	spinlock_t			lock;
};

extern const struct reset_control_ops sprd_reset_ops;
extern const struct reset_control_ops sprd_sc_reset_ops;

#endif /* _SPRD_RESET_H_ */
