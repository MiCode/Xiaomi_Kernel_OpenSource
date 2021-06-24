/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_PLAT_H__
#define __APUSYS_REVISER_PLAT_H__

struct reviser_plat {
	int (*init)(struct platform_device *pdev);
	int (*uninit)(struct platform_device *pdev);

	unsigned int bank_size;
	unsigned int mdla_max;
	unsigned int vpu_max;
	unsigned int edma_max;
	unsigned int up_max;
};

int reviser_plat_init(struct platform_device *pdev);
int reviser_plat_uninit(struct platform_device *pdev);

int reviser_v1_0_init(struct platform_device *pdev);
int reviser_v1_0_uninit(struct platform_device *pdev);

int reviser_vrv_init(struct platform_device *pdev);
int reviser_vrv_uninit(struct platform_device *pdev);
#endif
