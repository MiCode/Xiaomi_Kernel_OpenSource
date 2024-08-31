// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#include "ispv4_rproc.h"
#include <ispv4_regops.h>

#define CPU_PMU_DELAY 1000

static int ispv4_rproc_start(struct rproc *rproc)
{
	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	return 0;
}

static int ispv4_mcu_rproc_attach(struct rproc *rproc)
{
	struct xm_ispv4_rproc *rp = rproc->priv;
	int ret = 0;

	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	ispv4_load_rsc_table(rproc, rp->fw);

#if IS_ENABLED(CONFIG_MIISP_CHIP)
	/* Open cpu pmu */
	ret = enable_cpu_pmu();
	if (ret != 0) {
		dev_err(&rproc->dev, "ispv4: enable_cpu_pmu failed\n");
		return ret;
	}
#endif

	// TAG for FW know spi.
	ret |= ispv4_regops_write(0x0021be0c, 2);
	// Reset CPU
	ret |= ispv4_regops_write(0xD40040C, 0);
	// Config busmonitor
	ret |= ispv4_regops_write(0xD470018, 500);
	// Release CPU
	ret |= ispv4_regops_write(0xD40040C, 0x11F);

	if (ret != 0)
		dev_err(&rproc->dev, "attach cpu error.");

	return ret;
}

static int ispv4_rproc_stop(struct rproc *rproc)
{
	ispv4_regops_write(0xD40040C, 0x0);
	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	return 0;
}

static void ispv4_rproc_kick(struct rproc *rproc, int vqid)
{
	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	return;
}

static void *ispv4_mcu_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len,
				      bool *is_iomem)
{
	if (is_iomem != NULL)
		*is_iomem = false;
	return (void *)da;
}

static struct rproc_ops ispv4_core_ops = {
	.start = ispv4_rproc_start,
	.stop = ispv4_rproc_stop,
	.kick = ispv4_rproc_kick,
	.da_to_va = ispv4_mcu_rproc_da_to_va,
	.attach = ispv4_mcu_rproc_attach,
	.load = rproc_elf_load_segments,
	// .parse_fw = rproc_elf_load_rsc_table,
	// .find_loaded_rsc_table = rproc_elf_find_loaded_rsc_table,
	.sanity_check = rproc_elf_sanity_check,
	.get_boot_addr = rproc_elf_get_boot_addr,
};

static int ispv4_rproc_spi_init(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s\n", __FUNCTION__);
	ispv4_regops_write(0xD40103C, 1);
	ispv4_regops_write(0xD401040, 1);
	ispv4_regops_write(0xD401044, 1);
	ispv4_regops_write(0xD401048, 1);
	return 0;
}

static int ispv4_rproc_spi_boot(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s\n", __FUNCTION__);
	return 0;
}

static void ispv4_rproc_spi_deboot(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s\n", __FUNCTION__);
	return;
}

static void ispv4_rproc_spi_deinit(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s\n", __FUNCTION__);
}

static void ispv4_rproc_spi_shutdown(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s", __FUNCTION__);
	return;
}

static void ispv4_rproc_spi_remove(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s", __FUNCTION__);
	return;
}

struct xm_ispv4_ops ispv4_spi_ops = {
	.init = ispv4_rproc_spi_init,
	.deinit = ispv4_rproc_spi_deinit,
	.boot = ispv4_rproc_spi_boot,
	.deboot = ispv4_rproc_spi_deboot,
	.remove = ispv4_rproc_spi_remove,
	.shutdown = ispv4_rproc_spi_shutdown,
	.rproc_ops = &ispv4_core_ops,
};

MODULE_LICENSE("GPL v2");
