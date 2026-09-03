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
#include "sprd_nand_param.h"
#include "sprd_nfc.h"

struct nand_ecc_stats {
	u16 ecc_stats[16];
	u32 layout4_ecc_stats;
	u32 freecount[5];
};

static struct nand_inst inst_reset = {
	"_inst_reset",
	3, 0, INT_TO | INT_DONE, {
		INST_CMD(0xFF),
		INST_WRB0(),
		INST_DONE()
	}
};

static struct nand_inst inst_readid = {
	"_inst_readid",
	5, 0, INT_TO | INT_DONE, {
		INST_CMD(0x90),
		INST_ADDR(0, 0),
		INST_INOP(10),
		INST_IDST(0x08),
		INST_DONE()
	}
};

static struct sprd_nand_timing default_timing = {10, 25, 15};

static const u32 seedtbl[64] = {
	0x056c, 0x1bc77, 0x5d356, 0x1f645d, 0x0fbc, 0x0090c, 0x7f880, 0x3d9e86,
	0x1717, 0x1e1ad, 0x6db67, 0x7d7ea0, 0x0a52, 0x0d564, 0x6fbac, 0x6823dd,
	0x07cf, 0x1cb3b, 0x37cd1, 0x5c91f0, 0x064e, 0x167a7, 0x0f1d2, 0x506be8,
	0x098c, 0x1bd54, 0x2c2af, 0x4b5fb7, 0x1399, 0x11690, 0x1d310, 0x27e53b,
	0x1246, 0x14794, 0x0f34f, 0x347bc4, 0x0150, 0x00787, 0x73450, 0x3d8927,
	0x11f1, 0x17bad, 0x46eaa, 0x5403f5, 0x1026, 0x173ab, 0x79634, 0x01b987,
	0x1c45, 0x08b63, 0x42924, 0x4bf708, 0x012a, 0x03a3a, 0x435d5, 0x1a7baa,
	0x0849, 0x1cb9b, 0x28350, 0x1e8309, 0x1d4c, 0x0af6e, 0x0949e, 0x00193a,
};

static const s8 bit_num8[256] = {
	8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4, 7, 6, 6, 5, 6, 5, 5, 4,
	6, 5, 5, 4, 5, 4, 4, 3, 7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2, 7, 6, 6, 5, 6, 5, 5, 4,
	6, 5, 5, 4, 5, 4, 4, 3, 6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2, 5, 4, 4, 3, 4, 3, 3, 2,
	4, 3, 3, 2, 3, 2, 2, 1, 7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2, 6, 5, 5, 4, 5, 4, 4, 3,
	5, 4, 4, 3, 4, 3, 3, 2, 5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2, 5, 4, 4, 3, 4, 3, 3, 2,
	4, 3, 3, 2, 3, 2, 2, 1, 5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
	4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0
};

void sprd_nfc_cmd_init(struct nand_inst *cmd_inst, u32 position)
{
	cmd_inst->int_bits = position;
}
EXPORT_SYMBOL(sprd_nfc_cmd_init);

void sprd_nfc_cmd_add(struct nand_inst *cmd_inst, u16 inst_p)
{
	cmd_inst->inst[cmd_inst->cnt] = inst_p;
	cmd_inst->cnt++;
}
EXPORT_SYMBOL(sprd_nfc_cmd_add);

void sprd_nfc_cmd_tag(struct nand_inst *cmd_inst)
{
	cmd_inst->step_tag = cmd_inst->cnt;
}
EXPORT_SYMBOL(sprd_nfc_cmd_tag);

static void sprd_nfc_cmd_change(struct sprd_nfc *host,
	struct nand_inst *cmd_inst, u32 pg)
{
	cmd_inst->inst[cmd_inst->step_tag] =
	    INST_ADDR(SPRD_NAND_PAGE_SHIFT_0(pg), 1);
	cmd_inst->inst[cmd_inst->step_tag + 1] =
	    INST_ADDR(SPRD_NAND_PAGE_SHIFT_8(pg), 0);
	if (host->param.ncycle == 5)
		cmd_inst->inst[cmd_inst->step_tag + 2] =
		       INST_ADDR(SPRD_NAND_PAGE_SHIFT_16(pg), 0);
}

void sprd_nfc_cmd_exec(struct sprd_nfc *host, struct nand_inst *program, u32 repeat,
	u32 if_use_int)
{
	u32 i;

	host->nfc_cfg0_val |= CFG0_SET_REPEAT_NUM(repeat);

	host->int_ops->prepare_before_exec(host);

	for (i = 0; i < program->cnt; i += 2) {
		sprd_nfc_writel(host, (program->inst[i] | (program->inst[i + 1] << 16)),
			       NFC_INST00_REG + (i << 1));
	}

	sprd_nfc_writel(host, 0, NFC_INT_REG);
	sprd_nfc_writel(host, GENMASK(15, 8), NFC_INT_REG);
	if (if_use_int)
		sprd_nfc_writel(host, (program->int_bits & 0xF), NFC_INT_REG);

#ifdef CONFIG_MTD_NAND_SPRD_DEBUG
	sprd_nfc_dbg_sav_exec(host, DBG_SAV_STOP);
#endif

	sprd_nfc_writel(host, host->nfc_start | CTRL_NFC_CMD_START, NFC_START_REG);
}
EXPORT_SYMBOL(sprd_nfc_cmd_exec);

int sprd_nfc_cmd_wait(struct sprd_nfc *host, struct nand_inst *program)
{
	int ret = 0;
	u32 nfc_timeout_val, regval = 0;

	if (strcmp(program->program_name, "_inst_reset") == 0)
		nfc_timeout_val = NFC_RESET_TIMEOUT_VAL;
	else
		nfc_timeout_val = NFC_TIMEOUT_VAL;

	ret = readl_relaxed_poll_timeout(host->ioaddr + NFC_INT_REG, regval,
		       (SPRD_NAND_GET_INT_VAL(regval) & ((INT_TO | INT_STSMCH | INT_WP)
							  & program->int_bits))
		       || (SPRD_NAND_GET_INT_VAL(regval) & (INT_DONE & program->int_bits)),
				0, nfc_timeout_val);

#ifdef CONFIG_MTD_NAND_SPRD_DEBUG
	sprd_nfc_dbg_sav_exec(host, DBG_SAV_START);
#endif

	if (ret) {
		dev_err(host->dev, "cmd %s wait timeout.\n", program->program_name);
		goto out;
	}

	if (SPRD_NAND_GET_INT_VAL(regval) & ((INT_TO | INT_STSMCH | INT_WP)
			       & program->int_bits)) {
		ret = -EIO;
#ifdef CONFIG_MTD_NAND_SPRD_DEBUG
		sprd_nfc_dbg_sav_last_err(host);
#endif
	}

out:
	return ret;
}
EXPORT_SYMBOL(sprd_nfc_cmd_wait);

static void sprd_nfc_pageseed(struct sprd_nfc *host, u32 page)
{
	u32 i, j, remain, shift, mask, numbit;
	u32 offset = page >> 4;

	if (page & ~SPRD_NAND_PAGE_MASK)
		return;

	memset(host->seedbuf_v, 0, SEED_BUF_SIZE * 4);
	for (i = 0; i < SEED_TBL_SIZE; i++) {
		switch (i & 0x3) {
		case 0:
			numbit = 13;
			break;
		case 1:
			numbit = 17;
			break;
		case 2:
			numbit = 19;
			break;
		case 3:
			numbit = 23;
			break;
		}
		for (j = 0; j <= numbit - 1; j++) {
			if (seedtbl[i] & BIT(j))
				host->seedbuf_v[i] |= BIT(numbit - 1 - j);
		}
		if (offset) {
			if (offset > numbit - 1)
				shift = offset - numbit;
			else
				shift = offset;
			mask = (BIT(numbit) - 1) >> shift;
			remain = host->seedbuf_v[i] & ~mask;
			remain >>= numbit - shift;
			host->seedbuf_v[i] &= mask;
			host->seedbuf_v[i] <<= shift;
			host->seedbuf_v[i] |= remain;
		}
	}
	host->seedbuf_v[SEED_TBL_SIZE] = host->seedbuf_v[0];
	host->seedbuf_v[SEED_TBL_SIZE + 1] = host->seedbuf_v[1];
	host->seedbuf_v[SEED_TBL_SIZE + 2] = host->seedbuf_v[2];
	host->seedbuf_v[SEED_TBL_SIZE + 3] = host->seedbuf_v[3];
}

static void sprd_nfc_set_randomize(struct sprd_nfc *host, u32 page, bool en)
{
	u32 *seedaddr = host->seedbuf_p + PAGE2SEED_ADDR_OFFSET(page);

	if (en) {
		sprd_nfc_pageseed(host, page);

		sprd_nfc_writel(host, NFC_POLYNOMIALS0, NFC_POLY0_REG);
		sprd_nfc_writel(host, NFC_POLYNOMIALS1, NFC_POLY1_REG);
		sprd_nfc_writel(host, NFC_POLYNOMIALS2, NFC_POLY2_REG);
		sprd_nfc_writel(host, NFC_POLYNOMIALS3, NFC_POLY3_REG);
		sprd_nfc_writel(host, NFC_POLYNOMIALS0, NFC_POLY4_REG);
		sprd_nfc_writel(host, NFC_POLYNOMIALS1, NFC_POLY5_REG);
		sprd_nfc_writel(host, NFC_POLYNOMIALS2, NFC_POLY6_REG);
		sprd_nfc_writel(host, NFC_POLYNOMIALS3, NFC_POLY7_REG);
		sprd_nfc_writel(host, RAM_SEED_ADDRH(seedaddr), NFC_SEED_ADDRH_REG);
		sprd_nfc_writel(host, RAM_SEED_ADDRL(seedaddr), NFC_SEED_ADDRL_REG);
		host->nfc_cfg3 |= CFG3_RANDOM_EN | CFG3_POLY_4R1_EN;
		sprd_nfc_writel(host, host->nfc_cfg3, NFC_CFG3_REG);
	} else {
		host->nfc_cfg3 &= ~(CFG3_POLY_4R1_EN | CFG3_RANDOM_EN);
		sprd_nfc_writel(host, host->nfc_cfg3, NFC_CFG3_REG);
	}
}

/* host state0 is used for nand id and reset cmd */
static void sprd_nfc_init_reg_state0(struct sprd_nfc *host)
{
	host->nfc_cfg1 &= ~(CFG1_INTF_TYPE(7));
	host->nfc_cfg1 |= CFG1_INTF_TYPE(host->intf_type);
	sprd_nfc_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
	sprd_nfc_writel(host, 0, NFC_DLL0_CFG);
	sprd_nfc_writel(host, 0, NFC_DLL1_CFG);
	sprd_nfc_writel(host, 0, NFC_DLL2_CFG);

	host->nfc_cfg0 = CFG0_DEF0_MAST_ENDIAN | CFG0_DEF0_SECT_NUM_IN_INST;
	host->nfc_cfg3 = CFG3_DETECT_ALL_FF;
	host->nfc_cfg4 = CFG4_SLICE_CLK_EN | CFG4_PHY_DLL_CLK_2X_EN;
	sprd_nfc_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	sprd_nfc_writel(host, host->nfc_cfg3, NFC_CFG3_REG);
	sprd_nfc_writel(host, host->nfc_cfg4, NFC_CFG4_REG);

	sprd_nfc_writel(host, RAM_MAIN_ADDRH(-1), NFC_MAIN_ADDRH_REG);
	sprd_nfc_writel(host, RAM_MAIN_ADDRL(-1), NFC_MAIN_ADDRL_REG);
	sprd_nfc_writel(host, RAM_SPAR_ADDRH(-1), NFC_SPAR_ADDRH_REG);
	sprd_nfc_writel(host, RAM_SPAR_ADDRL(-1), NFC_SPAR_ADDRL_REG);
	sprd_nfc_writel(host, RAM_STAT_ADDRH(-1), NFC_STAT_ADDRH_REG);
	sprd_nfc_writel(host, RAM_STAT_ADDRL(-1), NFC_STAT_ADDRL_REG);
}

void sprd_nfc_prepare_before_exec(struct sprd_nfc *host)
{
	sprd_nfc_writel(host, host->nfc_cfg0_val, NFC_CFG0_REG);
	sprd_nfc_writel(host, host->nfc_cfg4, NFC_CFG4_REG);
	sprd_nfc_writel(host, host->nfc_time0_val, NFC_TIMING0_REG);
	sprd_nfc_writel(host, host->nfc_sts_mach_val, NFC_STAT_STSMCH_REG);
}
EXPORT_SYMBOL(sprd_nfc_prepare_before_exec);

static void sprd_nfc_select_cs(struct sprd_nfc *host, int cs)
{
	host->nfc_cfg0 = (host->nfc_cfg0 & CFG0_CS_MSKCLR) |
	       CFG0_SET_CS_SEL(cs);
	host->cur_cs = cs;
}

static int sprd_nfc_reset(struct sprd_nfc *host)
{
	struct nand_inst *inst = &inst_reset;

	host->nfc_sts_mach_val = DEF0_MATCH;
	host->nfc_time0_val = host->nfc_time0_r;
	host->nfc_cfg0_val = host->nfc_cfg0 | CFG0_SET_NFC_MODE(ONLY_NAND_MODE);
	sprd_nfc_cmd_exec(host, inst, 1, 0);

	return sprd_nfc_cmd_wait(host, inst);
}

static int sprd_nfc_readid(struct sprd_nfc *host, u32 *id0, u32 *id1)
{
	struct nand_inst *inst = &inst_readid;
	int ret;

	host->nfc_sts_mach_val = DEF0_MATCH;
	host->nfc_cfg0_val = host->nfc_cfg0 | CFG0_SET_NFC_MODE(ONLY_NAND_MODE);
	host->nfc_time0_val = NFC_READID_TIMING;
	sprd_nfc_cmd_exec(host, inst, 1, 0);
	ret = sprd_nfc_cmd_wait(host, inst);
	if (ret != 0)
		return ret;

	*id0 = sprd_nfc_readl(host, NFC_STATUS0_REG);
	*id1 = sprd_nfc_readl(host, NFC_STATUS1_REG);

	return ret;
}

static void sprd_nfc_delect_cs(struct sprd_nfc *host, int ret)
{
	if (ret == -EIO) {
		sprd_nfc_writel(host, CTRL_NFC_CMD_CLR, NFC_START_REG);
		sprd_nfc_reset(host);
	}

	host->cur_cs = -1;
}

static void sprd_nfc_set_timing_config(struct sprd_nfc *host,
	struct sprd_nand_timing *timing, u32 clk_hz)
{
	u32 temp_val, clk_mhz, reg_val = 0;

	/* the clock source is 2x clock */
	clk_mhz = clk_hz / 2000000;
	/* get acs value : 0ns */
	reg_val |= ((2 & 0x1F) << NFC_ACS_OFFSET);

	/* temp_val: 0: 1clock, 1: 2clocks... */
	temp_val = timing->ace_ns * clk_mhz / 1000 - 1;
	if (((timing->ace_ns * clk_mhz) % 1000) != 0)
		temp_val++;

	reg_val |= ((temp_val & 0x1F) << NFC_ACE_OFFSET);

	/* get rws value : 20 ns */
	temp_val = 20 * clk_mhz / 1000 - 1;
	reg_val |= ((temp_val & 0x3F) << NFC_RWS_OFFSET);

	/* get rws value : 0 ns */
	reg_val |= ((2 & 0x1F) << NFC_RWE_OFFSET);

	/*
	 * get rwh value,if the remainder bigger than 500, then plus 1 for more
	 * accurate
	 */
	temp_val = timing->rwh_ns * clk_mhz / 1000 - 1;
	if (((timing->rwh_ns * clk_mhz) % 1000) >= 500)
		temp_val++;

	reg_val |= ((temp_val & 0x1F) << NFC_RWH_OFFSET);

	/*
	 * get rwl value,if the remainder bigger than 500, then plus 1 for more
	 * accurate
	 */
	temp_val = timing->rwl_ns * clk_mhz / 1000 - 1;
	if (((timing->rwl_ns * clk_mhz) % 1000) >= 500)
		temp_val++;

	reg_val |= (temp_val & 0x3F);

	dev_dbg(host->dev, "%s nand timing val: 0x%x\n\r", __func__, reg_val);

	host->nfc_time0_r = reg_val;
	host->nfc_time0_w = reg_val;
	host->nfc_time0_e = reg_val;
}

static int sprd_nfc_param_init_nandhw(struct sprd_nfc *host,
	struct sprd_nand_param *param)
{
	sprd_nfc_set_timing_config(host, &param->s_timing, host->frequency);
	host->param.nblkcnt = param->blk_num;
	host->param.npage_per_blk = param->blk_size / param->page_size;
	host->param.nsect_per_page = param->page_size / param->nsect_size;
	host->param.nsect_size = param->nsect_size;
	host->param.nspare_size = param->s_oob.oob_size;
	host->param.page_size = param->page_size; /* no contain spare */
	host->param.main_size = param->page_size;
	host->param.spare_size = param->nspare_size;
	host->param.ecc_mode = param->s_oob.ecc_bits;
	host->param.ecc_pos = param->s_oob.ecc_pos;
	host->param.ecc_size = param->s_oob.ecc_size;
	host->param.info_pos = param->s_oob.info_pos;
	host->param.info_size = param->s_oob.info_size;
	host->param.badflag_pos = 0; /* --- default:need fixed */
	host->param.badflag_len = 2; /* --- default:need fixed */

	host->param.nbus_width = param->nbus_width;
	host->param.ncycle = param->ncycles;

	dev_dbg(host->dev, "nand:nBlkcnt = [%d], npageperblk = [%d]\n",
			host->param.nblkcnt, host->param.npage_per_blk);
	dev_dbg(host->dev, "nand:nsectperpage = [%d], nsecsize = [%d]\n",
			host->param.nsect_per_page, host->param.nsect_size);
	dev_dbg(host->dev, "nand:nspare_size = [%d], page_size = [%d]\n",
			host->param.nspare_size, host->param.page_size);
	dev_dbg(host->dev, "nand:page_size =  [%d], spare_size = [%d]\n",
			host->param.page_size, host->param.spare_size);
	dev_dbg(host->dev, "nand:ecc_mode = [%d], ecc_pos = [%d]\n",
			host->param.ecc_mode, host->param.ecc_pos);
	dev_dbg(host->dev, "nand:ecc_size = [%d], info_pos = [%d], info_size = [%d]\n",
			host->param.ecc_size, host->param.info_pos,
			host->param.info_size);

	if (host->param.ecc_mode > 1)
		host->risk_threshold = host->param.ecc_mode / 2;
	else
		host->risk_threshold = host->param.ecc_mode;

	/* setting globe param seting */
	switch (host->param.ecc_mode) {
	case 1:
		host->eccmode_reg = 0;
		break;
	case 2:
		host->eccmode_reg = 1;
		break;
	case 4:
		host->eccmode_reg = 2;
		break;
	case 8:
		host->eccmode_reg = 3;
		break;
	case 12:
		host->eccmode_reg = 4;
		break;
	case 16:
		host->eccmode_reg = 5;
		break;
	case 24:
		host->eccmode_reg = 6;
		break;
	case 32:
		host->eccmode_reg = 7;
		break;
	case 40:
		host->eccmode_reg = 8;
		break;
	case 48:
		host->eccmode_reg = 9;
		break;
	case 56:
		host->eccmode_reg = 10;
		break;
	case 60:
		host->eccmode_reg = 11;
		break;
	case 62:
		host->eccmode_reg = 12;
		break;
	case 64:
		host->eccmode_reg = 13;
		break;
	case 66:
		host->eccmode_reg = 14;
		break;
	case 68:
		host->eccmode_reg = 15;
		break;
	case 70:
		host->eccmode_reg = 16;
		break;
	case 72:
		host->eccmode_reg = 17;
		break;
	case 74:
		host->eccmode_reg = 18;
		break;
	case 76:
		host->eccmode_reg = 19;
		break;
	case 78:
		host->eccmode_reg = 20;
		break;
	case 80:
		host->eccmode_reg = 21;
		break;
	default:
		dev_err(host->dev, "nand:sprd nand ecc mode not support!\n");
		return -EINVAL;
	}

	host->sect_perpage_shift = ffs(host->param.nsect_per_page) - 1;
	host->sectshift = ffs(host->param.nsect_size) - 1;
	host->sectmsk = BIT(host->sectshift) - 1;
	host->pageshift = ffs(host->param.nsect_per_page <<
						     host->sectshift) - 1;
	host->blkshift = ffs(host->param.npage_per_blk << host->pageshift) - 1;
	host->chipshift = ffs(host->param.nblkcnt << host->blkshift) - 1;
	if (host->c_pages)
		host->bufshift = ffs(host->param.page_size * host->c_pages) - 1;
	else
		host->bufshift = ffs(NFC_MBUF_SIZE) - 1;
	host->bufshift = min(host->bufshift, host->blkshift);
	dev_dbg(host->dev, "nand: sectperpgshift %d, sectshift %d\n",
			host->sect_perpage_shift, host->sectshift);
	dev_dbg(host->dev, "nand:secmsk %d, pageshift %d,blkshift %d, bufshift %d\n",
			host->sectmsk, host->pageshift, host->blkshift, host->bufshift);

	return 0;
}

static void sprd_nfc_param_init_inst(struct sprd_nfc *host)
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
	sprd_nfc_cmd_add(&host->inst_erase, INST_CMD(0x60));
	sprd_nfc_cmd_tag(&host->inst_erase);
	sprd_nfc_cmd_add(&host->inst_erase, INST_ADDR(0, 1));
	sprd_nfc_cmd_add(&host->inst_erase, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nfc_cmd_add(&host->inst_erase, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_erase, INST_CMD(0xD0));
	sprd_nfc_cmd_add(&host->inst_erase, INST_WRB0());
	sprd_nfc_cmd_add(&host->inst_erase, INST_CMD(0x70));
	sprd_nfc_cmd_add(&host->inst_erase, INST_IDST(1));
	sprd_nfc_cmd_add(&host->inst_erase, INST_DONE());
	/* read main+spare(info)+Ecc or Raw */
	host->inst_read_main_spare.program_name = "_inst_read_main_spare";
	sprd_nfc_cmd_init(&host->inst_read_main_spare, INT_TO | INT_DONE);
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_CMD(0x00));
	sprd_nfc_cmd_add(&host->inst_read_main_spare,
						INST_ADDR((0xFF & (u8)column), 0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare,
						INST_ADDR((0xFF & (u8)(column >> 8)), 0));
	sprd_nfc_cmd_tag(&host->inst_read_main_spare);
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 1));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_CMD(0x30));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_WRB0());
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_SRDT());

	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_CMD(0x05));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_CMD(0xE0));
	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_MRDT());

	sprd_nfc_cmd_add(&host->inst_read_main_spare, INST_DONE());
	/* read main raw */
	host->inst_read_main_raw.program_name = "_inst_read_main_raw";
	sprd_nfc_cmd_init(&host->inst_read_main_raw, INT_TO | INT_DONE);
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_CMD(0x00));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 0));
	sprd_nfc_cmd_tag(&host->inst_read_main_raw);
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 1));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_CMD(0x30));
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_WRB0());
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_MRDT());
	sprd_nfc_cmd_add(&host->inst_read_main_raw, INST_DONE());
	/*
	 * read spare raw: read only main or only spare data,
	 * it is read to main addr.
	 */
	host->inst_read_spare_raw.program_name = "_inst_read_spare_raw";
	sprd_nfc_cmd_init(&host->inst_read_spare_raw, INT_TO | INT_DONE);
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, INST_CMD(0x00));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw,
						INST_ADDR((0xFF & (u8)column), 0));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw,
						INST_ADDR((0xFF & (u8)(column >> 8)), 0));
	sprd_nfc_cmd_tag(&host->inst_read_spare_raw);
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, INST_ADDR(0, 1));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nfc_cmd_add(&host->inst_read_spare_raw, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, INST_CMD(0x30));
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, INST_WRB0());
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, INST_MRDT());
	sprd_nfc_cmd_add(&host->inst_read_spare_raw, INST_DONE());
	/* write main+spare(info)+ecc */
	host->inst_write_main_spare.program_name = "_inst_write_main_spare";
	sprd_nfc_cmd_init(&host->inst_write_main_spare,
						 INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_CMD(0x80));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_ADDR(0, 0));
	sprd_nfc_cmd_tag(&host->inst_write_main_spare);
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_ADDR(0, 1));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nfc_cmd_add(&host->inst_write_main_spare,
							INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_MWDT());

	/* just need input column addr */
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_CMD(0x85));
	sprd_nfc_cmd_add(&host->inst_write_main_spare,
						INST_ADDR((0xFF & (u8)column), 0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare,
						INST_ADDR((0xFF & (u8)(column >> 8)), 0));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_SWDT());

	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_CMD(0x10));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_WRB0());
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_CMD(0x70));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_IDST(1));
	sprd_nfc_cmd_add(&host->inst_write_main_spare, INST_DONE());
	/* write main raw */
	host->inst_write_main_raw.program_name = "_inst_write_main_raw";
	sprd_nfc_cmd_init(&host->inst_write_main_raw,
						 INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_CMD(0x80));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 0));
	sprd_nfc_cmd_tag(&host->inst_write_main_raw);
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 1));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_MWDT());
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_CMD(0x10));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_WRB0());
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_CMD(0x70));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_IDST(1));
	sprd_nfc_cmd_add(&host->inst_write_main_raw, INST_DONE());
	/* write spare raw */
	host->inst_write_spare_raw.program_name = "_inst_write_spare_raw";
	sprd_nfc_cmd_init(&host->inst_write_spare_raw,
						 INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_CMD(0x80));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw,
						INST_ADDR((0xFF & column), 0));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw,
						INST_ADDR((0xFF & (column >> 8)), 0));
	sprd_nfc_cmd_tag(&host->inst_write_spare_raw);
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_ADDR(0, 1));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_ADDR(0, 0));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_SWDT());
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_CMD(0x10));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_WRB0());
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_CMD(0x70));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_IDST(1));
	sprd_nfc_cmd_add(&host->inst_write_spare_raw, INST_DONE());
}

static int sprd_nfc_param_init_buf(struct sprd_nfc *host)
{
	dma_addr_t phys_addr;
	void *virt_ptr;
	u32 mbuf_size = BIT(host->bufshift);
	u32 sbuf_size = BIT(host->bufshift - host->sectshift) *
					host->param.nspare_size;
	u32 stsbuf_size = BIT(host->bufshift - host->sectshift) *
						 (sizeof(struct nand_ecc_stats));
	u32 seedbuf_size = SEED_BUF_SIZE * 4;
	u32 buf_size;
	u32 offset = 0;

	dev_dbg(host->dev,
			"nand:mbuf_size %d, sbuf_size %d, stsbuf_size %d\n",
			mbuf_size, sbuf_size, stsbuf_size);
	dev_dbg(host->dev,
			"nand:host->bufshift %d, param.nspare_size %d,sectshift %d\n",
			host->bufshift, host->param.nspare_size, host->sectshift);

	buf_size = mbuf_size + sbuf_size + 64 + stsbuf_size + 64 + seedbuf_size;

	virt_ptr = dma_alloc_coherent(host->dev, buf_size, &phys_addr, GFP_KERNEL);
	if (!virt_ptr)
		return -ENOMEM;

	host->buf_p = (u8 *)phys_addr;
	host->buf_v = (u8 *)virt_ptr;
	host->buf_size = buf_size;

	host->mbuf_p = (u8 *)phys_addr;
	host->mbuf_v = (u8 *)virt_ptr;
	host->mbuf_size = mbuf_size;
	offset += mbuf_size;

	host->sbuf_p = (u8 *)phys_addr + offset;
	host->sbuf_v = (u8 *)virt_ptr + offset;
	host->sbuf_size = sbuf_size;

	offset = (offset + sbuf_size + 64) & ~0x3F;
	host->stsbuf_p = (u8 *)phys_addr + offset;
	host->stsbuf_v = (u8 *)virt_ptr + offset;
	host->stsbuf_size = stsbuf_size;

	offset = (offset + stsbuf_size + 64) & ~0x3F;
	host->seedbuf_p = (u32 *)((u8 *)phys_addr + offset);
	host->seedbuf_v = (u32 *)((u8 *)virt_ptr + offset);
	host->seedbuf_size = seedbuf_size;

	return 0;
}

static int sprd_nfc_param_init(struct sprd_nfc *host,
	struct sprd_nand_param *param)
{
	int ret;

	ret = sprd_nfc_param_init_nandhw(host, param);
	if (ret)
		return ret;

	return sprd_nfc_param_init_buf(host);
}

static void sprd_nfc_init_reg_state1(struct sprd_nfc *host)
{
	host->nfc_start |= CTRL_DEF1_ECC_MODE(host->eccmode_reg);
	host->nfc_cfg0 |= CFG0_DEF1_SECT_NUM(host->param.nsect_per_page) |
		CFG0_DEF1_BUS_WIDTH(host->param.nbus_width) |
		CFG0_DEF1_MAIN_SPAR_APT(host->param.nsect_per_page);
	host->nfc_cfg1 = CFG1_DEF1_SPAR_INFO_SIZE(host->param.info_size) |
		CFG1_DEF1_SPAR_SIZE(host->param.nspare_size) |
		CFG1_DEF_MAIN_SIZE(host->param.nsect_size) |
		((host->intf_type & FLASH_INTFTYPE_MASK) << INTF_TYPE_OFFSET);
	host->nfc_cfg2 = CFG2_DEF1_SPAR_SECTOR_NUM(host->param.nsect_per_page) |
		CFG2_DEF1_SPAR_INFO_POS(host->param.info_pos) |
		CFG2_DEF1_ECC_POSITION(host->param.ecc_pos);

	sprd_nfc_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	sprd_nfc_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
	sprd_nfc_writel(host, host->nfc_cfg2, NFC_CFG2_REG);
	sprd_nfc_writel(host, 0xFFFFFFFF, NFC_TIMEOUT_REG);

	sprd_nfc_writel(host, RAM_STAT_ADDRH(host->stsbuf_p), NFC_STAT_ADDRH_REG);
	sprd_nfc_writel(host, RAM_STAT_ADDRL(host->stsbuf_p), NFC_STAT_ADDRL_REG);
	sprd_nfc_writel(host, RAM_MAIN_ADDRH(host->mbuf_p), NFC_MAIN_ADDRH_REG);
	sprd_nfc_writel(host, RAM_MAIN_ADDRL(host->mbuf_p), NFC_MAIN_ADDRL_REG);
	sprd_nfc_writel(host, RAM_SPAR_ADDRH(host->sbuf_p), NFC_SPAR_ADDRH_REG);
	sprd_nfc_writel(host, RAM_SPAR_ADDRL(host->sbuf_p), NFC_SPAR_ADDRL_REG);
}

/*
 * 0: ecc pass
 * -EBADMSG: ecc fail
 * -EUCLEAN: ecc risk
 */
static int sprd_nfc_check_ff(struct sprd_nfc *host, u32 sects, u32 mode)
{
	u32 i, obb_size, sectsize = BIT(host->sectshift);
	u8 *main_buf, *spare_buf;
	s32 bit0_num, bit0_total = 0;
	u32 mbit_0pos[60];
	u32 mbit_0arridx = 0;
	u32 sbit_0pos[60];
	u32 sbit_0arridx = 0;
	s32 risk_num = min_t(int32_t, 4, host->risk_threshold);

	if (mode == MTD_OPS_AUTO_OOB)
		obb_size = host->param.info_size;
	else
		obb_size = host->param.nspare_size;

	main_buf = host->mbuf_v + (sects << host->sectshift);
	spare_buf = host->sbuf_v + (sects * obb_size);

	for (i = 0; i < sectsize; i++) {
		bit0_num = (int32_t)bit_num8[main_buf[i]];
		if (bit0_num) {
			dev_err(host->dev, "main_buf[i] = 0x%x\n", main_buf[i]);
			bit0_total += bit0_num;
			if (bit0_total > risk_num)
				return -EBADMSG;

			mbit_0pos[mbit_0arridx] = i;
			mbit_0arridx++;
		}
	}
	sbit_0arridx = 0;
	for (i = 0; i < obb_size; i++) {
		bit0_num = (int32_t)bit_num8[spare_buf[i]];
		if (bit0_num) {
			dev_err(host->dev, "spare_buf[i] = 0x%x\n", spare_buf[i]);
			bit0_total += bit0_num;
			if (bit0_total > risk_num)
				return -EBADMSG;

			sbit_0pos[sbit_0arridx] = i;
			sbit_0arridx++;
		}
	}
	for (i = 0; i < mbit_0arridx; i++)
		main_buf[mbit_0pos[i]] = 0xFF;
	for (i = 0; i < sbit_0arridx; i++)
		spare_buf[sbit_0pos[i]] = 0xFF;

	return bit0_total;
}

static int sprd_nfc_ecc_analyze(struct sprd_nfc *host, u32 num,
	struct mtd_ecc_stats *ecc_sts, u32 mode)
{
	u32 i;
	u32 n;
	struct nand_ecc_stats *nand_ecc_sts =
	    (struct nand_ecc_stats *)(host->stsbuf_v);
	u32 sector_num = host->param.nsect_per_page;
	u32 ecc_banknum = num / sector_num;
	u32 sector = 0;
	int ret = 0;

	for (i = 0; i <= ecc_banknum; i++) {
		for (n = 0; n < min((num - sector_num * i), sector_num); n++) {
			sector = n + sector_num * i;
			switch (ECC_STAT(nand_ecc_sts->ecc_stats[n])) {
			case 0x00:
				/* pass */
				break;
			case 0x02:
			case 0x03:
				/* fail */
				ret = sprd_nfc_check_ff(host, sector, mode);
				if (ret == -EBADMSG)
					ecc_sts->failed++;
				else
					ecc_sts->corrected += ret;
				break;
			case 0x01:
				if (ECC_NUM(nand_ecc_sts->ecc_stats[n]) ==
						0x1FF) {
					ret = sprd_nfc_check_ff(host, sector, mode);
					if (ret == -EBADMSG)
						ecc_sts->failed++;
					else
						ecc_sts->corrected += ret;
				};
				if (host->param.ecc_mode <=
						ECC_NUM(nand_ecc_sts->ecc_stats[n])) {
					ecc_sts->failed++;
					ret = -EBADMSG;
				} else {
					ecc_sts->corrected +=
					    ECC_NUM(nand_ecc_sts->ecc_stats[n]);
				}
				break;
			default:
				dev_err(host->dev, "sprd nand error ecc sts\n");
				break;
			}
			if (ret == -EBADMSG)
				goto err;
		}
		nand_ecc_sts++;
	}
	if (ret)
		dev_err(host->dev, "sprd nand read ecc sts %d\n", ret);

	return ret;
err:
	dev_err(host->dev, "sprd nand read ecc sts %d\n", ret);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_OFFSET, 32, 4,
			       (void *)(host->mbuf_v + (sector << host->sectshift)),
			       0x200, 1);

	return ret;
}

/*
 * if(0!=mBuf) then read main area
 * if(0!=sBuf) then read spare area or read spare info
 * if(MTD_OPS_PLACE_OOB == mode) then just read main area with Ecc
 * if(MTD_OPS_AUTO_OOB == mode) then read main area(if(0!=mBuf)),
 * and spare info(if(0!=sbuf)), with Ecc
 * if(MTD_OPS_RAW == mode) then read main area(if(0!=mBuf)),
 * and spare info(if(0!=sbuf)), without Ecc
 * return
 * 0: ecc pass
 * -EBADMSG	: ecc fail
 * -EUCLEAN	: ecc risk
 * -EIO		: read fail
 * mtd->ecc_stats
 */
static int sprd_nfc_read_page(struct sprd_nfc *host,
	u32 page, u32 num,
	u32 *ret_num, u32 if_has_mbuf,
	u32 if_has_sbuf, u32 mode,
	struct mtd_ecc_stats *ecc_sts,
	bool random,
	int flag)
{
	struct nand_inst inst_read_main_spare = host->inst_read_main_spare;
	struct nand_inst inst_read_main_raw = host->inst_read_main_raw;
	struct nand_inst inst_read_spare_raw = host->inst_read_spare_raw;
	struct nand_inst *inst = NULL;
	int ret;
	u32 if_change_buf = 0;

	host->int_ops->select_cs(host, GETCS(page));
	host->nfc_sts_mach_val = MACH_READ;
	host->nfc_time0_val = host->nfc_time0_r;
	host->nfc_cfg0_val = (host->nfc_cfg0 | CFG0_SET_NFC_MODE(AUTO_MODE));

	if (host->randomizer && random)
		host->int_ops->set_randomize(host, page, true);

	switch (mode) {
	case MTD_OPS_AUTO_OOB:
		host->nfc_cfg0_val |= CFG0_SET_SPARE_ONLY_INFO_PROD_EN;
		fallthrough;
	case MTD_OPS_PLACE_OOB:
		host->nfc_cfg0_val |= (CFG0_SET_ECC_EN | CFG0_SET_MAIN_USE |
							      CFG0_SET_SPAR_USE);
		inst = &inst_read_main_spare;
		break;
	case MTD_OPS_RAW:
		if (if_has_mbuf & if_has_sbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE |
								      CFG0_SET_SPAR_USE);
			inst = &inst_read_main_spare;
		} else if (if_has_mbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE);
			inst = &inst_read_main_raw;
		} else if (if_has_sbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE);
			inst = &inst_read_spare_raw;
			/*
			 * 0 nand controller use mainAddr to send spare data,
			 * So i have to change some globe config here
			 * if_change_buf = 1;
			 * 1 change to main buf
			 */
			if_change_buf = 1;
			sprd_nfc_writel(host, RAM_MAIN_ADDRH(host->sbuf_p), NFC_MAIN_ADDRH_REG);
			sprd_nfc_writel(host, RAM_MAIN_ADDRL(host->sbuf_p), NFC_MAIN_ADDRL_REG);
			/* 2 change page_size */
			host->nfc_cfg1 &= (~CFG1_temp_MIAN_SIZE_MSK);
			host->nfc_cfg1 |=
			    CFG1_DEF_MAIN_SIZE(host->param.nspare_size <<
									host->sect_perpage_shift);
			sprd_nfc_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
			/* 3 change sect number */
			host->nfc_cfg0_val &= (~CFG0_SET_SECT_NUM_MSK);
			host->nfc_cfg0_val |= CFG0_DEF1_SECT_NUM(1);
		} else {
			host->int_ops->delect_cs(host, -EINVAL);
			return -EINVAL;
		}
		break;
	default:
		dev_err(host->dev, "nand:sprd nand ops mode error!\n");
		host->int_ops->delect_cs(host, -EINVAL);
		return -EINVAL;
	}

	host->int_ops->cmd_change(host, inst, page);
	sprd_nfc_cmd_exec(host, inst, num, 0);
	ret = sprd_nfc_cmd_wait(host, inst);
	if (if_change_buf) {
		/* 1 change to main buf */
		sprd_nfc_writel(host, RAM_MAIN_ADDRH(host->mbuf_p), NFC_MAIN_ADDRH_REG);
		sprd_nfc_writel(host, RAM_MAIN_ADDRL(host->mbuf_p), NFC_MAIN_ADDRL_REG);
		/* 2 change page_size */
		host->nfc_cfg1 &= (~CFG1_temp_MIAN_SIZE_MSK);
		host->nfc_cfg1 |= CFG1_DEF_MAIN_SIZE(host->param.nsect_size);
		sprd_nfc_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
		/* 3 change sect number */
		sprd_nfc_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	}

	if (host->randomizer && random)
		host->int_ops->set_randomize(host, page, false);

	if (ret) {
		*ret_num = 0;
		host->int_ops->delect_cs(host, -EIO);
		return -EIO;
	}
	*ret_num = num;

	switch (mode) {
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_PLACE_OOB:
		ret = host->int_ops->ecc_analyze(host,
				num * host->param.nsect_per_page,
				ecc_sts, mode);
		if (ret)
			dev_err(host->dev, "nand:page %d, ecc error ret %d\n", page, ret);
		break;
	case MTD_OPS_RAW:
		break;
	default:
		dev_err(host->dev, "nand:sprd nand ops mode error!\n");
		break;
	}
	host->int_ops->delect_cs(host, ret);

	return ret;
}

static int sprd_nfc_read_page_retry(struct sprd_nfc *host,
	u32 page, u32 num,
	u32 *ret_num, u32 if_has_mbuf,
	u32 if_has_sbuf, u32 mode,
	struct mtd_ecc_stats *ecc_sts,
	bool random,
	int flag)
{
	int ret, i = 0;

	do {
		ret = sprd_nfc_read_page(host, page, num, ret_num,
							if_has_mbuf, if_has_sbuf,
							mode, ecc_sts, random, flag);
		if (!ret || ret == -EINVAL || ret == -EUCLEAN)
			break;
		i++;
		dev_err(host->dev, "sprd_nand:retry %d\n", i);
	} while (i < 3);

	return ret;
}

static void sprd_nfc_set_wp(struct sprd_nfc *host, bool en)
{
	u32 reg_nfc_cfg0;

	reg_nfc_cfg0 = sprd_nfc_readl(host, NFC_CFG0_REG);
	if (en) {
		host->nfc_cfg0 &= ~(CFG0_SET_WPN);
		reg_nfc_cfg0 &= ~CFG0_SET_WPN;
	} else {
		host->nfc_cfg0 |= CFG0_SET_WPN;
		reg_nfc_cfg0 |= CFG0_SET_WPN;
	}
	sprd_nfc_writel(host, reg_nfc_cfg0, NFC_CFG0_REG);
}

static int sprd_nfc_write_page(struct sprd_nfc *host,
	u32 page, u32 num,
	u32 *ret_num, u32 if_has_mbuf,
	u32 if_has_sbuf, u32 mode, bool random)
{
	struct nand_inst inst_write_main_spare = host->inst_write_main_spare;
	struct nand_inst inst_write_spare_raw = host->inst_write_spare_raw;
	struct nand_inst inst_write_main_raw = host->inst_write_main_raw;
	struct nand_inst *inst = NULL;
	int ret;
	u32 if_change_buf = 0;

	host->int_ops->select_cs(host, GETCS(page));
	host->int_ops->set_wp(host, false);

	host->nfc_sts_mach_val = MACH_WRITE;
	host->nfc_time0_val = host->nfc_time0_w;
	host->nfc_cfg0_val =
	    (host->nfc_cfg0 | CFG0_SET_NFC_MODE(AUTO_MODE) | CFG0_SET_NFC_RW);

	if (host->randomizer && random)
		host->int_ops->set_randomize(host, page, true);

	switch (mode) {
	case MTD_OPS_AUTO_OOB:
		host->nfc_cfg0_val |= CFG0_SET_SPARE_ONLY_INFO_PROD_EN;
		fallthrough;
	case MTD_OPS_PLACE_OOB:
		host->nfc_cfg0_val |= (CFG0_SET_ECC_EN | CFG0_SET_MAIN_USE |
							      CFG0_SET_SPAR_USE);
		inst = &inst_write_main_spare;
		break;
	case MTD_OPS_RAW:
		if (if_has_mbuf & if_has_sbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE |
								      CFG0_SET_SPAR_USE);
			inst = &inst_write_main_spare;
		} else if (if_has_mbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE);
			inst = &inst_write_main_raw;
		} else if (if_has_sbuf) {
			/* use main to write spare area */
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE);
			inst = &inst_write_spare_raw;
			/*
			 * 0 nand controller use mainAddr to send spare data,
			 * So i have to change some globe config here
			 * if_change_buf = 1;
			 * 1 change to main buf
			 */
			if_change_buf = 1;
			sprd_nfc_writel(host, RAM_MAIN_ADDRH(host->sbuf_p), NFC_MAIN_ADDRH_REG);
			sprd_nfc_writel(host, RAM_MAIN_ADDRL(host->sbuf_p), NFC_MAIN_ADDRL_REG);
			/* 2 change page_size */
			host->nfc_cfg1 &= (~CFG1_temp_MIAN_SIZE_MSK);
			host->nfc_cfg1 |=
			    CFG1_DEF_MAIN_SIZE(host->param.nspare_size <<
								      host->sect_perpage_shift);
			sprd_nfc_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
			/* 3 change sect number */
			host->nfc_cfg0_val &= (~CFG0_SET_SECT_NUM_MSK);
			host->nfc_cfg0_val |= CFG0_DEF1_SECT_NUM(1);
		} else {
			host->int_ops->set_wp(host, true);
			host->int_ops->delect_cs(host, -EINVAL);
			return -EINVAL;
		}
		break;
	default:
		dev_err(host->dev, "nand:sprd nand ops mode error!\n");
		host->int_ops->set_wp(host, true);
		host->int_ops->delect_cs(host, -EINVAL);
		return -EINVAL;
	}

	host->int_ops->cmd_change(host, inst, page);
	sprd_nfc_cmd_exec(host, inst, num, 0);
	ret = sprd_nfc_cmd_wait(host, inst);
	if (if_change_buf) {
		/* 1 change to main buf */
		sprd_nfc_writel(host, RAM_MAIN_ADDRH(host->mbuf_p), NFC_MAIN_ADDRH_REG);
		sprd_nfc_writel(host, RAM_MAIN_ADDRL(host->mbuf_p), NFC_MAIN_ADDRL_REG);
		/* 2 change page_size */
		host->nfc_cfg1 &= (~CFG1_temp_MIAN_SIZE_MSK);
		host->nfc_cfg1 |= CFG1_DEF_MAIN_SIZE(host->param.nsect_size);
		sprd_nfc_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
		/* 3 change sect number */
		sprd_nfc_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	}

	if (host->randomizer && random)
		host->int_ops->set_randomize(host, page, false);

	if (ret) {
		*ret_num = 0;
		host->int_ops->set_wp(host, true);
		host->int_ops->delect_cs(host, -EIO);
		return ret;
	}
	*ret_num = num;
	host->int_ops->set_wp(host, true);
	host->int_ops->delect_cs(host, 0);

	return 0;
}

static int sprd_nfc_erase(struct sprd_nfc *host, u32 page)
{
	struct nand_inst inst_erase_blk = host->inst_erase;
	struct nand_inst *inst = &inst_erase_blk;
	int ret;

	host->int_ops->select_cs(host, GETCS(page));
	host->int_ops->set_wp(host, false);

	host->nfc_sts_mach_val = MACH_ERASE;
	host->nfc_time0_val = host->nfc_time0_e;
	host->nfc_cfg0_val = (host->nfc_cfg0 | CFG0_SET_NFC_MODE(ONLY_NAND_MODE));

	host->int_ops->cmd_change(host, inst, page);
	sprd_nfc_cmd_exec(host, inst, 1, 0);
	ret = sprd_nfc_cmd_wait(host, inst);

	host->int_ops->set_wp(host, true);
	host->int_ops->delect_cs(host, ret);

	return ret;
}

int sprd_nfc_is_bad(struct sprd_nfc *host, u32 page, bool random)
{
	u32 i, k, num = 0, len;
	u8 *flag;
	u32 ret_num;
	int ret;

	ret = host->ops->read_page(host, page, 1, &ret_num, 0, 1, MTD_OPS_RAW, 0, random, 0);
	if (ret)
		return ret;

	flag = host->sbuf_v + host->param.badflag_pos;
	len = host->param.badflag_len;

	for (i = 0; i < len; i++) {
		for (k = 0; k < 8; k++) {
			if (flag[i] & BIT(k))
				num++;
		}
	}

	return (num < (len << 2));
}

int sprd_nfc_mark_bad(struct sprd_nfc *host, u32 page, bool random)
{
	u32 ret_num;
	int ret = 0;

	if (random) {
		ret = host->ops->erase(host, page);
		if (ret)
			return ret;
	}

	memset(host->sbuf_v, 0xFF,
		host->param.nspare_size * host->param.nsect_per_page);
	memset(host->sbuf_v + host->param.badflag_pos, 0,
		host->param.badflag_len);

	return host->ops->write_page(host, page, 1, &ret_num, 0, 1, MTD_OPS_RAW, random);
}

static int sprd_nfc_clk_enable(struct sprd_nfc *host)
{
	int ret;

	ret = clk_prepare_enable(host->nandc_ahb_enable);
	if (ret)
		return ret;

	if (host->version == SPRD_NANDC_VERSIONS_R6P0) {
		ret = clk_prepare_enable(host->nandc_ecc_clk_eb);
		if (ret)
			goto err_ecc_clk_eb;

		ret = clk_prepare_enable(host->nandc_26m_clk_eb);
		if (ret)
			goto err_26m_clk_eb;

		ret = clk_prepare_enable(host->nandc_1x_clk_eb);
		if (ret)
			goto err_1x_clk_eb;

		ret = clk_prepare_enable(host->nandc_2x_clk_eb);
		if (ret)
			goto err_2x_clk_eb;
	}

	clk_set_parent(host->nandc_2x_clk, host->nandc_2xwork_clk);
	host->frequency = clk_get_rate(host->nandc_2xwork_clk);
	ret = clk_prepare_enable(host->nandc_2x_clk);
	if (ret)
		goto err_2x_clk;

	clk_set_parent(host->nandc_ecc_clk, host->nandc_eccwork_clk);
	ret = clk_prepare_enable(host->nandc_ecc_clk);
	if (ret)
		goto err_ecc_clk;

	return 0;

err_ecc_clk:
	clk_disable_unprepare(host->nandc_2x_clk);
err_2x_clk:
	if (host->version == SPRD_NANDC_VERSIONS_R6P0) {
		clk_disable_unprepare(host->nandc_2x_clk_eb);
err_2x_clk_eb:
		clk_disable_unprepare(host->nandc_1x_clk_eb);
err_1x_clk_eb:
		clk_disable_unprepare(host->nandc_26m_clk_eb);
err_26m_clk_eb:
		clk_disable_unprepare(host->nandc_ecc_clk_eb);
	}
err_ecc_clk_eb:
	clk_disable_unprepare(host->nandc_ahb_enable);

	return ret;
}

static void sprd_nfc_clk_disable(struct sprd_nfc *host)
{
	clk_disable_unprepare(host->nandc_ecc_clk);
	clk_disable_unprepare(host->nandc_2x_clk);

	if (host->version == SPRD_NANDC_VERSIONS_R6P0) {
		clk_disable_unprepare(host->nandc_2x_clk_eb);
		clk_disable_unprepare(host->nandc_1x_clk_eb);
		clk_disable_unprepare(host->nandc_26m_clk_eb);
		clk_disable_unprepare(host->nandc_ecc_clk_eb);
	}

	clk_disable_unprepare(host->nandc_ahb_enable);
}

static int sprd_nfc_ctrl_en(struct sprd_nfc *host, bool en)
{
	if (en)
		return sprd_nfc_clk_enable(host);

	sprd_nfc_clk_disable(host);

	return 0;
}

/**
 * clock issues is handled by sprd_nfc_get_device & sprd_nfc_put_device
 * in nfc_base.c file.
 * when do nandc operateor such as erase write & read,
 * so we make suspend dummy here.
 */
static void sprd_nfc_suspend(struct sprd_nfc *host)
{

}

static int sprd_nfc_resume(struct sprd_nfc *host)
{
	int i, ret;

	host->int_ops->init_reg_state0(host);

	for (i = 0; i < host->csnum; i++) {
		dev_info(host->dev, "try to probe flash%d\n", i);
		host->int_ops->select_cs(host, i);
		ret = host->int_ops->reset(host);
		if (ret) {
			sprd_nfc_writel(host, CTRL_NFC_CMD_CLR, NFC_START_REG);
			dev_err(host->dev, "flash%d reset fail\n", i);
			break;
		}
		host->int_ops->delect_cs(host, ret);
	}
	host->int_ops->init_reg_state1(host);

	return 0;
}

static int sprd_nfc_probe_device(struct sprd_nfc *host, u32 *id0, u32 *id1)
{
	u32 cid0, cid1, cs0_id0 = 0, cs0_id1 = 0;
	int ret, i;

	ret = host->ops->ctrl_en(host, true);
	if (ret) {
		dev_err(host->dev, "nandc clk enable failed: %d\n", ret);
		return ret;
	}

	sprd_nfc_set_timing_config(host, &default_timing, host->frequency);
	host->int_ops->init_reg_state0(host);

	for (i = 0; i < host->cs_max; i++) {
		dev_dbg(host->dev, "try to probe flash%d\n", i);
		host->int_ops->select_cs(host, i);
		ret = host->int_ops->reset(host);
		if (ret) {
			sprd_nfc_writel(host, CTRL_NFC_CMD_CLR, NFC_START_REG);
			dev_warn(host->dev, "flash%d reset fail\n", i);
			break;
		}

		ret = host->int_ops->readid(host, &cid0, &cid1);
		if (ret) {
			dev_warn(host->dev,
					 "flash%d sprd_nfc_readid fail\n", i);
			host->int_ops->delect_cs(host, ret);
			break;
		}
		host->int_ops->delect_cs(host, ret);

		dev_dbg(host->dev, "find flash%d,id[] is %x %x %x %x %x\n",
				i, (cid0 >> 0) & 0xFF, (cid0 >> 8) & 0xFF,
				(cid0 >> 16) & 0xFF, (cid0 >> 24) & 0xFF,
				(cid1 >> 0) & 0xFF);
		if (i == 0) {
			cs0_id0 = cid0;
			cs0_id1 = cid1;

			host->param.id[0] = (cid0 >> 0) & 0xFF;
			host->param.id[1] = (cid0 >> 8) & 0xFF;
			host->param.id[2] = (cid0 >> 16) & 0xFF;
			host->param.id[3] = (cid0 >> 24) & 0xFF;
			host->param.id[4] = (cid1 >> 0) & 0xFF;
			host->param.id[5] = (cid1 >> 8) & 0xFF;
			host->param.id[6] = (cid1 >> 16) & 0xFF;
			host->param.id[7] = (cid1 >> 24) & 0xFF;
		} else if (cid0 != cs0_id0 || cid1 != cs0_id1)
			break;

		host->csnum = host->csnum + 1;
		host->cs[i] = i;

	}

	if (!host->csnum)
		return -ENODEV;

	*id0 = cs0_id0;
	*id1 = cs0_id1;

	return 0;
}

static struct sprd_nfc_internal_ops sprd_nfc_int_ops = {
	.prepare_before_exec = sprd_nfc_prepare_before_exec,
	.select_cs = sprd_nfc_select_cs,
	.delect_cs = sprd_nfc_delect_cs,
	.init_reg_state0 = sprd_nfc_init_reg_state0,
	.init_reg_state1 = sprd_nfc_init_reg_state1,
	.init_inst = sprd_nfc_param_init_inst,
	.set_wp = sprd_nfc_set_wp,
	.set_randomize = sprd_nfc_set_randomize,
	.ecc_analyze = sprd_nfc_ecc_analyze,
	.reset = sprd_nfc_reset,
	.readid = sprd_nfc_readid,
	.cmd_change = sprd_nfc_cmd_change,
};

static struct sprd_nfc_ops sprd_nfc_ops = {
	.ctrl_en = sprd_nfc_ctrl_en,
	.ctrl_suspend = sprd_nfc_suspend,
	.ctrl_resume = sprd_nfc_resume,
	.read_page = sprd_nfc_read_page_retry,
	.write_page = sprd_nfc_write_page,
	.erase = sprd_nfc_erase,
	.block_is_bad = sprd_nfc_is_bad,
	.block_mark_bad = sprd_nfc_mark_bad,
	.probe_device = sprd_nfc_probe_device,
};

struct sprd_nfc *sprd_nfc_alloc_host(struct device *dev, u8 intf_type)
{
	struct sprd_nfc *host;
#ifdef CONFIG_MTD_NAND_SPRD_DEBUG
	int ret;
#endif

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return ERR_PTR(-ENOMEM);

	host->dev = dev;
	host->int_ops = &sprd_nfc_int_ops;
	host->ops = &sprd_nfc_ops;
	host->intf_type = intf_type;

	if (intf_type == INTF_TYPE_SPI)
		sprd_snfc_attach((struct sprd_nfc *)host);
	else
		host->cs_max = CFG0_CS_MAX;

#ifdef CONFIG_MTD_NAND_SPRD_DEBUG
	ret = sprd_nfc_dbg_init(host);
	if (ret)
		dev_err(dev, "nfc dbg init failed: %d\n", ret);
#endif

	return host;
}
EXPORT_SYMBOL(sprd_nfc_alloc_host);

int sprd_nfc_setup_host(struct sprd_nfc *host,
	struct sprd_nand_param *param,
	u32 c_pages)
{
	int ret;

	host->c_pages = c_pages;
	ret = sprd_nfc_param_init(host, param);
	if (ret)
		return ret;

	host->int_ops->init_reg_state1(host);
	host->int_ops->init_inst(host);

#ifdef CONFIG_MTD_NAND_SPRD_DEBUG
	sprd_nfc_dbg_sav_startup(host);
#endif

	if (host->intf_type == INTF_TYPE_SPI)
		return sprd_snfc_setup_host((struct sprd_nfc *)host);

	return 0;
}
EXPORT_SYMBOL(sprd_nfc_setup_host);

void sprd_nfc_free_host(struct sprd_nfc *host)
{
	int ret;

	ret = host->ops->ctrl_en(host, false);
	if (ret)
		dev_warn(host->dev, "ctrl en failed: %d\n", ret);

	dma_free_coherent(host->dev, host->buf_size, (void *)host->buf_v,
						 (dma_addr_t)host->buf_p);
}
EXPORT_SYMBOL(sprd_nfc_free_host);
