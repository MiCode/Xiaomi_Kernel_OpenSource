/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>
/* #include <mt-plat/mtk_secure_api.h> */

#include <mtk_dcm_internal.h>
#include <mtk_dcm_autogen.h>
#include <mtk_dcm.h>

/* Below from DCM autogen. */
#define INFRACFG_AO_AUDIO_REG0_MASK ((0x1 << 29))
#define INFRACFG_AO_AUDIO_REG0_ON ((0x1 << 29))
#define INFRACFG_AO_AUDIO_REG0_OFF ((0x0 << 29))

bool dcm_infracfg_ao_audio_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_AUDIO_REG0_MASK &
		INFRACFG_AO_AUDIO_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_audio(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_audio'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_AUDIO_REG0_MASK) |
			INFRACFG_AO_AUDIO_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_audio'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_AUDIO_REG0_MASK) |
			INFRACFG_AO_AUDIO_REG0_OFF);
	}
}

#define INFRACFG_AO_DFS_MEM_REG0_MASK ((0x1 << 0) | \
			(0x1f << 1) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1f << 16) | \
			(0x1f << 21) | \
			(0x1 << 26))
#define INFRACFG_AO_DFS_MEM_REG0_ON ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x0 << 16) | \
			(0x1f << 21) | \
			(0x0 << 26))
#define INFRACFG_AO_DFS_MEM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x0 << 16) | \
			(0x1f << 21) | \
			(0x0 << 26))

bool dcm_infracfg_ao_dfs_mem_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(DFS_MEM_DCM_CTRL) &
		INFRACFG_AO_DFS_MEM_REG0_MASK &
		INFRACFG_AO_DFS_MEM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_dfs_mem(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_dfs_mem'" */
		reg_write(DFS_MEM_DCM_CTRL,
			(reg_read(DFS_MEM_DCM_CTRL) &
			~INFRACFG_AO_DFS_MEM_REG0_MASK) |
			INFRACFG_AO_DFS_MEM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_dfs_mem'" */
		reg_write(DFS_MEM_DCM_CTRL,
			(reg_read(DFS_MEM_DCM_CTRL) &
			~INFRACFG_AO_DFS_MEM_REG0_MASK) |
			INFRACFG_AO_DFS_MEM_REG0_OFF);
	}
}

#define INFRACFG_AO_ICUSB_REG0_MASK ((0x1 << 28))
#define INFRACFG_AO_ICUSB_REG0_ON ((0x1 << 28))
#define INFRACFG_AO_ICUSB_REG0_OFF ((0x0 << 28))

bool dcm_infracfg_ao_icusb_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_ICUSB_REG0_MASK &
		INFRACFG_AO_ICUSB_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_icusb(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_icusb'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_ICUSB_REG0_MASK) |
			INFRACFG_AO_ICUSB_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_icusb'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_ICUSB_REG0_MASK) |
			INFRACFG_AO_ICUSB_REG0_OFF);
	}
}

#define INFRACFG_AO_INFRA_MD_REG0_MASK ((0x1 << 28))
#define INFRACFG_AO_INFRA_MD_REG0_ON ((0x1 << 28))
#define INFRACFG_AO_INFRA_MD_REG0_OFF ((0x0 << 28))

bool dcm_infracfg_ao_infra_md_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(INFRA_MISC) &
		INFRACFG_AO_INFRA_MD_REG0_MASK &
		INFRACFG_AO_INFRA_MD_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_infra_md(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_md'" */
		reg_write(INFRA_MISC,
			(reg_read(INFRA_MISC) &
			~INFRACFG_AO_INFRA_MD_REG0_MASK) |
			INFRACFG_AO_INFRA_MD_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_md'" */
		reg_write(INFRA_MISC,
			(reg_read(INFRA_MISC) &
			~INFRACFG_AO_INFRA_MD_REG0_MASK) |
			INFRACFG_AO_INFRA_MD_REG0_OFF);
	}
}

/*
 * Align mt6739 and mt6763, only control bit 27.
 */
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

	ret &= !!(reg_read(MEM_DCM_CTRL) &
		INFRACFG_AO_INFRA_MEM_REG0_MASK &
		INFRACFG_AO_INFRA_MEM_REG0_ON);

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

#define INFRACFG_AO_INFRA_PERI_REG0_MASK ((0x1 << 0) | \
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
#define INFRACFG_AO_INFRA_PERI_REG1_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x1f << 10) | \
			(0x1f << 15) | \
			(0x1 << 20))
#define INFRACFG_AO_INFRA_PERI_REG0_ON ((0x1 << 0) | \
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
#define INFRACFG_AO_INFRA_PERI_REG1_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x1f << 5) | \
			(0x0 << 10) | \
			(0x1f << 15) | \
			(0x1 << 20))
#define INFRACFG_AO_INFRA_PERI_REG0_OFF ((0x0 << 0) | \
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
#define INFRACFG_AO_INFRA_PERI_REG1_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x1f << 5) | \
			(0x1f << 10) | \
			(0x0 << 15) | \
			(0x0 << 20))

bool dcm_infracfg_ao_infra_peri_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(INFRA_BUS_DCM_CTRL) &
		INFRACFG_AO_INFRA_PERI_REG0_MASK &
		INFRACFG_AO_INFRA_PERI_REG0_ON);
	ret &= !!(reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_INFRA_PERI_REG1_MASK &
		INFRACFG_AO_INFRA_PERI_REG1_ON);

	return ret;
}

void dcm_infracfg_ao_infra_peri(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_peri'" */
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_PERI_REG0_MASK) |
			INFRACFG_AO_INFRA_PERI_REG0_ON);
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_PERI_REG1_MASK) |
			INFRACFG_AO_INFRA_PERI_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_peri'" */
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_PERI_REG0_MASK) |
			INFRACFG_AO_INFRA_PERI_REG0_OFF);
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_PERI_REG1_MASK) |
			INFRACFG_AO_INFRA_PERI_REG1_OFF);
	}
}

#define INFRACFG_AO_P2P_DSI_CSI_REG0_MASK ((0xf << 0))
#define INFRACFG_AO_P2P_DSI_CSI_REG0_ON ((0x0 << 0))
/* TODO: Check why autogen is wrong. */
#define INFRACFG_AO_P2P_DSI_CSI_REG0_OFF ((0xf << 0))

bool dcm_infracfg_ao_p2p_dsi_csi_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(P2P_RX_CLK_ON) &
		INFRACFG_AO_P2P_DSI_CSI_REG0_MASK &
		INFRACFG_AO_P2P_DSI_CSI_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_p2p_dsi_csi(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_p2p_dsi_csi'" */
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_P2P_DSI_CSI_REG0_MASK) |
			INFRACFG_AO_P2P_DSI_CSI_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_p2p_dsi_csi'" */
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_P2P_DSI_CSI_REG0_MASK) |
			INFRACFG_AO_P2P_DSI_CSI_REG0_OFF);
	}
}

#define INFRACFG_AO_PMIC_REG0_MASK ((0x1 << 22) | \
			(0x1f << 23))
#define INFRACFG_AO_PMIC_REG0_ON ((0x1 << 22) | \
			(0x0 << 23))
#define INFRACFG_AO_PMIC_REG0_OFF ((0x0 << 22) | \
			(0x0 << 23))

bool dcm_infracfg_ao_pmic_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_PMIC_REG0_MASK &
		INFRACFG_AO_PMIC_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_pmic(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_pmic'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PMIC_REG0_MASK) |
			INFRACFG_AO_PMIC_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_pmic'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PMIC_REG0_MASK) |
			INFRACFG_AO_PMIC_REG0_OFF);
	}
}

#define INFRACFG_AO_SSUSB_REG0_MASK ((0x1 << 31))
#define INFRACFG_AO_SSUSB_REG0_ON ((0x1 << 31))
#define INFRACFG_AO_SSUSB_REG0_OFF ((0x0 << 31))

bool dcm_infracfg_ao_ssusb_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_SSUSB_REG0_MASK &
		INFRACFG_AO_SSUSB_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_ssusb(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_ssusb'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_SSUSB_REG0_MASK) |
			INFRACFG_AO_SSUSB_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_ssusb'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_SSUSB_REG0_MASK) |
			INFRACFG_AO_SSUSB_REG0_OFF);
	}
}

#define INFRACFG_AO_USB_REG0_MASK ((0x1 << 21))
#define INFRACFG_AO_USB_REG0_ON ((0x1 << 21))
#define INFRACFG_AO_USB_REG0_OFF ((0x0 << 21))

bool dcm_infracfg_ao_usb_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_USB_REG0_MASK &
		INFRACFG_AO_USB_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_usb(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_usb'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_USB_REG0_MASK) |
			INFRACFG_AO_USB_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_usb'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_USB_REG0_MASK) |
			INFRACFG_AO_USB_REG0_OFF);
	}
}

#if 0
#define SECURITY_AO_INFRA_DXCC_REG0_MASK ((0xffff << 0))
#define SECURITY_AO_INFRA_DXCC_REG0_ON ((0x0 << 0))
#define SECURITY_AO_INFRA_DXCC_REG0_OFF ((0xffff << 0))

bool dcm_security_ao_infra_dxcc_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(DXCC_NEW_HWDCM_CFG) &
		SECURITY_AO_INFRA_DXCC_REG0_MASK &
		SECURITY_AO_INFRA_DXCC_REG0_ON);

	return ret;
}

void dcm_security_ao_infra_dxcc(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'security_ao_infra_dxcc'" */
		reg_write(DXCC_NEW_HWDCM_CFG,
			(reg_read(DXCC_NEW_HWDCM_CFG) &
			~SECURITY_AO_INFRA_DXCC_REG0_MASK) |
			SECURITY_AO_INFRA_DXCC_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'security_ao_infra_dxcc'" */
		reg_write(DXCC_NEW_HWDCM_CFG,
			(reg_read(DXCC_NEW_HWDCM_CFG) &
			~SECURITY_AO_INFRA_DXCC_REG0_MASK) |
			SECURITY_AO_INFRA_DXCC_REG0_OFF);
	}
}
#endif

#define MCUCFG_MCSI_DCM_REG0_MASK ((0xffff << 16))
#define MCUCFG_MCSI_DCM_REG0_ON ((0xffff << 16))
#define MCUCFG_MCSI_DCM_REG0_OFF ((0x0 << 16))

bool dcm_mcucfg_mcsi_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MCSIA_DCM_EN) &
		MCUCFG_MCSI_DCM_REG0_MASK &
		MCUCFG_MCSI_DCM_REG0_ON);

	return ret;
}

void dcm_mcucfg_mcsi_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcucfg_mcsi_dcm'" */
		reg_write(MCSIA_DCM_EN,
			(reg_read(MCSIA_DCM_EN) &
			~MCUCFG_MCSI_DCM_REG0_MASK) |
			MCUCFG_MCSI_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcucfg_mcsi_dcm'" */
		reg_write(MCSIA_DCM_EN,
			(reg_read(MCSIA_DCM_EN) &
			~MCUCFG_MCSI_DCM_REG0_MASK) |
			MCUCFG_MCSI_DCM_REG0_OFF);
	}
}

#define MP0_CPUCFG_MP0_RGU_DCM_REG0_MASK ((0x1 << 0))
#define MP0_CPUCFG_MP0_RGU_DCM_REG0_ON ((0x1 << 0))
#define MP0_CPUCFG_MP0_RGU_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mp0_cpucfg_mp0_rgu_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MP0_CPUCFG_MP0_RGU_DCM_CONFIG) &
		MP0_CPUCFG_MP0_RGU_DCM_REG0_MASK &
		MP0_CPUCFG_MP0_RGU_DCM_REG0_ON);

	return ret;
}

void dcm_mp0_cpucfg_mp0_rgu_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp0_cpucfg_mp0_rgu_dcm'" */
		reg_write(MP0_CPUCFG_MP0_RGU_DCM_CONFIG,
			(reg_read(MP0_CPUCFG_MP0_RGU_DCM_CONFIG) &
			~MP0_CPUCFG_MP0_RGU_DCM_REG0_MASK) |
			MP0_CPUCFG_MP0_RGU_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp0_cpucfg_mp0_rgu_dcm'" */
		reg_write(MP0_CPUCFG_MP0_RGU_DCM_CONFIG,
			(reg_read(MP0_CPUCFG_MP0_RGU_DCM_CONFIG) &
			~MP0_CPUCFG_MP0_RGU_DCM_REG0_MASK) |
			MP0_CPUCFG_MP0_RGU_DCM_REG0_OFF);
	}
}

#define MP1_CPUCFG_MP1_RGU_DCM_REG0_MASK ((0x1 << 0))
#define MP1_CPUCFG_MP1_RGU_DCM_REG0_ON ((0x1 << 0))
#define MP1_CPUCFG_MP1_RGU_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mp1_cpucfg_mp1_rgu_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MP1_CPUCFG_MP1_RGU_DCM_CONFIG) &
		MP1_CPUCFG_MP1_RGU_DCM_REG0_MASK &
		MP1_CPUCFG_MP1_RGU_DCM_REG0_ON);

	return ret;
}

void dcm_mp1_cpucfg_mp1_rgu_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp1_cpucfg_mp1_rgu_dcm'" */
		reg_write(MP1_CPUCFG_MP1_RGU_DCM_CONFIG,
			(reg_read(MP1_CPUCFG_MP1_RGU_DCM_CONFIG) &
			~MP1_CPUCFG_MP1_RGU_DCM_REG0_MASK) |
			MP1_CPUCFG_MP1_RGU_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp1_cpucfg_mp1_rgu_dcm'" */
		reg_write(MP1_CPUCFG_MP1_RGU_DCM_CONFIG,
			(reg_read(MP1_CPUCFG_MP1_RGU_DCM_CONFIG) &
			~MP1_CPUCFG_MP1_RGU_DCM_REG0_MASK) |
			MP1_CPUCFG_MP1_RGU_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_ADB400_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 11))
#define MCU_MISCCFG_ADB400_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 11))
#define MCU_MISCCFG_ADB400_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 11))

bool dcm_mcu_misccfg_adb400_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(CCI_ADB400_DCM_CONFIG) &
		MCU_MISCCFG_ADB400_DCM_REG0_MASK &
		MCU_MISCCFG_ADB400_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_adb400_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcu_misccfg_adb400_dcm'" */
		reg_write(CCI_ADB400_DCM_CONFIG,
			(reg_read(CCI_ADB400_DCM_CONFIG) &
			~MCU_MISCCFG_ADB400_DCM_REG0_MASK) |
			MCU_MISCCFG_ADB400_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcu_misccfg_adb400_dcm'" */
		reg_write(CCI_ADB400_DCM_CONFIG,
			(reg_read(CCI_ADB400_DCM_CONFIG) &
			~MCU_MISCCFG_ADB400_DCM_REG0_MASK) |
			MCU_MISCCFG_ADB400_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_MASK ((0x1 << 11))
#define MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_ON ((0x1 << 11))
#define MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_OFF ((0x0 << 11))

bool dcm_mcu_misccfg_bus_arm_pll_divider_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(BUS_PLL_DIVIDER_CFG) &
		MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_MASK &
		MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_bus_arm_pll_divider_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_bus_arm_pll_divider_dcm'"
		 */
		reg_write(BUS_PLL_DIVIDER_CFG,
			(reg_read(BUS_PLL_DIVIDER_CFG) &
			~MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_bus_arm_pll_divider_dcm'"
		 */
		reg_write(BUS_PLL_DIVIDER_CFG,
			(reg_read(BUS_PLL_DIVIDER_CFG) &
			~MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_BUS_SYNC_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x7f << 2))
#define MCU_MISCCFG_BUS_SYNC_DCM_REG0_ON ((0x1 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2))
#define MCU_MISCCFG_BUS_SYNC_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2))

bool dcm_mcu_misccfg_bus_sync_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SYNC_DCM_CONFIG) &
		MCU_MISCCFG_BUS_SYNC_DCM_REG0_MASK &
		MCU_MISCCFG_BUS_SYNC_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_bus_sync_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_bus_sync_dcm'"
		 */
		reg_write(SYNC_DCM_CONFIG,
			(reg_read(SYNC_DCM_CONFIG) &
			~MCU_MISCCFG_BUS_SYNC_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_SYNC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_bus_sync_dcm'"
		 */
		reg_write(SYNC_DCM_CONFIG,
			(reg_read(SYNC_DCM_CONFIG) &
			~MCU_MISCCFG_BUS_SYNC_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_SYNC_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_BUS_CLOCK_DCM_REG0_MASK ((0x1 << 8))
#define MCU_MISCCFG_BUS_CLOCK_DCM_REG0_ON ((0x1 << 8))
#define MCU_MISCCFG_BUS_CLOCK_DCM_REG0_OFF ((0x0 << 8))

bool dcm_mcu_misccfg_bus_clock_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(CCI_CLK_CTRL) &
		MCU_MISCCFG_BUS_CLOCK_DCM_REG0_MASK &
		MCU_MISCCFG_BUS_CLOCK_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_bus_clock_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_bus_clock_dcm'"
		 */
		reg_write(CCI_CLK_CTRL,
			(reg_read(CCI_CLK_CTRL) &
			~MCU_MISCCFG_BUS_CLOCK_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_CLOCK_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_bus_clock_dcm'"
		 */
		reg_write(CCI_CLK_CTRL,
			(reg_read(CCI_CLK_CTRL) &
			~MCU_MISCCFG_BUS_CLOCK_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_CLOCK_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_BUS_FABRIC_DCM_REG0_MASK ((0x1 << 0) | \
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
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1 << 23) | \
			(0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 26) | \
			(0x1 << 27) | \
			(0x1 << 28) | \
			(0x1 << 29))
#define MCU_MISCCFG_BUS_FABRIC_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 5) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1 << 23) | \
			(0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 26) | \
			(0x1 << 27) | \
			(0x1 << 28) | \
			(0x1 << 29))
#define MCU_MISCCFG_BUS_FABRIC_DCM_REG0_OFF ((0x0 << 0) | \
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
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22) | \
			(0x0 << 23) | \
			(0x0 << 24) | \
			(0x0 << 25) | \
			(0x0 << 26) | \
			(0x0 << 27) | \
			(0x0 << 28) | \
			(0x0 << 29))

bool dcm_mcu_misccfg_bus_fabric_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(BUS_FABRIC_DCM_CTRL) &
		MCU_MISCCFG_BUS_FABRIC_DCM_REG0_MASK &
		MCU_MISCCFG_BUS_FABRIC_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_bus_fabric_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_bus_fabric_dcm'"
		 */
		reg_write(BUS_FABRIC_DCM_CTRL,
			(reg_read(BUS_FABRIC_DCM_CTRL) &
			~MCU_MISCCFG_BUS_FABRIC_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_FABRIC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_bus_fabric_dcm'"
		 */
		reg_write(BUS_FABRIC_DCM_CTRL,
			(reg_read(BUS_FABRIC_DCM_CTRL) &
			~MCU_MISCCFG_BUS_FABRIC_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_FABRIC_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_GIC_SYNC_DCM_REG0_MASK ((0x1 << 0))
#define MCU_MISCCFG_GIC_SYNC_DCM_REG0_ON ((0x1 << 0))
#define MCU_MISCCFG_GIC_SYNC_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mcu_misccfg_gic_sync_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MP_GIC_RGU_SYNC_DCM) &
		MCU_MISCCFG_GIC_SYNC_DCM_REG0_MASK &
		MCU_MISCCFG_GIC_SYNC_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_gic_sync_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_gic_sync_dcm'"
		 */
		reg_write(MP_GIC_RGU_SYNC_DCM,
			(reg_read(MP_GIC_RGU_SYNC_DCM) &
			~MCU_MISCCFG_GIC_SYNC_DCM_REG0_MASK) |
			MCU_MISCCFG_GIC_SYNC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_gic_sync_dcm'"
		 */
		reg_write(MP_GIC_RGU_SYNC_DCM,
			(reg_read(MP_GIC_RGU_SYNC_DCM) &
			~MCU_MISCCFG_GIC_SYNC_DCM_REG0_MASK) |
			MCU_MISCCFG_GIC_SYNC_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_L2_SHARED_DCM_REG0_MASK ((0x1 << 0))
#define MCU_MISCCFG_L2_SHARED_DCM_REG0_ON ((0x1 << 0))
#define MCU_MISCCFG_L2_SHARED_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mcu_misccfg_l2_shared_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(L2C_SRAM_CTRL) &
		MCU_MISCCFG_L2_SHARED_DCM_REG0_MASK &
		MCU_MISCCFG_L2_SHARED_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_l2_shared_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_l2_shared_dcm'"
		 */
		reg_write(L2C_SRAM_CTRL,
			(reg_read(L2C_SRAM_CTRL) &
			~MCU_MISCCFG_L2_SHARED_DCM_REG0_MASK) |
			MCU_MISCCFG_L2_SHARED_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_l2_shared_dcm'"
		 */
		reg_write(L2C_SRAM_CTRL,
			(reg_read(L2C_SRAM_CTRL) &
			~MCU_MISCCFG_L2_SHARED_DCM_REG0_MASK) |
			MCU_MISCCFG_L2_SHARED_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_MASK ((0x1 << 11) | \
			(0x1 << 31))
#define MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_ON ((0x1 << 11) | \
			(0x1 << 31))
#define MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_OFF ((0x0 << 11) | \
			(0x0 << 31))

bool dcm_mcu_misccfg_mp0_arm_pll_divider_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MP0_PLL_DIVIDER_CFG) &
		MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_MASK &
		MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_mp0_arm_pll_divider_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_mp0_arm_pll_divider_dcm'"
		 */
		reg_write(MP0_PLL_DIVIDER_CFG,
			(reg_read(MP0_PLL_DIVIDER_CFG) &
			~MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_MASK) |
			MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_mp0_arm_pll_divider_dcm'"
		 */
		reg_write(MP0_PLL_DIVIDER_CFG,
			(reg_read(MP0_PLL_DIVIDER_CFG) &
			~MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_MASK) |
			MCU_MISCCFG_MP0_ARM_PLL_DIVIDER_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_MP0_STALL_DCM_REG0_MASK ((0x1f << 0) | \
			(0x1 << 7))
#define MCU_MISCCFG_MP0_STALL_DCM_REG0_ON ((0xc << 0) | \
			(0x1 << 7))
#define MCU_MISCCFG_MP0_STALL_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 7))

bool dcm_mcu_misccfg_mp0_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SYNC_DCM_CLUSTER_CONFIG) &
		MCU_MISCCFG_MP0_STALL_DCM_REG0_MASK &
		MCU_MISCCFG_MP0_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_mp0_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_mp0_stall_dcm'"
		 */
		reg_write(SYNC_DCM_CLUSTER_CONFIG,
			(reg_read(SYNC_DCM_CLUSTER_CONFIG) &
			~MCU_MISCCFG_MP0_STALL_DCM_REG0_MASK) |
			MCU_MISCCFG_MP0_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_mp0_stall_dcm'"
		 */
		reg_write(SYNC_DCM_CLUSTER_CONFIG,
			(reg_read(SYNC_DCM_CLUSTER_CONFIG) &
			~MCU_MISCCFG_MP0_STALL_DCM_REG0_MASK) |
			MCU_MISCCFG_MP0_STALL_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_MASK ((0x1 << 10) | \
			(0x1 << 11) | \
			(0x7f << 12))
#define MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_ON ((0x1 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12))
#define MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_OFF ((0x0 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12))

bool dcm_mcu_misccfg_mp0_sync_dcm_enable_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SYNC_DCM_CONFIG) &
		MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_MASK &
		MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_mp0_sync_dcm_enable(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_mp0_sync_dcm_enable'"
		 */
		reg_write(SYNC_DCM_CONFIG,
			(reg_read(SYNC_DCM_CONFIG) &
			~MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_MASK) |
			MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_mp0_sync_dcm_enable'"
		 */
		reg_write(SYNC_DCM_CONFIG,
			(reg_read(SYNC_DCM_CONFIG) &
			~MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_MASK) |
			MCU_MISCCFG_MP0_SYNC_DCM_ENABLE_REG0_OFF);
	}
}

#define MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_MASK ((0x1 << 11) | \
			(0x1 << 31))
#define MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_ON ((0x1 << 11) | \
			(0x1 << 31))
#define MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_OFF ((0x0 << 11) | \
			(0x0 << 31))

bool dcm_mcu_misccfg_mp1_arm_pll_divider_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MP1_PLL_DIVIDER_CFG) &
		MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_MASK &
		MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_mp1_arm_pll_divider_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_mp1_arm_pll_divider_dcm'"
		 */
		reg_write(MP1_PLL_DIVIDER_CFG,
			(reg_read(MP1_PLL_DIVIDER_CFG) &
			~MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_MASK) |
			MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_mp1_arm_pll_divider_dcm'"
		 */
		reg_write(MP1_PLL_DIVIDER_CFG,
			(reg_read(MP1_PLL_DIVIDER_CFG) &
			~MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_MASK) |
			MCU_MISCCFG_MP1_ARM_PLL_DIVIDER_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_MP1_STALL_DCM_REG0_MASK ((0x1f << 8) | \
			(0x1 << 15))
#define MCU_MISCCFG_MP1_STALL_DCM_REG0_ON ((0xc << 8) | \
			(0x1 << 15))
#define MCU_MISCCFG_MP1_STALL_DCM_REG0_OFF ((0x0 << 8) | \
			(0x0 << 15))

bool dcm_mcu_misccfg_mp1_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SYNC_DCM_CLUSTER_CONFIG) &
		MCU_MISCCFG_MP1_STALL_DCM_REG0_MASK &
		MCU_MISCCFG_MP1_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_mp1_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_mp1_stall_dcm'"
		 */
		reg_write(SYNC_DCM_CLUSTER_CONFIG,
			(reg_read(SYNC_DCM_CLUSTER_CONFIG) &
			~MCU_MISCCFG_MP1_STALL_DCM_REG0_MASK) |
			MCU_MISCCFG_MP1_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_mp1_stall_dcm'"
		 */
		reg_write(SYNC_DCM_CLUSTER_CONFIG,
			(reg_read(SYNC_DCM_CLUSTER_CONFIG) &
			~MCU_MISCCFG_MP1_STALL_DCM_REG0_MASK) |
			MCU_MISCCFG_MP1_STALL_DCM_REG0_OFF);
	}
}

#define MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_MASK ((0x1 << 20) | \
			(0x1 << 21) | \
			(0x7f << 22))
#define MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_ON ((0x1 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22))
#define MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_OFF ((0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22))

bool dcm_mcu_misccfg_mp1_sync_dcm_enable_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SYNC_DCM_CONFIG) &
		MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_MASK &
		MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_mp1_sync_dcm_enable(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM
		 * 'mcu_misccfg_mp1_sync_dcm_enable'"
		 */
		reg_write(SYNC_DCM_CONFIG,
			(reg_read(SYNC_DCM_CONFIG) &
			~MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_MASK) |
			MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM
		 * 'mcu_misccfg_mp1_sync_dcm_enable'"
		 */
		reg_write(SYNC_DCM_CONFIG,
			(reg_read(SYNC_DCM_CONFIG) &
			~MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_MASK) |
			MCU_MISCCFG_MP1_SYNC_DCM_ENABLE_REG0_OFF);
	}
}

#define MCU_MISCCFG_MCU_MISC_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1))
#define MCU_MISCCFG_MCU_MISC_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1))
#define MCU_MISCCFG_MCU_MISC_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1))

bool dcm_mcu_misccfg_mcu_misc_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MCU_MISC_DCM_CTRL) &
		MCU_MISCCFG_MCU_MISC_DCM_REG0_MASK &
		MCU_MISCCFG_MCU_MISC_DCM_REG0_ON);

	return ret;
}

void dcm_mcu_misccfg_mcu_misc_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcu_misccfg_mcu_misc_dcm'" */
		reg_write(MCU_MISC_DCM_CTRL,
			(reg_read(MCU_MISC_DCM_CTRL) &
			~MCU_MISCCFG_MCU_MISC_DCM_REG0_MASK) |
			MCU_MISCCFG_MCU_MISC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcu_misccfg_mcu_misc_dcm'" */
		reg_write(MCU_MISC_DCM_CTRL,
			(reg_read(MCU_MISC_DCM_CTRL) &
			~MCU_MISCCFG_MCU_MISC_DCM_REG0_MASK) |
			MCU_MISCCFG_MCU_MISC_DCM_REG0_OFF);
	}
}

#define CHN0_EMI_DCM_EMI_GROUP_REG0_MASK ((0xff << 24))
#define CHN0_EMI_DCM_EMI_GROUP_REG0_ON ((0x0 << 24))
#define CHN0_EMI_DCM_EMI_GROUP_REG0_OFF ((0xff << 24))

bool dcm_chn0_emi_dcm_emi_group_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(CHN0_EMI_CHN_EMI_CONB) &
		CHN0_EMI_DCM_EMI_GROUP_REG0_MASK &
		CHN0_EMI_DCM_EMI_GROUP_REG0_ON);

	return ret;
}

void dcm_chn0_emi_dcm_emi_group(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'chn0_emi_dcm_emi_group'" */
		reg_write(CHN0_EMI_CHN_EMI_CONB,
			(reg_read(CHN0_EMI_CHN_EMI_CONB) &
			~CHN0_EMI_DCM_EMI_GROUP_REG0_MASK) |
			CHN0_EMI_DCM_EMI_GROUP_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'chn0_emi_dcm_emi_group'" */
		reg_write(CHN0_EMI_CHN_EMI_CONB,
			(reg_read(CHN0_EMI_CHN_EMI_CONB) &
			~CHN0_EMI_DCM_EMI_GROUP_REG0_MASK) |
			CHN0_EMI_DCM_EMI_GROUP_REG0_OFF);
	}
}

#define CHN1_EMI_DCM_EMI_GROUP_REG0_MASK ((0xff << 24))
#define CHN1_EMI_DCM_EMI_GROUP_REG0_ON ((0x0 << 24))
#define CHN1_EMI_DCM_EMI_GROUP_REG0_OFF ((0xff << 24))

bool dcm_chn1_emi_dcm_emi_group_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(CHN1_EMI_CHN_EMI_CONB) &
		CHN1_EMI_DCM_EMI_GROUP_REG0_MASK &
		CHN1_EMI_DCM_EMI_GROUP_REG0_ON);

	return ret;
}

void dcm_chn1_emi_dcm_emi_group(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'chn1_emi_dcm_emi_group'" */
		reg_write(CHN1_EMI_CHN_EMI_CONB,
			(reg_read(CHN1_EMI_CHN_EMI_CONB) &
			~CHN1_EMI_DCM_EMI_GROUP_REG0_MASK) |
			CHN1_EMI_DCM_EMI_GROUP_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'chn1_emi_dcm_emi_group'" */
		reg_write(CHN1_EMI_CHN_EMI_CONB,
			(reg_read(CHN1_EMI_CHN_EMI_CONB) &
			~CHN1_EMI_DCM_EMI_GROUP_REG0_MASK) |
			CHN1_EMI_DCM_EMI_GROUP_REG0_OFF);
	}
}

#if 0
#define GCE_GCE_DCM_REG0_MASK ((0xffff << 0))
#define GCE_GCE_DCM_REG0_ON ((0xffff << 0))
#define GCE_GCE_DCM_REG0_OFF ((0xffff << 0))

bool dcm_gce_gce_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(GCE_CTL_INT0) &
		GCE_GCE_DCM_REG0_MASK &
		GCE_GCE_DCM_REG0_ON);

	return ret;
}

void dcm_gce_gce_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'gce_gce_dcm'" */
		reg_write(GCE_CTL_INT0,
			(reg_read(GCE_CTL_INT0) &
			~GCE_GCE_DCM_REG0_MASK) |
			GCE_GCE_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'gce_gce_dcm'" */
		reg_write(GCE_CTL_INT0,
			(reg_read(GCE_CTL_INT0) &
			~GCE_GCE_DCM_REG0_MASK) |
			GCE_GCE_DCM_REG0_OFF);
	}
}

#define AUDIO_AUDIO_BUS_REG0_MASK ((0x1 << 29) | \
			(0x1 << 30))
#define AUDIO_AUDIO_BUS_REG0_ON ((0x1 << 29) | \
			(0x1 << 30))
#define AUDIO_AUDIO_BUS_REG0_OFF ((0x0 << 29) | \
			(0x0 << 30))

bool dcm_audio_audio_bus_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(AUDIO_TOP_CON0) &
		AUDIO_AUDIO_BUS_REG0_MASK &
		AUDIO_AUDIO_BUS_REG0_ON);

	return ret;
}

void dcm_audio_audio_bus(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'audio_audio_bus'" */
		reg_write(AUDIO_TOP_CON0,
			(reg_read(AUDIO_TOP_CON0) &
			~AUDIO_AUDIO_BUS_REG0_MASK) |
			AUDIO_AUDIO_BUS_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'audio_audio_bus'" */
		reg_write(AUDIO_TOP_CON0,
			(reg_read(AUDIO_TOP_CON0) &
			~AUDIO_AUDIO_BUS_REG0_MASK) |
			AUDIO_AUDIO_BUS_REG0_OFF);
	}
}

#define EFUSEC_EFUSE_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define EFUSEC_EFUSE_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define EFUSEC_EFUSE_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2))

bool dcm_efusec_efuse_dcm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(EFUSEC_DCM_ON) &
		EFUSEC_EFUSE_DCM_REG0_MASK &
		EFUSEC_EFUSE_DCM_REG0_ON);

	return ret;
}

void dcm_efusec_efuse_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'efusec_efuse_dcm'" */
		reg_write(EFUSEC_DCM_ON,
			(reg_read(EFUSEC_DCM_ON) &
			~EFUSEC_EFUSE_DCM_REG0_MASK) |
			EFUSEC_EFUSE_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'efusec_efuse_dcm'" */
		reg_write(EFUSEC_DCM_ON,
			(reg_read(EFUSEC_DCM_ON) &
			~EFUSEC_EFUSE_DCM_REG0_MASK) |
			EFUSEC_EFUSE_DCM_REG0_OFF);
	}
}

#define MFGCFG_MEG_REG0_MASK ((0x1 << 15))
#define MFGCFG_MEG_REG0_ON ((0x0 << 15))
#define MFGCFG_MEG_REG0_OFF ((0x0 << 15))

bool dcm_mfgcfg_meg_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MFGCFG_MFG_DCM_CON_0) &
		MFGCFG_MEG_REG0_MASK &
		MFGCFG_MEG_REG0_ON);

	return ret;
}

void dcm_mfgcfg_meg(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mfgcfg_meg'" */
		reg_write(MFGCFG_MFG_DCM_CON_0,
			(reg_read(MFGCFG_MFG_DCM_CON_0) &
			~MFGCFG_MEG_REG0_MASK) |
			MFGCFG_MEG_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mfgcfg_meg'" */
		reg_write(MFGCFG_MFG_DCM_CON_0,
			(reg_read(MFGCFG_MFG_DCM_CON_0) &
			~MFGCFG_MEG_REG0_MASK) |
			MFGCFG_MEG_REG0_OFF);
	}
}

#define MFGCFG_MFG_REG0_MASK ((0x1 << 31))
#define MFGCFG_MFG_REG0_ON ((0x0 << 31))
#define MFGCFG_MFG_REG0_OFF ((0x0 << 31))

bool dcm_mfgcfg_mfg_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MFGCFG_MFG_DCM_CON_0) &
		MFGCFG_MFG_REG0_MASK &
		MFGCFG_MFG_REG0_ON);

	return ret;
}

void dcm_mfgcfg_mfg(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mfgcfg_mfg'" */
		reg_write(MFGCFG_MFG_DCM_CON_0,
			(reg_read(MFGCFG_MFG_DCM_CON_0) &
			~MFGCFG_MFG_REG0_MASK) |
			MFGCFG_MFG_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mfgcfg_mfg'" */
		reg_write(MFGCFG_MFG_DCM_CON_0,
			(reg_read(MFGCFG_MFG_DCM_CON_0) &
			~MFGCFG_MFG_REG0_MASK) |
			MFGCFG_MFG_REG0_OFF);
	}
}

#define MMSYS_CONFIG_MMSYS_CONFIG_REG0_MASK ((0x1 << 0) | \
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
			(0x1 << 22) | \
			(0x1 << 23) | \
			(0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 26) | \
			(0x1 << 27) | \
			(0x1 << 28) | \
			(0x1 << 29))
#define MMSYS_CONFIG_MMSYS_CONFIG_REG1_MASK ((0x1 << 0) | \
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
			(0x1 << 17))
#define MMSYS_CONFIG_MMSYS_CONFIG_REG0_ON ((0x0 << 0) | \
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
			(0x0 << 22) | \
			(0x0 << 23) | \
			(0x0 << 24) | \
			(0x0 << 25) | \
			(0x0 << 26) | \
			(0x0 << 27) | \
			(0x0 << 28) | \
			(0x0 << 29))
#define MMSYS_CONFIG_MMSYS_CONFIG_REG1_ON ((0x0 << 0) | \
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
			(0x0 << 17))
#define MMSYS_CONFIG_MMSYS_CONFIG_REG0_OFF ((0x1 << 0) | \
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
			(0x1 << 22) | \
			(0x1 << 23) | \
			(0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 26) | \
			(0x1 << 27) | \
			(0x1 << 28) | \
			(0x1 << 29))
#define MMSYS_CONFIG_MMSYS_CONFIG_REG1_OFF ((0x1 << 0) | \
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
			(0x1 << 17))

bool dcm_mmsys_config_mmsys_config_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(MMSYS_HW_DCM_1ST_DIS0) &
		MMSYS_CONFIG_MMSYS_CONFIG_REG0_MASK &
		MMSYS_CONFIG_MMSYS_CONFIG_REG0_ON);
	ret &= !!(reg_read(MMSYS_HW_DCM_2ND_DIS0) &
		MMSYS_CONFIG_MMSYS_CONFIG_REG1_MASK &
		MMSYS_CONFIG_MMSYS_CONFIG_REG1_ON);

	return ret;
}

void dcm_mmsys_config_mmsys_config(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mmsys_config_mmsys_config'" */
		reg_write(MMSYS_HW_DCM_1ST_DIS0,
			(reg_read(MMSYS_HW_DCM_1ST_DIS0) &
			~MMSYS_CONFIG_MMSYS_CONFIG_REG0_MASK) |
			MMSYS_CONFIG_MMSYS_CONFIG_REG0_ON);
		reg_write(MMSYS_HW_DCM_2ND_DIS0,
			(reg_read(MMSYS_HW_DCM_2ND_DIS0) &
			~MMSYS_CONFIG_MMSYS_CONFIG_REG1_MASK) |
			MMSYS_CONFIG_MMSYS_CONFIG_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mmsys_config_mmsys_config'" */
		reg_write(MMSYS_HW_DCM_1ST_DIS0,
			(reg_read(MMSYS_HW_DCM_1ST_DIS0) &
			~MMSYS_CONFIG_MMSYS_CONFIG_REG0_MASK) |
			MMSYS_CONFIG_MMSYS_CONFIG_REG0_OFF);
		reg_write(MMSYS_HW_DCM_2ND_DIS0,
			(reg_read(MMSYS_HW_DCM_2ND_DIS0) &
			~MMSYS_CONFIG_MMSYS_CONFIG_REG1_MASK) |
			MMSYS_CONFIG_MMSYS_CONFIG_REG1_OFF);
	}
}

#define SMI_COMMON_SMI_COMM_REG0_MASK ((0x7f << 1))
#define SMI_COMMON_SMI_COMM_REG0_ON ((0x0 << 1))
#define SMI_COMMON_SMI_COMM_REG0_OFF ((0x1 << 1))

bool dcm_smi_common_smi_comm_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SMI_COMMON_SMI_DCM) &
		SMI_COMMON_SMI_COMM_REG0_MASK &
		SMI_COMMON_SMI_COMM_REG0_ON);

	return ret;
}

void dcm_smi_common_smi_comm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'smi_common_smi_comm'" */
		reg_write(SMI_COMMON_SMI_DCM,
			(reg_read(SMI_COMMON_SMI_DCM) &
			~SMI_COMMON_SMI_COMM_REG0_MASK) |
			SMI_COMMON_SMI_COMM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'smi_common_smi_comm'" */
		reg_write(SMI_COMMON_SMI_DCM,
			(reg_read(SMI_COMMON_SMI_DCM) &
			~SMI_COMMON_SMI_COMM_REG0_MASK) |
			SMI_COMMON_SMI_COMM_REG0_OFF);
	}
}

#define SMI_LARB0_SMI_LARB0_REG0_MASK ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB0_SMI_LARB0_REG1_MASK ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB0_SMI_LARB0_REG0_ON ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB0_SMI_LARB0_REG1_ON ((0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define SMI_LARB0_SMI_LARB0_REG0_OFF ((0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define SMI_LARB0_SMI_LARB0_REG1_OFF ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))

bool dcm_smi_larb0_smi_larb0_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SMI_LARB0_CON_SET) &
		SMI_LARB0_SMI_LARB0_REG0_MASK &
		SMI_LARB0_SMI_LARB0_REG0_ON);
	ret &= !!(reg_read(SMI_LARB0_CON_CLR) &
		SMI_LARB0_SMI_LARB0_REG1_MASK &
		SMI_LARB0_SMI_LARB0_REG1_ON);

	return ret;
}

void dcm_smi_larb0_smi_larb0(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'smi_larb0_smi_larb0'" */
		reg_write(SMI_LARB0_CON_SET,
			(reg_read(SMI_LARB0_CON_SET) &
			~SMI_LARB0_SMI_LARB0_REG0_MASK) |
			SMI_LARB0_SMI_LARB0_REG0_ON);
		reg_write(SMI_LARB0_CON_CLR,
			(reg_read(SMI_LARB0_CON_CLR) &
			~SMI_LARB0_SMI_LARB0_REG1_MASK) |
			SMI_LARB0_SMI_LARB0_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'smi_larb0_smi_larb0'" */
		reg_write(SMI_LARB0_CON_SET,
			(reg_read(SMI_LARB0_CON_SET) &
			~SMI_LARB0_SMI_LARB0_REG0_MASK) |
			SMI_LARB0_SMI_LARB0_REG0_OFF);
		reg_write(SMI_LARB0_CON_CLR,
			(reg_read(SMI_LARB0_CON_CLR) &
			~SMI_LARB0_SMI_LARB0_REG1_MASK) |
			SMI_LARB0_SMI_LARB0_REG1_OFF);
	}
}

#define SMI_LARB2_SMI_LARB2_REG0_MASK ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB2_SMI_LARB2_REG1_MASK ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB2_SMI_LARB2_REG0_ON ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB2_SMI_LARB2_REG1_ON ((0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define SMI_LARB2_SMI_LARB2_REG0_OFF ((0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define SMI_LARB2_SMI_LARB2_REG1_OFF ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))

bool dcm_smi_larb2_smi_larb2_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SMI_LARB2_CON_SET) &
		SMI_LARB2_SMI_LARB2_REG0_MASK &
		SMI_LARB2_SMI_LARB2_REG0_ON);
	ret &= !!(reg_read(SMI_LARB2_CON_CLR) &
		SMI_LARB2_SMI_LARB2_REG1_MASK &
		SMI_LARB2_SMI_LARB2_REG1_ON);

	return ret;
}

void dcm_smi_larb2_smi_larb2(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'smi_larb2_smi_larb2'" */
		reg_write(SMI_LARB2_CON_SET,
			(reg_read(SMI_LARB2_CON_SET) &
			~SMI_LARB2_SMI_LARB2_REG0_MASK) |
			SMI_LARB2_SMI_LARB2_REG0_ON);
		reg_write(SMI_LARB2_CON_CLR,
			(reg_read(SMI_LARB2_CON_CLR) &
			~SMI_LARB2_SMI_LARB2_REG1_MASK) |
			SMI_LARB2_SMI_LARB2_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'smi_larb2_smi_larb2'" */
		reg_write(SMI_LARB2_CON_SET,
			(reg_read(SMI_LARB2_CON_SET) &
			~SMI_LARB2_SMI_LARB2_REG0_MASK) |
			SMI_LARB2_SMI_LARB2_REG0_OFF);
		reg_write(SMI_LARB2_CON_CLR,
			(reg_read(SMI_LARB2_CON_CLR) &
			~SMI_LARB2_SMI_LARB2_REG1_MASK) |
			SMI_LARB2_SMI_LARB2_REG1_OFF);
	}
}

#define SMI_LARB1_SMI_LARB1_REG0_MASK ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB1_SMI_LARB1_REG1_MASK ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB1_SMI_LARB1_REG0_ON ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB1_SMI_LARB1_REG1_ON ((0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define SMI_LARB1_SMI_LARB1_REG0_OFF ((0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define SMI_LARB1_SMI_LARB1_REG1_OFF ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))

bool dcm_smi_larb1_smi_larb1_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SMI_LARB1_CON_SET) &
		SMI_LARB1_SMI_LARB1_REG0_MASK &
		SMI_LARB1_SMI_LARB1_REG0_ON);
	ret &= !!(reg_read(SMI_LARB1_CON_CLR) &
		SMI_LARB1_SMI_LARB1_REG1_MASK &
		SMI_LARB1_SMI_LARB1_REG1_ON);

	return ret;
}

void dcm_smi_larb1_smi_larb1(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'smi_larb1_smi_larb1'" */
		reg_write(SMI_LARB1_CON_SET,
			(reg_read(SMI_LARB1_CON_SET) &
			~SMI_LARB1_SMI_LARB1_REG0_MASK) |
			SMI_LARB1_SMI_LARB1_REG0_ON);
		reg_write(SMI_LARB1_CON_CLR,
			(reg_read(SMI_LARB1_CON_CLR) &
			~SMI_LARB1_SMI_LARB1_REG1_MASK) |
			SMI_LARB1_SMI_LARB1_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'smi_larb1_smi_larb1'" */
		reg_write(SMI_LARB1_CON_SET,
			(reg_read(SMI_LARB1_CON_SET) &
			~SMI_LARB1_SMI_LARB1_REG0_MASK) |
			SMI_LARB1_SMI_LARB1_REG0_OFF);
		reg_write(SMI_LARB1_CON_CLR,
			(reg_read(SMI_LARB1_CON_CLR) &
			~SMI_LARB1_SMI_LARB1_REG1_MASK) |
			SMI_LARB1_SMI_LARB1_REG1_OFF);
	}
}

#define VENC_VENC_CG_CTRL_REG0_MASK ((0xffffffff << 0))
#define VENC_VENC_CG_CTRL_REG0_ON ((0xffffffff << 0))
#define VENC_VENC_CG_CTRL_REG0_OFF ((0x0 << 0))

bool dcm_venc_venc_cg_ctrl_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(VENC_CLK_CG_CTRL) &
		VENC_VENC_CG_CTRL_REG0_MASK &
		VENC_VENC_CG_CTRL_REG0_ON);

	return ret;
}

void dcm_venc_venc_cg_ctrl(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'venc_venc_cg_ctrl'" */
		reg_write(VENC_CLK_CG_CTRL,
			(reg_read(VENC_CLK_CG_CTRL) &
			~VENC_VENC_CG_CTRL_REG0_MASK) |
			VENC_VENC_CG_CTRL_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'venc_venc_cg_ctrl'" */
		reg_write(VENC_CLK_CG_CTRL,
			(reg_read(VENC_CLK_CG_CTRL) &
			~VENC_VENC_CG_CTRL_REG0_MASK) |
			VENC_VENC_CG_CTRL_REG0_OFF);
	}
}

#define VENC_VENC_DCM_CTRL_REG0_MASK ((0x1 << 0))
#define VENC_VENC_DCM_CTRL_REG0_ON ((0x1 << 0))
#define VENC_VENC_DCM_CTRL_REG0_OFF ((0x0 << 0))

bool dcm_venc_venc_dcm_ctrl_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(VENC_CLK_DCM_CTRL) &
		VENC_VENC_DCM_CTRL_REG0_MASK &
		VENC_VENC_DCM_CTRL_REG0_ON);

	return ret;
}

void dcm_venc_venc_dcm_ctrl(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'venc_venc_dcm_ctrl'" */
		reg_write(VENC_CLK_DCM_CTRL,
			(reg_read(VENC_CLK_DCM_CTRL) &
			~VENC_VENC_DCM_CTRL_REG0_MASK) |
			VENC_VENC_DCM_CTRL_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'venc_venc_dcm_ctrl'" */
		reg_write(VENC_CLK_DCM_CTRL,
			(reg_read(VENC_CLK_DCM_CTRL) &
			~VENC_VENC_DCM_CTRL_REG0_MASK) |
			VENC_VENC_DCM_CTRL_REG0_OFF);
	}
}

#define JPGENC_JPGENC_REG0_MASK ((0x1 << 0))
#define JPGENC_JPGENC_REG0_ON ((0x0 << 0))
#define JPGENC_JPGENC_REG0_OFF ((0x1 << 0))

bool dcm_jpgenc_jpgenc_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(JPGENC_DCM_CTRL) &
		JPGENC_JPGENC_REG0_MASK &
		JPGENC_JPGENC_REG0_ON);

	return ret;
}

void dcm_jpgenc_jpgenc(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'jpgenc_jpgenc'" */
		reg_write(JPGENC_DCM_CTRL,
			(reg_read(JPGENC_DCM_CTRL) &
			~JPGENC_JPGENC_REG0_MASK) |
			JPGENC_JPGENC_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'jpgenc_jpgenc'" */
		reg_write(JPGENC_DCM_CTRL,
			(reg_read(JPGENC_DCM_CTRL) &
			~JPGENC_JPGENC_REG0_MASK) |
			JPGENC_JPGENC_REG0_OFF);
	}
}

#define SMI_LARB3_SMI_LARB3_REG0_MASK ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB3_SMI_LARB3_REG1_MASK ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB3_SMI_LARB3_REG0_ON ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))
#define SMI_LARB3_SMI_LARB3_REG1_ON ((0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define SMI_LARB3_SMI_LARB3_REG0_OFF ((0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 12))
#define SMI_LARB3_SMI_LARB3_REG1_OFF ((0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0xf << 8) | \
			(0xf << 12))

bool dcm_smi_larb3_smi_larb3_is_on(void)
{
	bool ret = true;

	ret &= !!(reg_read(SMI_LARB3_CON_SET) &
		SMI_LARB3_SMI_LARB3_REG0_MASK &
		SMI_LARB3_SMI_LARB3_REG0_ON);
	ret &= !!(reg_read(SMI_LARB3_CON_CLR) &
		SMI_LARB3_SMI_LARB3_REG1_MASK &
		SMI_LARB3_SMI_LARB3_REG1_ON);

	return ret;
}

void dcm_smi_larb3_smi_larb3(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'smi_larb3_smi_larb3'" */
		reg_write(SMI_LARB3_CON_SET,
			(reg_read(SMI_LARB3_CON_SET) &
			~SMI_LARB3_SMI_LARB3_REG0_MASK) |
			SMI_LARB3_SMI_LARB3_REG0_ON);
		reg_write(SMI_LARB3_CON_CLR,
			(reg_read(SMI_LARB3_CON_CLR) &
			~SMI_LARB3_SMI_LARB3_REG1_MASK) |
			SMI_LARB3_SMI_LARB3_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'smi_larb3_smi_larb3'" */
		reg_write(SMI_LARB3_CON_SET,
			(reg_read(SMI_LARB3_CON_SET) &
			~SMI_LARB3_SMI_LARB3_REG0_MASK) |
			SMI_LARB3_SMI_LARB3_REG0_OFF);
		reg_write(SMI_LARB3_CON_CLR,
			(reg_read(SMI_LARB3_CON_CLR) &
			~SMI_LARB3_SMI_LARB3_REG1_MASK) |
			SMI_LARB3_SMI_LARB3_REG1_OFF);
	}
}
#endif

