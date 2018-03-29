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
#include <asm/cacheflush.h>
/*#include <asm/outercache.h>*/
/*#include <asm/system.h>*/
#include <linux/delay.h>
/*#include <mach/mt_reg_base.h>*/
/* #include <mach/mt_clkmgr.h> */
#include <mach/mt_freqhopping.h>
#include <mach/emi_bwl.h>
/*#include <mach/mt_typedefs.h>*/
/*#include <mach/memory.h>*/
/*#include <mach/mt_sleep.h>*/
#include <mach/mt_dramc.h>
#include <mt-plat/dma.h>
/*#include <mach/sync_write.h>*/
#include <linux/of.h>
#include <linux/of_address.h>
#include <mt-plat/mt_io.h>
#include <mt-plat/sync_write.h>
#include <linux/clk.h>

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
#ifdef CONFIG_MTK_DCM
#include <mach/mt_dcm.h>
#endif

#ifdef CONFIG_MD32_SUPPORT
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
#endif

#if 0 /* #ifdef CONFIG_MTK_SLEEP */
#include <mach/mt_sleep.h>
#endif


static void __iomem *CQDMA_BASE_ADDR;
static void __iomem *DRAMCAO_BASE_ADDR;
static void __iomem *DDRPHY_BASE_ADDR;
static void __iomem *DRAMCNAO_BASE_ADDR;

static void __iomem *EMI_DVFS_SRAM_ADDR;
static void __iomem *VCORE_DVFS_ACCESS_BASE_ADDR;

#ifdef CONFIG_MD32_SUPPORT
static void __iomem *MD32_DTCM;
#endif

unsigned char *dst_array_v;
unsigned char *src_array_v;
unsigned int dst_array_p;
unsigned int src_array_p;
unsigned int dtcm_ready = 0;
int init_done = 0;
int org_dram_data_rate = 0;

/*#define MD32_DTCM ((unsigned long)0xF0028000)*/
/*temply for build pass*/

void enter_pasr_dpd_config(unsigned char segment_rank0, unsigned char segment_rank1)
{
	#if 0
	if (segment_rank1 == 0xFF) {	/* all segments of rank1 are not reserved -> rank1 enter DPD */
		slp_dpd_en(1);
		segment_rank1 = 0x0;
	}

	slp_pasr_en(1,
		    segment_rank0 | (segment_rank1 << 8) | (((unsigned short)org_dram_data_rate) <<
							    16));
#endif
}

void exit_pasr_dpd_config(void)
{
	#if 0
	slp_dpd_en(0);
	slp_pasr_en(0, 0);
	#endif
}

#define FREQREG_SIZE 51
#define PLLGRPREG_SIZE	20
vcore_dvfs_info_t __nosavedata g_vcore_dvfs_info;
unsigned int __nosavedata vcore_dvfs_data[((FREQREG_SIZE * 4) + (PLLGRPREG_SIZE * 2) + 2)];


#define CHA_SRAM_LowFreq_RegVal  0x0012FC00
#define CHB_SRAM_LowFreq_RegVal  0x0012FC80
#define CHA_SRAM_HighFreq_RegVal 0x0012FD00
#define CHB_SRAM_HighFreq_RegVal 0x0012FD80
#define PLL_SRAM_LowFreq_RegVal   0x0012FE00
#define PLL_SRAM_HighFreq_RegVal  0x0012FE80
#define SRAM_pll_setting_num           0x0012FF00
#define SRAM_freq_setting_num        (SRAM_pll_setting_num+0x4)

void store_vcore_dvfs_setting(void)
{
	/* unsigned int *atag_ptr, *buf; */
	unsigned int pll_setting_num, freq_setting_num;
	struct device_node *node;

#ifdef CONFIG_OF
	if (of_chosen) {
#if 0
		atag_ptr = (unsigned int *)of_get_property(of_chosen, "atag,vcore_dvfs", NULL);
		if (atag_ptr) {
			atag_ptr += 2;	/* skip tag header */

			memcpy((void *)vcore_dvfs_data, (void *)atag_ptr,
			       (FREQREG_SIZE * 4 + PLLGRPREG_SIZE * 2 + 2) * sizeof(unsigned int));
			buf = (unsigned int *)&vcore_dvfs_data[0];
			pll_setting_num = g_vcore_dvfs_info.pll_setting_num = *buf++;
			freq_setting_num = g_vcore_dvfs_info.freq_setting_num = *buf++;

			ASSERT((pll_setting_num == PLLGRPREG_SIZE)
			       && (freq_setting_num == FREQREG_SIZE));

			/* g_vcore_dvfs_info.low_freq_pll_setting_addr = (unsigned long)buf; */
			g_vcore_dvfs_info.low_freq_pll_setting_addr = (unsigned int)buf;
			buf += pll_setting_num;
			g_vcore_dvfs_info.low_freq_cha_setting_addr = (unsigned int)buf;
			buf += freq_setting_num;
			g_vcore_dvfs_info.low_freq_chb_setting_addr = (unsigned int)buf;
			buf += freq_setting_num;
			g_vcore_dvfs_info.high_freq_pll_setting_addr = (unsigned int)buf;
			buf += pll_setting_num;
			g_vcore_dvfs_info.high_freq_cha_setting_addr = (unsigned int)buf;
			buf += freq_setting_num;
			g_vcore_dvfs_info.high_freq_chb_setting_addr = (unsigned int)buf;
			pr_err
			    ("[vcore dvfs][kernel]low_freq_pll_setting_addr = 0x%x, value[0] = 0x%x\n",
			     g_vcore_dvfs_info.low_freq_pll_setting_addr,
			     *(unsigned int *)(g_vcore_dvfs_info.low_freq_pll_setting_addr));
			pr_err
			    ("[vcore dvfs][kernel]low_freq_cha_setting_addr = 0x%x, value[0] = 0x%x\n",
			     g_vcore_dvfs_info.low_freq_cha_setting_addr,
			     *(unsigned int *)(g_vcore_dvfs_info.low_freq_cha_setting_addr));
			pr_err
			    ("[vcore dvfs][kernel]low_freq_chb_setting_addr = 0x%x, value[0] = 0x%x\n",
			     g_vcore_dvfs_info.low_freq_chb_setting_addr,
			     *(unsigned int *)(g_vcore_dvfs_info.low_freq_chb_setting_addr));
			pr_err
			    ("[vcore dvfs][kernel]high_freq_pll_setting_addr = 0x%x, value[0] = 0x%x\n",
			     g_vcore_dvfs_info.high_freq_pll_setting_addr,
			     *(unsigned int *)(g_vcore_dvfs_info.high_freq_pll_setting_addr));
			pr_err
			    ("[vcore dvfs][kernel]high_freq_cha_setting_addr = 0x%x, value[0] = 0x%x\n",
			     g_vcore_dvfs_info.high_freq_cha_setting_addr,
			     *(unsigned int *)(g_vcore_dvfs_info.high_freq_cha_setting_addr));
			pr_err
			    ("[vcore dvfs][kernel]high_freq_chb_setting_addr = 0x%x, value[0] = 0x%x\n",
			     g_vcore_dvfs_info.high_freq_chb_setting_addr,
			     *(unsigned int *)(g_vcore_dvfs_info.high_freq_chb_setting_addr));
			pr_err("[vcore dvfs][kernel]pll_setting_num = %d\n",
			       g_vcore_dvfs_info.pll_setting_num);
			pr_err("[vcore dvfs][kernel]freq_setting_num = %d\n",
			       g_vcore_dvfs_info.freq_setting_num);
		} else
			pr_err("[%s] No atag,vcore_dvfs!\n", __func__);
#endif

		node = of_find_compatible_node(NULL, NULL, "mediatek,emi_dvfs_sram");
		if (node) {
			EMI_DVFS_SRAM_ADDR = of_iomap(node, 0);
			pr_info("get EMI_DVFS_SRAM_ADDR @ %p\n", EMI_DVFS_SRAM_ADDR);

			#if 0
			g_vcore_dvfs_info.low_freq_pll_setting_addr = 0x200;
			g_vcore_dvfs_info.low_freq_cha_setting_addr = 0x000;
			g_vcore_dvfs_info.low_freq_chb_setting_addr = 0x080;
			g_vcore_dvfs_info.high_freq_pll_setting_addr = 0x280;
			g_vcore_dvfs_info.high_freq_cha_setting_addr = 0x100;
			g_vcore_dvfs_info.high_freq_chb_setting_addr = 0x180;
			pll_setting_num = g_vcore_dvfs_info.pll_setting_num =
			    *(unsigned int *)(EMI_DVFS_SRAM_ADDR + 0x300);
			freq_setting_num = g_vcore_dvfs_info.freq_setting_num =
			    *(unsigned int *)(EMI_DVFS_SRAM_ADDR + 0x304);

			pr_err
			    ("[vcore dvfs][kernel]low_freq_pll_setting_addr = 0x%p, value[0] = 0x%x\n",
			     EMI_DVFS_SRAM_ADDR + 0x200,
			     *(unsigned int *)(EMI_DVFS_SRAM_ADDR + 0x200));
			pr_err
			    ("[vcore dvfs][kernel]low_freq_cha_setting_addr = 0x%p, value[0] = 0x%x\n",
			     EMI_DVFS_SRAM_ADDR + 0x000,
			     *(unsigned int *)(EMI_DVFS_SRAM_ADDR + 0x000));
			pr_err
			    ("[vcore dvfs][kernel]low_freq_chb_setting_addr = 0x%p, value[0] = 0x%x\n",
			     EMI_DVFS_SRAM_ADDR + 0x080,
			     *(unsigned int *)(EMI_DVFS_SRAM_ADDR + 0x080));
			pr_err
			    ("[vcore dvfs][kernel]high_freq_pll_setting_addr = 0x%p, value[0] = 0x%x\n",
			     EMI_DVFS_SRAM_ADDR + 0x280,
			     *(unsigned int *)(EMI_DVFS_SRAM_ADDR + 0x280));
			pr_err
			    ("[vcore dvfs][kernel]high_freq_cha_setting_addr = 0x%p, value[0] = 0x%x\n",
			     EMI_DVFS_SRAM_ADDR + 0x100,
			     *(unsigned int *)(EMI_DVFS_SRAM_ADDR + 0x100));
			pr_err
			    ("[vcore dvfs][kernel]high_freq_chb_setting_addr = 0x%p, value[0] = 0x%x\n",
			     EMI_DVFS_SRAM_ADDR + 0x180,
			     *(unsigned int *)(EMI_DVFS_SRAM_ADDR + 0x180));
			#else
			g_vcore_dvfs_info.low_freq_pll_setting_addr = 0x340;
			g_vcore_dvfs_info.low_freq_cha_setting_addr = 0x000;
			g_vcore_dvfs_info.low_freq_chb_setting_addr = 0x0D0;
			g_vcore_dvfs_info.high_freq_pll_setting_addr = 0x3C0;
			g_vcore_dvfs_info.high_freq_cha_setting_addr = 0x1A0;
			g_vcore_dvfs_info.high_freq_chb_setting_addr = 0x270;
			pll_setting_num = g_vcore_dvfs_info.pll_setting_num =
				*(unsigned int *)(EMI_DVFS_SRAM_ADDR+0x440);
			freq_setting_num = g_vcore_dvfs_info.freq_setting_num =
				*(unsigned int *)(EMI_DVFS_SRAM_ADDR+0x444);

			pr_info("[vcore dvfs][kernel]low_freq_pll_setting_addr = 0x%p, value[0] = 0x%x\n",
				EMI_DVFS_SRAM_ADDR+0x340, *(unsigned int *)(EMI_DVFS_SRAM_ADDR+0x340));
			pr_info("[vcore dvfs][kernel]low_freq_cha_setting_addr = 0x%p, value[0] = 0x%x\n",
				EMI_DVFS_SRAM_ADDR+0x000, *(unsigned int *)(EMI_DVFS_SRAM_ADDR+0x000));
			pr_info("[vcore dvfs][kernel]low_freq_chb_setting_addr = 0x%p, value[0] = 0x%x\n",
				EMI_DVFS_SRAM_ADDR+0x0D0, *(unsigned int *)(EMI_DVFS_SRAM_ADDR+0x0D0));
			pr_info("[vcore dvfs][kernel]high_freq_pll_setting_addr = 0x%p, value[0] = 0x%x\n",
				EMI_DVFS_SRAM_ADDR+0x3C0, *(unsigned int *)(EMI_DVFS_SRAM_ADDR+0x3C0));
			pr_info("[vcore dvfs][kernel]high_freq_cha_setting_addr = 0x%p, value[0] = 0x%x\n",
				EMI_DVFS_SRAM_ADDR+0x1A0, *(unsigned int *)(EMI_DVFS_SRAM_ADDR+0x1A0));
			pr_info("[vcore dvfs][kernel]high_freq_chb_setting_addr = 0x%p, value[0] = 0x%x\n",
				EMI_DVFS_SRAM_ADDR+0x270, *(unsigned int *)(EMI_DVFS_SRAM_ADDR+0x270));
			#endif
			pr_info("[vcore dvfs][kernel]pll_setting_num = %x\n", pll_setting_num);
			pr_info("[vcore dvfs][kernel]freq_setting_num = %x\n", freq_setting_num);
		} else {
			pr_info("can't find compatible node\n");
		}
	} else
		pr_info("[%s] of_chosen is NULL!\n", __func__);
#endif
	{
		unsigned long high, low;
		unsigned int pll_num;
		unsigned long high_cha, high_chb, low_cha, low_chb;
		unsigned int num;

		get_mempll_table_info(&high, &low, &pll_num);
		pr_err("[vcore dvfs]pll_high_addr = 0x%lx, pll_low_addr = 0x%lx, num = %d\n", high,
		       low, pll_num);

		get_freq_table_info(&high_cha, &high_chb, &low_cha, &low_chb, &num);
		/*pr_err
			("[vcore dvfs]high_cha_addr = 0x%lx, high_chb_addr = 0x%lx,
			low_cha_addr = 0x%lx, low_chb_addr = 0x%lx, num = %d\n",
		     high_cha, high_chb, low_cha, low_chb, num);*/
	}
}

#ifdef CONFIG_MD32_SUPPORT
void vcore_dvfs_ipi_handler(int id, void *data, unsigned int len)
{
	int i;
	unsigned int *src_ptr;
	unsigned int *dst_ptr;

	unsigned int pll_setting_num = g_vcore_dvfs_info.pll_setting_num;
	unsigned int freq_setting_num = g_vcore_dvfs_info.freq_setting_num;

	vcore_dvfs_info_t *md32_dvfs_info = (vcore_dvfs_info_t *) data;

	MD32_DTCM = md32_get_dtcm();
	/* *(u32*)(MD32_DTCM + (u32)&(md32_dvfs_info->pll_setting_num)) = pll_setting_num; */
	/* *(u32*)(MD32_DTCM + (u32)&(md32_dvfs_info->freq_setting_num)) = freq_setting_num; */
	*(unsigned int *)(MD32_DTCM + md32_dvfs_info->pll_setting_num) = pll_setting_num;
	*(unsigned int *)(MD32_DTCM + md32_dvfs_info->freq_setting_num) = freq_setting_num;

	pr_err("[vcore dvfs]md32 ipi handler is called\n");

	src_ptr = EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.low_freq_pll_setting_addr;
	dst_ptr = (unsigned int *)(MD32_DTCM + md32_dvfs_info->low_freq_pll_setting_addr);
	pr_err("[vcore dvfs][md32]MD32_DTCM=0x%p\n", MD32_DTCM);
	pr_err("[vcore dvfs][md32]src_ptr=0x%p\n", src_ptr);
	/* pr_err("[vcore dvfs][md32]md32_dvfs_info->low_freq_pll_setting_addr=0x%p\n",
			md32_dvfs_info->low_freq_pll_setting_addr ); */

	for (i = 0; i < pll_setting_num; i++) {
		*dst_ptr = *src_ptr;
		/* pr_err("[vcore dvfs][md32]pll_low: dst_ptr = 0x%p, src_ptr=0x%p,
				value=0x%x\n", dst_ptr, src_ptr, *dst_ptr); */
		dst_ptr++;
		src_ptr++;
	}

	src_ptr = EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.low_freq_cha_setting_addr;
	dst_ptr = (unsigned int *)(MD32_DTCM + md32_dvfs_info->low_freq_cha_setting_addr);
	for (i = 0; i < freq_setting_num; i++) {
		*dst_ptr = *src_ptr;
		/* pr_err("[vcore dvfs][md32]cha_low: dst_ptr = 0x%p, src_ptr=0x%p,
				value=0x%x\n", dst_ptr, src_ptr, *dst_ptr); */
		dst_ptr++;
		src_ptr++;
	}

	src_ptr = EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.low_freq_chb_setting_addr;
	dst_ptr = (unsigned int *)(MD32_DTCM + md32_dvfs_info->low_freq_chb_setting_addr);
	for (i = 0; i < freq_setting_num; i++) {
		*dst_ptr = *src_ptr;
		/* pr_err("[vcore dvfs][md32]chb_low: dst_ptr = 0x%p, src_ptr=0x%p,
				value=0x%x\n", dst_ptr, src_ptr, *dst_ptr); */
		dst_ptr++;
		src_ptr++;
	}

	src_ptr = EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.high_freq_pll_setting_addr;
	dst_ptr = (unsigned int *)(MD32_DTCM + md32_dvfs_info->high_freq_pll_setting_addr);
	for (i = 0; i < pll_setting_num; i++) {
		*dst_ptr = *src_ptr;
		/* pr_err("[vcore dvfs][md32]pll_high: dst_ptr = 0x%p, src_ptr=0x%p,
				value=0x%x\n", dst_ptr, src_ptr, *dst_ptr); */
		dst_ptr++;
		src_ptr++;
	}

	src_ptr = EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.high_freq_cha_setting_addr;
	dst_ptr = (unsigned int *)(MD32_DTCM + md32_dvfs_info->high_freq_cha_setting_addr);
	for (i = 0; i < freq_setting_num; i++) {
		*dst_ptr = *src_ptr;
		/* pr_err("[vcore dvfs][md32]cha_high: dst_ptr = 0x%p, src_ptr=0x%p,
				value=0x%x\n", dst_ptr, src_ptr, *dst_ptr); */
		dst_ptr++;
		src_ptr++;
	}

	src_ptr = EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.high_freq_chb_setting_addr;
	dst_ptr = (unsigned int *)(MD32_DTCM + md32_dvfs_info->high_freq_chb_setting_addr);
	for (i = 0; i < freq_setting_num; i++) {
		*dst_ptr = *src_ptr;
		/* pr_err("[vcore dvfs][md32]chb_high: dst_ptr = 0x%p, src_ptr=0x%p,
				value=0x%x\n", dst_ptr, src_ptr, *dst_ptr); */
		dst_ptr++;
		src_ptr++;
	}
	dtcm_ready = 1;
}
#endif

void get_freq_table_info(unsigned long *high_cha_addr, unsigned long *high_chb_addr,
			 unsigned long *low_cha_addr, unsigned long *low_chb_addr,
			 unsigned int *num)
{
	unsigned int freq_setting_num;

	freq_setting_num = g_vcore_dvfs_info.freq_setting_num;
	if (freq_setting_num != 0) {
		*high_cha_addr =
		    (unsigned long)EMI_DVFS_SRAM_ADDR +
		    g_vcore_dvfs_info.high_freq_cha_setting_addr;
		*high_chb_addr =
		    (unsigned long)EMI_DVFS_SRAM_ADDR +
		    g_vcore_dvfs_info.high_freq_chb_setting_addr;
		*low_cha_addr =
		    (unsigned long)EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.low_freq_cha_setting_addr;
		*low_chb_addr =
		    (unsigned long)EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.low_freq_chb_setting_addr;
		*num = freq_setting_num;
	} else {
		*high_cha_addr = *high_chb_addr = *low_cha_addr = *low_chb_addr = *num = 0;
	}
}


void get_mempll_table_info(unsigned long *high_addr, unsigned long *low_addr, unsigned int *num)
{
	unsigned int pll_setting_num;

	pll_setting_num = g_vcore_dvfs_info.pll_setting_num;
	if (pll_setting_num != 0) {
		*high_addr =
		    (unsigned long)EMI_DVFS_SRAM_ADDR +
		    g_vcore_dvfs_info.high_freq_pll_setting_addr;
		*low_addr =
		    (unsigned long)EMI_DVFS_SRAM_ADDR + g_vcore_dvfs_info.low_freq_pll_setting_addr;
		*num = pll_setting_num;
	} else {
		*high_addr = *low_addr = *num = 0;
	}
}


#define MEM_TEST_SIZE 0x2000
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5

int Binning_DRAM_complex_mem_test(void)
{
	unsigned char *MEM8_BASE;
	unsigned short *MEM16_BASE;
	unsigned int *MEM32_BASE;
	unsigned int *MEM_BASE;
	unsigned char pattern8;
	unsigned short pattern16;
	unsigned int i, j, size, pattern32;
	unsigned int value;
	unsigned int len = MEM_TEST_SIZE;
	void *ptr;
	unsigned long mem_ptr;

	ptr = vmalloc(PAGE_SIZE*2);
	MEM8_BASE = (unsigned char *)ptr;
	MEM16_BASE = (unsigned short *)ptr;
	MEM32_BASE = (unsigned int *)ptr;
	MEM_BASE = (unsigned int *)ptr;

	pr_err("Test DRAM start address 0x%lx\n", (unsigned long)ptr);
	pr_err("Test DRAM SIZE 0x%x\n", MEM_TEST_SIZE);
	size = len >> 2;

	/* === Verify the tied bits (tied high) === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0;

	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0) {
			vfree(ptr);
			return -1;
		} else
		MEM32_BASE[i] = 0xffffffff;
	}

	/* === Verify the tied bits (tied low) === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			vfree(ptr);
			return -2;
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
			vfree(ptr);
			return -3;
		}
	}

	/* === Verify pattern 2 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = j = 0; i < len; i += 2, j++) {
		if (MEM8_BASE[i] == pattern8)
			MEM16_BASE[j] = pattern8;
		if (MEM16_BASE[j] != pattern8) {
			vfree(ptr);
			return -4;
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
			vfree(ptr);
			return -5;
		}
	}

	/* === Verify pattern 4 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++)
		MEM32_BASE[i] = pattern32++;
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++) {
		if (MEM32_BASE[i] != pattern32++) {
			vfree(ptr);
			return -6;
		}
	}

	/* === Pattern 5: Filling memory range with 0x44332211 === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0x44332211;

	/* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x44332211) {
			vfree(ptr);
			return -7;
		} else {
		MEM32_BASE[i] = 0xa5a5a5a5;
	}
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 0h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a5a5) {
			vfree(ptr);
			return -8;
		} else {
		MEM8_BASE[i * 4] = 0x00;
	}
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 2h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a500) {
			vfree(ptr);
			return -9;
		} else {
		MEM8_BASE[i * 4 + 2] = 0x00;
	}
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 1h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa500a500) {
			vfree(ptr);
			return -10;
		} else {
		MEM8_BASE[i * 4 + 1] = 0x00;
	}
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 3h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5000000) {
			vfree(ptr);
			return -11;
		} else {
		MEM8_BASE[i * 4 + 3] = 0x00;
	}
	}

	/* === Read Check then Fill Memory with ffff Word Pattern at offset 1h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x00000000) {
			vfree(ptr);
			return -12;
		} else {
		MEM16_BASE[i * 2 + 1] = 0xffff;
	}
	}


	/* === Read Check then Fill Memory with ffff Word Pattern at offset 0h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffff0000) {
			vfree(ptr);
			return -13;
		} else {
		MEM16_BASE[i * 2] = 0xffff;
	}
	}
	/*===  Read Check === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			vfree(ptr);
			return -14;
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
			vfree(ptr);
			return -15;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 3 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			vfree(ptr);
			return -16;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 4 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			vfree(ptr);
			return -17;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 5 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			vfree(ptr);
			return -18;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 6 => read 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			vfree(ptr);
			return -19;
		}
	}

	/* === 1/2/4-byte combination test === */
	mem_ptr = (unsigned long)MEM_BASE;
	while (mem_ptr < ((unsigned long)MEM_BASE + (size << 2))) {
		*((unsigned char *) mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *) mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *) mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *) mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *) mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *) mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *) mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *) mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned char *) mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *) mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *) mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *) mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *) mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *) mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *) mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *) mem_ptr) = 0x12345678;
		mem_ptr += 4;
	}

	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != 0x12345678) {
			vfree(ptr);
			return -20;
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
			vfree(ptr);
			return -21;
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
			vfree(ptr);
			return -22;
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
			vfree(ptr);
			return -23;
		}
		pattern32++;
	}
	pr_err("complex R/W mem test pass\n");
	vfree(ptr);
	return 1;
}

static ssize_t complex_mem_test_show(struct device_driver *driver, char *buf)
{
	int ret;

	ret = Binning_DRAM_complex_mem_test();

	if (ret > 0)
		return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
	else
		return snprintf(buf, PAGE_SIZE, "MEM TEST failed %d\n", ret);
}

static ssize_t complex_mem_test_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

#ifdef APDMA_TEST
static ssize_t DFS_APDMA_TEST_show(struct device_driver *driver, char *buf)
{
	dma_dummy_read_for_vcorefs(7);
	return snprintf(buf, PAGE_SIZE, "DFS APDMA Dummy Read Address 0x%x\n",
			(unsigned int)src_array_p);
}

static ssize_t DFS_APDMA_TEST_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}
#endif

unsigned int ucDram_Register_Read(unsigned long u4reg_addr)
{
	unsigned int pu4reg_value;

	pu4reg_value = (*(volatile unsigned int *)(DRAMCAO_BASE_ADDR + (u4reg_addr))) |
	    (*(volatile unsigned int *)(DDRPHY_BASE_ADDR + (u4reg_addr))) |
	    (*(volatile unsigned int *)(DRAMCNAO_BASE_ADDR + (u4reg_addr)));

	return pu4reg_value;
}

void ucDram_Register_Write(unsigned long u4reg_addr, unsigned int u4reg_value)
{
	(*(volatile unsigned int *)(DRAMCAO_BASE_ADDR + (u4reg_addr))) = u4reg_value;
	(*(volatile unsigned int *)(DDRPHY_BASE_ADDR + (u4reg_addr))) = u4reg_value;
	(*(volatile unsigned int *)(DRAMCNAO_BASE_ADDR + (u4reg_addr))) = u4reg_value;
	/*dsb();*/
	mb();
}

bool pasr_is_valid(void)
{
#if 0
	unsigned int ddr_type = 0;

	ddr_type = get_ddr_type();
	/* Following DDR types can support PASR */
	if (ddr_type == LPDDR3_1866 || ddr_type == DUAL_LPDDR3_1600 || ddr_type == LPDDR2)
		return true;
#endif
	return false;
}

struct clk *mpll_clk;
static int mpll_probe(struct platform_device *pdev)
{
	mpll_clk = devm_clk_get(&pdev->dev, "mpll");
	BUG_ON(IS_ERR(mpll_clk));

	return 0;
}


unsigned int get_dram_data_rate(void)
{
	unsigned int u4value1;
	/*u4value2, MPLL_POSDIV, MPLL_PCW;*/
	unsigned int MEMPLL_FBKDIV, MPLL_FOUT, MEMPLL_FOUT;

	MPLL_FOUT = clk_get_rate(mpll_clk);

	if (MPLL_FOUT == 0) {
		pr_err("mpll_probe is not ready");
		return 0;
	}

	u4value1 = *(volatile unsigned int *)(DDRPHY_BASE_ADDR + 0x614);
	MEMPLL_FBKDIV = (u4value1 & 0x007f0000) >> 16;

	MEMPLL_FOUT = DIV_ROUND_CLOSEST(MPLL_FOUT, 28);
	BUG_ON(MEMPLL_FOUT > UINT_MAX / 4 / (MEMPLL_FBKDIV + 1));
	MEMPLL_FOUT = MEMPLL_FOUT * 1 * 4 * (MEMPLL_FBKDIV + 1);
	MEMPLL_FOUT = DIV_ROUND_CLOSEST(MEMPLL_FOUT, 1000000);

	pr_err("MPLL_FOUT=%d, MEMPLL_FBKDIV=%d, MEMPLL_FOUT=%d\n",
			MPLL_FOUT, MEMPLL_FBKDIV, MEMPLL_FOUT);

/*	pr_err("MPLL_POSDIV=%d, MPLL_PCW=0x%x, MPLL_FOUT=%d, MEMPLL_FBKDIV=%d, MEMPLL_FOUT=%d\n",
       MPLL_POSDIV, MPLL_PCW, MPLL_FOUT, MEMPLL_FBKDIV, MEMPLL_FOUT);
*/

	return MEMPLL_FOUT;
}

unsigned int DRAM_MRR(int MRR_num)
{
	unsigned int MRR_value = 0x0;
	unsigned int u4value;

	/* set DQ bit 0, 1, 2, 3, 4, 5, 6, 7 pinmux for LPDDR3 */
	ucDram_Register_Write(DRAMC_REG_RRRATE_CTL, 0x13121110);
	ucDram_Register_Write(DRAMC_REG_MRR_CTL, 0x17161514);

	ucDram_Register_Write(DRAMC_REG_MRS, MRR_num);
	ucDram_Register_Write(DRAMC_REG_SPCMD, ucDram_Register_Read(DRAMC_REG_SPCMD) | 0x00000002);
	/* udelay(1); */
	while ((ucDram_Register_Read(DRAMC_REG_SPCMDRESP) & 0x02) == 0)
		;
	ucDram_Register_Write(DRAMC_REG_SPCMD, ucDram_Register_Read(DRAMC_REG_SPCMD) & 0xFFFFFFFD);


	u4value = ucDram_Register_Read(DRAMC_REG_SPCMDRESP);
	MRR_value = (u4value >> 20) & 0xFF;

	return MRR_value;
}

unsigned int read_dram_temperature(void)
{
	unsigned int value;

	value = DRAM_MRR(4) & 0x7;
	return value;
}

#ifdef READ_DRAM_TEMP_TEST
static ssize_t read_dram_temp_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DRAM MR4 = 0x%x\n", read_dram_temperature());
}

static ssize_t read_dram_temp_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}
#endif

static ssize_t read_dram_data_rate_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DRAM data rate = %d\n", get_dram_data_rate());
}

static ssize_t read_dram_data_rate_store(struct device_driver *driver, const char *buf,
					 size_t count)
{
	return count;
}

DRIVER_ATTR(emi_clk_mem_test, 0664, complex_mem_test_show, complex_mem_test_store);

#ifdef APDMA_TEST
DRIVER_ATTR(dram_dummy_read_test, 0664, DFS_APDMA_TEST_show, DFS_APDMA_TEST_store);
#endif

#ifdef READ_DRAM_TEMP_TEST
DRIVER_ATTR(read_dram_temp_test, 0664, read_dram_temp_show, read_dram_temp_store);
#endif

DRIVER_ATTR(read_dram_data_rate, 0664, read_dram_data_rate_show, read_dram_data_rate_store);

static struct device_driver dram_test_drv = {
	.name = "emi_clk_test",
	.bus = &platform_bus_type,
	.owner = THIS_MODULE,
};

static const struct of_device_id mpll_of_match[] = {
	{.compatible = "mediatek,mt8173-ddrphy",},
	{},
};
MODULE_DEVICE_TABLE(of, mpll_of_match);

static struct platform_driver mpll_drv = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = mpll_probe,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "mt-mpll",
		.of_match_table = of_match_ptr(mpll_of_match),
	},
};

#define TAG     "[DDR /vcore] "

/*#define cpufreq_err(fmt, args...)       \
	printk(KERN_ERR TAG KERN_CONT fmt, ##args)
*/
static int __init dram_test_init(void)
{
	int ret;
	int err;
	struct device_node *node;

	/* DTS version */
	/* BUG_ON(1); */

	err = platform_driver_register(&mpll_drv);

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-cqdma");
	if (node) {
		CQDMA_BASE_ADDR = of_iomap(node, 0);
		pr_err("[DRAMC]get CQDMA_BASE_ADDR @ %p\n", CQDMA_BASE_ADDR);
	} else {
		pr_err("[DRAMC]can't find CQDMA_BASE_ADDR compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-dramco");
	if (node) {
		DRAMCAO_BASE_ADDR = of_iomap(node, 0);
		pr_err("[DRAMC]get DRAMCAO_BASE_ADDR @ %p\n", DRAMCAO_BASE_ADDR);
	} else {
		pr_err("[DRAMC]can't find DRAMC0 compatible node\n");
		/* return -1; */
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-ddrphy");
	if (node) {
		DDRPHY_BASE_ADDR = of_iomap(node, 0);
		pr_err("[DRAMC]get DDRPHY_BASE_ADDR @ %p\n", DDRPHY_BASE_ADDR);
	} else {
		pr_err("[DRAMC]can't find DDRPHY compatible node\n");
		/* return -1; */
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-dramc_nao");
	if (node) {
		DRAMCNAO_BASE_ADDR = of_iomap(node, 0);
		pr_err("[DRAMC]get DRAMCNAO_BASE_ADDR @ %p\n", DRAMCNAO_BASE_ADDR);
	} else {
		pr_err("[DRAMC]can't find DRAMCNAO compatible node\n");
		/* return -1; */
	}

	node = of_find_compatible_node(NULL, NULL, "vcore_dvfs-reserve-memory");
	if (node) {
		VCORE_DVFS_ACCESS_BASE_ADDR = of_iomap(node, 0);
		pr_err("[DRAMC]get vcore_dvfs-reserve-memory @ %p\n", VCORE_DVFS_ACCESS_BASE_ADDR);
	} else {
		pr_err("[DRAMC]can't find VCORE_DVFS_ACCESS_BASE_ADDR compatible node\n");
		/* return -1; */
	}

	ret = driver_register(&dram_test_drv);
	if (ret) {
		pr_err("fail to create the dram_test driver\n");
		/* return ret; */
	}

	ret = driver_create_file(&dram_test_drv, &driver_attr_emi_clk_mem_test);
	if (ret) {
		pr_err("fail to create the emi_clk_mem_test sysfs files\n");
		/* return ret; */
	}
#ifdef APDMA_TEST
	ret = driver_create_file(&dram_test_drv, &driver_attr_dram_dummy_read_test);
	if (ret) {
		pr_err("fail to create the DFS sysfs files\n");
		/* return ret; */
	}
#endif

#ifdef READ_DRAM_TEMP_TEST
	ret = driver_create_file(&dram_test_drv, &driver_attr_read_dram_temp_test);
	if (ret) {
		pr_err("fail to create the read dram temp sysfs files\n");
		/* return ret; */
	}
#endif

	ret = driver_create_file(&dram_test_drv, &driver_attr_read_dram_data_rate);
	if (ret) {
		pr_err("fail to create the read dram data rate sysfs files\n");
		/* return ret; */
	}

	pr_err("[DRAMC Driver] Store Vcore DVFS settings...\n");
	store_vcore_dvfs_setting();

#ifdef CONFIG_MD32_SUPPORT
	pr_err("[DRAMC Driver] Register MD32 Vcore DVFS Handler...\n");
	md32_ipi_registration(IPI_VCORE_DVFS, vcore_dvfs_ipi_handler, "vcore_dvfs");
#endif

	org_dram_data_rate = get_dram_data_rate();
	pr_err("[DRAMC Driver] Dram Data Rate = %d\n", org_dram_data_rate);

	return 0;
}

int DFS_APDMA_early_init(void)
{
	phys_addr_t max_dram_size = get_max_DRAM_size();
	/*phys_addr_t max_dram_size = 0x80000000;*/
	phys_addr_t dummy_read_center_address;

	if (init_done == 0) {
		if (max_dram_size == 0x100000000ULL) {	/* dram size = 4GB */
			dummy_read_center_address = 0x80000000ULL;
			pr_err("[DRAMC Driver] 4GB mode read_center_address = 0x80000000\n");
		} else if (max_dram_size <= 0xC0000000) {	/* dram size <= 3GB */
			dummy_read_center_address = DRAM_BASE + (max_dram_size >> 1);
		} else {
			ASSERT(0);
		}

		src_array_p = (volatile unsigned int)(dummy_read_center_address - (BUFF_LEN >> 1));
		dst_array_p = (volatile unsigned int)(dummy_read_center_address + (BUFF_LEN >> 1));

#ifdef APDMAREG_DUMP
		src_array_v =
		    ioremap(rounddown(src_array_p, IOREMAP_ALIGMENT),
			    IOREMAP_ALIGMENT << 1) + IOREMAP_ALIGMENT - (BUFF_LEN >> 1);
		dst_array_v = src_array_v + BUFF_LEN;
#endif

		init_done = 1;
	}

	return 1;
}

int DFS_APDMA_Init(void)
{
	writel(((~DMA_GSEC_EN_BIT)&readl(DMA_GSEC_EN)), DMA_GSEC_EN);
	return 1;
}

int DFS_APDMA_Enable(void)
{
#ifdef APDMAREG_DUMP
	int i;
#endif

	while (readl(DMA_START) & 0x1)
		;
	writel(src_array_p, DMA_SRC);
	writel(dst_array_p, DMA_DST);
	writel(BUFF_LEN , DMA_LEN1);
	writel(DMA_CON_BURST_8BEAT, DMA_CON);

#ifdef APDMAREG_DUMP
	pr_err("src_p=0x%x, dst_p=0x%x, src_v=0x%x, dst_v=0x%x, len=%d\n", src_array_p, dst_array_p,
	       (unsigned int)src_array_v, (unsigned int)dst_array_v, BUFF_LEN);
	for (i = 0; i < 0x60; i += 4) {
		pr_err("[Before]addr:0x%x, value:%x\n", (unsigned int)(DMA_BASE + i),
		       *((volatile int *)(DMA_BASE + i)));
	}

#ifdef APDMA_TEST
	for (i = 0; i < BUFF_LEN/sizeof(unsigned int); i++) {
		dst_array_v[i] = 0;
		src_array_v[i] = i;
	}
#endif
#endif

	mt_reg_sync_writel(0x1, DMA_START);

#ifdef APDMAREG_DUMP
	for (i = 0; i < 0x60; i += 4) {
		pr_err("[AFTER]addr:0x%x, value:%x\n", (unsigned int)(DMA_BASE + i),
		       *((volatile int *)(DMA_BASE + i)));
	}

#ifdef APDMA_TEST
	for (i = 0; i < BUFF_LEN/sizeof(unsigned int); i++) {
		if (dst_array_v[i] != src_array_v[i]) {
			pr_err("DMA ERROR at Address %x\n (i=%d, value=0x%x(should be 0x%x))",
				(unsigned int)&dst_array_v[i], i, dst_array_v[i], src_array_v[i]);
			ASSERT(0);
		}
	}
	pr_err("Channe0 DFS DMA TEST PASS\n");
#endif
#endif
	return 1;
}

int DFS_APDMA_END(void)
{
	while (readl(DMA_START))
		;
	return 1;
}

void dma_dummy_read_for_vcorefs(int loops)
{
	int i , j;
	unsigned int dummy_read_value;

	for (j = 0 ; j < loops ; j++) {
		for (i = 0 ; i < 100 ; i++)
			dummy_read_value = *(volatile unsigned int *)(VCORE_DVFS_ACCESS_BASE_ADDR + 0xe50);

		for (i = 0 ; i < 100 ; i++)
			dummy_read_value = *(volatile unsigned int *)(VCORE_DVFS_ACCESS_BASE_ADDR + 0xf50);

		for (i = 0 ; i < 100 ; i++)
			dummy_read_value = *(volatile unsigned int *)(VCORE_DVFS_ACCESS_BASE_ADDR + 0x1050);

		for (i = 0 ; i < 100 ; i++)
			dummy_read_value = *(volatile unsigned int *)(VCORE_DVFS_ACCESS_BASE_ADDR + 0x1150);
	}
}

/*
 * XXX: Reserved memory in low memory must be 1MB aligned.
 *	 This is because the Linux kernel always use 1MB section to map low memory.
 *
 *	We Reserved the memory regien which could cross rank for APDMA to do dummy read.
 *
 */

void DFS_Reserved_Memory(void)
{
	phys_addr_t high_memory_phys;
	phys_addr_t DFS_dummy_read_center_address;
	phys_addr_t max_dram_size = get_max_DRAM_size();
	/*phys_addr_t max_dram_size = 0x80000000;*/

	high_memory_phys = virt_to_phys(high_memory);

	if (max_dram_size == 0x100000000ULL) {	/* dram size = 4GB */
		DFS_dummy_read_center_address = 0x80000000ULL;
	} else if (max_dram_size <= 0xC0000000) {	/* dram size <= 3GB */
		DFS_dummy_read_center_address = DRAM_BASE+(max_dram_size >> 1);
	} else {
		ASSERT(0);
	}

	/*For DFS Purpose, we remove this memory block for Dummy read/write by APDMA.*/
	pr_err("[DFS Check]DRAM SIZE:0x%llx\n", (unsigned long long)max_dram_size);
	pr_err("[DFS Check]DRAM Dummy read from:0x%llx to 0x%llx\n",
		(unsigned long long)(DFS_dummy_read_center_address-(BUFF_LEN >> 1)),
		(unsigned long long)(DFS_dummy_read_center_address+(BUFF_LEN >> 1)));
	pr_err("[DFS Check]DRAM Dummy read center address:0x%llx\n",
		(unsigned long long)DFS_dummy_read_center_address);
	pr_err("[DFS Check]High Memory start address 0x%llx\n",
	       (unsigned long long)high_memory_phys);

	if ((DFS_dummy_read_center_address - SZ_4K) >= high_memory_phys) {
		pr_err("[DFS Check]DFS Dummy read reserved 0x%llx to 0x%llx\n",
			(unsigned long long)(DFS_dummy_read_center_address-SZ_4K),
			(unsigned long long)(DFS_dummy_read_center_address+SZ_4K));
		memblock_reserve(DFS_dummy_read_center_address-SZ_4K, (SZ_4K << 1));
		memblock_free(DFS_dummy_read_center_address-SZ_4K, (SZ_4K << 1));
		memblock_remove(DFS_dummy_read_center_address-SZ_4K, (SZ_4K << 1));
	} else {
#ifndef CONFIG_ARM_LPAE
		pr_err("[DFS Check]DFS Dummy read reserved 0x%llx to 0x%llx\n",
		(unsigned long long)(DFS_dummy_read_center_address-SZ_1M),
		(unsigned long long)(DFS_dummy_read_center_address+SZ_1M));
		memblock_reserve(DFS_dummy_read_center_address-SZ_1M, (SZ_1M << 1));
		memblock_free(DFS_dummy_read_center_address-SZ_1M, (SZ_1M << 1));
		memblock_remove(DFS_dummy_read_center_address-SZ_1M, (SZ_1M << 1));
#else
		pr_err("[DFS Check]DFS Dummy read reserved 0x%llx to 0x%llx\n",
		(unsigned long long)(DFS_dummy_read_center_address-SZ_2M),
		(unsigned long long)(DFS_dummy_read_center_address+SZ_2M));
		memblock_reserve(DFS_dummy_read_center_address-SZ_2M, (SZ_2M << 1));
		memblock_free(DFS_dummy_read_center_address-SZ_2M, (SZ_2M << 1));
		memblock_remove(DFS_dummy_read_center_address-SZ_2M, (SZ_2M << 1));
#endif
	}

}

void sync_hw_gating_value(void)
{
	unsigned int reg_val;

	reg_val = (*(volatile unsigned int *)(0xF0004028)) & (~(0x01 << 30));	/*  cha DLLFRZ=0 */
	mt_reg_sync_writel(reg_val, 0xF0004028);
	reg_val = (*(volatile unsigned int *)(0xF0011028)) & (~(0x01 << 30));	/*  chb DLLFRZ=0 */
	mt_reg_sync_writel(reg_val, 0xF0011028);

	mt_reg_sync_writel((*(volatile unsigned int *)(0xF020e374)), 0xF0004094);	/*  cha r0  */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF020e378)), 0xF0004098);	/*  cha r1 */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF0213374)), 0xF0011094);	/*  chb r0   */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF0213378)), 0xF0011098);	/*  chb r1 */

	reg_val = (*(volatile unsigned int *)(0xF0004028)) | (0x01 << 30);	/*  cha DLLFRZ=1 */
	mt_reg_sync_writel(reg_val, 0xF0004028);
	reg_val = (*(volatile unsigned int *)(0xF0011028)) | (0x01 << 30);	/*  chb DLLFRZ=0 */
	mt_reg_sync_writel(reg_val, 0xF0011028);
}

void disable_MR4_enable_manual_ref_rate(void)
{
	/* disable changelA MR4 */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF00041e8)) | (1 << 26), 0xF00041e8);
	/* disable changelB MR4 */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF00111e8)) | (1 << 26), 0xF00111e8);

	udelay(10);

	/* before deepidle */
	/* set R_DMREFRATE_MANUAL_TRIG=1 to change refresh_rate=h3 */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF0004114)) | 0x80000000, 0xF0004114);
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF0011114)) | 0x80000000, 0xF0011114);

	udelay(10);
	}

void enable_MR4_disable_manual_ref_rate(void)
{
	/* After leave self refresh, set R_DMREFRATE_MANUAL_TRIG=0  */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF0004114)) & 0x7FFFFFFF, 0xF0004114);
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF0011114)) & 0x7FFFFFFF, 0xF0011114);

	udelay(10);

	/* enable changelA MR4 */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF00041e8)) & ~(1 << 26), 0xF00041e8);
	/* enable changelB MR4 */
	mt_reg_sync_writel((*(volatile unsigned int *)(0xF00111e8)) & ~(1 << 26), 0xF00111e8);
}

arch_initcall(dram_test_init);
/* module_init(dram_test_init); */
