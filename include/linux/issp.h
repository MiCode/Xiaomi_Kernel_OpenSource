/*
 * Copyright (c) 2006-2013, Cypress Semiconductor Corporation
 * All rights reserved.
 *
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
 */

#ifndef _ISSP_H_
#define _ISSP_H_

#include <linux/kernel.h>

struct issp_platform_data {
	int reset_gpio;
	int data_gpio;
	int clk_gpio;
	char *fw_name;
	uint8_t si_id[4];
	int block_size;
	int blocks;
	int security_size;
	int version_addr;
	int force_update;
};

#endif
