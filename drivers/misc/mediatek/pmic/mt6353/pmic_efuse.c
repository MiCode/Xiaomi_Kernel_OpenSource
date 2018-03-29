/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include <linux/delay.h>

/*****************************************************************************
 * workaround for VMC voltage list
 ******************************************************************************/
int pmic_read_VMC_efuse(void)
{
	unsigned int val;

	pmic_set_register_value(PMIC_CLK_EFUSE_CK_PDN_HWEN, 0);
	pmic_set_register_value(PMIC_CLK_EFUSE_CK_PDN, 0);
	pmic_set_register_value(PMIC_RG_OTP_RD_SW, 1);

	pmic_set_register_value(PMIC_RG_OTP_PA, 0xd * 2);
	udelay(100);
	if (pmic_get_register_value(PMIC_RG_OTP_RD_TRIG) == 0)
		pmic_set_register_value(PMIC_RG_OTP_RD_TRIG, 1);
	else
		pmic_set_register_value(PMIC_RG_OTP_RD_TRIG, 0);
	udelay(300);
	while (pmic_get_register_value(PMIC_RG_OTP_RD_BUSY) == 1)
		;
	udelay(1000);
	val = pmic_get_register_value(PMIC_RG_OTP_DOUT_SW);

	val = val * 0x80;
	pmic_set_register_value(PMIC_CLK_EFUSE_CK_PDN_HWEN, 1);
	pmic_set_register_value(PMIC_CLK_EFUSE_CK_PDN, 1);

	return val;
}

static DEFINE_MUTEX(pmic_efuse_lock_mutex);
unsigned int pmic_Read_Efuse_HPOffset(int i)
{
	unsigned int ret = 0;
	unsigned int reg_val = 0;
	unsigned int efusevalue;

	pr_debug("pmic_Read_Efuse_HPOffset(+)\n");
	mutex_lock(&pmic_efuse_lock_mutex);
	/*1. enable efuse ctrl engine clock */
	pmic_config_interface(PMIC_CLK_EFUSE_CK_PDN_HWEN_ADDR, 0x00,
	PMIC_CLK_EFUSE_CK_PDN_HWEN_MASK, PMIC_CLK_EFUSE_CK_PDN_HWEN_SHIFT);
	pmic_config_interface(PMIC_CLK_EFUSE_CK_PDN_ADDR, 0x00,
		PMIC_CLK_EFUSE_CK_PDN_MASK, PMIC_CLK_EFUSE_CK_PDN_SHIFT);
	/*2. */
	pmic_config_interface(PMIC_RG_OTP_RD_SW_ADDR, 0x01,
	PMIC_RG_OTP_RD_SW_MASK, PMIC_RG_OTP_RD_SW_SHIFT);

	/*3. set row to read */
	pmic_config_interface(PMIC_RG_OTP_PA_ADDR, i*2,
	PMIC_RG_OTP_PA_MASK, PMIC_RG_OTP_PA_SHIFT);
	/*4. Toggle */
	udelay(100);
	ret = pmic_read_interface(PMIC_RG_OTP_RD_TRIG_ADDR, &reg_val,
		PMIC_RG_OTP_RD_TRIG_MASK, PMIC_RG_OTP_RD_TRIG_SHIFT);

	if (reg_val == 0) {
		pmic_config_interface(PMIC_RG_OTP_RD_TRIG_ADDR, 1,
			PMIC_RG_OTP_RD_TRIG_MASK, PMIC_RG_OTP_RD_TRIG_SHIFT);
	} else{
		pmic_config_interface(PMIC_RG_OTP_RD_TRIG_ADDR, 0,
			PMIC_RG_OTP_RD_TRIG_MASK, PMIC_RG_OTP_RD_TRIG_SHIFT);
	}

	/*5. polling Reg[0x61A] */
	udelay(300);
	do {
		ret = pmic_read_interface(PMIC_RG_OTP_RD_BUSY_ADDR, &reg_val,
		PMIC_RG_OTP_RD_BUSY_MASK, PMIC_RG_OTP_RD_BUSY_SHIFT);
	} while (reg_val == 1);
	udelay(1000);		/*Need to delay at least 1ms for 0x61A and than can read 0xC18 */
	/*6. read data */
	ret = pmic_read_interface(PMIC_RG_OTP_DOUT_SW_ADDR, &efusevalue,
	PMIC_RG_OTP_DOUT_SW_MASK, PMIC_RG_OTP_DOUT_SW_SHIFT);

	pr_debug("HPoffset : efuse=0x%x\n", efusevalue);
	/*7. Disable efuse ctrl engine clock */
	pmic_config_interface(PMIC_CLK_EFUSE_CK_PDN_HWEN_ADDR, 0x01,
		PMIC_CLK_EFUSE_CK_PDN_HWEN_MASK, PMIC_CLK_EFUSE_CK_PDN_HWEN_SHIFT);
	pmic_config_interface(PMIC_CLK_EFUSE_CK_PDN_ADDR, 0x01,
		PMIC_CLK_EFUSE_CK_PDN_MASK, PMIC_CLK_EFUSE_CK_PDN_SHIFT);

	mutex_unlock(&pmic_efuse_lock_mutex);
	return efusevalue;
}
