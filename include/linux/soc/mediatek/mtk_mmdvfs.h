/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __SOC_MTK_MMDVFS_H
#define __SOC_MTK_MMDVFS_H

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
int register_mmdvfs_notifier(struct notifier_block *nb);
int unregister_mmdvfs_notifier(struct notifier_block *nb);
#else
static inline int register_mmdvfs_notifier(struct notifier_block *nb)
{ return -EINVAL; }
static inline int unregister_mmdvfs_notifier(struct notifier_block *nb)
{ return -EINVAL; }

#endif /* CONFIG_MTK_MMDVFS */

#endif
