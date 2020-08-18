// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include <apusys_device.h>

#include "mnoc_drv.h"

#include <mnoc_util.h>
#include <mnoc_plat_api.h>
#include "mnoc_api.h"


/* for Kernel Native SMC API */
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>


enum APUSYS_MNOC_SMC_ID {
	MNOC_INFRA2APU_SRAM_EN,
	MNOC_INFRA2APU_SRAM_DIS,
	MNOC_APU2INFRA_BUS_PROTECT_EN,
	MNOC_APU2INFRA_BUS_PROTECT_DIS,

	NR_APUSYS_MNOC_SMC_ID
};


const struct of_device_id *mnoc_util_get_device_id(void)
{
	return mnoc_plat_get_device();
}


/* After APUSYS top power on */
void infra2apu_sram_en(void)
{
	struct arm_smccc_res res;

	LOG_DEBUG("+\n");

	/*
	 * arm_smccc_smc (unsigned long a0, unsigned long a1,
	 *	unsigned long a2, unsigned long a3, unsigned long a4,
	 *	unsigned long a5, unsigned long a6, unsigned long a7,
	 *	struct arm_smccc_res *res)
	 */
	arm_smccc_smc(MTK_SIP_APUSYS_MNOC_CONTROL,
		MNOC_INFRA2APU_SRAM_EN,
		0, 0, 0, 0, 0, 0, &res);

	LOG_DEBUG("-\n");
}

/* Before APUSYS top power off */
void infra2apu_sram_dis(void)
{
	struct arm_smccc_res res;

	LOG_DEBUG("+\n");

	/*
	 * arm_smccc_smc (unsigned long a0, unsigned long a1,
	 *	unsigned long a2, unsigned long a3, unsigned long a4,
	 *	unsigned long a5, unsigned long a6, unsigned long a7,
	 *	struct arm_smccc_res *res)
	 */
	arm_smccc_smc(MTK_SIP_APUSYS_MNOC_CONTROL,
		MNOC_INFRA2APU_SRAM_DIS,
		0, 0, 0, 0, 0, 0, &res);

	LOG_DEBUG("-\n");
}

/* Before APUSYS reset */
void apu2infra_bus_protect_en(void)
{
	struct arm_smccc_res res;

	LOG_DEBUG("+\n");

	/*
	 * arm_smccc_smc (unsigned long a0, unsigned long a1,
	 *	unsigned long a2, unsigned long a3, unsigned long a4,
	 *	unsigned long a5, unsigned long a6, unsigned long a7,
	 *	struct arm_smccc_res *res)
	 */
	arm_smccc_smc(MTK_SIP_APUSYS_MNOC_CONTROL,
		MNOC_APU2INFRA_BUS_PROTECT_EN,
		0, 0, 0, 0, 0, 0, &res);

	LOG_DEBUG("-\n");
}

/* After APUSYS reset */
void apu2infra_bus_protect_dis(void)
{
	struct arm_smccc_res res;

	LOG_DEBUG("+\n");

	/*
	 * arm_smccc_smc (unsigned long a0, unsigned long a1,
	 *	unsigned long a2, unsigned long a3, unsigned long a4,
	 *	unsigned long a5, unsigned long a6, unsigned long a7,
	 *	struct arm_smccc_res *res)
	 */
	arm_smccc_smc(MTK_SIP_APUSYS_MNOC_CONTROL,
		MNOC_APU2INFRA_BUS_PROTECT_DIS,
		0, 0, 0, 0, 0, 0, &res);

	LOG_DEBUG("-\n");
}

