// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/io.h>

#include <mt6895_dcm_internal.h>
#include <mt6895_dcm_autogen.h>
#include <mtk_dcm.h>
/*====================auto gen code 20210906_164512=====================*/
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK ((0x1f << 12) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON ((0x10 << 12) | \
			(0x1 << 17) | \
			(0x0 << 18))
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_OFF ((0x10 << 12) | \
			(0x0 << 17) | \
			(0x1 << 18))

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

#define INFRACFG_AO_MTS_BUS_DCM_REG0_MASK ((0x1 << 31))
#define INFRACFG_AO_MTS_BUS_DCM_REG0_ON ((0x1 << 31))
#define INFRACFG_AO_MTS_BUS_DCM_REG0_OFF ((0x0 << 31))

void dcm_infracfg_ao_mts_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_mts_bus_dcm'" */
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_MTS_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_MTS_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_mts_bus_dcm'" */
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_MTS_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_MTS_BUS_DCM_REG0_OFF);
	}
}

#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 7) | \
			(0x1f << 16))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_MASK ((0xfff << 20))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG2_MASK ((0x1 << 0))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 7) | \
			(0x0 << 16))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_ON ((0x10 << 20))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG2_ON ((0x1 << 0))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_OFF ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 7) | \
			(0x0 << 16))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG1_OFF ((0x10 << 20))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG2_OFF ((0x0 << 0))

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
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG2_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG2_ON);
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
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG2_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG2_OFF);
	}
}

#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK ((0x1 << 3) | \
			(0x1 << 4))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK ((0x1 << 5) | \
			(0x1f << 15))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_MASK ((0xfff << 1) | \
			(0x1 << 13))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_ON ((0x1 << 3) | \
			(0x1 << 4))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_ON ((0x1 << 5) | \
			(0x0 << 15))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_ON ((0x10 << 1) | \
			(0x1 << 13))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_OFF ((0x1 << 3) | \
			(0x1 << 4))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_OFF ((0x0 << 5) | \
			(0x0 << 15))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_OFF ((0x10 << 1) | \
			(0x0 << 13))

void dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(int on)
{
	if (on) {
			/* TINFO = "Turn ON DCM 'infra_ao_bcrm_infra_bus_fmem_sub_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_ON);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_ON);
	} else {
			/* TINFO = "Turn OFF DCM 'infra_ao_bcrm_infra_bus_fmem_sub_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_OFF);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG2_OFF);
	}
}

#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK ((0x1 << 1) | \
			(0x1 << 4) | \
			(0x1 << 10) | \
			(0x1 << 12) | \
			(0x1 << 27))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK ((0x1 << 10) | \
			(0x1 << 25))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON ((0x1 << 1) | \
			(0x1 << 4) | \
			(0x1 << 10) | \
			(0x1 << 12) | \
			(0x1 << 27))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON ((0x1 << 10) | \
			(0x1 << 25))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_OFF ((0x0 << 1) | \
			(0x0 << 4) | \
			(0x0 << 10) | \
			(0x0 << 12) | \
			(0x0 << 27))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_OFF ((0x0 << 10) | \
			(0x0 << 25))

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
	}
}

#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 4) | \
			(0x1f << 13))
#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 4) | \
			(0x0 << 13))
#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_OFF ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 4) | \
			(0x0 << 13))

void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'vlp_ao_bcrm_vlp_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0) &
			~VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) |
			VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'vlp_ao_bcrm_vlp_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0) &
			~VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) |
			VLP_AO_BCRM_VLP_BUS_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_ADB_DCM_REG0_MASK ((0x1 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_MASK ((0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_MASK ((0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define MP_CPUSYS_TOP_ADB_DCM_REG0_ON ((0x1 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_ON ((0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_ON ((0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define MP_CPUSYS_TOP_ADB_DCM_REG0_OFF ((0x0 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_OFF ((0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_OFF ((0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18))

void dcm_mp_cpusys_top_adb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_adb_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG0_ON);
		reg_write(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4,
			(reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4) &
			~MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG1_ON);
		reg_write(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_adb_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG0_OFF);
		reg_write(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4,
			(reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4) &
			~MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG1_OFF);
		reg_write(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG2_OFF);
	}
}

#define MP_CPUSYS_TOP_APB_DCM_REG0_MASK ((0x1 << 5) | \
			(0x1 << 6))
#define MP_CPUSYS_TOP_APB_DCM_REG1_MASK ((0x1 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_MASK ((0x1 << 16))
#define MP_CPUSYS_TOP_APB_DCM_REG0_ON ((0x1 << 5) | \
			(0x1 << 6))
#define MP_CPUSYS_TOP_APB_DCM_REG1_ON ((0x1 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_ON ((0x1 << 16))
#define MP_CPUSYS_TOP_APB_DCM_REG0_OFF ((0x0 << 5) | \
			(0x0 << 6))
#define MP_CPUSYS_TOP_APB_DCM_REG1_OFF ((0x0 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_OFF ((0x0 << 16))

void dcm_mp_cpusys_top_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_apb_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG0_ON);
		reg_write(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG1_ON);
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_apb_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG0_OFF);
		reg_write(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG1_OFF);
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
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

void dcm_mp_cpusys_top_bus_pll_div_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_bus_pll_div_dcm'" */
		reg_write(MP_CPUSYS_TOP_BUS_PLLDIV_CFG,
			(reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_bus_pll_div_dcm'" */
		reg_write(MP_CPUSYS_TOP_BUS_PLLDIV_CFG,
			(reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK ((0x1 << 0))
#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_ON ((0x1 << 0))
#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_OFF ((0x0 << 0))

void dcm_mp_cpusys_top_core_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_core_stall_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG7,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_core_stall_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG7,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK ((0xffff << 0))
#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_ON ((0xffff << 0))
#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_OFF ((0x0 << 0))

void dcm_mp_cpusys_top_cpubiu_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpubiu_dcm'" */
		reg_write(MP_CPUSYS_TOP_MCSIC_DCM0,
			(reg_read(MP_CPUSYS_TOP_MCSIC_DCM0) &
			~MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPUBIU_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpubiu_dcm'" */
		reg_write(MP_CPUSYS_TOP_MCSIC_DCM0,
			(reg_read(MP_CPUSYS_TOP_MCSIC_DCM0) &
			~MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPUBIU_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK ((0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_ON ((0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_OFF ((0x0 << 24) | \
			(0x0 << 25))

void dcm_mp_cpusys_top_cpu_pll_div_0_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpu_pll_div_0_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpu_pll_div_0_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK ((0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_ON ((0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_OFF ((0x0 << 24) | \
			(0x0 << 25))

void dcm_mp_cpusys_top_cpu_pll_div_1_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpu_pll_div_1_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpu_pll_div_1_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_MASK ((0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_ON ((0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_OFF ((0x0 << 24) | \
			(0x0 << 25))

void dcm_mp_cpusys_top_cpu_pll_div_2_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpu_pll_div_2_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG2,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG2) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpu_pll_div_2_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG2,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG2) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_2_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK ((0x1 << 4))
#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_ON ((0x1 << 4))
#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_OFF ((0x0 << 4))

void dcm_mp_cpusys_top_fcm_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_fcm_stall_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG7,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_fcm_stall_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG7,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK ((0x1 << 31))
#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_ON ((0x1 << 31))
#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_OFF ((0x0 << 31))

void dcm_mp_cpusys_top_last_cor_idle_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_last_cor_idle_dcm'" */
		reg_write(MP_CPUSYS_TOP_BUS_PLLDIV_CFG,
			(reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_last_cor_idle_dcm'" */
		reg_write(MP_CPUSYS_TOP_BUS_PLLDIV_CFG,
			(reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_MISC_DCM_REG0_MASK ((0x1 << 1) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_MISC_DCM_REG0_ON ((0x1 << 1) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_MISC_DCM_REG0_OFF ((0x0 << 1) | \
			(0x0 << 4))

void dcm_mp_cpusys_top_misc_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_misc_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MISC_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_MISC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_misc_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
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

void dcm_mp_cpusys_top_mp0_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_mp0_qdcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG0_ON);
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_mp0_qdcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG0_OFF);
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
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

void dcm_cpccfg_reg_emi_wfifo(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'cpccfg_reg_emi_wfifo'" */
		reg_write(CPCCFG_REG_EMI_WFIFO,
			(reg_read(CPCCFG_REG_EMI_WFIFO) &
			~CPCCFG_REG_EMI_WFIFO_REG0_MASK) |
			CPCCFG_REG_EMI_WFIFO_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'cpccfg_reg_emi_wfifo'" */
		reg_write(CPCCFG_REG_EMI_WFIFO,
			(reg_read(CPCCFG_REG_EMI_WFIFO) &
			~CPCCFG_REG_EMI_WFIFO_REG0_MASK) |
			CPCCFG_REG_EMI_WFIFO_REG0_OFF);
	}
}
