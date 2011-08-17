/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <mach/pmic.h>
#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"

/* WVGA Primary Display */
#define MDDI_QUICKVX_1_2		1
/* MDDI Manufacturer Code */
#define QUICKVX_MDDI_MFR_CODE	0xc583
/* MDDI Product Code */
#define QUICKVX_MDDI_PRD_CODE	0x5800

/* Register Address Maps */
/* MDDI Address Anti-fuse values for bits [31:22] */
#define QUICKVX_ADDR_31_22_AF	(0X000 << 22)

/* MDDI Address Maps */
/* VEE Block Address Base */
#define QUICKVX_VEE_BASE		(QUICKVX_ADDR_31_22_AF | 0x00000000)
/* SPI Block Address Base */
#define QUICKVX_SPI_BASE		(QUICKVX_ADDR_31_22_AF | 0x00010000)
/* Clock and Reset (CAR) Address Base */
#define QUICKVX_CAR_BASE		(QUICKVX_ADDR_31_22_AF | 0x00020000)
/* Register Control Block (RCB) Address Base */
#define QUICKVX_RCB_BASE		(QUICKVX_ADDR_31_22_AF | 0x00030000)
/* Cellular RAM Address Base */
#define QUICKVX_CELLRAM_BASE	(QUICKVX_ADDR_31_22_AF | 0x00100000)
/* FB through A2F Address Base */
#define QUICKVX_FB_A2F_BASE		(QUICKVX_ADDR_31_22_AF | 0x00200000)


/***************************************************
 * Common Registers in Register Control Block (RCB) Registers
 ***************************************************/
 /* CellRAM Configuration RCR Register */
#define QUICKVX_RCB_RCR_REG			(QUICKVX_RCB_BASE | 0x00000000)
/* Image Effect Register */
#define QUICKVX_RCB_IER_REG			(QUICKVX_RCB_BASE | 0x00000004)
/* Row Number Register */
#define QUICKVX_RCB_ROWNUM_REG		(QUICKVX_RCB_BASE | 0x00000008)
/* TCON Timing0 Register */
#define QUICKVX_RCB_TCON0_REG		(QUICKVX_RCB_BASE | 0x0000000C)
/* TCON Timing1 Register */
#define QUICKVX_RCB_TCON1_REG		(QUICKVX_RCB_BASE | 0x00000010)
/* TCON Timing2 Register */
#define QUICKVX_RCB_TCON2_REG		(QUICKVX_RCB_BASE | 0x00000014)
/* PWM Control Register */
#define QUICKVX_RCB_PWMC_REG		(QUICKVX_RCB_BASE | 0x00000018)
/* PWM Width Register */
#define QUICKVX_RCB_PWMW_REG		(QUICKVX_RCB_BASE | 0x0000001C)
/* VEE Configuration Register */
#define QUICKVX_RCB_VEECONF_REG		(QUICKVX_RCB_BASE | 0x00000020)
/* CellRAM Configuration BCR Register */
#define QUICKVX_RCB_CELLBCR_REG		(QUICKVX_RCB_BASE | 0x00000024)
/* CellRAM Configuration Control Register */
#define QUICKVX_RCB_CELLCC_REG		(QUICKVX_RCB_BASE | 0x00000028)
/* Use Case Register */
#define QUICKVX_RCB_USECASE_REG		(QUICKVX_RCB_BASE | 0x00000100)
/* Video Parameter Register */
#define QUICKVX_RCB_VPARM_REG		(QUICKVX_RCB_BASE | 0x00000104)
/* MDDI Client Wake-up Register */
#define QUICKVX_RCB_MCW_REG			(QUICKVX_RCB_BASE | 0x00000108)
/* Burst Length Register */
#define QUICKVX_RCB_BURSTLN_REG		(QUICKVX_RCB_BASE | 0x0000010C)
/* Display Attributes Register */
#define QUICKVX_RCB_DISPATTR_REG	(QUICKVX_RCB_BASE | 0x00000110)
/* Error Status Register */
#define QUICKVX_RCB_ERRSTAT_REG		(QUICKVX_RCB_BASE | 0x00000114)
/* Error Mask Register */
#define QUICKVX_RCB_ERRMSK_REG		(QUICKVX_RCB_BASE | 0x00000118)
/* MDDI ASSP FIFO Overflow Address Register */
#define QUICKVX_RCB_ASSPFOA_REG		(QUICKVX_RCB_BASE | 0x0000011C)
/* MDDI Fabric FIFO Overflow Address Register */
#define QUICKVX_RCB_FABFOA_REG		(QUICKVX_RCB_BASE | 0x00000120)
/* Incoming RGB FIFO Overflow Address Register */
#define QUICKVX_RCB_IRFOA_REG		(QUICKVX_RCB_BASE | 0x00000124)
/* SPI Overflow Address Register */
#define QUICKVX_RCB_SPIOA_REG		(QUICKVX_RCB_BASE | 0x00000128)
/* Ping Buffer Address Register */
#define QUICKVX_RCB_PINGBA_REG		(QUICKVX_RCB_BASE | 0x0000012C)
/* Pong Buffer Address Register */
#define QUICKVX_RCB_PONGBA_REG		(QUICKVX_RCB_BASE | 0x00000130)
/* Configuration Done Register */
#define QUICKVX_RCB_CONFDONE_REG	(QUICKVX_RCB_BASE | 0x00000134)
/* FIFO Flush Register */
#define QUICKVX_RCB_FFLUSH_REG		(QUICKVX_RCB_BASE | 0x00000138)


/***************************************************
 * SPI Block Registers
 ***************************************************/
/* SPI Rx0 Register */
#define QUICKVX_SPI_RX0_REG			(QUICKVX_SPI_BASE | 0x00000000)
/* SPI Rx1 Register */
#define QUICKVX_SPI_RX1_REG			(QUICKVX_SPI_BASE | 0x00000004)
/* SPI Rx2 Register */
#define QUICKVX_SPI_RX2_REG			(QUICKVX_SPI_BASE | 0x00000008)
/* SPI Rx3 Register */
#define QUICKVX_SPI_RX3_REG			(QUICKVX_SPI_BASE | 0x0000000C)
/* SPI Rx4 Register */
#define QUICKVX_SPI_RX4_REG			(QUICKVX_SPI_BASE | 0x00000010)
/* SPI Rx5 Register */
#define QUICKVX_SPI_RX5_REG			(QUICKVX_SPI_BASE | 0x00000014)
/* SPI Rx6 Register */
#define QUICKVX_SPI_RX6_REG			(QUICKVX_SPI_BASE | 0x00000018)
/* SPI Rx7 Register */
#define QUICKVX_SPI_RX7_REG			(QUICKVX_SPI_BASE | 0x0000001C)
/* SPI Tx0 Register */
#define QUICKVX_SPI_TX0_REG			(QUICKVX_SPI_BASE | 0x00000020)
/* SPI Tx1 Register */
#define QUICKVX_SPI_TX1_REG			(QUICKVX_SPI_BASE | 0x00000024)
/* SPI Tx2 Register */
#define QUICKVX_SPI_TX2_REG			(QUICKVX_SPI_BASE | 0x00000028)
/* SPI Tx3 Register */
#define QUICKVX_SPI_TX3_REG			(QUICKVX_SPI_BASE | 0x0000002C)
/* SPI Tx4 Register */
#define QUICKVX_SPI_TX4_REG			(QUICKVX_SPI_BASE | 0x00000030)
/* SPI Tx5 Register */
#define QUICKVX_SPI_TX5_REG			(QUICKVX_SPI_BASE | 0x00000034)
/* SPI Tx6 Register */
#define QUICKVX_SPI_TX6_REG			(QUICKVX_SPI_BASE | 0x00000038)
/* SPI Tx7 Register */
#define QUICKVX_SPI_TX7_REG			(QUICKVX_SPI_BASE | 0x0000003C)
/* SPI Control Register */
#define QUICKVX_SPI_CTRL_REG		(QUICKVX_SPI_BASE | 0x00000040)
/* SPI Transfer Length Register */
#define QUICKVX_SPI_TLEN_REG		(QUICKVX_SPI_BASE | 0x00000044)


/***************************************************
 * Clock and Reset (CAR) Block Registers
 ***************************************************/
/* ASSP Global Clock Enable Register */
#define QUICKVX_CAR_ASSP_GCE_REG	(QUICKVX_CAR_BASE | 0x00000000)
/* VLP Control1 Register */
#define QUICKVX_CAR_VLPCTRL1_REG	(QUICKVX_CAR_BASE | 0x00000004)
/* VLP Control2 Register */
#define QUICKVX_CAR_VLPCTRL2_REG	(QUICKVX_CAR_BASE | 0x00000008)
/* Clock Selection Register */
#define QUICKVX_CAR_CLKSEL_REG		(QUICKVX_CAR_BASE | 0x0000000C)
/* PLL Control Register */
#define QUICKVX_CAR_PLLCTRL_REG		(QUICKVX_CAR_BASE | 0x00000010)
/* PLL Clock Ratio Register */
#define QUICKVX_CAR_PLLCLKRATIO_REG	(QUICKVX_CAR_BASE | 0x00000014)


/***************************************************
 * VEE Block Registers
 ***************************************************/
/* VEE Control Register */
#define QUICKVX_VEE_VEECTRL_REG		(QUICKVX_VEE_BASE | 0x00000000)
/* Strength Register */
#define QUICKVX_VEE_STRENGTH_REG	(QUICKVX_VEE_BASE | 0x0000000C)
/* Variance Register */
#define QUICKVX_VEE_VARIANCE_REG	(QUICKVX_VEE_BASE | 0x00000010)
/* Slope Register */
#define QUICKVX_VEE_SLOPE_REG		(QUICKVX_VEE_BASE | 0x00000014)
/* Sharpen Control0 Register */
#define QUICKVX_VEE_SHRPCTRL0_REG	(QUICKVX_VEE_BASE | 0x0000001C)
/* Sharpen Control1 Register */
#define QUICKVX_VEE_SHRPCTRL1_REG	(QUICKVX_VEE_BASE | 0x00000020)
/* Upper Horizontal Positon Register */
#define QUICKVX_VEE_UHPOS_REG		(QUICKVX_VEE_BASE | 0x00000024)
/* Lower Horizontal Positon Register */
#define QUICKVX_VEE_LHPOS_REG		(QUICKVX_VEE_BASE | 0x00000028)
/* Upper Vertical Positon Register */
#define QUICKVX_VEE_UVPOS_REG		(QUICKVX_VEE_BASE | 0x0000002C)
/* Lower Vertical Positon Register */
#define QUICKVX_VEE_LVPOS_REG		(QUICKVX_VEE_BASE | 0x00000030)
/* Upper Frame Width Register */
#define QUICKVX_VEE_UFWDTH_REG		(QUICKVX_VEE_BASE | 0x00000034)
/* Lower Frame Width Register */
#define QUICKVX_VEE_LFWDTH_REG		(QUICKVX_VEE_BASE | 0x00000038)
/* Upper Frame Height Register */
#define QUICKVX_VEE_UFHGHT_REG		(QUICKVX_VEE_BASE | 0x0000003C)
/* Lower Frame Height Register */
#define QUICKVX_VEE_LFHGHT_REG		(QUICKVX_VEE_BASE | 0x00000040)
/* Control0 Register */
#define QUICKVX_VEE_CTRL0_REG		(QUICKVX_VEE_BASE | 0x00000044)
/* Control1 Register */
#define QUICKVX_VEE_CTRL1_REG		(QUICKVX_VEE_BASE | 0x00000048)
/* Video Enhancement Enable Register */
#define QUICKVX_VEE_VDOEEN_REG		(QUICKVX_VEE_BASE | 0x0000004C)
/* Black Level Register */
#define QUICKVX_VEE_BLCKLEV_REG		(QUICKVX_VEE_BASE | 0x00000050)
/* White Level Register */
#define QUICKVX_VEE_WHTLEV_REG		(QUICKVX_VEE_BASE | 0x00000054)
/* Amplification Limits Register */
#define QUICKVX_VEE_AMPLMTS_REG		(QUICKVX_VEE_BASE | 0x00000060)
/* Dithering Mode Register */
#define QUICKVX_VEE_DITHMOD_REG		(QUICKVX_VEE_BASE | 0x00000064)
/* Upper Look-up Data Register */
#define QUICKVX_VEE_ULUD_REG		(QUICKVX_VEE_BASE | 0x00000080)
/* Lower Look-up Data Register */
#define QUICKVX_VEE_LLUD_REG		(QUICKVX_VEE_BASE | 0x00000084)
/* Look-up Address Register */
#define QUICKVX_VEE_LUADDR_REG		(QUICKVX_VEE_BASE | 0x00000088)
/* Look-up Write Enable Register */
#define QUICKVX_VEE_LUWREN_REG		(QUICKVX_VEE_BASE | 0x0000008C)
/* VEE ID Register */
#define QUICKVX_VEE_VEEID_REG		(QUICKVX_VEE_BASE | 0x000003FC)
/* M_11 Register */
#define QUICKVX_VEE_M_11_REG		(QUICKVX_VEE_BASE | 0x000000C0)
/* M_12 Register */
#define QUICKVX_VEE_M_12_REG		(QUICKVX_VEE_BASE | 0x000000C4)
/* M_13 Register */
#define QUICKVX_VEE_M_13_REG		(QUICKVX_VEE_BASE | 0x000000C8)
/* M_21 Register */
#define QUICKVX_VEE_M_21_REG		(QUICKVX_VEE_BASE | 0x000000CC)
/* M_22 Register */
#define QUICKVX_VEE_M_22_REG		(QUICKVX_VEE_BASE | 0x000000D0)
/* M_23 Register */
#define QUICKVX_VEE_M_23_REG		(QUICKVX_VEE_BASE | 0x000000D4)
/* M_31 Register */
#define QUICKVX_VEE_M_31_REG		(QUICKVX_VEE_BASE | 0x000000D8)
/* M_32 Register */
#define QUICKVX_VEE_M_32_REG		(QUICKVX_VEE_BASE | 0x000000DC)
/* M_33 Register */
#define QUICKVX_VEE_M_33_REG		(QUICKVX_VEE_BASE | 0x000000E0)
/* R Offset Register */
#define QUICKVX_VEE_OFFSET_R_REG	(QUICKVX_VEE_BASE | 0x000000E8)
/* G Offset Register */
#define QUICKVX_VEE_OFFSET_G_REG	(QUICKVX_VEE_BASE | 0x000000EC)
/* B Offset Register */
#define QUICKVX_VEE_OFFSET_B_REG	(QUICKVX_VEE_BASE | 0x000000F0)

/* LCD Reset Register */
#define QUICKVX_FB_A2F_LCD_RESET_REG (QUICKVX_FB_A2F_BASE | 0x00000000)

/* Register bit defines */
/* PLL Lock bit in the PLL Control Register */
#define QUICKVX_PLL_LOCK_BIT		(1 << 7)

#define QL_SPI_CTRL_rSPISTart(x) (x)
#define QL_SPI_CTRL_rCPHA(x) (x << 1)
#define QL_SPI_CTRL_rCPOL(x) (x << 2)
#define QL_SPI_CTRL_rLSB(x) (x << 3)
#define QL_SPI_CTRL_rSLVSEL(x) (x << 4)
#define QL_SPI_CTRL_MASK_rTxDone (1 << 9)

#define QL_SPI_LCD_DEV_ID 0x1c
#define QL_SPI_LCD_RS(x) (x << 1)
#define QL_SPI_LCD_RW(x) (x)
#define QL_SPI_LCD_INDEX_START_BYTE ((QL_SPI_LCD_DEV_ID << 2) | \
	QL_SPI_LCD_RS(0) | QL_SPI_LCD_RW(0))
#define QL_SPI_LCD_CMD_START_BYTE ((QL_SPI_LCD_DEV_ID << 2) | \
	QL_SPI_LCD_RS(1) | QL_SPI_LCD_RW(0))
#define QL_SPI_CTRL_LCD_START (QL_SPI_CTRL_rSPISTart(1) | \
	QL_SPI_CTRL_rCPHA(1) | QL_SPI_CTRL_rCPOL(1) | \
	QL_SPI_CTRL_rLSB(0) | QL_SPI_CTRL_rSLVSEL(0))

int ql_mddi_write(uint32 address, uint32 value)
{
	uint32 regval = 0;
	int ret = 0;

	ret = mddi_queue_register_write(address, value, TRUE, 0);

	if (!ret) {
		ret = mddi_queue_register_read(address, &regval, TRUE, 0);
		if (regval != value) {
			MDDI_MSG_DEBUG("\nMismatch: ql_mddi_write[0x%x]->0x%x "
				"r0x%x\n", address, value, regval);
		} else {
			MDDI_MSG_DEBUG("\nMatch: ql_mddi_write[0x%x]->0x%x "
				"r0x%x\n", address, value, regval);
		}
	}

	return ret;
}

int ql_mddi_read(uint32 address, uint32 *regval)
{
	int ret = 0;

	ret = mddi_queue_register_read(address, regval, TRUE, 0);
	MDDI_MSG_DEBUG("\nql_mddi_read[0x%x]=0x%x", address, *regval);

	return ret;
}

int ql_send_spi_cmd_to_lcd(uint32 index, uint32 cmd)
{
	int retry, ret;
	uint32 readval;

	MDDI_MSG_DEBUG("\n %s(): index 0x%x, cmd 0x%x", __func__, index, cmd);
	/* do the index phase */
	/* send 24 bits in the index phase */
	ql_mddi_write(QUICKVX_SPI_TLEN_REG, 23);

	/* send 24 bits in the index phase, starting at bit 23 of TX0 reg */
	ql_mddi_write(QUICKVX_SPI_TX0_REG,
		(QL_SPI_LCD_INDEX_START_BYTE << 16) | index);

	/* set start */
	ql_mddi_write(QUICKVX_SPI_CTRL_REG,  QL_SPI_CTRL_LCD_START);
	retry = 0;

	do {
		ret = ql_mddi_read(QUICKVX_SPI_CTRL_REG, &readval);

		if (ret || ++retry > 5) {
			MDDI_MSG_DEBUG("\n ql_send_spi_cmd_to_lcd: retry "
				"timeout at index phase, ret = %d", ret);
			return -EIO;
		}
		mddi_wait(1);
	} while ((readval & QL_SPI_CTRL_MASK_rTxDone) == 0);

	/* do the command phase */
	/* send 24 bits in the cmd phase */
	ql_mddi_write(QUICKVX_SPI_TLEN_REG, 23);

	/* send 24 bits in the cmd phase, starting at bit 23 of TX0 reg. */
	ql_mddi_write(QUICKVX_SPI_TX0_REG,
		(QL_SPI_LCD_CMD_START_BYTE << 16) | cmd);

	/* set start */
	ql_mddi_write(QUICKVX_SPI_CTRL_REG,  QL_SPI_CTRL_LCD_START);
	retry = 0;

	do {
		ret = ql_mddi_read(QUICKVX_SPI_CTRL_REG, &readval);

		if (ret || ++retry > 5) {
			MDDI_MSG_DEBUG("\n ql_send_spi_cmd_to_lcd: retry "
				"timeout at cmd phase, ret = %d", ret);
			return -EIO;
		}
		mddi_wait(1);
	} while ((readval & QL_SPI_CTRL_MASK_rTxDone) == 0);

	return 0;
}


int ql_send_spi_data_from_lcd(uint32 index, uint32 *value)
{
	int retry, ret;
	uint32 readval;

	MDDI_MSG_DEBUG("\n %s(): index 0x%x", __func__, index);
	/* do the index phase */
	/* send 24 bits in the index phase */
	ql_mddi_write(QUICKVX_SPI_TLEN_REG, 23);

	/* send 24 bits in the index phase, starting at bit 23 of TX0 reg */
	ql_mddi_write(QUICKVX_SPI_TX0_REG,
		(QL_SPI_LCD_INDEX_START_BYTE << 16) | index);

	/* set start */
	ql_mddi_write(QUICKVX_SPI_CTRL_REG,  QL_SPI_CTRL_LCD_START);
	retry = 0;

	do {
		ret = ql_mddi_read(QUICKVX_SPI_CTRL_REG, &readval);

		if (ret || ++retry > 5) {
			MDDI_MSG_DEBUG("\n ql_send_spi_cmd_to_lcd: retry "
				"timeout at index phase, ret = %d", ret);
			return -EIO;
		}
		mddi_wait(1);
	} while ((readval & QL_SPI_CTRL_MASK_rTxDone) == 0);

	/* do the command phase */
	/* send 8 bits  and read 24 bits in the cmd phase, so total 32 bits */
	ql_mddi_write(QUICKVX_SPI_TLEN_REG, 31);

	/* send 24 bits in the cmd phase, starting at bit 31 of TX0 reg */
	ql_mddi_write(QUICKVX_SPI_TX0_REG,
		((QL_SPI_LCD_CMD_START_BYTE << 16)) << 8);

	/* set start */
	ql_mddi_write(QUICKVX_SPI_CTRL_REG,  QL_SPI_CTRL_LCD_START);
	retry = 0;

	do {
		ret = ql_mddi_read(QUICKVX_SPI_CTRL_REG, &readval);

		if (ret || ++retry > 5) {
			MDDI_MSG_DEBUG("\n ql_send_spi_cmd_to_lcd: retry "
				"timeout at cmd phase, ret = %d", ret);
			return -EIO;
		}
		mddi_wait(1);
	} while ((readval & QL_SPI_CTRL_MASK_rTxDone) == 0);

	/* value will appear at lower 16 bits */
	ret = ql_mddi_read(QUICKVX_SPI_RX0_REG, value);

	if (!ret) {
		*value = *value & 0xffff;
		MDDI_MSG_DEBUG("\n QUICKVX_SPI_RX0_REG value = 0x%x", *value);
	} else
		MDDI_MSG_DEBUG("\n Read QUICKVX_SPI_RX0_REG Failed");

	return ret;
}

/* Global Variables */
static uint32 mddi_quickvx_rows_per_second;
static uint32 mddi_quickvx_usecs_per_refresh;
static uint32 mddi_quickvx_rows_per_refresh;

void mddi_quickvx_configure_registers(void)
{
	MDDI_MSG_DEBUG("\n%s(): ", __func__);
	ql_mddi_write(QUICKVX_CAR_CLKSEL_REG, 0x00007000);

	ql_mddi_write(QUICKVX_RCB_PWMW_REG, 0x0000FFFF);

	ql_mddi_write(QUICKVX_RCB_PWMC_REG, 0x00000001);

	ql_mddi_write(QUICKVX_RCB_CONFDONE_REG, 0x00000000);

	/* display is x width = 480, y width = 864 */
	ql_mddi_write(QUICKVX_RCB_TCON0_REG, 0x035f01df);

	/* VFP=2, VBP=4, HFP=16, HBP=16 */
	ql_mddi_write(QUICKVX_RCB_TCON1_REG, 0x01e301e1);

	/* VSW =2, HSW=8 */
	ql_mddi_write(QUICKVX_RCB_TCON2_REG, 0x000000e1);

	ql_mddi_write(QUICKVX_RCB_DISPATTR_REG, 0x00000000);

	ql_mddi_write(QUICKVX_RCB_USECASE_REG, 0x00000025);

	ql_mddi_write(QUICKVX_RCB_VPARM_REG, 0x00000888);

	ql_mddi_write(QUICKVX_RCB_VEECONF_REG, 0x00000001);

	ql_mddi_write(QUICKVX_RCB_IER_REG, 0x00000000);

	ql_mddi_write(QUICKVX_RCB_RCR_REG, 0x80000010);

	ql_mddi_write(QUICKVX_RCB_CELLBCR_REG, 0x8008746F);

	ql_mddi_write(QUICKVX_RCB_CELLCC_REG, 0x800000A3);

	ql_mddi_write(QUICKVX_RCB_CONFDONE_REG, 0x00000001);
}

void mddi_quickvx_prim_lcd_init(void)
{
	uint32 value;

	MDDI_MSG_DEBUG("\n%s(): ", __func__);
	ql_send_spi_data_from_lcd(0, &value);

	ql_send_spi_cmd_to_lcd(0x0100, 0x3000); /* power control1 */
	ql_send_spi_cmd_to_lcd(0x0101, 0x4010); /* power control2 */
	ql_send_spi_cmd_to_lcd(0x0106, 0x0000); /* auto seq setting */
	mddi_wait(3);

	ql_mddi_write(QUICKVX_FB_A2F_LCD_RESET_REG, 0x00000001);
	mddi_wait(1);
	ql_mddi_write(QUICKVX_FB_A2F_LCD_RESET_REG, 0x00000000);
	mddi_wait(1);
	ql_mddi_write(QUICKVX_FB_A2F_LCD_RESET_REG, 0x00000001);
	mddi_wait(10);

	ql_send_spi_cmd_to_lcd(0x0001, 0x0310); /* driver out control */
	ql_send_spi_cmd_to_lcd(0x0002, 0x0100); /* lcd ac control */
	ql_send_spi_cmd_to_lcd(0x0003, 0x0000); /* entry mode */
	ql_send_spi_cmd_to_lcd(0x0007, 0x0000); /* disp cont1 */
	ql_send_spi_cmd_to_lcd(0x0008, 0x0004); /* disp cont2 */
	ql_send_spi_cmd_to_lcd(0x0009, 0x000C); /* disp cont3 */
	ql_send_spi_cmd_to_lcd(0x000C, 0x4010); /* disp if cont1 */
	ql_send_spi_cmd_to_lcd(0x000E, 0x0000); /* disp if cont2 */
	ql_send_spi_cmd_to_lcd(0x0020, 0x013F); /* panel if cont1 */
	ql_send_spi_cmd_to_lcd(0x0022, 0x7600); /* panel if cont3 */
	ql_send_spi_cmd_to_lcd(0x0023, 0x1C0A); /* panel if cont4 */
	ql_send_spi_cmd_to_lcd(0x0024, 0x1C2C); /* panel if cont5 */
	ql_send_spi_cmd_to_lcd(0x0025, 0x1C4E); /* panel if cont6 */
	ql_send_spi_cmd_to_lcd(0x0027, 0x0000); /* panel if cont8 */
	ql_send_spi_cmd_to_lcd(0x0028, 0x760C); /* panel if cont9 */
	ql_send_spi_cmd_to_lcd(0x0300, 0x0000); /* gamma adj0 */
	ql_send_spi_cmd_to_lcd(0x0301, 0x0502); /* gamma adj1 */
	ql_send_spi_cmd_to_lcd(0x0302, 0x0705); /* gamma adj2 */
	ql_send_spi_cmd_to_lcd(0x0303, 0x0000); /* gamma adj3 */
	ql_send_spi_cmd_to_lcd(0x0304, 0x0200); /* gamma adj4 */
	ql_send_spi_cmd_to_lcd(0x0305, 0x0707); /* gamma adj5 */
	ql_send_spi_cmd_to_lcd(0x0306, 0x1010); /* gamma adj6 */
	ql_send_spi_cmd_to_lcd(0x0307, 0x0202); /* gamma adj7 */
	ql_send_spi_cmd_to_lcd(0x0308, 0x0704); /* gamma adj8 */
	ql_send_spi_cmd_to_lcd(0x0309, 0x0707); /* gamma adj9 */
	ql_send_spi_cmd_to_lcd(0x030A, 0x0000); /* gamma adja */
	ql_send_spi_cmd_to_lcd(0x030B, 0x0000); /* gamma adjb */
	ql_send_spi_cmd_to_lcd(0x030C, 0x0707); /* gamma adjc */
	ql_send_spi_cmd_to_lcd(0x030D, 0x1010); /* gamma adjd */
	ql_send_spi_cmd_to_lcd(0x0310, 0x0104); /* gamma adj10 */
	ql_send_spi_cmd_to_lcd(0x0311, 0x0503); /* gamma adj11 */
	ql_send_spi_cmd_to_lcd(0x0312, 0x0304); /* gamma adj12 */
	ql_send_spi_cmd_to_lcd(0x0315, 0x0304); /* gamma adj15 */
	ql_send_spi_cmd_to_lcd(0x0316, 0x031C); /* gamma adj16 */
	ql_send_spi_cmd_to_lcd(0x0317, 0x0204); /* gamma adj17 */
	ql_send_spi_cmd_to_lcd(0x0318, 0x0402); /* gamma adj18 */
	ql_send_spi_cmd_to_lcd(0x0319, 0x0305); /* gamma adj19 */
	ql_send_spi_cmd_to_lcd(0x031C, 0x0707); /* gamma adj1c */
	ql_send_spi_cmd_to_lcd(0x031D, 0x021F); /* gamma adj1d */
	ql_send_spi_cmd_to_lcd(0x0320, 0x0507); /* gamma adj20 */
	ql_send_spi_cmd_to_lcd(0x0321, 0x0604); /* gamma adj21 */
	ql_send_spi_cmd_to_lcd(0x0322, 0x0405); /* gamma adj22 */
	ql_send_spi_cmd_to_lcd(0x0327, 0x0203); /* gamma adj27 */
	ql_send_spi_cmd_to_lcd(0x0328, 0x0300); /* gamma adj28 */
	ql_send_spi_cmd_to_lcd(0x0329, 0x0002); /* gamma adj29 */
	ql_send_spi_cmd_to_lcd(0x0100, 0x363C); /* power cont1 */
	mddi_wait(1);
	ql_send_spi_cmd_to_lcd(0x0101, 0x4003); /* power cont2 */
	ql_send_spi_cmd_to_lcd(0x0102, 0x0001); /* power cont3 */
	ql_send_spi_cmd_to_lcd(0x0103, 0x3C58); /* power cont4 */
	ql_send_spi_cmd_to_lcd(0x010C, 0x0135); /* power cont6 */
	ql_send_spi_cmd_to_lcd(0x0106, 0x0002); /* auto seq */
	ql_send_spi_cmd_to_lcd(0x0029, 0x03BF); /* panel if cont10 */
	ql_send_spi_cmd_to_lcd(0x0106, 0x0003); /* auto seq */
	mddi_wait(5);
	ql_send_spi_cmd_to_lcd(0x0101, 0x4010); /* power cont2 */
	mddi_wait(10);
}

/* Function to Power On the Primary and Secondary LCD panels */
static int mddi_quickvx_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	MDDI_MSG_DEBUG("\n%s(): ", __func__);
	mfd = platform_get_drvdata(pdev);

	if (!mfd) {
		MDDI_MSG_DEBUG("\n mddi_quickvx_lcd_on: Device not found!");
		return -ENODEV;
	}

	if (mfd->key != MFD_KEY) {
		MDDI_MSG_DEBUG("\n mddi_quickvx_lcd_on: Invalid MFD key!");
		return -EINVAL;
	}

	mddi_host_client_cnt_reset();
	mddi_quickvx_configure_registers();
	mddi_quickvx_prim_lcd_init();

	return 0;
}


/* Function to Power Off the Primary and Secondary LCD panels */
static int mddi_quickvx_lcd_off(struct platform_device *pdev)
{
	MDDI_MSG_DEBUG("\n%s(): ", __func__);
	mddi_wait(1);
	ql_send_spi_cmd_to_lcd(0x0106, 0x0002); /* Auto Sequencer setting */
	mddi_wait(10);
	ql_send_spi_cmd_to_lcd(0x0106, 0x0000); /* Auto Sequencer setting */
	ql_send_spi_cmd_to_lcd(0x0029, 0x0002); /* Panel IF control 10 */
	ql_send_spi_cmd_to_lcd(0x0100, 0x300D); /* Power Control 1 */
	mddi_wait(1);

	return 0;
}

/* Function to set the Backlight brightness level */
static void mddi_quickvx_lcd_set_backlight(struct msm_fb_data_type *mfd)
{
	int32 level, i = 0, ret;

	MDDI_MSG_DEBUG("%s(): ", __func__);

	level = mfd->bl_level;
	MDDI_MSG_DEBUG("\n level = %d", level);
	if (level < 0) {
		MDDI_MSG_DEBUG("mddi_quickvx_lcd_set_backlight: "
			"Invalid backlight level (%d)!\n", level);
		return;
	}
	while (i++ < 3) {
		ret = pmic_set_led_intensity(LED_LCD, level);
		if (ret == 0)
			return;
		msleep(10);
	}

	MDDI_MSG_DEBUG("%s: can't set lcd backlight!\n",
				__func__);
}

/* Driver Probe function */
static int __devinit mddi_quickvx_lcd_probe(struct platform_device *pdev)
{
	MDDI_MSG_DEBUG("\n%s(): id is %d", __func__, pdev->id);
	msm_fb_add_device(pdev);
	return 0;
}

/* Driver data structure */
static struct platform_driver this_driver = {
	.probe  = mddi_quickvx_lcd_probe,
	.driver	= {
		.name	= "mddi_quickvx",
	},
};


/* Primary LCD panel data structure */
static struct msm_fb_panel_data mddi_quickvx_panel_data0 = {
	.on					= mddi_quickvx_lcd_on,
	.off				= mddi_quickvx_lcd_off,
	.set_backlight		= mddi_quickvx_lcd_set_backlight,
};


/* Primary LCD panel device structure */
static struct platform_device this_device0 = {
	.name   = "mddi_quickvx",
	.id		= MDDI_QUICKVX_1_2,
	.dev	= {
		.platform_data = &mddi_quickvx_panel_data0,
	}
};

/* Module init - driver main entry point */
static int __init mddi_quickvx_lcd_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
	u32 cid;
	MDDI_MSG_DEBUG("\n%s(): ", __func__);

	ret = msm_fb_detect_client("mddi_quickvx");

	if (ret == -ENODEV)	{
		/* Device not found */
		MDDI_MSG_DEBUG("\n mddi_quickvx_lcd_init: No device found!");
		return 0;
	}

	if (ret) {
		cid = mddi_get_client_id();

		MDDI_MSG_DEBUG("\n cid = 0x%x", cid);
		if (((cid >> 16) != QUICKVX_MDDI_MFR_CODE) ||
			((cid & 0xFFFF) != QUICKVX_MDDI_PRD_CODE)) {
			/* MDDI Client ID not matching */
			MDDI_MSG_DEBUG("\n mddi_quickvx_lcd_init: "
				"Client ID missmatch!");

			return 0;
		}
		MDDI_MSG_DEBUG("\n mddi_quickvx_lcd_init: "
			"QuickVX LCD panel detected!");
	}

#endif /* CONFIG_FB_MSM_MDDI_AUTO_DETECT */

	mddi_quickvx_rows_per_refresh = 872;
	mddi_quickvx_rows_per_second = 52364;
	mddi_quickvx_usecs_per_refresh = 16574;

	ret = platform_driver_register(&this_driver);

	if (!ret) {
		pinfo = &mddi_quickvx_panel_data0.panel_info;
		pinfo->xres = 480;
		pinfo->yres = 864;
		MSM_FB_SINGLE_MODE_PANEL(pinfo);
		pinfo->type = MDDI_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
		pinfo->wait_cycle = 0;
		pinfo->bpp = 24;
		pinfo->fb_num = 2;

		pinfo->clk_rate = 192000000;
		pinfo->clk_min = 192000000;
		pinfo->clk_max = 200000000;
		pinfo->lcd.rev = 1;
		pinfo->lcd.vsync_enable = TRUE;
		pinfo->lcd.refx100 = (mddi_quickvx_rows_per_second \
			* 100)/mddi_quickvx_rows_per_refresh;
		pinfo->lcd.v_back_porch = 4;
		pinfo->lcd.v_front_porch = 2;
		pinfo->lcd.v_pulse_width = 2;
		pinfo->lcd.hw_vsync_mode = TRUE;
		pinfo->lcd.vsync_notifier_period = (1 * HZ);
		pinfo->bl_max = 10;
		pinfo->bl_min = 0;

		ret = platform_device_register(&this_device0);
		if (ret) {
			platform_driver_unregister(&this_driver);
			MDDI_MSG_DEBUG("mddi_quickvx_lcd_init: "
				"Primary device registration failed!\n");
		}
	}

	return ret;
}

module_init(mddi_quickvx_lcd_init);

