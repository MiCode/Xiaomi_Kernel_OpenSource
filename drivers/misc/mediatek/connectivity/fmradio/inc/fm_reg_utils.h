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

#ifndef FM_REG_UTILS_H
#define FM_REG_UTILS_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/vmalloc.h>

#include <fm_ext_api.h>

/* SPI register address */
#if CFG_FM_CONNAC2
#define SYS_SPI_BASE_ADDR         (0x0000)
#else
#define SYS_SPI_BASE_ADDR         (0x6000)
#endif
#define SYS_SPI_STA               (0x00 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_CRTL              (0x04 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_DIV               (0x08 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_FM_CTRL           (0x0C + SYS_SPI_BASE_ADDR)

/* control bit for SPI_STA */
#define SYS_SPI_ADDR_CR_MASK       (0x0FFF)
#define SYS_SPI_ADDR_CR_READ       (0x01 << 12)
#define SYS_SPI_ADDR_CR_WRITE      (0x00 << 12)

#define SYS_SPI_ADDR_CR_WF1        (0x00 << 13)
#define SYS_SPI_ADDR_CR_WF         (0x01 << 13)
#define SYS_SPI_ADDR_CR_BT         (0x02 << 13)
#define SYS_SPI_ADDR_CR_FM         (0x03 << 13)
#define SYS_SPI_ADDR_CR_GPS        (0x04 << 13)
#define SYS_SPI_ADDR_CR_TOP        (0x05 << 13)

#define SYS_SPI_STA_WF_BUSY_ADDR    SYS_SPI_STA
#define SYS_SPI_STA_WF_BUSY_MASK    0x0002
#define SYS_SPI_STA_WF_BUSY_SHFT    1

#define SYS_SPI_STA_BT_BUSY_ADDR    SYS_SPI_STA
#define SYS_SPI_STA_BT_BUSY_MASK    0x0004
#define SYS_SPI_STA_BT_BUSY_SHFT    2

#define SYS_SPI_STA_FM_BUSY_ADDR    SYS_SPI_STA
#define SYS_SPI_STA_FM_BUSY_MASK    0x0008
#define SYS_SPI_STA_FM_BUSY_SHFT    3

#define SYS_SPI_STA_GPS_BUSY_ADDR    SYS_SPI_STA
#define SYS_SPI_STA_GPS_BUSY_MASK    0x0010
#define SYS_SPI_STA_GPS_BUSY_SHFT    4

#define SYS_SPI_STA_TOP_BUSY_ADDR    SYS_SPI_STA
#define SYS_SPI_STA_TOP_BUSY_MASK    0x0020
#define SYS_SPI_STA_TOP_BUSY_SHFT    5

/* control bit for SPI_STA */
#define SYS_SPI_CRTL_MASTER_EN_ADDR    SYS_SPI_CRTL
#define SYS_SPI_CRTL_MASTER_EN_MASK    0x8000
#define SYS_SPI_CRTL_MASTER_EN_SHFT    15

/* control bit for SPI_DIV */
#define SYS_SPI_DIV_DIV_CNT_ADDR    SYS_SPI_DIV
#define SYS_SPI_DIV_DIV_CNT_MASK    0x00FF
#define SYS_SPI_DIV_DIV_CNT_SHFT    0

/* control bit for SPI_FM_CTRL */
#define SYS_SPI_FM_CTRL_FM_RD_EXT_CNT_ADDR    SYS_SPI_FM_CTRL
#define SYS_SPI_FM_CTRL_FM_RD_EXT_CNT_MASK    0x00FF
#define SYS_SPI_FM_CTRL_FM_RD_EXT_CNT_SHFT    0

#define SYS_SPI_FM_CTRL_FM_RD_EXT_EN_ADDR    SYS_SPI_FM_CTRL
#define SYS_SPI_FM_CTRL_FM_RD_EXT_EN_MASK    0x8000
#define SYS_SPI_FM_CTRL_FM_RD_EXT_EN_SHFT    15

/* WIFI data addr & mask */
#define SYS_SPI_WF_ADDR_ADDR    (0x10 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_WF_ADDR_MASK    0xFFFF
#define SYS_SPI_WF_ADDR_SHFT    0

#define SYS_SPI_WF_WDAT_ADDR    (0x14 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_WF_WDAT_MASK    0xFFFFFFFF
#define SYS_SPI_WF_WDAT_SHFT    0

#define SYS_SPI_WF_RDAT_ADDR    (0x18 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_WF_RDAT_MASK    0xFFFFFFFF
#define SYS_SPI_WF_RDAT_SHFT    0

/* BT data addr & mask */
#define SYS_SPI_BT_ADDR_ADDR    (0x20 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_BT_ADDR_MASK    0xFFFF
#define SYS_SPI_BT_ADDR_SHFT    0

#define SYS_SPI_BT_WDAT_ADDR    (0x24 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_BT_WDAT_MASK    0xFF
#define SYS_SPI_BT_WDAT_SHFT    0

#define SYS_SPI_BT_RDAT_ADDR    (0x28 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_BT_RDAT_MASK    0xFF
#define SYS_SPI_BT_RDAT_SHFT    0

/* FM data addr & mask */
#define SYS_SPI_FM_ADDR_ADDR    (0x30 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_FM_ADDR_MASK    0xFFFF
#define SYS_SPI_FM_ADDR_SHFT    0

#define SYS_SPI_FM_WDAT_ADDR    (0x34 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_FM_WDAT_MASK    0xFFFF
#define SYS_SPI_FM_WDAT_SHFT    0

#define SYS_SPI_FM_RDAT_ADDR    (0x38 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_FM_RDAT_MASK    0xFFFF
#define SYS_SPI_FM_RDAT_SHFT    0

/* GPS data addr & mask */
#define SYS_SPI_GPS_ADDR_ADDR    (0x40 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_GPS_ADDR_MASK    0xFFFF
#define SYS_SPI_GPS_ADDR_SHFT    0

#define SYS_SPI_GPS_WDAT_ADDR    (0x44 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_GPS_WDAT_MASK    0xFFFFFFFF
#define SYS_SPI_GPS_WDAT_SHFT    0

#define SYS_SPI_GPS_RDAT_ADDR    (0x48 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_GPS_RDAT_MASK    0xFFFFFFFF
#define SYS_SPI_GPS_RDAT_SHFT    0

/* TOP data addr & mask */
#define SYS_SPI_TOP_ADDR_ADDR    (0x50 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_TOP_ADDR_MASK    0xFFFF
#define SYS_SPI_TOP_ADDR_SHFT    0

#define SYS_SPI_TOP_WDAT_ADDR    (0x54 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_TOP_WDAT_MASK    0xFFFFFFFF
#define SYS_SPI_TOP_WDAT_SHFT    0

#define SYS_SPI_TOP_RDAT_ADDR    (0x58 + SYS_SPI_BASE_ADDR)
#define SYS_SPI_TOP_RDAT_MASK    0xFFFFFFFF
#define SYS_SPI_TOP_RDAT_SHFT    0

/* A-die TOP SPI registers */
#define SYSCTL_HW_ID             0x24
#define SYSCTL_ADIE_TOP_THADC    0xC4
#define SYSCTL_CLK_STATUS        0xA00
#define SYSCTL_WF_CLK_EN         0xA04
#define SYSCTL_BT_CLK_EN         0xA08
#define SYSCTL_GPS_CLK_EN        0xA0C
#define SYSCTL_TOP_CLK_EN        0xA10

#define SYSCTL_EFUSE_SIZE        32
#define SYSCTL_Macro0_Efuse_D0   0x11C
#define SYSCTL_Macro1_Efuse_D0   0x124

/* FM basic-operation's opcode */
#define FM_BOP_BASE			 (0x80)
enum {
	FM_WRITE_BASIC_OP = (FM_BOP_BASE + 0x00),
	FM_UDELAY_BASIC_OP = (FM_BOP_BASE + 0x01),
	FM_RD_UNTIL_BASIC_OP = (FM_BOP_BASE + 0x02),
	FM_MODIFY_BASIC_OP = (FM_BOP_BASE + 0x03),
	FM_MSLEEP_BASIC_OP = (FM_BOP_BASE + 0x04),
	FM_WRITE_SPI_BASIC_OP = (FM_BOP_BASE + 0x05),
	FM_RD_SPI_UNTIL_BASIC_OP = (FM_BOP_BASE + 0x06),
	FM_MODIFY_SPI_BASIC_OP = (FM_BOP_BASE + 0x07),
	FM_MAX_BASIC_OP = (FM_BOP_BASE + 0x08)
};

/* FM SPI control registers */
#define FSPI_MAS_BASE			   (0x80060000)
#define FSPI_MAS_CONTROL_REG		(FSPI_MAS_BASE + 0x0000)
#define FSPI_MAS_ADDR_REG		   (FSPI_MAS_BASE + 0x0004)
#define FSPI_MAS_WRDATA_REG		 (FSPI_MAS_BASE + 0x0008)
#define FSPI_MAS_RDDATA_REG		 (FSPI_MAS_BASE + 0x000C)
#define FSPI_MAS_CFG1_REG		   (FSPI_MAS_BASE + 0x0010)
#define FSPI_MAS_CFG2_REG		   (FSPI_MAS_BASE + 0x0014)
#define FSPI_MAS_MODESEL_REG		(FSPI_MAS_BASE + 0x0020)
#define FSPI_MAS_RESET_REG		  (FSPI_MAS_BASE + 0x0030)
#define FSPI_MAS_DEBUG1_REG		 (FSPI_MAS_BASE + 0x0034)
#define FSPI_MAS_DEBUG2_REG		 (FSPI_MAS_BASE + 0x0038)
#define FSPI_MAS_DEBUG3_REG		 (FSPI_MAS_BASE + 0x003C)
#define FSPI_MAS_DEBUG4_REG		 (FSPI_MAS_BASE + 0x0040)
#define FSPI_MAS_DEBUG5_REG		 (FSPI_MAS_BASE + 0x0048)

/* FM Main Control Register */
#define FM_MAIN_CTRL_TUNE		 (0x0001)
#define FM_MAIN_CTRL_SEEK		 (0x0002)
#define FM_MAIN_CTRL_SCAN		 (0x0004)
#define FM_MAIN_CTRL_DSP_INIT	 (0x0008)
#define FM_MAIN_CTRL_SCAN_CQI	 (0x0008)
#define FM_MAIN_CTRL_RDS			(0x0010)
#define FM_MAIN_CTRL_DCOC		 (0x0020)
#define FM_MAIN_CTRL_MUTE		 (0x0040)
#define FM_MAIN_CTRL_IQCAL		(0x0080)
#define FM_MAIN_CTRL_RAMPDOWN	 (0x0100)
#define FM_MAIN_CTRL_MEM_FLUSH	(0x0200)
#define FM_MAIN_CTRL_MASK		 (0x0Fff)

/* RDS control registers */
#define FM_RDS_BASE			 (0x0)
#define RDS_INFO_REG			(FM_RDS_BASE + 0x81)
#define RDS_CRC_CORR_CNT			0x001E
#define RDS_CRC_INFO				0x0001
#define RDS_DATA_REG			(FM_RDS_BASE + 0x82)
#define SCAN_BUFF_LEN			   0xF
#define RDS_SIN_REG			 (FM_RDS_BASE + 0x85)
#define RDS_COS_REG			 (FM_RDS_BASE + 0x86)
#define RDS_FIFO_STATUS0		(FM_RDS_BASE + 0x87)
#define RDS_POINTER			 (FM_RDS_BASE + 0xF0)

/* RDS Interrupt Status Register */
#define RDS_INTR_THRE		   (0x0001)
#define RDS_INTR_TO			 (0x0002)
#define RDS_INTR_FULL		   (0x0004)
#define RDS_INTR_MASK		   (0x0007)
#define RDS_TX_INTR_MASK		(0x0070)
#define RDS_TX_INTR_EMPTY	   (0x0010)
#define RDS_TX_INTR_LOW		 (0x0020)
#define RDS_TX_INTR_FULL		(0x0040)
#define RDS_TX_INTR_EMPTY_MASK  (0x0001)
#define RDS_TX_INTR_LOW_MASK	(0x0002)
#define RDS_TX_INTR_FULL_MASK   (0x0004)

/* RDS Data CRC Offset Register */
#define RDS_DCO_FIFO_OFST	 (0x007C)
#define RDS_DCO_FIFO_OFST_SHFT   2

/* Parameter for RDS */
/* total 12 groups */
#define RDS_RX_FIFO_CNT			 (12)
/* delay 1us between each register read */
#define RDS_READ_DELAY			  (1)
#define RDS_GROUP_READ_DELAY		(85)
#define FIFO_LEN					(48)

/* Parameter for DSP download */
#define OFFSET_REG      0x91
#define CONTROL_REG     0x90
#define DATA_REG        0x92

#define FM_SOFTMUTE_TUNE_CQI_SIZE		0x16

#define FM_SPI_COUNT_LIMIT         100000

#define FM_BUFFER_SIZE          (2048)
#define FM_HDR_SIZE             (4)

/* FM main registers */
enum {
	FM_MAIN_MCLKDESENSE = 0x38,
	FM_MAIN_CG1_CTRL = 0x60,
	FM_MAIN_CG2_CTRL = 0x61,
	FM_MAIN_HWVER = 0x62,
	FM_MAIN_CTRL = 0x63,
	FM_MAIN_EN1 = 0x64,
	FM_CHANNEL_SET = 0x65,
	FM_MAIN_CFG1 = 0x66,
	FM_MAIN_CFG2 = 0x67,
	FM_MAIN_CFG3 = 0x68,
	FM_MAIN_INTR = 0x69,
	FM_MAIN_INTRMASK = 0x6A,
	FM_MAIN_EXTINTRMASK = 0x6B,
	FM_RSSI_IND = 0x6C,
	FM_RSSI_TH = 0x6D,
	FM_MAIN_RESET = 0x6E,
	FM_MAIN_CHANDETSTAT = 0x6F,
	FM_MAIN_IQCOMP1 = 0x70,
	FM_MAIN_IQCOMP2 = 0x71,
	FM_MAIN_IQCOMP3 = 0x72,
	FM_MAIN_IQCOMP4 = 0x73,
	FM_MAIN_RXCALSTAT1 = 0x74,
	FM_MAIN_RXCALSTAT2 = 0x75,
	FM_MAIN_RXCALSTAT3 = 0x76,
	FM_MAIN_MCLKDESENSE2 = 0x77,
	FM_MAIN_MCLKDESENSE3 = 0x78,
	FM_MAIN_CHNLSCAN_CTRL = 0x79,
	FM_MAIN_CHNLSCAN_STAT = 0x7a,
	FM_MAIN_CHNLSCAN_STAT2 = 0x7b,
	FM_RDS_CFG0 = 0x80,
	FM_RDS_INFO = 0x81,
	FM_RDS_DATA_REG = 0x82,
	FM_RDS_GOODBK_CNT = 0x83,
	FM_RDS_BADBK_CNT = 0x84,
	FM_RDS_PWDI = 0x85,
	FM_RDS_PWDQ = 0x86,
	FM_RDS_FIFO_STATUS0 = 0x87,
	FM_FT_CON9 = 0x8F,
	FM_DSP_PATCH_CTRL = 0x90,
	FM_DSP_PATCH_OFFSET = 0x91,
	FM_DSP_PATCH_DATA = 0x92,
	FM_DSP_MEM_CTRL4 = 0x93,
	FM_MAIN_PGSEL = 0x9f,
	FM_ADDR_PAMD = 0xB4,
	FM_RDS_BDGRP_ABD_CTRL_REG = 0xB6,
	FM_RDS_POINTER = 0xF0,
};

/* FM Main Interrupt Register */
enum {
	FM_INTR_STC_DONE = 0x0001,
	FM_INTR_IQCAL_DONE = 0x0002,
	FM_INTR_DESENSE_HIT = 0x0004,
	FM_INTR_CHNL_CHG = 0x0008,
	FM_INTR_SW_INTR = 0x0010,
	FM_INTR_RDS = 0x0020,
	FM_INTR_CQI = 0x0021,
	FM_INTR_MASK = 0x003f
};

enum {
	DSP_ROM = 0,
	DSP_PATCH,
	DSP_COEFF,
	DSP_HWCOEFF
};

struct fm_wcn_reg_info {
	phys_addr_t spi_phy_addr;
	void __iomem *spi_vir_addr;
	unsigned int spi_size;
	phys_addr_t top_phy_addr;
	void __iomem *top_vir_addr;
	unsigned int top_size;
	phys_addr_t mcu_phy_addr;
	void __iomem *mcu_vir_addr;
	unsigned int mcu_size;
};

struct fm_spi_interface {
	struct fm_wcn_reg_info info;
	void (*spi_read)(struct fm_spi_interface *si, unsigned int addr, unsigned int *val);
	void (*spi_write)(struct fm_spi_interface *si, unsigned int addr, unsigned int val);
	void (*host_read)(struct fm_spi_interface *si, unsigned int addr, unsigned int *val);
	void (*host_write)(struct fm_spi_interface *si, unsigned int addr, unsigned int val);
	int (*sys_spi_read)(struct fm_spi_interface *si, unsigned int subsystem,
			    unsigned int addr, unsigned int *data);
	int (*sys_spi_write)(struct fm_spi_interface *si, unsigned int subsystem,
			     unsigned int addr, unsigned int data);
	bool (*set_own)(void);
	bool (*clr_own)(void);
};

struct fm_wcn_reg_ops {
	struct fm_ext_interface ei;
	struct fm_spi_interface si;
	unsigned char rx_buf[FM_BUFFER_SIZE];
	unsigned int rx_len;
	struct fm_lock *tx_lock;
	struct fm_lock *own_lock;
};

struct fm_full_cqi {
	unsigned short ch;
	unsigned short rssi;
	unsigned short pamd;
	unsigned short pr;
	unsigned short fpamd;
	unsigned short mr;
	unsigned short atdc;
	unsigned short prx;
	unsigned short atdev;
	unsigned short smg;		/* soft-mute gain */
	unsigned short drssi;		/* delta rssi */
};

/* FM interface */
void fw_spi_read(unsigned char addr, unsigned short *data);
void fw_spi_write(unsigned char addr, unsigned short data);
void fw_bop_udelay(unsigned int usec);
void fw_bop_rd_until(unsigned char addr, unsigned short mask,
		     unsigned short value);
void fw_bop_modify(unsigned char addr, unsigned short mask_and,
		   unsigned short mask_or);
void fw_bop_spi_rd_until(unsigned char subsys, unsigned short addr,
			 unsigned int mask, unsigned int value);
void fw_bop_spi_modify(unsigned char subsys, unsigned short addr,
		       unsigned int mask_and, unsigned int mask_or);

extern struct fm_wcn_reg_ops fm_wcn_ops;
extern unsigned char *cmd_buf;
extern struct fm_lock *cmd_buf_lock;
extern struct fm_res_ctx *fm_res;

#endif /* FM_REG_UTILS_H */
