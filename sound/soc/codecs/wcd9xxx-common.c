/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <sound/soc.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include "wcd9xxx-common.h"

#define CLSH_COMPUTE_EAR 0x01
#define CLSH_COMPUTE_HPH_L 0x02
#define CLSH_COMPUTE_HPH_R 0x03

#define BUCK_VREF_2V 0xFF
#define BUCK_VREF_1P8V 0xE6

#define NCP_FCLK_LEVEL_8 0x08
#define NCP_FCLK_LEVEL_5 0x05

#define BUCK_SETTLE_TIME_US 50
#define NCP_SETTLE_TIME_US 50

#define MAX_IMPED_PARAMS 13

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

static inline void wcd9xxx_enable_clsh_block(
	struct snd_soc_codec *codec,
	bool on)
{
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_B1_CTL,
		0x01, on ? 0x01 : 0x00);
}

static inline void wcd9xxx_enable_anc_delay(
	struct snd_soc_codec *codec,
	bool on)
{
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_B1_CTL,
		0x02, on ? 0x02 : 0x00);
}

static inline void wcd9xxx_enable_ncp(
	struct snd_soc_codec *codec,
	bool on)
{
	snd_soc_update_bits(codec, WCD9XXX_A_NCP_EN,
		0x01, on ? 0x01 : 0x00);
}

static inline void wcd9xxx_enable_buck(
	struct snd_soc_codec *codec,
	bool on)
{
	snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_1,
		0x80, on ? 0x80 : 0x00);
}

static int cdc_lo_count;

static void (*clsh_state_fp[NUM_CLSH_STATES])
			(struct snd_soc_codec *,
			 struct wcd9xxx_clsh_cdc_data *,
			 u8 req_state, bool req_type);

static const char *state_to_str(u8 state)
{
	if (state == WCD9XXX_CLSH_STATE_IDLE)
		return "STATE_IDLE";
	else if (state == WCD9XXX_CLSH_STATE_EAR)
		return "STATE_EAR";
	else if (state == WCD9XXX_CLSH_STATE_HPHL)
		return "STATE_HPH_L";
	else if (state == WCD9XXX_CLSH_STATE_HPHR)
		return "STATE_HPH_R";
	else if (state == (WCD9XXX_CLSH_STATE_HPHL
				| WCD9XXX_CLSH_STATE_HPHR))
		return "STATE_HPH_L_R";
	else if (state == WCD9XXX_CLSH_STATE_LO)
		return "STATE_LO";

	return "UNKNOWN_STATE";
}

static void wcd9xxx_cfg_clsh_buck(
		struct snd_soc_codec *codec)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_BUCK_CTRL_CCL_4, 0x0B, 0x00},
		{WCD9XXX_A_BUCK_CTRL_CCL_1, 0xF0, 0x50},
		{WCD9XXX_A_BUCK_CTRL_CCL_3, 0x03, 0x00},
		{WCD9XXX_A_BUCK_CTRL_CCL_3, 0x0B, 0x00},
	};

	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(codec, reg_set[i].reg, reg_set[i].mask,
						    reg_set[i].val);

	dev_dbg(codec->dev, "%s: Programmed buck parameters", __func__);
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

static void wcd9xxx_chargepump_request(
	struct snd_soc_codec *codec, bool on)
{
	static int cp_count;

	if (on && (++cp_count == 1)) {
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLK_OTHR_CTL,
							0x01, 0x01);
		dev_dbg(codec->dev, "%s: Charge Pump enabled, count = %d\n",
				__func__, cp_count);
	}

	else if (!on) {
		if (--cp_count < 0) {
			dev_dbg(codec->dev, "%s: Unbalanced disable for charge pump\n",
					__func__);
			if (snd_soc_read(codec, WCD9XXX_A_CDC_CLK_OTHR_CTL)
					& 0x01) {
				dev_dbg(codec->dev, "%s: Actual chargepump is ON\n",
						__func__);
			}
			cp_count = 0;
			WARN_ON(1);
		}

		if (cp_count == 0) {
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLK_OTHR_CTL,
							0x01, 0x00);
			dev_dbg(codec->dev, "%s: Charge pump disabled, count = %d\n",
					__func__, cp_count);
		}
	}
}

static int get_impedance_index(u32 imped)
{
	int i = 0;
	if (imped < imped_index[i].imped_val) {
		pr_debug("%s, detected impedance is less than 4 Ohm\n",
				__func__);
		goto ret;
	}
	for (i = 0; i < ARRAY_SIZE(imped_index); i++) {
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
	if (index > ARRAY_SIZE(imped_index)) {
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

static void wcd9xxx_enable_buck_mode(struct snd_soc_codec *codec,
		u8 buck_vref)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_BUCK_MODE_5, 0x02, 0x03},
		{WCD9XXX_A_BUCK_MODE_4, 0xFF, buck_vref},
		{WCD9XXX_A_BUCK_MODE_1, 0x04, 0x04},
		{WCD9XXX_A_BUCK_MODE_1, 0x08, 0x00},
		{WCD9XXX_A_BUCK_MODE_3, 0x04, 0x00},
		{WCD9XXX_A_BUCK_MODE_3, 0x08, 0x00},
		{WCD9XXX_A_BUCK_MODE_1, 0x80, 0x80},
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

static void wcd9xxx_set_fclk_enable_ncp(struct snd_soc_codec *codec,
		u8 fclk_level)
{
	int i;
	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_NCP_STATIC, 0x20, 0x20},
		{WCD9XXX_A_NCP_EN, 0x01, 0x01},
	};
	snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC,
						0x010, 0x00);
	snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC,
						0x0F, fclk_level);
	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(codec, reg_set[i].reg,
					reg_set[i].mask, reg_set[i].val);

	usleep_range(NCP_SETTLE_TIME_US, NCP_SETTLE_TIME_US);

	dev_dbg(codec->dev, "%s: set ncp done\n", __func__);
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

static void wcd9xxx_clsh_turnoff_postpa
	(struct snd_soc_codec *codec)
{

	int i;

	const struct wcd9xxx_reg_mask_val reg_set[] = {
		{WCD9XXX_A_NCP_EN, 0x01, 0x00},
		{WCD9XXX_A_BUCK_MODE_1, 0x80, 0x00},
		{WCD9XXX_A_CDC_CLSH_B1_CTL, 0x10, 0x00},
	};

	wcd9xxx_chargepump_request(codec, false);

	for (i = 0; i < ARRAY_SIZE(reg_set); i++)
		snd_soc_update_bits(codec, reg_set[i].reg,
				reg_set[i].mask, reg_set[i].val);

	wcd9xxx_enable_clsh_block(codec, false);

	dev_dbg(codec->dev, "%s: Done\n", __func__);
}

static void wcd9xxx_clsh_state_idle(struct snd_soc_codec *codec,
			struct wcd9xxx_clsh_cdc_data *clsh_d,
			u8 req_state, bool is_enable)
{
	if (is_enable) {
		dev_dbg(codec->dev, "%s: wrong transition, cannot enable IDLE state\n",
			   __func__);
	} else {
		if (req_state == WCD9XXX_CLSH_STATE_EAR) {
			wcd9xxx_clsh_turnoff_postpa(codec);
		} else if (req_state == WCD9XXX_CLSH_STATE_HPHL) {
			wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_L,
					      false);
			wcd9xxx_clsh_turnoff_postpa(codec);
		} else if (req_state == WCD9XXX_CLSH_STATE_HPHR) {
			wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_R,
					      false);
			wcd9xxx_clsh_turnoff_postpa(codec);
		} else if (req_state == WCD9XXX_CLSH_STATE_LO) {
			wcd9xxx_enable_ncp(codec, false);
			wcd9xxx_enable_buck(codec, false);
		}
	}
}

static void wcd9xxx_clsh_state_ear(struct snd_soc_codec *codec,
			struct wcd9xxx_clsh_cdc_data *clsh_d,
			u8 req_state, bool is_enable)
{
	if (is_enable) {
		wcd9xxx_cfg_clsh_buck(codec);
		wcd9xxx_cfg_clsh_param_common(codec);
		wcd9xxx_cfg_clsh_param_ear(codec);
		wcd9xxx_enable_clsh_block(codec, true);
		wcd9xxx_chargepump_request(codec, true);
		wcd9xxx_enable_anc_delay(codec, true);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_EAR, true);
		wcd9xxx_enable_buck_mode(codec, BUCK_VREF_2V);
		wcd9xxx_set_fclk_enable_ncp(codec, NCP_FCLK_LEVEL_8);

		dev_dbg(codec->dev, "%s: Enabled ear mode class h\n", __func__);
	} else {
		dev_dbg(codec->dev, "%s: stub fallback to ear\n", __func__);
	}
}

static void wcd9xxx_clsh_state_hph_l(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	if (is_enable) {
		wcd9xxx_cfg_clsh_buck(codec);
		wcd9xxx_cfg_clsh_param_common(codec);
		wcd9xxx_cfg_clsh_param_hph(codec);
		wcd9xxx_enable_clsh_block(codec, true);
		wcd9xxx_chargepump_request(codec, true);
		wcd9xxx_enable_anc_delay(codec, true);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_L, true);
		wcd9xxx_enable_buck_mode(codec, BUCK_VREF_2V);
		wcd9xxx_set_fclk_enable_ncp(codec, NCP_FCLK_LEVEL_8);

		dev_dbg(codec->dev, "%s: Done\n", __func__);
	} else {
		if (req_state == WCD9XXX_CLSH_STATE_HPHR) {
			wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_R,
					      false);
		} else {
			dev_dbg(codec->dev, "%s: stub fallback to hph_l\n",
					__func__);
		}
	}
}

static void wcd9xxx_clsh_state_hph_r(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	if (is_enable) {

		wcd9xxx_cfg_clsh_buck(codec);
		wcd9xxx_cfg_clsh_param_common(codec);
		wcd9xxx_cfg_clsh_param_hph(codec);
		wcd9xxx_enable_clsh_block(codec, true);
		wcd9xxx_chargepump_request(codec, true);
		wcd9xxx_enable_anc_delay(codec, true);
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_R, true);
		wcd9xxx_enable_buck_mode(codec, BUCK_VREF_2V);
		wcd9xxx_set_fclk_enable_ncp(codec, NCP_FCLK_LEVEL_8);

		dev_dbg(codec->dev, "%s: Done\n", __func__);
	} else {
		if (req_state == WCD9XXX_CLSH_STATE_HPHL) {
			wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_L,
					      false);
		} else {
			dev_dbg(codec->dev, "%s: stub fallback to hph_r\n",
					__func__);
		}
	}
}

static void wcd9xxx_clsh_state_hph_st(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	if (is_enable) {
		wcd9xxx_clsh_comp_req(codec, clsh_d, CLSH_COMPUTE_HPH_L, true);
		wcd9xxx_clsh_comp_req(codec, clsh_d,  CLSH_COMPUTE_HPH_R, true);
	} else {
		dev_dbg(codec->dev, "%s: stub fallback to hph_st\n", __func__);
	}
}

static void wcd9xxx_clsh_state_lo(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	if (is_enable) {
		if (++cdc_lo_count > 1)
			return;

		wcd9xxx_enable_buck_mode(codec, BUCK_VREF_1P8V);
		wcd9xxx_set_fclk_enable_ncp(codec, NCP_FCLK_LEVEL_5);

		if (clsh_d->buck_mv == WCD9XXX_CDC_BUCK_MV_1P8) {
			wcd9xxx_enable_buck(codec, false);
			snd_soc_update_bits(codec, WCD9XXX_A_NCP_STATIC,
							0x20, 0x01);
			wcd9xxx_enable_ncp(codec, true);
			/* NCP settle time recommended by codec specification */
			usleep_range(NCP_SETTLE_TIME_US,
				NCP_SETTLE_TIME_US + 10);

		} else {
			snd_soc_update_bits(codec, WCD9XXX_A_NCP_EN,
							0x40, 0x00);
			wcd9xxx_enable_ncp(codec, true);
			/* NCP settle time recommended by codec specification */
			usleep_range(NCP_SETTLE_TIME_US,
				NCP_SETTLE_TIME_US + 10);
			snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_5,
							0x01, 0x01);
			snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_5,
							0xFB, (0x02 << 2));
		}
		snd_soc_update_bits(codec, WCD9XXX_A_BUCK_MODE_1,
							0x04, 0x00);
	} else {
		dev_dbg(codec->dev, "%s: stub fallback to lineout\n", __func__);
	}
}

static void wcd9xxx_clsh_state_err(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable)
{
	dev_dbg(codec->dev, "%s Wrong request for class H state machine requested to %s %s"
			, __func__, is_enable ? "enable" : "disable",
			state_to_str(req_state));
	WARN_ON(1);
}

void wcd9xxx_clsh_fsm(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *cdc_clsh_d,
		u8 req_state, bool req_type, u8 clsh_event)
{
	u8 old_state, new_state;

	switch (clsh_event) {

	case WCD9XXX_CLSH_EVENT_PRE_DAC:

		/* PRE_DAC event should be used only for Enable */
		BUG_ON(req_type != WCD9XXX_CLSH_REQ_ENABLE);

		old_state = cdc_clsh_d->state;
		new_state = old_state | req_state;

		(*clsh_state_fp[new_state]) (codec, cdc_clsh_d,
							req_state, req_type);
		cdc_clsh_d->state = new_state;
		dev_dbg(codec->dev, "%s: ClassH state transition from %s to %s\n",
			__func__, state_to_str(old_state),
			state_to_str(cdc_clsh_d->state));

		break;

	case WCD9XXX_CLSH_EVENT_POST_PA:

		if (req_type == WCD9XXX_CLSH_REQ_DISABLE) {
			if (req_state == WCD9XXX_CLSH_STATE_LO
					&& --cdc_lo_count > 0)
				break;

			old_state = cdc_clsh_d->state;
			new_state = old_state & (~req_state);

			if (new_state < NUM_CLSH_STATES) {
				(*clsh_state_fp[new_state]) (codec, cdc_clsh_d,
							req_state, req_type);
				cdc_clsh_d->state = new_state;
				dev_dbg(codec->dev, "%s: ClassH state transition from %s to %s\n",
					__func__, state_to_str(old_state),
					state_to_str(cdc_clsh_d->state));

			} else {
				dev_dbg(codec->dev, "%s: wrong new state = %x\n",
						__func__, new_state);
			}


		} else if (req_state != WCD9XXX_CLSH_STATE_LO) {
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

	clsh_state_fp[WCD9XXX_CLSH_STATE_IDLE] = wcd9xxx_clsh_state_idle;
	clsh_state_fp[WCD9XXX_CLSH_STATE_EAR] = wcd9xxx_clsh_state_ear;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHL] =
						wcd9xxx_clsh_state_hph_l;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPHR] =
						wcd9xxx_clsh_state_hph_r;
	clsh_state_fp[WCD9XXX_CLSH_STATE_HPH_ST] =
						wcd9xxx_clsh_state_hph_st;
	clsh_state_fp[WCD9XXX_CLSH_STATE_LO] = wcd9xxx_clsh_state_lo;

}
EXPORT_SYMBOL_GPL(wcd9xxx_clsh_init);

MODULE_DESCRIPTION("WCD9XXX Common");
MODULE_LICENSE("GPL v2");
