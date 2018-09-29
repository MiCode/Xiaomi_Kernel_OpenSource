/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef __AUDIO_PDR_H_
#define __AUDIO_PDR_H_

enum {
	AUDIO_PDR_DOMAIN_ADSP,
	AUDIO_PDR_DOMAIN_MAX
};

enum {
	AUDIO_PDR_FRAMEWORK_DOWN,
	AUDIO_PDR_FRAMEWORK_UP
};

#ifdef CONFIG_MSM_QDSP6_PDR

/*
 * Use audio_pdr_register to register with the PDR subsystem this
 * should be done before module late init otherwise notification
 * of the AUDIO_PDR_FRAMEWORK_UP cannot be guaranteed.
 *
 * *nb - Pointer to a notifier block. Provide a callback function
 *       to be notified once the PDR framework has been initialized.
 *       Callback will receive either the AUDIO_PDR_FRAMEWORK_DOWN
 *       or AUDIO_PDR_FRAMEWORK_UP ioctl depending on the state of
 *       the PDR framework.
 *
 * Returns: Success: 0
 *          Failure: Error code
 */
int audio_pdr_register(struct notifier_block *nb);

/*
 * Use audio_pdr_service_register to register with a PDR service
 * Function should be called after nb callback registered with
 * audio_pdr_register has been called back with the
 * AUDIO_PDR_FRAMEWORK_UP ioctl.
 *
 * domain_id - Domain to use, example: AUDIO_PDR_ADSP
 * *nb - Pointer to a notifier block. Provide a callback function
 *       that will be notified of the state of the domain
 *       requested. The ioctls received by the callback are
 *       defined in service-notifier.h.
 *
 * Returns: Success: Client handle
 *          Failure: Pointer error code
 */
void *audio_pdr_service_register(int domain_id,
				 struct notifier_block *nb, int *curr_state);

/*
 * Use audio_pdr_service_deregister to deregister with a PDR
 * service that was registered using the audio_pdr_service_register
 * API.
 *
 * *service_handle - Service handle returned by audio_pdr_service_register
 * *nb - Pointer to the notifier block. Used in the call to
 *       audio_pdr_service_register.
 *
 * Returns: Success: Client handle
 *          Failure: Error code
 */
int audio_pdr_service_deregister(void *service_handle,
				 struct notifier_block *nb);

#else

static inline int audio_pdr_register(struct notifier_block *nb)
{
	return -ENODEV;
}


static inline void *audio_pdr_service_register(int domain_id,
					       struct notifier_block *nb,
					       int *curr_state)
{
	return NULL;
}

static inline int audio_pdr_service_deregister(void *service_handle,
					       struct notifier_block *nb)
{
	return 0;
}

#endif /* CONFIG_MSM_QDSP6_PDR */

#endif
