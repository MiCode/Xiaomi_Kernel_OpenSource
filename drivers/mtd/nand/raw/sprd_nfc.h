/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc NAND driver
 *
 * Copyright (C) 2023 Unisoc, Inc.
 * Author: Zhenxiong Lai <zhenxiong.lai@unisoc.com>
 */
#ifndef _SPRD_NFC_H_
#define _SPRD_NFC_H_

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mtd/nand.h>

#include "sprd_nand_param.h"
#include "sprd_nfc_dbg.h"

/* reg memory map --checked */
#define NFC_START_REG 0x00
#define NFC_CFG0_REG 0x04
#define NFC_CFG1_REG 0x08
#define NFC_CFG2_REG 0x0C
#define NFC_INT_REG 0x10
#define NFC_TIMING0_REG 0x14
#define NFC_TIMING1_REG 0x18
#define NFC_TIMING2_REG 0x1C
#define NFC_STAT_STSMCH_REG 0x30
#define NFC_TIMEOUT_REG 0x34
#define NFC_CFG3_REG 0x38
#define NFC_STATUS0_REG 0x40
#define NFC_STATUS1_REG 0x44
#define NFC_STATUS2_REG 0x48
#define NFC_STATUS3_REG 0x4C
#define NFC_STATUS4_REG 0x50
#define NFC_STATUS5_REG 0x54
#define NFC_STATUS6_REG 0x58
#define NFC_STATUS7_REG 0x5C
#define NFC_STATUS8_REG 0xA4

#define NFC_POLY0_REG 0xB0
#define NFC_POLY1_REG 0xB4
#define NFC_POLY2_REG 0xB8
#define NFC_POLY3_REG 0xBC
#define NFC_POLY4_REG 0x60
#define NFC_POLY5_REG 0x64
#define NFC_POLY6_REG 0x68
#define NFC_POLY7_REG 0x6C

#define	NFC_PHY_CFG				(0x00DC)

#define NFC_DLL0_CFG 0xE0
#define NFC_DLL1_CFG 0xE4
#define NFC_DLL2_CFG 0xE8
#define NFC_DLL_REG 0xEC
#define NFC_CFG4_REG 0xF8

#define NFC_FREE_COUNT0_REG 0x160
#define NFC_FREE_COUNT1_REG 0x164
#define NFC_FREE_COUNT2_REG 0x168
#define NFC_FREE_COUNT3_REG 0x16C
#define NFC_FREE_COUNT4_REG 0x170

#define NFC_MAIN_ADDRH_REG 0x200
#define NFC_MAIN_ADDRL_REG 0x204
#define NFC_SPAR_ADDRH_REG 0x208
#define NFC_SPAR_ADDRL_REG 0x20C
#define NFC_STAT_ADDRH_REG 0x210
#define NFC_STAT_ADDRL_REG 0x214
#define NFC_SEED_ADDRH_REG 0x218
#define NFC_SEED_ADDRL_REG 0x21C
#define NFC_INST00_REG 0x220
#define NFC_INST14_REG 0x258
#define NFC_INST15_REG 0x25C
#define NFC_INST23_REG 0x270

#define FLASH_INTFTYPE_MASK			0x3
#define INTF_TYPE_OFFSET			12
enum {
	INTF_TYPE_LEGACY = 0,
	INTF_TYPE_ONFI2,
	INTF_TYPE_TOGGLE,
	INTF_TYPE_SPI
};

enum {
	AUTO_MODE = 0,
	ONLY_MASTER_MODE,
	ONLY_NAND_MODE,
	ONLY_ECC_MODE
};

enum {
	DBG_SAV_STOP = 0,
	DBG_SAV_START
};
/**************SNFC_Register******************************/
#define SNFC_COM_CFG		(0x0190)
#define SNFC_STATUS			(0x0194)
#define SNFC_FEATURE		(0x019C)
#define SNFC_CLK_CFG		(0x0180)
#define SNFC_IO_DLY_CFG		(0x0184)
#define SNFC_WP_HOLD		(0x0188)
#define SNFC_CS_TIME		(0x0128)
#define SNFC_RD_SMP_CTL		(0x012C)
#define SNFC_INST0			(0x0130)
#define SNFC_INST1			(0x0134)
#define SNFC_INST2			(0x0138)
#define SNFC_INST3			(0x013C)
#define SNFC_COM_CFG_CS_MSKCLR		(~(GENMASK(31, 30)))
#define SNFC_COM_CFG_SET_CS_SEL(cs)	(((cs) & 0x3) << 30)
#define SNFC_CS_MAX			(2)

#define NFC_MAX_MC_INST_NUM	24

/* NFC_START bit define --checked */
#define CTRL_NFC_VALID BIT(31)
#define CTRL_DEF1_ECC_MODE(mode)	(((mode) & 0xF) << 11)
#define CTRL_NFC_CMD_CLR BIT(1)
#define CTRL_NFC_CMD_START BIT(0)

/* NFC_CFG0 bit define --checked */
#define CFG0_DEF0_MAST_ENDIAN		(0)
#define CFG0_DEF1_SECT_NUM(num)		(((num - 1) & 0x1F) << 24)
#define CFG0_SET_SECT_NUM_MSK		(0x1F << 24)
#define CFG0_SET_REPEAT_NUM(num)	(((num - 1) & 0xFF) << 16)
#define CFG0_SET_WPN				BIT(15)
#define CFG0_DEF1_BUS_WIDTH(width)	(((!!(width != BW_08)) & 0x1) << 14)
#define CFG0_SET_SPARE_ONLY_INFO_PROD_EN	BIT(13)
#define CFG0_DEF0_SECT_NUM_IN_INST			(0 << 12)
#define CFG0_DEF0_DETECT_ALL_FF				BIT(11)
#define CFG0_SET_CS_SEL(cs)					(((cs) & 0x3) << 9)
#define CFG0_CS_MAX					4
#define CFG0_CS_MSKCLR				(~(GENMASK(10, 9)))
#define CFG0_SET_NFC_RW				BIT(8)
#define CFG0_DEF1_MAIN_SPAR_APT(sectperpage)	((sectperpage == 1) ? 0 : BIT(6))
#define CFG0_SET_SPAR_USE			BIT(5)
#define CFG0_SET_MAIN_USE			BIT(4)
#define CFG0_SET_ECC_EN				BIT(2)
#define CFG0_SET_NFC_MODE(mode)		((mode) & 0x3)

/* NFC_CFG1 bit define --checked */
#define CFG1_DEF1_SPAR_INFO_SIZE(size)	(((size) & 0x7F) << 24)
#define CFG1_DEF1_SPAR_SIZE(size)		((((size) - 1) & 0x7F) << 16)
#define CFG1_INTF_TYPE(type)			(((type) & 0x07) << 12)
#define CFG1_DEF_MAIN_SIZE(size)		((((size) - 1)) & 0x7FF)
#define CFG1_temp_MIAN_SIZE_MSK			(0x7FF)

/* NFC_CFG2 bit define --checked */
#define CFG2_DEF1_SPAR_SECTOR_NUM(num)	(((num - 1) & 0x1F) << 24)
#define CFG2_DEF1_SPAR_INFO_POS(pos)	(((pos) & 0x7F) << 16)
#define CFG2_DEF1_ECC_POSITION(pos)		((pos) & 0x7F)

/* NFC_CFG3 --checked may be unused  */
#define CFG3_SEED_LOOP_CNT(cnt)			(((cnt) & 0x3FF) << 16)
#define CFG3_SEED_LOOP_EN				BIT(2)
#define CFG3_DETECT_ALL_FF				BIT(3)
#define CFG3_POLY_4R1_EN				BIT(1)
#define CFG3_RANDOM_EN					BIT(0)

/* NFC_CFG4 --checked may be unused  */
#define CFG4_SLICE_CTL_DLY_SEL_REN_SMP(num)		((num) >> 16)
#define CFG4_ONLY_SEL_MODE				BIT(14)
#define CFG4_SLICE_EN_DELAY_SEL(num)			((num) >> 6)
#define CFG4_SLICE2_EN					BIT(5)
#define CFG4_SLICE1_EN					BIT(4)
#define CFG4_SLICE0_EN					BIT(3)
#define CFG4_PHY_DLL_CLK_2X_EN				BIT(2)
#define CFG4_SLICE_CLK_EN				BIT(1)
/* NFC_POLYNOMIALS --checked may be unused	*/
#define NFC_POLYNOMIALS0	0x100d
#define NFC_POLYNOMIALS1	0x10004
#define NFC_POLYNOMIALS2	0x40013
#define NFC_POLYNOMIALS3	0x400010

/* NFC register --checked */
#define INT_DONE_RAW		BIT(24)
#define INT_STSMCH_CLR		BIT(11)
#define INT_WP_CLR			BIT(10)
#define INT_TO_CLR			BIT(9)
#define INT_DONE_CLR		BIT(8)
#define INT_STSMCH			BIT(3)
#define INT_WP				BIT(2)
#define INT_TO				BIT(1)
#define INT_DONE			BIT(0)

/* NFC_TIMING0 --checked */
#define NFC_ACS_OFFSET		(27)
#define NFC_ACS_MASK		(GENMASK((NFC_ACS_OFFSET + 4), NFC_ACS_OFFSET))
#define NFC_ACE_OFFSET		(22)
#define NFC_ACE_MASK		(GENMASK((NFC_ACE_OFFSET + 4), NFC_ACE_OFFSET))
#define NFC_RWS_OFFSET		(16)
#define NFC_RWS_MASK		(GENMASK((NFC_RWS_OFFSET + 5), NFC_RWS_OFFSET))
#define NFC_RWE_OFFSET		(11)
#define NFC_RWE_MASK		(GENMASK((NFC_RWE_OFFSET + 5), NFC_RWE_OFFSET))
#define NFC_RWH_OFFSET		(6)
#define NFC_RWH_MASK		(GENMASK((NFC_RWH_OFFSET + 4), NFC_RWH_OFFSET))
#define NFC_RWL_MASK		(GENMASK(5, 0))

#define TIME0_ACS(acs)		(((acs - 1) & 0x1F) << 27)
#define TIME0_ACE(ace)		(((ace - 1) & 0x1F) << 22)
#define TIME0_RDS(rds)		(((rds - 1) & 0x3F) << 16)
#define TIME0_RDE(rde)		(((rde - 1) & 0x1F) << 11)
#define TIME0_RWH(rwh)		(((rwh - 1) & 0x1F) << 6)
#define TIME0_RWL(rwl)		((rwl - 1) & 0x3F) /* must >= 2  */

/* NFC_TIMING1 --checked may be unused */
#define TIME1_WTE(wte)		(((wte - 1) & 0x1F) << 26)
#define TIME1_WTS(wts)		(((wts - 1) & 0x1F) << 21)
#define TIME1_WTI(wti)		(((wti - 1) & 0x1F) << 16)
#define TIME1_CL0(cl0)		(((cl0 - 1) & 0x1F) << 10)
#define TIME1_CL1(cl1)		(((cl1 - 1) & 0x1F) << 5)
#define TIME1_RDI(rdi)		(((rdi - 1) & 0x1F) << 0)

/* NFC_TIMEOUT bit define --checked */
#define TIMEOUT_REPT_EN		BIT(31)
#define TIMEOUT(val)		(val & 0x7FFFFFFF)
/* NFC Ram address --checked */
/* 0xFFFF'FFFF means not to move data to read buf, when read. */
#define RAM_MAIN_ADDRL(addr)	((unsigned long)(addr) & 0xffffffff)
#define RAM_MAIN_ADDRH(addr)	((((unsigned long)(addr)) >> 32) & 0xffffffff)
/* 0xFFFF'FFFF means not to move data to read buf, when read. */
#define RAM_SPAR_ADDRL(addr)	((unsigned long)(addr)&0xffffffff)
#define RAM_SPAR_ADDRH(addr)	((((unsigned long)(addr)) >> 32) & 0xffffffff)
/* 0xFFFF'FFFF means not to move data to read buf, when read. */
#define RAM_STAT_ADDRL(addr)	((unsigned long)(addr) & 0xffffffff)
#define RAM_STAT_ADDRH(addr)	((((unsigned long)(addr)) >> 32) & 0xffffffff)
#define RAM_SEED_ADDRL(addr)	((unsigned long)(addr) & 0xffffffff)
#define RAM_SEED_ADDRH(addr)	((((unsigned long)(addr)) >> 32) & 0xffffffff)
/* NFC status mach --checked */
/* check IO 0_bit, stop if error */
#define MACH_ERASE	(BIT(0) | BIT(11) | BIT(22))
/* check IO 0_bit, stop if error */
#define MACH_WRITE	(BIT(0) | BIT(11) | BIT(22))
#define MACH_READ	(0)
#define DEF0_MATCH	(0)
/* NFC_STATUS0_REG bit define --checked */
#define ECC_STAT(status)	(((status) >> 9) & 0x3)
#define ECC_NUM(status)		((status) & 0x1FF)
/* NFC Micro-Instrction Register --checked */
#define CMD_TYPE1(cmdid, param)                                                \
		((u16)(((param & 0xff) << 8) | (cmdid & 0xff)))
#define CMD_TYPE2(cmdid)	((u16)(cmdid & 0xff))
#define CMD_TYPE3(cmdid, param1, param0)                                       \
		((u16)(((param1 & 0xff) << 8) | ((cmdid & 0xf) << 4) | (param0 & 0xf)))
#define INST_CMD(cmd)			CMD_TYPE1(0xCD, (cmd))
#define INST_ADDR(addr, step)	CMD_TYPE3(0x0A, (addr), (step))
#define INST_WRB0()				CMD_TYPE2(0xB0)
#define INST_WRB1(cycle)		CMD_TYPE1(0xB1, (cycle))
#define INST_MRDT()				CMD_TYPE2(0xD0)
#define INST_MWDT()				CMD_TYPE2(0xD1)
#define INST_SRDT()				CMD_TYPE2(0xD2)
#define INST_SWDT()				CMD_TYPE2(0xD3)
#define INST_IDST(num)			CMD_TYPE1(0xDD, (num - 1))
/* 0 or 1, priority > _CFG0_CS_SEL */
#define INST_CSEN(en)			CMD_TYPE1(0xCE, (en))
#define INST_INOP(num)			CMD_TYPE1(0xF0, (num - 1))
#define INST_DONE()				CMD_TYPE2(0xFF)

/**************SNFC_CMD******************************/
#define SNF_WRTE_EN				(0x06)	//write enable
#define SNF_WRTE_DIS			(0x04)	//write disable
#define SNF_GET_FEATURE			(0x0F)	//get feature
#define SNF_SET_FEATURE			(0x1F)	//set feature
#define SNF_READ_TO_CACHE		(0x13)	//page read (to cache)
/* read from cache,the Command can also be used with 0x03 */
#define SNF_READ_FR_CACHE		(0x0B)
#define SNF_READ_FR_CACHE_X2	(0x3B)	//read from cache X2
#define SNF_READ_FR_CACHE_X4	(0x6B)	//read from cache X4
#define SNF_READ_FR_CACHE_D		(0xBB)	//read from cache Dual IO
#define SNF_READ_FR_CACHE_Q		(0xEB)	//read from cache Quad IO
#define SNF_READ_ID				(0x9F)	//read ID
#define SNF_PROGRAM				(0x02)	//program Load
#define SNF_PROGRAM_X4			(0x32)	//program Load X4
#define SNF_PROGRAM_EXEC		(0x10)	//program execute
#define SNF_PROGRAM_RDM			(0x84)	//program load random data
/* program load random data X4,the Command can also be used with 0x34 */
#define SNF_PROGRAM_RDM_X4		(0xC4)
#define SNF_PROGRAM_RDM_Q		(0x72)	//program load random data Quad IO
#define SNF_ERASE				(0xD8)	//block erase (128K)
#define SNF_RESET				(0xFF)	//reset

#define SNFC_MC_WBTY_ID		(0x0)
#define SNFC_MC_RBTY_ID		(0x1)
#define SNFC_MC_MWDT_ID		(0x2)
#define SNFC_MC_MRDT_ID		(0x3)
#define SNFC_MC_SWDT_ID		(0x4)
#define SNFC_MC_SRDT_ID		(0x5)
#define SNFC_MC_DUMY_ID		(0x6)
#define SNFC_MC_EXEC_ID		(0xE)
#define SNFC_MC_DONE_ID		(0xF)

#define MICR_CODE_MASK		(0xf)
#define MICR_CODE_OFFSET	(12)

#define ADD_MASK			(0x1)
#define ADD_OFFSET			(10)

#define MVS_MASK			(0x1)
#define MVS_OFFSET			(9)

#define CHK_MASK			(0x1)
#define CHK_OFFSET			(8)

#define BIT_MODE_MASK		(0x2)
#define BIT_MODE_OFFSET		(8)

#define DATA_MASK			(0xff)
#define DATA_OFFSET			(0)

#define MASK_MASK			(0xff)
#define MASK_OFFSET			(0)

#define SNFC_MC_CMD(cmd)			((u16)((cmd) & 0xff))
#define SNFC_MC_ADDR(add)			((u16)((add) & 0xff))
#define SNFC_MC_REPEAT_ADDR(add)	((u16)((0x4<<16) | ((add) & 0xff)))
#define SNFC_DUMMY_ADDR()			((u16)(0x6000))
#define SNFC_MC_READ_STAT()			((u16)(0x1000))

#define BIT_MODE_1			(0x0)
#define BIT_MODE_2			(0x1)
#define BIT_MODE_4			(0x2)

#define SNFC_MC_WBTY(add, bit_mode, data)	\
	((u16)(((SNFC_MC_WBTY_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET)\
	| (((add) & ADD_MASK) << ADD_OFFSET)\
	| (((bit_mode) & BIT_MODE_MASK) << BIT_MODE_OFFSET)))
#define SNFC_MC_RBTY(bit_mode)	\
	((u16)(((SNFC_MC_RBTY_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET)\
			| (((bit_mode) & BIT_MODE_MASK) << BIT_MODE_OFFSET)))
#define SNFC_MC_MWDT(bit_mode)	\
	((u16)(((SNFC_MC_MWDT_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET)\
			| (((bit_mode) & BIT_MODE_MASK) << BIT_MODE_OFFSET)))
#define SNFC_MC_MRDT(bit_mode)	\
	((u16)(((SNFC_MC_MRDT_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET)\
			| (((bit_mode) & BIT_MODE_MASK) << BIT_MODE_OFFSET)))
#define SNFC_MC_SWDT(bit_mode)	\
	((u16)(((SNFC_MC_SWDT_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET)\
			| (((bit_mode) & BIT_MODE_MASK) << BIT_MODE_OFFSET)))
#define SNFC_MC_SRDT(bit_mode)	\
	((u16)(((SNFC_MC_SRDT_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET)\
			| (((bit_mode) & BIT_MODE_MASK) << BIT_MODE_OFFSET)))
#define SNFC_MC_DUMY(bit_mode)	\
	((u16)(((SNFC_MC_DUMY_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET)\
			| (((bit_mode) & BIT_MODE_MASK) << BIT_MODE_OFFSET)))
#define SNFC_MC_EXEC(mvs, chk, mask)	\
	((u16)(((SNFC_MC_EXEC_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET)\
			| (((mvs) & MVS_MASK) << MVS_OFFSET) | (((chk) & CHK_MASK) << CHK_OFFSET)\
			| (((mask) & MASK_MASK) << MASK_OFFSET)))
#define SNFC_MC_DONE()	((u16)((SNFC_MC_DONE_ID & MICR_CODE_MASK) << MICR_CODE_OFFSET))

/* Other define */
#define NFC_MAX_CHIP 4
#define NFC_TIMEOUT_VAL 3000000      /* usecs */
#define NFC_RESET_TIMEOUT_VAL 500000 /* usecs */

#define NFC_READID_TIMING	\
		((0x1f) | (7 << NFC_RWH_OFFSET) | (0x1f << NFC_RWE_OFFSET)\
			| (0x1f << NFC_RWS_OFFSET) | (0x1f << NFC_ACE_OFFSET)\
			| (0x1f << NFC_ACS_OFFSET))
#define NFC_DEFAULT_TIMING	\
		((7) | (6 << NFC_RWH_OFFSET) | (7 << NFC_RWE_OFFSET)\
			| (7 << NFC_RWS_OFFSET) | (7 << NFC_ACE_OFFSET) | (7 << NFC_ACS_OFFSET))
#define GET_CYCLE(ns)	\
		((u32)(((u32)(((host->frequency / 1000000) * ns) / 1000)) + 1))
#define NFC_MBUF_SIZE	4096
#define SEED_TBL_SIZE	64
#define SEED_BUF_SIZE	68
#define PAGE2SEED_ADDR_OFFSET(pg)	((pg & GENMASK(3, 0)) << 2)
#define SPL_MAX_SIZE	BIT(16)
#define SPRD_NAND_PAGE_MASK			GENMASK(7, 0)
#define SPRD_NAND_PAGE_SHIFT_0(pg)	((pg) & SPRD_NAND_PAGE_MASK)
#define SPRD_NAND_PAGE_SHIFT_8(pg)	(((pg) >> 8) & SPRD_NAND_PAGE_MASK)
#define SPRD_NAND_PAGE_SHIFT_16(pg)	(((pg) >> 16) & SPRD_NAND_PAGE_MASK)
#define SPRD_NAND_GET_INT_VAL(x)	((x >> 24) & 0xF)

#define SPRD_NANDC_VERSIONS_R6P0 60
#define SPRD_NANDC_VERSIONS_R8P0 80

struct sprd_nand_vendor_param {
	u8 id[8];
	u32 nblkcnt;
	u32 npage_per_blk;
	u32 nsect_per_page;
	u32 nsect_size;
	u32 nspare_size;

	u32 page_size; /* no contain spare */
	u32 main_size; /* page_size */
	u32 spare_size;

	u32 badflag_pos;
	u32 badflag_len;
	u32 ecc_mode;
	u32 ecc_pos;
	u32 ecc_size;
	u32 info_pos;
	u32 info_size;

	u8 nbus_width;
	u8 ncycle;

	/* ACS */
	u32 t_als;
	u32 t_cls;
	/* ACE */
	u32 t_clh;
	u32 t_alh;
	/* RWS */
	u32 t_rr;
	u32 t_adl;
	/* RWE */

	/* RWH & RWL */
	u32 t_wh;  /* we high when write */
	u32 t_wp;  /* we low  when write */
	u32 t_reh; /* re high when read */
	u32 t_rp;  /* re low when read */

	u8 cell_type;
	u8 intf_type;
	u8 supt_cache_func;
	int plane_sel_bit;
};

struct ext_param {
	u32 nsect_size;
	u32 nbus_width;
	u32 ncycle;
	u32 info_pos;
	u32 info_size;
};

struct nand_inst {
	char *program_name;
	u16 cnt;
	u16 step_tag;
	u32 int_bits;
	u16 inst[NFC_MAX_MC_INST_NUM];
	u16 step_tag2;
	u16 step_tag3;
};

struct sprd_nfc;
struct sprd_nfc_ops {
	int (*ctrl_en)(struct sprd_nfc *host, bool en);
	void (*ctrl_suspend)(struct sprd_nfc *host);
	int (*ctrl_resume)(struct sprd_nfc *host);
	int (*read_page)(struct sprd_nfc *host, u32 page,
			u32 num, u32 *ret_num,
			u32 if_has_mbuf, u32 if_has_sbuf,
			u32 mode, struct mtd_ecc_stats *ecc_sts,
			bool random,
			int flag);
	int (*write_page)(struct sprd_nfc *host, u32 page,
			u32 num, u32 *ret_num,
			u32 if_has_mbuf, u32 if_has_sbuf,
			u32 mode,
			bool random);
	int (*erase)(struct sprd_nfc *host, u32 page);
	int (*block_is_bad)(struct sprd_nfc *host, u32 page, bool random);
	int (*block_mark_bad)(struct sprd_nfc *host, u32 page, bool random);
	int (*probe_device)(struct sprd_nfc *host, u32 *id0, u32 *id1);
};

struct sprd_nfc_internal_ops {
	void (*prepare_before_exec)(struct sprd_nfc *host);
	void (*select_cs)(struct sprd_nfc *host, int cs);
	void (*delect_cs)(struct sprd_nfc *host, int ret);
	void (*init_reg_state0)(struct sprd_nfc *host);
	void (*init_reg_state1)(struct sprd_nfc *host);
	void (*init_inst)(struct sprd_nfc *host);
	void (*set_wp)(struct sprd_nfc *host, bool en);
	void (*set_randomize)(struct sprd_nfc *host, u32 page, bool en);
	int (*ecc_analyze)(struct sprd_nfc *host, u32 num,
			struct mtd_ecc_stats *ecc_sts, u32 mode);
	int (*reset)(struct sprd_nfc *host);
	int (*readid)(struct sprd_nfc *host, u32 *id0, u32 *id1);
	void (*cmd_change)(struct sprd_nfc *host, struct nand_inst *cmd_inst, u32 pg);
};

struct sprd_nfc {
	struct nand_device nand;

	struct mutex lock;
	int suspended;

	/* resource0: interrupt id & clock name &  register bass address */
	int irq;
	u32 frequency;
	struct device *dev;
	void __iomem *ioaddr;
	struct clk *nandc_2x_clk;
	struct clk *nandc_ecc_clk;
	struct clk *nandc_2xwork_clk;
	struct clk *nandc_eccwork_clk;
	struct clk *nandc_ahb_enable;
	struct clk *nandc_ecc_clk_eb;
	struct clk *nandc_26m_clk_eb;
	struct clk *nandc_1x_clk_eb;
	struct clk *nandc_2x_clk_eb;

	u8 intf_type;

	/* resource1: nand param & mtd ecclayout & risk threshold */
	struct sprd_nand_vendor_param param;
	u32 eccmode_reg;
	u32 risk_threshold;
	u32 cs_max;
	u32 cs[CFG0_CS_MAX];
	u32 csnum;
	int cur_cs;

	/* u32 csDis; */
	u32 sect_perpage_shift;
	u32 sectshift;
	u32 sectmsk;
	u32 pageshift;
	u32 blkshift;
	u32 chipshift;
	u32 c_pages;
	u32 bufshift;
	u32 badblkcnt;
	/* resource3: local DMA buffer */
	u8 *buf_p;
	u8 *buf_v;
	u8 *mbuf_p;
	u8 *mbuf_v;
	u8 *sbuf_p;
	u8 *sbuf_v;
	u8 *stsbuf_p;
	u8 *stsbuf_v;
	u32 *seedbuf_p;
	u32 *seedbuf_v;
	u32 buf_size;
	u32 mbuf_size;
	u32 sbuf_size;
	u32 stsbuf_size;
	u32 seedbuf_size;
	/*
	 * resource4: register base value. some value is const
	 * while operate on nand flash, we store it to local valuable.
	 * Time & cfg & status mach. It is different with operation R W E.
	 */
	u32 nfc_time0_r;
	u32 nfc_time0_w;
	u32 nfc_time0_e;

	u32 nfc_start;
	u32 nfc_cfg0;
	u32 nfc_cfg1;
	u32 nfc_cfg2;
	u32 nfc_cfg3;
	u32 nfc_cfg4;

	u32 snfc_com_cfg;
	u32 snfc_clk_cfg;
	u32 snfc_rd_smp_ctl;

	u32 nfc_time0_val;
	u32 nfc_cfg0_val;
	u32 nfc_sts_mach_val;

	struct nand_inst inst_read_main_spare;
	struct nand_inst inst_read_main_raw;
	struct nand_inst inst_read_spare_raw;
	struct nand_inst inst_write_main_spare;
	struct nand_inst inst_write_main_raw;
	struct nand_inst inst_write_spare_raw;
	struct nand_inst inst_erase;

	bool randomizer;

	struct sprd_nfc_ops *ops;

	struct sprd_nfc_internal_ops *int_ops;

	u32 version;

	void *priv;
};

struct nandc_variant_ops {
	u32 version;
};

static inline struct sprd_nfc *mtd_to_sprd_nfc(struct mtd_info *mtd)
{
	return container_of(mtd_to_nanddev(mtd), struct sprd_nfc, nand);
}

static inline struct sprd_nfc *nanddev_to_sprd_nfc(struct nand_device *nand)
{
	return container_of(nand, struct sprd_nfc, nand);
}

static inline void sprd_nfc_writel(struct sprd_nfc *host,
	unsigned long val, unsigned long reg)
{
	writel_relaxed(val, host->ioaddr + reg);
}

static inline unsigned long sprd_nfc_readl(struct sprd_nfc *host,
	unsigned long reg)
{
	return readl_relaxed(host->ioaddr + reg);
}

#define GETCS(page) \
	host->cs[((page) >> ((host->chipshift) - (host->pageshift)))]

void sprd_nfc_cmd_init(struct nand_inst *cmd_inst, u32 position);
void sprd_nfc_cmd_add(struct nand_inst *cmd_inst, u16 inst_p);
void sprd_nfc_cmd_tag(struct nand_inst *cmd_inst);

void sprd_nfc_cmd_exec(struct sprd_nfc *host, struct nand_inst *program, u32 repeat,
		u32 if_use_int);
int sprd_nfc_cmd_wait(struct sprd_nfc *host, struct nand_inst *program);
void sprd_nfc_prepare_before_exec(struct sprd_nfc *host);

struct sprd_nfc *sprd_nfc_alloc_host(struct device *dev, u8 intf_type);
int sprd_nfc_setup_host(struct sprd_nfc *host,
		struct sprd_nand_param *param,
		u32 c_pages);
void sprd_nfc_free_host(struct sprd_nfc *host);

void sprd_snfc_attach(struct sprd_nfc *host);
int sprd_snfc_setup_host(struct sprd_nfc *host);
#endif

