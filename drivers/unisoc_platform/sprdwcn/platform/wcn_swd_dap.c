/*
 * Copyright (C) 2019 Unisoc Technologies Inc.
 *
 * Author:	xiaodong.bi
 * File:	wcn_swd_dap.c
 * Description:	Marlin Debug System main file. Dump arm registers
 * or access other address by swd dap method.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <misc/wcn_bus.h>
#include "wcn_swd_dap.h"
#include "../include/wcn_dbg.h"

/*
 * PCIe have mapped cp global 128 bits registers to bar4 base address.
 * But when read or write cp address directly, swd dap access will fail.
 * When read or write mapped bar4 address, swd dap access will success.
 * Because of this particularity, the following functions can not be combined
 * into one.
 */

/* select swd external: enable:1; disable:0 */
static void swd_ext_sel_pcie(bool enable)
{
	struct edma_info *edma = edma_info();
	unsigned int ahb_ctl, *reg;
	int ret;

	ahb_ctl = CM33_AHB_CTRL3_VALUE;
	ret = sprdwcn_bus_reg_write(CM33_AHB_CTRL3_ADDR, &ahb_ctl, 4);
	if (ret < 0)
		WCN_ERR("write CM33_AHB_CTRL3 reg error:%d\n", ret);
	ret = sprdwcn_bus_reg_read(CM33_AHB_CTRL3_ADDR, &ahb_ctl, 4);
	if (ret < 0)
		WCN_ERR("read CM33_AHB_CTRL3 reg error:%d\n", ret);
	WCN_INFO("ahb_ctl value is:0x%x\n", ahb_ctl);

	reg = (unsigned int *)(pcie_bar_vmem(edma->pcie_info, 4) + DAP_ADDR_PCIE);
	if (enable)
		*reg |= (BIT_SEL_MTCKMS_PCIE | BIT_IN_MTCK_PCIE | BIT_IN_MTMS_PCIE);
	else
		*reg &= ~(BIT_SEL_MTCKMS_PCIE | BIT_IN_MTCK_PCIE | BIT_IN_MTMS_PCIE);
}

/* clear swd clock */
static void swclk_clr_pcie(void)
{
	struct edma_info *edma = edma_info();
	unsigned int *reg;

	reg = (unsigned int *)(pcie_bar_vmem(edma->pcie_info, 4) + DAP_ADDR_PCIE);
	*reg &= ~BIT_IN_MTCK_PCIE;
}

/* set swd clock */
static void swclk_set_pcie(void)
{
	struct edma_info *edma = edma_info();
	unsigned int *reg;

	reg = (unsigned int *)(pcie_bar_vmem(edma->pcie_info, 4) + DAP_ADDR_PCIE);
	*reg |= BIT_IN_MTCK_PCIE;
}

/*
 * According to the lowest bit of value, writing BIT_IN_MTMS by swdio
 * If the lowest bit of value is 1, set BIT_IN_MTMS;
 * If the lowest bit of value is 0, clear BIT_IN_MTMS.
 */
static void sw_dio_out_pcie(unsigned int value)
{
	struct edma_info *edma = edma_info();
	unsigned int *reg;

	reg = (unsigned int *)(pcie_bar_vmem(edma->pcie_info, 4) + DAP_ADDR_PCIE);
	if (value & LOWEST_BIT_OF_VALUE)
		*reg |= BIT_IN_MTMS_PCIE;
	else
		*reg &= ~BIT_IN_MTMS_PCIE;
}

/* Read bit by swdio */
static unsigned int sw_dio_in_pcie(void)
{
	struct edma_info *edma = edma_info();
	unsigned int *reg, bit;

	reg = (unsigned int *)(pcie_bar_vmem(edma->pcie_info, 4) +
			       DAP_ACK_ADDR_PCIE);
	bit = (*reg & BIT_OUT_MTMS) >> 7;

	return bit;
}

/* select swd external: enable:1; disable:0 */
static void swd_ext_sel(bool enable)
{
	unsigned char reg;
	unsigned int ahb_ctl;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
		swd_ext_sel_pcie(enable);
		return;
	}

	sprdwcn_bus_aon_readb(DAP_ADDR, &reg);
	WCN_INFO("DAP_ADDR reg value is:0x%x\n", reg);

	/* disable sec */
	ahb_ctl = CM33_AHB_CTRL3_VALUE;
	sprdwcn_bus_reg_write(CM33_AHB_CTRL3_ADDR, &ahb_ctl, 4);
	ahb_ctl = 0;
	sprdwcn_bus_reg_read(CM33_AHB_CTRL3_ADDR, &ahb_ctl, 4);
	WCN_INFO("ahb_ctl value is:0x%x\n", ahb_ctl);

	if (enable)
		reg |= (BIT_SEL_MTCKMS | BIT_IN_MTCK | BIT_IN_MTMS);
	else
		reg &= ~(BIT_SEL_MTCKMS | BIT_IN_MTCK | BIT_IN_MTMS);
	sprdwcn_bus_aon_writeb(DAP_ADDR, reg);
	sprdwcn_bus_aon_readb(DAP_ADDR, &reg);
	WCN_INFO("reg value is:0x%x\n", reg);
}

/* clear swd clock */
static void swclk_clr(void)
{
	unsigned char reg;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
		swclk_clr_pcie();
		return;
	}

	sprdwcn_bus_aon_readb(DAP_ADDR, &reg);
	reg &= ~BIT_IN_MTCK;
	sprdwcn_bus_aon_writeb(DAP_ADDR, reg);
}

/* set swd clock */
static void swclk_set(void)
{
	unsigned char reg;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
		swclk_set_pcie();
		return;
	}

	sprdwcn_bus_aon_readb(DAP_ADDR, &reg);
	reg |= BIT_IN_MTCK;
	sprdwcn_bus_aon_writeb(DAP_ADDR, reg);
}

/*
 * According to the lowest bit of value, writing BIT_IN_MTMS by swdio
 * If the lowest bit of value is 1, set BIT_IN_MTMS;
 * If the lowest bit of value is 0, clear BIT_IN_MTMS.
 */
static void sw_dio_out(unsigned int value)
{
	unsigned char reg;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
		sw_dio_out_pcie(value);
		return;
	}

	sprdwcn_bus_aon_readb(DAP_ADDR, &reg);
	if (value & LOWEST_BIT_OF_VALUE)
		reg |= BIT_IN_MTMS;
	else
		reg &= ~BIT_IN_MTMS;
	sprdwcn_bus_aon_writeb(DAP_ADDR, reg);
}

/* Read bit by swdio */
static unsigned int sw_dio_in(void)
{
	unsigned char reg;
	unsigned int bit;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie)
		return sw_dio_in_pcie();

	sprdwcn_bus_aon_readb(DAP_ACK_ADDR, &reg);
	bit = (reg & BIT_OUT_MTMS) >> 7;

	return bit;
}

static void swd_clk_cycle(void)
{
	swclk_set();
	ndelay(100);
	swclk_clr();
	ndelay(100);
}

/* Write bit with 100ns delay before and after */
static void swd_write_bit(unsigned int bit)
{
	swclk_set();
	sw_dio_out(bit);
	ndelay(100);
	swclk_clr();
	ndelay(100);
}

/* Read bit with 100ns delay before and after */
static unsigned int swd_read_bit(void)
{
	unsigned int bit;

	swclk_set();
	ndelay(100);
	bit = sw_dio_in();
	swclk_clr();
	ndelay(100);

	return bit;
}

static void swd_insert_cycles(unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		swd_clk_cycle();
}

/*
 * transfer cmd with data;
 * cmd: DAP_TRANSFER_AP_DP / DAP_TRANSFER_RW /...
 * data: need to transfer
 * ack is equal to DAP_TRANSFER_OK, means transfer ok.
 */
unsigned int swd_transfer(unsigned char cmd, unsigned int *data)
{
	unsigned int ack = 0, bit, val, parity, n;

	parity = 0;
	/* Start Bit */
	swd_write_bit(1);
	bit = ((cmd >> 0) & LOWEST_BIT_OF_VALUE);
	/* APnDP Bit */
	swd_write_bit(bit);
	parity += bit;
	bit = ((cmd >> 1) & LOWEST_BIT_OF_VALUE);
	/* RnW Bit */
	swd_write_bit(bit);
	parity += bit;
	bit = ((cmd >> 2) & LOWEST_BIT_OF_VALUE);
	/* A2 Bit */
	swd_write_bit(bit);
	parity += bit;
	bit = ((cmd >> 3) & LOWEST_BIT_OF_VALUE);
	/* A3 Bit */
	swd_write_bit(bit);
	parity += bit;
	/* Parity Bit */
	swd_write_bit(parity & LOWEST_BIT_OF_VALUE);
	/* Stop Bit */
	swd_write_bit(0);
	/* Park Bit */
	swd_write_bit(1);

	/* Turnaround */
	swd_insert_cycles(1);

	/* Acknowledge response */
	bit = swd_read_bit();
	ack |= bit << 0;
	bit = swd_read_bit();
	ack |= bit << 1;
	bit = swd_read_bit();
	ack |= bit << 2;

	if (ack == DAP_TRANSFER_OK) {
		/* Data transfer */
		if (cmd & DAP_TRANSFER_RW) {
			val = 0;
			parity = 0;
			/* Read DATA[0:31] */
			for (n = 0; n < 32; n++) {
				bit = swd_read_bit();
				parity += bit;
				val |= (bit << n);
			}

			/* Read Parity */
			bit = swd_read_bit();
			if ((parity ^ bit) & 1)
				ack = DAP_TRANSFER_ERROR;

			if (data)
				*data = val;

			/* Turnaround */
			swd_insert_cycles(1);
		} else {
			/* Turnaround */
			swd_insert_cycles(1);
			sw_dio_out(0);

			val = *data;
			parity = 0;
			/* Write WDATA[0:31] */
			for (n = 0; n < 32; n++) {
				bit = val & LOWEST_BIT_OF_VALUE;
				swd_write_bit(bit);
				parity += bit;
				val >>= 1;
			}
			/* Write Parity Bit */
			swd_write_bit(parity & LOWEST_BIT_OF_VALUE);
			/* Turnaround */
			swd_insert_cycles(1);
		}

		/* Idle cycles >= 8 */
		sw_dio_out(0);
		swd_insert_cycles(8);

		return ack;
	}

	WCN_ERR("%s ack:0x%x\n", __func__, ack);
	if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {
		if ((cmd & DAP_TRANSFER_RW) != 0)
			swd_insert_cycles(32+1);
		else {
			/* Turnaround */
			swd_insert_cycles(1);
			if ((cmd & DAP_TRANSFER_RW) == 0)
				swd_insert_cycles(32 + 1);
		}

		/* Idle cycles >= 8 */
		sw_dio_out(0);
		swd_insert_cycles(8);

		return ack;
	}

	/* Turnaround */
	swd_insert_cycles(1);
	swd_insert_cycles(32+1);

	/* Idle cycles >= 8 */
	sw_dio_out(0);
	swd_insert_cycles(8);

	return ack;
}

static void swd_send_nbytes(unsigned char *buf, int nbytes)
{
	unsigned char i, j, dat;

	for (i = 0; i < nbytes; i++) {
		dat = buf[i];
		for (j = 0; j < 8; j++) {
			if ((dat & 0x80) == 0x80)
				swd_write_bit(1);
			else
				swd_write_bit(0);
			dat <<= 1;
		}
	}
}

static unsigned int swd_dap_read(unsigned char reg, unsigned int *data)
{
	unsigned int ack;

	reg &= ~DAP_TRANSFER_AP_DP;
	reg |= DAP_TRANSFER_RW;
	ack = swd_transfer(reg, data);

	return ack;
}

static unsigned int swd_dap_write(unsigned char reg, unsigned int *data)
{
	unsigned int ack;

	reg &= ~(DAP_TRANSFER_AP_DP | DAP_TRANSFER_RW);
	ack = swd_transfer(reg, data);

	return ack;
}

static unsigned int swd_ap_read(unsigned char reg, unsigned int *data)
{
	unsigned int ack;

	reg |= DAP_TRANSFER_AP_DP | DAP_TRANSFER_RW;
	ack = swd_transfer(reg, data);

	return ack;
}

static unsigned int swd_ap_write(unsigned int reg, unsigned int *data)
{
	unsigned int ack;

	reg &= ~(DAP_TRANSFER_RW);
	reg |= DAP_TRANSFER_AP_DP;
	ack = swd_transfer(reg, data);

	return ack;
}

static unsigned int swd_memap_read(unsigned int addr,
	unsigned int *data)
{
	unsigned char reg;
	unsigned int ack;

	reg = DAP_TRANSFER_A2;
	ack = swd_ap_write(reg, &addr);

	reg = DAP_TRANSFER_A2 | DAP_TRANSFER_A3;
	/* Need to read twice, otherwise it will fail. */
	ack = swd_ap_read(reg, data);
	ack = swd_ap_read(reg, data);

	return ack;
}

static unsigned int swd_memap_write(unsigned int addr,
	unsigned int *data)
{
	unsigned char reg;
	unsigned int ack;

	reg = DAP_TRANSFER_A2;
	ack = swd_ap_write(reg, &addr);

	reg = DAP_TRANSFER_A2 | DAP_TRANSFER_A3;
	ack = swd_ap_write(reg, data);

	return ack;
}

static void swd_read_arm_core(void)
{
	unsigned int value, index, addr;
	static const char *core_reg_name[19] = {
		"R0 ", "R1 ", "R2 ", "R3 ", "R4 ", "R5 ", "R6 ", "R7 ", "R8 ",
		"R9 ", "R10", "R11", "R12", "R13", "R14", "R15", "PSR", "MSP",
		"PSP",
	};

	/* reg arm reg */
	for (index = 0; index < 19; index++) {
		addr = 0xe000edf4;
		swd_memap_write(addr, &index);
		addr = 0xe000edf8;
		swd_memap_read(addr, &value);
		WCN_INFO("%s %s:0x%x\n", __func__, core_reg_name[index],
			 value);
	}
}

/* MSB first */
unsigned char swd_wakeup_seq[16] = {
	0x49, 0xCF, 0x90, 0x46,
	0xA9, 0xB4, 0xA1, 0x61,
	0x97, 0xF5, 0xBB, 0xC7,
	0x45, 0x70, 0x3D, 0x98,
};

/* LSB first */
unsigned char swd_wakeup_seq2[16] = {
	0x19, 0xBC, 0x0E, 0xA2,
	0xE3, 0xDD, 0xAF, 0xE9,
	0x86, 0x85, 0x2D, 0x95,
	0x62, 0x09, 0xF3, 0x92,
};

unsigned char swd_to_ds_seq[2] = {
	0x3d, 0xc7,
};

static void swd_dormant_to_wake(void)
{
	unsigned char data;

	/* Send at least eight SWCLKTCK cycles with SWDIOTMS HIGH */
	sw_dio_out(1);
	swd_insert_cycles(8);

	/* 128 bit Selection Alert sequence */
	swd_send_nbytes(swd_wakeup_seq, 16);

	/* four SWCLKTCK cycles with SWDIOTMS LOW */
	sw_dio_out(0);
	swd_insert_cycles(4);

	/* Send the activation code */
	data = 0x58;
	swd_send_nbytes(&data, 1);

	swd_insert_cycles(1);
	sw_dio_out(0);
}

static void swd_wake_to_dormant(void)
{
	/* Send at least eight SWCLKTCK cycles with SWDIOTMS HIGH */
	sw_dio_out(1);
	swd_insert_cycles(8);

	/* 16-bit SWD-to-DS select sequence */
	swd_send_nbytes(swd_to_ds_seq, 2);
	swd_insert_cycles(1);
}

static void switch_jtag_to_swd(void)
{
	unsigned char data[2];

	/* Send at least 50 SWCLKTCK cycles with SWDIOTMS HIGH */
	sw_dio_out(1);
	swd_insert_cycles(50);

	/* send the 16-bit JTAG-to-SWD select sequence */
	data[0] = 0x79;
	data[1] = 0xe7;
	swd_send_nbytes(data, 2);

	/* Send at least 50 SWCLKTCK cycles with SWDIOTMS HIGH */
	swd_insert_cycles(50);
	sw_dio_out(0);
}

/*
 * Complete SWD reset sequence
 * (50 cycles high followed by 2 or more idle cycles)
 */
static void swd_line_reset(void)
{
	sw_dio_out(1);
	swd_insert_cycles(50);
	sw_dio_out(0);
	swd_insert_cycles(2);
}

static void swd_read_dpidr(void)
{
	unsigned int ack, data = 0;

	/* the dp idr is 0x0be12477*/
	ack = swd_dap_read(DP_IDCODE, &data);

	WCN_INFO("%s idcode:0x%x\n", __func__, data);
}

static void swd_read_apidr(void)
{
	unsigned int ack, data = 0;

	data = (0xf << 4);
	ack = swd_dap_write(DP_SELECT, &data);

	/* the dp idr is 0x14770015*/
	ack = swd_ap_read(AP_IDCODE, &data);
	ack = swd_ap_read(AP_IDCODE, &data);
	WCN_INFO("%s idcode:0x%x\n", __func__, data);

	data = (0x0 << 4);
	ack = swd_dap_write(DP_SELECT, &data);
}

unsigned int swd_sel_target(unsigned char cmd, unsigned int *data)
{
	unsigned int bit, parity;
	unsigned int n, val;

	sw_dio_out(1);
	swd_insert_cycles(50);

	sw_dio_out(0);
	swd_insert_cycles(2);

	parity = 0;
	/* Start Bit */
	swd_write_bit(1);
	bit = ((cmd >> 0) & LOWEST_BIT_OF_VALUE);
	/* APnDP Bit */
	swd_write_bit(bit);
	parity += bit;
	bit = ((cmd >> 1) & LOWEST_BIT_OF_VALUE);
	/* RnW Bit */
	swd_write_bit(bit);
	parity += bit;
	bit = ((cmd >> 2) & LOWEST_BIT_OF_VALUE);
	/* A2 Bit */
	swd_write_bit(bit);
	parity += bit;
	bit = ((cmd >> 3) & LOWEST_BIT_OF_VALUE);
	/* A3 Bit */
	swd_write_bit(bit);
	parity += bit;
	/* Parity Bit */
	swd_write_bit(parity & LOWEST_BIT_OF_VALUE);
	/* Stop Bit */
	swd_write_bit(0);
	/* Park Bit */
	swd_write_bit(1);

	/* Turnaround */
	swd_insert_cycles(5);

	val = *data;
	parity = 0;
	/* Write WDATA[0:31] */
	for (n = 0; n < 32; n++) {
		bit = (val & LOWEST_BIT_OF_VALUE);
		swd_write_bit(bit);
		parity += bit;
		val >>= 1;
	}
	/* Write Parity Bit */
	swd_write_bit(parity & LOWEST_BIT_OF_VALUE);

	sw_dio_out(0);
	swd_insert_cycles(3);

	return 0;
}

static void btwf_sys_dap_sel(void)
{
	unsigned int data = 0;

	swd_line_reset();
	data = TARGETSEL_CP;
	swd_sel_target(DP_TARGETSEL, &data);
	swd_read_dpidr();
}

static int swd_power_up(void)
{
	unsigned int data;

	WCN_INFO("%s entry\n", __func__);

	data = SWD_POWERUP;
	swd_dap_write(DP_CTRL_STAT, &data);
	swd_dap_read(DP_CTRL_STAT, &data);

	WCN_INFO("%s read ctrl stat:0x%x\n", __func__, data);

	return 0;
}

static void swd_device_en(void)
{
	unsigned int data = 0;

	swd_ap_read(AP_CTRL, &data);
	swd_ap_read(AP_CTRL, &data);
	data = (data & 0xffffff88) | SWD_DEVICE_EN_1 | SWD_DEVICE_EN_6;
	swd_ap_write(AP_STAT, &data);
}

/*
 * Debug Exception and Monitor Control Register
 * (0xe000edfC) = 0x010007f1
 */
void swd_set_debug_mode(void)
{
	int ret;
	unsigned int reg_val;

	reg_val = DEBUG_EXCEPTION_MONITOR_CTRL_VAL;
	ret = swd_memap_write(DEBUG_EXCEPTION_MONITOR_CTRL_REG, &reg_val);
	if (ret < 0) {
		WCN_ERR("%s write error:%d\n", __func__, ret);
		return;
	}

	/*test */
	ret = swd_memap_read(DEBUG_EXCEPTION_MONITOR_CTRL_REG, &reg_val);
	if (ret < 0) {
		WCN_ERR("%s read error:%d\n", __func__, ret);
		return;
	}
	WCN_INFO("%s arm debug reg value is 0x%x:\n", __func__, reg_val);
}

/*
 * Debug Halting Control status Register
 * (0xe000edf0) = 0xa05f0003
 */
void swd_hold_btwf_core(void)
{
	int ret;
	unsigned int reg_val;

	reg_val = DEBUG_HALTING_CTRL_STATUS_VAL;
	ret = swd_memap_write(DEBUG_HALTING_CTRL_STATUS_REG, &reg_val);
	if (ret < 0) {
		WCN_ERR("%s write error:%d\n", __func__, ret);
		return;
	}

	ret = swd_memap_read(DEBUG_HALTING_CTRL_STATUS_REG, &reg_val);
	if (ret < 0) {
		WCN_ERR("%s read error:%d\n", __func__, ret);
		return;
	}
	WCN_INFO("%s arm hold btwf reg value is 0x%x:\n", __func__, reg_val);
}

int swd_dump_arm_reg(void)
{
	WCN_INFO("%s entry\n", __func__);

	swd_ext_sel(true);
	swd_line_reset();

	switch_jtag_to_swd();
	swd_dormant_to_wake();
	btwf_sys_dap_sel();

	swd_power_up();
	swd_read_apidr();
	swd_device_en();
	swd_hold_btwf_core();
	swd_set_debug_mode();
	swd_read_arm_core();

	/* release swd */
	swd_wake_to_dormant();
	swd_ext_sel(false);

	WCN_INFO("%s end\n", __func__);

	return 0;
}
