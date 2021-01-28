// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>
/* #include <mt-plat/mtk_secure_api.h> */
#include <mtk_dcm.h>

#include <mt6765_dcm_internal.h>
#include <mt6765_dcm_autogen.h>

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

