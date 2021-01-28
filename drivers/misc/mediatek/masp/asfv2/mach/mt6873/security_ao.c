// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 MediaTek Inc.
 */

#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "security_ao.h"
#include "sec_boot_lib.h"
#include "mt-plat/sync_write.h"

void __iomem *security_ao_base;
static const struct of_device_id security_ao_ids[] = {
	{.compatible = "mediatek,security_ao"},
	{}
};

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

int masp_hal_set_dm_verity_error(void)
{
	int ret = 0;
	struct device_node *np_secao = NULL;
	unsigned int rst_con = 0;
	unsigned int reg_val = 0;
	const char *compatible = security_ao_ids[0].compatible;

	/* get security ao base address */
	if (!security_ao_base) {
		np_secao = of_find_compatible_node(NULL,
						   NULL,
						   compatible);
		if (!np_secao) {
			pr_notice("[SEC AO] security ao node not found\n");
			return -ENXIO;
		}

		security_ao_base = (void __iomem *)of_iomap(np_secao, 0);
		if (!security_ao_base) {
			pr_notice("[SEC AO] security ao register remapping failed\n");
			return -ENXIO;
		}
	}

	/* configure to make misc register live after system reset */
	mt_reg_sync_writel(MISC_LOCK_KEY_MAGIC, (void *)MISC_LOCK_KEY);
	rst_con = __raw_readl((const void *)RST_CON);
	rst_con |= RST_CON_BIT(BOOT_MISC2_IDX);
	mt_reg_sync_writel(rst_con, (void *)RST_CON);
	mt_reg_sync_writel(0, (void *)MISC_LOCK_KEY);

	/* set dm-verity error flag to misc2 */
	reg_val = __raw_readl((const void *)BOOT_MISC2);
	reg_val |= BOOT_MISC2_VERITY_ERR;
	mt_reg_sync_writel(reg_val, (void *)BOOT_MISC2);

	return ret;
}

