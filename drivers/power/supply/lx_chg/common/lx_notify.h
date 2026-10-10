/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */
#ifndef __LX_NOTIFY_H__
#define __LX_NOTIFY_H__

#include <linux/notifier.h>

enum charger_notifier_events {
	CHARGER_EVENT_DEFAULT = 0,
	CHARGER_EVENT_BC12_DONE = 1,
	CHARGER_EVENT_HVDCP_DONE = 2,
	CHARGER_EVENT_BMS_AUTH_DONE = 3,
#if IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
	CHARGER_EVENT_SHUTDOWN_VOLTAGE_CHANGED = 101,
#endif //IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
};

int lx_charger_notifier_register(struct notifier_block *nb);
int lx_charger_notifier_unregister(struct notifier_block *nb);
int lx_charger_notifier_call_chain(unsigned long val, void *v);

#endif /* __LX_NOTIFY_H__ */
