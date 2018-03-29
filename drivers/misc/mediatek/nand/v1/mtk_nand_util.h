/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_NAND_UTIL_H
#define __MTK_NAND_UTIL_H
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand_ecc.h>
#include "nand_device_list.h"
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
#include "partition_define_tlc.h"
#else
#include "partition_define_mlc.h"
#endif

#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
  #define TRUE  (1)
#endif

#ifndef NULL
  #define NULL  (0)
#endif

#ifndef ASSERT
    #define ASSERT(expr)        BUG_ON(!(expr))
#endif

#define DRV_Reg8(x) __raw_readb(x)
#define DRV_Reg16(x) __raw_readw(x)
#define DRV_Reg32(x) __raw_readl(x)

#define DRV_WriteReg8(x, y) __raw_writeb(y, x)
#define DRV_WriteReg16(x, y) __raw_writew(y, x)
#define DRV_WriteReg32(x, y) __raw_writel(y, x)

extern bool init_pmt_done;
/*******************************************************************************
 * Data Structure Definition
 *******************************************************************************/
struct nfi_saved_para {
	u8 suspend_flag;
	u16 sNFI_CNFG_REG16;
	u32 sNFI_PAGEFMT_REG16;
	u32 sNFI_CON_REG16;
	u32 sNFI_ACCCON_REG32;
	u16 sNFI_INTR_EN_REG16;
	u16 sNFI_IOCON_REG16;
	u16 sNFI_CSEL_REG16;
	u16 sNFI_DEBUG_CON1_REG16;

	u32 sECC_ENCCNFG_REG32;
	u32 sECC_FDMADDR_REG32;
	u32 sECC_DECCNFG_REG32;

	u32 sSNAND_MISC_CTL;
	u32 sSNAND_MISC_CTL2;
	u32 sSNAND_DLY_CTL1;
	u32 sSNAND_DLY_CTL2;
	u32 sSNAND_DLY_CTL3;
	u32 sSNAND_DLY_CTL4;
	u32 sSNAND_CNFG;
};

struct mtk_nand_pl_test {
	suseconds_t last_erase_time;
	suseconds_t last_prog_time;
	u32 nand_program_wdt_enable;
	u32 nand_erase_wdt_enable;
};
struct mtk_nand_host {
	struct nand_chip nand_chip;
	struct mtd_info mtd;
	struct mtk_nand_host_hw *hw;
#ifdef CONFIG_PM
	struct nfi_saved_para saved_para;
#endif
#ifdef CONFIG_PWR_LOSS_MTK_SPOH
	struct mtk_nand_pl_test pl;
#endif
	struct mtd_erase_region_info erase_region[20];
};

struct NAND_CMD {
	u32 u4ColAddr;
	u32 u4RowAddr;
	u32 u4OOBRowAddr;
	u8 au1OOB[128];
	u8 *pDataBuf;
#ifdef NAND_PFM
	u32 pureReadOOB;
	u32 pureReadOOBNum;
#endif
};
enum readCommand {
	NORMAL_READ = 0,
	AD_CACHE_READ,
	AD_CACHE_FINAL
};
extern struct flashdev_info_t gn_devinfo;
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);

bool mtk_nand_SetFeature(struct mtd_info *mtd, u16 cmd, u32 addr, u8 *value, u8 bytes);
bool mtk_nand_GetFeature(struct mtd_info *mtd, u16 cmd, u32 addr, u8 *value, u8 bytes);
extern void part_init_pmt(struct mtd_info *mtd, u8 *buf);
extern u64 part_get_startaddress(u64 byte_address, u32 *idx);
extern bool raw_partition(u32 index);
extern struct mtd_perf_log g_MtdPerfLog;
extern struct mtd_partition g_pasStatic_Partition[];
extern int part_num;
extern struct mtd_partition g_exist_Partition[];
extern struct mtd_partition g_pasStatic_Partition[PART_MAX_COUNT];
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
extern u64 OFFSET(u32 block);
extern void mtk_pmt_reset(void);
extern bool mtk_nand_IsBMTPOOL(loff_t logical_address);
#endif
extern bool mtk_block_istlc(u64 addr);
extern void mtk_slc_blk_addr(u64 addr, u32 *blk_num, u32 *page_in_block);
bool mtk_is_normal_tlc_nand(void);
int mtk_nand_tlc_block_mark(struct mtd_info *mtd, struct nand_chip *chip, u32 mapped_block);
extern int mtk_nand_write_tlc_block_hw(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, u32 mapped_block, u32 page_in_block, u32 size);

void show_stack(struct task_struct *tsk, unsigned long *sp);
extern int mtk_nand_interface_async(void);

extern int mtk_nand_exec_read_page(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize, u8 *pPageBuf, u8 *pFDMBuf);
extern int mtk_nand_block_bad_hw(struct mtd_info *mtd, loff_t ofs);
extern int mtk_nand_erase_hw(struct mtd_info *mtd, int page);
extern int mtk_nand_block_markbad_hw(struct mtd_info *mtd, loff_t offset);
extern int mtk_nand_exec_write_page(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize, u8 *pPageBuf, u8 *pFDMBuf);

u32 MICRON_TRANSFER(u32 pageNo);
u32 SANDISK_TRANSFER(u32 pageNo);
u32 HYNIX_TRANSFER(u32 pageNo);
u32 hynix_pairpage_mapping(u32 page, bool high_to_low);
u32 micron_pairpage_mapping(u32 page, bool high_to_low);
u32 sandisk_pairpage_mapping(u32 page, bool high_to_low);
/* NAND driver */
struct mtk_nand_host_hw {
	unsigned int nfi_bus_width;							/* NFI_BUS_WIDTH */
	unsigned int nfi_access_timing;					/* NFI_ACCESS_TIMING */
	unsigned int nfi_cs_num;								/* NFI_CS_NUM */
	unsigned int nand_sec_size;							/* NAND_SECTOR_SIZE */
	unsigned int nand_sec_shift;						/* NAND_SECTOR_SHIFT */
	unsigned int nand_ecc_size;
	unsigned int nand_ecc_bytes;
	unsigned int nand_ecc_mode;
	unsigned int nand_fdm_size;            /*FDM size, for 8163 tlc*/
};
extern struct mtk_nand_host_hw mtk_nand_hw;

#define NFI_DEFAULT_ACCESS_TIMING        (0x44333)

/* uboot only support 1 cs */
#define NFI_CS_NUM                  (2)
#define NFI_DEFAULT_CS				(0)

/*
 *	ECC layout control structure. Exported to userspace for
 *  diagnosis and to allow creation of raw images
struct nand_ecclayout {
	uint32_t eccbytes;
	uint32_t eccpos[64];
	uint32_t oobavail;
	struct nand_oobfree oobfree[MTD_MAX_OOBFREE_ENTRIES];
};
*/
#define __DEBUG_NAND		1	/* Debug information on/off */

/* Debug message event */
#define DBG_EVT_NONE		0x00000000	/* No event */
#define DBG_EVT_INIT		0x00000001	/* Initial related event */
#define DBG_EVT_VERIFY		0x00000002	/* Verify buffer related event */
#define DBG_EVT_PERFORMANCE	0x00000004	/* Performance related event */
#define DBG_EVT_READ		0x00000008	/* Read related event */
#define DBG_EVT_WRITE		0x00000010	/* Write related event */
#define DBG_EVT_ERASE		0x00000020	/* Erase related event */
#define DBG_EVT_BADBLOCK	0x00000040	/* Badblock related event */
#define DBG_EVT_POWERCTL	0x00000080	/* Suspend/Resume related event */
#define DBG_EVT_OTP	0x00000100	/* OTP related event */

#define DBG_EVT_ALL			0xffffffff

#define DBG_EVT_MASK	(DBG_EVT_INIT|DBG_EVT_POWERCTL)

#if __DEBUG_NAND
#define MSG(evt, fmt, args...) \
do {	\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		pr_warn(fmt, ##args); \
	} \
} while (0)

#define MSG_FUNC_ENTRY(f)	MSG(FUC, "<FUN_ENT>: %s\n", __func__)
#else
#define MSG(evt, fmt, args...) do {} while (0)
#define MSG_FUNC_ENTRY(f)	   do {} while (0)
#endif

#endif
