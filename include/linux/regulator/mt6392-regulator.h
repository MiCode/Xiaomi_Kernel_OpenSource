/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
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

#ifndef __LINUX_REGULATOR_MT6392_H
#define __LINUX_REGULATOR_MT6392_H

enum {
	MT6392_ID_VPROC = 0,
	MT6392_ID_VSYS,
	MT6392_ID_VCORE,
	MT6392_ID_VXO22,
	MT6392_ID_VAUD22,
	MT6392_ID_VCAMA,
	MT6392_ID_VAUD28,
	MT6392_ID_VADC18,
	MT6392_ID_VCN35,
	MT6392_ID_VIO28,
	MT6392_ID_VUSB = 10,
	MT6392_ID_VMC,
	MT6392_ID_VMCH,
	MT6392_ID_VEMC3V3,
	MT6392_ID_VGP1,
	MT6392_ID_VGP2,
	MT6392_ID_VCN18,
	MT6392_ID_VCAMAF,
	MT6392_ID_VM,
	MT6392_ID_VIO18,
	MT6392_ID_VCAMD,
	MT6392_ID_VCAMIO,
	MT6392_ID_VM25,
	MT6392_ID_VEFUSE,
	MT6392_ID_RG_MAX,
};

#define MT6392_MAX_REGULATOR	MT6392_ID_RG_MAX

#endif /* __LINUX_REGULATOR_MT6392_H */
