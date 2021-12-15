// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <memory/mediatek/emi.h>

/*
 * EMI address-to-dram setting's structure.
 * a2d is the abbreviation of addr2dram.
 * s6s is the abbreviation of settings.
 */
struct a2d_s6s_v1 {
	unsigned int magics[8];
	unsigned long cas;
	unsigned long chab_rk0_sz, chab_rk1_sz;
	unsigned long chcd_rk0_sz, chcd_rk1_sz;
	unsigned int channels;
	unsigned int dualrk_ch0, dualrk_ch1;
	unsigned int chn_hash_lsb, chnpos;
	unsigned int chab_row_mask[2], chcd_row_mask[2];
	unsigned int chab_col_mask[2], chcd_col_mask[2];
	unsigned int dw32;
	unsigned int chn_4bank_mode;
};

struct a2d_s6s_v2 {
	unsigned int chn_bit_position;
	unsigned int chn_en;
	unsigned int magics[8];

	unsigned int dual_rank_en;
	unsigned int dw32_en;
	unsigned int bg1_bk3_pos;
	unsigned int rank_pos;
	unsigned int magics2[7];
	unsigned int rank0_row_width, rank0_bank_width, rank0_col_width;
	unsigned int rank0_size_MB, rank0_bg_16bank_mode;
	unsigned int rank1_row_width, rank1_bank_width, rank1_col_width;
	unsigned int rank1_size_MB, rank1_bg_16bank_mode;
};

struct emi_cen {
	/*
	 * EMI setting from device tree
	 */
	int ver;
	unsigned int emi_cen_cnt;
	unsigned int ch_cnt;
	unsigned int rk_cnt;
	unsigned long long *rk_size;
	void __iomem **emi_cen_base;
	void __iomem **emi_chn_base;

	/* address from the sysfs file for EMI addr2dram */
	unsigned long a2d_addr;

	/*
	 * EMI addr2dram settings from device tree
	 */
	unsigned int disph;
	unsigned int hash;

	/*
	 * EMI addr2dram settings calculated at run time
	 */
	unsigned long offset;
	unsigned long max;
	union {
		struct a2d_s6s_v1 v1;
		struct a2d_s6s_v2 v2;
	} a2d_s6s;
};

#define EMI_CONA_DW32_EN 1
#define EMI_CONA_CHN_POS_0 2
#define EMI_CONA_CHN_POS_1 3
#define EMI_CONA_COL 4
#define EMI_CONA_COL2ND 6
#define EMI_CONA_CHN_EN 8
#define EMI_CONA_ROW 12
#define EMI_CONA_ROW2ND 14
#define EMI_CONA_DUAL_RANK_EN_CHN1 16
#define EMI_CONA_DUAL_RANK_EN 17
#define EMI_CONA_CAS_SIZE 18
#define EMI_CONA_CHN1_COL 20
#define EMI_CONA_CHN1_COL2ND 22
#define EMI_CONA_ROW_EXT0 24
#define EMI_CONA_ROW2ND_EXT0 25
#define EMI_CONA_CAS_SIZE_BIT3 26
#define EMI_CONA_RANK_POS 27
#define EMI_CONA_CHN1_ROW 28
#define EMI_CONA_CHN1_ROW2ND 30

#define EMI_CONH_CHN1_ROW_EXT0 4
#define EMI_CONH_CHN1_ROW2ND_EXT0 5
#define EMI_CONH_CHNAB_RANK0_SIZE 16
#define EMI_CONH_CHNAB_RANK1_SIZE 20
#define EMI_CONH_CHNCD_RANK0_SIZE 24
#define EMI_CONH_CHNCD_RANK1_SIZE 28

#define EMI_CONH_2ND_CHN_4BANK_MODE 6

#define EMI_CONK_CHNAB_RANK0_SIZE_EXT 16
#define EMI_CONK_CHNAB_RANK1_SIZE_EXT 20
#define EMI_CONK_CHNCD_RANK0_SIZE_EXT 24
#define EMI_CONK_CHNCD_RANK1_SIZE_EXT 28

#define EMI_CHN_CONA_DUAL_RANK_EN 0
#define EMI_CHN_CONA_DW32_EN 1
#define EMI_CHN_CONA_ROW_EXT0 2
#define EMI_CHN_CONA_ROW2ND_EXT0 3
#define EMI_CHN_CONA_COL 4
#define EMI_CHN_CONA_COL2ND 6
#define EMI_CHN_CONA_RANK0_SIZE_EXT 8
#define EMI_CHN_CONA_RANK1_SIZE_EXT 9
#define EMI_CHN_CONA_16BANK_MODE 10
#define EMI_CHN_CONA_16BANK_MODE_2ND 11
#define EMI_CHN_CONA_ROW 12
#define EMI_CHN_CONA_ROW2ND 14
#define EMI_CHN_CONA_RANK0_SZ 16
#define EMI_CHN_CONA_RANK1_SZ 20
#define EMI_CHN_CONA_4BANK_MODE 24
#define EMI_CHN_CONA_4BANK_MODE_2ND 25
#define EMI_CHN_CONA_RANK_POS 27
#define EMI_CHN_CONA_BG1_BK3_POS 31

#define MTK_EMI_DRAM_OFFSET 0x40000000
#define MTK_EMI_HASH 0x7
#define MTK_EMI_DISPATCH 0x0
#define MTK_EMI_A2D_VERSION 1

static unsigned int emi_a2d_con_offset[] = {
	/* central EMI CONA, CONF, CONH, CONH_2ND, CONK */
	0x00, 0x28, 0x38, 0x3c, 0x50,
};

static unsigned int emi_a2d_chn_con_offset[] = {
	/* channel EMI CONA, CONC, CONC_2ND */
	0x00, 0x10, 0x14
};

/* global pointer for exported functions */
static struct emi_cen *global_emi_cen;

DEFINE_SPINLOCK(emidbg_lock);

/*
 * prepare_a2d_v1: a helper function to initialize and calculate settings for
 *                 the mtk_emicen_addr2dram_v1() function
 *
 * There is no code comment for the translation. This is intended since
 * the fomular of translation is derived from the implementation of EMI.
 */
static inline void prepare_a2d_v1(struct emi_cen *cen)
{
	const unsigned int mask_4b = 0xf, mask_2b = 0x3;
	void __iomem *emi_cen_base;
	struct a2d_s6s_v1 *s6s;
	unsigned long emi_cona;
	unsigned long emi_conf;
	unsigned long emi_conh;
	unsigned long emi_conh_2nd;
	unsigned long emi_conk;
	unsigned long tmp;

	if (!cen)
		return;

	emi_cen_base = cen->emi_cen_base[0];
	s6s = &cen->a2d_s6s.v1;

	emi_cona = readl(emi_cen_base + emi_a2d_con_offset[0]);
	emi_conf = readl(emi_cen_base + emi_a2d_con_offset[1]);
	emi_conh = readl(emi_cen_base + emi_a2d_con_offset[2]);
	emi_conh_2nd = readl(emi_cen_base + emi_a2d_con_offset[3]);
	emi_conk = readl(emi_cen_base + emi_a2d_con_offset[4]);

	cen->offset = MTK_EMI_DRAM_OFFSET;

	s6s->magics[0] = emi_conf & mask_4b;
	s6s->magics[1] = (emi_conf >> 4) & mask_4b;
	s6s->magics[2] = (emi_conf >> 8) & mask_4b;
	s6s->magics[3] = (emi_conf >> 12) & mask_4b;
	s6s->magics[4] = (emi_conf >> 16) & mask_4b;
	s6s->magics[5] = (emi_conf >> 20) & mask_4b;
	s6s->magics[6] = (emi_conf >> 24) & mask_4b;
	s6s->magics[7] = (emi_conf >> 28) & mask_4b;

	s6s->dw32 = test_bit(EMI_CONA_DW32_EN, &emi_cona) ? 1 : 0;

	s6s->channels = (emi_cona >> EMI_CONA_CHN_EN) & mask_2b;

	s6s->cas = (emi_cona >> EMI_CONA_CAS_SIZE) & mask_2b;
	s6s->cas += s6s->dw32 << 2;
	s6s->cas += ((emi_cona >> EMI_CONA_CAS_SIZE_BIT3) & 1) << 3;
	s6s->cas = s6s->cas << 28;
	s6s->cas = s6s->cas << s6s->channels;

	s6s->dualrk_ch0 = test_bit(EMI_CONA_DUAL_RANK_EN, &emi_cona) ? 1 : 0;
	s6s->dualrk_ch1 = test_bit(EMI_CONA_DUAL_RANK_EN_CHN1, &emi_cona) ? 1 : 0;

	s6s->chn_hash_lsb = 7 + (cen->hash & (~(cen->hash) + 1));
	if (cen->hash)
		s6s->chnpos = s6s->chn_hash_lsb;
	else {
		s6s->chnpos = test_bit(EMI_CONA_CHN_POS_1, &emi_cona) ? 2 : 0;
		s6s->chnpos |= test_bit(EMI_CONA_CHN_POS_0, &emi_cona) ? 1 : 0;
	}

	tmp = (emi_conh >> EMI_CONH_CHNAB_RANK0_SIZE) & mask_4b;
	tmp += ((emi_conk >> EMI_CONK_CHNAB_RANK0_SIZE_EXT) & mask_4b) << 4;
	if (tmp)
		s6s->chab_rk0_sz = tmp << 8;
	else {
		tmp = (emi_cona >> EMI_CONA_COL) & mask_2b;
		tmp += (emi_cona >> EMI_CONA_ROW) & mask_2b;
		tmp += test_bit(EMI_CONA_ROW_EXT0, &emi_cona) ? 4 : 0;
		tmp += s6s->dw32;
		tmp += 7;
		s6s->chab_rk0_sz = 1 << tmp;
	}

	tmp = (emi_conh >> EMI_CONH_CHNAB_RANK1_SIZE) & mask_4b;
	tmp += ((emi_conk >> EMI_CONK_CHNAB_RANK1_SIZE_EXT) & mask_4b) << 4;
	if (tmp)
		s6s->chab_rk1_sz = tmp << 8;
	else if (!test_bit(EMI_CONA_DUAL_RANK_EN, &emi_cona))
		s6s->chab_rk1_sz = 0;
	else {
		tmp = (emi_cona >> EMI_CONA_COL2ND) & mask_2b;
		tmp += (emi_cona >> EMI_CONA_ROW2ND) & mask_2b;
		tmp += test_bit(EMI_CONA_ROW2ND_EXT0, &emi_cona) ? 4 : 0;
		tmp += s6s->dw32;
		tmp += 7;
		s6s->chab_rk1_sz = 1 << tmp;
	}

	tmp = (emi_conh >> EMI_CONH_CHNCD_RANK0_SIZE) & mask_4b;
	tmp += ((emi_conk >> EMI_CONK_CHNCD_RANK0_SIZE_EXT) & mask_4b) << 4;
	if (tmp)
		s6s->chcd_rk0_sz = tmp << 8;
	else {
		tmp = (emi_cona >> EMI_CONA_CHN1_COL) & mask_2b;
		tmp += (emi_cona >> EMI_CONA_CHN1_ROW) & mask_2b;
		tmp += test_bit(EMI_CONH_CHN1_ROW_EXT0, &emi_conh) ? 4 : 0;
		tmp += s6s->dw32;
		tmp += 7;
		s6s->chcd_rk0_sz = 1 << tmp;
	}

	tmp = (emi_conh >> EMI_CONH_CHNCD_RANK1_SIZE) & mask_4b;
	tmp += ((emi_conk >> EMI_CONK_CHNCD_RANK1_SIZE_EXT) & mask_4b) << 4;
	if (tmp)
		s6s->chcd_rk1_sz = tmp << 8;
	else if (!test_bit(EMI_CONA_DUAL_RANK_EN_CHN1, &emi_cona))
		s6s->chcd_rk1_sz = 0;
	else {
		tmp = (emi_cona >> EMI_CONA_CHN1_COL2ND) & mask_2b;
		tmp += (emi_cona >> EMI_CONA_CHN1_ROW2ND) & mask_2b;
		tmp += test_bit(EMI_CONH_CHN1_ROW2ND_EXT0, &emi_conh) ? 4 : 0;
		tmp += s6s->dw32;
		tmp += 7;
		s6s->chcd_rk1_sz = 1 << tmp;
	}

	cen->max = s6s->chab_rk0_sz + s6s->chab_rk1_sz;
	cen->max += s6s->chcd_rk0_sz + s6s->chcd_rk0_sz;
	if ((s6s->channels > 1) || (cen->disph > 0))
		cen->max *= 2;
	cen->max = cen->max << 20;

	s6s->chab_row_mask[0] = (emi_cona >> EMI_CONA_ROW) & mask_2b;
	s6s->chab_row_mask[0] += test_bit(EMI_CONA_ROW_EXT0, &emi_cona) ? 4 : 0;
	s6s->chab_row_mask[0] += 13;
	s6s->chab_row_mask[1] = (emi_cona >> EMI_CONA_ROW2ND) & mask_2b;
	s6s->chab_row_mask[1] += test_bit(EMI_CONA_ROW2ND_EXT0, &emi_cona) ? 4 : 0;
	s6s->chab_row_mask[1] += 13;

	s6s->chcd_row_mask[0] = (emi_cona >> EMI_CONA_CHN1_ROW) & mask_2b;
	s6s->chcd_row_mask[0] += test_bit(EMI_CONH_CHN1_ROW_EXT0, &emi_conh) ? 4 : 0;
	s6s->chcd_row_mask[0] += 13;
	s6s->chcd_row_mask[1] = (emi_cona >> EMI_CONA_CHN1_ROW2ND) & mask_2b;
	s6s->chcd_row_mask[1] += test_bit(EMI_CONH_CHN1_ROW2ND_EXT0, &emi_conh) ? 4 : 0;
	s6s->chcd_row_mask[1] += 13;

	s6s->chab_col_mask[0] = (emi_cona >> EMI_CONA_COL) & mask_2b;
	s6s->chab_col_mask[0] += 9;
	s6s->chab_col_mask[1] = (emi_cona >> EMI_CONA_COL2ND) & mask_2b;
	s6s->chab_col_mask[1] += 9;

	s6s->chcd_col_mask[0] = (emi_cona >> EMI_CONA_CHN1_COL) & mask_2b;
	s6s->chcd_col_mask[0] += 9;
	s6s->chcd_col_mask[1] = (emi_cona >> EMI_CONA_CHN1_COL2ND) & mask_2b;
	s6s->chcd_col_mask[1] += 9;

	s6s->chn_4bank_mode = test_bit(EMI_CONH_2ND_CHN_4BANK_MODE, &emi_conh_2nd) ? 1 : 0;
}

/*
 * use_a2d_magic_v1: a helper function to calculate the input address
 *                   for the mtk_emicen_addr2dram_v1() function
 *
 * There is no code comment for the translation. This is intended since
 * the fomular of translation is derived from the implementation of EMI.
 */
static inline unsigned int use_a2d_magic_v1(unsigned long addr,
						unsigned int bit)
{
	unsigned long magic;
	unsigned int ret;

	if (!global_emi_cen)
		return 0;

	magic = global_emi_cen->a2d_s6s.v1.magics[((bit >= 9) & (bit <= 16)) ?
						(bit - 9) : 0];

	ret = test_bit(bit, &addr) ? 1 : 0;
	ret ^= (test_bit(16, &addr) && test_bit(0, &magic)) ? 1 : 0;
	ret ^= (test_bit(17, &addr) && test_bit(1, &magic)) ? 1 : 0;
	ret ^= (test_bit(18, &addr) && test_bit(2, &magic)) ? 1 : 0;
	ret ^= (test_bit(19, &addr) && test_bit(3, &magic)) ? 1 : 0;

	return ret;
}

/*
 * mtk_emicen_addr2dram_v1 - Translate a physical address to
 *                           a DRAM-point-of-view map for EMI v1
 * @addr - input physical address
 * @map - output map stored in struct emi_addr_map
 *
 * Return 0 on success, -1 on failures.
 *
 * There is no code comment for the translation. This is intended since
 * the fomular of translation is derived from the implementation of EMI.
 */
static int mtk_emicen_addr2dram_v1(unsigned long addr,
					struct emi_addr_map *map)
{
	struct a2d_s6s_v1 *s6s;
	unsigned long disph, hash;
	unsigned long saddr, bfraddr, chnaddr;
	unsigned long max_rk0_sz;
	unsigned int tmp;
	unsigned int chn_hash_lsb, row_mask, col_mask;
	bool ch_ab_not_cd;

	if (!global_emi_cen)
		return -1;
	if (!map)
		return -1;
	if (addr < global_emi_cen->offset)
		return -1;
	if (addr > global_emi_cen->max)
		return -1;

	addr -= global_emi_cen->offset;

	map->emi = -1;
	map->channel = -1;
	map->rank = -1;
	map->bank = -1;
	map->row = -1;
	map->column = -1;

	s6s = &global_emi_cen->a2d_s6s.v1;
	disph = global_emi_cen->disph;
	hash = global_emi_cen->hash;
	chn_hash_lsb = s6s->chn_hash_lsb;

	tmp = (test_bit(8, &addr) & test_bit(0, &disph)) ? 1 : 0;
	tmp ^= (test_bit(9, &addr) & test_bit(1, &disph)) ? 1 : 0;
	tmp ^= (test_bit(10, &addr) & test_bit(2, &disph)) ? 1 : 0;
	tmp ^= (test_bit(11, &addr) & test_bit(3, &disph)) ? 1 : 0;
	map->emi = tmp;

	saddr = addr;
	clear_bit(9, &saddr);
	clear_bit(10, &saddr);
	clear_bit(11, &saddr);
	clear_bit(12, &saddr);
	clear_bit(13, &saddr);
	clear_bit(14, &saddr);
	clear_bit(15, &saddr);
	clear_bit(16, &saddr);
	saddr |= use_a2d_magic_v1(addr, 9) << 9;
	saddr |= use_a2d_magic_v1(addr, 10) << 10;
	saddr |= use_a2d_magic_v1(addr, 11) << 11;
	saddr |= use_a2d_magic_v1(addr, 12) << 12;
	saddr |= use_a2d_magic_v1(addr, 13) << 13;
	saddr |= use_a2d_magic_v1(addr, 14) << 14;
	saddr |= use_a2d_magic_v1(addr, 15) << 15;
	saddr |= use_a2d_magic_v1(addr, 16) << 16;

	if (global_emi_cen->disph <= 0)
		bfraddr = saddr;
	else {
		tmp = 7 + __ffs(disph);
		bfraddr = (saddr >> (tmp + 1)) << tmp;
		bfraddr += saddr & ((1 << tmp) - 1);
	}

	if (bfraddr < s6s->cas)
		return -1;

	if (!s6s->channels)
		map->channel = s6s->channels;
	else if (hash) {
		tmp = (test_bit(8, &addr) && test_bit(0, &hash)) ? 1 : 0;
		tmp ^= (test_bit(9, &addr) && test_bit(1, &hash)) ? 1 : 0;
		tmp ^= (test_bit(10, &addr) && test_bit(2, &hash)) ? 1 : 0;
		tmp ^= (test_bit(11, &addr) && test_bit(3, &hash)) ? 1 : 0;
		map->channel = tmp;
	} else {
		if (s6s->channels == 1) {
			tmp = 0;
			switch (s6s->chnpos) {
			case 0:
				tmp = 7;
				break;
			case 1:
				tmp = 8;
				break;
			case 2:
				tmp = 9;
				break;
			case 3:
				tmp = 12;
				break;
			default:
				return -1;
			}
			map->channel = (bfraddr >> tmp) % 2;
		} else if (s6s->channels == 2) {
			tmp = 0;
			switch (s6s->chnpos) {
			case 0:
				tmp = 7;
				break;
			case 1:
				tmp = 8;
				break;
			case 2:
				tmp = 9;
				break;
			case 3:
				tmp = 12;
				break;
			default:
				return -1;
			}
			map->channel = (bfraddr >> tmp) % 4;
		} else {
			return -1;
		}
	}

	if (map->channel > 1)
		ch_ab_not_cd = 0;
	else {
		if (map->channel == 1)
			ch_ab_not_cd = (s6s->channels > 1) ?  1 : 0;
		else
			ch_ab_not_cd = 1;
	}

	max_rk0_sz = (ch_ab_not_cd) ?  s6s->chab_rk0_sz : s6s->chcd_rk0_sz;
	max_rk0_sz = max_rk0_sz << 20;

	if (!s6s->channels)
		chnaddr = bfraddr;
	else if (s6s->chnpos > 3) {
		tmp = chn_hash_lsb;
		chnaddr = bfraddr >> (tmp + 1);
		chnaddr = chnaddr << tmp;
		chnaddr += bfraddr & ((1 << tmp) - 1);
	} else if (s6s->channels == 1 ||
			s6s->channels == 2) {
		tmp = 0;
		switch (s6s->chnpos) {
		case 0:
			tmp = 7;
			break;
		case 1:
			tmp = 8;
			break;
		case 2:
			tmp = 9;
			break;
		case 3:
			tmp = 12;
			break;
		default:
			break;
		}
		chnaddr = bfraddr >> (tmp + (s6s->channels - 1));
		chnaddr = chnaddr << tmp;
		chnaddr += bfraddr & ((1 << tmp) - 1);
	} else {
		return -1;
	}

	if ((map->channel) ?  !s6s->dualrk_ch1 : !s6s->dualrk_ch0)
		map->rank = 0;
	else {
		if (chnaddr > max_rk0_sz)
			map->rank = 1;
		else
			map->rank = 0;
	}

	row_mask = (ch_ab_not_cd) ?
			((map->rank) ?
				s6s->chab_row_mask[1] : s6s->chab_row_mask[0]) :
			((map->rank) ?
				s6s->chcd_row_mask[1] : s6s->chcd_row_mask[0]);
	col_mask = (ch_ab_not_cd) ?
			((map->rank) ?
				s6s->chab_col_mask[1] : s6s->chab_col_mask[0]) :
			((map->rank) ?
				s6s->chcd_col_mask[1] : s6s->chcd_col_mask[0]);

	tmp = chnaddr - (max_rk0_sz * map->rank);
	tmp /= 1 << (s6s->dw32 + 1 + col_mask + 3);
	tmp &= (1 << row_mask) - 1;
	map->row = tmp;

	tmp = chnaddr;
	tmp /= 1 << (s6s->dw32 + 1 + col_mask);
	tmp &= ((!s6s->chn_4bank_mode) ? 8 : 4) - 1;
	map->bank = tmp;

	tmp = chnaddr;
	tmp /= 1 << (s6s->dw32 + 1);
	tmp &= (1 << col_mask) - 1;
	map->column = tmp;

	return 0;
}
/*
 * prepare_a2d_v2: a helper function to initialize and calculate settings for
 *                 the mtk_emicen_addr2dram_v2() function
 *
 * There is no code comment for the translation. This is intended since
 * the fomular of translation is derived from the implementation of EMI.
 */
static inline void prepare_a2d_v2(struct emi_cen *cen)
{
	const unsigned int mask_4b = 0xf, mask_2b = 0x3;
	struct a2d_s6s_v2 *s6s;
	void __iomem *emi_cen_base, *emi_chn_base;
	unsigned long emi_cona, emi_conf, emi_conh, emi_conh_2nd, emi_conk;
	unsigned long emi_chn_cona, emi_chn_conc, emi_chn_conc_2nd;
	int tmp;
	int col, col2nd, row, row2nd, row_ext0, row2nd_ext0;
	int rank0_size, rank1_size, rank0_size_ext, rank1_size_ext;
	int chn_4bank_mode, chn_bg_16bank_mode, chn_bg_16bank_mode_2nd;
	int b11s, b12s, b13s, b14s, b15s, b16s;
	int b8s, b11s_ext, b12s_ext, b13s_ext, b14s_ext, b15s_ext, b16s_ext;
	unsigned long ch0_rk0_sz, ch0_rk1_sz;
	unsigned long ch1_rk0_sz, ch1_rk1_sz;

	if (!cen)
		return;

	s6s = &cen->a2d_s6s.v2;
	cen->offset = MTK_EMI_DRAM_OFFSET;

	emi_cen_base = cen->emi_cen_base[0];
	emi_cona = readl(emi_cen_base + emi_a2d_con_offset[0]);
	emi_conf = readl(emi_cen_base + emi_a2d_con_offset[1]);
	emi_conh = readl(emi_cen_base + emi_a2d_con_offset[2]);
	emi_conh_2nd = readl(emi_cen_base + emi_a2d_con_offset[3]);
	emi_conk = readl(emi_cen_base + emi_a2d_con_offset[4]);

	emi_chn_base = cen->emi_chn_base[0];
	emi_chn_cona = readl(emi_chn_base + emi_a2d_chn_con_offset[0]);
	emi_chn_conc = readl(emi_chn_base + emi_a2d_chn_con_offset[1]);
	emi_chn_conc_2nd = readl(emi_chn_base + emi_a2d_chn_con_offset[2]);

	tmp = (emi_cona >> EMI_CONA_CHN_POS_0) & mask_2b;
	switch (tmp) {
	case 3:
		s6s->chn_bit_position = 12;
		break;
	case 2:
		s6s->chn_bit_position = 9;
		break;
	case 1:
		s6s->chn_bit_position = 8;
		break;
	default:
		s6s->chn_bit_position = 7;
		break;
	}

	s6s->chn_en = (emi_cona >> EMI_CONA_CHN_EN) & mask_2b;

	s6s->magics[0] = emi_conf & mask_4b;
	s6s->magics[1] = (emi_conf >> 4) & mask_4b;
	s6s->magics[2] = (emi_conf >> 8) & mask_4b;
	s6s->magics[3] = (emi_conf >> 12) & mask_4b;
	s6s->magics[4] = (emi_conf >> 16) & mask_4b;
	s6s->magics[5] = (emi_conf >> 20) & mask_4b;
	s6s->magics[6] = (emi_conf >> 24) & mask_4b;
	s6s->magics[7] = (emi_conf >> 28) & mask_4b;


	s6s->dual_rank_en =
		test_bit(EMI_CHN_CONA_DUAL_RANK_EN, &emi_chn_cona) ?  1 : 0;
	s6s->dw32_en = test_bit(EMI_CHN_CONA_DW32_EN, &emi_chn_cona) ? 1 : 0;
	row_ext0 = test_bit(EMI_CHN_CONA_ROW_EXT0, &emi_chn_cona) ? 1 : 0;
	row2nd_ext0 = test_bit(EMI_CHN_CONA_ROW2ND_EXT0, &emi_chn_cona) ? 1 : 0;
	col = (emi_chn_cona >> EMI_CHN_CONA_COL) & mask_2b;
	col2nd = (emi_chn_cona >> EMI_CHN_CONA_COL2ND) & mask_2b;
	rank0_size_ext =
		test_bit(EMI_CHN_CONA_RANK0_SIZE_EXT, &emi_chn_cona) ? 1 : 0;
	rank1_size_ext =
		test_bit(EMI_CHN_CONA_RANK1_SIZE_EXT, &emi_chn_cona) ? 1 : 0;
	chn_bg_16bank_mode =
		test_bit(EMI_CHN_CONA_16BANK_MODE, &emi_chn_cona) ? 1 : 0;
	chn_bg_16bank_mode_2nd =
		test_bit(EMI_CHN_CONA_16BANK_MODE_2ND, &emi_chn_cona) ? 1 : 0;
	row = (emi_chn_cona >> EMI_CHN_CONA_ROW) & mask_2b;
	row2nd = (emi_chn_cona >> EMI_CHN_CONA_ROW2ND) & mask_2b;
	rank0_size = (emi_chn_cona >> EMI_CHN_CONA_RANK0_SZ) & mask_4b;
	rank1_size = (emi_chn_cona >> EMI_CHN_CONA_RANK1_SZ) & mask_4b;
	chn_4bank_mode =
		test_bit(EMI_CHN_CONA_4BANK_MODE, &emi_chn_cona) ? 1 : 0;
	s6s->rank_pos = test_bit(EMI_CHN_CONA_RANK_POS, &emi_chn_cona) ? 1 : 0;
	s6s->bg1_bk3_pos =
		test_bit(EMI_CHN_CONA_BG1_BK3_POS, &emi_chn_cona) ? 1 : 0;

	b11s = (emi_chn_conc >> 8) & mask_4b;
	b12s = (emi_chn_conc >> 12) & mask_4b;
	b13s = (emi_chn_conc >> 16) & mask_4b;
	b14s = (emi_chn_conc >> 20) & mask_4b;
	b15s = (emi_chn_conc >> 24) & mask_4b;
	b16s = (emi_chn_conc >> 28) & mask_4b;

	b11s_ext = (emi_chn_conc_2nd >> 4) & mask_2b;
	b12s_ext = (emi_chn_conc_2nd >> 6) & mask_2b;
	b13s_ext = (emi_chn_conc_2nd >> 8) & mask_2b;
	b14s_ext = (emi_chn_conc_2nd >> 10) & mask_2b;
	b15s_ext = (emi_chn_conc_2nd >> 12) & mask_2b;
	b16s_ext = (emi_chn_conc_2nd >> 14) & mask_2b;
	b8s = (emi_chn_conc_2nd >> 16) & mask_2b;

	s6s->magics2[0] = b8s;
	s6s->magics2[1] = b11s_ext * 16 + b11s;
	s6s->magics2[2] = b12s_ext * 16 + b12s;
	s6s->magics2[3] = b13s_ext * 16 + b13s;
	s6s->magics2[4] = b14s_ext * 16 + b14s;
	s6s->magics2[5] = b15s_ext * 16 + b15s;
	s6s->magics2[6] = b16s_ext * 16 + b16s;

	s6s->rank0_row_width = row_ext0 * 4 + row + 13;
	s6s->rank0_bank_width = (chn_bg_16bank_mode == 1) ? 4 :
				(chn_4bank_mode == 1) ? 2 : 3;
	s6s->rank0_col_width = col + 9;
	s6s->rank0_bg_16bank_mode = chn_bg_16bank_mode;
	s6s->rank0_size_MB = (rank0_size_ext * 16 + rank0_size) * 256;
	if (!(s6s->rank0_size_MB)) {
		tmp = s6s->rank0_row_width + s6s->rank0_bank_width;
		tmp += s6s->rank0_col_width + s6s->dw32_en;
		s6s->rank0_size_MB = 2 << (tmp - 20);
	}

	s6s->rank1_row_width = row2nd_ext0 * 4 + row2nd + 13;
	s6s->rank1_bank_width = (chn_bg_16bank_mode_2nd == 1) ? 4 :
				(chn_4bank_mode == 1) ? 2 : 3;
	s6s->rank1_col_width = col2nd + 9;
	s6s->rank1_bg_16bank_mode = chn_bg_16bank_mode_2nd;
	s6s->rank1_size_MB = (rank1_size_ext * 16 + rank1_size) * 256;
	if (!(s6s->rank1_size_MB)) {
		tmp = s6s->rank1_row_width + s6s->rank1_bank_width;
		tmp += s6s->rank1_col_width + s6s->dw32_en;
		s6s->rank1_size_MB = 2 << (tmp - 20);
	}
	if (s6s->rank0_size_MB)
		ch0_rk0_sz = s6s->rank0_size_MB;
	else {
		tmp = s6s->rank0_row_width + s6s->rank0_bank_width;
		tmp += s6s->rank0_col_width + s6s->dw32_en ? 2 : 1;
		tmp -= 20;
		ch0_rk0_sz = 1 << tmp;
	}
	ch1_rk0_sz = ch0_rk0_sz;
	if (s6s->rank1_size_MB)
		ch0_rk1_sz = s6s->rank1_size_MB;
	else {
		tmp = s6s->rank1_row_width + s6s->rank1_bank_width;
		tmp += s6s->rank1_col_width + s6s->dw32_en ? 2 : 1;
		tmp -= 20;
		ch0_rk1_sz = 1 << tmp;
	}
	ch1_rk1_sz = ch0_rk1_sz;

	cen->max = ch0_rk0_sz;
	if (s6s->dual_rank_en)
		cen->max += ch0_rk1_sz;
	if (s6s->chn_en)
		cen->max += ch1_rk0_sz + ((s6s->dual_rank_en) ? ch1_rk1_sz : 0);
	if (cen->disph)
		cen->max *= 2;
	cen->max = cen->max << 20;
}

/*
 * use_a2d_magic_v2: a helper function to calculate the input address
 *                   for the mtk_emicen_addr2dram_v2() function
 *
 * There is no code comment for the translation. This is intended since
 * the fomular of translation is derived from the implementation of EMI.
 */
static inline unsigned int use_a2d_magic_v2(unsigned long addr,
						unsigned long magic,
						unsigned int bit)
{
	unsigned int ret;

	ret = test_bit(bit, &addr) ? 1 : 0;
	ret ^= (test_bit(16, &addr) & test_bit(0, &magic)) ? 1 : 0;
	ret ^= (test_bit(17, &addr) & test_bit(1, &magic)) ? 1 : 0;
	ret ^= (test_bit(18, &addr) & test_bit(2, &magic)) ? 1 : 0;
	ret ^= (test_bit(19, &addr) & test_bit(3, &magic)) ? 1 : 0;
	ret ^= (test_bit(20, &addr) & test_bit(4, &magic)) ? 1 : 0;
	ret ^= (test_bit(21, &addr) & test_bit(5, &magic)) ? 1 : 0;

	return ret;
}

/*
 * a2d_rm_bit: a helper function to calculate the input address
 *             for the mtk_emicen_addr2dram_v2() function
 *
 * There is no code comment for the translation. This is intended since
 * the fomular of translation is derived from the implementation of EMI.
 */
static inline unsigned long a2d_rm_bit(unsigned long taddr, int bit)
{
	unsigned long ret;

	ret = taddr;
	clear_bit(bit, &ret);

	ret = ret >> (bit + 1);
	ret = ret << bit;
	ret = ret & ~((1UL << bit) - 1);

	ret = ret | (taddr & ((1UL << bit) - 1));

	return ret;
}

/*
 * mtk_emicen_addr2dram_v2 - Translate a physical address to
 *                           a DRAM-point-of-view map for EMI v2
 * @addr - input physical address
 * @map - output map stored in struct emi_addr_map
 *
 * Return 0 on success, -1 on failures.
 *
 * There is no code comment for the translation. This is intended since
 * the fomular of translation is derived from the implementation of EMI.
 */
static int mtk_emicen_addr2dram_v2(unsigned long addr,
					struct emi_addr_map *map)
{
	struct a2d_s6s_v2 *s6s;
	unsigned long disph, hash;
	unsigned long saddr, taddr, bgaddr, noraddr;
	unsigned long tmp;
	int emi_tpos, chn_tpos;

	if (!global_emi_cen)
		return -1;
	if (!map)
		return -1;
	if (addr < global_emi_cen->offset)
		return -1;

	addr -= global_emi_cen->offset;
	if (addr > global_emi_cen->max)
		return -1;

	map->emi = -1;
	map->channel = -1;
	map->rank = -1;
	map->bank = -1;
	map->row = -1;
	map->column = -1;

	s6s = &global_emi_cen->a2d_s6s.v2;
	disph = global_emi_cen->disph;
	hash = global_emi_cen->hash;

	saddr = addr;
	clear_bit(9, &saddr);
	clear_bit(10, &saddr);
	clear_bit(11, &saddr);
	clear_bit(12, &saddr);
	clear_bit(13, &saddr);
	clear_bit(14, &saddr);
	clear_bit(15, &saddr);
	clear_bit(16, &saddr);
	saddr |= use_a2d_magic_v2(addr, s6s->magics[0], 9) << 9;
	saddr |= use_a2d_magic_v2(addr, s6s->magics[1], 10) << 10;
	saddr |= use_a2d_magic_v2(addr, s6s->magics[2], 11) << 11;
	saddr |= use_a2d_magic_v2(addr, s6s->magics[3], 12) << 12;
	saddr |= use_a2d_magic_v2(addr, s6s->magics[4], 13) << 13;
	saddr |= use_a2d_magic_v2(addr, s6s->magics[5], 14) << 14;
	saddr |= use_a2d_magic_v2(addr, s6s->magics[6], 15) << 15;
	saddr |= use_a2d_magic_v2(addr, s6s->magics[7], 16) << 16;

	if (!hash) {
		map->channel = test_bit(s6s->chn_bit_position, &saddr) ? 1 : 0;

		chn_tpos = s6s->chn_bit_position;
	} else {
		tmp = (test_bit(8, &saddr) && test_bit(0, &hash)) ? 1 : 0;
		tmp ^= (test_bit(9, &saddr) && test_bit(1, &hash)) ? 1 : 0;
		tmp ^= (test_bit(10, &saddr) && test_bit(2, &hash)) ? 1 : 0;
		tmp ^= (test_bit(11, &saddr) && test_bit(3, &hash)) ? 1 : 0;
		map->channel = tmp;

		if (test_bit(0, &hash))
			chn_tpos = 8;
		else if (test_bit(1, &hash))
			chn_tpos = 9;
		else if (test_bit(2, &hash))
			chn_tpos = 10;
		else if (test_bit(3, &hash))
			chn_tpos = 11;
		else
			chn_tpos = -1;
	}

	if (!disph) {
		map->emi = 0;

		emi_tpos = -1;
	} else {
		tmp = (test_bit(8, &saddr) && test_bit(0, &disph)) ? 1 : 0;
		tmp ^= (test_bit(9, &saddr) && test_bit(1, &disph)) ? 1 : 0;
		tmp ^= (test_bit(10, &saddr) && test_bit(2, &disph)) ? 1 : 0;
		tmp ^= (test_bit(11, &saddr) && test_bit(3, &disph)) ? 1 : 0;
		map->emi = tmp;

		if (test_bit(0, &disph))
			emi_tpos = 8;
		else if (test_bit(1, &disph))
			emi_tpos = 9;
		else if (test_bit(2, &disph))
			emi_tpos = 10;
		else if (test_bit(3, &disph))
			emi_tpos = 11;
		else
			emi_tpos = -1;
	}

	taddr = saddr;
	if (!disph) {
		if (!s6s->chn_en)
			taddr = saddr;
		else
			taddr = a2d_rm_bit(taddr, chn_tpos);
	} else {
		if ((chn_tpos < 0) || (emi_tpos < 0))
			return -1;
		if (!s6s->chn_en)
			taddr = a2d_rm_bit(taddr, emi_tpos);
		else if (emi_tpos > chn_tpos) {
			taddr = a2d_rm_bit(taddr, emi_tpos);
			taddr = a2d_rm_bit(taddr, chn_tpos);
		} else {
			taddr = a2d_rm_bit(taddr, chn_tpos);
			taddr = a2d_rm_bit(taddr, emi_tpos);
		}
	}

	saddr = taddr;
	clear_bit(8, &saddr);
	clear_bit(11, &saddr);
	clear_bit(12, &saddr);
	clear_bit(13, &saddr);
	clear_bit(14, &saddr);
	clear_bit(15, &saddr);
	clear_bit(16, &saddr);
	saddr |= use_a2d_magic_v2(taddr, s6s->magics2[0], 8) << 8;
	saddr |= use_a2d_magic_v2(taddr, s6s->magics2[1], 11) << 11;
	saddr |= use_a2d_magic_v2(taddr, s6s->magics2[2], 12) << 12;
	saddr |= use_a2d_magic_v2(taddr, s6s->magics2[3], 13) << 13;
	saddr |= use_a2d_magic_v2(taddr, s6s->magics2[4], 14) << 14;
	saddr |= use_a2d_magic_v2(taddr, s6s->magics2[5], 15) << 15;
	saddr |= use_a2d_magic_v2(taddr, s6s->magics2[6], 16) << 16;

	if (!s6s->dual_rank_en)
		map->rank = 0;
	else {
		if (!s6s->rank_pos)
			map->rank = ((saddr >> 20) > s6s->rank0_size_MB) ?
					1 : 0;
		else {
			tmp = 1 + s6s->dw32_en;
			tmp += s6s->rank0_col_width + s6s->rank0_bank_width;
			map->rank = saddr >> tmp;
		}
	}

	tmp = (map->rank)
		? s6s->rank1_bg_16bank_mode
		: s6s->rank0_bg_16bank_mode;
	if (tmp) {
		bgaddr = a2d_rm_bit(saddr, 8);
		map->column = (bgaddr >> (1 + s6s->dw32_en))
			% (1 << ((map->rank)
				? s6s->rank1_col_width
				: s6s->rank0_col_width));

		tmp = (map->rank) ? s6s->rank1_col_width : s6s->rank0_col_width;
		tmp = (bgaddr >> (1 + s6s->dw32_en + tmp))
			% (1 << ((map->rank)
				? s6s->rank1_bank_width - 1
				: s6s->rank0_bank_width - 1));
		map->bank = test_bit((s6s->bg1_bk3_pos) ? 0 : 1, &tmp) ? 1 : 0;
		map->bank += test_bit((s6s->bg1_bk3_pos) ? 1 : 2, &tmp) ? 2 : 0;
		map->bank += test_bit(8, &saddr) ? 4 : 0;
		map->bank += test_bit((s6s->bg1_bk3_pos) ? 2 : 0, &tmp) ? 8 : 0;
	} else {
		map->column = (saddr >> (1 + s6s->dw32_en))
			% (1 << ((map->rank)
				? s6s->rank1_col_width
				: s6s->rank0_col_width));

		tmp = (map->rank) ? s6s->rank1_col_width : s6s->rank0_col_width;
		map->bank = (saddr >> (1 + s6s->dw32_en + tmp))
			% (1 << ((map->rank)
				? s6s->rank1_bank_width
				: s6s->rank0_bank_width));
	}

	if (!s6s->rank_pos) {
		noraddr = (map->rank) ?
			saddr - (s6s->rank0_size_MB << 20) : saddr;
	} else {
		tmp = 1 + s6s->dw32_en;
		tmp += (map->rank) ?
			s6s->rank1_bank_width : s6s->rank0_bank_width;
		tmp += (map->rank) ?
			s6s->rank1_col_width : s6s->rank0_col_width;
		noraddr = a2d_rm_bit(saddr, tmp);
	}
	tmp = 1 + s6s->dw32_en;
	tmp += (map->rank) ? s6s->rank1_bank_width : s6s->rank0_bank_width;
	tmp += (map->rank) ? s6s->rank1_col_width : s6s->rank0_col_width;
	noraddr = noraddr >> tmp;
	tmp = (map->rank) ? s6s->rank1_row_width : s6s->rank0_row_width;
	map->row = noraddr % (1 << tmp);

	return 0;
}

/*
 * mtk_emicen_addr2dram - Translate a physical address to
			a DRAM-point-of-view map
 * @addr - input physical address
 * @map - output map stored in struct emi_addr_map
 *
 * Return 0 on success, -1 on failures.
 *
 * There is no code comment for the translation. This is intended since
 * the fomular of translation is derived from the implementation of EMI.
 */
int mtk_emicen_addr2dram(unsigned long addr, struct emi_addr_map *map)
{
	if (!global_emi_cen)
		return -1;

	if (global_emi_cen->ver == 1)
		return mtk_emicen_addr2dram_v1(addr, map);
	else
		return mtk_emicen_addr2dram_v2(addr, map);

}
EXPORT_SYMBOL(mtk_emicen_addr2dram);

static ssize_t emicen_addr2dram_show(struct device_driver *driver, char *buf)
{
	int ret;
	struct emi_addr_map map;
	unsigned long addr;

	if (!global_emi_cen)
		return 0;

	addr = global_emi_cen->a2d_addr;

	ret = mtk_emicen_addr2dram(addr, &map);
	if (!ret)
		return snprintf(buf, PAGE_SIZE,
		     "0x%lx\n->\nemi%d\nchn%d\nrank%d\nbank%d\nrow%d\ncol%d\n",
		     addr, map.emi, map.channel, map.rank,
		     map.bank, map.row, map.column);
	else
		return snprintf(buf, PAGE_SIZE, "0x%lx\n->failed\n", addr);
}

static ssize_t emicen_addr2dram_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	u64 addr;
	int ret;

	if (!global_emi_cen)
		return count;

	ret = kstrtou64(buf, 16, &addr);
	if (ret)
		return ret;

	global_emi_cen->a2d_addr = (unsigned long)addr;

	return count;
}

static DRIVER_ATTR_RW(emicen_addr2dram);

static int emicen_probe(struct platform_device *pdev)
{
	struct device_node *emicen_node = pdev->dev.of_node;
	struct device_node *emichn_node =
		of_parse_phandle(emicen_node, "mediatek,emi-reg", 0);
	struct emi_cen *cen;
	unsigned int i;
	int ret;
	int emi_cen_cnt_temp;

	pr_info("%s: module probe.\n", __func__);

	cen = devm_kzalloc(&pdev->dev,
		sizeof(struct emi_cen), GFP_KERNEL);
	if (!cen)
		return -ENOMEM;

	cen->ver = (int)of_device_get_match_data(&pdev->dev);

	ret = of_property_read_u32(emicen_node,
		"ch_cnt", &(cen->ch_cnt));
	if (ret) {
		pr_info("%s: get ch_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(emicen_node,
		"rk_cnt", &(cen->rk_cnt));
	if (ret) {
		pr_info("%s: get rk_cnt fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: %s(%d), %s(%d)\n", __func__,
		"ch_cnt", cen->ch_cnt,
		"rk_cnt", cen->rk_cnt);

	cen->rk_size = devm_kmalloc_array(&pdev->dev,
		cen->rk_cnt, sizeof(unsigned long long),
		GFP_KERNEL);
	if (!(cen->rk_size))
		return -ENOMEM;
	ret = of_property_read_u64_array(emicen_node,
		"rk_size", cen->rk_size, cen->rk_cnt);

	for (i = 0; i < cen->rk_cnt; i++)
		pr_info("%s: rk_size%d(0x%llx)\n", __func__,
			i, cen->rk_size[i]);

	emi_cen_cnt_temp = of_property_count_elems_of_size(
		emicen_node, "reg", sizeof(unsigned int) * 4);
	if (emi_cen_cnt_temp <= 0) {
		pr_info("%s: get emi_cen_cnt fail\n", __func__);
		return -EINVAL;
	} else
		cen->emi_cen_cnt = (unsigned int)emi_cen_cnt_temp;

	cen->emi_cen_base = devm_kmalloc_array(&pdev->dev,
		cen->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(cen->emi_cen_base))
		return -ENOMEM;
	for (i = 0; i < cen->emi_cen_cnt; i++)
		cen->emi_cen_base[i] = of_iomap(emicen_node, i);

	cen->emi_chn_base = devm_kmalloc_array(&pdev->dev,
		cen->ch_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(cen->emi_chn_base))
		return -ENOMEM;
	for (i = 0; i < cen->ch_cnt; i++)
		cen->emi_chn_base[i] = of_iomap(emichn_node, i);

	ret = of_property_read_u32(emicen_node,
		"a2d_disph", &(cen->disph));
	if (ret) {
		dev_info(&pdev->dev, "No a2d_disph\n");
		cen->disph = MTK_EMI_DISPATCH;
	}

	ret = of_property_read_u32(emicen_node,
		"a2d_hash", &(cen->hash));
	if (ret) {
		dev_info(&pdev->dev, "No a2d_hash\n");
		cen->hash = MTK_EMI_HASH;
	}

	ret = of_property_read_u32_array(emicen_node,
		"a2d_conf_offset", emi_a2d_con_offset,
		ARRAY_SIZE(emi_a2d_con_offset));
	if (ret)
		dev_info(&pdev->dev, "No a2d_conf_offset\n");

	ret = of_property_read_u32_array(emicen_node,
		"a2d_chn_conf_offset", emi_a2d_chn_con_offset,
		ARRAY_SIZE(emi_a2d_chn_con_offset));
	if (ret)
		dev_info(&pdev->dev, "No a2d_chn_conf_offset\n");

	if (cen->ver == 1)
		prepare_a2d_v1(cen);
	else if (cen->ver == 2)
		prepare_a2d_v2(cen);
	else
		return -ENXIO;

	global_emi_cen = cen;

	dev_info(&pdev->dev, "%s(%d) %s(%d), %s(%d)\n",
		"version", cen->ver,
		"ch_cnt", cen->ch_cnt,
		"rk_cnt", cen->rk_cnt);

	for (i = 0; i < cen->rk_cnt; i++)
		dev_info(&pdev->dev, "rk_size%d(0x%llx)\n",
			i, cen->rk_size[i]);

	dev_info(&pdev->dev, "a2d_disph %d\n", cen->disph);

	dev_info(&pdev->dev, "a2d_hash %d\n", cen->hash);

	for (i = 0; i < ARRAY_SIZE(emi_a2d_con_offset); i++)
		dev_info(&pdev->dev, "emi_a2d_con_offset[%d] %d\n",
			i, emi_a2d_con_offset[i]);

	for (i = 0; i < ARRAY_SIZE(emi_a2d_chn_con_offset); i++)
		dev_info(&pdev->dev, "emi_a2d_chn_con_offset[%d] %d\n",
			i, emi_a2d_chn_con_offset[i]);

	platform_set_drvdata(pdev, cen);

	return 0;

}

static int emicen_remove(struct platform_device *dev)
{
	global_emi_cen = NULL;
	return 0;
}

static const struct of_device_id emicen_of_ids[] = {
	{.compatible = "mediatek,common-emicen", .data = (void *)1 },
	{.compatible = "mediatek,mt6873-emicen", .data = (void *)1 },
	{.compatible = "mediatek,mt6877-emicen", .data = (void *)2 },
	{}
};

static struct platform_driver emicen_drv = {
	.probe = emicen_probe,
	.remove = emicen_remove,
	.driver = {
		.name = "emicen_drv",
		.owner = THIS_MODULE,
		.of_match_table = emicen_of_ids,
	},
};

static int __init emicen_drv_init(void)
{
	int ret;

	ret = platform_driver_register(&emicen_drv);
	if (ret) {
		pr_info("%s: init fail, ret 0x%x\n", __func__, ret);
		return ret;
	}

	ret = driver_create_file(&emicen_drv.driver,
		&driver_attr_emicen_addr2dram);
	if (ret) {
		pr_info("emicen: failed to create addr2dram file\n");
		return ret;
	}
	return ret;
}

static void __exit emicen_drv_exit(void)
{
	platform_driver_unregister(&emicen_drv);
}

module_init(emicen_drv_init);
module_exit(emicen_drv_exit);

/*
 * mtk_emicen_get_ch_cnt - get the channel count
 *
 * Returns the channel count
 */
unsigned int mtk_emicen_get_ch_cnt(void)
{
	return (global_emi_cen) ? global_emi_cen->ch_cnt : 0;
}
EXPORT_SYMBOL(mtk_emicen_get_ch_cnt);

/*
 * mtk_emicen_get_rk_cnt - get the rank count
 *
 * Returns the rank count
 */
unsigned int mtk_emicen_get_rk_cnt(void)
{
	return (global_emi_cen) ? global_emi_cen->rk_cnt : 0;
}
EXPORT_SYMBOL(mtk_emicen_get_rk_cnt);

/*
 * mtk_emicen_get_rk_size - get the rank size of target rank
 * @rk_id: the id of target rank
 *
 * Returns the rank size of target rank
 */
unsigned int mtk_emicen_get_rk_size(unsigned int rk_id)
{
	if (rk_id < mtk_emicen_get_rk_cnt())
		return (global_emi_cen) ? global_emi_cen->rk_size[rk_id] : 0;
	else
		return 0;
}
EXPORT_SYMBOL(mtk_emicen_get_rk_size);

/*
 * mtk_emidbg_dump - dump emi full status to atf log
 *
 */
void mtk_emidbg_dump(void)
{
	unsigned long spinlock_save_flags;
	struct arm_smccc_res smc_res;

	spin_lock_irqsave(&emidbg_lock, spinlock_save_flags);

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIDBG_DUMP,
		0, 0, 0, 0, 0, 0, &smc_res);
	while (smc_res.a0 > 0) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIDBG_MSG,
		0, 0, 0, 0, 0, 0, &smc_res);

		pr_info("%s: %d, 0x%x, 0x%x, 0x%x\n", __func__,
			smc_res.a0, smc_res.a1, smc_res.a2, smc_res.a3);
	}

	spin_unlock_irqrestore(&emidbg_lock, spinlock_save_flags);
}
EXPORT_SYMBOL(mtk_emidbg_dump);

MODULE_DESCRIPTION("MediaTek EMICEN Driver v0.1");
