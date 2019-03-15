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
/* #include <mach/mtk_clkmgr.h> */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <asm/setup.h>
#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mtk_io.h>
/* #include <mt-plat/dma.h> */
#include <mt-plat/sync_write.h>

#include "mtk_dramc.h"
#include "dramc.h"
#include "emi_bwl.h"

#ifdef CONFIG_OF_RESERVED_MEM
#define DRAM_R0_DUMMY_READ_RESERVED_KEY "reserve-memory-dram_r0_dummy_read"
#define DRAM_R1_DUMMY_READ_RESERVED_KEY "reserve-memory-dram_r1_dummy_read"
#include <linux/of_reserved_mem.h>
/* #include <mt-plat/mtk_memcfg.h> */
#endif

#include <mt-plat/aee.h>
#include <mt-plat/mtk_chip.h>

void __iomem *DRAMC_AO_CHA_BASE_ADDR;
void __iomem *DRAMC_NAO_CHA_BASE_ADDR;
void __iomem *DDRPHY_CHA_BASE_ADDR;
#define DRAM_RSV_SIZE 0x1000


static DEFINE_MUTEX(dram_dfs_mutex);
unsigned char No_DummyRead;
unsigned int DRAM_TYPE;
unsigned int CBT_MODE;

/*extern bool spm_vcorefs_is_dvfs_in_porgress(void);*/
#define Reg_Sync_Writel(addr, val)   writel(val, IOMEM(addr))
#define Reg_Readl(addr) readl(IOMEM(addr))
static unsigned int dram_rank_num;
phys_addr_t dram_rank0_addr, dram_rank1_addr;


struct dram_info *g_dram_info_dummy_read, *get_dram_info;
struct dram_info dram_info_dummy_read;

#define DRAMC_RSV_TAG "[DRAMC_RSV]"
#define dramc_rsv_aee_warn(string, args...) do {\
	pr_info("[ERR]"string, ##args); \
	aee_kernel_warning(DRAMC_RSV_TAG, "[ERR]"string, ##args);  \
} while (0)

/* Return 0 if success, -1 if failure */
static int __init dram_dummy_read_fixup(void)
{
	int ret = 0;

	ret = acquire_buffer_from_memory_lowpower(&dram_rank1_addr);

	/* Success to acquire memory */
	if (ret == 0) {
		pr_info("%s: %pa\n", __func__, &dram_rank1_addr);
		return 0;
	}

	/* error occurs */
	pr_info("%s: failed to acquire last 1 page(%d)\n", __func__, ret);
	return -1;
}

static int __init dt_scan_dram_info(unsigned long node,
const char *uname, int depth, void *data)
{
	char *type = (char *)of_get_flat_dt_prop(node, "device_type", NULL);
	const __be32 *reg, *endp;
	unsigned long l;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		* The longtrail doesn't have a device_type on the
		* /memory node, so look for the node called /memory@0.
		*/
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

	reg = (const __be32 *)of_get_flat_dt_prop(node,
	(const char *)"reg", (int *)&l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));
	if (node) {
		get_dram_info =
		(struct dram_info *)of_get_flat_dt_prop(node,
		"orig_dram_info", NULL);
		if (get_dram_info == NULL) {
			No_DummyRead = 1;
			return 0;
		}

		g_dram_info_dummy_read = &dram_info_dummy_read;
		dram_info_dummy_read.rank_num = get_dram_info->rank_num;
		dram_rank_num = get_dram_info->rank_num;
		pr_info("[DRAMC] dram info dram rank number = %d\n",
		g_dram_info_dummy_read->rank_num);

		if (dram_rank_num == SINGLE_RANK) {
			dram_info_dummy_read.rank_info[0].start = dram_rank0_addr;
			dram_info_dummy_read.rank_info[1].start = dram_rank0_addr;
			pr_info("[DRAMC] dram info dram rank0 base = 0x%llx\n",
			g_dram_info_dummy_read->rank_info[0].start);
		} else if (dram_rank_num == DUAL_RANK) {
			/* No dummy read address for rank1, try to fix it up */
			if (dram_rank1_addr == 0 && dram_dummy_read_fixup() != 0) {
				No_DummyRead = 1;
				dramc_rsv_aee_warn("dram dummy read reserve fail on rank1 !!!\n");
			}

			dram_info_dummy_read.rank_info[0].start = dram_rank0_addr;
			dram_info_dummy_read.rank_info[1].start = dram_rank1_addr;
			pr_info("[DRAMC] dram info dram rank0 base = 0x%llx\n",
			g_dram_info_dummy_read->rank_info[0].start);
			pr_info("[DRAMC] dram info dram rank1 base = 0x%llx\n",
			g_dram_info_dummy_read->rank_info[1].start);
		} else {
			No_DummyRead = 1;
			pr_info("[DRAMC] dram info dram rank number incorrect !!!\n");
		}
	}

	return node;
}

#ifdef CONFIG_MTK_DRAMC_PASR
#define __ETT__ 0
int enter_pasr_dpd_config(unsigned char segment_rank0,
			   unsigned char segment_rank1)
{
	/* for D-3, support run time MRW */
	unsigned int rank_pasr_segment[2];
	unsigned int dramc0_spcmd, dramc0_pd_ctrl, dramc0_padctl4;
	unsigned int i, cnt = 1000;

	rank_pasr_segment[0] = segment_rank0 & 0xFF;	/* for rank0 */
	rank_pasr_segment[1] = segment_rank1 & 0xFF;	/* for rank1 */
	pr_info("[DRAMC0] PASR r0 = 0x%x  r1 = 0x%x\n", rank_pasr_segment[0], rank_pasr_segment[1]);

	/* backup original data */
	dramc0_spcmd = readl(PDEF_DRAMC0_REG_1E4);
	dramc0_pd_ctrl = readl(PDEF_DRAMC0_REG_1DC);
	dramc0_padctl4 = readl(PDEF_DRAMC0_REG_0E4);

	/* Set MIOCKCTRLOFF(0x1dc[26])=1: not stop to DRAM clock! */
	writel(dramc0_pd_ctrl | 0x04000000, PDEF_DRAMC0_REG_1DC);

	/* fix CKE */
	writel(dramc0_padctl4 | 0x00000004, PDEF_DRAMC0_REG_0E4);

	udelay(1);

	for (i = 0; i < 2; i++) {
		/* set MRS settings include rank number, segment information and MRR17 */
		writel(((i << 28) | (rank_pasr_segment[i] << 16) | 0x00000011), PDEF_DRAMC0_REG_088);
		/* Mode register write command enable */
		writel(0x00000001, PDEF_DRAMC0_REG_1E4);

		/* wait MRW command response */
		/* wait >1us */
		/* gpt_busy_wait_us(1); */
		do {
			if (cnt-- == 0) {
				pr_info("[DRAMC0] PASR MRW fail!\n");
				return -1;
			}
			udelay(1);
		} while ((readl(PDEF_DRAMC0_REG_3B8) & 0x00000001) == 0x0);
		mb();	/* make sure the DRAM have been read */

		/* Mode register write command disable */
		writel(0x0, PDEF_DRAMC0_REG_1E4);
	}

	/* release fix CKE */
	writel(dramc0_padctl4, PDEF_DRAMC0_REG_0E4);

	/* recover Set MIOCKCTRLOFF(0x1dc[26]) */
	/* Set MIOCKCTRLOFF(0x1DC[26])=0: stop to DRAM clock */
	writel(dramc0_pd_ctrl, PDEF_DRAMC0_REG_1DC);

	/* writel(0x00000004, PDEF_DRAMC0_REG_088); */
	pr_info("[DRAMC0] PASR offset 0x88 = 0x%x\n", readl(PDEF_DRAMC0_REG_088));
	writel(dramc0_spcmd, PDEF_DRAMC0_REG_1E4);

	return 0;
#if 0
	if (segment_rank1 == 0xFF) {	/*all segments of rank1 are not reserved -> rank1 enter DPD*/
		slp_dpd_en(1);
	}
#endif

	/*slp_pasr_en(1, segment_rank0 | (segment_rank1 << 8));*/
}

int exit_pasr_dpd_config(void)
{
	int ret;
	/*slp_dpd_en(0);*/
	/*slp_pasr_en(0, 0);*/
	ret = enter_pasr_dpd_config(0, 0);

	return ret;
}
#else
int enter_pasr_dpd_config(unsigned char segment_rank0, unsigned char segment_rank1)
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
		/* pr_info("fail to vmalloc\n"); */
		/*ASSERT(0);*/
		ret = -24;
		goto fail;
	}

	MEM8_BASE = (unsigned char *)ptr;
	MEM16_BASE = (unsigned short *)ptr;
	MEM32_BASE = (unsigned int *)ptr;
	MEM_BASE = (unsigned int *)ptr;
	/*pr_info("Test DRAM start address 0x%lx\n", (unsigned long)ptr);*/
	pr_info("Test DRAM start address %p\n", ptr);
	pr_info("Test DRAM SIZE 0x%x\n", MEM_TEST_SIZE);
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
	pr_info("complex R/W mem test pass\n");

fail:
	vfree(ptr);
	return ret;
}

unsigned int ucDram_Register_Read(unsigned int u4reg_addr)
{
	unsigned int pu4reg_value;

	pu4reg_value = readl(IOMEM(DRAMC_AO_CHA_BASE_ADDR + (u4reg_addr))) |
		readl(IOMEM(DDRPHY_CHA_BASE_ADDR + (u4reg_addr))) |
		readl(IOMEM(DRAMC_NAO_CHA_BASE_ADDR + (u4reg_addr)));

	return pu4reg_value;
}

unsigned int lpDram_Register_Read(unsigned int Reg_base, unsigned int Offset)
{
	if ((Reg_base == DRAMC_NAO_CHA) && (Offset < 0x1000))
		return readl(IOMEM(DRAMC_NAO_CHA_BASE_ADDR + Offset));
	else if ((Reg_base == DRAMC_AO_CHA) && (Offset < 0x1000))
		return readl(IOMEM(DRAMC_AO_CHA_BASE_ADDR + Offset));
	else if ((Reg_base == PHY_AO_CHA) && (Offset < 0x1000))
		return readl(IOMEM(DDRPHY_CHA_BASE_ADDR + Offset));

	pr_info("lpDram_Register_Read: unsupported Reg_base (%d)\n", Reg_base);
	return 0;
}
EXPORT_SYMBOL(lpDram_Register_Read);


/* clone from legacy platform */
unsigned int get_dram_data_rate(void)
{
	unsigned int MEMPLL1_DIV, MEMPLL1_NCPO, MEMPLL1_FOUT;
	unsigned int MEMPLL2_FOUT, MEMPLL2_FBSEL, MEMPLL2_FBDIV;
	unsigned int MEMPLL2_M4PDIV;

	MEMPLL1_DIV = (ucDram_Register_Read(0x0604) & (0x0000007f << 25)) >> 25;
	MEMPLL1_NCPO = (ucDram_Register_Read(0x0624) & (0x7fffffff << 1)) >> 1;
	MEMPLL2_FBSEL = (ucDram_Register_Read(0x0608) & (0x00000003 << 10)) >> 10;
	MEMPLL2_FBSEL = 1 << MEMPLL2_FBSEL;
	MEMPLL2_FBDIV = (ucDram_Register_Read(0x0618) & (0x0000007f << 2)) >> 2;
	MEMPLL2_M4PDIV = (ucDram_Register_Read(0x060c) & (0x00000003 << 10)) >> 10;
	MEMPLL2_M4PDIV = 1 << (MEMPLL2_M4PDIV + 1);

	/*  1PLL:  26*MEMPLL1_NCPO/MEMPLL1_DIV*MEMPLL2_FBSEL*MEMPLL2_FBDIV/2^24 */
	/*  3PLL:  26*MEMPLL1_NCPO/MEMPLL1_DIV*MEMPLL2_M4PDIV*MEMPLL2_FBDIV*2/2^24 */

	MEMPLL1_FOUT = (MEMPLL1_NCPO / MEMPLL1_DIV) * 26;
	if ((ucDram_Register_Read(0x0640) & 0x3) == 3) {
		/*  1PLL */
		MEMPLL2_FOUT = (((MEMPLL1_FOUT * MEMPLL2_FBSEL) >> 12) * MEMPLL2_FBDIV) >> 12;
	} else {
		/*  2 or 3 PLL */
		MEMPLL2_FOUT = (((MEMPLL1_FOUT * MEMPLL2_M4PDIV * 2) >> 12) * MEMPLL2_FBDIV) >> 12;
	}

#if 0
	pr_info("MEMPLL1_DIV=%d, MEMPLL1_NCPO=%d, MEMPLL2_FBSEL=%d, MEMPLL2_FBDIV=%d\n",
			MEMPLL1_DIV, MEMPLL1_NCPO, MEMPLL2_FBSEL, MEMPLL2_FBDIV);
	pr_info("MEMPLL2_M4PDIV=%d, MEMPLL1_FOUT=%d, MEMPLL2_FOUT=%d\n",
			MEMPLL2_M4PDIV, MEMPLL1_FOUT, MEMPLL2_FOUT);
#endif

	/*  workaround (Darren) */
	MEMPLL2_FOUT++;

	switch (MEMPLL2_FOUT) {
	case 1066:
	case 1333:
		break;

	default:
		pr_info("[DRAMC] MemFreq region is incorrect MEMPLL2_FOUT=%d\n", MEMPLL2_FOUT);
	}

	return MEMPLL2_FOUT;
}
EXPORT_SYMBOL(get_dram_data_rate);

unsigned int read_dram_temperature(unsigned char channel)
{
	unsigned int value = 0;

	if (channel == CHANNEL_A) {
		value =
		(readl(IOMEM(DRAMC_NAO_CHA_BASE_ADDR + 0x3b8)) >> 8) & 0x7;
	}

	return value;
}

unsigned int get_shuffle_status(void)
{
	/* TODO */
	return 0;
	/* return (readl(PDEF_DRAMC0_CHA_REG_0E4) & 0x6) >> 1; */
	/* HPM = 0, LPM = 1, ULPM = 2; */
}

int get_ddr_type(void)
{
	int type = get_dram_type();

	switch (type) {
	case 2:
		return TYPE_LPDDR2;
	case 3:
		return TYPE_LPDDR3;
	default:
		return -1;
	}
}
EXPORT_SYMBOL(get_ddr_type);

int get_emi_ch_num(void)
{
	/* XXX: only support 1 channel */
	return 1;
}
EXPORT_SYMBOL(get_emi_ch_num);

int dram_steps_freq(unsigned int step)
{
	int freq = -1;

	switch (step) {
	case 0:
		freq = 1333;
		break;
	case 1:
		freq = 1066;
		break;
	case 2:
		freq = 1066;
		break;
	case 3:
		freq = 1066;
		break;
	default:
		return -1;
	}
	return freq;
}
EXPORT_SYMBOL(dram_steps_freq);

int dram_can_support_fh(void)
{
	if (No_DummyRead)
		return 0;

	/*
	 * LPDDR2 cannot support DVFS
	 */
	if (get_ddr_type() == TYPE_LPDDR2)
		return 0;

	return 1;
}
EXPORT_SYMBOL(dram_can_support_fh);

#ifdef CONFIG_OF_RESERVED_MEM
int dram_dummy_read_reserve_mem_of_init(struct reserved_mem *rmem)
{
	phys_addr_t rptr = 0;
	unsigned int rsize = 0;

	rptr = rmem->base;
	rsize = (unsigned int)rmem->size;

	if (strstr(DRAM_R0_DUMMY_READ_RESERVED_KEY, rmem->name)) {
		if (rsize < DRAM_RSV_SIZE) {
			pr_info("[DRAMC] Can NOT reserve memory for Rank0\n");
			No_DummyRead = 1;
			return 0;
		}
		dram_rank0_addr = rptr;
		dram_rank_num++;
		pr_info("[dram_dummy_read_reserve_mem_of_init] dram_rank0_addr = %pa, size = 0x%x\n",
				&dram_rank0_addr, rsize);
	}

	if (strstr(DRAM_R1_DUMMY_READ_RESERVED_KEY, rmem->name)) {
		if (rsize < DRAM_RSV_SIZE) {
			pr_info("[DRAMC] Can NOT reserve memory for Rank1\n");
			No_DummyRead = 1;
			return 0;
		}
		dram_rank1_addr = rptr;
		dram_rank_num++;
		pr_info("[dram_dummy_read_reserve_mem_of_init] dram_rank1_addr = %pa, size = 0x%x\n",
				&dram_rank1_addr, rsize);
	}

	return 0;
}
RESERVEDMEM_OF_DECLARE(dram_reserve_r0_dummy_read_init,
DRAM_R0_DUMMY_READ_RESERVED_KEY,
			dram_dummy_read_reserve_mem_of_init);
RESERVEDMEM_OF_DECLARE(dram_reserve_r1_dummy_read_init,
DRAM_R1_DUMMY_READ_RESERVED_KEY,
			dram_dummy_read_reserve_mem_of_init);
#endif
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


DRIVER_ATTR(emi_clk_mem_test, 0664,
complex_mem_test_show, complex_mem_test_store);

DRIVER_ATTR(read_dram_data_rate, 0664,
read_dram_data_rate_show, read_dram_data_rate_store);

/*DRIVER_ATTR(dram_dfs, 0664, dram_dfs_show, dram_dfs_store);*/

static int dram_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int i;
	struct resource *res;
	void __iomem *base_temp[3];
	struct device_node *node = NULL;

	pr_info("[DRAMC] module probe.\n");

	for (i = 0; i <= 2; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		base_temp[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(base_temp[i])) {
			pr_info("[DRAMC] unable to map %d base\n", i);
			return -EINVAL;
		}
	}

	DRAMC_AO_CHA_BASE_ADDR = base_temp[0];

	DRAMC_NAO_CHA_BASE_ADDR = base_temp[1];

	DDRPHY_CHA_BASE_ADDR = base_temp[2];

	pr_info("[DRAMC]get DRAMC_AO_CHA_BASE_ADDR @ %p\n", DRAMC_AO_CHA_BASE_ADDR);

	pr_info("[DRAMC]get DDRPHY_CHA_BASE_ADDR @ %p\n", DDRPHY_CHA_BASE_ADDR);

	pr_info("[DRAMC]get DRAMC_NAO_CHA_BASE_ADDR @ %p\n", DRAMC_NAO_CHA_BASE_ADDR);

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (node) {
		SLEEP_BASE_ADDR = of_iomap(node, 0);
		pr_info("[DRAMC]get SLEEP_BASE_ADDR @ %p\n",
		SLEEP_BASE_ADDR);
	} else {
		pr_info("[DRAMC]can't find SLEEP_BASE_ADDR compatible node\n");
		return -1;
	}

	DRAM_TYPE = get_ddr_type();
	pr_info("[DRAMC Driver] dram type =%d\n", DRAM_TYPE);

	if (!DRAM_TYPE) {
		pr_info("[DRAMC Driver] dram type error !!\n");
		return -1;
	}

	pr_info("[DRAMC Driver] Dram Data Rate = %d\n", get_dram_data_rate());
	pr_info("[DRAMC Driver] shuffle_status = %d\n", get_shuffle_status());

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_emi_clk_mem_test);
	if (ret) {
		pr_info("fail to create the emi_clk_mem_test sysfs files\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_read_dram_data_rate);
	if (ret) {
		pr_info("fail to create the read dram data rate sysfs files\n");
		return ret;
	}

	if (dram_can_support_fh())
		pr_info("[DRAMC Driver] dram can support DFS\n");
	else
		pr_info("[DRAMC Driver] dram can not support DFS\n");

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
		pr_info("[DRAMC] init fail, ret 0x%x\n", ret);
		return ret;
	}

	if (of_scan_flat_dt(dt_scan_dram_info, NULL) > 0) {
		pr_info("[DRAMC]find dt_scan_dram_info\n");
	} else {
		pr_info("[DRAMC]can't find dt_scan_dram_info\n");
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
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_dramc_nao_chn_base_get);

void *mt_ddrphy_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DDRPHY_CHA_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_ddrphy_chn_base_get);

#ifdef LAST_DRAMC_IP_BASED
unsigned int mt_dramc_chn_get(unsigned int emi_cona)
{
	switch ((emi_cona >> 8) & 0x3) {
	case 0:
		return 1;
	default:
		pr_info("[LastDRAMC] invalid channel num (emi_cona = 0x%x)\n", emi_cona);
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
	if (rank >= get_dram_info->rank_num)
		return 0;

	return get_dram_info->rank_info[rank].start;
}

unsigned int mt_dramc_ta_support_ranks(void)
{
	/*MT6739 :support single rank only*/
	return SINGLE_RANK;
}

phys_addr_t mt_dramc_ta_reserve_addr(unsigned int rank)
{
	switch (rank) {
	case 0:
		return dram_rank0_addr;
	case 1:
		return dram_rank1_addr;
	default:
		return 0;
	}
}
unsigned int  mt_dramc_ta_addr_set(unsigned int rank, unsigned int temp_addr)
{
	/*MT6739 : 0x38[3:0] ->test addr [31:28],ofset 0x3c[23:0] ->test addr [27:4]*/
	unsigned int test_agent_base_temp;
	test_agent_base_temp = (Reg_Readl(DRAMC_AO_CHA_BASE_ADDR+0x3c) & ~(0xffffff)) | ((temp_addr>>4) & 0xffffff);
	Reg_Sync_Writel(DRAMC_AO_CHA_BASE_ADDR+0x3c, test_agent_base_temp);
	test_agent_base_temp = ((Reg_Readl(DRAMC_AO_CHA_BASE_ADDR+0x38) & ~(0xf)) | ((temp_addr>>28) & 0xf));
	Reg_Sync_Writel(DRAMC_AO_CHA_BASE_ADDR+0x38, test_agent_base_temp);
	/*pr_info("\n\n[LastDRAMC] temp addr =0x%x",test_agent_base_temp);*/
	return 1;
}
EXPORT_SYMBOL(mt_dramc_ta_addr_set);

#endif

unsigned int platform_support_dram_type(void)
{
	return 1;
}
EXPORT_SYMBOL(platform_support_dram_type);

MODULE_DESCRIPTION("MediaTek DRAMC Driver v0.1");
