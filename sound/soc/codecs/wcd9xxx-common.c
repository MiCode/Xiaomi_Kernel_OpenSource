/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include "wcd9xxx-common.h"

#define CLSH_COMPUTE_EAR 0x01
#define CLSH_COMPUTE_HPH_L 0x02
#define CLSH_COMPUTE_HPH_R 0x03

#define BUCK_VREF_0P494V 0x3F
#define BUCK_VREF_2V 0xFF
#define BUCK_VREF_0P494V 0x3F
#define BUCK_VREF_1P8V 0xE6

#define BUCK_SETTLE_TIME_US 50
#define NCP_SETTLE_TIME_US 50

#define MAX_IMPED_PARAMS 13

#define USLEEP_RANGE_MARGIN_US 100

struct wcd9xxx_imped_val {
	u32 imped_val;
	u8 index;
};

static const struct wcd9xxx_reg_mask_val imped_table[][MAX_IMPED_PARAMS] = {
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x46},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x04},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x11},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x02},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x9B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x02},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x15},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x04},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x0C},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x47},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x05},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x11},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x02},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x9B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x02},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x15},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x05},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x0C},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x49},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x07},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x02},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x12},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x35},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x4E},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x06},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x0E},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x49},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x16},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAC},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x02},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x17},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x5F},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xCF},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x06},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x0F},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x59},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x15},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x9C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x02},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xCE},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xBD},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x07},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x10},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x66},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x04},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x9A},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x02},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2E},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xBD},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xA6},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x07},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x11},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x79},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x04},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x11},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x37},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xA6},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAD},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x08},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x12},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x76},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x04},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x11},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x4E},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAD},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAC},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x09},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x12},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x78},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x05},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x12},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xD0},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAC},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x13},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x0A},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x13},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x7A},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x06},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x14},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xB7},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x13},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x14},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x0B},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x14},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x60},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x09},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xA4},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x14},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1F},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x0C},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x14},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x79},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x17},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x25},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAE},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1F},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1D},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x0D},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x15},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x78},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x16},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAC},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1D},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x0E},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x16},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x89},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x05},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x40},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x13},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x10},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x16},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x97},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x05},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xD0},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x14},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x12},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x17},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x8A},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x06},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xB7},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x10},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x24},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x13},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x17},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x8A},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x07},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xA4},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1D},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x24},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x25},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x15},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x18},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x9A},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x08},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAE},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x25},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x27},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x18},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x19},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x8B},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x18},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAC},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x20},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2E},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x1A},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x19},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x9A},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x17},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x13},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1B},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2E},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2D},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x1D},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x1A},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0xA9},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x06},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x14},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x24},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2D},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x1F},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x19},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0xB9},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x06},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x10},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x25},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x23},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x18},
	},
	{
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0xA9},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x07},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1D},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x27},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x35},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0xff, 0x26},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0xff, 0x16},
	},
};

static const struct wcd9xxx_imped_val imped_index[] = {
	{4000, 0},
	{4500, 1},
	{5000, 2},
	{5500, 3},
	{6000, 4},
	{6500, 5},
	{7000, 6},
	{7700, 7},
	{8470, 8},
	{9317, 9},
	{10248, 10},
	{11273, 11},
	{12400, 12},
	{13641, 13},
	{15005, 14},
	{16505, 15},
	{18156, 16},
	{19971, 17},
	{21969, 18},
	{24165, 19},
	{26582, 20},
	{29240, 21},
	{32164, 22},
};

static inline void
wcd9xxx_enable_clsh_block(struct snd_soc_codec *codec,
			  struct wcd9xxx_clsh_cdc_data *clsh_d, bool enable)
{
	if ((enable && ++clsh_d->clsh_users == 1) ||
	    (!enable && --clsh_d->clsh_users == 0))
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_B1_CTL,
				    0x01, enable ? 0x01 : 0x00);
	dev_dbg(codec->dev, "%s: clsh_users %d, enable %d", __func__,
		clsh_d->clsh_users, enable);
}

static inline void wcd9xxx_enable_anc_delay(
	struct snd_soc_codec *codec,
	bool on)
{
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_B1_CTL,
		0x02, on ? 0x02 : 0x00);
}

static inline void
wcd9xxx_enable_buck(struct snd_soc_codec *codec,
		    struct wcd9xxx_clsh_cdc_data *clsh_d, bool enable)
{
	if ((enable && ++clsh_d->buck_users == 1) ||
	    (!enable && --clsh_d->buck_users == 0))
		snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_1,
				    0x80, enable ? 0x80 : 0x00);
	dev_dbg(codec->dev, "%s: buck_users %d, enable %d", __func__,
		clsh_d->buck_users, enable);
}

static void (*clsh_state_fp[NUM_CLSH_STATES])(struct snd_soc_codec *,
					      struct wcd9xxx_clsh_cdc_data *,
					      u8 req_state, bool req_type);

static const char *state_to_str(u8 state, char *buf, size_t buflen)
{
	int i;
	int cnt = 0;
	/*
	 * This array of strings should match with enum wcd9xxx_clsh_state_bit.
	 */
	const char *states[] = {
		"STATE_EAR",
		"STATE_HPH_L",
		"STATE_HPH_R",
		"STATE_LO",
	};

	if (state == WCD9XXX_CLSH_STATE_IDLE) {
		snprintf(buf, buflen, "[STATE_IDLE]");
		goto done;
	}

	buf[0] = '\0';
	for (i = 0; i < ARRAY_SIZE(states); i++) {
		if (!(state & (1 << i)))
			continue;
		cnt = snprintf(buf, buflen - cnt - 1, "%s%s%s", buf,
			       buf[0] == '\0' ? "[" : "|",
			       states[i]);
	}
	if (cnt > 0)
		strlcat(buf + cnt, "]", buflen);

done:
	if (buf[0] == '\0')
		snprintf(buf, buflen, "[STATE_UNKNOWN]");
	return buf;
}

static void wcd9xxx_cfg_clsh_param_common(
		struct snd_soc_codec *codec)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_CDC_CLSH_BUCK_NCP_VARS, 0x3 << 0, 0},
		{WCD9XXX_A_CDC_CLSH_BUCK_NCP_VARS, 0x3 << 2, 1 << 2},
		{WCD9XXX_A_CDC_CLSH_BUCK_NCP_VARS, (0x1 << 4), 0},
		{WCD9XXX_A_CDC_CLSH_B2_CTL, (0x3 << 0), 0x01},
		{WCD9XXX_A_CDC_CLSH_B2_CTL, (0x3 << 2), (0x01 << 2)},
		{WCD9XXX_A_CDC_CLSH_B2_CTL, (0xf << 4), (0x03 << 4)},
		{WCD9XXX_A_CDC_CLSH_B3_CTL, (0xf << 4), (0x03 << 4)},
		{WCD9XXX_A_CDC_CLSH_B3_CTL, (0xf << 0), (0x0B)},
		{WCD9XXX_A_CDC_CLSH_B1_CTL, (0x1 << 5), (0x01 << 5)},
		{WCD9XXX_A_CDC_CLSH_B1_CTL, (0x1 << 1), (0x01 << 1)},
	};

	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(codec, reg_set[i].reg, reg_set[i].mask,
						    reg_set[i].val);

	dev_dbg(codec->dev, "%s: Programmed class H controller common parameters",
			 __func__);
}

static void wcd9xxx_chargepump_request(struct snd_soc_codec *codec, bool on)
{
	static int cp_count;

	if (on && (++cp_count == 1)) {
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLK_OTHR_CTL,
				    0x01, 0x01);
		dev_dbg(codec->dev, "%s: Charge Pump enabled, count = %d\n",
			__func__, cp_count);
	} else if (!on) {
		if (--cp_count < 0) {
			dev_dbg(codec->dev,
				"%s: Unbalanced disable for charge pump\n",
				__func__);
			if (snd_soc_read(codec, WCD9XXX_A_CDC_CLK_OTHR_CTL) &
			    0x01) {
				dev_dbg(codec->dev,
					"%s: Actual chargepump is ON\n",
					__func__);
			}
			cp_count = 0;
			WARN_ON(1);
		}

		if (cp_count == 0) {
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLK_OTHR_CTL,
					    0x01, 0x00);
			dev_dbg(codec->dev,
				"%s: Charge pump disabled, count = %d\n",
				__func__, cp_count);
		}
	}
}

void wcd9xxx_enable_high_perf_mode(struct snd_soc_codec *codec,
				struct wcd9xxx_clsh_cdc_data *clsh_d,
				u8 uhqa_mode, u8 req_state, bool req_type)
{
	dev_dbg(codec->dev, "%s: users fclk8 %d, fclk5 %d", __func__,
			clsh_d->ncp_users[NCP_FCLK_LEVEL_8],
			clsh_d->ncp_users[NCP_FCLK_LEVEL_5]);

	if (req_type == WCD9XXX_CLSAB_REQ_ENABLE) {
		clsh_d->ncp_users[NCP_FCLK_LEVEL_8]++;
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_BIAS_PA,
					WCD9XXX_A_RX_HPH_BIAS_PA__POR);
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_L_PA_CTL, 0x48);
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_R_PA_CTL, 0x48);
		if (uhqa_mode)
			snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_CHOP_CTL,
						0x20, 0x00);
		wcd9xxx_chargepump_request(codec, true);
		wcd9xxx_enable_anc_delay(codec, true);
		wcd9xxx_enable_buck(codec, clsh_d, false);
		if (clsh_d->ncp_users[NCP_FCLK_LEVEL_8] > 0)
			snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC,
						0x0F, 0x08);
		snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC, 0x30, 0x30);

		/* Enable NCP and wait until settles down */
		if (snd_soc_update_bits(codec, WCD9XXX_A_NCP_EN, 0x01, 0x01))
			usleep_range(NCP_SETTLE_TIME_US, NCP_SETTLE_TIME_US+10);
	} else {
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_CHOP_CTL,
					0x20, 0x20);
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_L_PA_CTL,
					WCD9XXX_A_RX_HPH_L_PA_CTL__POR);
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_R_PA_CTL,
					WCD9XXX_A_RX_HPH_R_PA_CTL__POR);
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_BIAS_PA, 0x57);
		wcd9xxx_enable_buck(codec, clsh_d, true);
		wcd9xxx_chargepump_request(codec, false);
		wcd9xxx_enable_anc_delay(codec, false);
		clsh_d->ncp_users[NCP_FCLK_LEVEL_8]--;
		if (clsh_d->ncp_users[NCP_FCLK_LEVEL_8] == 0 &&
		    clsh_d->ncp_users[NCP_FCLK_LEVEL_5] == 0)
			snd_soc_update_bits(codec, WCD9XXX_A_NCP_EN,
						0x01, 0x00);
		else if (clsh_d->ncp_users[NCP_FCLK_LEVEL_8] == 0)
			snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC,
						0x0F, 0x05);
	}
	dev_dbg(codec->dev, "%s: leave\n", __func__);
}
EXPORT_SYMBOL(wcd9xxx_enable_high_perf_mode);

static int get_impedance_index(u32 imped)
{
	int i = 0;
	if (imped < imped_index[i].imped_val) {
		pr_debug("%s, detected impedance is less than 4 Ohm\n",
				__func__);
		goto ret;
	}
	if (imped >= imped_index[ARRAY_SIZE(imped_index) - 1].imped_val) {
		pr_debug("%s, detected impedance is greater than 32164 Ohm\n",
				__func__);
		i = ARRAY_SIZE(imped_index) - 1;
		goto ret;
	}
	for (i = 0; i < ARRAY_SIZE(imped_index) - 1; i++) {
		if (imped >= imped_index[i].imped_val &&
			imped < imped_index[i + 1].imped_val)
			break;
	}
ret:
	pr_debug("%s: selected impedance index = %d\n",
			__func__, imped_index[i].index);
	return imped_index[i].index;
}

void wcd9xxx_clsh_imped_config(struct snd_soc_codec *codec,
				  int imped)
{
	int i  = 0;
	int index = 0;
	index = get_impedance_index(imped);
	if (index >= ARRAY_SIZE(imped_index)) {
		pr_err("%s, invalid imped = %d\n", __func__, imped);
		return;
	}
	for (i = 0; i < MAX_IMPED_PARAMS; i++)
		snd_soc_write(codec, imped_table[index][i].reg,
					imped_table[index][i].val);
}

static void wcd9xxx_clsh_comp_req(struct snd_soc_codec *codec,
				  struct wcd9xxx_clsh_cdc_data *clsh_d,
				  int compute_pa, bool on)
{
	u8 shift;

	if (compute_pa == CLSH_COMPUTE_EAR) {
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_B1_CTL, 0x10,
				    (on ? 0x10 : 0));
	} else {
		if (compute_pa == CLSH_COMPUTE_HPH_L) {
			shift = 3;
		} else if (compute_pa == CLSH_COMPUTE_HPH_R) {
			shift = 2;
		} else {
			dev_dbg(codec->dev,
				"%s: classh computation request is incorrect\n",
				__func__);
			return;
		}

		if (on)
			wcd9xxx_resmgr_add_cond_update_bits(clsh_d->resmgr,
						  WCD9XXX_COND_HPH,
						  WCD9XXX_A_CDC_CLSH_B1_CTL,
						  shift, false);
		else
			wcd9xxx_resmgr_rm_cond_update_bits(clsh_d->resmgr,
						  WCD9XXX_COND_HPH,
						  WCD9XXX_A_CDC_CLSH_B1_CTL,
						  shift, false);
	}
}

int wcd9xxx_soc_update_bits_push(struct snd_soc_codec *codec,
					struct list_head *list,
					uint16_t reg, uint8_t mask,
					uint8_t value, int delay)
{
	int rc;
	struct wcd9xxx_register_save_node *node;

	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (unlikely(!node)) {
		pr_err("%s: Not enough memory\n", __func__);
		return -ENOMEM;
	}
	node->reg = reg;
	node->value = snd_soc_read(codec, reg);
	list_add(&node->lh, list);
	if (mask == 0xFF)
		rc = snd_soc_write(codec, reg, value);
	else
		rc = snd_soc_update_bits(codec, reg, mask, value);
	if (delay)
		usleep_range(delay, delay + USLEEP_RANGE_MARGIN_US);
	return rc;
}
EXPORT_SYMBOL(wcd9xxx_soc_update_bits_push);

void wcd9xxx_restore_registers(struct snd_soc_codec *codec,
			       struct list_head *lh)
{
	struct wcd9xxx_register_save_node *node, *nodetmp;

	list_for_each_entry_safe(node, nodetmp, lh, lh) {
		snd_soc_write(codec, node->reg, node->value);
		list_del(&node->lh);
		kfree(node);
	}
}
EXPORT_SYMBOL(wcd9xxx_restore_registers);

static void wcd9xxx_dynamic_bypass_buck_ctrl_lo(struct snd_soc_codec *cdc,
						bool enable)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_BUCK_MODE_3, (0x1 << 3), (enable << 3)},
		{WCD9XXX_A_BUCK_MODE_5, enable ? 0xFF : 0x02, 0x02},
		{WCD9XXX_A_BUCK_MODE_5, 0x1, 0x01}
	};

	if (!enable) {
		snd_soc_update_bits(cdc, WCD9XXX_A_BUCK_MODE_1,
					(0x1 << 3), 0x00);
		snd_soc_update_bits(cdc, WCD9XXX_A_BUCK_MODE_4,
					0xFF, BUCK_VREF_2V);
	}
	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(cdc, reg_set[i].reg, reg_set[i].mask,
							reg_set[i].val);

	/* 50us sleep is reqd. as per the class H HW design sequence */
	usleep_range(BUCK_SETTLE_TIME_US, BUCK_SETTLE_TIME_US+10);
}

static void wcd9xxx_dynamic_bypass_buck_ctrl(struct snd_soc_codec *cdc,
						bool enable)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_BUCK_MODE_3, (0x1 << 3), (enable << 3)},
		{WCD9XXX_A_BUCK_MODE_5, (0x1 << 1), ((!enable) << 1)},
		{WCD9XXX_A_BUCK_MODE_5, 0x1, !enable}
	};
	if (!enable) {
		snd_soc_update_bits(cdc, WCD9XXX_A_BUCK_MODE_1,
					(0x1 << 3), 0x00);
		snd_soc_update_bits(cdc, WCD9XXX_A_BUCK_MODE_4,
					0xFF, BUCK_VREF_2V);
	}
	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(cdc, reg_set[i].reg, reg_set[i].mask,
							reg_set[i].val);

	/* 50us sleep is reqd. as per the class H HW design sequence */
	usleep_range(BUCK_SETTLE_TIME_US, BUCK_SETTLE_TIME_US+10);
}

static void wcd9xxx_set_buck_mode(struct snd_soc_codec *codec, u8 buck_vref)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_BUCK_MODE_5, 0x02, 0x02},
		{WCD9XXX_A_BUCK_MODE_4, 0xFF, buck_vref},
		{WCD9XXX_A_BUCK_MODE_1, 0x04, 0x04},
		{WCD9XXX_A_BUCK_MODE_3, 0x04, 0x00},
		{WCD9XXX_A_BUCK_MODE_3, 0x08, 0x00},
	};

	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(codec, reg_set[i].reg,
					reg_set[i].mask, reg_set[i].val);

	dev_dbg(codec->dev, "%s: Done\n", __func__);
	usleep_range(BUCK_SETTLE_TIME_US, BUCK_SETTLE_TIME_US);
}


/* This will be called for all states except Lineout */
static void wcd9xxx_clsh_enable_post_pa(struct snd_soc_codec *codec,
	struct wcd9xxx_clsh_cdc_data *cdc_clsh_d)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_BUCK_MODE_5, 0x02, 0x00},
		{WCD9XXX_A_NCP_STATIC, 0x20, 0x00},
		{WCD9XXX_A_BUCK_MODE_3, 0x04, 0x04},
	};

	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(codec, reg_set[i].reg,
					reg_set[i].mask, reg_set[i].val);

	if (!cdc_clsh_d->is_dynamic_vdd_cp)
		snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_3,
							0x08, 0x08);

	dev_dbg(codec->dev, "%s: completed clsh mode settings after PA enable\n",
		   __func__);

}

static void wcd9xxx_set_fclk_get_ncp(struct snd_soc_codec *codec,
				     struct wcd9xxx_clsh_cdc_data *clsh_d,
				     enum ncp_fclk_level fclk_level)
{
	clsh_d->ncp_users[fclk_level]++;

	pr_debug("%s: enter ncp type %d users fclk8 %d, fclk5 %d\n", __func__,
		 fclk_level, clsh_d->ncp_users[NCP_FCLK_LEVEL_8],
		 clsh_d->ncp_users[NCP_FCLK_LEVEL_5]);

	snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC, 0x10, 0x00);
	/* fclk level 8 dominates level 5 */
	if (clsh_d->ncp_users[NCP_FCLK_LEVEL_8] > 0)
		snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC, 0x0F, 0x08);
	else if (clsh_d->ncp_users[NCP_FCLK_LEVEL_5] > 0)
		snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC, 0x0F, 0x05);
	else
		WARN_ONCE(1, "Unexpected users %d,%d\n",
			  clsh_d->ncp_users[NCP_FCLK_LEVEL_8],
			  clsh_d->ncp_users[NCP_FCLK_LEVEL_5]);
	snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC, 0x20, 0x20);

	/* enable NCP and wait until settles down */
	if (snd_soc_update_bits(codec, WCD9XXX_A_NCP_EN, 0x01, 0x01))
		usleep_range(NCP_SETTLE_TIME_US, NCP_SETTLE_TIME_US + 50);
	pr_debug("%s: leave\n", __func__);
}

static void wcd9xxx_set_fclk_put_ncp(struct snd_soc_codec *codec,
				     struct wcd9xxx_clsh_cdc_data *clsh_d,
				     enum ncp_fclk_level fclk_level)
{
	clsh_d->ncp_users[fclk_level]--;

	pr_debug("%s: enter ncp type %d users fclk8 %d, fclk5 %d\n", __func__,
		 fclk_level, clsh_d->ncp_users[NCP_FCLK_LEVEL_8],
		 clsh_d->ncp_users[NCP_FCLK_LEVEL_5]);

	if (clsh_d->ncp_users[NCP_FCLK_LEVEL_8] == 0 &&
	    clsh_d->ncp_users[NCP_FCLK_LEVEL_5] == 0)
		snd_soc_update_bits(codec, WCD9XXX_A_NCP_EN, 0x01, 0x00);
	else if (clsh_d->ncp_users[NCP_FCLK_LEVEL_8] == 0)
		/* if dominating level 8 has gone, switch to 5 */
		snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC, 0x0F, 0x05);
	pr_debug("%s: leave\n", __func__);
}

static void wcd9xxx_cfg_clsh_param_ear(struct snd_soc_codec *codec)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_CDC_CLSH_B1_CTL, (0x1 << 7), 0},
		{WCD9XXX_A_CDC_CLSH_V_PA_HD_EAR, (0x3f << 0), 0x0D},
		{WCD9XXX_A_CDC_CLSH_V_PA_MIN_EAR, (0x3f << 0), 0x3A},

		/* Under assumption that EAR load is 10.7ohm */
		{WCD9XXX_A_CDC_CLSH_IDLE_EAR_THSD, (0x3f << 0), 0x26},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_EAR_THSD, (0x3f << 0), 0x2C},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_EAR_L, 0xff, 0xA9},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_EAR_U, 0xff, 0x07},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, (0x1 << 7), 0},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, (0xf << 0), 0x08},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1b},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x2d},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x36},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x37},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
	};

	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(codec, reg_set[i].reg,
					reg_set[i].mask, reg_set[i].val);

	dev_dbg(codec->dev, "%s: Programmed Class H controller EAR specific params\n",
			 __func__);
}

static void wcd9xxx_cfg_clsh_param_hph(struct snd_soc_codec *codec)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_CDC_CLSH_B1_CTL, (0x1 << 6), 0},
		{WCD9XXX_A_CDC_CLSH_V_PA_HD_HPH, 0x3f, 0x0D},
		{WCD9XXX_A_CDC_CLSH_V_PA_MIN_HPH, 0x3f, 0x1D},

		/* Under assumption that HPH load is 16ohm per channel */
		{WCD9XXX_A_CDC_CLSH_IDLE_HPH_THSD, 0x3f, 0x13},
		{WCD9XXX_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0x1f, 0x19},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_L, 0xff, 0x97},
		{WCD9XXX_A_CDC_CLSH_I_PA_FACT_HPH_U, 0xff, 0x05},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, (0x1 << 7), 0},
		{WCD9XXX_A_CDC_CLSH_K_ADDR, 0x0f, 0},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0xAE},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x01},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x1C},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x24},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x25},
		{WCD9XXX_A_CDC_CLSH_K_DATA, 0xff, 0x00},
	};

	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(codec, reg_set[i].reg, reg_set[i].mask,
							reg_set[i].val);
	dev_dbg(codec->dev, "%s: Programmed Class H controller HPH specific params\n",
			 __func__);
}

static void wcd9xxx_ncp_bypass_enable(struct snd_soc_codec *cdc, bool enable)
{
	snd_soc_update_bits(cdc, WCD9XXX_A_NCP_STATIC, 0x10, (enable << 4));
	/* 50us sleep is reqd. as per the class H HW design sequence */
	usleep_range(BUCK_SETTLE_TIME_US, BUCK_SETTLE_TIME_US+10);
}

static void wcd9xxx_clsh_set_Iest(struct snd_soc_codec *codec,
		u8 value)
{
	snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_5,
				    0x01, (0x01 & 0x03));
	snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_5,
				    0xFC, (value << 2));
}

static void wcd9xxx_clsh_state_hph_ear(struct snd_soc_codec *codec,
			struct wcd9xxx_clsh_cdc_data *clsh_d,
			u8 req_state, bool is_enable)
{
	int compute_pa = 0;

	dev_dbg(codec->dev, "%s: enter %s\n", __func__,
			is_enable ? "enable" : "disable");

	if (is_enable) {
		/*
		 * The below check condition is required to make sure
		 * functions inside if condition will execute only once.
		 */
		if (req_state == WCD9XXX_CLSH_STATE_EAR)
			wcd9xxx_cfg_clsh_param_ear(codec);
		if (clsh_d->state == WCD9XXX_CLSH_STATE_EAR)
			wcd9xxx_cfg_clsh_param_hph(codec);

		if ((clsh_d->state == WCD9XXX_CLSH_STATE_EAR) ||
			(req_state == WCD9XXX_CLSH_STATE_EAR)) {
			wcd9xxx_dynamic_bypass_buck_ctrl(codec, false);
			wcd9xxx_ncp_bypass_enable(codec, true);
		}
		switch (req_state) {
		case WCD9XXX_CLSH_STATE_HPHL:
			compute_pa = CLSH_COMPUTE_HPH_L;
			break;
		case WCD9XXX_CLSH_STATE_HPHR:
			compute_pa = CLSH_COMPUTE_HPH_R;
			break;
		case WCD9XXX_CLSH_STATE_EAR:
			compute_pa = CLSH_COMPUTE_EAR;
			break;
		default:
			dev_dbg(codec->dev,
				"%s:Invalid state:0x%x,enable:0x%x\n",
				__func__, req_state, is_enable);
			break;
		}
		wcd9xxx_clsh_comp_req(codec, clsh_d, compute_pa, true);

		dev_dbg(codec->dev, "%s: Enabled hph+ear mode clsh\n",
				__func__);
	} else {
		switch (req_state) {
		case WCD9XXX_CLSH_STATE_HPHL:
			compute_pa = CLSH_COMPUTE_HPH_L;
			break;
		case WCD9XXX_CLSH_STATE_HPHR:
			compute_pa = CLSH_COMPUTE_HPH_R;
			break;
		case WCD9XXX_CLSH_STATE_EAR:
			compute_pa = CLSH_COMPUTE_EAR;
			break;
		default:
			dev_dbg(codec->dev,
				"%s:Invalid state:0x%x,enable:0x%x\n",
				__func__, req_state, is_enable);
			break;
		}
		wcd9xxx_clsh_comp_req(codec, clsh_d, compute_pa, false);

		if (((clsh_d->state & (~req_state)) ==
				WCD9XXX_CLSH_STATE_EAR) ||
			(req_state == WCD9XXX_CLSH_STATE_EAR)) {
			wcd9xxx_ncp_bypass_enable(codec, false);
			wcd9xxx_dynamic_bypass_buck_ctrl(codec, true);
		}
	}
}

static void wcd9xxx_clsh_state_hph_lo(struct snd_soc_codec *codec,
			struct wcd9xxx_clsh_cdc_data *clsh_d,
			u8 req_state, bool is_enable)
{

	dev_dbg(codec->dev, "%s: enter %s\n", __func__,
			is_enable ? "enable" : "disable");
	if (is_enable) {
		if (clsh_d->state == WCD9XXX_CLSH_STATE_LO)
			wcd9xxx_cfg_clsh_param_hph(codec);

		if ((clsh_d->state == WCD9XXX_CLSH_STATE_LO) ||
			(req_state == WCD9XXX_CLSH_STATE_LO)) {
			wcd9xxx_dynamic_bypass_buck_ctrl_lo(codec, false);
			wcd9xxx_enable_buck(codec, clsh_d, true);
			wcd9xxx_set_fclk_get_ncp(codec, clsh_d,
						NCP_FCLK_LEVEL_8);
			if (req_state & WCD9XXX_CLSH_STATE_HPH_ST) {
				wcd9xxx_ncp_bypass_enable(codec, true);
				wcd9xxx_enable_clsh_block(codec, clsh_d, true);
				wcd9xxx_chargepump_request(codec, true);
				wcd9xxx_enable_anc_delay(codec, true);
			}
		}
		if (req_state == WCD9XXX_CLSH_STATE_HPHL)
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_HPH_L, true);
		if (req_state == WCD9XXX_CLSH_STATE_HPHR)
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_HPH_R, true);
	} else {
		switch (req_state) {
		case WCD9XXX_CLSH_STATE_LO:
			snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC,
						0x20, 0x00);
			wcd9xxx_dynamic_bypass_buck_ctrl_lo(codec, true);
			break;
		case WCD9XXX_CLSH_STATE_HPHL:
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_HPH_L, false);
			break;
		case WCD9XXX_CLSH_STATE_HPHR:
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_HPH_R, false);
			break;
		default:
			dev_dbg(codec->dev,
				 "%s:Invalid state:0x%x,enable:0x%x\n",
				__func__, req_state, is_enable);
			break;
		}
		if ((req_state == WCD9XXX_CLSH_STATE_LO) ||
		((clsh_d->state & (~req_state)) == WCD9XXX_CLSH_STATE_LO)) {
			wcd9xxx_set_fclk_put_ncp(codec, clsh_d,
						NCP_FCLK_LEVEL_8);
			wcd9xxx_ncp_bypass_enable(codec, false);

			if (req_state & WCD9XXX_CLSH_STATE_HPH_ST) {
				usleep_range(BUCK_SETTLE_TIME_US,
						BUCK_SETTLE_TIME_US + 10);
				if (clsh_d->buck_mv ==
						WCD9XXX_CDC_BUCK_MV_1P8) {
					wcd9xxx_enable_buck(codec, clsh_d,
								false);
					wcd9xxx_ncp_bypass_enable(codec, true);
				} else {
					/*
					 *NCP settle time recommended by codec
					 *specification
					 */
					usleep_range(NCP_SETTLE_TIME_US,
						NCP_SETTLE_TIME_US + 10);
					wcd9xxx_clsh_set_Iest(codec, 0x02);
				}
				snd_soc_update_bits(codec,
						WCD9XXX_A_BUCK_MODE_1,
						0x04, 0x00);
				snd_soc_update_bits(codec,
						 WCD9XXX_A_BUCK_MODE_4,
						0xFF, BUCK_VREF_1P8V);
			}
		}
	}
}

static void wcd9xxx_clsh_state_ear_lo(struct snd_soc_codec *codec,
			struct wcd9xxx_clsh_cdc_data *clsh_d,
			u8 req_state, bool is_enable)
{

	dev_dbg(codec->dev, "%s: enter %s\n", __func__,
			is_enable ? "enable" : "disable");
	if (is_enable) {
		wcd9xxx_dynamic_bypass_buck_ctrl(codec, false);
		wcd9xxx_enable_buck(codec, clsh_d, true);
		if (req_state & WCD9XXX_CLSH_STATE_EAR) {
			wcd9xxx_cfg_clsh_param_ear(codec);
			wcd9xxx_set_fclk_get_ncp(codec, clsh_d,
						NCP_FCLK_LEVEL_8);
			wcd9xxx_ncp_bypass_enable(codec, true);
			wcd9xxx_enable_clsh_block(codec, clsh_d, true);
			wcd9xxx_chargepump_request(codec, true);
			wcd9xxx_enable_anc_delay(codec, true);
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_EAR, true);
		}
	} else {
		wcd9xxx_set_fclk_put_ncp(codec, clsh_d, NCP_FCLK_LEVEL_8);
		wcd9xxx_ncp_bypass_enable(codec, false);
		if (req_state & WCD9XXX_CLSH_STATE_LO) {
			snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC,
						0x20, 0x00);
			wcd9xxx_dynamic_bypass_buck_ctrl(codec, true);
		} else if (req_state & WCD9XXX_CLSH_STATE_EAR) {
			wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_EAR,
						false);
			/*sleep 5ms*/
			if (clsh_d->buck_mv == WCD9XXX_CDC_BUCK_MV_1P8) {
				wcd9xxx_enable_buck(codec, clsh_d, false);
				wcd9xxx_ncp_bypass_enable(codec, true);
			} else {
				/* NCP settle time recommended by codec	spec */
				usleep_range(NCP_SETTLE_TIME_US,
					     NCP_SETTLE_TIME_US + 10);
				wcd9xxx_clsh_set_Iest(codec, 0x02);
			}
			snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_1,
						0x04, 0x00);
			snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_4,
						0xFF, BUCK_VREF_1P8V);
		}
	}
}

static void wcd9xxx_clsh_state_hph_ear_lo(struct snd_soc_codec *codec,
			struct wcd9xxx_clsh_cdc_data *clsh_d,
			u8 req_state, bool is_enable)
{
	dev_dbg(codec->dev, "%s: enter %s\n", __func__,
			is_enable ? "enable" : "disable");

	if (clsh_d->state == WCD9XXX_CLSH_STATE_EAR_LO)
		wcd9xxx_cfg_clsh_param_hph(codec);

	if (req_state & WCD9XXX_CLSH_STATE_HPHL)
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_L,
					is_enable);

	if (req_state & WCD9XXX_CLSH_STATE_HPHR)
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_R,
					is_enable);

	if (req_state & WCD9XXX_CLSH_STATE_EAR) {
		wcd9xxx_cfg_clsh_param_ear(codec);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_EAR,
					is_enable);
	}
}

static void wcd9xxx_clsh_state_ear(struct snd_soc_codec *codec,
			struct wcd9xxx_clsh_cdc_data *clsh_d,
			u8 req_state, bool is_enable)
{
	pr_debug("%s: enter %s\n", __func__, is_enable ? "enable" : "disable");
	if (is_enable) {
		wcd9xxx_cfg_clsh_param_common(codec);
		wcd9xxx_cfg_clsh_param_ear(codec);
		wcd9xxx_enable_clsh_block(codec, clsh_d, true);
		wcd9xxx_chargepump_request(codec, true);
		wcd9xxx_enable_anc_delay(codec, true);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_EAR, true);
		wcd9xxx_set_buck_mode(codec, BUCK_VREF_2V);
		wcd9xxx_enable_buck(codec, clsh_d, true);
		wcd9xxx_set_fclk_get_ncp(codec, clsh_d, NCP_FCLK_LEVEL_8);

		dev_dbg(codec->dev, "%s: Enabled ear mode class h\n", __func__);
	} else {
		dev_dbg(codec->dev, "%s: stub fallback to ear\n", __func__);
		wcd9xxx_set_fclk_put_ncp(codec, clsh_d, NCP_FCLK_LEVEL_8);
		wcd9xxx_enable_buck(codec, clsh_d, false);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_EAR, false);
		wcd9xxx_chargepump_request(codec, false);
		wcd9xxx_enable_clsh_block(codec, clsh_d, false);
	}
}

static void wcd9xxx_clsh_state_hph_l(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	pr_debug("%s: enter %s\n", __func__, is_enable ? "enable" : "disable");

	if (is_enable) {
		wcd9xxx_cfg_clsh_param_common(codec);
		wcd9xxx_cfg_clsh_param_hph(codec);
		wcd9xxx_enable_clsh_block(codec, clsh_d, true);
		wcd9xxx_chargepump_request(codec, true);
		wcd9xxx_enable_anc_delay(codec, true);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_L, true);
		wcd9xxx_set_buck_mode(codec, BUCK_VREF_0P494V);
		wcd9xxx_enable_buck(codec, clsh_d, true);
		wcd9xxx_set_fclk_get_ncp(codec, clsh_d, NCP_FCLK_LEVEL_8);

		dev_dbg(codec->dev, "%s: Done\n", __func__);
	} else {
		wcd9xxx_set_fclk_put_ncp(codec, clsh_d, NCP_FCLK_LEVEL_8);
		wcd9xxx_enable_buck(codec, clsh_d, false);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_L, false);
		wcd9xxx_chargepump_request(codec, false);
		wcd9xxx_enable_clsh_block(codec, clsh_d, false);
	}
}

static void wcd9xxx_clsh_state_hph_r(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	pr_debug("%s: enter %s\n", __func__, is_enable ? "enable" : "disable");

	if (is_enable) {
		wcd9xxx_cfg_clsh_param_common(codec);
		wcd9xxx_cfg_clsh_param_hph(codec);
		wcd9xxx_enable_clsh_block(codec, clsh_d, true);
		wcd9xxx_chargepump_request(codec, true);
		wcd9xxx_enable_anc_delay(codec, true);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_R, true);
		wcd9xxx_set_buck_mode(codec, BUCK_VREF_0P494V);
		wcd9xxx_enable_buck(codec, clsh_d, true);
		wcd9xxx_set_fclk_get_ncp(codec, clsh_d, NCP_FCLK_LEVEL_8);

		dev_dbg(codec->dev, "%s: Done\n", __func__);
	} else {
		wcd9xxx_set_fclk_put_ncp(codec, clsh_d, NCP_FCLK_LEVEL_8);
		wcd9xxx_enable_buck(codec, clsh_d, false);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_R, false);
		wcd9xxx_chargepump_request(codec, false);
		wcd9xxx_enable_clsh_block(codec, clsh_d, false);
	}
}

static void wcd9xxx_clsh_state_hph_st(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	pr_debug("%s: enter %s\n", __func__, is_enable ? "enable" : "disable");

	if (is_enable) {
		if (req_state == WCD9XXX_CLSH_STATE_HPHL)
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_HPH_L, true);
		if (req_state == WCD9XXX_CLSH_STATE_HPHR)
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_HPH_R, true);
	} else {
		dev_dbg(codec->dev, "%s: stub fallback to hph_st\n", __func__);
		if (req_state == WCD9XXX_CLSH_STATE_HPHL)
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_HPH_L, false);
		if (req_state == WCD9XXX_CLSH_STATE_HPHR)
			wcd9xxx_clsh_comp_req(codec, clsh_d,
						CLSH_COMPUTE_HPH_R, false);
	}
}

static void wcd9xxx_clsh_state_lo(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	pr_debug("%s: enter %s, buck_mv %d\n", __func__,
		 is_enable ? "enable" : "disable", clsh_d->buck_mv);

	if (is_enable) {
		wcd9xxx_set_buck_mode(codec, BUCK_VREF_1P8V);
		wcd9xxx_enable_buck(codec, clsh_d, true);
		wcd9xxx_set_fclk_get_ncp(codec, clsh_d, NCP_FCLK_LEVEL_5);

		if (clsh_d->buck_mv == WCD9XXX_CDC_BUCK_MV_1P8) {
			wcd9xxx_enable_buck(codec, clsh_d, false);
			snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC,
					    1 << 4, 1 << 4);
			/* NCP settle time recommended by codec specification */
			usleep_range(NCP_SETTLE_TIME_US,
				     NCP_SETTLE_TIME_US + 10);
		} else {
			/* NCP settle time recommended by codec specification */
			usleep_range(NCP_SETTLE_TIME_US,
				     NCP_SETTLE_TIME_US + 10);
			snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_5,
					    0x01, (0x01 & 0x03));
			snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_5,
					    0xFC, (0xFC & 0xB));
		}
		snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_1, 0x04, 0x00);
	} else {
		dev_dbg(codec->dev, "%s: stub fallback to lineout\n", __func__);
		wcd9xxx_set_fclk_put_ncp(codec, clsh_d, NCP_FCLK_LEVEL_5);
		if (clsh_d->buck_mv != WCD9XXX_CDC_BUCK_MV_1P8)
			wcd9xxx_enable_buck(codec, clsh_d, false);
	}
}

static void wcd9xxx_clsh_state_err(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	char msg[128];

	dev_dbg(codec->dev,
		"%s Wrong request for class H state machine requested to %s %s",
		__func__, is_enable ? "enable" : "disable",
		state_to_str(req_state, msg, sizeof(msg)));
	WARN_ON(1);
}

/*
 * Function: wcd9xxx_clsh_is_state_valid
 * Params: state
 * Description:
 * Provides information on valid states of Class H configuration
 */
static int wcd9xxx_clsh_is_state_valid(u8 state)
{
	switch (state) {
	case WCD9XXX_CLSH_STATE_IDLE:
	case WCD9XXX_CLSH_STATE_EAR:
	case WCD9XXX_CLSH_STATE_HPHL:
	case WCD9XXX_CLSH_STATE_HPHR:
	case WCD9XXX_CLSH_STATE_HPH_ST:
	case WCD9XXX_CLSH_STATE_LO:
	case WCD9XXX_CLSH_STATE_HPHL_EAR:
	case WCD9XXX_CLSH_STATE_HPHR_EAR:
	case WCD9XXX_CLSH_STATE_HPH_ST_EAR:
	case WCD9XXX_CLSH_STATE_HPHL_LO:
	case WCD9XXX_CLSH_STATE_HPHR_LO:
	case WCD9XXX_CLSH_STATE_HPH_ST_LO:
	case WCD9XXX_CLSH_STATE_EAR_LO:
	case WCD9XXX_CLSH_STATE_HPHL_EAR_LO:
	case WCD9XXX_CLSH_STATE_HPHR_EAR_LO:
	case WCD9XXX_CLSH_STATE_HPH_ST_EAR_LO:
		return 1;
	default:
		break;
	}
	return 0;
}

/*
 * Function: wcd9xxx_clsh_fsm
 * Params: codec, cdc_clsh_d, req_state, req_type, clsh_event
 * Description:
 * This function handles PRE DAC and POST DAC conditions of different devices
 * and updates class H configuration of different combination of devices
 * based on validity of their states. cdc_clsh_d will contain current
 * class h state information
 */
void wcd9xxx_clsh_fsm(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *cdc_clsh_d,
		u8 req_state, bool req_type, u8 clsh_event)
{
	u8 old_state, new_state;
	char msg0[128], msg1[128];

	switch (clsh_event) {
	case WCD9XXX_CLSH_EVENT_PRE_DAC:
		/* PRE_DAC event should be used only for Enable */
		BUG_ON(req_type != WCD9XXX_CLSH_REQ_ENABLE);

		old_state = cdc_clsh_d->state;
		new_state = old_state | req_state;

		if (!wcd9xxx_clsh_is_state_valid(new_state)) {
			dev_dbg(codec->dev,
				"%s: classH not a valid new state: %s\n",
				__func__,
				state_to_str(new_state, msg0, sizeof(msg0)));
			return;
		}
		if (new_state == old_state) {
			dev_dbg(codec->dev,
				"%s: classH already in requested state: %s\n",
				__func__,
				state_to_str(new_state, msg0, sizeof(msg0)));
			return;
		}
		(*clsh_state_fp[new_state]) (codec, cdc_clsh_d, req_state,
					     req_type);
		cdc_clsh_d->state = new_state;
		dev_dbg(codec->dev,
			"%s: ClassH state transition from %s to %s\n",
			__func__, state_to_str(old_state, msg0, sizeof(msg0)),
			state_to_str(cdc_clsh_d->state, msg1, sizeof(msg1)));

		break;
	case WCD9XXX_CLSH_EVENT_POST_PA:
		if (req_type == WCD9XXX_CLSH_REQ_DISABLE) {
			old_state = cdc_clsh_d->state;
			new_state = old_state & (~req_state);

			if (new_state < NUM_CLSH_STATES) {
				if (!wcd9xxx_clsh_is_state_valid(old_state)) {
					dev_dbg(codec->dev,
						"%s:Invalid old state:%s\n",
						__func__,
						state_to_str(old_state, msg0,
						sizeof(msg0)));
					return;
				}
				if (new_state == old_state) {
					dev_dbg(codec->dev,
					"%s: clsH already in old state: %s\n",
					__func__,
					state_to_str(new_state, msg0,
					sizeof(msg0)));
					return;
				}
				(*clsh_state_fp[old_state]) (codec, cdc_clsh_d,
							     req_state,
							     req_type);
				cdc_clsh_d->state = new_state;
				dev_dbg(codec->dev, "%s: ClassH state transition from %s to %s\n",
					__func__, state_to_str(old_state, msg0,
							       sizeof(msg0)),
					state_to_str(cdc_clsh_d->state, msg1,
						     sizeof(msg1)));

			} else {
				dev_dbg(codec->dev, "%s:wrong new state=0x%x\n",
						__func__, new_state);
			}
		} else if (!(cdc_clsh_d->state & WCD9XXX_CLSH_STATE_LO)) {
			wcd9xxx_clsh_enable_post_pa(codec, cdc_clsh_d);
		}

		break;
	}

}
EXPORT_SYMBOL_GPL(wcd9xxx_clsh_fsm);

void wcd9xxx_clsh_init(struct wcd9xxx_clsh_cdc_data *clsh,
		       struct wcd9xxx_resmgr *resmgr)
{
	int i;
	clsh->state = WCD9XXX_CLSH_STATE_IDLE;
	clsh->resmgr = resmgr;

	for (i = 0; i < NUM_CLSH_STATES; i++)
		clsh_state_fp[i] = wcd9xxx_clsh_state_err;

	clsh_state_fp[WCD9XXX_CLSH_STATE_EAR] = wcd9xxx_clsh_state_ear;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHL] =
						wcd9xxx_clsh_state_hph_l;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHR] =
						wcd9xxx_clsh_state_hph_r;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPH_ST] =
						wcd9xxx_clsh_state_hph_st;
	clsh_state_fp[WCD9XXX_CLSH_STATE_LO] = wcd9xxx_clsh_state_lo;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHL_EAR] =
						wcd9xxx_clsh_state_hph_ear;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHR_EAR] =
						wcd9xxx_clsh_state_hph_ear;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPH_ST_EAR] =
						wcd9xxx_clsh_state_hph_ear;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHL_LO] = wcd9xxx_clsh_state_hph_lo;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHR_LO] = wcd9xxx_clsh_state_hph_lo;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPH_ST_LO] =
						wcd9xxx_clsh_state_hph_lo;
	clsh_state_fp[WCD9XXX_CLSH_STATE_EAR_LO] = wcd9xxx_clsh_state_ear_lo;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHL_EAR_LO] =
						wcd9xxx_clsh_state_hph_ear_lo;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHR_EAR_LO] =
						wcd9xxx_clsh_state_hph_ear_lo;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPH_ST_EAR_LO] =
						wcd9xxx_clsh_state_hph_ear_lo;

}
EXPORT_SYMBOL_GPL(wcd9xxx_clsh_init);

MODULE_DESCRIPTION("WCD9XXX Common");
MODULE_LICENSE("GPL v2");
