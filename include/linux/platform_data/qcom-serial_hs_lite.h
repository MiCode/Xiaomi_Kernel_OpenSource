/* Copyright (c) 2011-2013 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MSM_SERIAL_HS_LITE_H
#define __ASM_ARCH_MSM_SERIAL_HS_LITE_H
/**
 * struct msm_serial_hslite_platform_data - platform device data
 *              for msm_hs_lite.
 * @config_gpio: Select GPIOs to configure.
 *		Set 4 if 4-wire UART used (for Tx, Rx, CTS, RFR GPIOs).
 *		Set 1 if 2-wire UART used (for Tx, Rx GPIOs).
 * @uart_tx_gpio: GPIO number for UART Tx Line.
 * @uart_rx_gpio: GPIO number for UART Rx Line.
 * @uart_cts_gpio: GPIO number for UART CTS Line.
 * @uart_rfr_gpio: GPIO number for UART RFR Line.
 * @set_uart_clk_zero: use this if setting UART Clock to zero is required
 * It is mainly required where same UART is used across different processor.
 * Make sure that Clock driver for platform support setting clock rate to zero.
 * @use_pm: use this to enable power management
 * @line: Used to set UART Port number.
 */
struct msm_serial_hslite_platform_data {
	unsigned config_gpio;
	unsigned uart_tx_gpio;
	unsigned uart_rx_gpio;
	unsigned uart_cts_gpio;
	unsigned uart_rfr_gpio;
	bool set_uart_clk_zero;
	bool use_pm;
	int line;
};

#endif

