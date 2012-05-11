/* drivers/tty/smux_loopback.h
 *
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef SMUX_LOOPBACK_H
#define SMUX_LOOPBACK_H

#include "smux_private.h"

#ifdef CONFIG_N_SMUX_LOOPBACK

int smux_loopback_init(void);
int smux_tx_loopback(struct smux_pkt_t *pkt_ptr);

#else
static inline int smux_loopback_init(void)
{
	return 0;
}

static inline int smux_tx_loopback(struct smux_pkt_t *pkt_ptr)
{
	return -ENODEV;
}


#endif /* CONFIG_N_SMUX_LOOPBACK */
#endif /* SMUX_LOOPBACK_H */

