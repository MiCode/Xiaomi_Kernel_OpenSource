/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/delay.h>
#include <linux/mutex.h>
#include <mt-plat/upmu_common.h>

static DEFINE_MUTEX(pmic_efuse_lock_mutex);

unsigned int pmic_read_efuse_nolock(int i)
{
	unsigned int efuse_data = 0;

	/* 1. enable efuse ctrl engine clock */
	pmic_set_register_value(PMIC_TOP_CKHWEN_CON0_CLR,
				1 << PMIC_RG_EFUSE_CK_PDN_HWEN_SHIFT);
	pmic_set_register_value(PMIC_TOP_CKPDN_CON0_CLR,
				1 << PMIC_RG_EFUSE_CK_PDN_SHIFT);
	/* 2. */
	pmic_set_register_value(PMIC_RG_OTP_RD_SW, 1);
	/* 3. Set row to read */
	pmic_set_register_value(PMIC_RG_OTP_PA, i * 2);
	/* 4. Toggle RG_OTP_RD_TRIG */
	if (pmic_get_register_value(PMIC_RG_OTP_RD_TRIG) == 0)
		pmic_set_register_value(PMIC_RG_OTP_RD_TRIG, 1);
	else
		pmic_set_register_value(PMIC_RG_OTP_RD_TRIG, 0);
	/* 5. Polling RG_OTP_RD_BUSY = 0 */
	udelay(300);
	while (pmic_get_register_value(PMIC_RG_OTP_RD_BUSY) == 1)
		;
	/* 6. Read RG_OTP_DOUT_SW */
	udelay(100);
	efuse_data = pmic_get_register_value(PMIC_RG_OTP_DOUT_SW);
	/* 7. disable efuse ctrl engine clock */
	pmic_set_register_value(PMIC_TOP_CKHWEN_CON0_SET,
				1 << PMIC_RG_EFUSE_CK_PDN_HWEN_SHIFT);
	pmic_set_register_value(PMIC_TOP_CKPDN_CON0_SET,
				1 << PMIC_RG_EFUSE_CK_PDN_SHIFT);

	return efuse_data;
}

unsigned int pmic_Read_Efuse_HPOffset(int i)
{
	unsigned int efuse_data = 0;

	mutex_lock(&pmic_efuse_lock_mutex);
	efuse_data = pmic_read_efuse_nolock(i);
	mutex_unlock(&pmic_efuse_lock_mutex);
	return efuse_data;
}

