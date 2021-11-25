/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef APU_REGDUMP_H
#define APU_REGDUMP_H

#define NAME_MAX_LEN   30
#define REGION_MAX_NUM 50

struct apusys_regdump_region_info {
	char name[NAME_MAX_LEN + 1];
	uint32_t start;
	uint32_t size;
};

struct apusys_regdump_info {
	uint64_t size;
	uint32_t region_info_num;
	struct apusys_regdump_region_info region_info[REGION_MAX_NUM];
};

void apu_regdump(void);
void apu_regdump_init(struct platform_device *pdev);

#endif /* APU_REGDUMP_H */
