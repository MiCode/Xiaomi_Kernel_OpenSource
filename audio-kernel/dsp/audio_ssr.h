/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef __AUDIO_SSR_H_
#define __AUDIO_SSR_H_

enum {
	AUDIO_SSR_DOMAIN_ADSP,
	AUDIO_SSR_DOMAIN_MODEM,
	AUDIO_SSR_DOMAIN_MAX
};

#ifdef CONFIG_MSM_QDSP6_SSR

/*
 * Use audio_ssr_register to register with the SSR subsystem
 *
 * domain_id - Service to use, example: AUDIO_SSR_DOMAIN_ADSP
 * *nb - Pointer to a notifier block. Provide a callback function
 *       to be notified of an event for that service. The ioctls
 *       used by the callback are defined in subsystem_notif.h.
 *
 * Returns: Success: Client handle
 *          Failure: Pointer error code
 */
void *audio_ssr_register(int domain_id, struct notifier_block *nb);

/*
 * Use audio_ssr_deregister to register with the SSR subsystem
 *
 * handle - Handle received from audio_ssr_register
 * *nb - Pointer to a notifier block. Callback function
 *       Used from audio_ssr_register.
 *
 * Returns: Success: 0
 *          Failure: Error code
 */
int audio_ssr_deregister(void *handle, struct notifier_block *nb);


/*
 * Use audio_ssr_send_nmi to force a RAM dump on ADSP
 * down event.
 *
 * *ssr_cb_data - *data received from notifier callback
 */
void audio_ssr_send_nmi(void *ssr_cb_data);

#else

static inline void *audio_ssr_register(int domain_id,
				       struct notifier_block *nb)
{
	return NULL;
}

static inline int audio_ssr_deregister(void *handle, struct notifier_block *nb)
{
	return 0;
}

static inline void audio_ssr_send_nmi(void *ssr_cb_data)
{
}

#endif /* CONFIG_MSM_QDSP6_SSR */

#endif
