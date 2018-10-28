/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _RNDIS_IPA_H_
#define _RNDIS_IPA_H_

#include <linux/ipa.h>

/*
 * @priv: private data given upon ipa_connect
 * @evt: event enum, should be IPA_WRITE_DONE
 * @data: for tx path the data field is the sent socket buffer.
 */
typedef void (*ipa_callback)(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data);

/*
 * struct ipa_usb_init_params - parameters for driver initialization API
 *
 * @device_ready_notify: callback supplied by USB core driver
 * This callback shall be called by the Netdev once the device
 * is ready to receive data from tethered PC.
 * @ipa_rx_notify: The network driver will set this callback (out parameter).
 * this callback shall be supplied for ipa_connect upon pipe
 * connection (USB->IPA), once IPA driver receive data packets
 * from USB pipe destined for Apps this callback will be called.
 * @ipa_tx_notify: The network driver will set this callback (out parameter).
 * this callback shall be supplied for ipa_connect upon pipe
 * connection (IPA->USB), once IPA driver send packets destined
 * for USB, IPA BAM will notify for Tx-complete.
 * @host_ethaddr: host Ethernet address in network order
 * @device_ethaddr: device Ethernet address in network order
 * @private: The network driver will set this pointer (out parameter).
 * This pointer will hold the network device for later interaction
 * with between USB driver and the network driver.
 * @skip_ep_cfg: boolean field that determines if Apps-processor
 *  should or should not configure this end-point.
 */
struct ipa_usb_init_params {
	void (*device_ready_notify)(void);
	ipa_callback ipa_rx_notify;
	ipa_callback ipa_tx_notify;
	u8 host_ethaddr[ETH_ALEN];
	u8 device_ethaddr[ETH_ALEN];
	void *private;
	bool skip_ep_cfg;
};

#ifdef CONFIG_RNDIS_IPA

int rndis_ipa_init(struct ipa_usb_init_params *params);

int rndis_ipa_pipe_connect_notify(u32 usb_to_ipa_hdl,
			u32 ipa_to_usb_hdl,
			u32 max_xfer_size_bytes_to_dev,
			u32 max_packet_number_to_dev,
			u32 max_xfer_size_bytes_to_host,
			void *private);

int rndis_ipa_pipe_disconnect_notify(void *private);

void rndis_ipa_cleanup(void *private);

#else /* CONFIG_RNDIS_IPA*/

static inline int rndis_ipa_init(struct ipa_usb_init_params *params)
{
	return -ENOMEM;
}

static inline int rndis_ipa_pipe_connect_notify(u32 usb_to_ipa_hdl,
			u32 ipa_to_usb_hdl,
			u32 max_xfer_size_bytes_to_dev,
			u32 max_packet_number_to_dev,
			u32 max_xfer_size_bytes_to_host,
			void *private)
{
	return -ENOMEM;
}

static inline int rndis_ipa_pipe_disconnect_notify(void *private)
{
	return -ENOMEM;
}

static inline void rndis_ipa_cleanup(void *private)
{

}
#endif /* CONFIG_RNDIS_IPA */

#endif /* _RNDIS_IPA_H_ */
