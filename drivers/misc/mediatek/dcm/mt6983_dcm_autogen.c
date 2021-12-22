// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>

#include <mt6983_dcm_internal.h>
#include <mt6983_dcm_autogen.h>
#include <mtk_dcm.h>
/*====================auto gen code 20210629_093447=====================*/
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

#define MCUSYS_TOP_MCU_ACP_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 16))
#define MCUSYS_TOP_MCU_ACP_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 16))
#define MCUSYS_TOP_MCU_ACP_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 16))

void dcm_mcusys_top_mcu_acp_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_top_mcu_acp_dcm'" */
		reg_write(MCUSYS_TOP_MP_ADB_DCM_CFG0,
			(reg_read(MCUSYS_TOP_MP_ADB_DCM_CFG0) &
			~MCUSYS_TOP_MCU_ACP_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_ACP_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_top_mcu_acp_dcm'" */
		reg_write(MCUSYS_TOP_MP_ADB_DCM_CFG0,
			(reg_read(MCUSYS_TOP_MP_ADB_DCM_CFG0) &
			~MCUSYS_TOP_MCU_ACP_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_ACP_DCM_REG0_OFF);
	}
}

#define MCUSYS_TOP_MCU_ADB_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10))
#define MCUSYS_TOP_MCU_ADB_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10))
#define MCUSYS_TOP_MCU_ADB_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10))

void dcm_mcusys_top_mcu_adb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_top_mcu_adb_dcm'" */
		reg_write(MCUSYS_TOP_ADB_FIFO_DCM_EN,
			(reg_read(MCUSYS_TOP_ADB_FIFO_DCM_EN) &
			~MCUSYS_TOP_MCU_ADB_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_ADB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_top_mcu_adb_dcm'" */
		reg_write(MCUSYS_TOP_ADB_FIFO_DCM_EN,
			(reg_read(MCUSYS_TOP_ADB_FIFO_DCM_EN) &
			~MCUSYS_TOP_MCU_ADB_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_ADB_DCM_REG0_OFF);
	}
}

#define MCUSYS_TOP_MCU_APB_DCM_REG0_MASK ((0xffff << 8) | \
			(0x1 << 24))
#define MCUSYS_TOP_MCU_APB_DCM_REG0_ON ((0xffff << 8) | \
			(0x1 << 24))
#define MCUSYS_TOP_MCU_APB_DCM_REG0_OFF ((0x0 << 8) | \
			(0x0 << 24))

void dcm_mcusys_top_mcu_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_top_mcu_apb_dcm'" */
		reg_write(MCUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_TOP_MP0_DCM_CFG0) &
			~MCUSYS_TOP_MCU_APB_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_APB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_top_mcu_apb_dcm'" */
		reg_write(MCUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_TOP_MP0_DCM_CFG0) &
			~MCUSYS_TOP_MCU_APB_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_APB_DCM_REG0_OFF);
	}
}

#define MCUSYS_TOP_MCU_BUS_QDCM_REG0_MASK ((0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24))
#define MCUSYS_TOP_MCU_BUS_QDCM_REG1_MASK ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12))
#define MCUSYS_TOP_MCU_BUS_QDCM_REG0_ON ((0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24))
#define MCUSYS_TOP_MCU_BUS_QDCM_REG1_ON ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12))
#define MCUSYS_TOP_MCU_BUS_QDCM_REG0_OFF ((0x0 << 16) | \
			(0x0 << 20) | \
			(0x0 << 24))
#define MCUSYS_TOP_MCU_BUS_QDCM_REG1_OFF ((0x0 << 0) | \
			(0x0 << 4) | \
			(0x0 << 8) | \
			(0x0 << 12))

void dcm_mcusys_top_mcu_bus_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_top_mcu_bus_qdcm'" */
		reg_write(MCUSYS_TOP_QDCM_CONFIG0,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG0) &
			~MCUSYS_TOP_MCU_BUS_QDCM_REG0_MASK) |
			MCUSYS_TOP_MCU_BUS_QDCM_REG0_ON);
		reg_write(MCUSYS_TOP_QDCM_CONFIG1,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG1) &
			~MCUSYS_TOP_MCU_BUS_QDCM_REG1_MASK) |
			MCUSYS_TOP_MCU_BUS_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_top_mcu_bus_qdcm'" */
		reg_write(MCUSYS_TOP_QDCM_CONFIG0,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG0) &
			~MCUSYS_TOP_MCU_BUS_QDCM_REG0_MASK) |
			MCUSYS_TOP_MCU_BUS_QDCM_REG0_OFF);
		reg_write(MCUSYS_TOP_QDCM_CONFIG1,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG1) &
			~MCUSYS_TOP_MCU_BUS_QDCM_REG1_MASK) |
			MCUSYS_TOP_MCU_BUS_QDCM_REG1_OFF);
	}
}

#define MCUSYS_TOP_MCU_CBIP_DCM_REG0_MASK ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG1_MASK ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG2_MASK ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG3_MASK ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG4_MASK ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG5_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG0_ON ((0x0 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG1_ON ((0x0 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG2_ON ((0x0 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG3_ON ((0x0 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG4_ON ((0x0 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG5_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG0_OFF ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG1_OFF ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG2_OFF ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG3_OFF ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG4_OFF ((0x1 << 0))
#define MCUSYS_TOP_MCU_CBIP_DCM_REG5_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2))

void dcm_mcusys_top_mcu_cbip_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_top_mcu_cbip_dcm'" */
		reg_write(MCUSYS_TOP_CBIP_CABGEN_3TO1_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_3TO1_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG0_ON);
		reg_write(MCUSYS_TOP_CBIP_CABGEN_2TO1_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_2TO1_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG1_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG1_ON);
		reg_write(MCUSYS_TOP_CBIP_CABGEN_4TO2_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_4TO2_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG2_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG2_ON);
		reg_write(MCUSYS_TOP_CBIP_CABGEN_1TO2_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_1TO2_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG3_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG3_ON);
		reg_write(MCUSYS_TOP_CBIP_CABGEN_2TO5_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_2TO5_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG4_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG4_ON);
		reg_write(MCUSYS_TOP_CBIP_P2P_CONFIG0,
			(reg_read(MCUSYS_TOP_CBIP_P2P_CONFIG0) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG5_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG5_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_top_mcu_cbip_dcm'" */
		reg_write(MCUSYS_TOP_CBIP_CABGEN_3TO1_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_3TO1_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG0_OFF);
		reg_write(MCUSYS_TOP_CBIP_CABGEN_2TO1_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_2TO1_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG1_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG1_OFF);
		reg_write(MCUSYS_TOP_CBIP_CABGEN_4TO2_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_4TO2_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG2_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG2_OFF);
		reg_write(MCUSYS_TOP_CBIP_CABGEN_1TO2_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_1TO2_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG3_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG3_OFF);
		reg_write(MCUSYS_TOP_CBIP_CABGEN_2TO5_CONFIG,
			(reg_read(MCUSYS_TOP_CBIP_CABGEN_2TO5_CONFIG) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG4_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG4_OFF);
		reg_write(MCUSYS_TOP_CBIP_P2P_CONFIG0,
			(reg_read(MCUSYS_TOP_CBIP_P2P_CONFIG0) &
			~MCUSYS_TOP_MCU_CBIP_DCM_REG5_MASK) |
			MCUSYS_TOP_MCU_CBIP_DCM_REG5_OFF);
	}
}

#define MCUSYS_TOP_MCU_CORE_QDCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12))
#define MCUSYS_TOP_MCU_CORE_QDCM_REG1_MASK ((0x1 << 0) | \
			(0x1 << 4))
#define MCUSYS_TOP_MCU_CORE_QDCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12))
#define MCUSYS_TOP_MCU_CORE_QDCM_REG1_ON ((0x1 << 0) | \
			(0x1 << 4))
#define MCUSYS_TOP_MCU_CORE_QDCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 4) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define MCUSYS_TOP_MCU_CORE_QDCM_REG1_OFF ((0x0 << 0) | \
			(0x0 << 4))

void dcm_mcusys_top_mcu_core_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_top_mcu_core_qdcm'" */
		reg_write(MCUSYS_TOP_QDCM_CONFIG2,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG2) &
			~MCUSYS_TOP_MCU_CORE_QDCM_REG0_MASK) |
			MCUSYS_TOP_MCU_CORE_QDCM_REG0_ON);
		reg_write(MCUSYS_TOP_QDCM_CONFIG3,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG3) &
			~MCUSYS_TOP_MCU_CORE_QDCM_REG1_MASK) |
			MCUSYS_TOP_MCU_CORE_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_top_mcu_core_qdcm'" */
		reg_write(MCUSYS_TOP_QDCM_CONFIG2,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG2) &
			~MCUSYS_TOP_MCU_CORE_QDCM_REG0_MASK) |
			MCUSYS_TOP_MCU_CORE_QDCM_REG0_OFF);
		reg_write(MCUSYS_TOP_QDCM_CONFIG3,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG3) &
			~MCUSYS_TOP_MCU_CORE_QDCM_REG1_MASK) |
			MCUSYS_TOP_MCU_CORE_QDCM_REG1_OFF);
	}
}

#define MCUSYS_TOP_MCU_IO_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 12))
#define MCUSYS_TOP_MCU_IO_DCM_REG1_MASK ((0x1 << 0))
#define MCUSYS_TOP_MCU_IO_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 12))
#define MCUSYS_TOP_MCU_IO_DCM_REG1_ON ((0x1 << 0))
#define MCUSYS_TOP_MCU_IO_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 12))
#define MCUSYS_TOP_MCU_IO_DCM_REG1_OFF ((0x0 << 0))

void dcm_mcusys_top_mcu_io_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_top_mcu_io_dcm'" */
		reg_write(MCUSYS_TOP_QDCM_CONFIG0,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG0) &
			~MCUSYS_TOP_MCU_IO_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_IO_DCM_REG0_ON);
		reg_write(MCUSYS_TOP_L3GIC_ARCH_CG_CONFIG,
			(reg_read(MCUSYS_TOP_L3GIC_ARCH_CG_CONFIG) &
			~MCUSYS_TOP_MCU_IO_DCM_REG1_MASK) |
			MCUSYS_TOP_MCU_IO_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_top_mcu_io_dcm'" */
		reg_write(MCUSYS_TOP_QDCM_CONFIG0,
			(reg_read(MCUSYS_TOP_QDCM_CONFIG0) &
			~MCUSYS_TOP_MCU_IO_DCM_REG0_MASK) |
			MCUSYS_TOP_MCU_IO_DCM_REG0_OFF);
		reg_write(MCUSYS_TOP_L3GIC_ARCH_CG_CONFIG,
			(reg_read(MCUSYS_TOP_L3GIC_ARCH_CG_CONFIG) &
			~MCUSYS_TOP_MCU_IO_DCM_REG1_MASK) |
			MCUSYS_TOP_MCU_IO_DCM_REG1_OFF);
	}
}

#define MCUSYS_TOP_MCU_STALLDCM_REG0_MASK ((0xff << 0))
#define MCUSYS_TOP_MCU_STALLDCM_REG0_ON ((0xf0 << 0))
#define MCUSYS_TOP_MCU_STALLDCM_REG0_OFF ((0x0 << 0))

void dcm_mcusys_top_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_top_mcu_stalldcm'" */
		reg_write(MCUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_TOP_MP0_DCM_CFG0) &
			~MCUSYS_TOP_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_TOP_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_top_mcu_stalldcm'" */
		reg_write(MCUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_TOP_MP0_DCM_CFG0) &
			~MCUSYS_TOP_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_TOP_MCU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_CPC_CPC_PBI_DCM_REG0_MASK ((0x1 << 0))
#define MCUSYS_CPC_CPC_PBI_DCM_REG0_ON ((0x1 << 0))
#define MCUSYS_CPC_CPC_PBI_DCM_REG0_OFF ((0x0 << 0))

void dcm_mcusys_cpc_cpc_pbi_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_cpc_cpc_pbi_dcm'" */
		reg_write(MCUSYS_CPC_CPC_DCM_Enable,
			(reg_read(MCUSYS_CPC_CPC_DCM_Enable) &
			~MCUSYS_CPC_CPC_PBI_DCM_REG0_MASK) |
			MCUSYS_CPC_CPC_PBI_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_cpc_cpc_pbi_dcm'" */
		reg_write(MCUSYS_CPC_CPC_DCM_Enable,
			(reg_read(MCUSYS_CPC_CPC_DCM_Enable) &
			~MCUSYS_CPC_CPC_PBI_DCM_REG0_MASK) |
			MCUSYS_CPC_CPC_PBI_DCM_REG0_OFF);
	}
}

#define MCUSYS_CPC_CPC_TURBO_DCM_REG0_MASK ((0x1 << 1))
#define MCUSYS_CPC_CPC_TURBO_DCM_REG0_ON ((0x1 << 1))
#define MCUSYS_CPC_CPC_TURBO_DCM_REG0_OFF ((0x0 << 1))

void dcm_mcusys_cpc_cpc_turbo_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_cpc_cpc_turbo_dcm'" */
		reg_write(MCUSYS_CPC_CPC_DCM_Enable,
			(reg_read(MCUSYS_CPC_CPC_DCM_Enable) &
			~MCUSYS_CPC_CPC_TURBO_DCM_REG0_MASK) |
			MCUSYS_CPC_CPC_TURBO_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_cpc_cpc_turbo_dcm'" */
		reg_write(MCUSYS_CPC_CPC_DCM_Enable,
			(reg_read(MCUSYS_CPC_CPC_DCM_Enable) &
			~MCUSYS_CPC_CPC_TURBO_DCM_REG0_MASK) |
			MCUSYS_CPC_CPC_TURBO_DCM_REG0_OFF);
	}
}

#define MP_CPU4_TOP_MCU_APB_DCM_REG0_MASK ((0x1 << 4))
#define MP_CPU4_TOP_MCU_APB_DCM_REG0_ON ((0x1 << 4))
#define MP_CPU4_TOP_MCU_APB_DCM_REG0_OFF ((0x0 << 4))

void dcm_mp_cpu4_top_mcu_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpu4_top_mcu_apb_dcm'" */
		reg_write(MP_CPU4_TOP_PTP3_CPU_PCSM_SW_PCHANNEL,
			(reg_read(MP_CPU4_TOP_PTP3_CPU_PCSM_SW_PCHANNEL) &
			~MP_CPU4_TOP_MCU_APB_DCM_REG0_MASK) |
			MP_CPU4_TOP_MCU_APB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpu4_top_mcu_apb_dcm'" */
		reg_write(MP_CPU4_TOP_PTP3_CPU_PCSM_SW_PCHANNEL,
			(reg_read(MP_CPU4_TOP_PTP3_CPU_PCSM_SW_PCHANNEL) &
			~MP_CPU4_TOP_MCU_APB_DCM_REG0_MASK) |
			MP_CPU4_TOP_MCU_APB_DCM_REG0_OFF);
	}
}

#define MP_CPU4_TOP_MCU_STALLDCM_REG0_MASK ((0x1 << 0))
#define MP_CPU4_TOP_MCU_STALLDCM_REG0_ON ((0x1 << 0))
#define MP_CPU4_TOP_MCU_STALLDCM_REG0_OFF ((0x0 << 0))

void dcm_mp_cpu4_top_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpu4_top_mcu_stalldcm'" */
		reg_write(MP_CPU4_TOP_STALL_DCM_CONF,
			(reg_read(MP_CPU4_TOP_STALL_DCM_CONF) &
			~MP_CPU4_TOP_MCU_STALLDCM_REG0_MASK) |
			MP_CPU4_TOP_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpu4_top_mcu_stalldcm'" */
		reg_write(MP_CPU4_TOP_STALL_DCM_CONF,
			(reg_read(MP_CPU4_TOP_STALL_DCM_CONF) &
			~MP_CPU4_TOP_MCU_STALLDCM_REG0_MASK) |
			MP_CPU4_TOP_MCU_STALLDCM_REG0_OFF);
	}
}

#define MP_CPU5_TOP_MCU_APB_DCM_REG0_MASK ((0x1 << 4))
#define MP_CPU5_TOP_MCU_APB_DCM_REG0_ON ((0x1 << 4))
#define MP_CPU5_TOP_MCU_APB_DCM_REG0_OFF ((0x0 << 4))

void dcm_mp_cpu5_top_mcu_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpu5_top_mcu_apb_dcm'" */
		reg_write(MP_CPU5_TOP_PTP3_CPU_PCSM_SW_PCHANNEL,
			(reg_read(MP_CPU5_TOP_PTP3_CPU_PCSM_SW_PCHANNEL) &
			~MP_CPU5_TOP_MCU_APB_DCM_REG0_MASK) |
			MP_CPU5_TOP_MCU_APB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpu5_top_mcu_apb_dcm'" */
		reg_write(MP_CPU5_TOP_PTP3_CPU_PCSM_SW_PCHANNEL,
			(reg_read(MP_CPU5_TOP_PTP3_CPU_PCSM_SW_PCHANNEL) &
			~MP_CPU5_TOP_MCU_APB_DCM_REG0_MASK) |
			MP_CPU5_TOP_MCU_APB_DCM_REG0_OFF);
	}
}

#define MP_CPU5_TOP_MCU_STALLDCM_REG0_MASK ((0x1 << 0))
#define MP_CPU5_TOP_MCU_STALLDCM_REG0_ON ((0x1 << 0))
#define MP_CPU5_TOP_MCU_STALLDCM_REG0_OFF ((0x0 << 0))

void dcm_mp_cpu5_top_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpu5_top_mcu_stalldcm'" */
		reg_write(MP_CPU5_TOP_STALL_DCM_CONF,
			(reg_read(MP_CPU5_TOP_STALL_DCM_CONF) &
			~MP_CPU5_TOP_MCU_STALLDCM_REG0_MASK) |
			MP_CPU5_TOP_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpu5_top_mcu_stalldcm'" */
		reg_write(MP_CPU5_TOP_STALL_DCM_CONF,
			(reg_read(MP_CPU5_TOP_STALL_DCM_CONF) &
			~MP_CPU5_TOP_MCU_STALLDCM_REG0_MASK) |
			MP_CPU5_TOP_MCU_STALLDCM_REG0_OFF);
	}
}

#define MP_CPU6_TOP_MCU_APB_DCM_REG0_MASK ((0x1 << 4))
#define MP_CPU6_TOP_MCU_APB_DCM_REG0_ON ((0x1 << 4))
#define MP_CPU6_TOP_MCU_APB_DCM_REG0_OFF ((0x0 << 4))

void dcm_mp_cpu6_top_mcu_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpu6_top_mcu_apb_dcm'" */
		reg_write(MP_CPU6_TOP_PTP3_CPU_PCSM_SW_PCHANNEL,
			(reg_read(MP_CPU6_TOP_PTP3_CPU_PCSM_SW_PCHANNEL) &
			~MP_CPU6_TOP_MCU_APB_DCM_REG0_MASK) |
			MP_CPU6_TOP_MCU_APB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpu6_top_mcu_apb_dcm'" */
		reg_write(MP_CPU6_TOP_PTP3_CPU_PCSM_SW_PCHANNEL,
			(reg_read(MP_CPU6_TOP_PTP3_CPU_PCSM_SW_PCHANNEL) &
			~MP_CPU6_TOP_MCU_APB_DCM_REG0_MASK) |
			MP_CPU6_TOP_MCU_APB_DCM_REG0_OFF);
	}
}

#define MP_CPU6_TOP_MCU_STALLDCM_REG0_MASK ((0x1 << 0))
#define MP_CPU6_TOP_MCU_STALLDCM_REG0_ON ((0x1 << 0))
#define MP_CPU6_TOP_MCU_STALLDCM_REG0_OFF ((0x0 << 0))

void dcm_mp_cpu6_top_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpu6_top_mcu_stalldcm'" */
		reg_write(MP_CPU6_TOP_STALL_DCM_CONF,
			(reg_read(MP_CPU6_TOP_STALL_DCM_CONF) &
			~MP_CPU6_TOP_MCU_STALLDCM_REG0_MASK) |
			MP_CPU6_TOP_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpu6_top_mcu_stalldcm'" */
		reg_write(MP_CPU6_TOP_STALL_DCM_CONF,
			(reg_read(MP_CPU6_TOP_STALL_DCM_CONF) &
			~MP_CPU6_TOP_MCU_STALLDCM_REG0_MASK) |
			MP_CPU6_TOP_MCU_STALLDCM_REG0_OFF);
	}
}

#define MP_CPU7_TOP_MCU_APB_DCM_REG0_MASK ((0x1 << 4))
#define MP_CPU7_TOP_MCU_APB_DCM_REG0_ON ((0x1 << 4))
#define MP_CPU7_TOP_MCU_APB_DCM_REG0_OFF ((0x0 << 4))

void dcm_mp_cpu7_top_mcu_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpu7_top_mcu_apb_dcm'" */
		reg_write(MP_CPU7_TOP_PTP3_CPU_PCSM_SW_PCHANNEL,
			(reg_read(MP_CPU7_TOP_PTP3_CPU_PCSM_SW_PCHANNEL) &
			~MP_CPU7_TOP_MCU_APB_DCM_REG0_MASK) |
			MP_CPU7_TOP_MCU_APB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpu7_top_mcu_apb_dcm'" */
		reg_write(MP_CPU7_TOP_PTP3_CPU_PCSM_SW_PCHANNEL,
			(reg_read(MP_CPU7_TOP_PTP3_CPU_PCSM_SW_PCHANNEL) &
			~MP_CPU7_TOP_MCU_APB_DCM_REG0_MASK) |
			MP_CPU7_TOP_MCU_APB_DCM_REG0_OFF);
	}
}

#define MP_CPU7_TOP_MCU_STALLDCM_REG0_MASK ((0x1 << 0))
#define MP_CPU7_TOP_MCU_STALLDCM_REG0_ON ((0x1 << 0))
#define MP_CPU7_TOP_MCU_STALLDCM_REG0_OFF ((0x0 << 0))

void dcm_mp_cpu7_top_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpu7_top_mcu_stalldcm'" */
		reg_write(MP_CPU7_TOP_STALL_DCM_CONF,
			(reg_read(MP_CPU7_TOP_STALL_DCM_CONF) &
			~MP_CPU7_TOP_MCU_STALLDCM_REG0_MASK) |
			MP_CPU7_TOP_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpu7_top_mcu_stalldcm'" */
		reg_write(MP_CPU7_TOP_STALL_DCM_CONF,
			(reg_read(MP_CPU7_TOP_STALL_DCM_CONF) &
			~MP_CPU7_TOP_MCU_STALLDCM_REG0_MASK) |
			MP_CPU7_TOP_MCU_STALLDCM_REG0_OFF);
	}
}
