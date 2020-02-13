/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#ifndef	_DWMAC_QCOM_ETH_IPA_OFFLOAD_H
#define	_DWMAC_QCOM_ETH_IPA_OFFLOAD_H

#define IPA_DMA_RX_CH 0
#define IPA_DMA_TX_CH 0

#define ALL_OTHER_TX_TRAFFIC_IPA_DISABLED 0
#define ALL_OTHER_TRAFFIC_TX_CHANNEL 1

#define QTAG_VLAN_ETH_TYPE_OFFSET 16
#define QTAG_UCP_FIELD_OFFSET 14
#define QTAG_ETH_TYPE_OFFSET 12
#define PTP_UDP_EV_PORT 0x013F
#define PTP_UDP_GEN_PORT 0x0140

#define GET_ETH_TYPE(buf, ethtype) do { \
	unsigned char *buf1 = buf;\
	ethtype = (((((u16)buf1[QTAG_ETH_TYPE_OFFSET] << 8) | \
		   buf1[QTAG_ETH_TYPE_OFFSET + 1]) == ETH_P_8021Q) ? \
		   (((u16)buf1[QTAG_VLAN_ETH_TYPE_OFFSET] << 8) | \
		   buf1[QTAG_VLAN_ETH_TYPE_OFFSET + 1]) : \
		   (((u16)buf1[QTAG_ETH_TYPE_OFFSET] << 8) | \
		    buf1[QTAG_ETH_TYPE_OFFSET + 1]));\
} while (0)


#ifdef CONFIG_ETH_IPA_OFFLOAD
void ethqos_ipa_offload_event_handler(void *data, int ev);
#else
static inline void ethqos_ipa_offload_event_handler(void *data, int ev)
{
}
#endif

#define EV_INVALID 0
#define EV_PROBE_INIT (EV_INVALID + 1)
#define EV_IPA_ENABLED (EV_PROBE_INIT + 1)
#define EV_DEV_OPEN (EV_IPA_ENABLED + 1)
#define EV_DEV_CLOSE (EV_DEV_OPEN + 1)
#define EV_IPA_READY (EV_DEV_CLOSE + 1)
#define EV_IPA_UC_READY (EV_IPA_READY + 1)
#define EV_PHY_LINK_UP (EV_IPA_UC_READY + 1)
#define EV_PHY_LINK_DOWN (EV_PHY_LINK_UP + 1)
#define EV_DPM_SUSPEND (EV_PHY_LINK_DOWN + 1)
#define EV_DPM_RESUME (EV_DPM_SUSPEND + 1)
#define EV_USR_SUSPEND (EV_DPM_RESUME + 1)
#define EV_USR_RESUME (EV_USR_SUSPEND + 1)
#define EV_IPA_OFFLOAD_REMOVE (EV_USR_RESUME + 1)
#define EV_IPA_OFFLOAD_MAX (EV_IPA_OFFLOAD_REMOVE + 1)

#endif
