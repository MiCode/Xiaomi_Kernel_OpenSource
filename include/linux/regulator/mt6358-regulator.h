/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_REGULATOR_MT6358_H
#define __LINUX_REGULATOR_MT6358_H

#if defined(CONFIG_MACH_MT6781)
#define USE_PMIC_MT6366		1
#else
#define USE_PMIC_MT6366		0
#endif

enum {
	MT6358_ID_VDRAM1 = 0,
	MT6358_ID_VCORE,
	MT6358_ID_VPA,
	MT6358_ID_VPROC11,
	MT6358_ID_VPROC12,
	MT6358_ID_VGPU,
	MT6358_ID_VS2,
	MT6358_ID_VMODEM,
	MT6358_ID_VS1,
	MT6358_ID_VDRAM2 = 9,
	MT6358_ID_VSIM1,
	MT6358_ID_VIBR,
	MT6358_ID_VRF12,
	MT6358_ID_VIO18,
	MT6358_ID_VUSB,
#if !USE_PMIC_MT6366
	MT6358_ID_VCAMIO,
	MT6358_ID_VCAMD,
#endif
	MT6358_ID_VCN18,
	MT6358_ID_VFE28,
	MT6358_ID_VSRAM_PROC11,
	MT6358_ID_VCN28,
	MT6358_ID_VSRAM_OTHERS,
	MT6358_ID_VSRAM_GPU,
	MT6358_ID_VXO22,
	MT6358_ID_VEFUSE,
	MT6358_ID_VAUX18,
	MT6358_ID_VMCH,
	MT6358_ID_VBIF28,
	MT6358_ID_VSRAM_PROC12,
#if !USE_PMIC_MT6366
	MT6358_ID_VCAMA1,
#endif
	MT6358_ID_VEMC,
	MT6358_ID_VIO28,
	MT6358_ID_VA12,
	MT6358_ID_VRF18,
	MT6358_ID_VCN33_BT,
	MT6358_ID_VCN33_WIFI,
#if !USE_PMIC_MT6366
	MT6358_ID_VCAMA2,
#endif
	MT6358_ID_VMC,
#if !USE_PMIC_MT6366
	MT6358_ID_VLDO28,
#endif
	MT6358_ID_VAUD28,
	MT6358_ID_VSIM2,
	MT6358_ID_VA09,
	MT6358_ID_RG_MAX,
};

#define MT6358_MAX_REGULATOR	MT6358_ID_RG_MAX

#endif /* __LINUX_REGULATOR_MT6358_H */
