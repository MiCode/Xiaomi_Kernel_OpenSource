/*
 *  drivers/misc/mediatek/pmic/mt6360/inc/mt6360_pmu_core.h
 *
 *  Copyright (C) 2018 Mediatek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT6360_PMU_CORE_H
#define __MT6360_PMU_CORE_H

struct mt6360_core_platform_data {
	u32 i2cstmr_rst_en;
	u32 i2cstmr_rst_tmr;
	u32 mren;
	u32 mrstb_tmr;
	u32 mrstb_rst_sel;
	u32 apwdtrst_en;
	u32 apwdtrst_sel;
	u32 cc_open_sel;
	u32 i2c_cc_open_tsel;
	u32 pd_mden;
	u32 ship_rst_dis;
	u32 ot_shdn_sel;
	u32 vddaov_shdn_sel;
	u32 otp0_en;
	u32 otp1_en;
	u32 otp1_lpfoff_en;
	u32 ldo5_otp_en;
	u32 ldo5_otp_lpfoff_en;
	u32 shipping_mode_pass_clock;
	u32 sda_sizesel;
	u32 sda_drvsrsel;
	u32 fon_enbase;
	u32 fon_osc;
	u32 fod_hw_en;
	u32 fod_isense;
};

#endif /* __MT6360_PMU_CORE_H */
