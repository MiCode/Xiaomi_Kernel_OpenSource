/*
 * Copyright (C) 2009 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_BCM_BT_LPM_H
#define __ASM_ARCH_BCM_BT_LPM_H

#include <linux/serial_core.h>

/* Uart driver must call this every time it beings TX, to ensure
 * this driver keeps WAKE asserted during TX. Called with uart
 * spinlock held. */
extern void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

struct bcm_bt_lpm_platform_data {
	unsigned int gpio_wake;   /* CPU -> BCM wakeup gpio */
	unsigned int gpio_host_wake;  /* BCM -> CPU wakeup gpio */

	/* Callback to request the uart driver to clock off.
         * Called with uart spinlock held. */
	void (*request_clock_off_locked)(struct uart_port *uport);
	/* Callback to request the uart driver to clock on.
         * Called with uart spinlock held. */
	void (*request_clock_on_locked)(struct uart_port *uport);
};

#endif
