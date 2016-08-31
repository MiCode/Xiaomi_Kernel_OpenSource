/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2010-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Sumit Sharma <sumsharma@nvidia.com>
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

#include <linux/tegra-soc.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>

#ifndef __FUSE_H
#define __FUSE_H

#define FUSE_SKU_INFO		0x110
#define FUSE_VP8_ENABLE_0	0x1c4
#define FUSE_SKU_USB_CALIB_0	0x1f0
#define FUSE_SKU_DIRECT_CONFIG_0            0x1f4
#define FUSE_SKU_GPU_1_PIXEL_PIPE           0x200
#define FUSE_SKU_GPU_1_ALU_PER_PIXEL_PIPE   0x400

#define NFUSES	92
#define STATE_IDLE	(0x4 << 16)
#define SENSE_DONE	(0x1 << 30)

/* fuse registers */
#define FUSE_CTRL		0x000
#define FUSE_REG_ADDR		0x004
#define FUSE_REG_READ		0x008
#define FUSE_REG_WRITE		0x00C
#define FUSE_TIME_PGM2		0x01C
#define FUSE_PRIV2INTFC		0x020
#define FUSE_DIS_PGM		0x02C
#define FUSE_WRITE_ACCESS	0x030
#define FUSE_PWR_GOOD_SW	0x034
#define FUSE_VP8_ENABLE_0	0x1c4

#define FUSE_NAME_LEN	30

#define FUSE_READ	0x1
#define FUSE_WRITE	0x2
#define FUSE_SENSE	0x3
#define FUSE_CMD_MASK	0x3

#define CAR_OSC_CTRL		0x50
#define PMC_PLLP_OVERRIDE	0xF8
#define PMC_OSC_OVERRIDE	BIT(8)
#define PMC_OSC_FREQ_MASK	(BIT(2) | BIT(3))
#define PMC_OSC_FREQ_SHIFT	2
#define CAR_OSC_FREQ_SHIFT	28

#define FUSE_SENSE_DONE_BIT	BIT(30)
#define START_DATA		BIT(0)
#define SKIP_RAMREPAIR		BIT(1)
#define FUSE_PGM_TIMEOUT_MS	50

#define SBK_DEVKEY_STATUS_SZ	sizeof(u32)

/*
 * fuse io parameters: params with sizes less than a byte are
 * explicitly mentioned
 */
enum fuse_io_param {
	DEVKEY,
	JTAG_DIS, /* 1 bit long */
	/*
	 * Programming the odm production fuse at the same
	 * time as the sbk or dev_key is not allowed as it is not possible to
	 * verify that the sbk or dev_key were programmed correctly.
	 */
	ODM_PROD_MODE, /* 1 bit long */
	SEC_BOOT_DEV_CFG,
	SEC_BOOT_DEV_SEL, /* 3 bits long */
	SBK,
	SW_RSVD, /* 4 bits long */
	IGNORE_DEV_SEL_STRAPS, /* 1 bit long */
	ODM_RSVD,
	PUBLIC_KEY,
	PKC_DISABLE, /* 1 bit long */
	VP8_ENABLE, /* 1 bit long */
	ODM_LOCK, /* 4 bits long */
	SBK_DEVKEY_STATUS,
	_PARAMS_U32 = 0x7FFFFFFF
};

#define MAX_PARAMS SBK_DEVKEY_STATUS

/* the order of the members is pre-decided. please do not change */

/* secondary boot device options */
enum {
	SECBOOTDEV_SDMMC,
	SECBOOTDEV_NOR,
	SECBOOTDEV_SPI,
	SECBOOTDEV_NAND,
	SECBOOTDEV_LBANAND,
	SECBOOTDEV_MUXONENAND,
	_SECBOOTDEV_MAX,
	_SECBOOTDEV_U32 = 0x7FFFFFFF
};

#define FLAGS_DEVKEY			BIT(DEVKEY)
#define FLAGS_JTAG_DIS			BIT(JTAG_DIS)
#define FLAGS_SBK_DEVKEY_STATUS		BIT(SBK_DEVKEY_STATUS)
#define FLAGS_ODM_PROD_MODE		BIT(ODM_PROD_MODE)
#define FLAGS_SEC_BOOT_DEV_CFG		BIT(SEC_BOOT_DEV_CFG)
#define FLAGS_SEC_BOOT_DEV_SEL		BIT(SEC_BOOT_DEV_SEL)
#define FLAGS_SBK			BIT(SBK)
#define FLAGS_SW_RSVD			BIT(SW_RSVD)
#define FLAGS_IGNORE_DEV_SEL_STRAPS	BIT(IGNORE_DEV_SEL_STRAPS)
#define FLAGS_ODMRSVD			BIT(ODM_RSVD)
#define FLAGS_PUB_KEY			BIT(PUBLIC_KEY)
#define FLAGS_PKC_DIS			BIT(PKC_DISABLE)
#define FLAGS_VP8_EN			BIT(VP8_ENABLE)
#define FLAGS_ODM_LOCK			BIT(ODM_LOCK)

struct fuse_data {
	u32 devkey;
	u32 jtag_dis;
	u32 odm_prod_mode;
	u32 bootdev_cfg;
	u32 bootdev_sel;
	u32 sbk[4];
	u32 sw_rsvd;
	u32 ignore_devsel_straps;
	u32 odm_rsvd[8];
	u32 public_key[8];
	u32 pkc_disable;
	u32 vp8_enable;
	u32 odm_lock;
};

ssize_t tegra_fuse_show(struct device *dev, struct device_attribute *attr,
								char *buf);
ssize_t tegra_fuse_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count);
#endif /* FUSE_H */
