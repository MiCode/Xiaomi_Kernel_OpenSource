/* linux/arch/arm/mach-msm/board-swordfish-mmc.c
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/platform_device.h>

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/mach/mmc.h>

#include <mach/vreg.h>
#include <mach/proc_comm.h>

#include "devices.h"

#define FPGA_BASE		0x70000000
#define FPGA_SDIO_STATUS	0x280

static void __iomem *fpga_base;

#define DEBUG_SWORDFISH_MMC 1

extern int msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat,
			unsigned int stat_irq, unsigned long stat_irq_flags);

static int config_gpio_table(unsigned *table, int len, int enable)
{
	int n;
	int rc = 0;

	for (n = 0; n < len; n++) {
		unsigned dis = !enable;
		unsigned id = table[n];

		if (msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, &dis)) {
			pr_err("%s: id=0x%08x dis=%d\n", __func__, table[n],
			       dis);
			rc = -1;
		}
	}

	return rc;
}

static unsigned sdc1_gpio_table[] = {
	PCOM_GPIO_CFG(51, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(52, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(53, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(54, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(55, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(56, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA),
};

static unsigned sdc2_gpio_table[] = {
	PCOM_GPIO_CFG(62, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA),
	PCOM_GPIO_CFG(63, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(64, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(65, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(66, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(67, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_4MA),
};

static unsigned sdc3_gpio_table[] = {
	PCOM_GPIO_CFG(88, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA),
	PCOM_GPIO_CFG(89, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(90, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(91, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(92, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(93, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
};

static unsigned sdc4_gpio_table[] = {
	PCOM_GPIO_CFG(142, 3, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA),
	PCOM_GPIO_CFG(143, 3, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(144, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(145, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(146, 3, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	PCOM_GPIO_CFG(147, 3, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
};

struct sdc_info {
	unsigned *table;
	unsigned len;
};

static struct sdc_info sdcc_gpio_tables[] = {
	[0] = {
		.table	= sdc1_gpio_table,
		.len	= ARRAY_SIZE(sdc1_gpio_table),
	},
	[1] = {
		.table	= sdc2_gpio_table,
		.len	= ARRAY_SIZE(sdc2_gpio_table),
	},
	[2] = {
		.table	= sdc3_gpio_table,
		.len	= ARRAY_SIZE(sdc3_gpio_table),
	},
	[3] = {
		.table	= sdc4_gpio_table,
		.len	= ARRAY_SIZE(sdc4_gpio_table),
	},
};

static int swordfish_sdcc_setup_gpio(int dev_id, unsigned enable)
{
	struct sdc_info *info;

	if (dev_id < 1 || dev_id > 4)
		return -1;

	info = &sdcc_gpio_tables[dev_id - 1];
	return config_gpio_table(info->table, info->len, enable);
}

struct mmc_vdd_xlat {
	int mask;
	int level;
};

static struct mmc_vdd_xlat mmc_vdd_table[] = {
	{ MMC_VDD_165_195,	1800 },
	{ MMC_VDD_20_21,	2050 },
	{ MMC_VDD_21_22,	2150 },
	{ MMC_VDD_22_23,	2250 },
	{ MMC_VDD_23_24,	2350 },
	{ MMC_VDD_24_25,	2450 },
	{ MMC_VDD_25_26,	2550 },
	{ MMC_VDD_26_27,	2650 },
	{ MMC_VDD_27_28,	2750 },
	{ MMC_VDD_28_29,	2850 },
	{ MMC_VDD_29_30,	2950 },
};

static struct vreg *vreg_sdcc;
static unsigned int vreg_sdcc_enabled;
static unsigned int sdcc_vdd = 0xffffffff;

static uint32_t sdcc_translate_vdd(struct device *dev, unsigned int vdd)
{
	int i;
	int rc = 0;
	struct platform_device *pdev;

	pdev = container_of(dev, struct platform_device, dev);
	BUG_ON(!vreg_sdcc);

	if (vdd == sdcc_vdd)
		return 0;

	sdcc_vdd = vdd;

	/* enable/disable the signals to the slot */
	swordfish_sdcc_setup_gpio(pdev->id, !!vdd);

	/* power down */
	if (vdd == 0) {
#if DEBUG_SWORDFISH_MMC
		pr_info("%s: disable sdcc power\n", __func__);
#endif
		vreg_disable(vreg_sdcc);
		vreg_sdcc_enabled = 0;
		return 0;
	}

	if (!vreg_sdcc_enabled) {
		rc = vreg_enable(vreg_sdcc);
		if (rc)
			pr_err("%s: Error enabling vreg (%d)\n", __func__, rc);
		vreg_sdcc_enabled = 1;
	}

	for (i = 0; i < ARRAY_SIZE(mmc_vdd_table); i++) {
		if (mmc_vdd_table[i].mask != (1 << vdd))
			continue;
#if DEBUG_SWORDFISH_MMC
		pr_info("%s: Setting level to %u\n", __func__,
			mmc_vdd_table[i].level);
#endif
		rc = vreg_set_level(vreg_sdcc, mmc_vdd_table[i].level);
		if (rc)
			pr_err("%s: Error setting vreg level (%d)\n", __func__, 				rc);
		return 0;
	}

	pr_err("%s: Invalid VDD %d specified\n", __func__, vdd);
	return 0;
}

static unsigned int swordfish_sdcc_slot_status (struct device *dev)
{
	struct platform_device *pdev;
	uint32_t sdcc_stat;

	pdev = container_of(dev, struct platform_device, dev);

	sdcc_stat = readl(fpga_base + FPGA_SDIO_STATUS);

	/* bit 0 - sdcc1 crd_det
	 * bit 1 - sdcc1 wr_prt
	 * bit 2 - sdcc2 crd_det
	 * bit 3 - sdcc2 wr_prt
	 * etc...
	 */

	/* crd_det is active low */
	return !(sdcc_stat & (1 << ((pdev->id - 1) << 1)));
}

#define SWORDFISH_MMC_VDD (MMC_VDD_165_195 | MMC_VDD_20_21 | MMC_VDD_21_22 \
			| MMC_VDD_22_23 | MMC_VDD_23_24 | MMC_VDD_24_25 \
			| MMC_VDD_25_26 | MMC_VDD_26_27 | MMC_VDD_27_28 \
			| MMC_VDD_28_29 | MMC_VDD_29_30)

static struct mmc_platform_data swordfish_sdcc_data = {
	.ocr_mask	= SWORDFISH_MMC_VDD/*MMC_VDD_27_28 | MMC_VDD_28_29*/,
	.status		= swordfish_sdcc_slot_status,
	.translate_vdd	= sdcc_translate_vdd,
};

int __init swordfish_init_mmc(void)
{
	vreg_sdcc_enabled = 0;
	vreg_sdcc = vreg_get(NULL, "gp5");
	if (IS_ERR(vreg_sdcc)) {
		pr_err("%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_sdcc));
		return PTR_ERR(vreg_sdcc);
	}

	fpga_base = ioremap(FPGA_BASE, SZ_4K);
	if (!fpga_base) {
		pr_err("%s: Can't ioremap FPGA base address (0x%08x)\n",
		       __func__, FPGA_BASE);
		vreg_put(vreg_sdcc);
		return -EIO;
	}

	msm_add_sdcc(1, &swordfish_sdcc_data, 0, 0);
	msm_add_sdcc(4, &swordfish_sdcc_data, 0, 0);

	return 0;
}

