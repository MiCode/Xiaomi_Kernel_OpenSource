/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_PLAT_H__
#define __APU_PLAT_H__
#include <linux/module.h>
#include <linux/devfreq.h>
#include "apu_clk.h"
#include "apu_regulator.h"
#include "apusys_power_user.h"

struct apu_plat_data {
	enum DVFS_USER user;
	const char *clkgp_name;
	const char *rgulgp_name;
	const char *plat_ops_name;

	/* the min voltage that child has to vote parent */
	int threshold_volt;

	/* the max voltage that child can raise */
	int child_volt_limit;
	/* platform flags */
	unsigned bypass_target:1;	/* shall this devfreq bypass target setting? */
};

enum EFUSE_INOF {
	EF_SEGMENT = 0,	// 30
	EF_POD19 = 1,	// 69
	EF_POD26 = 2,	// 209
	APUSYS_EFUSE_NUM,
};

struct apu_dev;

/**
 * struct apu_plat_ops - platform operations.
 *
 * @init_regs: apu_dev register initialization.
 *             (include each engine's )
 * @init_opps: Bining/Raising/Aging initialization
 * @init_clks: Clks/CGs initialization.
 * @init_rgus:		May also return negative errno.
 * @init_devfreq:
 *
 * This struct describes platform operations define for different platform.
 */
struct apu_plat_ops {
	const char *name;
	/* initial operations */
	int (*init_regs)(struct apu_dev *ad);
	int (*init_opps)(struct apu_dev *ad);
	int (*init_clks)(struct apu_dev *ad);
	int (*init_rguls)(struct apu_dev *ad);
	int (*init_devfreq)(struct apu_dev *ad, struct devfreq_dev_profile *pf, void *data);
	int (*init_misc)(struct apu_dev *ad);

	/* uninitial operations */
	void (*uninit_regs)(struct apu_dev *ad);
	void (*uninit_opps)(struct apu_dev *ad);
	void (*uninit_clks)(struct apu_dev *ad);
	void (*uninit_rguls)(struct apu_dev *ad);
	void (*uninit_devfreq)(struct apu_dev *ad);
	void (*uninit_misc)(struct apu_dev *ad);

	/* opp segment flags */
	unsigned segment:1; /* vcore has segment or not */
};

struct apu_plat_ops *apu_plat_get_ops(struct apu_dev *ad, const char *name);

#endif
