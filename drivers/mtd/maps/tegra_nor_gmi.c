/*
 * Copyright (C) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * Author:
 * Bharath H S <bhs@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * drivers/mtd/maps/tegra_nor_gmi.c
 *
 * MTD mapping driver for the internal SNOR controller in Tegra SoCs
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/platform_data/tegra_nor.h>
#include <linux/mtd/concat.h>
#include <linux/gpio.h>
#include <linux/tegra_snor.h>

#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
#include <linux/tegra_gmi_access.h>
#endif

struct map_info_list {
	struct map_info *map;
	int n_maps;
	unsigned long long totalflashsize;
};

struct tegra_nor_info {
	struct tegra_nor_platform_data *plat;	/* Platform data */
	struct device *dev;			/* Platform device */
	struct clk *clk;			/* Clock */
	struct mtd_partition *parts;		/* MtdParts passed by
						   bootloader */
	struct map_info *map;
	unsigned int n_maps;
	struct mtd_info **mtd;
	struct mtd_info *concat_mtd;
	struct gpio_addr_info *adinfo;
	struct completion dma_complete;		/* comletion for DMA */
	void __iomem *base;			/* SNOR register address */
	void *dma_virt_buffer;
	dma_addr_t dma_phys_buffer;
	u32 init_config;
	u32 timing0_default, timing1_default;
	u32 timing0_read, timing1_read;
	u32 gmiLockHandle;
	int (*request_gmi_access)(u32 gmiLockHandle);
	void (*release_gmi_access)(void);
};

/* Master structure of all tegra NOR private data*/
struct tegra_nor_private {
	struct tegra_nor_info *info;
	struct cs_info *cs;
};

#define DRV_NAME "tegra-nor"

#define gmi_lock(info)	\
do {	\
	if ((info)->request_gmi_access)	\
		(info)->request_gmi_access((info)->gmiLockHandle);	\
} while (0)

#define gmi_unlock(info)	\
do {	\
	if ((info)->release_gmi_access)	\
		(info)->release_gmi_access();	\
} while (0)


#if defined(CONFIG_TEGRA_EFS) || defined(CONFIG_TEGRA_PFLASH)
static struct map_info_list nor_gmi_map_list;
#endif

static inline unsigned long tegra_snor_readl(struct tegra_nor_info *tnor,
					     unsigned long reg)
{
	return readl(tnor->base + reg);
}

static inline void tegra_snor_writel(struct tegra_nor_info *tnor,
				     unsigned long val, unsigned long reg)
{
	writel(val, tnor->base + reg);
}


static const char * const part_probes[] = { "cmdlinepart", NULL };

static int wait_for_dma_completion(struct tegra_nor_info *info)
{
	unsigned long dma_timeout;
	int ret;

	dma_timeout = msecs_to_jiffies(TEGRA_SNOR_DMA_TIMEOUT_MS);
	ret = wait_for_completion_timeout(&info->dma_complete, dma_timeout);
	return ret ? 0 : -ETIMEDOUT;
}

static void flash_bank_cs(struct map_info *map)
{
	struct tegra_nor_private *priv =
				(struct tegra_nor_private *)map->map_priv_1;
	struct cs_info *csinfo = priv->cs;
	struct gpio_state *state = &csinfo->gpio_cs;
	int i;
	u32 snor_config = 0;
	struct tegra_nor_info *c = priv->info;

	snor_config = tegra_snor_readl(c, TEGRA_SNOR_CONFIG_REG);
	snor_config &= SNOR_CONFIG_MASK;
	snor_config |= TEGRA_SNOR_CONFIG_SNOR_CS(csinfo->cs);
	tegra_snor_writel(c, snor_config, TEGRA_SNOR_CONFIG_REG);

	c->init_config = snor_config;

	snor_config = tegra_snor_readl(c, TEGRA_SNOR_CONFIG_REG);
	for (i = 0; i < csinfo->num_cs_gpio; i++)
		gpio_set_value(state->gpio_num, state->value);
}

static void flash_bank_addr(struct map_info *map, unsigned int offset)
{
	struct tegra_nor_private *priv =
				(struct tegra_nor_private *)map->map_priv_1;
	struct gpio_addr_info *adinfo = priv->info->adinfo;
	struct gpio_addr *addr = adinfo->addr;
	struct gpio_state state;
	int i;

	for (i = 0; i < adinfo->num_gpios; i++) {
		state.gpio_num = addr[i].gpio_num;
		state.value = (BIT(addr[i].line_num) & offset) ? HIGH : LOW;
		gpio_set_value(state.gpio_num, state.value);
	}
}

static void tegra_flash_dma(struct map_info *map,
			    void *to, unsigned long from, ssize_t len)
{
	u32 snor_config, dma_config = 0;
	int dma_transfer_count = 0, word32_count = 0;
	u32 nor_address, current_transfer = 0;
	u32 copy_to = (u32)to;
	struct tegra_nor_private *priv =
		(struct tegra_nor_private *)map->map_priv_1;
	struct tegra_nor_info *c = priv->info;
	struct tegra_nor_chip_parms *chip_parm = &c->plat->chip_parms;
	unsigned int bytes_remaining = len;

	snor_config = c->init_config;

	tegra_snor_writel(c, c->timing0_read, TEGRA_SNOR_TIMING0_REG);
	tegra_snor_writel(c, c->timing1_read, TEGRA_SNOR_TIMING1_REG);

	if (len > 32) {
		word32_count = len >> 2;
		bytes_remaining = len & 0x00000003;
		/*
		 * The parameters can be setup in any order since we write to
		 * controller register only after all parameters are set.
		 */
		/* SNOR CONFIGURATION SETUP */

		switch (chip_parm->ReadMode) {
		case NorReadMode_Async:
			snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(0);
			break;

		case NorReadMode_Page:
			switch (chip_parm->PageLength) {
			case NorPageLength_Unsupported:
				snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(0);
				break;

			case NorPageLength_4Word:
				snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(1);
				snor_config |= TEGRA_SNOR_CONFIG_PAGE_SZ(1);
				break;

			case NorPageLength_8Word:
				snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(1);
				snor_config |= TEGRA_SNOR_CONFIG_PAGE_SZ(2);
				break;

			case NorPageLength_16Word:
				snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(1);
				snor_config |= TEGRA_SNOR_CONFIG_PAGE_SZ(3);
				break;
			}
			break;

		case NorReadMode_Burst:
			snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(2);
			switch (chip_parm->BurstLength) {
			case NorBurstLength_CntBurst:
				snor_config |= TEGRA_SNOR_CONFIG_BURST_LEN(0);
				break;
			case NorBurstLength_8Word:
				snor_config |= TEGRA_SNOR_CONFIG_BURST_LEN(1);
				break;

			case NorBurstLength_16Word:
				snor_config |= TEGRA_SNOR_CONFIG_BURST_LEN(2);
				break;

			case NorBurstLength_32Word:
				snor_config |= TEGRA_SNOR_CONFIG_BURST_LEN(3);
				break;
			}
			break;
		}
		snor_config |= TEGRA_SNOR_CONFIG_MST_ENB;
		/* SNOR DMA CONFIGURATION SETUP */
		/* NOR -> AHB */
		dma_config &= ~TEGRA_SNOR_DMA_CFG_DIR;
		/* One word burst */
		/* Fix this to have max burst as in commented code*/
		/*dma_config |= TEGRA_SNOR_DMA_CFG_BRST_SZ(6);*/
		dma_config |= TEGRA_SNOR_DMA_CFG_BRST_SZ(4);

		for (nor_address = (unsigned int)(map->phys + from);
		     word32_count > 0;
		     word32_count -= current_transfer,
		     dma_transfer_count += current_transfer,
		     nor_address += (current_transfer * 4),
		     copy_to += (current_transfer * 4)) {

			current_transfer =
			    (word32_count > TEGRA_SNOR_DMA_LIMIT_WORDS)
			    ? (TEGRA_SNOR_DMA_LIMIT_WORDS) : word32_count;
			/* Start NOR operation */
			snor_config |= TEGRA_SNOR_CONFIG_GO;
			dma_config |= TEGRA_SNOR_DMA_CFG_GO;
			/* Enable interrupt before every transaction since the
			 * interrupt handler disables it */
			dma_config |= TEGRA_SNOR_DMA_CFG_INT_ENB;
			/* Num of AHB (32-bit) words to transferred minus 1 */
			dma_config |=
			    TEGRA_SNOR_DMA_CFG_WRD_CNT(current_transfer - 1);
			tegra_snor_writel(c, c->dma_phys_buffer,
					  TEGRA_SNOR_AHB_ADDR_PTR_REG);
			tegra_snor_writel(c, nor_address,
					  TEGRA_SNOR_NOR_ADDR_PTR_REG);
			tegra_snor_writel(c, snor_config,
					  TEGRA_SNOR_CONFIG_REG);
			tegra_snor_writel(c, dma_config,
					  TEGRA_SNOR_DMA_CFG_REG);
			if (wait_for_dma_completion(c)) {
				dev_err(c->dev, "timout waiting for DMA\n");
				/* Transfer the remaining words by memcpy */
				bytes_remaining += (word32_count << 2);
				break;
			}
			/* use dma map single instead of coheret buffer */
			dma_sync_single_for_cpu(c->dev, c->dma_phys_buffer,
				(current_transfer << 2), DMA_FROM_DEVICE);
			memcpy((char *)(copy_to), (char *)(c->dma_virt_buffer),
				(current_transfer << 2));

		}
	}
	/* Put the controller back into slave mode. */
	snor_config = tegra_snor_readl(c, TEGRA_SNOR_CONFIG_REG);
	snor_config &= ~TEGRA_SNOR_CONFIG_MST_ENB;
	snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(0);
	tegra_snor_writel(c, snor_config, TEGRA_SNOR_CONFIG_REG);

	memcpy_fromio(((char *)to + (dma_transfer_count << 2)),
		      ((char *)(map->virt + from) + (dma_transfer_count << 2)),
		      bytes_remaining);

	tegra_snor_writel(c, c->timing0_default, TEGRA_SNOR_TIMING0_REG);
	tegra_snor_writel(c, c->timing1_default, TEGRA_SNOR_TIMING1_REG);
}

void tegra_nor_copy_from(struct map_info *map,
			    void *to, unsigned long from, ssize_t len)
{

	struct tegra_nor_private *priv =
			(struct tegra_nor_private *)map->map_priv_1;
	struct tegra_nor_info *c = priv->info;
	unsigned long bank_boundary;

	gmi_lock(c);
	/* All feature for go thru loop*/
	flash_bank_cs(map);

	/* Tegra SNOR controller window is 128MB this
	check is required to take care of case which cross bank boundary*/
	bank_boundary = (from & ~SNOR_WINDOW_SIZE) + SNOR_WINDOW_SIZE + 1;

	if ((from + len) > bank_boundary) {
		unsigned long transfer_chunk = bank_boundary - from;

		flash_bank_addr(map, from);
		tegra_flash_dma(map, to, (from & SNOR_WINDOW_SIZE),
						bank_boundary - from);

		flash_bank_addr(map, bank_boundary);
		tegra_flash_dma(map, to + (bank_boundary - from), 0,
							len - transfer_chunk);
	} else {
		flash_bank_addr(map, from);
		tegra_flash_dma(map, to, (from & SNOR_WINDOW_SIZE), len);
	}

	gmi_unlock(c);

}

map_word tegra_nor_read(struct map_info *map,
				unsigned long ofs)
{
	map_word ret;
	struct tegra_nor_private *priv =
			(struct tegra_nor_private *)map->map_priv_1;
	struct tegra_nor_info *c = priv->info;

	gmi_lock(c);

	tegra_snor_writel(c, c->init_config, TEGRA_SNOR_CONFIG_REG);
	tegra_snor_writel(c, c->timing0_read, TEGRA_SNOR_TIMING0_REG);
	tegra_snor_writel(c, c->timing1_read, TEGRA_SNOR_TIMING1_REG);

	flash_bank_cs(map);
	flash_bank_addr(map, ofs);
	ret = inline_map_read(map, ofs & SNOR_WINDOW_SIZE);

	gmi_unlock(c);
	return ret;
}

void tegra_nor_write(struct map_info *map,
				map_word datum, unsigned long ofs)
{

	struct tegra_nor_private *priv =
				(struct tegra_nor_private *)map->map_priv_1;
	struct tegra_nor_info *c = priv->info;

	gmi_lock(c);
	tegra_snor_writel(c, c->init_config, TEGRA_SNOR_CONFIG_REG);
	tegra_snor_writel(c, c->timing0_read, TEGRA_SNOR_TIMING0_REG);
	tegra_snor_writel(c, c->timing1_read, TEGRA_SNOR_TIMING1_REG);

	flash_bank_cs(map);
	flash_bank_addr(map, ofs);
	inline_map_write(map, datum, ofs & SNOR_WINDOW_SIZE);

	gmi_unlock(c);
}

static irqreturn_t tegra_nor_isr(int flag, void *dev_id)
{
	struct tegra_nor_info *info = (struct tegra_nor_info *)dev_id;

	u32 dma_config = tegra_snor_readl(info, TEGRA_SNOR_DMA_CFG_REG);

	if (dma_config & TEGRA_SNOR_DMA_CFG_INT_STA) {
		/* Disable interrupts. WAR for BUG:821560 */
		dma_config &= ~TEGRA_SNOR_DMA_CFG_INT_ENB;
		tegra_snor_writel(info, dma_config, TEGRA_SNOR_DMA_CFG_REG);
		complete(&info->dma_complete);
	} else {
		pr_err("%s: Spurious interrupt\n", __func__);
	}
	return IRQ_HANDLED;
}

static int tegra_snor_controller_init(struct tegra_nor_info *info)
{
	struct tegra_nor_chip_parms *chip_parm = &info->plat->chip_parms;
	u32 width = info->plat->flash.width;
	u32 config = 0;

	config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(0);
	config |= TEGRA_SNOR_CONFIG_SNOR_CS(0);
	config &= ~TEGRA_SNOR_CONFIG_DEVICE_TYPE;	/* Select NOR */
	config |= TEGRA_SNOR_CONFIG_WP;	/* Enable writes */
	switch (width) {
	case 2:
		config &= ~TEGRA_SNOR_CONFIG_WORDWIDE;	/* 16 bit */
		break;
	case 4:
		config |= TEGRA_SNOR_CONFIG_WORDWIDE;	/* 32 bit */
		break;
	default:
		return -EINVAL;
	}
	switch (chip_parm->MuxMode) {
	case NorMuxMode_ADNonMux:
		config &= ~TEGRA_SNOR_CONFIG_MUX_MODE;
		break;
	case NorMuxMode_ADMux:
		config |= TEGRA_SNOR_CONFIG_MUX_MODE;
		break;
	default:
		return -EINVAL;
	}
	switch (chip_parm->ReadyActive) {
	case NorReadyActive_WithData:
		config &= ~TEGRA_SNOR_CONFIG_RDY_ACTIVE;
		break;
	case NorReadyActive_BeforeData:
		config |= TEGRA_SNOR_CONFIG_RDY_ACTIVE;
		break;
	default:
		return -EINVAL;
	}
	tegra_snor_writel(info, config, TEGRA_SNOR_CONFIG_REG);
	info->init_config = config;

	info->timing0_default = chip_parm->timing_default.timing0;
	info->timing0_read = chip_parm->timing_read.timing0;
	info->timing1_default = chip_parm->timing_default.timing1;
	info->timing1_read = chip_parm->timing_read.timing1;

	tegra_snor_writel(info, info->timing1_default, TEGRA_SNOR_TIMING1_REG);
	tegra_snor_writel(info, info->timing0_default, TEGRA_SNOR_TIMING0_REG);
	return 0;
}

#if defined(CONFIG_TEGRA_EFS) || defined(CONFIG_TEGRA_PFLASH)
struct map_info *get_map_info(unsigned int bank_index)
{
	struct map_info *map = &nor_gmi_map_list.map[bank_index];
	return map;
}
int get_maps_no(void)
{
	int no_of_maps = nor_gmi_map_list.n_maps;
	return no_of_maps;
}
unsigned long long getflashsize(void)
{
	int flash_size = nor_gmi_map_list.totalflashsize;
	return flash_size;
}
#endif

static int flash_probe(struct tegra_nor_info *info)
{
	struct mtd_info **mtd;
	struct map_info *map_list = info->map;
	struct map_info *map;
	int i;
	struct device *dev = info->dev;
	int present_banks = 0;
	unsigned long long size = 0;


	mtd = devm_kzalloc(dev, sizeof(struct mtd_info *) *
						info->n_maps, GFP_KERNEL);

	if (!mtd) {
		dev_err(dev, "cannot allocate memory for mtd_info\n");
		return -ENOMEM;
	}

	for (i = 0; i < info->n_maps; i++) {
		map = &map_list[i];
		mtd[i] = do_map_probe(info->plat->flash.map_name, map);
		if (mtd[i] == NULL)
			dev_err(dev, "cannot probe flash\n");
		else {
			present_banks++;
			size += map->size;
		}

	}

	info->mtd = mtd;

	if (present_banks == 0) {
		return -EIO;
	} else{
		if (present_banks < info->n_maps)
			info->concat_mtd = mtd_concat_create(info->mtd,
						present_banks, DRV_NAME);
		else
			info->concat_mtd = mtd_concat_create(info->mtd,
						info->n_maps, DRV_NAME);
	}
	if (!info->concat_mtd) {
		dev_err(dev, "cannot concatenate flash\n");
		return -ENODEV;
	}

	info->concat_mtd->owner = THIS_MODULE;

#if defined(CONFIG_TEGRA_EFS) || defined(CONFIG_TEGRA_PFLASH)
	nor_gmi_map_list.totalflashsize = size;
#endif

	return 0;
}

static int flash_maps_init(struct tegra_nor_info *info, struct resource *res)
{
	struct map_info *map_list;
	struct map_info *map;
	struct flash_info *flinfo = &info->plat->info;
	int num_chips = flinfo->num_chips;
	int i = 0;
	struct tegra_nor_private *priv;
	struct device *dev = info->dev;

	priv = devm_kzalloc(dev, sizeof(struct tegra_nor_private) * num_chips,
							GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "cannot allocate memory for" \
						"private variable\n");
		return -ENOMEM;
	}

	map_list = devm_kzalloc(dev, sizeof(struct map_info) * num_chips,
							 GFP_KERNEL);
	if (!map_list) {
		dev_err(dev, "cannot allocate memory for" \
							"map_info\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_chips; i++) {
		struct cs_info *csinfo = NULL;
		struct gpio_state *state = NULL;
		struct gpio_addr_info *adinfo = NULL;
		struct gpio_addr *addr = NULL;
		int j, k;

		map = &map_list[i];

		/* Setup private structure*/
		priv[i].info = info;
		priv[i].cs = flinfo->cs + i;
		map->map_priv_1 = (unsigned long)&priv[i];

		csinfo =  priv[i].cs;
		state  =  &csinfo->gpio_cs;
		adinfo = priv[i].info->adinfo;
		addr = adinfo->addr;
		/* Request Needed GPIO's */

		for (j = 0; j < csinfo->num_cs_gpio; j++) {
			/* For now ignore the request failures */
			int err = 0;
			err = gpio_request(state[j].gpio_num, state[j].label);
			gpio_direction_output(state[j].gpio_num, 0);
		}
		for (k = 0; k < adinfo->num_gpios; k++) {
			int err_add = 0;
			err_add = gpio_request(addr[k].gpio_num, NULL);
			gpio_direction_output(addr[k].gpio_num, 0);
		}
		/* Others*/
		map->name = dev_name(info->dev);
		map->size = csinfo->size;
		map->bankwidth = info->plat->flash.width;
		map->virt = priv->cs->virt;
		map->phys = priv->cs->phys;

		simple_map_init(map);
		map->read = tegra_nor_read;
		map->write = tegra_nor_write;
		map->copy_from = tegra_nor_copy_from;
	}

	info->n_maps = num_chips;
	info->map = map_list;
	return 0;
}

static int tegra_nor_probe(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_nor_platform_data *plat = pdev->dev.platform_data;
	struct tegra_nor_info *info = NULL;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq;

	if (!plat) {
		pr_err("%s: no platform device info\n", __func__);
		return -EINVAL;
	}

	info = devm_kzalloc(dev, sizeof(struct tegra_nor_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* Get NOR controller & map the same */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no mem resource?\n");
		return -ENODEV;
	}

	info->base = devm_request_and_ioremap(dev, res);
	if (!info->base) {
		dev_err(dev, "Couldn't ioremap regs\n");
		return -ENOMEM;
	}

	info->plat = plat;
	info->dev = dev;

#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
	info->gmiLockHandle = register_gmi_device("tegra-nor", 0);
	info->request_gmi_access = request_gmi_access;
	info->release_gmi_access = release_gmi_access;
#else
	info->gmiLockHandle = 0;
	info->request_gmi_access = NULL;
	info->release_gmi_access = NULL;
#endif
	/* Get address line gpios */
	info->adinfo = &plat->addr;

	/* Initialize mapping */
	if (flash_maps_init(info, res)) {
		dev_err(dev, "can't map NOR flash\n");
		return -ENOMEM;
	}

	/* Clock setting */
	info->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(info->clk))
		return PTR_ERR(info->clk);

	err = clk_prepare_enable(info->clk);
	if (err != 0)
		return err;

	/* Intialise the SNOR controller before probe */
	err = tegra_snor_controller_init(info);
	if (err) {
		dev_err(dev, "Error initializing controller\n");
		goto out_clk_disable;
	}

	init_completion(&info->dma_complete);

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(dev, "no irq resource?\n");
		err = -ENODEV;
		goto out_clk_disable;
	}

	/* Register SNOR DMA completion interrupt */
	err = request_irq(irq, tegra_nor_isr, IRQF_DISABLED,
			dev_name(dev), info);
	if (err) {
		dev_err(dev, "Failed to request irq %i\n", irq);
		goto out_clk_disable;
	}
	info->dma_virt_buffer = dma_alloc_coherent(dev,
							TEGRA_SNOR_DMA_LIMIT,
							&info->dma_phys_buffer,
							GFP_KERNEL);
	if (!info->dma_virt_buffer) {
		dev_err(&pdev->dev, "Could not allocate buffer for DMA");
		err = -ENOMEM;
		goto out_clk_disable;
	}

	/* Probe the flash */
	if (flash_probe(info)) {
		dev_err(dev, "can't probe full flash\n");
		err = -EIO;
		goto out_dma_free_coherent;
	}

	info->parts = NULL;
	plat->flash.parts = NULL;

	platform_set_drvdata(pdev, info);
	/* Parse partitions and register mtd */
	err = mtd_device_parse_register(info->concat_mtd,
			(const char **)part_probes, NULL, info->parts, 0);

	if (err)
		goto out_dma_free_coherent;

#if defined(CONFIG_TEGRA_EFS) || defined(CONFIG_TEGRA_PFLASH)
	nor_gmi_map_list.map = info->map;
	nor_gmi_map_list.n_maps = info->n_maps;
#endif
	return 0;

out_dma_free_coherent:
	dma_free_coherent(dev, TEGRA_SNOR_DMA_LIMIT,
				info->dma_virt_buffer, info->dma_phys_buffer);
out_clk_disable:
	clk_disable_unprepare(info->clk);

	pr_err("Tegra NOR probe failed\n");
	return err;
}

static int tegra_nor_remove(struct platform_device *pdev)
{
	struct tegra_nor_info *info = platform_get_drvdata(pdev);

	mtd_device_unregister(info->concat_mtd);
	kfree(info->parts);
	dma_free_coherent(&pdev->dev, TEGRA_SNOR_DMA_LIMIT,
				info->dma_virt_buffer, info->dma_phys_buffer);
	map_destroy(info->concat_mtd);
	clk_disable_unprepare(info->clk);

	return 0;
}

static struct platform_driver __refdata tegra_nor_driver = {
	.probe = tegra_nor_probe,
	.remove = tegra_nor_remove,
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   },
};


module_platform_driver(tegra_nor_driver);

MODULE_AUTHOR("Bharath H S <bhs@nvidia.com>");
MODULE_DESCRIPTION("NOR Flash mapping driver for NVIDIA Tegra based boards");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
