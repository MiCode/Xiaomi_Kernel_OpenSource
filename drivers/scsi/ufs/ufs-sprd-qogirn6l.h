/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Unisoc. All rights reserved.
 */

#ifndef _UFS_SPRD_QOGIRN6L_H_
#define _UFS_SPRD_QOGIRN6L_H_

struct ufs_sprd_ums9621_data {
	struct regulator *vdd_mphy;
	struct regulator *vddsram;
	struct syscon_ufs phy_sram_ext_ld_done;
	struct syscon_ufs phy_sram_bypass;
	struct syscon_ufs phy_sram_init_done;
	struct syscon_ufs aon_apb_ufs_clk_en;
	struct syscon_ufs ufsdev_refclk_en;
	struct syscon_ufs usb31pllv_ref2mphy_en;
	struct syscon_ufs ufs_cfg_eb;

	struct clk *hclk_source;
	struct clk *hclk;
	struct clk *rco_100M;
	struct reset_control *ap_ahb_ufs_rst;
	struct reset_control *aon_apb_ufs_rst;
	uint32_t ufs_lane_calib_data0;
	uint32_t ufs_lane_calib_data1;
	void __iomem *syssel_reg;
	void __iomem *anlg_phy_g12;
};


/*ufs hclk register*/
#define REG_HCLKDIV    0xFC

/*
 * Synopsys common M-PHY Attributes
 */
#define CBRATESEL				0x8114
#define CBCREGADDRLSB				0x8116
#define CBCREGADDRMSB				0x8117
#define CBCREGWRLSB				0x8118
#define CBCREGWRMSB				0x8119
#define CBCREGRDWRSEL				0x811C
#define CBCRCTRL				0x811F
#define CBREFCLKCTRL2				0x8132
#define CBUPLH8					0x8130

/*
 * Synopsys RX implementation specific M-PHY Attributes
 */
#define RXSQCONTROL				0x8009

#define VS_MPHYDISABLE		0xD0C1

/* Define debug bus register */
#define REG_DEBUG_BUS_SYSSEL	0x7800A100

#define REG_ANLG_PHY_G12	0x64380000

#define UFS_SLOW_MODE		(SLOW_MODE | (SLOW_MODE << 4))

/*
 * This capability allows the host controller enter
 * ULP state in Hibernate8, if it is set
 */
#define UFSHCD_CAP_H8_ULP	(1 << 31)

/* compensation time for TActivate in Hibernate ULP state */
#define ULP_TACTIVATE_COMP_TIME 200

/* Enable ULP H8 */
#define ULP_H8_EN		0x1

#endif/* _UFS_SPRD_QOGIRN6L_H_ */
