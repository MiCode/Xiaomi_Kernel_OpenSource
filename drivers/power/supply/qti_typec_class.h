/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */
#ifndef __QTI_TYPEC_CLASS_H
#define __QTI_TYPEC_CLASS_H

#include <linux/usb/typec.h>

struct typec_role_class {
	struct typec_port		*typec_port;
	struct typec_capability		typec_caps;
	struct typec_partner		*typec_partner;
	struct typec_partner_desc	typec_partner_desc;
};

enum charger_notifier_events {
	/* thermal board temp */
	 THERMAL_BOARD_TEMP = 0,
};

int qti_typec_partner_register(struct typec_role_class *chip, int mode);
void qti_typec_partner_unregister(struct typec_role_class *chip);
struct typec_role_class *qti_typec_class_init(struct device *dev);
void qti_typec_class_deinit(struct typec_role_class *chip);

extern struct srcu_notifier_head charger_notifier;
extern int charger_reg_notifier(struct notifier_block *nb);
extern int charger_unreg_notifier(struct notifier_block *nb);
extern int charger_notifier_call_cnain(unsigned long event,int val);
#endif /* __QTI_TYPEC_CLASS_H */
