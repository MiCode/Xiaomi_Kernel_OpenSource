/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SB_NOTIFICATION_H
#define _SB_NOTIFICATION_H

/* Indicates a system wake up event */
#define EVT_WAKE_UP 0x01

#ifdef CONFIG_QTI_NOTIFY_SIDEBAND
/**
 * sb_register_evt_listener - registers a notifier callback
 * @nb: pointer to the notifier block for the callback events
 */
int sb_register_evt_listener(struct notifier_block *nb);

/**
 * sb_unregister_evt_listener - un-registers a notifier callback
 * registered previously.
 * @nb: pointer to the notifier block for the callback events
 */
int sb_unregister_evt_listener(struct notifier_block *nb);

/**
 * sb_notifier_call_chain - send events to all registered listeners
 * as received from publishers.
 * @nb: pointer to the notifier block for the callback events
 */
int sb_notifier_call_chain(unsigned long val, void *v);

#else
static inline int sb_register_evt_listener(struct notifier_block *nb)
{
	return -EINVAL;
}
static inline int sb_unregister_evt_listener(struct notifier_block *nb)
{
	return -EINVAL;
}
static inline int sb_notifier_call_chain(unsigned long val, void *v)
{
	return -EINVAL;
}
#endif /* !CONFIG_QTI_NOTIFY_SIDEBAND */

#endif /* _SB_NOTIFICATION_H */
