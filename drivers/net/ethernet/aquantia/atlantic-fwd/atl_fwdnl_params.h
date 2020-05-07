/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_FWDNL_PARAMS_H_
#define _ATL_FWDNL_PARAMS_H_

extern unsigned int atlfwd_nl_tx_clean_threshold_msec;
extern unsigned int atlfwd_nl_tx_clean_threshold_frac;
extern bool atlfwd_nl_speculative_queue_stop;
extern unsigned int altfwd_nl_rx_poll_interval_msec;
extern int atlfwd_nl_rx_clean_budget;

#endif
