/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef APU_REGDUMP_H
#define APU_REGDUMP_H

struct regdump_region_info {
	char *name;
	uint32_t start;
	uint32_t size;
};

void apu_regdump(void);
void apu_regdump_init(struct platform_device *pdev);

#endif /* APU_REGDUMP_H */
