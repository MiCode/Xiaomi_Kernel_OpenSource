/*
 * Copyright (C) 2011 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_data/tegra_emc.h>

#include "board-whistler.h"
#include "tegra2_emc.h"
#include "board.h"
#include "fuse.h"
#include "devices.h"

static struct tegra_emc_table whistler_emc_tables_elpida_300Mhz[] = {
	{
		.rate = 25000,   /* SDRAM frquency */
		.regs = {
			0x00000002,   /* RC */
			0x00000006,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000004,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000004d,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000004,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000068,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000003,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xa06a04ae,   /* CFG_DIG_DLL */
			0x0001f000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000003,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 50000,   /* SDRAM frequency */
		.regs = {
			0x00000003,   /* RC */
			0x00000007,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000009f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000007,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000000d0,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xa06a04ae,   /* CFG_DIG_DLL */
			0x0001f000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000005,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 75000,   /* SDRAM frequency */
		.regs = {
			0x00000005,   /* RC */
			0x0000000a,   /* RFC */
			0x00000004,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x000000ff,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x0000000b,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000138,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xa06a04ae,   /* CFG_DIG_DLL */
			0x0001f000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000007,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 150000,   /* SDRAM frequency */
		.regs = {
			0x00000009,   /* RC */
			0x00000014,   /* RFC */
			0x00000007,   /* RAS */
			0x00000004,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000021f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000004,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000015,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000270,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000001,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xA04C04AE,   /* CFG_DIG_DLL */
			0x007FC010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000000e,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 300000,   /* SDRAM frequency */
		.regs = {
			0x00000012,   /* RC */
			0x00000027,   /* RFC */
			0x0000000D,   /* RAS */
			0x00000007,   /* RP */
			0x00000007,   /* R2W */
			0x00000005,   /* W2R */
			0x00000003,   /* R2P */
			0x00000009,   /* W2P */
			0x00000006,   /* RD_RCD */
			0x00000006,   /* WR_RCD */
			0x00000003,   /* RRD */
			0x00000003,   /* REXT */
			0x00000002,   /* WDV */
			0x00000006,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000009,   /* QSAFE */
			0x0000000c,   /* RDV */
			0x0000045f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000004,   /* PDEX2WR */
			0x00000004,   /* PDEX2RD */
			0x00000007,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000e,   /* RW2PDEN */
			0x0000002A,   /* TXSR */
			0x00000003,   /* TCKE */
			0x0000000F,   /* TFAW */
			0x00000008,   /* TRPAB */
			0x00000005,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000004E1,   /* TREFBW */
			0x00000005,   /* QUSE_EXTRA */
			0x00000002,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xE03C048B,   /* CFG_DIG_DLL */
			0x007FC010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000001B,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	}
};

static struct tegra_emc_table whistler_emc_tables_elpida_380Mhz[] = {
	{
		.rate = 23750,   /* SDRAM frquency */
		.regs = {
			0x00000002,   /* RC */
			0x00000006,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x0000000b,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000003,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000004,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000c,   /* RDV */
			0x00000047,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000b,   /* RW2PDEN */
			0x00000004,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000008,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000060,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000003,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xa0ae04ae,   /* CFG_DIG_DLL */
			0x0001f800,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000003,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 63334,   /* SDRAM frquency */
		.regs = {
			0x00000004,   /* RC */
			0x00000009,   /* RFC */
			0x00000003,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x0000000b,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000003,   /* WDV */
			0x00000006,   /* QUSE */
			0x00000004,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000c,   /* RDV */
			0x000000c4,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000b,   /* RW2PDEN */
			0x00000009,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000008,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000107,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000000,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xa0ae04ae,   /* CFG_DIG_DLL */
			0x0001f800,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000006,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 95000,   /* SDRAM frquency */
		.regs = {
			0x00000006,   /* RC */
			0x0000000d,   /* RFC */
			0x00000004,   /* RAS */
			0x00000003,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x0000000b,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000003,   /* WDV */
			0x00000006,   /* QUSE */
			0x00000004,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000c,   /* RDV */
			0x0000013f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000003,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000b,   /* RW2PDEN */
			0x0000000e,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000008,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x0000018c,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000001,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xa0ae04ae,   /* CFG_DIG_DLL */
			0x0001f000,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000009,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 190000,   /* SDRAM frquency */
		.regs = {
			0x0000000c,   /* RC */
			0x00000019,   /* RFC */
			0x00000008,   /* RAS */
			0x00000004,   /* RP */
			0x00000007,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x0000000b,   /* W2P */
			0x00000004,   /* RD_RCD */
			0x00000004,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000003,   /* REXT */
			0x00000003,   /* WDV */
			0x00000006,   /* QUSE */
			0x00000004,   /* QRST */
			0x00000009,   /* QSAFE */
			0x0000000d,   /* RDV */
			0x000002bf,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000004,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000c,   /* RW2PDEN */
			0x0000001b,   /* TXSR */
			0x00000003,   /* TCKE */
			0x0000000a,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000008,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000317,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000002,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xa06204ae,   /* CFG_DIG_DLL */
			0x007f7010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000012,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 300000,   /* SDRAM frquency */
		.regs = {
			0x00000012,   /* RC */
			0x00000027,   /* RFC */
			0x0000000d,   /* RAS */
			0x00000006,   /* RP */
			0x00000007,   /* R2W */
			0x00000005,   /* W2R */
			0x00000003,   /* R2P */
			0x0000000b,   /* W2P */
			0x00000006,   /* RD_RCD */
			0x00000006,   /* WR_RCD */
			0x00000003,   /* RRD */
			0x00000003,   /* REXT */
			0x00000003,   /* WDV */
			0x00000007,   /* QUSE */
			0x00000004,   /* QRST */
			0x00000009,   /* QSAFE */
			0x0000000d,   /* RDV */
			0x0000045f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000004,   /* PDEX2WR */
			0x00000004,   /* PDEX2RD */
			0x00000006,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000f,   /* RW2PDEN */
			0x0000002a,   /* TXSR */
			0x00000003,   /* TCKE */
			0x0000000f,   /* TFAW */
			0x00000007,   /* TRPAB */
			0x00000007,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000004e0,   /* TREFBW */
			0x00000006,   /* QUSE_EXTRA */
			0x00000002,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xe05e048b,   /* CFG_DIG_DLL */
			0x007f2010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000001b,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 380000,   /* SDRAM frquency */
		.regs = {
			0x00000017,   /* RC */
			0x00000032,   /* RFC */
			0x00000010,   /* RAS */
			0x00000007,   /* RP */
			0x00000008,   /* R2W */
			0x00000005,   /* W2R */
			0x00000003,   /* R2P */
			0x0000000b,   /* W2P */
			0x00000007,   /* RD_RCD */
			0x00000007,   /* WR_RCD */
			0x00000004,   /* RRD */
			0x00000003,   /* REXT */
			0x00000003,   /* WDV */
			0x00000007,   /* QUSE */
			0x00000004,   /* QRST */
			0x0000000a,   /* QSAFE */
			0x0000000e,   /* RDV */
			0x0000059f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000004,   /* PDEX2WR */
			0x00000004,   /* PDEX2RD */
			0x00000007,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x00000011,   /* RW2PDEN */
			0x00000036,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000013,   /* TFAW */
			0x00000008,   /* TRPAB */
			0x00000007,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x0000062d,   /* TREFBW */
			0x00000006,   /* QUSE_EXTRA */
			0x00000003,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xe044048b,   /* CFG_DIG_DLL */
			0x007fb010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x00000023,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	}
};

static struct tegra_emc_pdata whistler_emc_chip = {
	.description = "Elpida 300MHz",
	.mem_manufacturer_id = 0x0303,
	.mem_revision_id1 = -1,
	.mem_revision_id2 = -1,
	.mem_pid = -1,
	.tables = whistler_emc_tables_elpida_300Mhz,
	.num_tables = ARRAY_SIZE(whistler_emc_tables_elpida_300Mhz)
};

static struct tegra_emc_pdata whistler_ap25_emc_chip = {
	.description = "Elpida 380MHz",
	.mem_manufacturer_id = 0x0303,
	.mem_revision_id1 = -1,
	.mem_revision_id2 = -1,
	.mem_pid = -1,
	.tables = whistler_emc_tables_elpida_380Mhz,
	.num_tables = ARRAY_SIZE(whistler_emc_tables_elpida_380Mhz)
};

#define TEGRA25_SKU 0x17

int __init whistler_emc_init(void)
{
	int sku_id = tegra_sku_id;
	struct tegra_emc_pdata *emc_platdata;

	if (sku_id == TEGRA25_SKU)
		emc_platdata = &whistler_ap25_emc_chip;
	else
		emc_platdata = &whistler_emc_chip;

	tegra_emc_device.dev.platform_data = emc_platdata;
	platform_device_register(&tegra_emc_device);

	tegra_emc_init();

	return 0;
}
