/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful, but without
 * any warranty; without even the implied warranty of merchantability or
 * fitness for a particular purpose.  see the gnu general public license for
 * more details.
 *
 * you should have received a copy of the gnu general public license along
 * with this program; if not, write to the free software foundation, inc.,
 * 51 franklin street, fifth floor, boston, ma  02110-1301, usa.
 */

#include <mach/iomap.h>
#include <mach/tegra_fuse.h>
#include <mach/hardware.h>

#include "apbio.h"
#include "fuse.h"

#ifndef __TEGRA11x_FUSE_OFFSETS_H
#define __TEGRA11x_FUSE_OFFSETS_H

/* private_key4 */
#define DEVKEY_START_OFFSET		0x2C
#define DEVKEY_START_BIT		7

/* arm_debug_dis */
#define JTAG_START_OFFSET		0x0
#define JTAG_START_BIT			3

/* security_mode */
#define ODM_PROD_START_OFFSET		0x0
#define ODM_PROD_START_BIT		7

/* boot_device_info */
#define SB_DEVCFG_START_OFFSET		0x2E
#define SB_DEVCFG_START_BIT		7

/* reserved_sw[2:0] */
#define SB_DEVSEL_START_OFFSET		0x2E
#define SB_DEVSEL_START_BIT		23

/* private_key0 -> private_key3 (SBK) */
#define SBK_START_OFFSET		0x24
#define SBK_START_BIT			7

/* reserved_sw[7:4] */
#define SW_RESERVED_START_OFFSET	0x2E
#define SW_RESERVED_START_BIT		27

/* reserved_sw[3] */
#define IGNORE_DEVSEL_START_OFFSET	0x2E
#define IGNORE_DEVSEL_START_BIT		26

/* public key */
#define	PUBLIC_KEY_START_OFFSET		0x0A
#define PUBLIC_KEY_START_BIT		25

/* pkc_disable */
#define PKC_DISABLE_START_OFFSET	0x5A
#define PKC_DISABLE_START_BIT		22

/* video vp8 enable */
#define VP8_ENABLE_START_OFFSET		0x2E
#define VP8_ENABLE_START_BIT		31

/* odm lock */
#define ODM_LOCK_START_OFFSET		0x02
#define ODM_LOCK_START_BIT		0

/* reserved_odm0 -> reserved_odm7 */
#define ODM_RESERVED_DEVSEL_START_OFFSET	0x30
#define ODM_RESERVED_START_BIT			0

#define FUSE_VENDOR_CODE	0x200
#define FUSE_VENDOR_CODE_MASK	0xf
#define FUSE_FAB_CODE		0x204
#define FUSE_FAB_CODE_MASK	0x3f
#define FUSE_LOT_CODE_0		0x208
#define FUSE_LOT_CODE_1		0x20c
#define FUSE_WAFER_ID		0x210
#define FUSE_WAFER_ID_MASK	0x3f
#define FUSE_X_COORDINATE	0x214
#define FUSE_X_COORDINATE_MASK	0x1ff
#define FUSE_Y_COORDINATE	0x218
#define FUSE_Y_COORDINATE_MASK	0x1ff
#define FUSE_GPU_INFO		0x390
#define FUSE_GPU_INFO_MASK	(1<<2)
#define FUSE_SPARE_BIT		0x280
/* fuse registers used in public fuse data read API */
#define FUSE_TEST_PROGRAM_REVISION_0	0x128
/* fuse spare bits are used to get Tj-ADT values */
#define NUM_TSENSOR_SPARE_BITS	28
/* tsensor calibration register */
#define FUSE_TSENSOR_CALIB_0	0x198
#define FUSE_VSENSOR_CALIB_0	0x18c

#define TEGRA_FUSE_SUPPLY	"vpp_fuse"

int fuse_pgm_cycles[] = {156, 202, 0, 0, 231, 461, 0, 0, 144, 576, 0, 0, 312};

#define CHK_ERR(x) \
{ \
	if (x) { \
		pr_err("%s: sysfs_create_file fail(%d)!", __func__, x); \
		return x; \
	} \
}

DEVICE_ATTR(public_key, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(pkc_disable, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(vp8_enable, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(odm_lock, 0440, tegra_fuse_show, tegra_fuse_store);

int tegra_fuse_get_revision(u32 *rev)
{
	/* fuse revision */
	*rev = tegra_fuse_readl(FUSE_TEST_PROGRAM_REVISION_0);
	return 0;
}

int tegra_fuse_get_tsensor_calibration_data(u32 *calib)
{
	/* tsensor calibration fuse */
	*calib = tegra_fuse_readl(FUSE_TSENSOR_CALIB_0);
	return 0;
}

int tegra_fuse_get_tsensor_spare_bits(u32 *spare_bits)
{
	u32 value;
	int i;

	BUG_ON(NUM_TSENSOR_SPARE_BITS > (sizeof(u32) * 8));
	if (!spare_bits)
		return -ENOMEM;
	*spare_bits = 0;
	/* spare bits 0-27 */
	for (i = 0; i < NUM_TSENSOR_SPARE_BITS; i++) {
		value = tegra_fuse_readl(FUSE_SPARE_BIT +
			(i << 2));
		if (value)
			*spare_bits |= BIT(i);
	}
	return 0;
}

unsigned long long tegra_chip_uid(void)
{

	u64 uid = 0ull;
	u32 reg;
	u32 cid;
	u32 vendor;
	u32 fab;
	u32 lot;
	u32 wafer;
	u32 x;
	u32 y;
	u32 i;

	/* This used to be so much easier in prior chips. Unfortunately, there
	   is no one-stop shopping for the unique id anymore. It must be
	   constructed from various bits of information burned into the fuses
	   during the manufacturing process. The 64-bit unique id is formed
	   by concatenating several bit fields. The notation used for the
	   various fields is <fieldname:size_in_bits> with the UID composed
	   thusly:

	   <CID:4><VENDOR:4><FAB:6><LOT:26><WAFER:6><X:9><Y:9>

	   Where:

		Field    Bits  Position Data
		-------  ----  -------- ----------------------------------------
		CID        4     60     Chip id
		VENDOR     4     56     Vendor code
		FAB        6     50     FAB code
		LOT       26     24     Lot code (5-digit base-36-coded-decimal,
					re-encoded to 26 bits binary)
		WAFER      6     18     Wafer id
		X          9      9     Wafer X-coordinate
		Y          9      0     Wafer Y-coordinate
		-------  ----
		Total     64
	*/

	/* chip id is 1 for tegra 11x */
	cid = 1;

	vendor = tegra_fuse_readl(FUSE_VENDOR_CODE) & FUSE_VENDOR_CODE_MASK;
	fab = tegra_fuse_readl(FUSE_FAB_CODE) & FUSE_FAB_CODE_MASK;

	/* Lot code must be re-encoded from a 5 digit base-36 'BCD' number
	   to a binary number. */
	lot = 0;
	reg = tegra_fuse_readl(FUSE_LOT_CODE_0) << 2;

	for (i = 0; i < 5; ++i) {
		u32 digit = (reg & 0xFC000000) >> 26;
		BUG_ON(digit >= 36);
		lot *= 36;
		lot += digit;
		reg <<= 6;
	}

	wafer = tegra_fuse_readl(FUSE_WAFER_ID) & FUSE_WAFER_ID_MASK;
	x = tegra_fuse_readl(FUSE_X_COORDINATE) & FUSE_X_COORDINATE_MASK;
	y = tegra_fuse_readl(FUSE_Y_COORDINATE) & FUSE_Y_COORDINATE_MASK;

	uid = ((unsigned long long)cid  << 60ull)
	    | ((unsigned long long)vendor << 56ull)
	    | ((unsigned long long)fab << 50ull)
	    | ((unsigned long long)lot << 24ull)
	    | ((unsigned long long)wafer << 18ull)
	    | ((unsigned long long)x << 9ull)
	    | ((unsigned long long)y << 0ull);
	return uid;
}

int tegra_fuse_get_vsensor_calib(u32 *calib)
{
	*calib = tegra_fuse_readl(FUSE_VSENSOR_CALIB_0);
	return 0;
}

static int tsensor_calib_offset[] = {
	[0] = 0x198,
	[1] = 0x184,
	[2] = 0x188,
	[3] = 0x22c,
	[4] = 0x254,
	[5] = 0x258,
	[6] = 0x25c,
	[7] = 0x260,
};

int tegra_fuse_get_tsensor_calib(int index, u32 *calib)
{
	if (index < 0 || index > 7)
		return -EINVAL;
	*calib = tegra_fuse_readl(tsensor_calib_offset[index]);
	return 0;
}

int tegra_fuse_add_sysfs_variables(struct platform_device *pdev,
					bool odm_security_mode)
{
	dev_attr_odm_lock.attr.mode = 0640;
	if (odm_security_mode) {
		dev_attr_public_key.attr.mode =  0440;
		dev_attr_pkc_disable.attr.mode = 0440;
		dev_attr_vp8_enable.attr.mode = 0440;
	} else {
		dev_attr_public_key.attr.mode =  0640;
		dev_attr_pkc_disable.attr.mode = 0640;
		dev_attr_vp8_enable.attr.mode = 0640;
	}
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj, &dev_attr_public_key.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj, &dev_attr_pkc_disable.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj, &dev_attr_vp8_enable.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj, &dev_attr_odm_lock.attr));

	return 0;
}

int tegra_fuse_rm_sysfs_variables(struct platform_device *pdev)
{
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_public_key.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_pkc_disable.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_vp8_enable.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_odm_lock.attr);

	return 0;
}

int tegra_fuse_ch_sysfs_perm(struct kobject *kobj)
{
	CHK_ERR(sysfs_chmod_file(kobj, &dev_attr_public_key.attr, 0440));
	CHK_ERR(sysfs_chmod_file(kobj, &dev_attr_pkc_disable.attr, 0440));
	CHK_ERR(sysfs_chmod_file(kobj, &dev_attr_vp8_enable.attr, 0440));

	return 0;
}
#endif /* __TEGRA11x_FUSE_OFFSETS_H */
