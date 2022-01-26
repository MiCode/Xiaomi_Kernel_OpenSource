/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NFC_ARIES_H__
#define __NFC_ARIES_H__

#include "nandx_util.h"
#include "nandx_info.h"
#include "nfc_core.h"

/* nfi register define + */
#define NFI_CNFG		(0x00)
#define		CNFG_AHB		BIT(0)
#define		CNFG_READ_EN		BIT(1)
#define		CNFG_DMA_BURST_EN	BIT(2)
#define		CNFG_RESEED_SEC_EN	BIT(4)
#define		CNFG_RAND_SEL		BIT(5)
#define		CNFG_BYTE_RW		BIT(6)
#define		CNFG_HW_ECC_EN		BIT(8)
#define		CNFG_AUTO_FMT_EN	BIT(9)
#define		CNFG_RAND_MASK		(3 << 4)
#define		CNFG_OP_CUST		(6 << 12)
#define NFI_PAGEFMT		(0x04)
#define		PAGEFMT_FDM_ECC_SHIFT	(12)
#define		PAGEFMT_FDM_SHIFT	(8)
#define		PAGEFMT_SEC_SEL_512	BIT(2)
#define		PAGEFMT_512_2K		(0)
#define		PAGEFMT_2K_4K		(1)
#define		PAGEFMT_4K_8K		(2)
#define		PAGEFMT_8K_16K		(3)
#define NFI_CON			(0x08)
#define		CON_FIFO_FLUSH		BIT(0)
#define		CON_NFI_RST		BIT(1)
#define		CON_BRD			BIT(8)
#define		CON_BWR			BIT(9)
#define		CON_SEC_SHIFT		(12)
#define NFI_ACCCON		(0x0c)
#define NFI_INTR_EN		(0x10)
#define		INTR_BUSY_RETURN_EN	BIT(4)
#define		INTR_AHB_DONE_EN	BIT(6)
#define NFI_INTR_STA		(0x14)
#define NFI_CMD			(0x20)
#define NFI_ADDRNOB		(0x30)
#define		ROW_SHIFT		(4)
#define NFI_COLADDR		(0x34)
#define NFI_ROWADDR		(0x38)
#define NFI_STRDATA		(0x40)
#define		STAR_EN			(1)
#define		STAR_DE			(0)
#define NFI_CNRNB		(0x44)
#define NFI_DATAW		(0x50)
#define NFI_DATAR		(0x54)
#define NFI_PIO_DIRDY		(0x58)
#define		PIO_DI_RDY		(0x01)
#define NFI_STA			(0x60)
#define		STA_CMD			BIT(0)
#define		STA_ADDR		BIT(1)
#define		FLASH_MACRO_IDLE	BIT(5)
#define		STA_BUSY		BIT(8)
#define		STA_BUSY2READY		BIT(9)
#define		STA_EMP_PAGE		BIT(12)
#define		NFI_FSM_CUSTDATA	(0xe << 16)
#define		NFI_FSM_MASK		(0xf << 16)
#define NFI_ADDRCNTR		(0x70)
#define		CNTR_MASK		GENMASK(16, 12)
#define		ADDRCNTR_SEC_SHIFT	(12)
#define		ADDRCNTR_SEC(val) \
		(((val) & CNTR_MASK) >> ADDRCNTR_SEC_SHIFT)
#define NFI_STRADDR		(0x80)
#define NFI_BYTELEN		(0x84)
#define NFI_CSEL		(0x90)
#define NFI_FDML(x)		(0xa0 + (x) * sizeof(u32) * 2)
#define NFI_FDMM(x)		(0xa4 + (x) * sizeof(u32) * 2)
#define		FDM_MAX_SIZE		(8)
#define NFI_DEBUG_CON1		(0x220)
#define		STROBE_MASK		GENMASK(4, 3)
#define		STROBE_SHIFT		(3)
#define		ECC_CLK_EN		BIT(11)
#define		AUTOC_SRAM_MODE		BIT(12)
#define		BYPASS_MASTER_EN	BIT(15)
#define NFI_MASTER_STA		(0x224)
#define		MASTER_STA_MASK		(0x0FFF)
#define		MASTER_BUS_BUSY		(0x3)
#define NFI_SECCUS_SIZE		(0x22c)
#define		SECCUS_SIZE_EN		BIT(17)
#define NFI_RANDOM_CNFG		(0x238)
#define		RAN_ENCODE_EN		BIT(0)
#define		ENCODE_SEED_SHIFT	(1)
#define		RAN_DECODE_EN		BIT(16)
#define		DECODE_SEED_SHIFT	(17)
#define		RAN_SEED_MASK		(0x7fff)
#define NFI_EMPTY_THRESH	(0x23c)
#define NFI_NAND_TYPE_CNFG	(0x240)
#define		NAND_TYPE_ASYNC		(0)
#define		NAND_TYPE_TOGGLE	(1)
#define		NAND_TYPE_SYNC		(2)
#define NFI_ACCCON1		(0x244)
#define NFI_DELAY_CTRL		(0x248)
#define NFI_TLC_RD_WHR2		(0x300)
#define		TLC_RD_WHR2_EN		BIT(12)
#define		TLC_RD_WHR2_MASK	GENMASK(11, 0)

#define ACCTIMING(tpoecs, tprecs, tc2r, tw2r, twh, twst, trlt) \
		((tpoecs) << 28 | (tprecs) << 22 | (tc2r) << 16 | \
		(tw2r) << 12 | (twh) << 8 | (twst) << 4 | (trlt))
#define ACCTIMING1(trdpre, twrpre, trdpst, twrpst) \
		((trdpre << 24) | (twrpre) << 16 | (trdpst) << 8 | (twrpst))
#define MTK_TIMEOUT		(1000000)
#define MTK_RESET_TIMEOUT	(1000000)
#define MTK_MAX_SECTOR		(16)
/* nfi register define - */

/* ecc register define + */
#define ECC_ENCCON		(0x00)
#define ECC_ENCCNFG		(0x04)
#define ECC_MODE_SHIFT		(5)
#define ECC_MS_SHIFT		(16)
#define ECC_ENCDIADDR		(0x08)
#define ECC_ENCIDLE		(0x0c)
#define ECC_ENCSTA		(0x7c)
#define		ENC_IDLE		BIT(0)
#define ECC_ENCIRQ_EN		(0x80)
#define		ECC_IRQ_EN		BIT(0)
#define ECC_ENCIRQ_STA		(0x84)
#define		ECC_PG_IRQ_SEL		BIT(1)
#define ECC_PIO_DIRDY		(0x90)
#define		PIO_DI_RDY		(0x01)
#define ECC_PIO_DI		(0x94)
#define ECC_DECCON		(0x100)
#define ECC_DECCNFG		(0x104)
#define		DEC_EMPTY_EN		BIT(31)
#define		DEC_CON_SHIFT		(12)
#define ECC_DECDIADDR		(0x108)
#define ECC_DECIDLE		(0x10c)
#define ECC_DECENUM(x)		(0x114 + (x) * sizeof(u32))
#define		ERR_MASK		(0x7f)
#define ECC_DECDONE		(0x124)
#define ECC_DECIRQ_EN		(0x200)
#define ECC_DECIRQ_STA		(0x204)
#define ECC_DECFSM		(0x208)
#define		FSM_MASK		(0x3f3fff0f)
#define		FSM_IDLE		(0x01011101)
#define ECC_BYPASS		(0x20c)
#define		ECC_BYPASS_EN		BIT(0)
#define EL_MASK			(0x3fff)

#define ECC_TIMEOUT		(1000000)
#define ECC_IDLE_REG(op) \
	((op) == ECC_ENCODE ? ECC_ENCIDLE : ECC_DECIDLE)
#define ECC_CTL_REG(op) \
	((op) == ECC_ENCODE ? ECC_ENCCON : ECC_DECCON)
#define ECC_IRQ_REG(op) \
	((op) == ECC_ENCODE ? ECC_ENCIRQ_EN : ECC_DECIRQ_EN)
#define ECC_CNFG_REG(op) \
	((op) == ECC_ENCODE ? ECC_ENCCNFG : ECC_DECCNFG)
#define ECC_DIADDR_REG(op) \
	((op) == ECC_ENCODE ? ECC_ENCDIADDR : ECC_DECDIADDR)
#define ECC_PARITY_BITS		(14)
#define ECC_IDLE_MASK		BIT(0)
#define ECC_OP_ENABLE		(1)
#define ECC_OP_DISABLE		(0)
/* ecc register define - */

#define NFI_IRQ_NAME	"mtk-nfi"
#define ECC_IRQ_NAME	"ecc-nfi"

/* structure define + */
enum ECC_MODE {
	ECC_DMA_MODE = 0,
	ECC_NFI_MODE = 1,
	ECC_PIO_MODE = 2
};

enum ECC_DECCON_TYPE {
	ECC_DEC_FER = 1,
	ECC_DEC_LOCATE = 2,
	ECC_DEC_CORRECT = 3
};

enum ECC_OPERATION {
	ECC_ENCODE,
	ECC_DECODE
};

struct ecc_config {
	enum ECC_OPERATION op;
	enum ECC_MODE mode;
	enum ECC_DECCON_TYPE deccon;
	u32 addr;
	u32 strength;
	u32 sectors;
	u32 len;
};

/**
 * struct nfc_mode - the definition of each HW working mode
 * @ecc_en:  1: enable HW ECC; 0: disabel HW ECC
 * @dma_en:  1: enable NFI & ECC dma mode; 0: pio mode
 * @irq_en:  1: interrupt mode; 0: polling mode
 */
struct nfc_mode {
	bool ecc_en;
	bool dma_en;
	bool irq_en;
};

/**
 * struct nfc_saved - nfc saved info for suspend / resume
 * @nfi_pagefmt: register value of NFI_PAGEFMT
 * @nfi_seccus_size: register value of NFI_SECCUS_SIZE
 */
struct nfc_saved {
	u32 nfi_pagefmt;
	u32 nfi_seccus_size;
};

struct nfc_info {
	struct nfc_handler handler;
	struct nfc_format format;
	struct nfc_mode mode;
	struct nfc_saved saved;
	u32 version;
	u32 nfi_clk_freq;
	u32 interface_type;
	/* temp buffer for virtual to physical transform */
	u8 *buf;

	/* irq done event */
	void *nfi_done;
	void *ecc_done;
	/* whether enable ahb done irq */
	bool ahb_irq_en;

	/* nfc capacities */
	struct nfc_resource *res;
	u8 max_cs;
	u8 nfi_clk_div;
	bool pg_irq_sel;

	/* ecc capacities */
	u32 decfsm_mask;
	u32 decfsm_idle;

	/* some IPs have individual ecc clk source */
	bool ecc_clk_en;
	const u8 *ecc_strength;
	u8 ecc_strength_num;

	/* ecc configs to enable ecc */
	struct ecc_config ecccfg;
};
/* structure define - */

/* inline function define + */
static inline struct nfc_info *handler_to_info(struct nfc_handler *handler)
{
	return container_of(handler, struct nfc_info, handler);
}

static inline struct nfc_handler *info_to_handler(struct nfc_info *info)
{
	return &info->handler;
}

static inline void nfi_writel(struct nfc_info *info, u32 value, u32 offset)
{
	nwritel(value, info->res->nfi_regs + offset);
}

static inline void nfi_writew(struct nfc_info *info, u16 value, u32 offset)
{
	nwritew(value, info->res->nfi_regs + offset);
}

static inline void nfi_writeb(struct nfc_info *info, u8 value, u32 offset)
{
	nwriteb(value, info->res->nfi_regs + offset);
}

static inline u32 nfi_readl(struct nfc_info *info, u32 offset)
{
	return nreadl(info->res->nfi_regs + offset);
}

static inline u16 nfi_readw(struct nfc_info *info, u32 offset)
{
	return nreadw(info->res->nfi_regs + offset);
}

static inline u8 nfi_readb(struct nfc_info *info, u32 offset)
{
	return nreadb(info->res->nfi_regs + offset);
}

static inline void ecc_writel(struct nfc_info *info, u32 value, u32 offset)
{
	nwritel(value, info->res->ecc_regs + offset);
}

static inline void ecc_writew(struct nfc_info *info, u16 value, u32 offset)
{
	nwritew(value, info->res->ecc_regs + offset);
}

static inline void ecc_writeb(struct nfc_info *info, u8 value, u32 offset)
{
	nwriteb(value, info->res->ecc_regs + offset);
}

static inline u32 ecc_readl(struct nfc_info *info, u32 offset)
{
	return nreadl(info->res->ecc_regs + offset);
}

static inline u16 ecc_readw(struct nfc_info *info, u32 offset)
{
	return nreadw(info->res->ecc_regs + offset);
}

static inline u8 ecc_readb(struct nfc_info *info, u32 offset)
{
	return nreadb(info->res->ecc_regs + offset);
}

static inline u32 ecc_encpar_reg(struct nfc_info *info, u32 index)
{
	u32 reg = 0;

	if (info->res->ver == NANDX_MT8167) {
		if (index < 27)
			reg = 0x10 + index * sizeof(u32);
		else if (index < 35)
			reg = 0x300 + (index - 27) * sizeof(u32);
	}

	return reg;
}

static inline u32 ecc_decel_reg(struct nfc_info *info, u32 index)
{
	u32 reg = 0;

	if (info->res->ver == NANDX_MT8167) {
		if (index < 30)
			reg = 0x128 + index * sizeof(u32);
		else if (index < 39)
			reg = 0x400 + (index - 30) * sizeof(u32);
	}

	return reg;
}

/* inline function define - */

/* randomizer define + */
enum RANDOMIZER_OPERATION {
	RAND_ENCODE,
	RAND_DECODE
};
#define SS_SEED_NUM             128
#define RAND_SEED_SHIFT(op) \
	((op) == RAND_ENCODE ? ENCODE_SEED_SHIFT : DECODE_SEED_SHIFT)
#define RAND_EN(op) \
	((op) == RAND_ENCODE ? RAN_ENCODE_EN : RAN_DECODE_EN)
static u16 ss_randomizer_seed[SS_SEED_NUM] = {
	0x576A, 0x05E8, 0x629D, 0x45A3, 0x649C, 0x4BF0, 0x2342, 0x272E,
	0x7358, 0x4FF3, 0x73EC, 0x5F70, 0x7A60, 0x1AD8, 0x3472, 0x3612,
	0x224F, 0x0454, 0x030E, 0x70A5, 0x7809, 0x2521, 0x484F, 0x5A2D,
	0x492A, 0x043D, 0x7F61, 0x3969, 0x517A, 0x3B42, 0x769D, 0x0647,
	0x7E2A, 0x1383, 0x49D9, 0x07B8, 0x2578, 0x4EEC, 0x4423, 0x352F,
	0x5B22, 0x72B9, 0x367B, 0x24B6, 0x7E8E, 0x2318, 0x6BD0, 0x5519,
	0x1783, 0x18A7, 0x7B6E, 0x7602, 0x4B7F, 0x3648, 0x2C53, 0x6B99,
	0x0C23, 0x67CF, 0x7E0E, 0x4D8C, 0x5079, 0x209D, 0x244A, 0x747B,
	0x350B, 0x0E4D, 0x7004, 0x6AC3, 0x7F3E, 0x21F5, 0x7A15, 0x2379,
	0x1517, 0x1ABA, 0x4E77, 0x15A1, 0x04FA, 0x2D61, 0x253A, 0x1302,
	0x1F63, 0x5AB3, 0x049A, 0x5AE8, 0x1CD7, 0x4A00, 0x30C8, 0x3247,
	0x729C, 0x5034, 0x2B0E, 0x57F2, 0x00E4, 0x575B, 0x6192, 0x38F8,
	0x2F6A, 0x0C14, 0x45FC, 0x41DF, 0x38DA, 0x7AE1, 0x7322, 0x62DF,
	0x5E39, 0x0E64, 0x6D85, 0x5951, 0x5937, 0x6281, 0x33A1, 0x6A32,
	0x3A5A, 0x2BAC, 0x743A, 0x5E74, 0x3B2E, 0x7EC7, 0x4FD2, 0x5D28,
	0x751F, 0x3EF8, 0x39B1, 0x4E49, 0x746B, 0x6EF6, 0x44BE, 0x6DB7
};

/* randomizer define - */

#endif
