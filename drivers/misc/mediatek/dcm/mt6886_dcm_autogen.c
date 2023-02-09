// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/io.h>

#include <mt6886_dcm_internal.h>
#include <mt6886_dcm_autogen.h>
#include <mtk_dcm.h>
/*====================auto gen code 20220415_154900=====================*/
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK ((0x1f << 12) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON ((0x10 << 12) | \
			(0x1 << 17) | \
			(0x0 << 18))
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_OFF ((0x10 << 12) | \
			(0x0 << 17) | \
			(0x1 << 18))

bool dcm_infracfg_ao_aximem_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
		INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_aximem_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_aximem_bus_dcm'" */
		reg_write(INFRA_AXIMEM_IDLE_BIT_EN_0,
			(reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
			~INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_aximem_bus_dcm'" */
		reg_write(INFRA_AXIMEM_IDLE_BIT_EN_0,
			(reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
			~INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_AXIMEM_BUS_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x1 << 20) | \
			(0x1 << 23) | \
			(0x1 << 30))
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x10 << 5) | \
			(0x1 << 20) | \
			(0x1 << 23) | \
			(0x1 << 30))
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 3) | \
			(0x1 << 4) | \
			(0x10 << 5) | \
			(0x0 << 20) | \
			(0x0 << 23) | \
			(0x0 << 30))

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
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_bus_dcm'" */
		infracfg_ao_infra_dcm_rg_sfsel_set(0x1);
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG0_OFF);
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

#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK ((0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 9) | \
			(0x1f << 18))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_MASK ((0xfff << 0) | \
			(0x1 << 12))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON ((0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 9) | \
			(0x0 << 18))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_ON ((0x10 << 0) | \
			(0x1 << 12))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_OFF ((0x1 << 2) | \
			(0x1 << 3) | \
			(0x0 << 9) | \
			(0x0 << 18))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_OFF ((0x10 << 0) | \
			(0x0 << 12))

bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
		INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1) &
		INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_MASK) ==
		(unsigned int) INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_ON);

	return ret;
}

void dcm_infra_ao_bcrm_infra_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infra_ao_bcrm_infra_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infra_ao_bcrm_infra_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_OFF);
	}
}

#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK ((0x1 << 5) | \
			(0x1 << 6))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK ((0x1 << 10) | \
			(0x1f << 20))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_MASK ((0xfff << 0) | \
			(0x1 << 12))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_ON ((0x1 << 5) | \
			(0x1 << 6))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_ON ((0x1 << 10) | \
			(0x0 << 20))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_ON ((0x10 << 0) | \
			(0x1 << 12))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_OFF ((0x1 << 5) | \
			(0x1 << 6))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_OFF ((0x0 << 10) | \
			(0x0 << 20))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_OFF ((0x10 << 0) | \
			(0x0 << 12))

bool dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
		INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK) ==
		(unsigned int) INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2) &
		INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK) ==
		(unsigned int) INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3) &
		INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_MASK) ==
		(unsigned int) INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_ON);

	return ret;
}

void dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(int on)
{
	if (on) {
			/* TINFO = "Turn ON DCM 'infra_ao_bcrm_infra_bus_fmem_sub_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_ON);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_ON);
	} else {
			/* TINFO = "Turn OFF DCM 'infra_ao_bcrm_infra_bus_fmem_sub_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_OFF);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_OFF);
	}
}

#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK ((0x1 << 4) | \
			(0x1 << 7) | \
			(0x1 << 10) | \
			(0x1 << 12))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK ((0x1 << 15))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK ((0x1 << 13))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON ((0x1 << 4) | \
			(0x1 << 7) | \
			(0x1 << 10) | \
			(0x1 << 12))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON ((0x1 << 15))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON ((0x1 << 13))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_OFF ((0x0 << 4) | \
			(0x0 << 7) | \
			(0x0 << 10) | \
			(0x0 << 12))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_OFF ((0x0 << 15))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_OFF ((0x0 << 13))

bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) ==
		(unsigned int) PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) ==
		(unsigned int) PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) ==
		(unsigned int) PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON);

	return ret;
}

void dcm_peri_ao_bcrm_peri_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'peri_ao_bcrm_peri_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'peri_ao_bcrm_peri_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG1_OFF);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG2_OFF);
	}
}

#define UFS0_AO_BCRM_UFS_BUS_DCM_REG0_MASK ((0x1 << 3) | \
			(0x1 << 6) | \
			(0x1 << 8))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG1_MASK ((0x1 << 15))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG0_ON ((0x1 << 3) | \
			(0x1 << 6) | \
			(0x1 << 8))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG1_ON ((0x1 << 15))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG0_OFF ((0x0 << 3) | \
			(0x0 << 6) | \
			(0x0 << 8))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG1_OFF ((0x0 << 15))

bool dcm_ufs0_ao_bcrm_ufs_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0) &
		UFS0_AO_BCRM_UFS_BUS_DCM_REG0_MASK) ==
		(unsigned int) UFS0_AO_BCRM_UFS_BUS_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1) &
		UFS0_AO_BCRM_UFS_BUS_DCM_REG1_MASK) ==
		(unsigned int) UFS0_AO_BCRM_UFS_BUS_DCM_REG1_ON);

	return ret;
}

void dcm_ufs0_ao_bcrm_ufs_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'ufs0_ao_bcrm_ufs_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0) &
			~UFS0_AO_BCRM_UFS_BUS_DCM_REG0_MASK) |
			UFS0_AO_BCRM_UFS_BUS_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1,
			(reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1) &
			~UFS0_AO_BCRM_UFS_BUS_DCM_REG1_MASK) |
			UFS0_AO_BCRM_UFS_BUS_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'ufs0_ao_bcrm_ufs_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0) &
			~UFS0_AO_BCRM_UFS_BUS_DCM_REG0_MASK) |
			UFS0_AO_BCRM_UFS_BUS_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1,
			(reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1) &
			~UFS0_AO_BCRM_UFS_BUS_DCM_REG1_MASK) |
			UFS0_AO_BCRM_UFS_BUS_DCM_REG1_OFF);
	}
}

#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK ((0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 5) | \
			(0x1f << 14))
#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON ((0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 5) | \
			(0x0 << 14))
#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_OFF ((0x1 << 1) | \
			(0x1 << 2) | \
			(0x0 << 5) | \
			(0x0 << 14))

bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_3) &
		VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) ==
		(unsigned int) VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'vlp_ao_bcrm_vlp_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_3,
			(reg_read(
			VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_3) &
			~VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) |
			VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'vlp_ao_bcrm_vlp_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_3,
			(reg_read(
			VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_3) &
			~VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) |
			VLP_AO_BCRM_VLP_BUS_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 16))
#define MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 16))
#define MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 16))

bool dcm_mcusys_par_wrap_mcu_acp_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_acp_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_acp_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_acp_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_MASK ((0x1 << 0) | \
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
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20))
#define MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_ON ((0x1 << 0) | \
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
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20))
#define MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_OFF ((0x0 << 0) | \
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
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20))

bool dcm_mcusys_par_wrap_mcu_adb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN) &
		MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_adb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_adb_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN,
			(reg_read(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN) &
			~MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_adb_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN,
			(reg_read(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN) &
			~MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_MASK ((0xffff << 8) | \
			(0x1 << 24))
#define MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_ON ((0xffff << 8) | \
			(0x1 << 24))
#define MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_OFF ((0x0 << 8) | \
			(0x0 << 24))

bool dcm_mcusys_par_wrap_mcu_apb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_apb_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_apb_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_MASK ((0x1 << 1))
#define MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_ON ((0x1 << 1))
#define MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_OFF ((0x0 << 1))

bool dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_CI700_DCM_CTRL) &
		MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_bkr_ldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_bkr_ldcm'" */
		reg_write(MCUSYS_PAR_WRAP_CI700_DCM_CTRL,
			(reg_read(MCUSYS_PAR_WRAP_CI700_DCM_CTRL) &
			~MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_bkr_ldcm'" */
		reg_write(MCUSYS_PAR_WRAP_CI700_DCM_CTRL,
			(reg_read(MCUSYS_PAR_WRAP_CI700_DCM_CTRL) &
			~MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_MASK ((0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_MASK ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_ON ((0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_ON ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_OFF ((0x0 << 16) | \
			(0x0 << 20) | \
			(0x0 << 24))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_OFF ((0x0 << 0) | \
			(0x0 << 4) | \
			(0x0 << 8) | \
			(0x0 << 12))

bool dcm_mcusys_par_wrap_mcu_bus_qdcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
		MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG1) &
		MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_bus_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_bus_qdcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG1,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG1) &
			~MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_bus_qdcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_OFF);
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG1,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG1) &
			~MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2))

bool dcm_mcusys_par_wrap_mcu_cbip_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_cbip_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_cbip_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_cbip_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12) | \
			(0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24) | \
			(0x1 << 28))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_MASK ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12) | \
			(0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24) | \
			(0x1 << 28))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_ON ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 4) | \
			(0x0 << 8) | \
			(0x0 << 12) | \
			(0x0 << 16) | \
			(0x0 << 20) | \
			(0x0 << 24) | \
			(0x0 << 28))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_OFF ((0x0 << 0) | \
			(0x0 << 4) | \
			(0x0 << 8))

bool dcm_mcusys_par_wrap_mcu_core_qdcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG2) &
		MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG3) &
		MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_core_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_core_qdcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG2,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG2) &
			~MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG3,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG3) &
			~MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_core_qdcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG2,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG2) &
			~MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_OFF);
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG3,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG3) &
			~MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_MASK ((0xffffffff << 0))
#define MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_ON ((0xf1 << 0))
#define MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_OFF ((0xf0 << 0))

bool dcm_mcusys_par_wrap_mcu_dsu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG1) &
		MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_dsu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_dsu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG1,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG1) &
			~MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_dsu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG1,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG1) &
			~MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 12))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 12))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_ON ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 12))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_OFF ((0x0 << 0))

bool dcm_mcusys_par_wrap_mcu_io_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
		MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_io_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_io_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_io_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_OFF);
		reg_write(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_ON ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mcusys_par_wrap_mcu_misc_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG) &
		MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_misc_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_misc_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG,
			(reg_read(
			MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG) &
			~MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_misc_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG,
			(reg_read(
			MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG) &
			~MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_MASK ((0xff << 0))
#define MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_ON ((0xff << 0))
#define MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_OFF ((0x0 << 0))

bool dcm_mcusys_par_wrap_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_OFF);
	}
}
