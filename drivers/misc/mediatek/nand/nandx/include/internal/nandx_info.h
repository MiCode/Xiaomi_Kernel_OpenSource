/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NANDX_INFO_H__
#define __NANDX_INFO_H__

#include "nandx_util.h"

/* The minimum oob required by user. */
#define MIN_OOB_REQUIRED (32)

enum OPS_MODE_TYPE {
	/* if true slc mode, else normal mode */
	OPS_MODE_SLC,
	/* if true ddr interface, else legacy mode */
	OPS_MODE_DDR,
	/* if true dma mode, else mcu mode */
	OPS_MODE_DMA,
	/* if true irq mode, else polling mode */
	OPS_MODE_IRQ,
	/* if true use ecc, else non-ecc operation */
	OPS_MODE_ECC,
	/* if true open randomizer, else close randomizer */
	OPS_MODE_RANDOMIZE,
	/* if true use calibration for error handle, else none */
	OPS_MODE_CALIBRATION,
	/* if true multi-plane, else non-multi-plane operation */
	OPS_MODE_MULTI_PLANE,
	/* if true use cache, else non-cache mode */
	OPS_MODE_CACHE,
};

enum OPS_MODE_MASK {
	MODE_SLC = (1 << OPS_MODE_SLC),
	MODE_DDR = (1 << OPS_MODE_DDR),
	MODE_DMA = (1 << OPS_MODE_DMA),
	MODE_IRQ = (1 << OPS_MODE_IRQ),
	MODE_ECC = (1 << OPS_MODE_ECC),
	MODE_RANDOMIZE = (1 << OPS_MODE_RANDOMIZE),
	MODE_CALIBRATION = (1 << OPS_MODE_CALIBRATION),
	MODE_MULTI = (1 << OPS_MODE_MULTI_PLANE),
	MODE_CACHE = (1 << OPS_MODE_CACHE),
};

enum IC_VER {
	NANDX_MT8127,
	NANDX_MT8163,
	NANDX_MT8167,
	NANDX_NONE,
};

#define MAX_NFI2X_CLK_NUM	10
#define MAX_ECC_CLK_NUM		5

struct nfc_frequency {
	/* The async clk freq if IP supports async freq */
	u32 freq_async;

	/* NFI 2x clk freq set */
	u32 freq_2x[MAX_NFI2X_CLK_NUM];
	void **nfi_clk_sets;
	int nfi_clk_num;
	/* The selected NFI2X clk. If use async freq, set it as -1. */
	int sel_2x_idx;

	/* ECC clk freq set */
	u32 freq_ecc[MAX_ECC_CLK_NUM];
	void **ecc_clk_sets;
	int ecc_clk_num;
	/* The selected ECC clk. If not use ECC clk, set it as -1. */
	int sel_ecc_idx;
};

/**
 * struct platform_data - nand platform data
 * @ver: IC version
 * @nfi_regs: nfi register base address
 * @ecc_regs: ecc register base address
 * @top_regs: [optional] top layer register for some NFC controller designs
 * @nfi_irq: nfi irq number
 * @ecc_irq: ecc irq number
 * @dev: [optional] platform device pointer
 */

struct nfc_resource {
	enum IC_VER ver;
	void *nfi_regs;
	void *ecc_regs;
	void *top_regs;
	u32 nfi_irq;
	u32 ecc_irq;
	void *dev;
};

struct platform_data {
	struct nfc_resource *res;
	struct nfc_frequency freq;
	void *clk_sets;
};

struct nandx_ops {
	u32 row;
	u32 col;
	u32 len;
	int status;
	void *data;
	void *oob;
};

struct nandx_chip_info {
	u8 plane_num;		/* operation unit plane num */
	u8 wl_page_num;
	u32 block_num;		/* total block num */
	u32 block_size;
	u32 slc_block_size;
	u32 page_size;
	u32 oob_size;		/* fdm size */
	u32 sector_size;
	u32 ecc_strength;	/* calc ecc */
};

bool nandx_get_mode(enum OPS_MODE_TYPE mode);
int nandx_change_mode(enum OPS_MODE_TYPE mode, bool enable);

#endif				/* __NANDX_INFO_H__ */
