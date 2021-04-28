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
#include <mt-plat/mtk_meminfo.h>
#include <mt-plat/mtk_chip.h>
#include <mt-plat/aee.h>

#include "mtk_dramc.h"
#include "dramc.h"



#define Reg_Sync_Writel(addr, val)   writel(val, IOMEM(addr))
#define Reg_Readl(addr) readl(IOMEM(addr))

#ifdef LAST_DRAMC_IP_BASED
static void __iomem *(*get_emi_base)(void);
__weak unsigned int mt_dramc_ta_addr_set(unsigned int rank,
		unsigned int temp_addr)
{
	return 0;
}

__weak unsigned int platform_support_dram_type(void)
{
	return 0;
}

static int __init set_single_channel_test_angent(int channel)
{
	void __iomem *dramc_ao_base = NULL;
	void __iomem *emi_base;
	unsigned int bit_scramble, bit_xor, bit_shift;
	unsigned int emi_cona, emi_conf;
	unsigned int channel_position, channel_num, channel_width = 0;
	unsigned int rank, temp, rank_max;
	unsigned int ddr_type;
	unsigned int base_reg[2] = { 0x94, 0x8c };
	phys_addr_t test_agent_base, rank_base;

	emi_base = get_emi_base();
	if (emi_base == NULL) {
		pr_err("[LastDRAMC] can't find EMI base\n");
		return -1;
	}
	emi_cona = readl(IOMEM(emi_base+0x000));
	emi_conf = readl(IOMEM(emi_base+0x028))>>8;

	channel_num = mt_dramc_chn_get(emi_cona);

	if (channel < 0 || channel >= channel_num) {
		pr_err("[LastDRAMC] invalid channel: %d\n", channel);
		return -1;
	}

	dramc_ao_base = mt_dramc_chn_base_get(channel);

	if (!dramc_ao_base) {
		pr_err("[LastDRAMC] no dramc_ao_base, skip channel %d\n",
			channel);
		return -1;
	}

	rank_max = mt_dramc_ta_support_ranks();

	if (rank_max > 2) {
		pr_err("[LastDRAMC] invalid rank num, rank_max %u\n",
			rank_max);
		return -1;
	}

	ddr_type = get_ddr_type();

	for (rank = 0; rank < rank_max; rank++) {
		rank_base = mt_dramc_rankbase_get(rank);

		if (!rank_base) {
			pr_err("[LastDRAMC] invalid base, rank %u\n",
				rank);
			return -1;
		}

		test_agent_base = mt_dramc_ta_reserve_addr(rank);

		if (!test_agent_base) {
			pr_err("[LastDRAMC] invalid addr, rank %u\n",
				rank);
			return -1;
		}

		test_agent_base = (test_agent_base - rank_base) & 0xFFFFFFFF;

		/* calculate DRAM base address (test_agent_base) */
		/* pr_err("[LastDRAMC] reserved address before emi: */
		/* %llx\n", test_agent_base); */
		for (bit_scramble = 11; bit_scramble < 17; bit_scramble++) {
			bit_xor = (emi_conf >> (4*(bit_scramble-11))) & 0xf;
			bit_xor &= test_agent_base >> 16;
			for (bit_shift = 0; bit_shift < 4; bit_shift++)
				test_agent_base ^= ((bit_xor>>bit_shift)&0x1)
					<< bit_scramble;
		}

		/* pr_err("[LastDRAMC] reserved address after emi: %llx\n", */
		/* test_agent_base); */

		if (channel_num > 1) {
			channel_position = mt_dramc_chp_get(emi_cona);

			for (channel_width = bit_shift = 0; bit_shift < 4;
			bit_shift++) {
				if ((1 << bit_shift) >= channel_num)
					break;
				channel_width++;
			}

			temp = ((test_agent_base &
				~(((0x1<<channel_width)-1)<<channel_position))
				>> channel_width);
			test_agent_base = temp |
				(test_agent_base & ((0x1<<channel_position)-1));
		}
		/* pr_err("[LastDRAMC] reserved address after emi: %llx\n", */
		/* test_agent_base); */

		/* set base address for test agent */
		temp = Reg_Readl(dramc_ao_base+base_reg[rank]) & 0xF;
		if ((ddr_type == TYPE_LPDDR4) || (ddr_type == TYPE_LPDDR4X))
			temp |= (test_agent_base>>1) & 0xFFFFFFF0;
		else if ((ddr_type == TYPE_LPDDR3) ||
				platform_support_dram_type())
			temp |= (test_agent_base) & 0xFFFFFFF0;
		else {
			pr_err("[LastDRAMC] undefined DRAM type\n");
			return -1;
		}

		if (!mt_dramc_ta_addr_set(rank, temp))
			Reg_Sync_Writel(dramc_ao_base+base_reg[rank], temp);

	}

	if (rank_max > 1)
		Reg_Sync_Writel(dramc_ao_base+0x9c,
			Reg_Readl(dramc_ao_base+0x9c) | (rank_max - 1));

	/* write test pattern */

	return 0;
}

static int __init last_dramc_test_agent_init(void)
{
	void __iomem *emi_base;
	unsigned int emi_cona, i, channel_num;

	get_emi_base = (void __iomem *)symbol_get(mt_emi_base_get);
	if (get_emi_base == NULL) {
		pr_err("[LastDRAMC] mt_emi_base_get is NULL\n");
		return 0;
	}

	emi_base = get_emi_base();
	if (emi_base == NULL) {
		pr_err("[LastDRAMC] can't find EMI base\n");
		return 0;
	}
	emi_cona = readl(IOMEM(emi_base+0x000));

	channel_num = mt_dramc_chn_get(emi_cona);

	for (i = 0; i < channel_num; ++i)
		set_single_channel_test_angent(i);

	symbol_put(mt_emi_base_get);
	get_emi_base = NULL;

	return 0;
}

late_initcall(last_dramc_test_agent_init);

static int dram_calib_perf_check_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	void __iomem *DRAMC_SRAM_DEBUG_BASE_ADDR;
	unsigned long val, last_dramc_ofs;

	if (node) {
		DRAMC_SRAM_DEBUG_BASE_ADDR = of_iomap(node, 0);
	} else {
		pr_err("can't find compatible node for dram_calib_perf_check\n");
		return -ENODEV;
	}

	last_dramc_ofs = 0;

#ifdef LAST_DRAMC_SRAM_MGR
	for (last_dramc_ofs = 0; last_dramc_ofs < DBG_INFO_TYPE_MAX;
	last_dramc_ofs++) {
		val = Reg_Readl(DRAMC_SRAM_DEBUG_BASE_ADDR +
			(last_dramc_ofs * 4));

		if ((val >> 16) == LASTDRAMC_KEY)
			break;
	}

	if (last_dramc_ofs == DBG_INFO_TYPE_MAX) {
		pr_err("[DRAMC] lastdramc with sram mgr not found\n");
		return 0;
	}

	last_dramc_ofs = val & 0xFFFF;
#else
	last_dramc_ofs = LAST_DRAMC_SRAM_SIZE;
#endif

	val = Reg_Readl(DRAMC_SRAM_DEBUG_BASE_ADDR +
		last_dramc_ofs + DRAMC_STORAGE_API_ERR_OFFSET);
	if ((val & STORAGE_READ_API_MASK) == ERR_PL_UPDATED) {
		pr_err("[DRAMC] k time too long: PL updated (0x%08lx)\n", val);
	} else if (val != 0) {
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DUMMY_DUMP, "DRAM Calibration Time",
			"k time too long: api error (0x%08lx)\n", val);
		pr_err("[DRAMC] k time too long: api error (0x%08lx)\n", val);
	} else {
		pr_err("[DRAMC] k time optimized\n");
	}

	return 0;
}

static const struct of_device_id dramc_sram_debug_match[] = {
	{ .compatible = "mediatek,dramc_sram_debug" },
	{}
};

static struct platform_driver dramc_sram_debug_drv = {
	.probe = dram_calib_perf_check_probe,
	.driver = {
		.name = "dramc_sram_debug",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table	= dramc_sram_debug_match,
	}
};

static int __init dram_calib_perf_check(void)
{
	int ret;

	ret = platform_driver_register(&dramc_sram_debug_drv);
	if (ret) {
		pr_err("%s:%d: platform_driver_register failed\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	return 0;
}

/* NOTE: must be called after aed driver initialized
 *(i.e. must be later than arch_initcall)
 */
late_initcall(dram_calib_perf_check);
#endif
