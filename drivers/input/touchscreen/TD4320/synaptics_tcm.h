/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SYNAPTICS_TCM_H_
#define _SYNAPTICS_TCM_H_

#define I2C_MODULE_NAME "synaptics_tcm_i2c"
#define SPI_MODULE_NAME "synaptics_tcm_spi"

struct syna_tcm_board_data {
	bool x_flip;
	bool y_flip;
	bool swap_axes;
	int irq_gpio;
	int irq_on_state;
	int power_gpio;
	int power_on_state;
	int reset_gpio;
	int reset_on_state;
	unsigned int spi_mode;
	unsigned int power_delay_ms;
	unsigned int reset_delay_ms;
	unsigned int reset_active_ms;
	unsigned int byte_delay_us;
	unsigned int block_delay_us;
	unsigned int ubl_i2c_addr;
	unsigned int ubl_max_freq;
	unsigned int ubl_byte_delay_us;
	unsigned int max_x;
	unsigned int max_y;
	unsigned long irq_flags;
	const char *pwr_reg_name;
	const char *bus_reg_name;
};

#endif
