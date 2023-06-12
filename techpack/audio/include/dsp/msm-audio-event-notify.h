/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_AUDIO_EVENT_NOTIFY_H_
#define __MSM_AUDIO_EVENT_NOTIFY_H_

#include <linux/notifier.h>

#if (IS_ENABLED(CONFIG_SND_SOC_MSM_QDSP6V2_INTF) || \
	IS_ENABLED(CONFIG_SND_SOC_MSM_QDSP6V2_VM))
int msm_aud_evt_register_client(struct notifier_block *nb);
int msm_aud_evt_unregister_client(struct notifier_block *nb);
int msm_aud_evt_notifier_call_chain(unsigned long val, void *v);
int msm_aud_evt_blocking_register_client(struct notifier_block *nb);
int msm_aud_evt_blocking_unregister_client(struct notifier_block *nb);
int msm_aud_evt_blocking_notifier_call_chain(unsigned long val, void *v);
#else
static inline int msm_aud_evt_register_client(struct notifier_block *nb)
{
	return -ENOSYS;
}

static inline int msm_aud_evt_unregister_client(struct notifier_block *nb)
{
	return -ENOSYS;
}

static inline int msm_aud_evt_notifier_call_chain(unsigned long val, void *v)
{
	return -ENOSYS;
}

static inline int msm_aud_evt_blocking_register_client(
			struct notifier_block *nb)
{
	return -ENOSYS;
}

static inline int msm_aud_evt_blocking_unregister_client(
			struct notifier_block *nb)
{
	return -ENOSYS;
}

static inline int msm_aud_evt_blocking_notifier_call_chain(
			unsigned long val, void *v)
{
	return -ENOSYS;
}
#endif

enum {
	MSM_AUD_DC_EVENT = 1,
	SWR_WAKE_IRQ_REGISTER,
	SWR_WAKE_IRQ_DEREGISTER,
	SWR_WAKE_IRQ_EVENT,
};

#endif
