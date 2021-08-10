// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */
#include <dsp/msm-audio-event-notify.h>
#include <linux/export.h>

static ATOMIC_NOTIFIER_HEAD(msm_aud_evt_notifier_list);
static BLOCKING_NOTIFIER_HEAD(msm_aud_evt_blocking_notifier_list);

/**
 *	msm_aud_evt_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int msm_aud_evt_register_client(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&msm_aud_evt_notifier_list, nb);
}
EXPORT_SYMBOL(msm_aud_evt_register_client);

/**
 *	msm_aud_evt_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int msm_aud_evt_unregister_client(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&msm_aud_evt_notifier_list, nb);
}
EXPORT_SYMBOL(msm_aud_evt_unregister_client);

/**
 * msm_aud_evt_notifier_call_chain - notify clients of fb_events
 *
 */
int msm_aud_evt_notifier_call_chain(unsigned long val, void *v)
{
	return atomic_notifier_call_chain(&msm_aud_evt_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(msm_aud_evt_notifier_call_chain);

/**
 *	msm_aud_evt_blocking_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int msm_aud_evt_blocking_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(
			&msm_aud_evt_blocking_notifier_list, nb);
}
EXPORT_SYMBOL(msm_aud_evt_blocking_register_client);

/**
 *	msm_aud_evt_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int msm_aud_evt_blocking_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
			&msm_aud_evt_blocking_notifier_list, nb);
}
EXPORT_SYMBOL(msm_aud_evt_blocking_unregister_client);

/**
 * msm_aud_evt_notifier_call_chain - notify clients of fb_events
 * @val: event or enum maintained by caller
 * @v: private data pointer
 *
 */
int msm_aud_evt_blocking_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(
			&msm_aud_evt_blocking_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(msm_aud_evt_blocking_notifier_call_chain);
