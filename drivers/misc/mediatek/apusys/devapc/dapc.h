// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_DAPC_H__
#define __APUSYS_DAPC_H__

#include <linux/types.h>
#include "dapc_cfg.h"

struct dapc_driver {
	void __iomem *reg;
	int irq;
	unsigned int debug_log;
	int enable_ke;
	int enable_aee;
	int enable_irq;
	struct dentry *droot;
	struct dentry *ddebug;
	struct dentry *dfile;
	struct dapc_config *cfg;
};

#endif
