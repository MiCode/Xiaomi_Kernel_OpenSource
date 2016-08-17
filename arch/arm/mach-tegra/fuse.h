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

#include <mach/iomap.h>
#include <mach/tegra_fuse.h>
#include <mach/hardware.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>

#include "apbio.h"

#ifndef __TEGRA_FUSE_H
#define __TEGRA_FUSE_H

#define FUSE_SKU_INFO		0x110
#define FUSE_VP8_ENABLE_0	0x1c4
#define FUSE_SKU_DIRECT_CONFIG_0            0x1f4
#define FUSE_SKU_GPU_1_PIXEL_PIPE           0x200
#define FUSE_SKU_GPU_1_ALU_PER_PIXEL_PIPE   0x400
#define FUSE_SKU_USB_CALIB_0    0x1f0

#define TEGRA_AGE_0_6 0x2cc /*Spare bit 34*/
#define TEGRA_AGE_1_6 0x308 /*Spare bit 49*/
#define TEGRA_AGE_0_5 0x2c8 /*Spare bit 33*/
#define TEGRA_AGE_1_5 0x304 /*Spare bit 48*/
#define TEGRA_AGE_0_4 0x2c4 /*Spare bit 32*/
#define TEGRA_AGE_1_4 0x300 /*Spare bit 47*/
#define TEGRA_AGE_0_3 0x2c0 /*Spare bit 31*/
#define TEGRA_AGE_1_3 0x2fc /*Spare bit 46*/
#define TEGRA_AGE_0_2 0x2bc /*Spare bit 30*/
#define TEGRA_AGE_1_2 0x2f8 /*Spare bit 45*/
#define TEGRA_AGE_0_1 0x2b8 /*Spare bit 29*/
#define TEGRA_AGE_1_1 0x2f4 /*Spare bit 44*/
#define TEGRA_AGE_0_0 0x2b4 /*Spare bit 28*/
#define TEGRA_AGE_1_0 0x2f0 /*Spare bit 43*/


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
#define SKU_ID_T20	8
#define SKU_ID_T25SE	20
#define SKU_ID_AP25	23
#define SKU_ID_T25	24
#define SKU_ID_AP25E	27
#define SKU_ID_T25E	28

#define TEGRA20		0x20
#define TEGRA30		0x30
#define TEGRA11X	0x35

#define INVALID_PROCESS_ID	99 /* don't expect to have 100 process id's */

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

extern int tegra_sku_id;
extern int tegra_chip_id;
extern int tegra_bct_strapping;

u32 tegra_fuse_readl(unsigned long offset);
void tegra_fuse_writel(u32 val, unsigned long offset);

ssize_t tegra_fuse_show(struct device *dev, struct device_attribute *attr,
								char *buf);
ssize_t tegra_fuse_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count);

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define PUBLIC_KEY_START_OFFSET	0x0
#define PUBLIC_KEY_START_BIT	0

#define PKC_DISABLE_START_OFFSET	0x0
#define PKC_DISABLE_START_BIT		0

#define VP8_ENABLE_START_OFFSET	0x0
#define VP8_ENABLE_START_BIT	0

#define ODM_LOCK_START_OFFSET	0x0
#define ODM_LOCK_START_BIT	0

static inline int tegra_fuse_add_sysfs_variables(struct platform_device *pdev,
					bool odm_prod_mode)
{
	return -ENOENT;
}

static inline int tegra_fuse_rm_sysfs_variables(struct platform_device *pdev)
{
	return -ENOENT;
}

static inline int tegra_fuse_ch_sysfs_perm(struct kobject *kobj)
{
	return -ENOENT;
}

static inline int tegra_apply_fuse(void)
{
	return 0;
}
#else
static inline int tegra_apply_fuse(void)
{
	return -ENOENT;
}
#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static inline int tegra_fuse_get_revision(u32 *rev)
{
	return -ENOENT;
}

static inline int tegra_fuse_get_tsensor_calibration_data(u32 *calib)
{
	return -ENOENT;
}
static inline int tegra_fuse_get_tsensor_spare_bits(u32 *spare_bits)
{
	return -ENOENT;
}
int tegra_fuse_get_priv(char *priv);
#else
int tegra_fuse_get_revision(u32 *rev);
int tegra_fuse_get_tsensor_calibration_data(u32 *calib);
int tegra_fuse_get_tsensor_spare_bits(u32 *spare_bits);
static inline int tegra_fuse_get_priv(char *priv)
{
	return -ENOENT;
}
#endif

unsigned long long tegra_chip_uid(void);
unsigned int tegra_spare_fuse(int bit);
void tegra_init_fuse(void);
const char *tegra_get_revision_name(void);

/*
 * read the fuse settings
 * @param: io_param_type - param type enum
 * @param: size - read size in bytes
 */
int tegra_fuse_read(enum fuse_io_param io_param_type, u32 *data, int size);

/*
 * Prior to invoking this routine, the caller is responsible for supplying
 * valid fuse programming voltage.
 *
 * @param: pgm_data - entire data to be programmed
 * @flags: program flags (e.g. FLAGS_DEVKEY)
 */
int tegra_fuse_program(struct fuse_data *pgm_data, u32 flags);

/* Disables the fuse programming until the next system reset */
void tegra_fuse_program_disable(void);

extern int (*tegra_fuse_regulator_en)(int);
#ifdef CONFIG_TEGRA_SILICON_PLATFORM

int tegra_soc_speedo_id(void);
void tegra_init_speedo_data(void);
int tegra_cpu_process_id(void);
int tegra_core_process_id(void);
int tegra_get_age(void);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
int tegra_package_id(void);
int tegra_cpu_speedo_id(void);
int tegra_cpu_speedo_mv(void);
int tegra_cpu_speedo_value(void);
int tegra_core_speedo_mv(void);
int tegra_get_sku_override(void);
int tegra_get_cpu_iddq_value(void);
#else
static inline int tegra_package_id(void) { return -1; }
static inline int tegra_cpu_speedo_id(void) { return 0; }
static inline int tegra_cpu_speedo_value(void) { return 1777; }
static inline int tegra_cpu_speedo_mv(void) { return 1000; }
static inline int tegra_core_speedo_mv(void) { return 1200; }
static inline int tegra_get_cpu_iddq_value(void) { return 0; }
#endif /* CONFIG_ARCH_TEGRA_2x_SOC */

#else

static inline int tegra_cpu_process_id(void) { return 0; }
static inline int tegra_core_process_id(void) { return 0; }
static inline int tegra_cpu_speedo_id(void) { return 0; }
static inline int tegra_cpu_speedo_value(void) { return 1777; }
static inline int tegra_soc_speedo_id(void) { return 0; }
static inline int tegra_package_id(void) { return -1; }
static inline int tegra_cpu_speedo_mv(void) { return 1250; }
static inline int tegra_core_speedo_mv(void) { return 1100; }
static inline void tegra_init_speedo_data(void) { }

#endif /* CONFIG_TEGRA_SILICON_PLATFORM */

#endif /* TEGRA_FUSE_H */
