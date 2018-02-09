/* Qti (or) Qualcomm Technologies Inc CE device driver.
 *
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DRIVERS_CRYPTO_PARSE_H_
#define _DRIVERS_CRYPTO_PARSE_H_

#include <linux/of.h>
#include <linux/of_platform.h>

struct context_bank_info {
	struct list_head list;
	const char *name;
	u32 buffer_type;
	u32 start_addr;
	u32 size;
	bool is_secure;
	struct device *dev;
	struct dma_iommu_mapping *mapping;
};

int qcedev_parse_context_bank(struct platform_device *pdev);

#endif

