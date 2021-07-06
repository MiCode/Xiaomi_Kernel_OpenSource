/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "mt6311-i2c.h"
#include "mt6311.h"

unsigned int thr_l_int_status;
unsigned int thr_h_int_status;

void mt6311_clr_thr_l_int_status(void)
{
	thr_l_int_status = 0;
	MT6311LOG("[%s]\n", __func__);
}

void mt6311_clr_thr_h_int_status(void)
{
	thr_h_int_status = 0;
	MT6311LOG("[%s]\n", __func__);
}

void mt6311_set_rg_int_en(unsigned char val)
{
	unsigned char ret = 0;

	ret = mt6311_config_interface(
			(unsigned char) (MT6311_TOP_INT_CON),
			(unsigned char) (val),
			(unsigned char) (MT6311_PMIC_RG_INT_EN_MASK),
			(unsigned char) (MT6311_PMIC_RG_INT_EN_SHIFT));
}

unsigned char mt6311_get_rg_thr_l_int_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;

	ret = mt6311_read_interface(
			(unsigned char) (MT6311_TOP_INT_MON),
			(&val),
			(unsigned char) (MT6311_PMIC_RG_THR_L_INT_STATUS_MASK),
			(unsigned char) (MT6311_PMIC_RG_THR_L_INT_STATUS_SHIFT)
	    );

	return val;
}

unsigned char mt6311_get_rg_thr_h_int_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;

	ret = mt6311_read_interface(
			(unsigned char) (MT6311_TOP_INT_MON),
			(&val),
			(unsigned char) (MT6311_PMIC_RG_THR_H_INT_STATUS_MASK),
			(unsigned char) (MT6311_PMIC_RG_THR_H_INT_STATUS_SHIFT)
	    );

	return val;
}

unsigned char mt6311_get_pmu_thr_status(void)
{
	unsigned char ret = 0;
	unsigned char val = 0;

	ret = mt6311_read_interface(
			(unsigned char) (MT6311_STRUP_CON2),
			(&val),
			(unsigned char) (MT6311_PMIC_PMU_THR_STATUS_MASK),
			(unsigned char) (MT6311_PMIC_PMU_THR_STATUS_SHIFT)
	    );

	return val;
}


void mt6311_set_rg_strup_thr_110_clr(unsigned char val)
{
	unsigned char ret = 0;

	ret = mt6311_config_interface(
			(unsigned char) (MT6311_STRUP_CON14),
			(unsigned char) (val),
			(unsigned char) (MT6311_PMIC_RG_STRUP_THR_110_CLR_MASK),
			(unsigned char) (MT6311_PMIC_RG_STRUP_THR_110_CLR_SHIFT)
	    );
}

void mt6311_set_rg_strup_thr_125_clr(unsigned char val)
{
	unsigned char ret = 0;

	ret = mt6311_config_interface(
			(unsigned char) (MT6311_STRUP_CON14),
			(unsigned char) (val),
			(unsigned char) (MT6311_PMIC_RG_STRUP_THR_125_CLR_MASK),
			(unsigned char) (MT6311_PMIC_RG_STRUP_THR_125_CLR_SHIFT)
	    );
}

void mt6311_set_rg_strup_thr_110_irq_en(unsigned char val)
{
	unsigned char ret = 0;

	ret = mt6311_config_interface(
		(unsigned char) (MT6311_STRUP_CON14),
		(unsigned char) (val),
		(unsigned char) (MT6311_PMIC_RG_STRUP_THR_110_IRQ_EN_MASK),
		(unsigned char) (MT6311_PMIC_RG_STRUP_THR_110_IRQ_EN_SHIFT)
	    );
}

void mt6311_set_rg_strup_thr_125_irq_en(unsigned char val)
{
	unsigned char ret = 0;

	ret = mt6311_config_interface(
		(unsigned char) (MT6311_STRUP_CON14),
		(unsigned char) (val),
		(unsigned char) (MT6311_PMIC_RG_STRUP_THR_125_IRQ_EN_MASK),
		(unsigned char) (MT6311_PMIC_RG_STRUP_THR_125_IRQ_EN_SHIFT)
	    );
}

void mt6311_set_rg_thrdet_sel(unsigned char val)
{
	unsigned char ret = 0;

	ret = mt6311_config_interface(
			(unsigned char) (MT6311_STRUP_ANA_CON0),
			(unsigned char) (val),
			(unsigned char) (MT6311_PMIC_RG_THRDET_SEL_MASK),
			(unsigned char) (MT6311_PMIC_RG_THRDET_SEL_SHIFT)
	    );
}
