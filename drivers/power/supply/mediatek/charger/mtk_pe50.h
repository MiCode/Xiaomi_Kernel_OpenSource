/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __MTK_PE_50_H
#define __MTK_PE_50_H

#include <mt-plat/prop_chgalgo_class.h>

enum {
	PE50_INIT,
	PE50_RUNNING,
};

struct pe50 {
	int state;
	struct prop_chgalgo_device *pca_algo;
	struct notifier_block nb;
	bool online;
	bool is_enabled;
};

#ifdef CONFIG_MTK_PUMP_EXPRESS_50_SUPPORT
extern int pe50_init(void);
extern bool pe50_is_ready(void);
extern int pe50_stop(void);
extern int pe50_run(void);
#else
static inline int pe50_init(void)
{
	return -ENOTSUPP;
}

static inline bool pe50_is_ready(void)
{
	return -ENOTSUPP;
}

static inline  int pe50_stop(void)
{
	return -ENOTSUPP;
}

static inline  int pe50_run(void)
{
	return -ENOTSUPP;
}
#endif /* CONFIG_MTK_PUMP_EXPRESS_50_SUPPORT */
#endif /* __MTK_PE_50_H */
