/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
 */

#ifndef _QPNP_LABIBB_REGULATOR_H
#define _QPNP_LABIBB_REGULATOR_H

enum labibb_notify_event {
	LAB_VREG_OK = 1,
	LAB_VREG_NOT_OK,
};

#ifdef CONFIG_REGULATOR_QPNP_LABIBB
int qpnp_labibb_notifier_register(struct notifier_block *nb);
int qpnp_labibb_notifier_unregister(struct notifier_block *nb);
#else
static inline int qpnp_labibb_notifier_register(struct notifier_block *nb)
{
	return 0;
}
static inline int qpnp_labibb_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}
#endif

#endif
