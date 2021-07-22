// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>

#include "mdla_cfg_data.h"

#define DRAM_CFG0_SIZE 0x400
#define DRAM_CFG1_SIZE 0xFF00

enum LOAD_DATA_METHOD {
	LOAD_NONE,
	LOAD_ELF_FILE,
	LOAD_HDR_FILE,
	LOAD_BIN_FILE,
};

static int load_data_method;

struct mdla_data {
	void *buf;
	dma_addr_t da;
};
static struct mdla_data cfg0;
static struct mdla_data cfg1;

static int mdla_data_alloc_mem(struct device *dev)
{
	cfg0.buf = dma_alloc_coherent(dev, DRAM_CFG0_SIZE, &cfg0.da, GFP_KERNEL);
	if (cfg0.buf == NULL || cfg0.da == 0) {
		dev_info(dev, "%s() dma_alloc_coherent cfg_init_data fail\n\n", __func__);
		return -1;
	}

	cfg1.buf = dma_alloc_coherent(dev, DRAM_CFG1_SIZE, &cfg1.da, GFP_KERNEL);
	if (cfg1.buf == NULL || cfg1.da == 0) {
		dev_info(dev, "%s() dma_alloc_coherent cfg_main_data fail\n\n", __func__);
		dma_free_coherent(dev, DRAM_CFG0_SIZE, cfg0.buf, cfg0.da);
		return -1;
	}

	memset(cfg0.buf, 0, DRAM_CFG0_SIZE);
	memset(cfg1.buf, 0, DRAM_CFG1_SIZE);

	/* AISIM: It's not necessary to get iova */

	return 0;
}

static void mdla_data_free_mem(struct device *dev)
{

	if (cfg0.buf && cfg0.da)
		dma_free_coherent(dev, DRAM_CFG0_SIZE, cfg0.buf, cfg0.da);
	if (cfg1.buf && cfg1.da)
		dma_free_coherent(dev, DRAM_CFG1_SIZE, cfg1.buf, cfg1.da);

	cfg0.da = 0;
	cfg1.da = 0;
}


static int mdla_plat_load_elf(struct device *dev, unsigned int *initdata, unsigned int *maindata)
{
	return 0;
}

static int mdla_plat_load_img(struct device *dev, unsigned int *initdata, unsigned int *maindata)
{
	int ret = 0;

	if (mdla_data_alloc_mem(dev) < 0)
		return -1;

	dev_info(dev, "%s(): done\n", __func__);
	return ret;
}

static int mdla_plat_load_hdr(struct device *dev, unsigned int *cfg0_data, unsigned int *cfg1_data)
{
	u32 i;
	int ret = 0;

	if (mdla_data_alloc_mem(dev) < 0)
		return -1;

	/* cfg0 : 1K */
	memcpy(cfg0.buf, cfg_init, sizeof(cfg_init));

	for (i = 0; cfg_main_section[i].data != NULL; i++)
		memcpy(cfg1.buf + cfg_main_section[i].ofs,
				cfg_main_section[i].data, cfg_main_section[i].size);

	*cfg0_data = (unsigned int)cfg0.da;
	*cfg1_data = (unsigned int)cfg1.da;
	dev_info(dev, "%s(): done\n", __func__);

	return ret;
}


int mdla_plat_load_data(struct device *dev, unsigned int *cfg0_data, unsigned int *cfg1_data)
{
	int ret;
	const char *method = NULL;

	ret = of_property_read_string(dev->of_node, "boot-method", &method);

	if (ret < 0)
		return -1;

	if (!strcmp(method, "bin"))
		load_data_method = LOAD_BIN_FILE;
	else if (!strcmp(method, "elf"))
		load_data_method = LOAD_ELF_FILE;
	else if (!strcmp(method, "array"))
		load_data_method = LOAD_HDR_FILE;

	switch (load_data_method) {
	case LOAD_BIN_FILE:
		mdla_plat_load_img(dev, cfg0_data, cfg1_data);
		break;
	case LOAD_ELF_FILE:
		ret = mdla_plat_load_elf(dev, cfg0_data, cfg1_data);
		break;
	case LOAD_HDR_FILE:
		mdla_plat_load_hdr(dev, cfg0_data, cfg1_data);
		break;
	default:
		break;
	}


	return ret;
}

void mdla_plat_unload_data(struct device *dev)
{
	switch (load_data_method) {
	case LOAD_BIN_FILE:
	case LOAD_ELF_FILE:
	case LOAD_HDR_FILE:
		mdla_data_free_mem(dev);
		break;
	default:
		break;
	}
}

