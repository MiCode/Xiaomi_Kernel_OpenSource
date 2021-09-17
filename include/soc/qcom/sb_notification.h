/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SB_NOTIFICATION_H
#define _SB_NOTIFICATION_H

/* Indicates a system wake up event */
#define EVENT_REQUEST_WAKE_UP 0x01

/* Events to indicate the remote processor power-up and power-down */
#define EVENT_REMOTE_STATUS_UP 0x02
#define EVENT_REMOTE_STATUS_DOWN 0x03

/* Indicates remote processor woke up the local processor */
#define EVENT_REMOTE_WOKEN_UP 0x04

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
