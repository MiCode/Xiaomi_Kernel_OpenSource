/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

//#include <mt-plat/mtk_io.h>
//#include <mt-plat/sync_write.h>
/* #include <mt-plat/mtk_secure_api.h> */
#include <linux/io.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>

#include <mt6768_dcm_internal.h>
#include <mt6768_dcm_autogen.h>
#include <mtk_dcm.h>
// ========== auto gen code 2018 1213 ====================
#define INFRACFG_AO_AUDIO_BUS_REG0_MASK ((0x1 << 29))
#define INFRACFG_AO_AUDIO_BUS_REG0_ON ((0x1 << 29))
#define INFRACFG_AO_AUDIO_BUS_REG0_OFF ((0x0 << 29))

bool dcm_infracfg_ao_audio_bus_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_AUDIO_BUS_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_AUDIO_BUS_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_audio_bus(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_audio_bus'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_AUDIO_BUS_REG0_MASK) |
			INFRACFG_AO_AUDIO_BUS_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_audio_bus'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_AUDIO_BUS_REG0_MASK) |
			INFRACFG_AO_AUDIO_BUS_REG0_OFF);
	}
}

#define INFRACFG_AO_ICUSB_BUS_REG0_MASK ((0x1 << 28))
#define INFRACFG_AO_ICUSB_BUS_REG0_ON ((0x1 << 28))
#define INFRACFG_AO_ICUSB_BUS_REG0_OFF ((0x0 << 28))

bool dcm_infracfg_ao_icusb_bus_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_ICUSB_BUS_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_ICUSB_BUS_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_icusb_bus(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_icusb_bus'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_ICUSB_BUS_REG0_MASK) |
			INFRACFG_AO_ICUSB_BUS_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_icusb_bus'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_ICUSB_BUS_REG0_MASK) |
			INFRACFG_AO_ICUSB_BUS_REG0_OFF);
	}
}

#define INFRACFG_AO_INFRA_BUS_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x1f << 10) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1 << 23) | \
			(0x1 << 30))
#define INFRACFG_AO_INFRA_BUS_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x10 << 5) | \
			(0x1 << 10) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1 << 23) | \
			(0x1 << 30))
#define INFRACFG_AO_INFRA_BUS_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x10 << 5) | \
			(0x1 << 10) | \
			(0x0 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x0 << 23) | \
			(0x0 << 30))

bool dcm_infracfg_ao_infra_bus_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_BUS_DCM_CTRL) &
		INFRACFG_AO_INFRA_BUS_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_INFRA_BUS_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_infra_bus(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_bus'" */
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_bus'" */
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_REG0_OFF);
	}
}

#define INFRACFG_AO_INFRA_DFS_MEM_REG0_MASK ((0x1 << 0) | \
			(0x1f << 1) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1f << 16) | \
			(0x1f << 21) | \
			(0x1 << 26))
#define INFRACFG_AO_INFRA_DFS_MEM_REG0_ON ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x0 << 16) | \
			(0x1f << 21) | \
			(0x0 << 26))
#define INFRACFG_AO_INFRA_DFS_MEM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x0 << 16) | \
			(0x1f << 21) | \
			(0x0 << 26))

bool dcm_infracfg_ao_infra_dfs_mem_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DFS_MEM_DCM_CTRL) &
		INFRACFG_AO_INFRA_DFS_MEM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_INFRA_DFS_MEM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_infra_dfs_mem(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_dfs_mem'" */
		reg_write(DFS_MEM_DCM_CTRL,
			(reg_read(DFS_MEM_DCM_CTRL) &
			~INFRACFG_AO_INFRA_DFS_MEM_REG0_MASK) |
			INFRACFG_AO_INFRA_DFS_MEM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_dfs_mem'" */
		reg_write(DFS_MEM_DCM_CTRL,
			(reg_read(DFS_MEM_DCM_CTRL) &
			~INFRACFG_AO_INFRA_DFS_MEM_REG0_MASK) |
			INFRACFG_AO_INFRA_DFS_MEM_REG0_OFF);
	}
}

#if 0
#define INFRACFG_AO_INFRA_MEM_REG0_MASK ((0x1 << 0) | \
			(0x1f << 1) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x7f << 9) | \
			(0x1f << 16) | \
			(0x1f << 21) | \
			(0x1 << 26) | \
			(0x1 << 27) | \
			(0x1 << 28) | \
			(0x1 << 29) | \
			(0x1 << 31))
#define INFRACFG_AO_INFRA_MEM_REG0_ON ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x0 << 9) | \
			(0x0 << 16) | \
			(0x1f << 21) | \
			(0x0 << 26) | \
			(0x1 << 27) | \
			(0x0 << 28) | \
			(0x0 << 29) | \
			(0x0 << 31))
#define INFRACFG_AO_INFRA_MEM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x0 << 9) | \
			(0x0 << 16) | \
			(0x1f << 21) | \
			(0x0 << 26) | \
			(0x1 << 27) | \
			(0x0 << 28) | \
			(0x0 << 29) | \
			(0x0 << 31))
#endif
#define INFRACFG_AO_INFRA_MEM_REG0_MASK (0x1 << 27)
#define INFRACFG_AO_INFRA_MEM_REG0_ON (0x1 << 27)
#define INFRACFG_AO_INFRA_MEM_REG0_OFF (0x0 << 27)


bool dcm_infracfg_ao_infra_mem_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MEM_DCM_CTRL) &
		INFRACFG_AO_INFRA_MEM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_INFRA_MEM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_infra_mem(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_mem'" */
		reg_write(MEM_DCM_CTRL,
			(reg_read(MEM_DCM_CTRL) &
			~INFRACFG_AO_INFRA_MEM_REG0_MASK) |
			INFRACFG_AO_INFRA_MEM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_mem'" */
		reg_write(MEM_DCM_CTRL,
			(reg_read(MEM_DCM_CTRL) &
			~INFRACFG_AO_INFRA_MEM_REG0_MASK) |
			INFRACFG_AO_INFRA_MEM_REG0_OFF);
	}
}

#define INFRACFG_AO_PERI_BUS_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x1f << 10) | \
			(0x1f << 15) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1f << 23) | \
			(0x1 << 31))
#define INFRACFG_AO_PERI_BUS_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x1f << 5) | \
			(0x0 << 10) | \
			(0x1f << 15) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x0 << 23) | \
			(0x1 << 31))
#define INFRACFG_AO_PERI_BUS_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x1f << 5) | \
			(0x1f << 10) | \
			(0x0 << 15) | \
			(0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22) | \
			(0x0 << 23) | \
			(0x0 << 31))

bool dcm_infracfg_ao_peri_bus_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_PERI_BUS_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_PERI_BUS_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_peri_bus(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_peri_bus'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_BUS_REG0_MASK) |
			INFRACFG_AO_PERI_BUS_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_peri_bus'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_BUS_REG0_MASK) |
			INFRACFG_AO_PERI_BUS_REG0_OFF);
	}
}
#if 0
#define PWRAP_PMIC_WRAP_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22))
#define PWRAP_PMIC_WRAP_REG0_ON ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12) | \
			(0x0 << 13) | \
			(0x0 << 14) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22))
#define PWRAP_PMIC_WRAP_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12) | \
			(0x0 << 13) | \
			(0x0 << 14) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22))

bool dcm_pwrap_pmic_wrap_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PMIC_WRAP_DCM_EN) &
		PWRAP_PMIC_WRAP_REG0_MASK) ==
		(unsigned int) PWRAP_PMIC_WRAP_REG0_ON);

	return ret;
}

void dcm_pwrap_pmic_wrap(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'pwrap_pmic_wrap'" */
		reg_write(PMIC_WRAP_DCM_EN,
			(reg_read(PMIC_WRAP_DCM_EN) &
			~PWRAP_PMIC_WRAP_REG0_MASK) |
			PWRAP_PMIC_WRAP_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'pwrap_pmic_wrap'" */
		reg_write(PMIC_WRAP_DCM_EN,
			(reg_read(PMIC_WRAP_DCM_EN) &
			~PWRAP_PMIC_WRAP_REG0_MASK) |
			PWRAP_PMIC_WRAP_REG0_OFF);
	}
}
#endif
#define EMI_EMI_DCM_REG0_MASK ((0xff << 24))
#define EMI_EMI_DCM_REG1_MASK ((0xff << 24))
#define EMI_EMI_DCM_REG0_ON ((0x0 << 24))
#define EMI_EMI_DCM_REG1_ON ((0x0 << 24))
#define EMI_EMI_DCM_REG0_OFF ((0xff << 24))
#define EMI_EMI_DCM_REG1_OFF ((0xff << 24))

bool dcm_emi_emi_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(EMI_CONM) &
		EMI_EMI_DCM_REG0_MASK) ==
		(unsigned int) EMI_EMI_DCM_REG0_ON);
	ret &= ((reg_read(EMI_CONN) &
		EMI_EMI_DCM_REG1_MASK) ==
		(unsigned int) EMI_EMI_DCM_REG1_ON);

	return ret;
}

void dcm_emi_emi_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'emi_emi_dcm'" */
		reg_write(EMI_CONM,
			(reg_read(EMI_CONM) &
			~EMI_EMI_DCM_REG0_MASK) |
			EMI_EMI_DCM_REG0_ON);
		reg_write(EMI_CONN,
			(reg_read(EMI_CONN) &
			~EMI_EMI_DCM_REG1_MASK) |
			EMI_EMI_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'emi_emi_dcm'" */
		reg_write(EMI_CONM,
			(reg_read(EMI_CONM) &
			~EMI_EMI_DCM_REG0_MASK) |
			EMI_EMI_DCM_REG0_OFF);
		reg_write(EMI_CONN,
			(reg_read(EMI_CONN) &
			~EMI_EMI_DCM_REG1_MASK) |
			EMI_EMI_DCM_REG1_OFF);
	}
}

#define DRAMC_CH0_TOP0_DDRPHY_REG0_MASK ((0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 19))
#define DRAMC_CH0_TOP0_DDRPHY_REG1_MASK ((0x1 << 6) | \
			(0x1 << 7) | \
			(0x1f << 21) | \
			(0x1 << 26))
#define DRAMC_CH0_TOP0_DDRPHY_REG2_MASK ((0x1 << 26) | \
			(0x1 << 27))
#define DRAMC_CH0_TOP0_DDRPHY_REG0_ON ((0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12) | \
			(0x0 << 13) | \
			(0x0 << 14) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 19))
#define DRAMC_CH0_TOP0_DDRPHY_REG1_ON ((0x0 << 6) | \
			(0x0 << 7) | \
			(0x8 << 21) | \
			(0x0 << 26))
#define DRAMC_CH0_TOP0_DDRPHY_REG2_ON ((0x0 << 26) | \
			(0x0 << 27))
#define DRAMC_CH0_TOP0_DDRPHY_REG0_OFF ((0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 19))
#define DRAMC_CH0_TOP0_DDRPHY_REG1_OFF ((0x1 << 6) | \
			(0x1 << 7) | \
			(0x0 << 21) | \
			(0x0 << 26))
#define DRAMC_CH0_TOP0_DDRPHY_REG2_OFF ((0x1 << 26) | \
			(0x1 << 27))

bool dcm_dramc_ch0_top0_ddrphy_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DRAMC_CH0_TOP0_MISC_CG_CTRL0) &
		DRAMC_CH0_TOP0_DDRPHY_REG0_MASK) ==
		(unsigned int) DRAMC_CH0_TOP0_DDRPHY_REG0_ON);
	ret &= ((reg_read(DRAMC_CH0_TOP0_MISC_CG_CTRL2) &
		DRAMC_CH0_TOP0_DDRPHY_REG1_MASK) ==
		(unsigned int) DRAMC_CH0_TOP0_DDRPHY_REG1_ON);
	ret &= ((reg_read(DRAMC_CH0_TOP0_MISC_CTRL3) &
		DRAMC_CH0_TOP0_DDRPHY_REG2_MASK) ==
		(unsigned int) DRAMC_CH0_TOP0_DDRPHY_REG2_ON);

	return ret;
}

void dcm_dramc_ch0_top0_ddrphy(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'dramc_ch0_top0_ddrphy'" */
		reg_write(DRAMC_CH0_TOP0_MISC_CG_CTRL0,
			(reg_read(DRAMC_CH0_TOP0_MISC_CG_CTRL0) &
			~DRAMC_CH0_TOP0_DDRPHY_REG0_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG0_ON);
		reg_write(DRAMC_CH0_TOP0_MISC_CG_CTRL2,
			(reg_read(DRAMC_CH0_TOP0_MISC_CG_CTRL2) &
			~DRAMC_CH0_TOP0_DDRPHY_REG1_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG1_ON);
		reg_write(DRAMC_CH0_TOP0_MISC_CTRL3,
			(reg_read(DRAMC_CH0_TOP0_MISC_CTRL3) &
			~DRAMC_CH0_TOP0_DDRPHY_REG2_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'dramc_ch0_top0_ddrphy'" */
		reg_write(DRAMC_CH0_TOP0_MISC_CG_CTRL0,
			(reg_read(DRAMC_CH0_TOP0_MISC_CG_CTRL0) &
			~DRAMC_CH0_TOP0_DDRPHY_REG0_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG0_OFF);
		reg_write(DRAMC_CH0_TOP0_MISC_CG_CTRL2,
			(reg_read(DRAMC_CH0_TOP0_MISC_CG_CTRL2) &
			~DRAMC_CH0_TOP0_DDRPHY_REG1_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG1_OFF);
		reg_write(DRAMC_CH0_TOP0_MISC_CTRL3,
			(reg_read(DRAMC_CH0_TOP0_MISC_CTRL3) &
			~DRAMC_CH0_TOP0_DDRPHY_REG2_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG2_OFF);
	}
}

#define DRAMC_CH0_TOP1_DRAMC_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 26) | \
			(0x1 << 30) | \
			(0x1 << 31))
#define DRAMC_CH0_TOP1_DRAMC_DCM_REG1_MASK ((0x1 << 31))
#define DRAMC_CH0_TOP1_DRAMC_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x0 << 26) | \
			(0x1 << 30) | \
			(0x1 << 31))
#define DRAMC_CH0_TOP1_DRAMC_DCM_REG1_ON ((0x1 << 31))
#define DRAMC_CH0_TOP1_DRAMC_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x1 << 26) | \
			(0x0 << 30) | \
			(0x0 << 31))
#define DRAMC_CH0_TOP1_DRAMC_DCM_REG1_OFF ((0x0 << 31))

bool dcm_dramc_ch0_top1_dramc_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DRAMC_CH0_TOP1_DRAMC_PD_CTRL) &
		DRAMC_CH0_TOP1_DRAMC_DCM_REG0_MASK) ==
		(unsigned int) DRAMC_CH0_TOP1_DRAMC_DCM_REG0_ON);
	ret &= ((reg_read(DRAMC_CH0_TOP1_CLKAR) &
		DRAMC_CH0_TOP1_DRAMC_DCM_REG1_MASK) ==
		(unsigned int) DRAMC_CH0_TOP1_DRAMC_DCM_REG1_ON);

	return ret;
}

void dcm_dramc_ch0_top1_dramc_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'dramc_ch0_top1_dramc_dcm'" */
		reg_write(DRAMC_CH0_TOP1_DRAMC_PD_CTRL,
			(reg_read(DRAMC_CH0_TOP1_DRAMC_PD_CTRL) &
			~DRAMC_CH0_TOP1_DRAMC_DCM_REG0_MASK) |
			DRAMC_CH0_TOP1_DRAMC_DCM_REG0_ON);
		reg_write(DRAMC_CH0_TOP1_CLKAR,
			(reg_read(DRAMC_CH0_TOP1_CLKAR) &
			~DRAMC_CH0_TOP1_DRAMC_DCM_REG1_MASK) |
			DRAMC_CH0_TOP1_DRAMC_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'dramc_ch0_top1_dramc_dcm'" */
		reg_write(DRAMC_CH0_TOP1_DRAMC_PD_CTRL,
			(reg_read(DRAMC_CH0_TOP1_DRAMC_PD_CTRL) &
			~DRAMC_CH0_TOP1_DRAMC_DCM_REG0_MASK) |
			DRAMC_CH0_TOP1_DRAMC_DCM_REG0_OFF);
		reg_write(DRAMC_CH0_TOP1_CLKAR,
			(reg_read(DRAMC_CH0_TOP1_CLKAR) &
			~DRAMC_CH0_TOP1_DRAMC_DCM_REG1_MASK) |
			DRAMC_CH0_TOP1_DRAMC_DCM_REG1_OFF);
	}
}

#define CHN0_EMI_EMI_DCM_REG0_MASK ((0xff << 24))
#define CHN0_EMI_EMI_DCM_REG0_ON ((0x0 << 24))
#define CHN0_EMI_EMI_DCM_REG0_OFF ((0xff << 24))

bool dcm_chn0_emi_emi_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CHN0_EMI_CHN_EMI_CONB) &
		CHN0_EMI_EMI_DCM_REG0_MASK) ==
		(unsigned int) CHN0_EMI_EMI_DCM_REG0_ON);

	return ret;
}

void dcm_chn0_emi_emi_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'chn0_emi_emi_dcm'" */
		reg_write(CHN0_EMI_CHN_EMI_CONB,
			(reg_read(CHN0_EMI_CHN_EMI_CONB) &
			~CHN0_EMI_EMI_DCM_REG0_MASK) |
			CHN0_EMI_EMI_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'chn0_emi_emi_dcm'" */
		reg_write(CHN0_EMI_CHN_EMI_CONB,
			(reg_read(CHN0_EMI_CHN_EMI_CONB) &
			~CHN0_EMI_EMI_DCM_REG0_MASK) |
			CHN0_EMI_EMI_DCM_REG0_OFF);
	}
}

#define DRAMC_CH1_TOP0_DDRPHY_REG0_MASK ((0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 19))
#define DRAMC_CH1_TOP0_DDRPHY_REG1_MASK ((0x1 << 6) | \
			(0x1 << 7) | \
			(0x1f << 21) | \
			(0x1 << 26))
#define DRAMC_CH1_TOP0_DDRPHY_REG2_MASK ((0x1 << 26) | \
			(0x1 << 27))
#define DRAMC_CH1_TOP0_DDRPHY_REG0_ON ((0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12) | \
			(0x0 << 13) | \
			(0x0 << 14) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 19))
#define DRAMC_CH1_TOP0_DDRPHY_REG1_ON ((0x0 << 6) | \
			(0x0 << 7) | \
			(0x8 << 21) | \
			(0x0 << 26))
#define DRAMC_CH1_TOP0_DDRPHY_REG2_ON ((0x0 << 26) | \
			(0x0 << 27))
#define DRAMC_CH1_TOP0_DDRPHY_REG0_OFF ((0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 19))
#define DRAMC_CH1_TOP0_DDRPHY_REG1_OFF ((0x1 << 6) | \
			(0x1 << 7) | \
			(0x0 << 21) | \
			(0x0 << 26))
#define DRAMC_CH1_TOP0_DDRPHY_REG2_OFF ((0x1 << 26) | \
			(0x1 << 27))

bool dcm_dramc_ch1_top0_ddrphy_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DRAMC_CH1_TOP0_MISC_CG_CTRL0) &
		DRAMC_CH1_TOP0_DDRPHY_REG0_MASK) ==
		(unsigned int) DRAMC_CH1_TOP0_DDRPHY_REG0_ON);
	ret &= ((reg_read(DRAMC_CH1_TOP0_MISC_CG_CTRL2) &
		DRAMC_CH1_TOP0_DDRPHY_REG1_MASK) ==
		(unsigned int) DRAMC_CH1_TOP0_DDRPHY_REG1_ON);
	ret &= ((reg_read(DRAMC_CH1_TOP0_MISC_CTRL3) &
		DRAMC_CH1_TOP0_DDRPHY_REG2_MASK) ==
		(unsigned int) DRAMC_CH1_TOP0_DDRPHY_REG2_ON);

	return ret;
}

void dcm_dramc_ch1_top0_ddrphy(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'dramc_ch1_top0_ddrphy'" */
		reg_write(DRAMC_CH1_TOP0_MISC_CG_CTRL0,
			(reg_read(DRAMC_CH1_TOP0_MISC_CG_CTRL0) &
			~DRAMC_CH1_TOP0_DDRPHY_REG0_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG0_ON);
		reg_write(DRAMC_CH1_TOP0_MISC_CG_CTRL2,
			(reg_read(DRAMC_CH1_TOP0_MISC_CG_CTRL2) &
			~DRAMC_CH1_TOP0_DDRPHY_REG1_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG1_ON);
		reg_write(DRAMC_CH1_TOP0_MISC_CTRL3,
			(reg_read(DRAMC_CH1_TOP0_MISC_CTRL3) &
			~DRAMC_CH1_TOP0_DDRPHY_REG2_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'dramc_ch1_top0_ddrphy'" */
		reg_write(DRAMC_CH1_TOP0_MISC_CG_CTRL0,
			(reg_read(DRAMC_CH1_TOP0_MISC_CG_CTRL0) &
			~DRAMC_CH1_TOP0_DDRPHY_REG0_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG0_OFF);
		reg_write(DRAMC_CH1_TOP0_MISC_CG_CTRL2,
			(reg_read(DRAMC_CH1_TOP0_MISC_CG_CTRL2) &
			~DRAMC_CH1_TOP0_DDRPHY_REG1_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG1_OFF);
		reg_write(DRAMC_CH1_TOP0_MISC_CTRL3,
			(reg_read(DRAMC_CH1_TOP0_MISC_CTRL3) &
			~DRAMC_CH1_TOP0_DDRPHY_REG2_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG2_OFF);
	}
}

#define DRAMC_CH1_TOP1_DRAMC_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 26) | \
			(0x1 << 30) | \
			(0x1 << 31))
#define DRAMC_CH1_TOP1_DRAMC_DCM_REG1_MASK ((0x1 << 31))
#define DRAMC_CH1_TOP1_DRAMC_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x0 << 26) | \
			(0x1 << 30) | \
			(0x1 << 31))
#define DRAMC_CH1_TOP1_DRAMC_DCM_REG1_ON ((0x1 << 31))
#define DRAMC_CH1_TOP1_DRAMC_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x1 << 26) | \
			(0x0 << 30) | \
			(0x0 << 31))
#define DRAMC_CH1_TOP1_DRAMC_DCM_REG1_OFF ((0x0 << 31))

bool dcm_dramc_ch1_top1_dramc_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DRAMC_CH1_TOP1_DRAMC_PD_CTRL) &
		DRAMC_CH1_TOP1_DRAMC_DCM_REG0_MASK) ==
		(unsigned int) DRAMC_CH1_TOP1_DRAMC_DCM_REG0_ON);
	ret &= ((reg_read(DRAMC_CH1_TOP1_CLKAR) &
		DRAMC_CH1_TOP1_DRAMC_DCM_REG1_MASK) ==
		(unsigned int) DRAMC_CH1_TOP1_DRAMC_DCM_REG1_ON);

	return ret;
}

void dcm_dramc_ch1_top1_dramc_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'dramc_ch1_top1_dramc_dcm'" */
		reg_write(DRAMC_CH1_TOP1_DRAMC_PD_CTRL,
			(reg_read(DRAMC_CH1_TOP1_DRAMC_PD_CTRL) &
			~DRAMC_CH1_TOP1_DRAMC_DCM_REG0_MASK) |
			DRAMC_CH1_TOP1_DRAMC_DCM_REG0_ON);
		reg_write(DRAMC_CH1_TOP1_CLKAR,
			(reg_read(DRAMC_CH1_TOP1_CLKAR) &
			~DRAMC_CH1_TOP1_DRAMC_DCM_REG1_MASK) |
			DRAMC_CH1_TOP1_DRAMC_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'dramc_ch1_top1_dramc_dcm'" */
		reg_write(DRAMC_CH1_TOP1_DRAMC_PD_CTRL,
			(reg_read(DRAMC_CH1_TOP1_DRAMC_PD_CTRL) &
			~DRAMC_CH1_TOP1_DRAMC_DCM_REG0_MASK) |
			DRAMC_CH1_TOP1_DRAMC_DCM_REG0_OFF);
		reg_write(DRAMC_CH1_TOP1_CLKAR,
			(reg_read(DRAMC_CH1_TOP1_CLKAR) &
			~DRAMC_CH1_TOP1_DRAMC_DCM_REG1_MASK) |
			DRAMC_CH1_TOP1_DRAMC_DCM_REG1_OFF);
	}
}

#define CHN1_EMI_EMI_DCM_REG0_MASK ((0xff << 24))
#define CHN1_EMI_EMI_DCM_REG0_ON ((0x0 << 24))
#define CHN1_EMI_EMI_DCM_REG0_OFF ((0xff << 24))

bool dcm_chn1_emi_emi_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CHN1_EMI_CHN_EMI_CONB) &
		CHN1_EMI_EMI_DCM_REG0_MASK) ==
		(unsigned int) CHN1_EMI_EMI_DCM_REG0_ON);

	return ret;
}

void dcm_chn1_emi_emi_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'chn1_emi_emi_dcm'" */
		reg_write(CHN1_EMI_CHN_EMI_CONB,
			(reg_read(CHN1_EMI_CHN_EMI_CONB) &
			~CHN1_EMI_EMI_DCM_REG0_MASK) |
			CHN1_EMI_EMI_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'chn1_emi_emi_dcm'" */
		reg_write(CHN1_EMI_CHN_EMI_CONB,
			(reg_read(CHN1_EMI_CHN_EMI_CONB) &
			~CHN1_EMI_EMI_DCM_REG0_MASK) |
			CHN1_EMI_EMI_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_ADB_DCM_REG0_MASK ((0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_MASK ((0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define MP_CPUSYS_TOP_ADB_DCM_REG0_ON ((0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_ON ((0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define MP_CPUSYS_TOP_ADB_DCM_REG0_OFF ((0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_OFF ((0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18))

bool dcm_mp_cpusys_top_adb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_ADB_DCM_CFG4) &
		MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_DCM_CFG0) &
		MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG1_ON);

	return ret;
}

void dcm_mp_cpusys_top_adb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_adb_dcm'" */
		reg_write(MP_ADB_DCM_CFG4,
			(reg_read(MP_ADB_DCM_CFG4) &
			~MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG0_ON);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_adb_dcm'" */
		reg_write(MP_ADB_DCM_CFG4,
			(reg_read(MP_ADB_DCM_CFG4) &
			~MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG0_OFF);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG1_OFF);
	}
}

#define MP_CPUSYS_TOP_APB_DCM_REG0_MASK ((0x1 << 5))
#define MP_CPUSYS_TOP_APB_DCM_REG1_MASK ((0x1 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_MASK ((0x1 << 16))
#define MP_CPUSYS_TOP_APB_DCM_REG0_ON ((0x1 << 5))
#define MP_CPUSYS_TOP_APB_DCM_REG1_ON ((0x1 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_ON ((0x1 << 16))
#define MP_CPUSYS_TOP_APB_DCM_REG0_OFF ((0x0 << 5))
#define MP_CPUSYS_TOP_APB_DCM_REG1_OFF ((0x0 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_OFF ((0x0 << 16))

bool dcm_mp_cpusys_top_apb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_MISC_DCM_CFG0) &
		MP_CPUSYS_TOP_APB_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_APB_DCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_DCM_CFG0) &
		MP_CPUSYS_TOP_APB_DCM_REG1_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_APB_DCM_REG1_ON);
	ret &= ((reg_read(MP0_DCM_CFG0) &
		MP_CPUSYS_TOP_APB_DCM_REG2_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_APB_DCM_REG2_ON);

	return ret;
}

void dcm_mp_cpusys_top_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_apb_dcm'" */
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG0_ON);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG1_ON);
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_apb_dcm'" */
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG0_OFF);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG1_OFF);
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG2_OFF);
	}
}

#define MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK ((0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_ON ((0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_OFF ((0x0 << 11) | \
			(0x0 << 24) | \
			(0x0 << 25))

bool dcm_mp_cpusys_top_bus_pll_div_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(BUS_PLLDIV_CFG) &
		MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_bus_pll_div_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_bus_pll_div_dcm'" */
		reg_write(BUS_PLLDIV_CFG,
			(reg_read(BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_bus_pll_div_dcm'" */
		reg_write(BUS_PLLDIV_CFG,
			(reg_read(BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK ((0x1 << 0))
#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_ON ((0x1 << 0))
#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mp_cpusys_top_core_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP0_DCM_CFG7) &
		MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_core_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_core_stall_dcm'" */
		reg_write(MP0_DCM_CFG7,
			(reg_read(MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_core_stall_dcm'" */
		reg_write(MP0_DCM_CFG7,
			(reg_read(MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_MASK ((0x1 << 0))
#define MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_ON ((0x0 << 0))
#define MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_OFF ((0x1 << 0))

bool dcm_mp_cpusys_top_cpubiu_dbg_cg_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCSI_CFG2) &
		MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_cpubiu_dbg_cg(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpubiu_dbg_cg'" */
		reg_write(MCSI_CFG2,
			(reg_read(MCSI_CFG2) &
			~MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_MASK) |
			MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpubiu_dbg_cg'" */
		reg_write(MCSI_CFG2,
			(reg_read(MCSI_CFG2) &
			~MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_MASK) |
			MP_CPUSYS_TOP_CPUBIU_DBG_CG_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK ((0xffff << 0))
#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_ON ((0xffff << 0))
#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mp_cpusys_top_cpubiu_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCSI_DCM0) &
		MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CPUBIU_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_cpubiu_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpubiu_dcm'" */
		reg_write(MCSI_DCM0,
			(reg_read(MCSI_DCM0) &
			~MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPUBIU_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpubiu_dcm'" */
		reg_write(MCSI_DCM0,
			(reg_read(MCSI_DCM0) &
			~MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPUBIU_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK ((0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_ON ((0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_OFF ((0x0 << 11) | \
			(0x0 << 24) | \
			(0x0 << 25))

bool dcm_mp_cpusys_top_cpu_pll_div_0_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CPU_PLLDIV_CFG0) &
		MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_cpu_pll_div_0_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpu_pll_div_0_dcm'" */
		reg_write(CPU_PLLDIV_CFG0,
			(reg_read(CPU_PLLDIV_CFG0) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpu_pll_div_0_dcm'" */
		reg_write(CPU_PLLDIV_CFG0,
			(reg_read(CPU_PLLDIV_CFG0) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK ((0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_ON ((0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_OFF ((0x0 << 11) | \
			(0x0 << 24) | \
			(0x0 << 25))

bool dcm_mp_cpusys_top_cpu_pll_div_1_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CPU_PLLDIV_CFG1) &
		MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_cpu_pll_div_1_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpu_pll_div_1_dcm'" */
		reg_write(CPU_PLLDIV_CFG1,
			(reg_read(CPU_PLLDIV_CFG1) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpu_pll_div_1_dcm'" */
		reg_write(CPU_PLLDIV_CFG1,
			(reg_read(CPU_PLLDIV_CFG1) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_MASK ((0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_ON ((0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_OFF ((0x0 << 11) | \
			(0x0 << 24) | \
			(0x0 << 25))

bool dcm_mp_cpusys_top_cpu_pll_div_2_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CPU_PLLDIV_CFG2) &
		MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_cpu_pll_div_2_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpu_pll_div_2_dcm'" */
		reg_write(CPU_PLLDIV_CFG2,
			(reg_read(CPU_PLLDIV_CFG2) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpu_pll_div_2_dcm'" */
		reg_write(CPU_PLLDIV_CFG2,
			(reg_read(CPU_PLLDIV_CFG2) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK ((0x1 << 4))
#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_ON ((0x1 << 4))
#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_OFF ((0x0 << 4))

bool dcm_mp_cpusys_top_fcm_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP0_DCM_CFG7) &
		MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_fcm_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_fcm_stall_dcm'" */
		reg_write(MP0_DCM_CFG7,
			(reg_read(MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_fcm_stall_dcm'" */
		reg_write(MP0_DCM_CFG7,
			(reg_read(MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK ((0x1 << 31))
#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_ON ((0x1 << 31))
#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_OFF ((0x0 << 31))

bool dcm_mp_cpusys_top_last_cor_idle_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(BUS_PLLDIV_CFG) &
		MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_last_cor_idle_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_last_cor_idle_dcm'" */
		reg_write(BUS_PLLDIV_CFG,
			(reg_read(BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_last_cor_idle_dcm'" */
		reg_write(BUS_PLLDIV_CFG,
			(reg_read(BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_MISC_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_MISC_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_MISC_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4))

bool dcm_mp_cpusys_top_misc_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_MISC_DCM_CFG0) &
		MP_CPUSYS_TOP_MISC_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_MISC_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_misc_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_misc_dcm'" */
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MISC_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_MISC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_misc_dcm'" */
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MISC_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_MISC_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3))

bool dcm_mp_cpusys_top_mp0_qdcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP0_DCM_CFG0) &
		MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_MP0_QDCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_mp0_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_mp0_qdcm'" */
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_mp0_qdcm'" */
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG0_OFF);
	}
}

#define CPCCFG_REG_EMI_WFIFO_REG0_MASK ((0x1 << 0) | \
			(0x1 << 2))
#define CPCCFG_REG_EMI_WFIFO_REG0_ON ((0x1 << 0) | \
			(0x1 << 2))
#define CPCCFG_REG_EMI_WFIFO_REG0_OFF ((0x0 << 0) | \
			(0x0 << 2))

bool dcm_cpccfg_reg_emi_wfifo_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(EMI_WFIFO) &
		CPCCFG_REG_EMI_WFIFO_REG0_MASK) ==
		(unsigned int) CPCCFG_REG_EMI_WFIFO_REG0_ON);

	return ret;
}

void dcm_cpccfg_reg_emi_wfifo(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'cpccfg_reg_emi_wfifo'" */
		reg_write(EMI_WFIFO,
			(reg_read(EMI_WFIFO) &
			~CPCCFG_REG_EMI_WFIFO_REG0_MASK) |
			CPCCFG_REG_EMI_WFIFO_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'cpccfg_reg_emi_wfifo'" */
		reg_write(EMI_WFIFO,
			(reg_read(EMI_WFIFO) &
			~CPCCFG_REG_EMI_WFIFO_REG0_MASK) |
			CPCCFG_REG_EMI_WFIFO_REG0_OFF);
	}
}
