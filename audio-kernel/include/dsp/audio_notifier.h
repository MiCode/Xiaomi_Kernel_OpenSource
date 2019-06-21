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

#ifndef __AUDIO_NOTIFIER_H_
#define __AUDIO_NOTIFIER_H_

/* State of the notifier domain */
enum {
	AUDIO_NOTIFIER_SERVICE_DOWN,
	AUDIO_NOTIFIER_SERVICE_UP
};

/* Service order determines connection priority
 * Highest number connected first
 */
enum {
	AUDIO_NOTIFIER_SSR_SERVICE,
	AUDIO_NOTIFIER_PDR_SERVICE,
	AUDIO_NOTIFIER_MAX_SERVICES
};

enum {
	AUDIO_NOTIFIER_ADSP_DOMAIN,
	AUDIO_NOTIFIER_MODEM_DOMAIN,
	AUDIO_NOTIFIER_MAX_DOMAINS
};

/* Structure populated in void *data of nb function
 * callback used for audio_notifier_register
 */
struct audio_notifier_cb_data {
	int service;
	int domain;
};

#if IS_ENABLED(CONFIG_MSM_QDSP6_NOTIFIER)

/*
 * Use audio_notifier_register to register any audio
 * clients who need to be notified of a remote process.
 * This API will determine and register the client with
 * the best available subsystem (SSR or PDR) for that
 * domain (Adsp or Modem). When an event is sent from that
 * domain the notifier block callback function will be called.
 *
 * client_name - A unique user name defined by the client.
 *	If the same name is used for multiple calls each will
 *	be tracked & called back separately and a single call
 *	to deregister will delete them all.
 * domain - Domain the client wants to get events from.
 *	AUDIO_NOTIFIER_ADSP_DOMAIN
 *	AUDIO_NOTIFIER_MODEM_DOMAIN
 * *nb - Pointer to a notifier block. Provide a callback function
 *	to be notified of an even on that domain.
 *
 *      nb_func(struct notifier_block *this, unsigned long opcode, void *data)
 *		this - pointer to own nb
 *		opcode - event from registered domain
 *			AUDIO_NOTIFIER_SERVICE_DOWN
 *			AUDIO_NOTIFIER_SERVICE_UP
 *		*data - pointer to struct audio_notifier_cb_data
 *
 * Returns:	Success: 0
 *		Error: -#
 */
int audio_notifier_register(char *client_name, int domain,
			    struct notifier_block *nb);

/*
 * Use audio_notifier_deregister to deregister the clients from
 * all domains registered using audio_notifier_register that
 * match the client name.
 *
 * client_name - Unique user name used in audio_notifier_register.
 * Returns:	Success: 0
 *		Error: -#
 */
int audio_notifier_deregister(char *client_name);

#else

static inline int audio_notifier_register(char *client_name, int domain,
					  struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int audio_notifier_deregister(char *client_name)
{
	return 0;
}

#endif /* CONFIG_MSM_QDSP6_NOTIFIER */

#endif
