/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2014-2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef ATL_PTP_H
#define ATL_PTP_H

#include <linux/net_tstamp.h>

#include "atl_compat.h"

struct atl_nic;
struct atl_queue_vec;

/* Common functions */
int atl_ptp_init(struct atl_nic *nic);
int atl_ptp_register(struct atl_nic *nic);

void atl_ptp_unregister(struct atl_nic *nic);
void atl_ptp_free(struct atl_nic *nic);

int atl_ptp_irq_alloc(struct atl_nic *nic);
void atl_ptp_irq_free(struct atl_nic *nic);

int atl_ptp_ring_alloc(struct atl_nic *nic);
void atl_ptp_ring_free(struct atl_nic *nic);

int atl_ptp_ring_start(struct atl_nic *nic);
void atl_ptp_ring_stop(struct atl_nic *nic);

void atl_ptp_work(struct atl_nic *nic);

void atl_ptp_tm_offset_set(struct atl_nic *nic, unsigned int mbps);

void atl_ptp_clock_init(struct atl_nic *nic);

int atl_ptp_qvec_intr(struct atl_queue_vec *qvec);

/* Traffic processing functions */
netdev_tx_t atl_ptp_start_xmit(struct atl_nic *nic, struct sk_buff *skb);
void atl_ptp_tx_hwtstamp(struct atl_nic *nic, u64 timestamp);

/* Check for PTP availability before calling! */
void atl_ptp_hwtstamp_config_get(struct atl_nic *nic,
				 struct hwtstamp_config *config);
int atl_ptp_hwtstamp_config_set(struct atl_nic *nic,
				struct hwtstamp_config *config);

/* Return whether ring belongs to PTP or not*/
bool atl_is_ptp_ring(struct atl_nic *nic, struct atl_desc_ring *ring);
u16 atl_ptp_extract_ts(struct atl_nic *nic, struct sk_buff *skb, u8 *p,
		       unsigned int len);

struct ptp_clock *atl_ptp_get_ptp_clock(struct atl_nic *nic);

int atl_ptp_link_change(struct atl_nic *nic);

#endif /* ATL_PTP_H */
