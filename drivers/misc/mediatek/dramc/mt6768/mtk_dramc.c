/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <asm/setup.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_chip.h>

#include "mtk_dramc.h"
#include "dramc.h"
#ifdef EMI_READY
#include "mt_emi_api.h"
#endif

struct mem_desc {
	u64 start;
	u64 size;
};

struct dram_info {
	u32 rank_num;
	struct mem_desc rank_info[4];
};

static unsigned int lp4_highest_freq;

void __iomem *DRAMC_AO_CHA_BASE_ADDR;
void __iomem *DRAMC_AO_CHB_BASE_ADDR;
void __iomem *DRAMC_NAO_CHA_BASE_ADDR;
void __iomem *DRAMC_NAO_CHB_BASE_ADDR;
void __iomem *DDRPHY_AO_CHA_BASE_ADDR;
void __iomem *DDRPHY_AO_CHB_BASE_ADDR;
void __iomem *DDRPHY_NAO_CHA_BASE_ADDR;
void __iomem *DDRPHY_NAO_CHB_BASE_ADDR;

unsigned int DRAM_TYPE;

#define DRAMC_TAG "[DRAMC]"
#define dramc_info(format, ...)	pr_info(DRAMC_TAG format, ##__VA_ARGS__)
struct dram_info *g_dram_info_dummy_read, *get_dram_info;
struct dram_info dram_info_dummy_read;
static unsigned int dram_rank_num;
static int __init dt_scan_dram_info(unsigned long node,
const char *uname, int depth, void *data)
{
	char *type = (char *)of_get_flat_dt_prop(node, "device_type", NULL);

       /* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

	if (node) {
		get_dram_info =
			(struct dram_info *)of_get_flat_dt_prop(node,
				"orig_dram_info", NULL);

		g_dram_info_dummy_read = &dram_info_dummy_read;
		dram_rank_num = get_dram_info->rank_num;
		dram_info_dummy_read.rank_num = dram_rank_num;
		dramc_info("dram info dram rank number = %d\n",
			g_dram_info_dummy_read->rank_num);
		dram_info_dummy_read.rank_info[0].start = 0;
		dram_info_dummy_read.rank_info[1].start = 0;
	}

	return node;
}


#ifdef CONFIG_MTK_DRAMC_PASR
#define __ETT__ 0
int enter_pasr_dpd_config(unsigned char segment_rank0,
			   unsigned char segment_rank1)
{
	unsigned int rank_pasr_segment[2];
	unsigned int iRankIdx = 0, iChannelIdx = 0, cnt = 1000;
	unsigned int u4value_24 = 0;
	unsigned int u4value_64 = 0;
	unsigned int u4value_38 = 0;
	void __iomem *u4rg_24; /* CKE control */
	void __iomem *u4rg_64; /* MR4 ZQCS */
	void __iomem *u4rg_38; /* MIOCKCTRLOFF */
	void __iomem *u4rg_5C; /* MRS */
	void __iomem *u4rg_60; /* MRWEN */
	void __iomem *u4rg_88; /* MRW_RESPONSE */
#if !__ETT__
	unsigned long save_flags;

	dramc_info("PASR r0 = 0x%x  r1 = 0x%x\n",
			(segment_rank0 & 0xFF), (segment_rank1 & 0xFF));
	local_irq_save(save_flags);
	if (acquire_dram_ctrl() != 0) {
		local_irq_restore(save_flags);
		return -1;
	}
	/* pr_info("[DRAMC0] get SPM HW SEMAPHORE!\n"); */
#endif
	rank_pasr_segment[0] = segment_rank0 & 0xFF; /* for rank0 */
	rank_pasr_segment[1] = segment_rank1 & 0xFF; /* for rank1 */

/* #if PASR_TEST_SCENARIO == PASR_SUPPORT_2_CHANNEL*/
#ifdef EMI_READY
	for (iChannelIdx = 0; iChannelIdx < get_ch_num(); iChannelIdx++) {
#else
	for (iChannelIdx = 0; iChannelIdx < 2; iChannelIdx++) {
#endif
		if ((DRAM_TYPE == TYPE_LPDDR4) || (DRAM_TYPE == TYPE_LPDDR4X)) {
			if (iChannelIdx == 0) { /*Channel-A*/
				u4rg_24 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x24);
				u4rg_64 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x64);
				u4rg_38 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x38);
				u4rg_5C = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x5C);
				u4rg_60 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x60);
				u4rg_88 = IOMEM(DRAMC_NAO_CHA_BASE_ADDR + 0x88);
			} else { /*Channel-B*/
				u4rg_24 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x24);
				u4rg_64 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x64);
				u4rg_38 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x38);
				u4rg_5C = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x5C);
				u4rg_60 = IOMEM(DRAMC_AO_CHB_BASE_ADDR + 0x60);
				u4rg_88 = IOMEM(DRAMC_NAO_CHB_BASE_ADDR + 0x88);
			}
		} else if (DRAM_TYPE == TYPE_LPDDR3) {
			if (iChannelIdx == 1)
				break;
			u4rg_24 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x24);
			u4rg_64 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x64);
			u4rg_38 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x38);
			u4rg_5C = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x5C);
			u4rg_60 = IOMEM(DRAMC_AO_CHA_BASE_ADDR + 0x60);
			u4rg_88 = IOMEM(DRAMC_NAO_CHA_BASE_ADDR + 0x88);
		} else {
			break;
		}

		u4value_64 = readl(u4rg_64);
		u4value_38 = readl(u4rg_38);
		u4value_24 = readl(u4rg_24);

		/* Disable MR4 => 0x64[29] = 1 */
		writel(readl(u4rg_64) | 0x20000000, u4rg_64);
		/* Disable ZQCS => LPDDR4: 0x64[30] = 0 LPDDR3: 0x64[31] = 0 */
		writel(readl(u4rg_64) & 0x3FFFFFFF, u4rg_64);
#if !__ETT__
		mb(); /* flush memory */
#endif
		udelay(2);
		/* DCMEN2 = 0 */
		writel(readl(u4rg_38) & 0xFFFFFFFD, u4rg_38);
		/* PHYCLKDYNGEN = 0 */
		writel(readl(u4rg_38) & 0xBFFFFFFF, u4rg_38);
		/* MIOCKCTRLOFF = 1 */
		writel(readl(u4rg_38) | 0x04000000, u4rg_38);
		writel((readl(u4rg_24) & (~((0x1<<5) | (0x1<<7)))) |
		((0x1<<4) | (0x1<<6)), u4rg_24);
		/* CKE0 CKE1 fix on no matter the setting of CKE2RANK*/
#ifdef EMI_READY
		for (iRankIdx = 0; iRankIdx < get_rk_num(); iRankIdx++) {
#else
		for (iRankIdx = 0; iRankIdx < 2; iRankIdx++) {
#endif
			writel(((iRankIdx << 24) | rank_pasr_segment[iRankIdx] |
			(0x00000011 << 8)), u4rg_5C);
			writel(readl(u4rg_60) | 0x00000001, u4rg_60);
			cnt = 1000;
			do {
				if (cnt-- == 0) {
					dramc_info("R%d PASR MRW fail!\n",
						(iRankIdx == 0) ? 0 : 1);
#if !__ETT__
					release_dram_ctrl();
					local_irq_restore(save_flags);
#endif
					return -1;
				}
				udelay(1);
			} while ((readl(u4rg_88) & 0x00000001) == 0x0);
			writel(readl(u4rg_60) & 0xfffffffe, u4rg_60);
		}
		writel(u4value_64, u4rg_64);
		writel(u4value_24, u4rg_24);
		writel(u4value_38, u4rg_38);
		writel(0, u4rg_5C);
}
#if !__ETT__
	if (release_dram_ctrl() != 0)
		pr_warn("[DRAMC0] release SPM HW SEMAPHORE fail!\n");
	/* pr_info("[DRAMC0] release SPM HW SEMAPHORE success!\n"); */
	local_irq_restore(save_flags);
#endif
	return 0;
}

int exit_pasr_dpd_config(void)
{
	int ret;

	ret = enter_pasr_dpd_config(0, 0);

	return ret;
}
#else
int enter_pasr_dpd_config(
	unsigned char segment_rank0, unsigned char segment_rank1)
{
	return 0;
}

int exit_pasr_dpd_config(void)
{
	return 0;
}
#endif

#define MEM_TEST_SIZE 0x2000
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
int Binning_DRAM_complex_mem_test(void)
{
	unsigned char *MEM8_BASE;
	unsigned short *MEM16_BASE;
	unsigned int *MEM32_BASE;
	unsigned int *MEM_BASE;
	unsigned long mem_ptr;
	unsigned char pattern8;
	unsigned short pattern16;
	unsigned int i, j, size, pattern32;
	unsigned int value;
	unsigned int len = MEM_TEST_SIZE;
	void *ptr;
	int ret = 1;

	ptr = vmalloc(MEM_TEST_SIZE);

	if (!ptr) {
		/*dramc_info("fail to vmalloc\n");*/
		/*ASSERT(0);*/
		ret = -24;
		goto fail;
	}

	MEM8_BASE = (unsigned char *)ptr;
	MEM16_BASE = (unsigned short *)ptr;
	MEM32_BASE = (unsigned int *)ptr;
	MEM_BASE = (unsigned int *)ptr;
	/* dramc_info("Test DRAM start address 0x%lx\n", (unsigned long)ptr); */
	dramc_info("Test DRAM start address %p\n", ptr);
	dramc_info("Test DRAM SIZE 0x%x\n", MEM_TEST_SIZE);
	size = len >> 2;

	/* === Verify the tied bits (tied high) === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0;

	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0) {
			/* return -1; */
			ret = -1;
			goto fail;
		} else
			MEM32_BASE[i] = 0xffffffff;
	}

	/* === Verify the tied bits (tied low) === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			/* return -2; */
			ret = -2;
			goto fail;
		} else
			MEM32_BASE[i] = 0x00;
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = 0; i < len; i++)
		MEM8_BASE[i] = pattern8++;
	pattern8 = 0x00;
	for (i = 0; i < len; i++) {
		if (MEM8_BASE[i] != pattern8++) {
			/* return -3; */
			ret = -3;
			goto fail;
		}
	}

	/* === Verify pattern 2 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = j = 0; i < len; i += 2, j++) {
		if (MEM8_BASE[i] == pattern8)
			MEM16_BASE[j] = pattern8;
		if (MEM16_BASE[j] != pattern8) {
			/* return -4; */
			ret = -4;
			goto fail;
		}
		pattern8 += 2;
	}

	/* === Verify pattern 3 (0x00~0xffff) === */
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++)
		MEM16_BASE[i] = pattern16++;
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++) {
		if (MEM16_BASE[i] != pattern16++) {
			/* return -5; */
			ret = -5;
			goto fail;
		}
	}

	/* === Verify pattern 4 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++)
		MEM32_BASE[i] = pattern32++;
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++) {
		if (MEM32_BASE[i] != pattern32++) {
			/* return -6; */
			ret = -6;
			goto fail;
		}
	}

	/* === Pattern 5: Filling memory range with 0x44332211 === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0x44332211;

	/* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x44332211) {
			/* return -7; */
			ret = -7;
			goto fail;
		} else {
			MEM32_BASE[i] = 0xa5a5a5a5;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 0h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a5a5) {
			/* return -8; */
			ret = -8;
			goto fail;
		} else {
			MEM8_BASE[i * 4] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 2h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a500) {
			/* return -9; */
			ret = -9;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 2] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 1h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa500a500) {
			/* return -10; */
			ret = -10;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 1] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 3h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5000000) {
			/* return -11; */
			ret = -11;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 3] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with ffff */
	/* Word Pattern at offset 1h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x00000000) {
			/* return -12; */
			ret = -12;
			goto fail;
		} else {
			MEM16_BASE[i * 2 + 1] = 0xffff;
		}
	}

	/* === Read Check then Fill Memory with ffff */
	/* Word Pattern at offset 0h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffff0000) {
			/* return -13; */
			ret = -13;
			goto fail;
		} else {
			MEM16_BASE[i * 2] = 0xffff;
		}
	}
    /*===  Read Check === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			/* return -14; */
			ret = -14;
			goto fail;
		}
	}

    /************************************************
     * Additional verification
     ************************************************/
	/* === stage 1 => write 0 === */

	for (i = 0; i < size; i++)
		MEM_BASE[i] = PATTERN1;

	/* === stage 2 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];

		if (value != PATTERN1) {
			/* return -15; */
			ret = -15;
			goto fail;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 3 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			/* return -16; */
			ret = -16;
			goto fail;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 4 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			/* return -17; */
			ret = -17;
			goto fail;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 5 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			/* return -18; */
			ret = -18;
			goto fail;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 6 => read 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			/* return -19; */
			ret = -19;
			goto fail;
		}
	}

	/* === 1/2/4-byte combination test === */
	mem_ptr = (unsigned long)MEM_BASE;
	while (mem_ptr < ((unsigned long)MEM_BASE + (size << 2))) {
		*((unsigned char *)mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *)mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *)mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *)mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned char *)mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *)mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *)mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *)mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
	}
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != 0x12345678) {
			/* return -20; */
			ret = -20;
			goto fail;
		}
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	MEM8_BASE[0] = pattern8;
	for (i = 0; i < size * 4; i++) {
		unsigned char waddr8, raddr8;

		waddr8 = i + 1;
		raddr8 = i;
		if (i < size * 4 - 1)
			MEM8_BASE[waddr8] = pattern8 + 1;
		if (MEM8_BASE[raddr8] != pattern8) {
			/* return -21; */
			ret = -21;
			goto fail;
		}
		pattern8++;
	}

	/* === Verify pattern 2 (0x00~0xffff) === */
	pattern16 = 0x00;
	MEM16_BASE[0] = pattern16;
	for (i = 0; i < size * 2; i++) {
		if (i < size * 2 - 1)
			MEM16_BASE[i + 1] = pattern16 + 1;
		if (MEM16_BASE[i] != pattern16) {
			/* return -22; */
			ret = -22;
			goto fail;
		}
		pattern16++;
	}
	/* === Verify pattern 3 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	MEM32_BASE[0] = pattern32;
	for (i = 0; i < size; i++) {
		if (i < size - 1)
			MEM32_BASE[i + 1] = pattern32 + 1;
		if (MEM32_BASE[i] != pattern32) {
			/* return -23; */
			ret = -23;
			goto fail;
		}
		pattern32++;
	}
	dramc_info("complex R/W mem test pass\n");

fail:
	vfree(ptr);
	return ret;
}

unsigned int lpDram_Register_Read(unsigned int Reg_base, unsigned int Offset)
{
	if ((Reg_base == DRAMC_NAO_CHA) && (Offset < 0x1000))
		return readl(IOMEM(DRAMC_NAO_CHA_BASE_ADDR + Offset));

	else if ((Reg_base == DRAMC_NAO_CHB) && (Offset < 0x1000))
		return readl(IOMEM(DRAMC_NAO_CHB_BASE_ADDR + Offset));

	else if ((Reg_base == DRAMC_AO_CHA) && (Offset < 0x2000))
		return readl(IOMEM(DRAMC_AO_CHA_BASE_ADDR + Offset));

	else if ((Reg_base == DRAMC_AO_CHB) && (Offset < 0x2000))
		return readl(IOMEM(DRAMC_AO_CHB_BASE_ADDR + Offset));

	else if ((Reg_base == PHY_NAO_CHA) && (Offset < 0x1000))
		return readl(IOMEM(DDRPHY_NAO_CHA_BASE_ADDR + Offset));

	else if ((Reg_base == PHY_NAO_CHB) && (Offset < 0x1000))
		return readl(IOMEM(DDRPHY_NAO_CHB_BASE_ADDR + Offset));

	else if ((Reg_base == PHY_AO_CHA) && (Offset < 0x2000))
		return readl(IOMEM(DDRPHY_AO_CHA_BASE_ADDR + Offset));

	else if ((Reg_base == PHY_AO_CHB) && (Offset < 0x2000))
		return readl(IOMEM(DDRPHY_AO_CHB_BASE_ADDR + Offset));

	else
		return 0;
}
EXPORT_SYMBOL(lpDram_Register_Read);

unsigned int get_dram_data_rate(void)
{
	unsigned int u4PllIdx, u4ShuLevel, u4SDM_PCW, u4PREDIV, u4POSDIV;
	unsigned int u4CKDIV4, u4VCOFreq, u4DataRate = 0;
	unsigned int pcw_ofs[2] = { 0xd9c, 0xd94 };
	unsigned int prepost_ofs[2] = { 0xda8, 0xda0 };
	int channels;

	channels = get_emi_ch_num();
	u4ShuLevel = get_shuffle_status();

	/* only CHA have this bit */
	u4PllIdx = readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + 0x510)) >> 31 & 0x00000001;

	u4SDM_PCW = readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + pcw_ofs[u4PllIdx] +
			(0x500 * u4ShuLevel))) >> 16;
	u4PREDIV = (readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + prepost_ofs[u4PllIdx] +
			(0x500 * u4ShuLevel))) & 0x000c0000) >> 18;
	u4POSDIV = readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + prepost_ofs[u4PllIdx] +
			(0x500 * u4ShuLevel))) & 0x00000007;
	u4CKDIV4 = (readl(
		IOMEM(DDRPHY_AO_CHA_BASE_ADDR + 0xd18 +
			(0x500 * u4ShuLevel))) & 0x08000000) >> 27;

	u4VCOFreq = ((52>>u4PREDIV)*(u4SDM_PCW>>8))>>u4POSDIV;

	u4DataRate = u4VCOFreq>>u4CKDIV4;

	/*
	 *dramc_info("PCW=0x%X, u4PREDIV=%d, u4POSDIV=%d,"
	 *	"  CKDIV4=%d, DataRate=%d\n",
	 *	u4SDM_PCW, u4PREDIV, u4POSDIV, u4CKDIV4, u4DataRate);
	 */

	if (DRAM_TYPE == TYPE_LPDDR3) {
		if (u4DataRate == 1859)
			u4DataRate = 1866;
		else if (u4DataRate == 1599)
			u4DataRate = 1600;
		else if (u4DataRate == 1534)
			u4DataRate = 1534;
		else if (u4DataRate == 1196)
			u4DataRate = 1200;
		else
			u4DataRate = 0;
	} else if ((DRAM_TYPE == TYPE_LPDDR4) || (DRAM_TYPE == TYPE_LPDDR4X)) {
		if (u4DataRate == 3588)
			u4DataRate = 3600;
		else if (u4DataRate == 3198)
			u4DataRate = 3200;
		else if (u4DataRate == 2392)
			u4DataRate = 2400;
		else if (u4DataRate == 1599)
			u4DataRate = 1600;
		else if (u4DataRate == 1534)
			u4DataRate = 1534;
		else
			u4DataRate = 0;
	} else
		u4DataRate = 0;

	return u4DataRate;
}
EXPORT_SYMBOL(get_dram_data_rate);


unsigned int get_shuffle_status(void)
{
	return (readl(PDEF_DRAMC0_CHA_REG_0E4) & 0x6) >> 1;
	/* HPM = 0, LPM = 1, ULPM = 2; */

}

int get_ddr_type(void)
{
	return DRAM_TYPE;

}
EXPORT_SYMBOL(get_ddr_type);

unsigned char get_ddr_mr(unsigned int index)
{
	return (unsigned char)get_dram_mr(index);

}
EXPORT_SYMBOL(get_ddr_mr);

int get_emi_ch_num(void)
{
#ifdef EMI_READY
	return get_ch_num();
#else
	return 0;
#endif
}
EXPORT_SYMBOL(get_emi_ch_num);

int dram_steps_freq(unsigned int step)
{
	int freq = -1;

	/* step here means ddr shuffle */

	switch (step) {
	case 0:
		if (DRAM_TYPE == TYPE_LPDDR3)
			freq = 1866;
		else if (DRAM_TYPE == TYPE_LPDDR4X)
			freq = 3600;
		else if (DRAM_TYPE == TYPE_LPDDR4)
			freq = lp4_highest_freq;
		break;
	case 1:
		if (DRAM_TYPE == TYPE_LPDDR3)
			freq = 1534;
		else if ((DRAM_TYPE == TYPE_LPDDR4) ||
				(DRAM_TYPE == TYPE_LPDDR4X))
			freq = 2400;
		break;
	case 2:
		if (DRAM_TYPE == TYPE_LPDDR3)
			freq = 1200;
		else if ((DRAM_TYPE == TYPE_LPDDR4) ||
				(DRAM_TYPE == TYPE_LPDDR4X))
			freq = 1534;
		break;
	default:
		return -1;
	}
	return freq;
}
EXPORT_SYMBOL(dram_steps_freq);


static ssize_t complex_mem_test_show(struct device_driver *driver, char *buf)
{
	int ret;

	ret = Binning_DRAM_complex_mem_test();
	if (ret > 0)
		return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
	else
		return snprintf(buf, PAGE_SIZE, "MEM TEST failed %d\n", ret);
}

static ssize_t complex_mem_test_store(struct device_driver *driver,
const char *buf, size_t count)
{
	/*snprintf(buf, "do nothing\n");*/
	return count;
}

static ssize_t read_dram_data_rate_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DRAM data rate = %d\n",
	get_dram_data_rate());
}

static ssize_t read_dram_data_rate_store(struct device_driver *driver,
const char *buf, size_t count)
{
	return count;
}

#define DRIVER_ATTR(_name, _mode, _show, _store) \
	struct driver_attribute driver_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)


static DRIVER_ATTR(emi_clk_mem_test, 0664,
	complex_mem_test_show, complex_mem_test_store);
static DRIVER_ATTR(read_dram_data_rate, 0664,
	read_dram_data_rate_show, read_dram_data_rate_store);

static int dram_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int i;
	struct resource *res;
	void __iomem *base_temp[8];
	struct device_node *node = NULL;

	pr_debug("[DRAMC] module probe.\n");

	for (i = 0; i < (sizeof(base_temp) / sizeof(*base_temp)); i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		base_temp[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(base_temp[i])) {
			dramc_info("unable to map %d base\n", i);
			return -EINVAL;
		}
	}

	DRAMC_AO_CHA_BASE_ADDR = base_temp[0];
	DRAMC_AO_CHB_BASE_ADDR = base_temp[1];

	DRAMC_NAO_CHA_BASE_ADDR = base_temp[2];
	DRAMC_NAO_CHB_BASE_ADDR = base_temp[3];

	DDRPHY_AO_CHA_BASE_ADDR = base_temp[4];
	DDRPHY_AO_CHB_BASE_ADDR = base_temp[5];

	DDRPHY_NAO_CHA_BASE_ADDR = base_temp[6];
	DDRPHY_NAO_CHB_BASE_ADDR = base_temp[7];

	dramc_info("get DRAMC_AO_CHA_BASE_ADDR @ %p\n",
			DRAMC_AO_CHA_BASE_ADDR);
	dramc_info("get DRAMC_AO_CHB_BASE_ADDR @ %p\n",
			DRAMC_AO_CHB_BASE_ADDR);

	dramc_info("get DDRPHY_AO_CHA_BASE_ADDR @ %p\n",
			DDRPHY_AO_CHA_BASE_ADDR);
	dramc_info("get DDRPHY_AO_CHB_BASE_ADDR @ %p\n",
			DDRPHY_AO_CHB_BASE_ADDR);

	dramc_info("get DRAMC_NAO_CHA_BASE_ADDR @ %p\n",
			DRAMC_NAO_CHA_BASE_ADDR);
	dramc_info("get DRAMC_NAO_CHB_BASE_ADDR @ %p\n",
			DRAMC_NAO_CHB_BASE_ADDR);

	dramc_info("get DDRPHY_NAO_CHA_BASE_ADDR @ %p\n",
			DDRPHY_NAO_CHA_BASE_ADDR);
	dramc_info("get DDRPHY_NAO_CHB_BASE_ADDR @ %p\n",
			DDRPHY_NAO_CHB_BASE_ADDR);

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (node) {
		SLEEP_BASE_ADDR = of_iomap(node, 0);
		dramc_info("get SLEEP_BASE_ADDR @ %p\n", SLEEP_BASE_ADDR);
	} else {
		dramc_info("can't find SLEEP_BASE_ADDR compatible node\n");
		return -1;
	}

#ifdef EMI_READY
	DRAM_TYPE = get_dram_type();
#else
	DRAM_TYPE = TYPE_LPDDR4X;
#endif
	dramc_info("dram type =%d\n", DRAM_TYPE);

	if (!DRAM_TYPE) {
		dramc_info("dram type error !!\n");
		return -1;
	}

	dramc_info("Dram Data Rate = %d\n", get_dram_data_rate());
	dramc_info("shuffle_status = %d\n", get_shuffle_status());

	if (DRAM_TYPE == TYPE_LPDDR4)
		lp4_highest_freq = get_dram_data_rate();
	else
		lp4_highest_freq = 0;

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_emi_clk_mem_test);
	if (ret) {
		dramc_info("fail to create emi_clk_mem_test sysfs files\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_read_dram_data_rate);
	if (ret) {
		dramc_info("fail to create read_dram_data_rate sysfs files\n");
		return ret;
	}

	return 0;
}

static int dram_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dram_of_ids[] = {
	{.compatible = "mediatek,dramc",},
	{}
};
#endif

static struct platform_driver dram_test_drv = {
	.probe = dram_probe,
	.remove = dram_remove,
	.driver = {
		.name = "emi_clk_test",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dram_of_ids,
#endif
		},
};

/* int __init dram_test_init(void) */
static int __init dram_test_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&dram_test_drv);
	if (ret) {
		dramc_info("init fail, ret 0x%x\n", ret);
		return ret;
	}
	if (of_scan_flat_dt(dt_scan_dram_info, NULL) > 0) {
		dramc_info("find dt_scan_dram_info\n");
	} else {
		dramc_info("can't find dt_scan_dram_info\n");
		return -1;
	}
	return ret;
}

static void __exit dram_test_exit(void)
{
	platform_driver_unregister(&dram_test_drv);
}

postcore_initcall(dram_test_init);
module_exit(dram_test_exit);

void *mt_dramc_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DRAMC_AO_CHA_BASE_ADDR;
	case 1:
		return DRAMC_AO_CHB_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_dramc_chn_base_get);

void *mt_dramc_nao_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DRAMC_NAO_CHA_BASE_ADDR;
	case 1:
		return DRAMC_NAO_CHB_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_dramc_nao_chn_base_get);

void *mt_ddrphy_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DDRPHY_AO_CHA_BASE_ADDR;
	case 1:
		return DDRPHY_AO_CHB_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_ddrphy_chn_base_get);

void *mt_ddrphy_nao_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DDRPHY_NAO_CHA_BASE_ADDR;
	case 1:
		return DDRPHY_NAO_CHB_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_ddrphy_nao_chn_base_get);


unsigned int mt_dramc_chn_get(unsigned int emi_cona)
{
	switch ((emi_cona >> 8) & 0x3) {
	case 0:
		return 1;
	case 1:
		return 2;
	default:
		dramc_info("invalid channel num (emi_cona = 0x%x)\n",
				emi_cona);
	}
	return 0;
}

unsigned int mt_dramc_chp_get(unsigned int emi_cona)
{
	unsigned int chp;

	chp = (emi_cona >> 2) & 0x3;

	return chp + 7;
}
phys_addr_t mt_dramc_rankbase_get(unsigned int rank)
{
	if (!get_dram_info)
		return 0;

	if (rank >= get_dram_info->rank_num)
		return 0;

	return get_dram_info->rank_info[rank].start;
}

unsigned int mt_dramc_ta_support_ranks(void)
{
	return dram_rank_num;
}

MODULE_DESCRIPTION("MediaTek DRAMC Driver v0.1");
