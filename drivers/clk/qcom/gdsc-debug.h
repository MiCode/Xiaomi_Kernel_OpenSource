/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QCOM_GDSC_DEBUG_H__
#define __QCOM_GDSC_DEBUG_H__

#if IS_ENABLED(CONFIG_QCOM_GDSC_REGULATOR)
void gdsc_debug_print_regs(struct regulator *regulator);
#else
static inline void gdsc_debug_print_regs(struct regulator *regulator)
{ }
#endif

#if IS_ENABLED(CONFIG_QCOM_GDSC)
struct gdsc_debug {
	struct list_head list;
	struct gdsc *sc;
	struct device *dev;
};

static DEFINE_MUTEX(gdsc_genpd_debug_lock);

static LIST_HEAD(gdsc_genpd_debug_list);
static struct dentry *genpd_rootdir;

int gdsc_genpd_debug_register(struct gdsc *sc);
void gdsc_genpd_debug_unregister(struct gdsc *sc);
#endif /* CONFIG_QCOM_GDSC */

#endif  /* __QCOM_GDSC_DEBUG_H__ */
