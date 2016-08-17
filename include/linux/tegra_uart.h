/* include/linux/tegra_uart.h
 *
 * Copyright (C) 2011-2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _TEGRA_UART_H_
#define _TEGRA_UART_H_

#include <linux/clk.h>

struct uart_clk_parent {
	const char	*name;
	struct clk	*parent_clk;
	unsigned long	fixed_clk_rate;
};

struct tegra_uart_platform_data {
	void (*wake_peer)(struct uart_port *);
	struct uart_clk_parent *parent_clk_list;
	int parent_clk_count;
	bool is_loopback;
	bool is_irda;
	int (*irda_init)(void);
	int (*irda_mode_switch)(int);
	void (*irda_start)(void);
	void (*irda_shutdown)(void);
	void (*irda_remove)(void);
};

int tegra_uart_is_tx_empty(struct uart_port *);
void tegra_uart_request_clock_on(struct uart_port *);
void tegra_uart_set_mctrl(struct uart_port *, unsigned int);
void tegra_uart_request_clock_off(struct uart_port *uport);

#endif /* _TEGRA_UART_H_ */

