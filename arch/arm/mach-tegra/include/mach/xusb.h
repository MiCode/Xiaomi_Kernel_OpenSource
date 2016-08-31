/*
 * arch/arm/mach-tegra/include/mach/xusb.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Ajay Gupta <ajayg@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _XUSB_H
#define _XUSB_H

/*
 * BIT0 - BIT7 : SS ports
 * BIT8 - BIT15 : USB2 UTMI ports
 * BIT16 - BIT23 : HSIC ports
 * BIT24 - BIT31 : ULPI ports
 */
#define TEGRA_XUSB_SS_P0	(1 << 0)
#define TEGRA_XUSB_SS_P1	(1 << 1)
#define XUSB_UTMI_INDEX	(8)
#define XUSB_UTMI_COUNT	(3)
#define TEGRA_XUSB_USB2_P0	BIT(XUSB_UTMI_INDEX)
#define TEGRA_XUSB_USB2_P1	BIT(XUSB_UTMI_INDEX + 1)
#define TEGRA_XUSB_USB2_P2	BIT(XUSB_UTMI_INDEX + 2)
#define XUSB_HSIC_INDEX	(16)
#define XUSB_HSIC_COUNT	(2)
#define XUSB_SS_PORT_COUNT	(2)
#define XUSB_UTMI_COUNT		(3)
#define XUSB_UTMI_INDEX		(8)
#define TEGRA_XUSB_HSIC_P0	BIT(XUSB_HSIC_INDEX)
#define TEGRA_XUSB_HSIC_P1	BIT(XUSB_HSIC_INDEX + 1)
#define TEGRA_XUSB_ULPI_P0	(1 << 24)
#define TEGRA_XUSB_SS_PORT_MAP_USB2_P0 (0x0)
#define TEGRA_XUSB_SS_PORT_MAP_USB2_P1 (0x1)
#define TEGRA_XUSB_SS_PORT_MAP_USB2_P2 (0x2)
#define TEGRA_XUSB_SS0_PORT_MAP	(0xf)
#define TEGRA_XUSB_SS1_PORT_MAP	(0xf0)
#define TEGRA_XUSB_ULPI_PORT_CAP_MASTER	(0x0)
#define TEGRA_XUSB_ULPI_PORT_CAP_PHY	(0x1)
#define TEGRA_XUSB_UTMIP_PMC_PORT0	(0x0)
#define TEGRA_XUSB_UTMIP_PMC_PORT1	(0x1)
#define TEGRA_XUSB_UTMIP_PMC_PORT2	(0x2)

struct tegra_xusb_regulator_name {
	const char *utmi_vbuses[XUSB_UTMI_COUNT];
	const char *s3p3v;
	const char *s1p8v;
	const char *vddio_hsic;
	const char *s1p05v;
};

/* Ensure dt compatiblity when changing order */
struct tegra_xusb_hsic_config {
	u8 rx_strobe_trim;
	u8 rx_data_trim;
	u8 tx_rtune_n;
	u8 tx_rtune_p;
	u8 tx_slew_n;
	u8 tx_slew_p;
	u8 auto_term_en;
	u8 strb_trim_val;
	u8 pretend_connect;
};

struct tegra_xusb_board_data {
	u32	portmap;
	/*
	 * SS0 or SS1 port may be mapped either to USB2_P0 or USB2_P1
	 * ss_portmap[0:3] = SS0 map, ss_portmap[4:7] = SS1 map
	 */
	u8	ss_portmap;
	u8	ulpicap;
	u8	lane_owner;
	bool uses_external_pmic;
	bool gpio_controls_muxed_ss_lanes;
	u32 gpio_ss1_sata;
	struct tegra_xusb_regulator_name supply;
	struct tegra_xusb_hsic_config hsic[XUSB_HSIC_COUNT];
};

struct tegra_xusb_platform_data {
	u32 portmap;
	u8 lane_owner;
	bool pretend_connect_0;
};

struct tegra_xusb_chip_calib {
	u32 hs_curr_level_pad0;
	u32 hs_curr_level_pad1;
	u32 hs_curr_level_pad2;
	u32 hs_iref_cap;
	u32 hs_term_range_adj;
	u32 hs_squelch_level;
};
struct tegra_xusb_soc_config {
	struct tegra_xusb_board_data *bdata;
	u32 rx_wander;
	u32 rx_eq;
	u32 cdr_cntl;
	u32 dfe_cntl;
	u32 hs_slew;
	u32 ls_rslew_pad0;
	u32 ls_rslew_pad1;
	u32 ls_rslew_pad2;
	u32 hs_disc_lvl;
	u32 spare_in;
	/*
	 * BIT[0:3] = PMC port # for USB2_P0
	 * BIT[4:7] = PMC port # for USB2_P1
	 * BIT[8:11] = PMC port # for USB2_P2
	 */
	u32 pmc_portmap;
	/* chip specific */
	unsigned long quirks;
};

#define TEGRA_XUSB_USE_HS_SRC_CLOCK2 BIT(0)

#endif /* _XUSB_H */
