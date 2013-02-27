/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ECM_IPA_H_
#define _ECM_IPA_H_

#include <mach/ipa.h>

/*
 * @priv: private data given upon ipa_connect
 * @evt: event enum, should be IPA_WRITE_DONE
 * @data: for tx path the data field is the sent socket buffer.
 */
typedef void (*ecm_ipa_callback)(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data);


#ifdef CONFIG_ECM_IPA

int ecm_ipa_init(ecm_ipa_callback * ecm_ipa_rx_dp_notify,
		ecm_ipa_callback * ecm_ipa_tx_dp_notify,
		void **priv);

int ecm_ipa_configure(u8 host_ethaddr[], u8 device_ethaddr[],
		void *priv);

int ecm_ipa_connect(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl,
		void *priv);

int ecm_ipa_disconnect(void *priv);

void ecm_ipa_cleanup(void *priv);

#else /* CONFIG_ECM_IPA*/

static inline int ecm_ipa_init(ecm_ipa_callback *ecm_ipa_rx_dp_notify,
		ecm_ipa_callback *ecm_ipa_tx_dp_notify,
		void **priv)
{
	return 0;
}

static inline int ecm_ipa_configure(u8 host_ethaddr[], u8 device_ethaddr[],
		void *priv)
{
	return 0;
}

static inline int ecm_ipa_connect(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl,
		void *priv)
{
	return 0;
}

static inline int ecm_ipa_disconnect(void *priv)
{
	return 0;
}

static inline void ecm_ipa_cleanup(void *priv)
{

}
#endif /* CONFIG_ECM_IPA*/

#endif /* _ECM_IPA_H_ */
