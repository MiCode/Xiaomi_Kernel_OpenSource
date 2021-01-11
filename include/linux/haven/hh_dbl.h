/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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

/* Possible flags to pass for send, set_mask, read, reset */
#define HH_DBL_NONBLOCK		BIT(32)

#if IS_ENABLED(CONFIG_HH_DBL)
void *hh_dbl_tx_register(enum hh_dbl_label label);
void *hh_dbl_rx_register(enum hh_dbl_label label, dbl_rx_cb_t rx_cb,
			 void *priv);

int hh_dbl_tx_unregister(void *dbl_client_desc);
int hh_dbl_rx_unregister(void *dbl_client_desc);

int hh_dbl_send(void *dbl_client_desc, uint64_t *newflags,
		const unsigned long flags);
int hh_dbl_set_mask(void *dbl_client_desc, hh_dbl_flags_t enable_mask,
		    hh_dbl_flags_t ack_mask, const unsigned long flags);
int hh_dbl_read_and_clean(void *dbl_client_desc, hh_dbl_flags_t *clear_flags,
			  const unsigned long flags);
int hh_dbl_reset(void *dbl_client_desc, const unsigned long flags);
int hh_dbl_populate_cap_info(enum hh_dbl_label label, u64 cap_id,
						int direction, int rx_irq);
#else
static inline void *hh_dbl_tx_register(enum hh_dbl_label label)
{
	return ERR_PTR(-ENODEV);
}

static inline void *hh_dbl_rx_register(enum hh_dbl_label label,
			 dbl_rx_cb_t rx_cb,
			 void *priv)
{
	return ERR_PTR(-ENODEV);
}

static inline int hh_dbl_tx_unregister(void *dbl_client_desc)
{
	return -EINVAL;
}

static inline int hh_dbl_rx_unregister(void *dbl_client_desc)
{
	return -EINVAL;
}

static inline int hh_dbl_send(void *dbl_client_desc, uint64_t *newflags,
			      const unsigned long flags)
{
	return -EINVAL;
}

static inline int hh_dbl_set_mask(void *dbl_client_desc,
		    hh_dbl_flags_t enable_mask,
		    hh_dbl_flags_t ack_mask,
		    const unsigned long flags)
{
	return -EINVAL;
}

static inline int hh_dbl_read_and_clean(void *dbl_client_desc,
					hh_dbl_flags_t *clear_flags,
					const unsigned long flags)
{
	return -EINVAL;
}

static inline int hh_dbl_reset(void *dbl_client_desc, const unsigned long flags)
{
	return -EINVAL;
}

static inline int hh_dbl_populate_cap_info(enum hh_dbl_label label, u64 cap_id,
						int direction, int rx_irq)
{
	return -EINVAL;
}
#endif
#endif
