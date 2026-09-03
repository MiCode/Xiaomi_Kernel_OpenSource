// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc NAND driver
 *
 * Copyright (C) 2023 Unisoc, Inc.
 * Author: Zhenxiong Lai <zhenxiong.lai@unisoc.com>
 */
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/flashchip.h>
#include <linux/of_device.h>
#include "sprd_nfc.h"

/* feature register */
#define REG_BLOCK_LOCK		0xa0
#define BL_ALL_UNLOCKED		0x00

/* configuration register */
#define REG_CFG			0xb0
#define CFG_OTP_ENABLE		BIT(6)
#define CFG_ECC_ENABLE		BIT(4)
#define CFG_QUAD_ENABLE		BIT(0)

/* status register */
#define REG_STATUS		0xc0
#define STATUS_BUSY		BIT(0)
#define STATUS_ERASE_FAILED	BIT(2)
#define STATUS_PROG_FAILED	BIT(3)
#define STATUS_ECC_MASK		GENMASK(5, 4)
#define STATUS_ECC_NO_BITFLIPS	(0 << 4)
#define STATUS_ECC_HAS_BITFLIPS	(1 << 4)
#define STATUS_ECC_UNCOR_ERROR	(2 << 4)

static struct nand_inst inst_reset = {
	"_inst_reset",
	7, 0, INT_TO | INT_DONE, {
		SNFC_MC_CMD(0xFF),
		SNFC_MC_EXEC(0, 0, 0),
		SNFC_MC_CMD(SNF_GET_FEATURE),
		SNFC_MC_ADDR(0xC0),
		SNFC_MC_READ_STAT(),
		SNFC_MC_EXEC(0, 0, 1),
		SNFC_MC_DONE()
	}
};

static struct nand_inst inst_readid = {
	"_inst_readid",
	6, 0, INT_TO | INT_DONE, {
		SNFC_MC_CMD(SNF_READ_ID),
		SNFC_MC_ADDR(0),
		SNFC_MC_READ_STAT(),
		SNFC_MC_READ_STAT(),
		SNFC_MC_EXEC(0, 0, 0),
		SNFC_MC_DONE()
	}
};

static struct nand_inst inst_get_feature = {
	"_inst_get_feature",
	5, 0, INT_TO | INT_DONE | INT_WP | INT_STSMCH, {
		SNFC_MC_CMD(SNF_GET_FEATURE),
		SNFC_MC_CMD(REG_STATUS),
		SNFC_MC_READ_STAT(),
		SNFC_MC_EXEC(0, 0, 1),
		SNFC_MC_DONE()
	}
};

static struct nand_inst inst_set_feature = {
	"_inst_set_feature",
	5, 0, INT_TO | INT_DONE | INT_WP | INT_STSMCH, {
		SNFC_MC_CMD(SNF_SET_FEATURE),
		SNFC_MC_ADDR(0),
		0,
		SNFC_MC_EXEC(0, 0, 0),
		SNFC_MC_DONE()
	}
};

static u8 g_cfg_cache[CFG0_CS_MAX];

static int sprd_snfc_get_feature(struct sprd_nfc *host, u8 faddr, u8 *fdata);

static void sprd_nfc_cmd_tag2(struct nand_inst *cmd_inst)
{
	cmd_inst->step_tag2 = cmd_inst->cnt;
}

static void sprd_nfc_cmd_tag3(struct nand_inst *cmd_inst)
{
	cmd_inst->step_tag3 = cmd_inst->cnt;
}

static void sprd_snfc_cmd_change(struct sprd_nfc *host,
	struct nand_inst *cmd_inst, u32 pg)
{
	int plane_sel = host->param.plane_sel_bit;

	cmd_inst->inst[cmd_inst->step_tag] =
		SNFC_MC_ADDR(SPRD_NAND_PAGE_SHIFT_16(pg));
	cmd_inst->inst[cmd_inst->step_tag + 1] =
		SNFC_MC_ADDR(SPRD_NAND_PAGE_SHIFT_8(pg));
	cmd_inst->inst[cmd_inst->step_tag + 2] =
		SNFC_MC_ADDR(SPRD_NAND_PAGE_SHIFT_0(pg));
	if (plane_sel != -1 &&
		(cmd_inst->inst[cmd_inst->step_tag + 2] & BIT(host->blkshift - host->pageshift))) {
		if (plane_sel > host->pageshift && plane_sel < 16) {
			if (cmd_inst->step_tag2)
				cmd_inst->inst[cmd_inst->step_tag2] |= BIT(plane_sel - 8);
			if (cmd_inst->step_tag3)
				cmd_inst->inst[cmd_inst->step_tag3] |= BIT(plane_sel - 8);
		} else {
			dev_err(host->dev, "plane_sel is in position of page address\n");
			WARN_ON(1);
		}
	}
}

static void sprd_snfc_init_reg_state0(struct sprd_nfc *host)
{
	host->nfc_cfg1 &= ~(CFG1_INTF_TYPE(7));
	host->nfc_cfg1 |= CFG1_INTF_TYPE(host->intf_type);
	sprd_nfc_writel(host, host->nfc_cfg1, NFC_CFG1_REG);

	host->nfc_cfg0 = CFG0_DEF0_MAST_ENDIAN | CFG0_DEF0_SECT_NUM_IN_INST;
	host->nfc_cfg3 = CFG3_DETECT_ALL_FF;
	host->nfc_cfg4 = 0x38 | CFG4_SLICE_CLK_EN;
	sprd_nfc_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	sprd_nfc_writel(host, host->nfc_cfg3, NFC_CFG3_REG);
	sprd_nfc_writel(host, host->nfc_cfg4, NFC_CFG4_REG);

	sprd_nfc_writel(host, 0x1004, NFC_POLY0_REG);
	sprd_nfc_writel(host, 0x1004, NFC_POLY1_REG);
	sprd_nfc_writel(host, 0x4013, NFC_POLY2_REG);
	sprd_nfc_writel(host, 0x400010, NFC_POLY3_REG);
	sprd_nfc_writel(host, 0x6, NFC_PHY_CFG);
	sprd_nfc_writel(host, 0x100, NFC_DLL0_CFG);
	sprd_nfc_writel(host, 0x100, NFC_DLL1_CFG);
	sprd_nfc_writel(host, 0xbf, NFC_DLL2_CFG);
	sprd_nfc_writel(host, 0x40002, NFC_DLL_REG);

	host->snfc_com_cfg = 0x680;
	host->snfc_clk_cfg = 0x00000680;
	host->snfc_rd_smp_ctl = 0x0000548a;
	sprd_nfc_writel(host, host->snfc_com_cfg, SNFC_COM_CFG);
	sprd_nfc_writel(host, host->snfc_clk_cfg, SNFC_CLK_CFG);
	sprd_nfc_writel(host, host->snfc_rd_smp_ctl, SNFC_RD_SMP_CTL);

	sprd_nfc_writel(host, RAM_MAIN_ADDRH(-1), NFC_MAIN_ADDRH_REG);
	sprd_nfc_writel(host, RAM_MAIN_ADDRL(-1), NFC_MAIN_ADDRL_REG);
	sprd_nfc_writel(host, RAM_SPAR_ADDRH(-1), NFC_SPAR_ADDRH_REG);
	sprd_nfc_writel(host, RAM_SPAR_ADDRL(-1), NFC_SPAR_ADDRL_REG);
	sprd_nfc_writel(host, RAM_STAT_ADDRH(-1), NFC_STAT_ADDRH_REG);
	sprd_nfc_writel(host, RAM_STAT_ADDRL(-1), NFC_STAT_ADDRL_REG);
}

static void sprd_snfc_prepare_before_exec(struct sprd_nfc *host)
{
	sprd_nfc_prepare_before_exec(host);

	/* for rf cs */
	sprd_nfc_writel(host, host->snfc_com_cfg, SNFC_COM_CFG);
}

static void sprd_snfc_select_cs(struct sprd_nfc *host, int cs)
{
	host->snfc_com_cfg = (host->snfc_com_cfg & SNFC_COM_CFG_CS_MSKCLR) |
						    SNFC_COM_CFG_SET_CS_SEL(cs);
	host->cur_cs = cs;
}

static int sprd_snfc_reset(struct sprd_nfc *host)
{
	struct nand_inst *inst = &inst_reset;

	host->nfc_sts_mach_val = DEF0_MATCH;
	host->nfc_time0_val = host->nfc_time0_r;
	host->nfc_cfg0_val = host->nfc_cfg0 | CFG0_SET_NFC_MODE(ONLY_NAND_MODE);
	sprd_nfc_cmd_exec(host, inst, 1, 0);

	return sprd_nfc_cmd_wait(host, inst);
}

static int sprd_snfc_readid(struct sprd_nfc *host, u32 *id1, u32 *id2)
{
	struct nand_inst *inst = &inst_readid;
	int ret;
	u32 id;

	host->nfc_sts_mach_val = DEF0_MATCH;
	host->nfc_cfg0_val = host->nfc_cfg0 | CFG0_SET_NFC_MODE(ONLY_NAND_MODE);
	host->nfc_time0_val = NFC_READID_TIMING;
	sprd_nfc_cmd_exec(host, inst, 1, 0);
	ret = sprd_nfc_cmd_wait(host, inst);
	if (ret != 0)
		return ret;

	id = sprd_nfc_readl(host, SNFC_FEATURE);
	*(id1) = (id & 0xFFFF) | (0x534E << 16);
	*(id2) = 0;

	return 0;
}

static void sprd_snfc_delect_cs(struct sprd_nfc *host, int ret)
{
	if (ret == -EIO) {
		sprd_nfc_writel(host, CTRL_NFC_CMD_CLR, NFC_START_REG);
		sprd_snfc_reset(host);
	}
	host->cur_cs = -1;
}

static int sprd_snfc_erase(struct sprd_nfc *host, u32 page)
{
	struct nand_inst inst_erase_blk = host->inst_erase;
	struct nand_inst *inst = &inst_erase_blk;
	u8 status = 0;
	int ret;

	host->int_ops->select_cs(host, GETCS(page));
	host->int_ops->set_wp(host, false);

	host->nfc_sts_mach_val = MACH_ERASE;
	host->nfc_time0_val = host->nfc_time0_e;
	host->nfc_cfg0_val = (host->nfc_cfg0 | CFG0_SET_NFC_MODE(ONLY_NAND_MODE));

	host->int_ops->cmd_change(host, inst, page);
	sprd_nfc_cmd_exec(host, inst, 1, 0);
	ret = sprd_nfc_cmd_wait(host, inst);
	if (ret)
		goto out;

	ret = sprd_snfc_get_feature(host, REG_STATUS, &status);
	if (!ret) {
		if (status & BIT(0)) {
			dev_err(host->dev, "erase timeout, status:%x\n", status);
			ret = -ETIMEDOUT;
		} else if (status & BIT(2)) {
			dev_err(host->dev, "erase fail, status:%x\n", status);
			ret = -EFAULT;
		}
	}

out:
	host->int_ops->set_wp(host, true);
	host->int_ops->delect_cs(host, ret);

	return ret;
}

static void sprd_snfc_param_init_inst(struct sprd_nfc *host)
{
	u32 column = BIT(host->pageshift);

	dev_dbg(host->dev,
			"nand:param init inst column %d,host->pageshift %d\n",
			column, host->pageshift);
	if (host->param.nbus_width == BW_16) {
		dev_dbg(host->dev, "nand:buswidth = 16\n");
		column >>= 1;
	}
	/* erase */
	host->inst_erase.program_name = "_inst_erase";
	sprd_nfc_cmd_init(&host->inst_erase,
						 INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nfc_cmd_add(&host->inst_erase, SNFC_MC_CMD(SNF_WRTE_EN));
	sprd_nfc_cmd_add(&host->inst_erase, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_erase, SNFC_MC_CMD(SNF_ERASE));
	sprd_nfc_cmd_tag(&host->inst_erase);
	sprd_nfc_cmd_add(&host->inst_erase, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_erase, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_erase, SNFC_MC_ADDR(0));

	sprd_nfc_cmd_add(&host->inst_erase, SNFC_MC_EXEC(0, 0, 0));
	sprd_nfc_cmd_add(&host->inst_erase, SNFC_MC_DONE());

	/* read main+spare(info)+Ecc or Raw */
	host->inst_read_main_spare.program_name = "_inst_read_main_spare";
	sprd_nfc_cmd_init(&host->inst_read_main_spare, INT_TO | INT_DONE);
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_CMD(SNF_READ_TO_CACHE));
	sprd_nfc_cmd_tag(&host->inst_read_main_spare);
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_CMD(SNF_GET_FEATURE));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_ADDR(0xC0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_READ_STAT());
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_EXEC(0, 0, 1));

	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_CMD(SNF_READ_FR_CACHE_X4));
	sprd_nfc_cmd_tag2(&host->inst_read_main_spare);
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_ADDR((column >> 8) & 0xFF));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_ADDR(column & 0xFF));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_DUMMY_ADDR());
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_SRDT(BIT_MODE_4));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_CMD(SNF_READ_FR_CACHE_X4));
	sprd_nfc_cmd_tag3(&host->inst_read_main_spare);
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_DUMMY_ADDR());
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_MRDT(BIT_MODE_4));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_EXEC(0, 0, 0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, SNFC_MC_DONE());

	/* read main raw */
	host->inst_read_main_raw.program_name = "_inst_read_main_raw";
	sprd_nfc_cmd_init(&host->inst_read_main_raw, INT_TO | INT_DONE);

	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_CMD(SNF_READ_TO_CACHE));
	sprd_nfc_cmd_tag(&host->inst_read_main_raw);
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_CMD(SNF_GET_FEATURE));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_ADDR(0xC0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_READ_STAT());
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_EXEC(0, 0, 1));

	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_CMD(SNF_READ_FR_CACHE_X4));
	sprd_nfc_cmd_tag2(&host->inst_read_main_raw);
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_DUMMY_ADDR());
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_MRDT(BIT_MODE_4));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_EXEC(0, 0, 0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, SNFC_MC_DONE());

	/*
	 * read spare raw: read only main or only spare data,
	 * it is read to main addr.
	 */
	host->inst_read_spare_raw.program_name = "_inst_read_spare_raw";
	sprd_nfc_cmd_init(&host->inst_read_spare_raw, INT_TO | INT_DONE);

	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_CMD(SNF_READ_TO_CACHE));
	sprd_nfc_cmd_tag(&host->inst_read_spare_raw);
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_CMD(SNF_GET_FEATURE));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_ADDR(0xC0));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_READ_STAT());
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_EXEC(0, 0, 1));

	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_CMD(SNF_READ_FR_CACHE_X4));
	sprd_nfc_cmd_tag2(&host->inst_read_spare_raw);
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_ADDR((column >> 8) & 0xFF));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_ADDR(column & 0xFF));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_DUMMY_ADDR());
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_MRDT(BIT_MODE_4));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_read_spare_raw, SNFC_MC_DONE());

	/* write main+spare(info)+ecc */
	host->inst_write_main_spare.program_name = "_inst_write_main_spare";
	sprd_nfc_cmd_init(&host->inst_write_main_spare,
						 INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_CMD(SNF_WRTE_EN));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_CMD(SNF_PROGRAM_X4));
	sprd_nfc_cmd_tag2(&host->inst_write_main_spare);
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_MWDT(BIT_MODE_4));

	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_SWDT(BIT_MODE_4));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_CMD(SNF_PROGRAM_EXEC));
	sprd_nfc_cmd_tag(&host->inst_write_main_spare);
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_CMD(SNF_GET_FEATURE));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_ADDR(0xC0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_READ_STAT());
	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_EXEC(1, 0, 1));

	sprd_nfc_cmd_add(&host->inst_write_main_spare, SNFC_MC_DONE());

	/* write main raw */
	host->inst_write_main_raw.program_name = "_inst_write_main_raw";
	sprd_nfc_cmd_init(&host->inst_write_main_raw,
						 INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_CMD(SNF_WRTE_EN));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_CMD(SNF_PROGRAM_X4));
	sprd_nfc_cmd_tag2(&host->inst_write_main_raw);
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_MWDT(BIT_MODE_4));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_CMD(SNF_PROGRAM_EXEC));
	sprd_nfc_cmd_tag(&host->inst_write_main_raw);
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_CMD(SNF_GET_FEATURE));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_ADDR(0xC0));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_READ_STAT());
	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_EXEC(1, 0, 1));

	sprd_nfc_cmd_add(&host->inst_write_main_raw, SNFC_MC_DONE());

	/* write spare raw */
	host->inst_write_spare_raw.program_name = "_inst_write_spare_raw";
	sprd_nfc_cmd_init(&host->inst_write_spare_raw,
						 INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_CMD(SNF_WRTE_EN));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_CMD(SNF_PROGRAM_X4));
	sprd_nfc_cmd_tag2(&host->inst_write_spare_raw);
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_ADDR((column >> 8) & 0xFF));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_ADDR(column & 0xFF));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_MWDT(BIT_MODE_4));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_CMD(SNF_PROGRAM_EXEC));
	sprd_nfc_cmd_tag(&host->inst_write_spare_raw);
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_ADDR(0));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_EXEC(0, 0, 0));

	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_CMD(SNF_GET_FEATURE));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_ADDR(0xC0));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_READ_STAT());
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_EXEC(1, 0, 1));

	sprd_nfc_cmd_add(&host->inst_write_spare_raw, SNFC_MC_DONE());
}

static int sprd_snfc_set_feature(struct sprd_nfc *host, u8 faddr, u8 fdata)
{
	struct nand_inst *inst = &inst_set_feature;

	host->nfc_sts_mach_val = DEF0_MATCH;
	host->nfc_cfg0_val = host->nfc_cfg0 | CFG0_SET_NFC_MODE(ONLY_NAND_MODE);
	inst->inst[1] = SNFC_MC_ADDR(faddr);
	inst->inst[2] = SNFC_MC_ADDR(fdata);
	sprd_nfc_cmd_exec(host, inst, 1, 0);

	return sprd_nfc_cmd_wait(host, inst);
}

static int sprd_snfc_get_feature(struct sprd_nfc *host, u8 faddr, u8 *fdata)
{
	struct nand_inst *inst = &inst_get_feature;
	u32 sdata;
	int ret;

	host->nfc_sts_mach_val = DEF0_MATCH;
	host->nfc_cfg0_val = host->nfc_cfg0 | CFG0_SET_NFC_MODE(ONLY_NAND_MODE);
	inst->inst[1] = SNFC_MC_ADDR(faddr);
	if (faddr == REG_STATUS)
		inst->inst[3] = SNFC_MC_EXEC(0, 0, 1);
	else
		inst->inst[3] = SNFC_MC_EXEC(0, 0, 0);
	sprd_nfc_cmd_exec(host, inst, 1, 0);

	ret = sprd_nfc_cmd_wait(host, inst);
	if (ret != 0)
		return ret;

	sdata = sprd_nfc_readl(host, SNFC_FEATURE);
	*fdata = (u8)(sdata & 0xFF);

	return 0;
}

static int sprd_snfc_get_cfg(struct sprd_nfc *host, u8 *cfg)
{
	if (WARN_ON(host->cur_cs < 0 ||
				host->cur_cs >= ARRAY_SIZE(host->cs)))
		return -EINVAL;

	*cfg = g_cfg_cache[host->cur_cs];

	return 0;
}

static int sprd_snfc_set_cfg(struct sprd_nfc *host, u8 cfg)
{
	int ret;

	if (WARN_ON(host->cur_cs < 0 ||
				host->cur_cs >= ARRAY_SIZE(host->cs)))
		return -EINVAL;

	if (g_cfg_cache[host->cur_cs] == cfg)
		return 0;

	ret = sprd_snfc_set_feature(host, REG_CFG, cfg);
	if (ret)
		return ret;

	g_cfg_cache[host->cur_cs] = cfg;

	return 0;
}

static int sprd_snfc_upd_cfg(struct sprd_nfc *host, u8 mask, u8 val)
{
	int ret;
	u8 cfg;

	ret = sprd_snfc_get_cfg(host, &cfg);
	if (ret)
		return ret;

	cfg &= ~mask;
	cfg |= val;

	return sprd_snfc_set_cfg(host, cfg);
}

static int sprd_snfc_init_cfg_cache(struct sprd_nfc *host, int cs)
{
	return sprd_snfc_get_feature(host, REG_CFG, &g_cfg_cache[cs]);
}

void sprd_snfc_attach(struct sprd_nfc *host)
{
	host->cs_max = SNFC_CS_MAX;

	/* override */
	host->int_ops->init_reg_state0 = sprd_snfc_init_reg_state0;
	host->int_ops->prepare_before_exec = sprd_snfc_prepare_before_exec;
	host->int_ops->select_cs = sprd_snfc_select_cs;
	host->int_ops->delect_cs = sprd_snfc_delect_cs;
	host->int_ops->reset = sprd_snfc_reset;
	host->int_ops->readid = sprd_snfc_readid;
	host->int_ops->init_inst = sprd_snfc_param_init_inst;
	host->int_ops->cmd_change = sprd_snfc_cmd_change;

	host->ops->erase = sprd_snfc_erase;
}
EXPORT_SYMBOL(sprd_snfc_attach);

int sprd_snfc_setup_host(struct sprd_nfc *host)
{
	int ret = 0;
	int i;

	for (i = 0; i < host->cs_max; i++) {
		if (i > 0 && !host->cs[i])
			continue;

		host->int_ops->select_cs(host, i);

		ret = sprd_snfc_init_cfg_cache(host, i);
		if (ret) {
			dev_err(host->dev, "init config cache[%d] failed\n", i);
			break;
		}
		/* ecc disable */
		ret = sprd_snfc_upd_cfg(host, CFG_ECC_ENABLE, 0);
		if (ret) {
			dev_err(host->dev, "disable internal ecc failed\n");
			break;
		}
		/* enable Qual SPI */
		ret = sprd_snfc_upd_cfg(host, CFG_QUAD_ENABLE, 1);
		if (ret) {
			dev_err(host->dev, "enable qual spi failed\n");
			break;
		}

		/* disable OTP */
		ret = sprd_snfc_upd_cfg(host, CFG_OTP_ENABLE, 0);
		if (ret) {
			dev_err(host->dev, "disable OTP failed\n");
			break;
		}

		/* unlock block */
		ret = sprd_snfc_set_feature(host, REG_BLOCK_LOCK, BL_ALL_UNLOCKED);
		if (ret) {
			dev_err(host->dev, "block unlock failed\n");
			break;
		}

		host->int_ops->delect_cs(host, ret);
	}

	return ret;
}
EXPORT_SYMBOL(sprd_snfc_setup_host);
