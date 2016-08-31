/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * arch/arm/mach-tegra/therm_monitor.h
 *
 */

#ifndef _MACH_TEGRA_THERM_MONITOR_H
#define _MACH_TEGRA_THERM_MONITOR_H


#define MAX_NUM_TEMPERAT 10
#define INVALID_ADDR 0xffffffff

#define UTMIP_XCVR_CFG0			0x808
#define UTMIP_SPARE_CFG0		0x834
#define FUSE_SETUP_SEL			(1 << 3)
#define UTMIP_XCVR_SETUP_MSK		0xF
#define UTMIP_XCVR_HSSLEW_MSB_MSK	(0x7F << 25)
#define FUSE_USB_CALIB_0		0x1F0
#define UTMIP_XCVR_SETUP_MAX_VAL	0xF
#define UTMIP_XCVR_HSSLEW_MSB_BIT_OFFS	25
/* Remote Temp < rtemp_boundary. */
#define UTMIP_XCVR_HSSLEW_MSB_LOW_TEMP_VAL \
	(0x8 << UTMIP_XCVR_HSSLEW_MSB_BIT_OFFS)
/* Remote Temp >= rtemp_boundary. */
#define UTMIP_XCVR_HSSLEW_MSB_HIGH_TEMP_VAL \
	(0x2 << UTMIP_XCVR_HSSLEW_MSB_BIT_OFFS)


struct therm_monitor_ldep_data {
	unsigned int reg_addr;
	int temperat[MAX_NUM_TEMPERAT];
	unsigned int value[MAX_NUM_TEMPERAT - 1];
	int previous_val;
};

struct therm_monitor_data {
	struct therm_monitor_ldep_data *brd_ltemp_reg_data;
	unsigned int delta_temp;
	unsigned int delta_time;
	unsigned int remote_offset;
	int utmip_temp_bound;
	unsigned char local_temp_update;
	unsigned char utmip_reg_update;
	unsigned char i2c_bus_num;
	unsigned int i2c_dev_addrs;
	unsigned char *i2c_dev_name;
	unsigned char i2c_board_size;
};

void register_therm_monitor(struct therm_monitor_data *brd_therm_monitor_data);

#endif
