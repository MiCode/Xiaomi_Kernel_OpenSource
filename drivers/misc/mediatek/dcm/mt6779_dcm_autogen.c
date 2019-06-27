// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>

#include <mt6779_dcm_internal.h>
#include <mt6779_dcm_autogen.h>
#include <mtk_dcm.h>
// ========== auto gen code 2018 0827 ====================
// ========== auto gen code 2018 0905 ====================
// ========== auto gen code 2018 0919 ====================
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x1 << 20) | \
			(0x1 << 23) | \
			(0x1 << 30))
#define INFRACFG_AO_INFRA_BUS_DCM_REG1_MASK ((0x1f << 12) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x10 << 5) | \
			(0x1 << 20) | \
			(0x1 << 23) | \
			(0x1 << 30))
#define INFRACFG_AO_INFRA_BUS_DCM_REG1_ON ((0x10 << 12) | \
			(0x1 << 17) | \
			(0x0 << 18))
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x10 << 5) | \
			(0x0 << 20) | \
			(0x0 << 23) | \
			(0x0 << 30))
#define INFRACFG_AO_INFRA_BUS_DCM_REG1_OFF ((0x10 << 12) | \
			(0x0 << 17) | \
			(0x1 << 18))

static void infracfg_ao_infra_dcm_rg_sfsel_set(unsigned int val)
{
	reg_write(INFRA_BUS_DCM_CTRL,
		(reg_read(INFRA_BUS_DCM_CTRL) &
		~(0x1f << 10)) |
		(val & 0x1f) << 10);
}

bool dcm_infracfg_ao_infra_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_BUS_DCM_CTRL) &
		INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_INFRA_BUS_DCM_REG0_ON);
	ret &= ((reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
		INFRACFG_AO_INFRA_BUS_DCM_REG1_MASK) ==
		(unsigned int) INFRACFG_AO_INFRA_BUS_DCM_REG1_ON);

	return ret;
}

void dcm_infracfg_ao_infra_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_bus_dcm'" */
		infracfg_ao_infra_dcm_rg_sfsel_set(0x1);
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG0_ON);
		reg_write(INFRA_AXIMEM_IDLE_BIT_EN_0,
			(reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG1_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_bus_dcm'" */
		infracfg_ao_infra_dcm_rg_sfsel_set(0x1);
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG0_OFF);
		reg_write(INFRA_AXIMEM_IDLE_BIT_EN_0,
			(reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG1_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG1_OFF);
	}
}

#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK ((0xf << 0))
#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON ((0x0 << 0))
#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_OFF ((0xf << 0))

bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(P2P_RX_CLK_ON) &
		INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_infra_rx_p2p_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_rx_p2p_dcm'" */
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_rx_p2p_dcm'" */
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_PERI_BUS_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x1f << 15) | \
			(0x1 << 20) | \
			(0x1 << 21))
#define INFRACFG_AO_PERI_BUS_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x1f << 5) | \
			(0x1f << 15) | \
			(0x1 << 20) | \
			(0x1 << 21))
#define INFRACFG_AO_PERI_BUS_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x1f << 5) | \
			(0x0 << 15) | \
			(0x0 << 20) | \
			(0x0 << 21))

static void infracfg_ao_peri_dcm_rg_sfsel_set(unsigned int val)
{
	reg_write(PERI_BUS_DCM_CTRL,
		(reg_read(PERI_BUS_DCM_CTRL) &
		~(0x1f << 10)) |
		(val & 0x1f) << 10);
}

bool dcm_infracfg_ao_peri_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_PERI_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_peri_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_peri_bus_dcm'" */
		infracfg_ao_peri_dcm_rg_sfsel_set(0x0);
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_peri_bus_dcm'" */
		infracfg_ao_peri_dcm_rg_sfsel_set(0x1f);
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_BUS_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK ((0x1 << 28) | \
			(0x1 << 29) | \
			(0x1 << 30))
#define INFRACFG_AO_PERI_MODULE_DCM_REG0_ON ((0x1 << 28) | \
			(0x1 << 29) | \
			(0x1 << 30))
#define INFRACFG_AO_PERI_MODULE_DCM_REG0_OFF ((0x0 << 28) | \
			(0x0 << 29) | \
			(0x0 << 30))

bool dcm_infracfg_ao_peri_module_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_PERI_MODULE_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_peri_module_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_peri_module_dcm'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_MODULE_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_peri_module_dcm'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_MODULE_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_MASK ((0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 10) | \
			(0x1 << 12))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_MASK ((0xffffffff << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_MASK ((0xffffffff << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_MASK ((0xffffffff << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_ON ((0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 10) | \
			(0x1 << 12))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_ON ((0x40388000 << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_ON ((0xff << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_ON ((0x7 << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_OFF ((0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 10) | \
			(0x0 << 12))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_OFF ((0x40388000 << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_OFF ((0x0 << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_OFF ((0x7 << 0))

bool dcm_infracfg_ao_mem_dcm_emi_group_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_EMI_DCM_CFG0) &
		INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_ON);
	ret &= ((reg_read(INFRA_EMI_DCM_CFG1) &
		INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_MASK) ==
		(unsigned int) INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_ON);
	ret &= ((reg_read(INFRA_EMI_DCM_CFG3) &
		INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_MASK) ==
		(unsigned int) INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_ON);
	ret &= ((reg_read(TOP_CK_ANCHOR_CFG) &
		INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_MASK) ==
		(unsigned int) INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_ON);

	return ret;
}

void dcm_infracfg_ao_mem_dcm_emi_group(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_mem_dcm_emi_group'" */
		reg_write(INFRA_EMI_DCM_CFG0,
			(reg_read(INFRA_EMI_DCM_CFG0) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_ON);
		reg_write(INFRA_EMI_DCM_CFG1,
			(reg_read(INFRA_EMI_DCM_CFG1) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_ON);
		reg_write(INFRA_EMI_DCM_CFG3,
			(reg_read(INFRA_EMI_DCM_CFG3) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_ON);
		reg_write(TOP_CK_ANCHOR_CFG,
			(reg_read(TOP_CK_ANCHOR_CFG) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_mem_dcm_emi_group'" */
		reg_write(INFRA_EMI_DCM_CFG0,
			(reg_read(INFRA_EMI_DCM_CFG0) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_OFF);
		reg_write(INFRA_EMI_DCM_CFG1,
			(reg_read(INFRA_EMI_DCM_CFG1) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_OFF);
		reg_write(INFRA_EMI_DCM_CFG3,
			(reg_read(INFRA_EMI_DCM_CFG3) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_OFF);
		reg_write(TOP_CK_ANCHOR_CFG,
			(reg_read(TOP_CK_ANCHOR_CFG) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG3_OFF);
	}
}

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

#define EMI_EMI_DCM_REG0_MASK ((0xff << 24))
#define EMI_EMI_DCM_REG1_MASK ((0xff << 24))
#define EMI_EMI_DCM_REG2_MASK ((0x1 << 13))
#define EMI_EMI_DCM_REG0_ON ((0x0 << 24))
#define EMI_EMI_DCM_REG1_ON ((0x0 << 24))
#define EMI_EMI_DCM_REG2_ON ((0x0 << 13))
#define EMI_EMI_DCM_REG0_OFF ((0xff << 24))
#define EMI_EMI_DCM_REG1_OFF ((0xff << 24))
#define EMI_EMI_DCM_REG2_OFF ((0x1 << 13))

bool dcm_emi_emi_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(EMI_CONM) &
		EMI_EMI_DCM_REG0_MASK) ==
		(unsigned int) EMI_EMI_DCM_REG0_ON);
	ret &= ((reg_read(EMI_CONN) &
		EMI_EMI_DCM_REG1_MASK) ==
		(unsigned int) EMI_EMI_DCM_REG1_ON);
	ret &= ((reg_read(EMI_THRO_CTRL0) &
		EMI_EMI_DCM_REG2_MASK) ==
		(unsigned int) EMI_EMI_DCM_REG2_ON);

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
		reg_write(EMI_THRO_CTRL0,
			(reg_read(EMI_THRO_CTRL0) &
			~EMI_EMI_DCM_REG2_MASK) |
			EMI_EMI_DCM_REG2_ON);
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
		reg_write(EMI_THRO_CTRL0,
			(reg_read(EMI_THRO_CTRL0) &
			~EMI_EMI_DCM_REG2_MASK) |
			EMI_EMI_DCM_REG2_OFF);
	}
}

#define MM_IOMMU_MM_MMU_DCM_CFG_REG0_MASK ((0x1 << 0) | \
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
#define MM_IOMMU_MM_MMU_DCM_CFG_REG0_ON ((0x0 << 0) | \
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
#define MM_IOMMU_MM_MMU_DCM_CFG_REG0_OFF ((0x1 << 0) | \
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

bool dcm_mm_iommu_mm_mmu_dcm_cfg_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MM_MMU_DCM_DIS) &
		MM_IOMMU_MM_MMU_DCM_CFG_REG0_MASK) ==
		(unsigned int) MM_IOMMU_MM_MMU_DCM_CFG_REG0_ON);

	return ret;
}

void dcm_mm_iommu_mm_mmu_dcm_cfg(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mm_iommu_mm_mmu_dcm_cfg'" */
		reg_write(MM_MMU_DCM_DIS,
			(reg_read(MM_MMU_DCM_DIS) &
			~MM_IOMMU_MM_MMU_DCM_CFG_REG0_MASK) |
			MM_IOMMU_MM_MMU_DCM_CFG_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mm_iommu_mm_mmu_dcm_cfg'" */
		reg_write(MM_MMU_DCM_DIS,
			(reg_read(MM_MMU_DCM_DIS) &
			~MM_IOMMU_MM_MMU_DCM_CFG_REG0_MASK) |
			MM_IOMMU_MM_MMU_DCM_CFG_REG0_OFF);
	}
}

#define DRAMC_CH0_TOP0_DDRPHY_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 26) | \
			(0x1 << 30) | \
			(0x1 << 31))
#define DRAMC_CH0_TOP0_DDRPHY_REG1_MASK ((0x1 << 31))
#define DRAMC_CH0_TOP0_DDRPHY_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x0 << 26) | \
			(0x1 << 30) | \
			(0x1 << 31))
#define DRAMC_CH0_TOP0_DDRPHY_REG1_ON ((0x1 << 31))
#define DRAMC_CH0_TOP0_DDRPHY_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x1 << 26) | \
			(0x0 << 30) | \
			(0x0 << 31))
#define DRAMC_CH0_TOP0_DDRPHY_REG1_OFF ((0x0 << 31))

bool dcm_dramc_ch0_top0_ddrphy_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DRAMC_CH0_TOP0_DRAMC_PD_CTRL) &
		DRAMC_CH0_TOP0_DDRPHY_REG0_MASK) ==
		(unsigned int) DRAMC_CH0_TOP0_DDRPHY_REG0_ON);
	ret &= ((reg_read(DRAMC_CH0_TOP0_CLKAR) &
		DRAMC_CH0_TOP0_DDRPHY_REG1_MASK) ==
		(unsigned int) DRAMC_CH0_TOP0_DDRPHY_REG1_ON);

	return ret;
}

void dcm_dramc_ch0_top0_ddrphy(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'dramc_ch0_top0_ddrphy'" */
		reg_write(DRAMC_CH0_TOP0_DRAMC_PD_CTRL,
			(reg_read(DRAMC_CH0_TOP0_DRAMC_PD_CTRL) &
			~DRAMC_CH0_TOP0_DDRPHY_REG0_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG0_ON);
		reg_write(DRAMC_CH0_TOP0_CLKAR,
			(reg_read(DRAMC_CH0_TOP0_CLKAR) &
			~DRAMC_CH0_TOP0_DDRPHY_REG1_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'dramc_ch0_top0_ddrphy'" */
		reg_write(DRAMC_CH0_TOP0_DRAMC_PD_CTRL,
			(reg_read(DRAMC_CH0_TOP0_DRAMC_PD_CTRL) &
			~DRAMC_CH0_TOP0_DDRPHY_REG0_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG0_OFF);
		reg_write(DRAMC_CH0_TOP0_CLKAR,
			(reg_read(DRAMC_CH0_TOP0_CLKAR) &
			~DRAMC_CH0_TOP0_DDRPHY_REG1_MASK) |
			DRAMC_CH0_TOP0_DDRPHY_REG1_OFF);
	}
}

#define CHN0_EMI_CHN_EMI_DCM_REG0_MASK ((0xff << 24))
#define CHN0_EMI_CHN_EMI_DCM_REG0_ON ((0x0 << 24))
#define CHN0_EMI_CHN_EMI_DCM_REG0_OFF ((0xff << 24))

bool dcm_chn0_emi_chn_emi_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CHN0_EMI_CHN_EMI_CONB) &
		CHN0_EMI_CHN_EMI_DCM_REG0_MASK) ==
		(unsigned int) CHN0_EMI_CHN_EMI_DCM_REG0_ON);

	return ret;
}

void dcm_chn0_emi_chn_emi_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'chn0_emi_chn_emi_dcm'" */
		reg_write(CHN0_EMI_CHN_EMI_CONB,
			(reg_read(CHN0_EMI_CHN_EMI_CONB) &
			~CHN0_EMI_CHN_EMI_DCM_REG0_MASK) |
			CHN0_EMI_CHN_EMI_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'chn0_emi_chn_emi_dcm'" */
		reg_write(CHN0_EMI_CHN_EMI_CONB,
			(reg_read(CHN0_EMI_CHN_EMI_CONB) &
			~CHN0_EMI_CHN_EMI_DCM_REG0_MASK) |
			CHN0_EMI_CHN_EMI_DCM_REG0_OFF);
	}
}

#define DRAMC_CH0_TOP5_DDRPHY_REG0_MASK ((0x1 << 8) | \
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
#define DRAMC_CH0_TOP5_DDRPHY_REG1_MASK ((0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 26))
#define DRAMC_CH0_TOP5_DDRPHY_REG2_MASK ((0x3 << 0))
#define DRAMC_CH0_TOP5_DDRPHY_REG0_ON ((0x0 << 8) | \
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
#define DRAMC_CH0_TOP5_DDRPHY_REG1_ON ((0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 26))
#define DRAMC_CH0_TOP5_DDRPHY_REG2_ON ((0x0 << 0))
#define DRAMC_CH0_TOP5_DDRPHY_REG0_OFF ((0x1 << 8) | \
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
#define DRAMC_CH0_TOP5_DDRPHY_REG1_OFF ((0x1 << 6) | \
			(0x1 << 7) | \
			(0x0 << 26))
#define DRAMC_CH0_TOP5_DDRPHY_REG2_OFF ((0x1 << 0))

static void dramc_ch0_top5_rg_mem_dcm_idle_fsel_set(unsigned int val)
{
	reg_write(DRAMC_CH0_TOP5_MISC_CG_CTRL2,
		(reg_read(DRAMC_CH0_TOP5_MISC_CG_CTRL2) &
		~(0x1f << 21)) |
		(val & 0x1f) << 21);
}

bool dcm_dramc_ch0_top5_ddrphy_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DRAMC_CH0_TOP5_MISC_CG_CTRL0) &
		DRAMC_CH0_TOP5_DDRPHY_REG0_MASK) ==
		(unsigned int) DRAMC_CH0_TOP5_DDRPHY_REG0_ON);
	ret &= ((reg_read(DRAMC_CH0_TOP5_MISC_CG_CTRL2) &
		DRAMC_CH0_TOP5_DDRPHY_REG1_MASK) ==
		(unsigned int) DRAMC_CH0_TOP5_DDRPHY_REG1_ON);
	ret &= ((reg_read(DRAMC_CH0_TOP5_MISC_CTRL2) &
		DRAMC_CH0_TOP5_DDRPHY_REG2_MASK) ==
		(unsigned int) DRAMC_CH0_TOP5_DDRPHY_REG2_ON);

	return ret;
}

void dcm_dramc_ch0_top5_ddrphy(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'dramc_ch0_top5_ddrphy'" */
		dramc_ch0_top5_rg_mem_dcm_idle_fsel_set(0x8);
		reg_write(DRAMC_CH0_TOP5_MISC_CG_CTRL0,
			(reg_read(DRAMC_CH0_TOP5_MISC_CG_CTRL0) &
			~DRAMC_CH0_TOP5_DDRPHY_REG0_MASK) |
			DRAMC_CH0_TOP5_DDRPHY_REG0_ON);
		reg_write(DRAMC_CH0_TOP5_MISC_CG_CTRL2,
			(reg_read(DRAMC_CH0_TOP5_MISC_CG_CTRL2) &
			~DRAMC_CH0_TOP5_DDRPHY_REG1_MASK) |
			DRAMC_CH0_TOP5_DDRPHY_REG1_ON);
		reg_write(DRAMC_CH0_TOP5_MISC_CTRL2,
			(reg_read(DRAMC_CH0_TOP5_MISC_CTRL2) &
			~DRAMC_CH0_TOP5_DDRPHY_REG2_MASK) |
			DRAMC_CH0_TOP5_DDRPHY_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'dramc_ch0_top5_ddrphy'" */
		dramc_ch0_top5_rg_mem_dcm_idle_fsel_set(0x0);
		reg_write(DRAMC_CH0_TOP5_MISC_CG_CTRL0,
			(reg_read(DRAMC_CH0_TOP5_MISC_CG_CTRL0) &
			~DRAMC_CH0_TOP5_DDRPHY_REG0_MASK) |
			DRAMC_CH0_TOP5_DDRPHY_REG0_OFF);
		reg_write(DRAMC_CH0_TOP5_MISC_CG_CTRL2,
			(reg_read(DRAMC_CH0_TOP5_MISC_CG_CTRL2) &
			~DRAMC_CH0_TOP5_DDRPHY_REG1_MASK) |
			DRAMC_CH0_TOP5_DDRPHY_REG1_OFF);
		reg_write(DRAMC_CH0_TOP5_MISC_CTRL2,
			(reg_read(DRAMC_CH0_TOP5_MISC_CTRL2) &
			~DRAMC_CH0_TOP5_DDRPHY_REG2_MASK) |
			DRAMC_CH0_TOP5_DDRPHY_REG2_OFF);
	}
}

#define DRAMC_CH1_TOP0_DDRPHY_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 26) | \
			(0x1 << 30) | \
			(0x1 << 31))
#define DRAMC_CH1_TOP0_DDRPHY_REG1_MASK ((0x1 << 31))
#define DRAMC_CH1_TOP0_DDRPHY_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x0 << 26) | \
			(0x1 << 30) | \
			(0x1 << 31))
#define DRAMC_CH1_TOP0_DDRPHY_REG1_ON ((0x1 << 31))
#define DRAMC_CH1_TOP0_DDRPHY_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x1 << 26) | \
			(0x0 << 30) | \
			(0x0 << 31))
#define DRAMC_CH1_TOP0_DDRPHY_REG1_OFF ((0x0 << 31))

bool dcm_dramc_ch1_top0_ddrphy_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DRAMC_CH1_TOP0_DRAMC_PD_CTRL) &
		DRAMC_CH1_TOP0_DDRPHY_REG0_MASK) ==
		(unsigned int) DRAMC_CH1_TOP0_DDRPHY_REG0_ON);
	ret &= ((reg_read(DRAMC_CH1_TOP0_CLKAR) &
		DRAMC_CH1_TOP0_DDRPHY_REG1_MASK) ==
		(unsigned int) DRAMC_CH1_TOP0_DDRPHY_REG1_ON);

	return ret;
}

void dcm_dramc_ch1_top0_ddrphy(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'dramc_ch1_top0_ddrphy'" */
		reg_write(DRAMC_CH1_TOP0_DRAMC_PD_CTRL,
			(reg_read(DRAMC_CH1_TOP0_DRAMC_PD_CTRL) &
			~DRAMC_CH1_TOP0_DDRPHY_REG0_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG0_ON);
		reg_write(DRAMC_CH1_TOP0_CLKAR,
			(reg_read(DRAMC_CH1_TOP0_CLKAR) &
			~DRAMC_CH1_TOP0_DDRPHY_REG1_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'dramc_ch1_top0_ddrphy'" */
		reg_write(DRAMC_CH1_TOP0_DRAMC_PD_CTRL,
			(reg_read(DRAMC_CH1_TOP0_DRAMC_PD_CTRL) &
			~DRAMC_CH1_TOP0_DDRPHY_REG0_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG0_OFF);
		reg_write(DRAMC_CH1_TOP0_CLKAR,
			(reg_read(DRAMC_CH1_TOP0_CLKAR) &
			~DRAMC_CH1_TOP0_DDRPHY_REG1_MASK) |
			DRAMC_CH1_TOP0_DDRPHY_REG1_OFF);
	}
}

#define DRAMC_CH1_TOP5_DDRPHY_REG0_MASK ((0x1 << 8) | \
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
#define DRAMC_CH1_TOP5_DDRPHY_REG1_MASK ((0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 26))
#define DRAMC_CH1_TOP5_DDRPHY_REG2_MASK ((0x3 << 0))
#define DRAMC_CH1_TOP5_DDRPHY_REG0_ON ((0x0 << 8) | \
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
#define DRAMC_CH1_TOP5_DDRPHY_REG1_ON ((0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 26))
#define DRAMC_CH1_TOP5_DDRPHY_REG2_ON ((0x0 << 0))
#define DRAMC_CH1_TOP5_DDRPHY_REG0_OFF ((0x1 << 8) | \
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
#define DRAMC_CH1_TOP5_DDRPHY_REG1_OFF ((0x1 << 6) | \
			(0x1 << 7) | \
			(0x0 << 26))
#define DRAMC_CH1_TOP5_DDRPHY_REG2_OFF ((0x1 << 0))

static void dramc_ch1_top5_rg_mem_dcm_idle_fsel_set(unsigned int val)
{
	reg_write(DRAMC_CH1_TOP5_MISC_CG_CTRL2,
		(reg_read(DRAMC_CH1_TOP5_MISC_CG_CTRL2) &
		~(0x1f << 21)) |
		(val & 0x1f) << 21);
}

bool dcm_dramc_ch1_top5_ddrphy_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DRAMC_CH1_TOP5_MISC_CG_CTRL0) &
		DRAMC_CH1_TOP5_DDRPHY_REG0_MASK) ==
		(unsigned int) DRAMC_CH1_TOP5_DDRPHY_REG0_ON);
	ret &= ((reg_read(DRAMC_CH1_TOP5_MISC_CG_CTRL2) &
		DRAMC_CH1_TOP5_DDRPHY_REG1_MASK) ==
		(unsigned int) DRAMC_CH1_TOP5_DDRPHY_REG1_ON);
	ret &= ((reg_read(DRAMC_CH1_TOP5_MISC_CTRL2) &
		DRAMC_CH1_TOP5_DDRPHY_REG2_MASK) ==
		(unsigned int) DRAMC_CH1_TOP5_DDRPHY_REG2_ON);

	return ret;
}

void dcm_dramc_ch1_top5_ddrphy(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'dramc_ch1_top5_ddrphy'" */
		dramc_ch1_top5_rg_mem_dcm_idle_fsel_set(0x8);
		reg_write(DRAMC_CH1_TOP5_MISC_CG_CTRL0,
			(reg_read(DRAMC_CH1_TOP5_MISC_CG_CTRL0) &
			~DRAMC_CH1_TOP5_DDRPHY_REG0_MASK) |
			DRAMC_CH1_TOP5_DDRPHY_REG0_ON);
		reg_write(DRAMC_CH1_TOP5_MISC_CG_CTRL2,
			(reg_read(DRAMC_CH1_TOP5_MISC_CG_CTRL2) &
			~DRAMC_CH1_TOP5_DDRPHY_REG1_MASK) |
			DRAMC_CH1_TOP5_DDRPHY_REG1_ON);
		reg_write(DRAMC_CH1_TOP5_MISC_CTRL2,
			(reg_read(DRAMC_CH1_TOP5_MISC_CTRL2) &
			~DRAMC_CH1_TOP5_DDRPHY_REG2_MASK) |
			DRAMC_CH1_TOP5_DDRPHY_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'dramc_ch1_top5_ddrphy'" */
		dramc_ch1_top5_rg_mem_dcm_idle_fsel_set(0x0);
		reg_write(DRAMC_CH1_TOP5_MISC_CG_CTRL0,
			(reg_read(DRAMC_CH1_TOP5_MISC_CG_CTRL0) &
			~DRAMC_CH1_TOP5_DDRPHY_REG0_MASK) |
			DRAMC_CH1_TOP5_DDRPHY_REG0_OFF);
		reg_write(DRAMC_CH1_TOP5_MISC_CG_CTRL2,
			(reg_read(DRAMC_CH1_TOP5_MISC_CG_CTRL2) &
			~DRAMC_CH1_TOP5_DDRPHY_REG1_MASK) |
			DRAMC_CH1_TOP5_DDRPHY_REG1_OFF);
		reg_write(DRAMC_CH1_TOP5_MISC_CTRL2,
			(reg_read(DRAMC_CH1_TOP5_MISC_CTRL2) &
			~DRAMC_CH1_TOP5_DDRPHY_REG2_MASK) |
			DRAMC_CH1_TOP5_DDRPHY_REG2_OFF);
	}
}

#define VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_MASK ((0x1 << 0) | \
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
#define VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_ON ((0x0 << 0) | \
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
#define VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_OFF ((0x1 << 0) | \
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

bool dcm_vpu_iommu_vpu_mmu_dcm_cfg_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VPU_MMU_DCM_DIS) &
		VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_MASK) ==
		(unsigned int) VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_ON);

	return ret;
}

void dcm_vpu_iommu_vpu_mmu_dcm_cfg(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'vpu_iommu_vpu_mmu_dcm_cfg'" */
		reg_write(VPU_MMU_DCM_DIS,
			(reg_read(VPU_MMU_DCM_DIS) &
			~VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_MASK) |
			VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'vpu_iommu_vpu_mmu_dcm_cfg'" */
		reg_write(VPU_MMU_DCM_DIS,
			(reg_read(VPU_MMU_DCM_DIS) &
			~VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_MASK) |
			VPU_IOMMU_VPU_MMU_DCM_CFG_REG0_OFF);
	}
}

#define SSPM_SSPM_DCM_REG0_MASK ((0x1 << 8))
#define SSPM_SSPM_DCM_REG1_MASK ((0x1fffff << 0))
#define SSPM_SSPM_DCM_REG0_ON ((0x1 << 8))
#define SSPM_SSPM_DCM_REG1_ON ((0x1fbfff << 0))
#define SSPM_SSPM_DCM_REG0_OFF ((0x0 << 8))
#define SSPM_SSPM_DCM_REG1_OFF ((0x0 << 0))

bool dcm_sspm_sspm_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(SSPM_MCLK_DIV) &
		SSPM_SSPM_DCM_REG0_MASK) ==
		(unsigned int) SSPM_SSPM_DCM_REG0_ON);
	ret &= ((reg_read(SSPM_DCM_CTRL) &
		SSPM_SSPM_DCM_REG1_MASK) ==
		(unsigned int) SSPM_SSPM_DCM_REG1_ON);

	return ret;
}

void dcm_sspm_sspm_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'sspm_sspm_dcm'" */
		reg_write(SSPM_MCLK_DIV,
			(reg_read(SSPM_MCLK_DIV) &
			~SSPM_SSPM_DCM_REG0_MASK) |
			SSPM_SSPM_DCM_REG0_ON);
		reg_write(SSPM_DCM_CTRL,
			(reg_read(SSPM_DCM_CTRL) &
			~SSPM_SSPM_DCM_REG1_MASK) |
			SSPM_SSPM_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'sspm_sspm_dcm'" */
		reg_write(SSPM_MCLK_DIV,
			(reg_read(SSPM_MCLK_DIV) &
			~SSPM_SSPM_DCM_REG0_MASK) |
			SSPM_SSPM_DCM_REG0_OFF);
		reg_write(SSPM_DCM_CTRL,
			(reg_read(SSPM_DCM_CTRL) &
			~SSPM_SSPM_DCM_REG1_MASK) |
			SSPM_SSPM_DCM_REG1_OFF);
	}
}

#define AUDIO_AUD_MAS_AHB_CK_DCM_REG0_MASK ((0x1 << 29) | \
			(0x1 << 30))
#define AUDIO_AUD_MAS_AHB_CK_DCM_REG0_ON ((0x1 << 29) | \
			(0x1 << 30))
#define AUDIO_AUD_MAS_AHB_CK_DCM_REG0_OFF ((0x0 << 29) | \
			(0x0 << 30))

bool dcm_audio_aud_mas_ahb_ck_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(AUDIO_TOP_CON0) &
		AUDIO_AUD_MAS_AHB_CK_DCM_REG0_MASK) ==
		(unsigned int) AUDIO_AUD_MAS_AHB_CK_DCM_REG0_ON);

	return ret;
}

void dcm_audio_aud_mas_ahb_ck_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'audio_aud_mas_ahb_ck_dcm'" */
		reg_write(AUDIO_TOP_CON0,
			(reg_read(AUDIO_TOP_CON0) &
			~AUDIO_AUD_MAS_AHB_CK_DCM_REG0_MASK) |
			AUDIO_AUD_MAS_AHB_CK_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'audio_aud_mas_ahb_ck_dcm'" */
		reg_write(AUDIO_TOP_CON0,
			(reg_read(AUDIO_TOP_CON0) &
			~AUDIO_AUD_MAS_AHB_CK_DCM_REG0_MASK) |
			AUDIO_AUD_MAS_AHB_CK_DCM_REG0_OFF);
	}
}

#define MSDC1_DCMEN_DCM_REG0_MASK ((0x1 << 21))
#define MSDC1_DCMEN_DCM_REG0_ON ((0x0 << 21))
#define MSDC1_DCMEN_DCM_REG0_OFF ((0x1 << 21))

bool dcm_msdc1_dcmen_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_DCMEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_DCMEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_dcmen_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_dcmen_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_DCMEN_DCM_REG0_MASK) |
			MSDC1_DCMEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_dcmen_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_DCMEN_DCM_REG0_MASK) |
			MSDC1_DCMEN_DCM_REG0_OFF);
	}
}

#define MSDC1_HGDMACKEN_DCM_REG0_MASK ((0x1 << 23))
#define MSDC1_HGDMACKEN_DCM_REG0_ON ((0x0 << 23))
#define MSDC1_HGDMACKEN_DCM_REG0_OFF ((0x1 << 23))

bool dcm_msdc1_hgdmacken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_HGDMACKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_HGDMACKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_hgdmacken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_hgdmacken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_HGDMACKEN_DCM_REG0_MASK) |
			MSDC1_HGDMACKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_hgdmacken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_HGDMACKEN_DCM_REG0_MASK) |
			MSDC1_HGDMACKEN_DCM_REG0_OFF);
	}
}

#define MSDC1_MACMDCKEN_DCM_REG0_MASK ((0x1 << 27))
#define MSDC1_MACMDCKEN_DCM_REG0_ON ((0x0 << 27))
#define MSDC1_MACMDCKEN_DCM_REG0_OFF ((0x1 << 27))

bool dcm_msdc1_macmdcken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_MACMDCKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_MACMDCKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_macmdcken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_macmdcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MACMDCKEN_DCM_REG0_MASK) |
			MSDC1_MACMDCKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_macmdcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MACMDCKEN_DCM_REG0_MASK) |
			MSDC1_MACMDCKEN_DCM_REG0_OFF);
	}
}

#define MSDC1_MPSCCKEN_DCM_REG0_MASK ((0x1 << 25))
#define MSDC1_MPSCCKEN_DCM_REG0_ON ((0x0 << 25))
#define MSDC1_MPSCCKEN_DCM_REG0_OFF ((0x1 << 25))

bool dcm_msdc1_mpsccken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_MPSCCKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_MPSCCKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_mpsccken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_mpsccken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MPSCCKEN_DCM_REG0_MASK) |
			MSDC1_MPSCCKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_mpsccken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MPSCCKEN_DCM_REG0_MASK) |
			MSDC1_MPSCCKEN_DCM_REG0_OFF);
	}
}

#define MSDC1_MRCTLCKEN_DCM_REG0_MASK ((0x1 << 30))
#define MSDC1_MRCTLCKEN_DCM_REG0_ON ((0x0 << 30))
#define MSDC1_MRCTLCKEN_DCM_REG0_OFF ((0x1 << 30))

bool dcm_msdc1_mrctlcken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_MRCTLCKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_MRCTLCKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_mrctlcken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_mrctlcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MRCTLCKEN_DCM_REG0_MASK) |
			MSDC1_MRCTLCKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_mrctlcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MRCTLCKEN_DCM_REG0_MASK) |
			MSDC1_MRCTLCKEN_DCM_REG0_OFF);
	}
}

#define MSDC1_MSDCKEN_DCM_REG0_MASK ((0x1 << 28))
#define MSDC1_MSDCKEN_DCM_REG0_ON ((0x0 << 28))
#define MSDC1_MSDCKEN_DCM_REG0_OFF ((0x1 << 28))

bool dcm_msdc1_msdcken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_MSDCKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_MSDCKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_msdcken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_msdcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MSDCKEN_DCM_REG0_MASK) |
			MSDC1_MSDCKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_msdcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MSDCKEN_DCM_REG0_MASK) |
			MSDC1_MSDCKEN_DCM_REG0_OFF);
	}
}

#define MSDC1_MSHBFCKEN_DCM_REG0_MASK ((0x1 << 31))
#define MSDC1_MSHBFCKEN_DCM_REG0_ON ((0x0 << 31))
#define MSDC1_MSHBFCKEN_DCM_REG0_OFF ((0x1 << 31))

bool dcm_msdc1_mshbfcken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_MSHBFCKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_MSHBFCKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_mshbfcken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_mshbfcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MSHBFCKEN_DCM_REG0_MASK) |
			MSDC1_MSHBFCKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_mshbfcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MSHBFCKEN_DCM_REG0_MASK) |
			MSDC1_MSHBFCKEN_DCM_REG0_OFF);
	}
}

#define MSDC1_MSPCCKEN_DCM_REG0_MASK ((0x1 << 24))
#define MSDC1_MSPCCKEN_DCM_REG0_ON ((0x0 << 24))
#define MSDC1_MSPCCKEN_DCM_REG0_OFF ((0x1 << 24))

bool dcm_msdc1_mspccken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_MSPCCKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_MSPCCKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_mspccken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_mspccken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MSPCCKEN_DCM_REG0_MASK) |
			MSDC1_MSPCCKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_mspccken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MSPCCKEN_DCM_REG0_MASK) |
			MSDC1_MSPCCKEN_DCM_REG0_OFF);
	}
}

#define MSDC1_MVOLDTCKEN_DCM_REG0_MASK ((0x1 << 26))
#define MSDC1_MVOLDTCKEN_DCM_REG0_ON ((0x0 << 26))
#define MSDC1_MVOLDTCKEN_DCM_REG0_OFF ((0x1 << 26))

bool dcm_msdc1_mvoldtcken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_MVOLDTCKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_MVOLDTCKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_mvoldtcken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_mvoldtcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MVOLDTCKEN_DCM_REG0_MASK) |
			MSDC1_MVOLDTCKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_mvoldtcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MVOLDTCKEN_DCM_REG0_MASK) |
			MSDC1_MVOLDTCKEN_DCM_REG0_OFF);
	}
}

#define MSDC1_MWCTLCKEN_DCM_REG0_MASK ((0x1 << 29))
#define MSDC1_MWCTLCKEN_DCM_REG0_ON ((0x0 << 29))
#define MSDC1_MWCTLCKEN_DCM_REG0_OFF ((0x1 << 29))

bool dcm_msdc1_mwctlcken_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MSDC1_PATCH_BIT1) &
		MSDC1_MWCTLCKEN_DCM_REG0_MASK) ==
		(unsigned int) MSDC1_MWCTLCKEN_DCM_REG0_ON);

	return ret;
}

void dcm_msdc1_mwctlcken_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'msdc1_mwctlcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MWCTLCKEN_DCM_REG0_MASK) |
			MSDC1_MWCTLCKEN_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'msdc1_mwctlcken_dcm'" */
		reg_write(MSDC1_PATCH_BIT1,
			(reg_read(MSDC1_PATCH_BIT1) &
			~MSDC1_MWCTLCKEN_DCM_REG0_MASK) |
			MSDC1_MWCTLCKEN_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_ADB_DCM_REG0_MASK ((0x1 << 16) | \
			(0x1 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_MASK ((0x1 << 16) | \
			(0x1 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_MASK ((0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22))
#define MP_CPUSYS_TOP_ADB_DCM_REG3_MASK ((0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20))
#define MP_CPUSYS_TOP_ADB_DCM_REG0_ON ((0x1 << 16) | \
			(0x1 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_ON ((0x1 << 16) | \
			(0x1 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_ON ((0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22))
#define MP_CPUSYS_TOP_ADB_DCM_REG3_ON ((0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20))
#define MP_CPUSYS_TOP_ADB_DCM_REG0_OFF ((0x0 << 16) | \
			(0x0 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_OFF ((0x0 << 16) | \
			(0x0 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_OFF ((0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22))
#define MP_CPUSYS_TOP_ADB_DCM_REG3_OFF ((0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20))

bool dcm_mp_cpusys_top_adb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_ADB_DCM_CFG0) &
		MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG0_ON);
	ret &= ((reg_read(MP_ADB_DCM_CFG2) &
		MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG1_ON);
	ret &= ((reg_read(MP_ADB_DCM_CFG4) &
		MP_CPUSYS_TOP_ADB_DCM_REG2_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG2_ON);
	ret &= ((reg_read(MCUSYS_DCM_CFG0) &
		MP_CPUSYS_TOP_ADB_DCM_REG3_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG3_ON);

	return ret;
}

void dcm_mp_cpusys_top_adb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_adb_dcm'" */
		reg_write(MP_ADB_DCM_CFG0,
			(reg_read(MP_ADB_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG0_ON);
		reg_write(MP_ADB_DCM_CFG2,
			(reg_read(MP_ADB_DCM_CFG2) &
			~MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG1_ON);
		reg_write(MP_ADB_DCM_CFG4,
			(reg_read(MP_ADB_DCM_CFG4) &
			~MP_CPUSYS_TOP_ADB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG2_ON);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG3_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG3_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_adb_dcm'" */
		reg_write(MP_ADB_DCM_CFG0,
			(reg_read(MP_ADB_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG0_OFF);
		reg_write(MP_ADB_DCM_CFG2,
			(reg_read(MP_ADB_DCM_CFG2) &
			~MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG1_OFF);
		reg_write(MP_ADB_DCM_CFG4,
			(reg_read(MP_ADB_DCM_CFG4) &
			~MP_CPUSYS_TOP_ADB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG2_OFF);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG3_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG3_OFF);
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
			(0x1 << 4))
#define MP_CPUSYS_TOP_MISC_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_MISC_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
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

#define MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK ((0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG0_ON ((0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG1_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG0_OFF ((0x0 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG1_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3))

bool dcm_mp_cpusys_top_mp0_qdcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_MISC_DCM_CFG0) &
		MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_MP0_QDCM_REG0_ON);
	ret &= ((reg_read(MP0_DCM_CFG0) &
		MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_MP0_QDCM_REG1_ON);

	return ret;
}

void dcm_mp_cpusys_top_mp0_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_mp0_qdcm'" */
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG0_ON);
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_mp0_qdcm'" */
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG0_OFF);
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG1_OFF);
	}
}

#define CPCCFG_REG_EMI_WFIFO_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define CPCCFG_REG_EMI_WFIFO_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define CPCCFG_REG_EMI_WFIFO_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3))

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

#define CPCCFG_REG_MP_STALL_DCM_REG0_MASK ((0x1 << 4))
#define CPCCFG_REG_MP_STALL_DCM_REG0_ON ((0x1 << 4))
#define CPCCFG_REG_MP_STALL_DCM_REG0_OFF ((0x0 << 4))

bool dcm_cpccfg_reg_mp_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(SLOW_CK_CFG) &
		CPCCFG_REG_MP_STALL_DCM_REG0_MASK) ==
		(unsigned int) CPCCFG_REG_MP_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_cpccfg_reg_mp_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'cpccfg_reg_mp_stall_dcm'" */
		reg_write(SLOW_CK_CFG,
			(reg_read(SLOW_CK_CFG) &
			~CPCCFG_REG_MP_STALL_DCM_REG0_MASK) |
			CPCCFG_REG_MP_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'cpccfg_reg_mp_stall_dcm'" */
		reg_write(SLOW_CK_CFG,
			(reg_read(SLOW_CK_CFG) &
			~CPCCFG_REG_MP_STALL_DCM_REG0_MASK) |
			CPCCFG_REG_MP_STALL_DCM_REG0_OFF);
	}
}

