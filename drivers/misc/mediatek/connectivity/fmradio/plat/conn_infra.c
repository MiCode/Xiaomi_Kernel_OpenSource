/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "plat.h"

/* CONNSYS register address */
#if CFG_FM_CONNAC2
#define AP_BASE_ADDRESS             0x18000000
#define MCU_CFG_ADDR                (AP_BASE_ADDRESS + 0x0)
#define MCU_CFG_CONSYS_BASE         MCU_CFG_ADDR
#define MCU_CFG_SIZE                0x10000
#define TOP_MISC_OFF_ADDR           (AP_BASE_ADDRESS + 0x60000)
#define TOP_MISC_OFF_CONSYS_BASE    TOP_MISC_OFF_ADDR
#define TOP_MISC_OFF_SIZE           0x1000
#define TOP_RF_SPI_AON_ADDR         (AP_BASE_ADDRESS + 0x4000)
#define TOP_RF_SPI_AON_CONSYS_BASE  TOP_RF_SPI_AON_ADDR
#define TOP_RF_SPI_AON_SIZE         0x1000
#else
#define AP_BASE_ADDRESS             0x18000000
#define MCU_CFG_ADDR                (AP_BASE_ADDRESS + 0x02000)
#define MCU_CFG_CONSYS_BASE         0x80000000
#define MCU_CFG_SIZE                0x1000
#define TOP_MISC_OFF_ADDR           (AP_BASE_ADDRESS + 0xB0000)
#define TOP_MISC_OFF_CONSYS_BASE    0x80020000
#define TOP_MISC_OFF_SIZE           0x10000
#define TOP_RF_SPI_AON_ADDR         (AP_BASE_ADDRESS + 0xC0000)
#define TOP_RF_SPI_AON_CONSYS_BASE  0x81020000
#define TOP_RF_SPI_AON_SIZE         0x10000
#endif /* CFG_FM_CONNAC2 */

#define FM_IRQ_NUMBER 0
#define MAX_SET_OWN_COUNT    1000

#if CFG_FM_CONNAC2
static int (*whole_chip_reset)(signed int sta);

static int fm_pre_whole_chip_rst(enum consys_drv_type drv, char *reason)
{
	WCN_DBG(FM_WAR | LINK, "FM pre whole chip rst!\n");
	if (whole_chip_reset)
		whole_chip_reset(1);
	return 0;
}

static int fm_post_whole_chip_rst(void)
{
	WCN_DBG(FM_WAR | LINK, "FM post whole chip rst!\n");
	if (whole_chip_reset)
		whole_chip_reset(0);
	return 0;
}

static struct sub_drv_ops_cb fm_drv_cbs = {
	.rst_cb.pre_whole_chip_rst = fm_pre_whole_chip_rst,
	.rst_cb.post_whole_chip_rst = fm_post_whole_chip_rst,
	.pre_cal_cb.pwr_on_cb = NULL,
	.pre_cal_cb.do_cal_cb = NULL,
};

static int drv_sys_spi_read(
	struct fm_spi_interface *si, unsigned int subsystem,
	unsigned int addr, unsigned int *data)
{
	WCN_DBG(FM_DBG | CHIP, "[0x%08x]=[0x%08x]\n", addr, *data);
	return conninfra_spi_read(subsystem, addr, data);
}

static int drv_sys_spi_write(
	struct fm_spi_interface *si, unsigned int subsystem,
	unsigned int addr, unsigned int data)
{
	WCN_DBG(FM_DBG | CHIP, "[0x%08x]=[0x%08x]\n", addr, data);
	return conninfra_spi_write(subsystem, addr, data);
}
#else /* CFG_FM_CONNAC2 */
static int sys_spi_wait(unsigned int spi_busy)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	unsigned int spi_count, rdata;

	/* It needs to prevent infinite loop */
	for (spi_count = 0; spi_count < FM_SPI_COUNT_LIMIT; spi_count++) {
		si->spi_read(si, SYS_SPI_STA, &rdata);
		if ((rdata & spi_busy) == 0)
			break;
	}
	if (spi_count == FM_SPI_COUNT_LIMIT) {
		WCN_DBG(FM_WAR | CHIP,
			"SPI busy[0x%08x], retry count reached maximum.\n",
			rdata);
		return FM_SYS_SPI_BUSY;
	}
	return FM_SYS_SPI_OK;
}

static int fm_sys_spi_read(
	struct fm_spi_interface *si, unsigned int subsystem,
	unsigned int addr, unsigned int *data)
{
	unsigned int spi_busy, spi_addr, spi_mask, spi_wdat, spi_rdat, rdata;

	if (!si->spi_read || !si->spi_write) {
		WCN_DBG(FM_ERR | CHIP, "spi api is NULL.\n");
		return FM_SYS_SPI_ERR;
	}

	switch (subsystem) {
	case SYS_SPI_WF:
		spi_busy =  1 << SYS_SPI_STA_WF_BUSY_SHFT;
		spi_addr = SYS_SPI_WF_ADDR_ADDR;
		spi_mask = SYS_SPI_WF_RDAT_MASK;
		spi_wdat = SYS_SPI_WF_WDAT_ADDR;
		spi_rdat = SYS_SPI_WF_RDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_READ | SYS_SPI_ADDR_CR_WF | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_BT:
		spi_busy =  1 << SYS_SPI_STA_BT_BUSY_SHFT;
		spi_addr = SYS_SPI_BT_ADDR_ADDR;
		spi_mask = SYS_SPI_BT_RDAT_MASK;
		spi_wdat = SYS_SPI_BT_WDAT_ADDR;
		spi_rdat = SYS_SPI_BT_RDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_READ | SYS_SPI_ADDR_CR_BT | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_FM:
		spi_busy =  1 << SYS_SPI_STA_FM_BUSY_SHFT;
		spi_addr = SYS_SPI_FM_ADDR_ADDR;
		spi_mask = SYS_SPI_FM_RDAT_MASK;
		spi_wdat = SYS_SPI_FM_WDAT_ADDR;
		spi_rdat = SYS_SPI_FM_RDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_READ | SYS_SPI_ADDR_CR_FM | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_GPS:
		spi_busy =  1 << SYS_SPI_STA_GPS_BUSY_SHFT;
		spi_addr = SYS_SPI_GPS_ADDR_ADDR;
		spi_mask = SYS_SPI_GPS_RDAT_MASK;
		spi_wdat = SYS_SPI_GPS_WDAT_ADDR;
		spi_rdat = SYS_SPI_GPS_RDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_READ | SYS_SPI_ADDR_CR_GPS | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_TOP:
		spi_busy =  1 << SYS_SPI_STA_TOP_BUSY_SHFT;
		spi_addr = SYS_SPI_TOP_ADDR_ADDR;
		spi_mask = SYS_SPI_TOP_RDAT_MASK;
		spi_wdat = SYS_SPI_TOP_WDAT_ADDR;
		spi_rdat = SYS_SPI_TOP_RDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_READ | SYS_SPI_ADDR_CR_TOP | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_WF1:
		spi_busy =  1 << SYS_SPI_STA_WF_BUSY_SHFT;
		spi_addr = SYS_SPI_WF_ADDR_ADDR;
		spi_mask = SYS_SPI_WF_RDAT_MASK;
		spi_wdat = SYS_SPI_WF_WDAT_ADDR;
		spi_rdat = SYS_SPI_WF_RDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_READ | SYS_SPI_ADDR_CR_WF1 | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	default:
		return FM_SYS_SPI_ERR;
	}

	if (sys_spi_wait(spi_busy) == FM_SYS_SPI_BUSY)
		return FM_SYS_SPI_BUSY;

	si->spi_write(si, spi_addr, addr);
	si->spi_write(si, spi_wdat, 0);

	sys_spi_wait(spi_busy);

	si->spi_read(si, spi_rdat, &rdata);
	*data = rdata & spi_mask;

	WCN_DBG(FM_DBG | CHIP, "[0x%08x]=[0x%08x]\n", addr, *data);

	return FM_SYS_SPI_OK;
}

static int fm_sys_spi_write(
	struct fm_spi_interface *si, unsigned int subsystem,
	unsigned int addr, unsigned int data)
{
	unsigned int spi_busy, spi_addr, spi_mask, spi_wdat;

	if (!si->spi_read || !si->spi_write) {
		WCN_DBG(FM_ERR | CHIP, "spi api is NULL.\n");
		return FM_SYS_SPI_ERR;
	}

	switch (subsystem) {
	case SYS_SPI_WF:
		spi_busy =  1 << SYS_SPI_STA_WF_BUSY_SHFT;
		spi_addr = SYS_SPI_WF_ADDR_ADDR;
		spi_mask = SYS_SPI_WF_WDAT_MASK;
		spi_wdat = SYS_SPI_WF_WDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_WRITE | SYS_SPI_ADDR_CR_WF | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_BT:
		spi_busy =  1 << SYS_SPI_STA_BT_BUSY_SHFT;
		spi_addr = SYS_SPI_BT_ADDR_ADDR;
		spi_mask = SYS_SPI_BT_WDAT_MASK;
		spi_wdat = SYS_SPI_BT_WDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_WRITE | SYS_SPI_ADDR_CR_BT | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_FM:
		spi_busy =  1 << SYS_SPI_STA_FM_BUSY_SHFT;
		spi_addr = SYS_SPI_FM_ADDR_ADDR;
		spi_mask = SYS_SPI_FM_WDAT_MASK;
		spi_wdat = SYS_SPI_FM_WDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_WRITE | SYS_SPI_ADDR_CR_FM | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_TOP:
		spi_busy =  1 << SYS_SPI_STA_TOP_BUSY_SHFT;
		spi_addr = SYS_SPI_TOP_ADDR_ADDR;
		spi_mask = SYS_SPI_TOP_WDAT_MASK;
		spi_wdat = SYS_SPI_TOP_WDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_WRITE | SYS_SPI_ADDR_CR_TOP | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	case SYS_SPI_WF1:
		spi_busy =  1 << SYS_SPI_STA_WF_BUSY_SHFT;
		spi_addr = SYS_SPI_WF_ADDR_ADDR;
		spi_mask = SYS_SPI_WF_WDAT_MASK;
		spi_wdat = SYS_SPI_WF_WDAT_ADDR;
		addr = SYS_SPI_ADDR_CR_WRITE | SYS_SPI_ADDR_CR_WF1 | (addr & SYS_SPI_ADDR_CR_MASK);
		break;
	default:
		return FM_SYS_SPI_ERR;
	}

	if (sys_spi_wait(spi_busy) == FM_SYS_SPI_BUSY)
		return FM_SYS_SPI_BUSY;

	si->spi_write(si, spi_addr, addr);
	si->spi_write(si, spi_wdat, (data & spi_mask));

	sys_spi_wait(spi_busy);

	WCN_DBG(FM_DBG | CHIP, "[0x%08x]=[0x%08x]\n", addr, data);

	return FM_SYS_SPI_OK;
}
#endif /* CFG_FM_CONNAC2 */

static void drv_spi_read(
	struct fm_spi_interface *si, unsigned int addr, unsigned int *val)
{
	*val = readl(si->info.spi_vir_addr + addr);
}

static void drv_spi_write(
	struct fm_spi_interface *si, unsigned int addr, unsigned int val)
{
	writel(val, si->info.spi_vir_addr + addr);
}

static void drv_top_read(
	struct fm_spi_interface *si, unsigned int addr, unsigned int *val)
{
	*val = readl(si->info.top_vir_addr + addr);
}

static void drv_top_write(
	struct fm_spi_interface *si, unsigned int addr, unsigned int val)
{
	writel(val, si->info.top_vir_addr + addr);
}

static void drv_mcu_read(
	struct fm_spi_interface *si, unsigned int addr, unsigned int *val)
{
	*val = readl(si->info.mcu_vir_addr + addr);
}

static void drv_mcu_write(
	struct fm_spi_interface *si, unsigned int addr, unsigned int val)
{
	writel(val, si->info.mcu_vir_addr + addr);
}

static void drv_host_read(
	struct fm_spi_interface *si, unsigned int addr, unsigned int *data)
{
	unsigned new_addr = addr;

	if (addr >= TOP_RF_SPI_AON_CONSYS_BASE &&
	    addr <= TOP_RF_SPI_AON_CONSYS_BASE + TOP_RF_SPI_AON_SIZE) {
		new_addr = addr - TOP_RF_SPI_AON_CONSYS_BASE;
		drv_spi_read(si, new_addr, data);
	} else if (addr >= MCU_CFG_CONSYS_BASE &&
		   addr <= MCU_CFG_CONSYS_BASE + MCU_CFG_SIZE) {
		new_addr = addr - MCU_CFG_CONSYS_BASE;
		drv_mcu_read(si, new_addr, data);
	} else if (addr >= TOP_MISC_OFF_CONSYS_BASE &&
		   addr <= TOP_MISC_OFF_CONSYS_BASE + TOP_MISC_OFF_SIZE) {
		new_addr = addr - TOP_MISC_OFF_CONSYS_BASE;
		drv_top_read(si, new_addr, data);
	} else {
		WCN_DBG(FM_WAR | CHIP, "not support addr[0x%08x].\n", addr);
		return;
	}

	WCN_DBG(FM_DBG | CHIP, "read [0x%08x]=[0x%08x]\n",
		new_addr, addr, *data);
}

static void drv_host_write(
	struct fm_spi_interface *si, unsigned int addr, unsigned int data)
{
	unsigned new_addr = addr;

	if (addr >= TOP_RF_SPI_AON_CONSYS_BASE &&
	    addr <= TOP_RF_SPI_AON_CONSYS_BASE + TOP_RF_SPI_AON_SIZE) {
		new_addr = addr - TOP_RF_SPI_AON_CONSYS_BASE;
		drv_spi_write(si, new_addr, data);
	} else if (addr >= MCU_CFG_CONSYS_BASE &&
		   addr <= MCU_CFG_CONSYS_BASE + MCU_CFG_SIZE) {
		new_addr = addr - MCU_CFG_CONSYS_BASE;
		drv_mcu_write(si, new_addr, data);
	} else if (addr >= TOP_MISC_OFF_CONSYS_BASE &&
		   addr <= TOP_MISC_OFF_CONSYS_BASE + TOP_MISC_OFF_SIZE) {
		new_addr = addr - TOP_MISC_OFF_CONSYS_BASE;
		drv_top_write(si, new_addr, data);
	} else {
		WCN_DBG(FM_WAR | CHIP, "not support addr[0x%08x].\n", addr);
		return;
	}

	WCN_DBG(FM_DBG | CHIP, "write [0x%08x/0x%08x]=[0x%08x]\n",
		new_addr, addr, data);
}

/**
 * Send TX data via STP format
 *
 * @param None
 *
 * @return None
 */
static void fm_tx(unsigned char *buf, unsigned short length)
{
	if (FM_LOCK(fm_wcn_ops.tx_lock))
		return;

	if (length == 0xFFFF) {
		length = (unsigned short)buf[2] +
			(unsigned short)(buf[3] << 8) + FM_HDR_SIZE;
	}

	memset(fm_wcn_ops.rx_buf, 0, RX_BUF_SIZE);
	memcpy(fm_wcn_ops.rx_buf, buf, length);
	fm_wcn_ops.rx_len = length;
	fm_event_parser(fm_rds_parser);

	FM_UNLOCK(fm_wcn_ops.tx_lock);
}

static void fm_stc_done_rechandler(void)
{
	unsigned short i, reg_value, freq, isr_value;

	fw_spi_read(FM_MAIN_CTRL, &isr_value);

	WCN_DBG(FM_NTC | CHIP, "isr_value[%04x]\n", isr_value);

	if (isr_value & FM_MAIN_CTRL_SEEK) {
		unsigned char data[6];

		fw_spi_read(FM_MAIN_CHANDETSTAT, &reg_value);

		/* freq's unit is 10kHz */
		freq = ((((reg_value & 0x3ff0) >> 4) >> 1) + 640) * 10;

		data[0] = FM_TASK_EVENT_PKT_TYPE;
		data[1] = FM_SEEK_OPCODE;
		/* payload length */
		data[2] = 2;
		data[3] = 0x00;
		data[4] = (unsigned char)freq & 0xFF;
		data[5] = (unsigned char)(freq >> 8);

		fm_tx(data, 0xFFFF);
	} else if (isr_value & FM_MAIN_CTRL_SCAN) {
		unsigned char data[36];

		for (i = 0; i <= SCAN_BUFF_LEN; i++) {
			fw_spi_read(RDS_DATA_REG, &reg_value);
			data[4 + (i * 2)] = reg_value & 0xff;
			data[4 + (i * 2) + 1] = (reg_value & 0xff00) >> 8;
		}

		data[0] = FM_TASK_EVENT_PKT_TYPE;
		data[1] = FM_SCAN_OPCODE;
		/* payload length */
		data[2] = 32;
		data[3] = 0x00;

		fm_tx(data, 0xFFFF);
	} else if (isr_value & FM_MAIN_CTRL_TUNE) {
		unsigned char data[5];

		data[0] = FM_TASK_EVENT_PKT_TYPE;
		data[1] = FM_TUNE_OPCODE;
		/* payload length */
		data[2] = 0x01;
		data[3] = 0x00;
		data[4] = 0x01;
		fm_tx(data, 0x0005);
	}
}

static void fm_rds_rechandler(void)
{
	unsigned short fifo_cnt, reg_value, rds_data[6], reg_sin, reg_cos;
	unsigned short crc = 0, corr_cnt = 0, rds_info;
	unsigned short output_point, temp_data;
	unsigned short i = 0;
	unsigned char data[128];

	do {
		fw_spi_read(RDS_FIFO_STATUS0, &fifo_cnt);
		fifo_cnt = (fifo_cnt & RDS_DCO_FIFO_OFST) >>
			RDS_DCO_FIFO_OFST_SHFT;

		/* block A data and info handling */
		fw_spi_read(RDS_INFO_REG, &rds_info);
		crc |= (rds_info & RDS_CRC_INFO) << 3;
		corr_cnt |= ((rds_info & RDS_CRC_CORR_CNT) << 11);
		fw_spi_read(RDS_DATA_REG, &(rds_data[0]));

		/* block B data and info handling */
		fw_spi_read(RDS_INFO_REG, &rds_info);
		crc |= (rds_info & RDS_CRC_INFO) << 2;
		corr_cnt |= ((rds_info & RDS_CRC_CORR_CNT) << 7);
		fw_spi_read(RDS_DATA_REG, &(rds_data[1]));

		/* block C data and info handling */
		fw_spi_read(RDS_INFO_REG, &rds_info);
		crc |= (rds_info & RDS_CRC_INFO) << 1;
		corr_cnt |= ((rds_info & RDS_CRC_CORR_CNT) << 3);
		fw_spi_read(RDS_DATA_REG, &(rds_data[2]));

		/* block D data and info handling */
		fw_spi_read(RDS_INFO_REG, &rds_info);
		crc |= (rds_info & RDS_CRC_INFO);
		corr_cnt |= ((rds_info & RDS_CRC_CORR_CNT) >> 1);
		fw_spi_read(RDS_DATA_REG, &(rds_data[3]));


		rds_data[4] = corr_cnt;
		rds_data[5] = crc;

		/* -1 due to H/W behavior */
		if (fifo_cnt > 0)
			fifo_cnt--;

		/* RDS recovery start */
		/* check if reading doesn't start at block A */
		fw_spi_read(RDS_POINTER, &output_point);
		while (output_point & 0x3) {
			fw_spi_read(RDS_DATA_REG, &temp_data);
			fw_spi_read(RDS_POINTER, &output_point);
		}
		/* RDS recovery end */

		fm_set_u16_to_auc(&(data[8 + 12 * i]), rds_data[0]);
		fm_set_u16_to_auc(&(data[10 + 12 * i]), rds_data[1]);
		fm_set_u16_to_auc(&(data[12 + 12 * i]), rds_data[2]);
		fm_set_u16_to_auc(&(data[14 + 12 * i]), rds_data[3]);
		fm_set_u16_to_auc(&(data[16 + 12 * i]), rds_data[4]);
		fm_set_u16_to_auc(&(data[18 + 12 * i]), rds_data[5]);

		i++;
	} while ((i < 10) && ((fifo_cnt & 0x1F) > 0));

	if (i > 0) {
		fw_spi_read(RDS_SIN_REG, &reg_sin);
		fw_spi_read(RDS_COS_REG, &reg_cos);

		data[0] = FM_TASK_EVENT_PKT_TYPE;
		data[1] = RDS_RX_DATA_OPCODE;
		/* payload length */
		data[2] = (unsigned char)(12 * i + 4);
		data[3] = 0x00;
		fm_set_u16_to_auc(&(data[4]), reg_sin);
		fm_set_u16_to_auc(&(data[6]), reg_cos);
		fm_tx(data, 0xFFFF);
	}

	fw_spi_read(FM_MAIN_EXTINTRMASK, &reg_value);
	reg_value |= (FM_INTR_RDS << 8);
	fw_spi_write(FM_MAIN_EXTINTRMASK, reg_value);
}

static void fm_fifo_rechandler(unsigned short intr)
{
	unsigned short reg_value;

	/* Handle channel info. data in FIFO if RDS interrupt
	 * and CQI interrupt arise simultaneously
	 */
	if ((intr & 0x0001) && (intr & 0x0020)) {
		unsigned short i;
		unsigned char data[100];

		for (i = 0; i < FIFO_LEN; i++) {
			fw_spi_read(RDS_DATA_REG, &reg_value);
			data[4 + (i * 2)] = reg_value & 0xff;
			data[4 + (i * 2) + 1] = (reg_value & 0xff00) >> 8;
		}

		data[0] = FM_TASK_EVENT_PKT_TYPE;
		data[1] = FM_SCAN_OPCODE;
		/* payload length */
		data[2] = (unsigned char)(FIFO_LEN<<1);
		data[3] = 0x00;

		fm_tx(data, 0xFFFF);
	}
	/* Handle RDS data in FIFO, while only RDS interrupt issues */
	else if (intr & 0x0020) {

		fw_spi_read(FM_MAIN_EXTINTRMASK, &reg_value);
		reg_value &= ~(FM_INTR_RDS << 8);
		fw_spi_write(FM_MAIN_EXTINTRMASK, reg_value);

		fm_rds_rechandler();
	}

}

static void fm_softmute_tune(unsigned short freq, unsigned char *pos)
{
	unsigned short rdata;
	unsigned int i = 0, PRX = 0, ATEDV = 0, PR = 0;
	int RSSI = 0, PAMD = 0, FPAMD = 0, MR = 0, ATDC = 0;
	struct fm_full_cqi *p_cqi = (struct fm_full_cqi *)pos;

	p_cqi->ch = freq;

	/* soft mute tune */
	fw_bop_modify(FM_MAIN_CG2_CTRL, 0xBFFF, 0x4000);
	/* disable interrupt */
	fw_spi_write(FM_MAIN_INTRMASK, 0x0000);
	fw_spi_write(FM_MAIN_EXTINTRMASK, 0x0000);
	/* ramp down */
	fw_bop_modify(FM_MAIN_CTRL, 0xFFF0, 0x0000);
	/* Set DSP ramp down state */
	fw_bop_modify(FM_MAIN_CTRL, 0xFFFF, 0x0100);

	fw_bop_rd_until(FM_MAIN_INTR, 0x0001, 0x0001);

	fw_bop_modify(FM_MAIN_CTRL, 0xFEFF, 0x0000);
	fw_bop_modify(FM_MAIN_INTR, 0xFFFF, 0x0001);

	/* tune */
	freq = freq / 5 - 1280;
	fw_bop_modify(0x65, 0xFC00, freq);
	fw_bop_modify(FM_MAIN_CTRL, 0xFFFF, 0x0001);
	fw_bop_rd_until(FM_MAIN_INTR, 0x0001, 0x0001);
	fw_bop_modify(FM_MAIN_INTR, 0xFFFF, 0x0001);

	/* get CQI */
	fw_bop_udelay(9000);
	for (i = 0; i < 8; i++) {
		/* RSSI */
		fw_spi_read(0x6C, &rdata);
		RSSI += ((rdata & 0x3FF) >= 512) ?
			((rdata & 0x3FF) - 1024) : (rdata & 0x3FF);

		/* PAMD */
		fw_spi_read(0xB4, &rdata);
		PAMD += ((rdata & 0x1FF) >= 256) ?
			((rdata & 0x1FF) - 512) : (rdata & 0x1FF);

		/* PR */
		fw_spi_read(0xB5, &rdata);
		PR += (rdata & 0x3FF);

		/* FPAMD */
		fw_spi_read(0xBC, &rdata);
		FPAMD += ((rdata & 0xFFF) >= 2048) ?
			((rdata & 0xFFF) - 4096) : (rdata & 0xFFF);

		/* MR */
		fw_spi_read(0xBD, &rdata);
		MR += ((rdata & 0x1FF) >= 256) ?
			((rdata & 0x1FF) - 512) : (rdata & 0x1FF);

		/* ATDC */
		fw_spi_read(0x83, &rdata);
		ATDC += (rdata >= 32768) ? (65536 - rdata) : rdata;

		/* PRX */
		fw_spi_read(0x84, &rdata);
		PRX += rdata & 0xFF;

		/* ATDEV */
		fw_spi_read(0x85, &rdata);
		ATEDV += rdata;

		fw_bop_udelay(2250);
	}

	RSSI = (RSSI + 4) / 8;
	p_cqi->rssi = RSSI & 0x03FF;
	PAMD = (PAMD + 4) / 8;
	p_cqi->pamd = PAMD & 0x01FF;
	PR  = (PR  + 4) / 8;
	p_cqi->pr = PR & 0x03FF;
	FPAMD = (FPAMD + 4) / 8;
	p_cqi->fpamd = FPAMD & 0x0FFF;
	MR = (MR + 4) / 8;
	p_cqi->mr = MR & 0x01FF;
	ATDC = (ATDC + 4) / 8;
	p_cqi->atdc = ATDC & 0xFFFF;
	PRX = (PRX + 4) / 8;
	p_cqi->prx = PRX & 0x00FF;
	ATEDV = (ATEDV + 4) / 8;
	p_cqi->atdev = ATEDV & 0xFFFF;

	/* Soft_mute Gain */
	fw_spi_read(0x86, &rdata);
	p_cqi->smg = rdata;

	/* delta RSSI */
	fw_spi_read(0x88, &rdata);
	p_cqi->drssi = rdata;

	/* clear soft mute tune */
	fw_bop_modify(FM_MAIN_CG2_CTRL, 0xBFFF, 0x0000);

	WCN_DBG(FM_DBG | CHIP,
		"freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
		p_cqi->ch, p_cqi->rssi, p_cqi->pamd,
		p_cqi->pr, p_cqi->fpamd, p_cqi->mr,
		p_cqi->atdc, p_cqi->prx, p_cqi->atdev,
		p_cqi->smg, p_cqi->drssi);
}

static void fm_dsp_download(
	unsigned char target, unsigned short length,
	unsigned char total_seg, unsigned char current_seg,
	unsigned char *data)
{
	unsigned int control_code = 0;
	unsigned short i;

	switch (target) {
	case DSP_ROM:
	case DSP_PATCH:
		control_code = 0x10;
		break;
	case DSP_COEFF:
		control_code = 0xe;
		break;
	case DSP_HWCOEFF:
		control_code = 0xd;
		break;
	default:
		break;
	}

	if (current_seg == 0) {
		fw_spi_write(CONTROL_REG, 0);
		/* Start address */
		fw_spi_write(OFFSET_REG, data[1] << 8 | data[0]);
		/* Reset download control */
		fw_spi_write(CONTROL_REG, 0x40);
		/* Set download control */
		fw_spi_write(CONTROL_REG, control_code);
		data += 4;
		length -= 4;
	}

	if (length > 0) {
		for (i = 0; i < (length >> 1); i++)
			fw_spi_write(DATA_REG, data[2 * i + 1] << 8 | data[2 * i]);
	} else {
		WCN_DBG(FM_ERR | CHIP, "incorrect length[%d].\n", length);
	}
}

static void fm_task_rx_basic_op(
	unsigned char bop, unsigned char length, unsigned char *buf)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;

	switch (bop) {
	case FM_WRITE_BASIC_OP:
		fw_spi_write(buf[0], fm_get_u16_from_auc(&buf[1]));
		break;

	case FM_UDELAY_BASIC_OP:
		fw_bop_udelay(fm_get_u32_from_auc(&buf[0]));
		break;

	case FM_RD_UNTIL_BASIC_OP:
		fw_bop_rd_until(buf[0], fm_get_u16_from_auc(&buf[1]),
				fm_get_u16_from_auc(&buf[3]));
		break;

	case FM_MODIFY_BASIC_OP:
		fw_bop_modify(buf[0], fm_get_u16_from_auc(&buf[1]),
			      fm_get_u16_from_auc(&buf[3]));
		break;

	case FM_MSLEEP_BASIC_OP:
		fm_delayms(fm_get_u32_from_auc(&buf[0]));
		break;

	case FM_WRITE_SPI_BASIC_OP:
		si->sys_spi_write(si, buf[0], fm_get_u16_from_auc(&buf[1]),
				  fm_get_u32_from_auc(&buf[3]));
		break;
	case FM_RD_SPI_UNTIL_BASIC_OP:
		fw_bop_spi_rd_until(buf[0], fm_get_u16_from_auc(&buf[1]),
				    fm_get_u32_from_auc(&buf[3]),
				    fm_get_u32_from_auc(&buf[7]));
		break;
	case FM_MODIFY_SPI_BASIC_OP:
		fw_bop_spi_modify(buf[0], fm_get_u16_from_auc(&buf[1]),
				  fm_get_u32_from_auc(&buf[3]),
				  fm_get_u32_from_auc(&buf[7]));
		break;

	default:
		break;
	}
}

/**
 * FM task rx dispatcher default functions for basic operation processing
 *
 * @param opcode	opcode for different packet type
 * @param length	the length of parameters
 * @param buf	   the parameters for different packet type
 *
 * @return None
 */
static void fm_task_rx_dispatcher_default(
	unsigned char opcode, unsigned short length, unsigned char *buf)
{
	int unused_op_size = length;
	unsigned short used_op_size = 0;

	while (unused_op_size > 0) {
		unsigned char basic_op = buf[used_op_size];
		unsigned char basic_op_length = buf[used_op_size + 1];
		unsigned char *basic_op_buf = &buf[used_op_size + 2];

		fm_task_rx_basic_op(basic_op, basic_op_length,
				    basic_op_buf);
		unused_op_size -= (int)(basic_op_length + 2);
		used_op_size += (unsigned short)(basic_op_length + 2);
	}
}

/**
 * FM task rx dispatcher functions for different packet type
 *
 * @param opcode	opcode for different packet type
 * @param length	the length of parameters
 * @param buf	   the parameters for different packet type
 *
 * @return None
 */
static void fm_task_rx_dispatcher(
	unsigned char opcode, unsigned short length, unsigned char *buf)
{
	unsigned char event[28];

	/* Prepare FM event packet, default no payload */
	event[0] = FM_TASK_EVENT_PKT_TYPE;
	event[1] = opcode;
	event[2] = 0x0;
	event[3] = 0x0;

	switch (opcode) {
	case FM_STP_TEST_OPCODE:
		event[2] = 0x3;
		event[3] = 0x0;
		event[4] = 0x0;
		event[5] = 0x1;
		event[6] = 0x2;
		break;
	case FSPI_ENABLE_OPCODE:
		break;
	case FSPI_MUX_SEL_OPCODE:
		break;
	case FSPI_READ_OPCODE:
	{
		unsigned short data;

		fw_spi_read(buf[0], &data);
		event[2] = 0x2;
		event[3] = 0x0;
		fm_set_u16_to_auc(&event[4], data);
		break;
	}
	case FSPI_WRITE_OPCODE:
		fw_spi_write(buf[0], fm_get_u16_from_auc(&buf[1]));
		break;

	case FM_PATCH_DOWNLOAD_OPCODE:
		fm_dsp_download(DSP_PATCH, length - 2, buf[0], buf[1], &buf[2]);
		break;

	case FM_COEFF_DOWNLOAD_OPCODE:
		fm_dsp_download(DSP_COEFF, length - 2, buf[0], buf[1], &buf[2]);
		break;

	case FM_HWCOEFF_DOWNLOAD_OPCODE:
		fm_dsp_download(DSP_HWCOEFF, length - 2, buf[0], buf[1], &buf[2]);
		break;

	case FM_ROM_DOWNLOAD_OPCODE:
		fm_dsp_download(DSP_ROM, length - 2, buf[0], buf[1], &buf[2]);
		break;
	case FM_SOFT_MUTE_TUNE_OPCODE:
	{
		switch (buf[0]) {
		case 1:
		{
			unsigned short freq = 0;

			fm_set_u16_to_auc(&event[2],
					  (FM_SOFTMUTE_TUNE_CQI_SIZE + 2));
			event[4] = FM_SOFTMUTE_TUNE_CQI_SIZE;
			event[5] = 0x01;
			freq = fm_get_u16_from_auc(&buf[1]);
			fm_softmute_tune(freq, &event[6]);
			break;
		}
		case 2:
		case 3:
		case 4:
		default:
		{
			event[2] = 0x00;
			event[3] = 0x00;
			memcpy(&event[2], buf, 3);
			break;
		}
		}
		break;
	}
	case FM_HOST_READ_OPCODE:
	{
		struct fm_spi_interface *si = &fm_wcn_ops.si;
		unsigned int addr, data;

		addr = fm_get_u32_from_auc(&buf[0]);
		si->host_read(si, addr, &data);
		event[2] = 0x4;
		event[3] = 0x0;
		fm_set_u32_to_auc(&event[4], data);
	}
	break;
	case FM_HOST_WRITE_OPCODE:
	{
		struct fm_spi_interface *si = &fm_wcn_ops.si;
		unsigned int addr, data;

		addr = fm_get_u32_from_auc(&buf[0]);
		data = fm_get_u32_from_auc(&buf[4]);
		si->host_write(si, addr, data);

		break;
	}
	case CSPI_WRITE_OPCODE:
	{
		struct fm_spi_interface *si = &fm_wcn_ops.si;

		si->sys_spi_write(si, buf[0], fm_get_u16_from_auc(&buf[1]),
				  fm_get_u32_from_auc(&buf[3]));
		break;
	}
	case CSPI_READ_OPCODE:
	{
		struct fm_spi_interface *si = &fm_wcn_ops.si;
		unsigned int data;
		unsigned short ret;

		ret = si->sys_spi_read(
			si, buf[0], fm_get_u16_from_auc(&buf[1]), &data);

		if (ret != 0)
			data = (unsigned short)ret;

		event[2] = 0x4;
		event[3] = 0x0;
		fm_set_u32_to_auc(&event[4], data);
		break;
	}
	case FM_HOST_MODIFY_OPCODE:
	{
		struct fm_spi_interface *si = &fm_wcn_ops.si;
		unsigned int addr, data;

		addr = fm_get_u32_from_auc(&buf[0]);
		si->host_read(si, addr, &data);
		data &= fm_get_u32_from_auc(&buf[4]);
		data |= fm_get_u32_from_auc(&buf[8]);
		si->host_write(si, addr, data);
		break;
	}
	default:
		fm_task_rx_dispatcher_default(opcode, length, buf);
		break;
	}
	fm_tx(event, 0xFFFF);
}

static bool drv_set_own(void)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;
	unsigned int val, tmp, i;
	int ret = 0;

	ret = FM_LOCK(fm_wcn_ops.own_lock);
	for (i = 0; ret && i < MAX_SET_OWN_COUNT; i++) {
		fm_delayms(2);
		ret = FM_LOCK(fm_wcn_ops.own_lock);
	}

	/* get lock fail */
	if (i == MAX_SET_OWN_COUNT) {
		WCN_DBG(FM_ERR | CHIP, "get own lock fail[%d]\n", ret);
		return false;
	}

	/* wakeup conninfra */
	drv_host_write(si, 0x180601B0, 0x1);

	/* polling chipid */
	drv_host_read(si, 0x18001000, &val);
	for (i = 0; (val & 0xFFFF0000) != 0x20010000 && i < MAX_SET_OWN_COUNT; i++) {
		fm_delayus(5000);
		drv_host_read(si, 0x18001000, &val);
	}

	/* polling fail */
	if (i == MAX_SET_OWN_COUNT) {
		/* unlock if set own fail */
		drv_host_read(si, 0x180601B0, &val);
		drv_host_read(si, 0x18001808, &tmp);
		WCN_DBG(FM_ERR | CHIP,
			"polling chip id fail [0x180601B0]=[0x%08x], [0x18001808]=[0x%08x]\n",
			val, tmp);
		FM_UNLOCK(fm_wcn_ops.own_lock);
		return false;
	}

	if (ei->is_bus_hang && ei->is_bus_hang()) {
		FM_UNLOCK(fm_wcn_ops.own_lock);
		return false;
	}

	/* conn_infra bus debug function setting */
	conninfra_config_setup();

	return true;
}

static bool drv_clr_own(void)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;

	drv_host_write(si, 0x180601B0, 0x0);

	FM_UNLOCK(fm_wcn_ops.own_lock);

	return true;
}

static int drv_stp_send_data(unsigned char *buf, unsigned int len)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;

	if (len < 4) {
		WCN_DBG(FM_ERR | CHIP, "buf length error[%d].\n", len);
		return -1;
	}

	if (si->set_own && !si->set_own()) {
		WCN_DBG(FM_ERR | CHIP, "set_own fail\n");
		return -1;
	}

	fm_task_rx_dispatcher(buf[1], len - 4, buf + 4);

	if (si->clr_own)
		si->clr_own();

	WCN_DBG(FM_DBG | CHIP, "buffer: %02x %02x %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

	return 1;
}

static int drv_stp_recv_data(unsigned char *buf, unsigned int len)
{
	unsigned int length = fm_wcn_ops.rx_len;

	if (length > len)
		length = len;

	memcpy(buf, fm_wcn_ops.rx_buf, length);

	return length;
}

static int drv_spi_hopping(void)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	int ret = 0, i = 0;
	unsigned int val = 0;

	if (si->set_own && !si->set_own()) {
		WCN_DBG(FM_ERR | CHIP, "set_own fail\n");
		return -1;
	}

	/* enable 'rf_spi_div_en' */
	drv_host_read(si, 0x18001A00, &val);
	drv_host_write(si, 0x18001A00, val | (0x1 << 28));

	/* lock 64M */
	drv_host_read(si, 0x18003004, &val);
	drv_host_write(si, 0x18003004, val | (0x1 << 15));

	/*rd 0x18001810 until D1 == 1*/
	for (i = 0; i < 100; i++) {
		drv_host_read(si, 0x18001810, &val);
		if (val & 0x00000002) {
			WCN_DBG(FM_NTC | CHIP,
				"%s: POLLING PLL_RDY success !\n", __func__);
			/* switch SPI clock to 64MHz */
			if (conninfra_spi_clock_switch(CONNSYS_SPI_SPEED_64M) == -1) {
				WCN_DBG(FM_ERR | CHIP,
					"conninfra clock switch 64M fail.\n");
				ret = -1;
			}
			break;
		}
		fm_delayus(10);
	}

	if (i == 100) {
		ret = -1;
		WCN_DBG(FM_ERR | CHIP,
			"%s: Polling to read rd 0x18001810[1] ==0x1 failed !\n",
			__func__);
	}

	if (si->clr_own)
		si->clr_own();

	return ret;
}

static void drv_enable_eint(void)
{
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;

	enable_irq(ei->irq_id);
}

static void drv_disable_eint(void)
{
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;

	disable_irq_nosync(ei->irq_id);
}

static void drv_eint_handler(void)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	unsigned short main_isr;

	if (si->set_own && !si->set_own()) {
		WCN_DBG(FM_ERR | CHIP, "set_own fail\n");
		return;
	}

	fw_spi_read(FM_MAIN_INTR, &main_isr);
	main_isr &= FM_INTR_MASK;

	WCN_DBG(FM_NTC | CHIP, "interrupt[%04x]\n", main_isr);

	if (main_isr & FM_INTR_STC_DONE)
		fm_stc_done_rechandler();

	if (main_isr & FM_INTR_RDS)
		fm_fifo_rechandler(main_isr);

	fw_spi_write(FM_MAIN_INTR, main_isr);

	if (si->clr_own)
		si->clr_own();
}

static int drv_interface_init(void)
{
	struct fm_spi_interface *interface = &fm_wcn_ops.si;
	struct fm_wcn_reg_info *info = &interface->info;

	info->spi_phy_addr = TOP_RF_SPI_AON_ADDR;
	info->spi_size = TOP_RF_SPI_AON_SIZE;
	request_mem_region(info->spi_phy_addr, info->spi_size, "FM_SPI");
	info->spi_vir_addr = ioremap_nocache(
		info->spi_phy_addr, info->spi_size);
	if (info->spi_vir_addr == NULL) {
		WCN_DBG(FM_ERR | CHIP, "Cannot remap address.\n");
		return -1;
	}

	info->top_phy_addr = TOP_MISC_OFF_ADDR;
	info->top_size = TOP_MISC_OFF_SIZE;
	request_mem_region(info->top_phy_addr, info->top_size, "FM_TOP");
	info->top_vir_addr = ioremap_nocache(
		info->top_phy_addr, info->top_size);
	if (info->top_vir_addr == NULL) {
		WCN_DBG(FM_ERR | CHIP, "Cannot remap address.\n");
		return -1;
	}

	info->mcu_phy_addr = MCU_CFG_ADDR;
	info->mcu_size = MCU_CFG_SIZE;
	request_mem_region(info->mcu_phy_addr, info->mcu_size, "FM_MCU");
	info->mcu_vir_addr = ioremap_nocache(
		info->mcu_phy_addr, info->mcu_size);
	if (info->mcu_vir_addr == NULL) {
		WCN_DBG(FM_ERR | CHIP, "Cannot remap address.\n");
		return -1;
	}

	interface->spi_read = drv_spi_read;
	interface->spi_write = drv_spi_write;
	interface->host_read = drv_host_read;
	interface->host_write = drv_host_write;
#if CFG_FM_CONNAC2
	interface->sys_spi_read = drv_sys_spi_read;
	interface->sys_spi_write = drv_sys_spi_write;
	interface->set_own = drv_set_own;
	interface->clr_own = drv_clr_own;
#else
	interface->sys_spi_read = fm_sys_spi_read;
	interface->sys_spi_write = fm_sys_spi_write;
	interface->set_own = NULL;
	interface->clr_own = NULL;
#endif

	return 0;
}

static int drv_interface_uninit(void)
{
	struct fm_spi_interface *interface = &fm_wcn_ops.si;
	struct fm_wcn_reg_info *info = &interface->info;

	if (info->spi_vir_addr) {
		iounmap(info->spi_vir_addr);
		release_mem_region(info->spi_phy_addr, info->spi_size);
	}

	if (info->top_vir_addr) {
		iounmap(info->top_vir_addr);
		release_mem_region(info->top_phy_addr, info->top_size);
	}

	if (info->mcu_vir_addr) {
		iounmap(info->mcu_vir_addr);
		release_mem_region(info->mcu_phy_addr, info->mcu_size);
	}
	return 0;
}

static int drv_get_hw_version(void)
{
	return FM_CONNAC_2_1;
}

static unsigned char drv_get_top_index(void)
{
	return SYS_SPI_TOP;
}

static unsigned int drv_get_get_adie(void)
{
	return 0x6635;
}

#if CFG_FM_CONNAC2

static int fm_conninfra_stp_register_event_cb(void *cb)
{
	fm_wcn_ops.ei.eint_cb = (void (*)(void))cb;
	return 0;
}

static int fm_conninfra_msgcb_reg(void *data)
{
	/* get whole chip reset cb */
	whole_chip_reset = data;
	return conninfra_sub_drv_ops_register(
		CONNDRV_TYPE_FM, &fm_drv_cbs);
}

static int fm_conninfra_func_on(void)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	int ret = 0;

	ret = conninfra_pwr_on(CONNDRV_TYPE_FM);
	if (ret == -1) {
		WCN_DBG(FM_ERR | CHIP, "conninfra power on fail.\n");
		return 0;
	}

	if (si->set_own && !si->set_own()) {
		WCN_DBG(FM_ERR | CHIP, "set_own fail\n");
		return 0;
	}

	ret = conninfra_adie_top_ck_en_on(CONNSYS_ADIE_CTL_HOST_FM);

	if (si->clr_own)
		si->clr_own();

	if (ret == -1) {
		WCN_DBG(FM_ERR | CHIP, "top ck en fail.\n");
		return 0;
	}

	return 1;
}

static int fm_conninfra_func_off(void)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	int ret = 0;

	if (si->set_own && !si->set_own()) {
		WCN_DBG(FM_ERR | CHIP, "set_own fail\n");
		return 0;
	}

	ret  = conninfra_adie_top_ck_en_off(CONNSYS_ADIE_CTL_HOST_FM);

	if (si->clr_own)
		si->clr_own();

	if (ret == -1) {
		WCN_DBG(FM_ERR | CHIP, "top ck en off fail.\n");
		return 0;
	}

	ret = conninfra_pwr_off(CONNDRV_TYPE_FM);
	if (ret == -1) {
		WCN_DBG(FM_ERR | CHIP, "conninfra power off fail.\n");
		return 0;
	}

	return 1;
}

static int fm_conninfra_chipid_query(void)
{
#ifdef CFG_FM_CHIP_ID
	return CFG_FM_CHIP_ID;
#else
	return 0;
#endif
}

static int fm_conninfra_spi_clock_switch(enum fm_spi_speed speed)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	enum connsys_spi_speed_type sp_type = CONNSYS_SPI_SPEED_26M;
	int ret = 0;

	if (si->set_own && !si->set_own()) {
		WCN_DBG(FM_ERR | CHIP, "set_own fail\n");
		return -2;
	}

	switch (speed) {
	case FM_SPI_SPEED_26M:
		sp_type = CONNSYS_SPI_SPEED_26M;
		break;
	case FM_SPI_SPEED_64M:
		sp_type = CONNSYS_SPI_SPEED_64M;
		break;
	default:
		break;
	}

	ret = conninfra_spi_clock_switch(sp_type);

	if (si->clr_own)
		si->clr_own();

	if (ret == -1) {
		WCN_DBG(FM_ERR | CHIP, "conninfra clock switch fail.\n");
		return -1;
	}

	return 0;
}

static bool fm_conninfra_is_bus_hang(void)
{
	int ret = 0;

	if (conninfra_reg_readable())
		return false;

	/* Check conninfra bus before accessing BGF's CR */
	ret = conninfra_is_bus_hang();
	if (ret > 0) {
		WCN_DBG(FM_ERR | CHIP, "conninfra bus is hang[0x%x]\n", ret);
		conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_FM, "bus hang");
		return true;
	}

	WCN_DBG(FM_ERR | CHIP,
		"conninfra not readable, but not bus hang ret = %d", ret);

	return false;
}

#else /* CFG_FM_CONNAC2 */


static int fm_stp_register_event_cb(void *cb)
{
	fm_wcn_ops.ei.eint_cb = (void (*)(void))cb;
	return mtk_wcn_stp_register_event_cb(FM_TASK_INDX, cb);
}

static int fm_wmt_msgcb_reg(void *data)
{
	/* get whole chip reset cb */
	return 0;
}

static int fm_wmt_func_on(void)
{
	return mtk_wcn_wmt_func_on(WMTDRV_TYPE_FM) != MTK_WCN_BOOL_FALSE;
}

static int fm_wmt_func_off(void)
{
	return mtk_wcn_wmt_func_off(WMTDRV_TYPE_FM) != MTK_WCN_BOOL_FALSE;
}

static unsigned int fm_wmt_ic_info_get(void)
{
	return mtk_wcn_wmt_ic_info_get(1);
}

static int fm_wmt_chipid_query(void)
{
	return mtk_wcn_wmt_chipid_query();
}

static int fm_wmt_spi_clock_switch(enum fm_spi_speed speed)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	unsigned int reg_val = 0;

	switch (speed) {
	case FM_SPI_SPEED_26M:
		si->host_read(si, 0x18004004, &reg_val);
		reg_val &= 0xFFFFFFFE;
		si->host_write(si, 0x18004004, reg_val);
		break;
	case FM_SPI_SPEED_64M:
		si->host_read(si, 0x18004004, &reg_val);
		reg_val |= 0x00000001;
		si->host_write(si, 0x18004004, reg_val);
		break;
	default:
		break;
	}

	return 0;
}
#endif /* CFG_FM_CONNAC2 */

static irqreturn_t fm_isr(int irq, void *dev)
{
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;

	if (!ei->eint_cb) {
		WCN_DBG(FM_WAR | CHIP, "fm eint cb is NULL\n");
		return IRQ_NONE;
	}

	if (ei->disable_eint)
		ei->disable_eint();

	ei->eint_cb();

	return IRQ_HANDLED;
}

int fm_register_irq(struct platform_driver *drv)
{
#ifdef CONFIG_OF
	struct device_node *node = NULL;
#endif
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;
	int ret = 0;

	ei->drv = drv;
	ei->irq_id = FM_IRQ_NUMBER;
#ifdef CONFIG_OF
	node = of_find_compatible_node(NULL, NULL, "mediatek,fm");
	if (node)
		ei->irq_id = irq_of_parse_and_map(node, 0);
	else
		WCN_DBG(FM_ERR | CHIP, "get fm dts node fail\n");
#endif
	WCN_DBG(FM_NTC | CHIP, "request_irq num(%d)\n", ei->irq_id);
	ret = request_irq(ei->irq_id, fm_isr, IRQF_SHARED, FM_NAME, drv);
	if (ret != 0)
		WCN_DBG(FM_ERR | CHIP, "request_irq  ERROR(%d)\n", ret);

	return ret;
}

static void register_drv_ops_init(void)
{
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;

	drv_interface_init();
	ei->eint_handler = drv_eint_handler;
	ei->stp_send_data = drv_stp_send_data;
	ei->stp_recv_data = drv_stp_recv_data;
	ei->get_hw_version = drv_get_hw_version;
	ei->get_top_index = drv_get_top_index;
	ei->get_get_adie = drv_get_get_adie;

#if CFG_FM_CONNAC2
	ei->enable_eint = drv_enable_eint;
	ei->disable_eint = drv_disable_eint;
	ei->stp_register_event_cb = fm_conninfra_stp_register_event_cb;
	ei->wmt_msgcb_reg = fm_conninfra_msgcb_reg;
	ei->wmt_func_on = fm_conninfra_func_on;
	ei->wmt_func_off = fm_conninfra_func_off;
	ei->wmt_ic_info_get = NULL;
	ei->wmt_chipid_query = fm_conninfra_chipid_query;
	ei->spi_clock_switch = fm_conninfra_spi_clock_switch;
	ei->is_bus_hang = fm_conninfra_is_bus_hang;
	ei->spi_hopping = drv_spi_hopping;
#else
	ei->enable_eint = NULL;
	ei->disable_eint = NULL;
	ei->stp_register_event_cb = fm_stp_register_event_cb;
	ei->wmt_msgcb_reg = fm_wmt_msgcb_reg;
	ei->wmt_func_on = fm_wmt_func_on;
	ei->wmt_func_off = fm_wmt_func_off;
	ei->wmt_ic_info_get = fm_wmt_ic_info_get;
	ei->wmt_chipid_query = fm_wmt_chipid_query;
	ei->spi_clock_switch = fm_wmt_spi_clock_switch;
	ei->is_bus_hang = NULL;
	ei->spi_hopping = NULL;
#endif
	ei->low_ops_register = mt6635_fm_low_ops_register;
	ei->rds_ops_unregister = mt6635_fm_rds_ops_unregister;
	ei->rds_ops_register = mt6635_fm_rds_ops_register;
	ei->rds_ops_unregister = mt6635_fm_rds_ops_unregister;
}

static void register_drv_ops_uninit(void)
{
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;

	if (ei->irq_id)
		free_irq(ei->irq_id, ei->drv);
	drv_interface_uninit();
	fm_memset(&fm_wcn_ops, 0, sizeof(struct fm_wcn_reg_ops));
}

int fm_wcn_ops_register(void)
{
	register_drv_ops_init();
	return 0;
}

int fm_wcn_ops_unregister(void)
{
	register_drv_ops_uninit();
	return 0;
}
