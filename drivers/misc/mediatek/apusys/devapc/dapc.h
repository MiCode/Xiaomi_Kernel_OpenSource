/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
