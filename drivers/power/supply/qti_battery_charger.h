/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */
#ifndef __QTI_TYPEC_CLASS_H
#define __QTI_TYPEC_CLASS_H

enum charger_notifier_events {
	/* thermal board temp */
	 THERMAL_BOARD_TEMP = 0,
};

extern struct blocking_notifier_head charger_notifier;
extern int charger_reg_notifier(struct notifier_block *nb);
extern int charger_unreg_notifier(struct notifier_block *nb);
extern int charger_notifier_call_chain(unsigned long event,int val);

#endif /* __QTI_TYPEC_CLASS_H */