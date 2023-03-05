/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_LVSYS_NOTIFY_H__
#define __MTK_LVSYS_NOTIFY_H__

#include <linux/notifier.h>

#define LVSYS_F_3400	3400

#define LVSYS_R_3500	(BIT(15) | 3500)

int lvsys_register_notifier(struct notifier_block *nb);
int lvsys_unregister_notifier(struct notifier_block *nb);

#endif /* __MTK_LVSYS_NOTIFY_H__ */
