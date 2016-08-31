/*
 * drivers/i2c/busses/i2c-tegra.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation
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

#ifndef _LINUX_I2C_TEGRA_H
#define _LINUX_I2C_TEGRA_H

struct tegra_i2c_platform_data {
	unsigned long bus_clk_rate;
	bool is_dvc;
	bool is_clkon_always;
	int retries;
	int timeout;	/* in jiffies */
	u16 slave_addr;
	int scl_gpio;
	int sda_gpio;
	bool is_high_speed_enable;
	u16 hs_master_code;
	bool needs_cl_dvfs_clock;
	bool bit_banging_xfer_after_shutdown;
};

struct tegra_i2c_slave_platform_data {
	int adapter_nr;
	const struct tegra_pingroup_config *pinmux;
	int bus_mux_len;
	unsigned long bus_clk_rate;
	int max_rx_buffer_size;
	int max_tx_buffer_size;
};

#endif /* _LINUX_I2C_TEGRA_H */
