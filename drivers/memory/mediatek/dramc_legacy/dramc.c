// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <asm/cacheflush.h>
#include <asm/setup.h>
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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <soc/mediatek/dramc_legacy.h>

#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY_V0)
/*extern unsigned int get_dram_type(void);*/
#include "emi_ctrl.h"
#endif

#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)
#define DRAM_R0_DUMMY_READ_RESERVED_KEY "reserve-memory-dram_r0_dummy_read"
#define DRAM_R1_DUMMY_READ_RESERVED_KEY "reserve-memory-dram_r1_dummy_read"
#include <linux/of_reserved_mem.h>
#endif

#ifdef CONFIG_ARM64
#define IOMEM(a) ((void __force __iomem *)((a)))
#endif

void __iomem *SLEEP_BASE_ADDR;
void __iomem *DRAMC_AO_CHA_BASE_ADDR;
void __iomem *DRAMC_NAO_CHA_BASE_ADDR;
void __iomem *DDRPHY_CHA_BASE_ADDR;

unsigned int No_DummyRead;
unsigned int coverted_dram_type;

#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)
#define DRAM_RSV_SIZE 0x1000

static unsigned int dram_rank_num;
phys_addr_t dram_rank0_addr, dram_rank1_addr;


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
RESERVEDMEM_OF_DECLARE(dram_reserve_r0_dummy_read_init, DRAM_R0_DUMMY_READ_RESERVED_KEY,
	dram_dummy_read_reserve_mem_of_init);
RESERVEDMEM_OF_DECLARE(dram_reserve_r1_dummy_read_init, DRAM_R1_DUMMY_READ_RESERVED_KEY,
	dram_dummy_read_reserve_mem_of_init);
#endif

#define MEM_TEST_SIZE 0x2000
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
int do_binning_test(void)
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
		ret = -24;
		goto fail;
	}

	MEM8_BASE = (unsigned char *)ptr;
	MEM16_BASE = (unsigned short *)ptr;
	MEM32_BASE = (unsigned int *)ptr;
	MEM_BASE = (unsigned int *)ptr;
	/* pr_info("Test DRAM start address 0x%lx\n", (unsigned long)ptr); */
	pr_info("Test DRAM start address %p\n", ptr);
	pr_info("Test DRAM SIZE 0x%x\n", MEM_TEST_SIZE);
	size = len >> 2;

	/* === Verify the tied bits (tied high) === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0;

	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0) {
			ret = -1;
			goto fail;
		} else
			MEM32_BASE[i] = 0xffffffff;
	}

	/* === Verify the tied bits (tied low) === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
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
			ret = -13;
			goto fail;
		} else {
			MEM16_BASE[i * 2] = 0xffff;
		}
	}
    /*===  Read Check === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
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
			ret = -15;
			goto fail;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 3 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			ret = -16;
			goto fail;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 4 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			ret = -17;
			goto fail;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 5 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			ret = -18;
			goto fail;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 6 => read 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
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

static ssize_t binning_test_show(struct device_driver *driver, char *buf)
{
	int ret;

	ret = do_binning_test();
	if (ret > 0)
		return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
	else
		return snprintf(buf, PAGE_SIZE, "MEM Test failed %d\n", ret);
}
static DRIVER_ATTR_RO(binning_test);

unsigned int ucDram_Register_Read(unsigned int u4reg_addr)
{
	unsigned int pu4reg_value;

	pu4reg_value = readl(IOMEM(DRAMC_AO_CHA_BASE_ADDR + (u4reg_addr))) |
		readl(IOMEM(DDRPHY_CHA_BASE_ADDR + (u4reg_addr))) |
		readl(IOMEM(DRAMC_NAO_CHA_BASE_ADDR + (u4reg_addr)));

	return pu4reg_value;
}

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
		/* 1 PLL */
		MEMPLL2_FOUT = (((MEMPLL1_FOUT * MEMPLL2_FBSEL) >> 12) * MEMPLL2_FBDIV) >> 12;
	} else {
		/* 2 or 3 PLL */
		MEMPLL2_FOUT = (((MEMPLL1_FOUT * MEMPLL2_M4PDIV * 2) >> 12) * MEMPLL2_FBDIV) >> 12;
	}

	/* workaround */
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

static ssize_t dram_data_rate_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DRAM data rate = %d\n", get_dram_data_rate());
}
static DRIVER_ATTR_RO(dram_data_rate);

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

int get_ddr_type(void)
{
	unsigned int type = get_dram_type();

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
	return 1;
}
EXPORT_SYMBOL(get_emi_ch_num);

int dram_can_support_fh(void)
{
	if (No_DummyRead)
		return 0;

	if (get_ddr_type() == TYPE_LPDDR2)
		return 0;

	return 1;
}
EXPORT_SYMBOL(dram_can_support_fh);

unsigned int get_shuffle_status(void)
{
	return 0;
}

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

	pr_info("[DRAMC] get DRAMC_AO_CHA_BASE_ADDR @ %p\n", DRAMC_AO_CHA_BASE_ADDR);
	pr_info("[DRAMC] get DDRPHY_CHA_BASE_ADDR @ %p\n", DDRPHY_CHA_BASE_ADDR);
	pr_info("[DRAMC] get DRAMC_NAO_CHA_BASE_ADDR @ %p\n", DRAMC_NAO_CHA_BASE_ADDR);

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (node) {
		SLEEP_BASE_ADDR = of_iomap(node, 0);
		pr_info("[DRAMC] get SLEEP_BASE_ADDR @ %p\n",
		SLEEP_BASE_ADDR);
	} else {
		pr_info("[DRAMC] can't find SLEEP_BASE_ADDR compatible node\n");
		return -1;
	}

	coverted_dram_type = get_ddr_type();
	pr_info("[DRAMC] dram type =%d\n", coverted_dram_type);

	if (coverted_dram_type == -1) {
		pr_info("[DRAMC] dram type error !!\n");
		return -1;
	}

	pr_info("[DRAMC] Dram Data Rate = %d\n", get_dram_data_rate());
	pr_info("[DRAMC] shuffle_status = %d\n", get_shuffle_status());

	ret = driver_create_file(pdev->dev.driver, &driver_attr_binning_test);
	if (ret) {
		pr_info("[DRAMC] fail to create the binning_test sysfs files\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver, &driver_attr_dram_data_rate);
	if (ret) {
		pr_info("[DRAMC] fail to create the dram_data_rate sysfs files\n");
		return ret;
	}

	if (dram_can_support_fh())
		pr_info("[DRAMC] dram can support DFS\n");
	else
		pr_info("[DRAMC] dram can not support DFS\n");

	return 0;
}

static int dram_remove(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id dram_of_ids[] = {
	{.compatible = "mediatek,dramc",},
	{}
};

static struct platform_driver dram_drv = {
	.probe = dram_probe,
	.remove = dram_remove,
	.driver = {
		.name = "dramc_drv",
		.owner = THIS_MODULE,
		.of_match_table = dram_of_ids,
		},
};

static int __init dram_drv_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&dram_drv);
	if (ret) {
		pr_info("[DRAMC] init fail, ret 0x%x\n", ret);
		return ret;
	}
	return ret;
}

static void __exit dram_drv_exit(void)
{
	platform_driver_unregister(&dram_drv);
}

module_init(dram_drv_init);
module_exit(dram_drv_exit);

MODULE_DESCRIPTION("MediaTek DRAMC Legacy Driver");
MODULE_LICENSE("GPL v2");
