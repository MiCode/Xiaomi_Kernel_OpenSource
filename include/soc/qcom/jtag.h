/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2012-2015, 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef __MACH_JTAG_H
#define __MACH_JTAG_H

#if defined(CONFIG_MSM_JTAG) || defined(CONFIG_MSM_JTAG_MM) || \
	defined(CONFIG_MSM_JTAGV8)
extern void msm_jtag_save_state(void);
extern void msm_jtag_restore_state(void);
extern void msm_jtag_etm_save_state(void);
extern void msm_jtag_etm_restore_state(void);
extern bool msm_jtag_fuse_apps_access_disabled(void);
#else
static inline void msm_jtag_save_state(void) {}
static inline void msm_jtag_restore_state(void) {}
static inline void msm_jtag_etm_save_state(void) {}
static inline void msm_jtag_etm_restore_state(void){}
static inline bool msm_jtag_fuse_apps_access_disabled(void) { return false; }
#endif
#ifdef CONFIG_MSM_JTAGV8
extern int msm_jtag_save_register(struct notifier_block *nb);
extern int msm_jtag_save_unregister(struct notifier_block *nb);
extern int msm_jtag_restore_register(struct notifier_block *nb);
extern int msm_jtag_restore_unregister(struct notifier_block *nb);
#else
static inline int msm_jtag_save_register(struct notifier_block *nb)
{
	return 0;
}
static inline int msm_jtag_save_unregister(struct notifier_block *nb)
{
	return 0;
}
static inline int msm_jtag_restore_register(struct notifier_block *nb)
{
	return 0;
}
static inline int msm_jtag_restore_unregister(struct notifier_block *nb)
{
	return 0;
}
#endif

#endif
