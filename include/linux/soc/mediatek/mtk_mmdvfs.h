/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __SOC_MTK_MMDVFS_H
#define __SOC_MTK_MMDVFS_H

typedef void (*record_opp)(const u8 opp);

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
int register_mmdvfs_notifier(struct notifier_block *nb);
int unregister_mmdvfs_notifier(struct notifier_block *nb);
int mmdvfs_set_force_step(int force_step);
int mmdvfs_set_vote_step(int vote_step);
void mmdvfs_debug_record_opp_set_fp(record_opp fp);
#else
static inline int register_mmdvfs_notifier(struct notifier_block *nb)
{ return -EINVAL; }
static inline int unregister_mmdvfs_notifier(struct notifier_block *nb)
{ return -EINVAL; }
static inline int mmdvfs_set_force_step(int new_force_step)
{ return 0; }
static inline int mmdvfs_set_vote_step(int new_force_step)
{ return 0; }
static inline void mmdvfs_debug_record_opp_set_fp(record_opp fp) {}
#endif /* CONFIG_MTK_MMDVFS */

#endif
