/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_DBL_H
#define __HH_DBL_H

#include "hh_common.h"

typedef void (*dbl_rx_cb_t)(int irq, void *priv_data);

enum hh_dbl_label {
	HH_DBL_TUI_LABEL,
	HH_DBL_TUI_NEURON_BLK0,
	HH_DBL_TUI_NEURON_BLK1,
	HH_DBL_TUI_QRTR,
	HH_DBL_LABEL_MAX
};

void *hh_dbl_tx_register(enum hh_dbl_label label);
void *hh_dbl_rx_register(enum hh_dbl_label label, dbl_rx_cb_t rx_cb,
			 void *priv);

int hh_dbl_tx_unregister(void *dbl_client_desc);
int hh_dbl_rx_unregister(void *dbl_client_desc);

int hh_dbl_send(void *dbl_client_desc, uint64_t *newflags);
int hh_dbl_set_mask(void *dbl_client_desc, hh_dbl_flags_t enable_mask,
		    hh_dbl_flags_t ack_mask);
int hh_dbl_read_and_clean(void *dbl_client_desc, hh_dbl_flags_t *clear_flags);
int hh_dbl_reset(void *dbl_client_desc);
int hh_dbl_populate_cap_info(enum hh_dbl_label label, u64 cap_id,
						int direction, int rx_irq);

#endif
