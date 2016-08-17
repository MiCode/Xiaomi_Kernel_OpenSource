/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/ctype.h>
#include <linux/wakelock.h>

#include <mach/iomap.h>
#include <mach/tegra_fuse.h>
#include <mach/hardware.h>
#include <mach/gpufuse.h>

#include "fuse.h"
#include "apbio.h"

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
#include "tegra2_fuse_offsets.h"
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
#include "tegra3_fuse_offsets.h"
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
#include "tegra11x_fuse_offsets.h"
#else
#include "tegra14x_fuse_offsets.h"
#endif

DEVICE_ATTR(device_key, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(jtag_disable, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(odm_production_mode, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(sec_boot_dev_cfg, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(sec_boot_dev_sel, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(secure_boot_key, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(sw_reserved, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(ignore_dev_sel_straps, 0440, tegra_fuse_show, tegra_fuse_store);
DEVICE_ATTR(odm_reserved, 0440, tegra_fuse_show, tegra_fuse_store);

struct tegra_id {
	enum tegra_chipid chipid;
	unsigned int major, minor, netlist, patch;
	enum tegra_revision revision;
	char *priv;
};

static struct tegra_id tegra_id;

int tegra_sku_id;
int tegra_chip_id;
enum tegra_revision tegra_revision;
static unsigned int tegra_fuse_vp8_enable;
static int tegra_gpu_num_pixel_pipes;
static int tegra_gpu_num_alus_per_pixel_pipe;

static u32 fuse_pgm_data[NFUSES / 2];
static u32 fuse_pgm_mask[NFUSES / 2];
static u32 tmp_fuse_pgm_data[NFUSES / 2];

static struct fuse_data fuse_info;
struct regulator *fuse_regulator;
struct clk *clk_fuse;

struct param_info {
	u32 *addr;
	int sz;
	u32 start_off;
	int start_bit;
	int nbits;
	int data_offset;
	char sysfs_name[FUSE_NAME_LEN];
};

DEFINE_MUTEX(fuse_lock);

/* The BCT to use at boot is specified by board straps that can be read
 * through a APB misc register and decoded. 2 bits, i.e. 4 possible BCTs.
 */
int tegra_bct_strapping;

#define STRAP_OPT 0x008
#define GMI_AD0 BIT(4)
#define GMI_AD1 BIT(5)
#define RAM_ID_MASK (GMI_AD0 | GMI_AD1)
#define RAM_CODE_SHIFT 4

static const char *tegra_revision_name[TEGRA_REVISION_MAX] = {
	[TEGRA_REVISION_UNKNOWN] = "unknown",
	[TEGRA_REVISION_A01]     = "A01",
	[TEGRA_REVISION_A02]     = "A02",
	[TEGRA_REVISION_A03]     = "A03",
	[TEGRA_REVISION_A03p]    = "A03 prime",
	[TEGRA_REVISION_A04]     = "A04",
	[TEGRA_REVISION_A04p]    = "A04 prime",
	[TEGRA_REVISION_QT]      = "QT",
};

#ifdef CONFIG_TEGRA_PRE_SILICON_SUPPORT
static enum tegra_platform tegra_platform;
static bool cpu_is_asim;
static const char *tegra_platform_name[TEGRA_PLATFORM_MAX] = {
	[TEGRA_PLATFORM_SILICON] = "silicon",
	[TEGRA_PLATFORM_QT]      = "quickturn",
	[TEGRA_PLATFORM_LINSIM]  = "linsim",
	[TEGRA_PLATFORM_FPGA]    = "fpga",
};
#endif

static struct param_info fuse_info_tbl[] = {
	[DEVKEY] = {
		.addr = &fuse_info.devkey,
		.sz = sizeof(fuse_info.devkey),
		.start_off = DEVKEY_START_OFFSET,
		.start_bit = DEVKEY_START_BIT,
		.nbits = 32,
		.data_offset = 0,
		.sysfs_name = "device_key",
	},
	[JTAG_DIS] = {
		.addr = &fuse_info.jtag_dis,
		.sz = sizeof(fuse_info.jtag_dis),
		.start_off = JTAG_START_OFFSET,
		.start_bit = JTAG_START_BIT,
		.nbits = 1,
		.data_offset = 1,
		.sysfs_name = "jtag_disable",
	},
	[ODM_PROD_MODE] = {
		.addr = &fuse_info.odm_prod_mode,
		.sz = sizeof(fuse_info.odm_prod_mode),
		.start_off = ODM_PROD_START_OFFSET,
		.start_bit = ODM_PROD_START_BIT,
		.nbits = 1,
		.data_offset = 2,
		.sysfs_name = "odm_production_mode",
	},
	[SEC_BOOT_DEV_CFG] = {
		.addr = &fuse_info.bootdev_cfg,
		.sz = sizeof(fuse_info.bootdev_cfg),
		.start_off = SB_DEVCFG_START_OFFSET,
		.start_bit = SB_DEVCFG_START_BIT,
		.nbits = 16,
		.data_offset = 3,
		.sysfs_name = "sec_boot_dev_cfg",
	},
	[SEC_BOOT_DEV_SEL] = {
		.addr = &fuse_info.bootdev_sel,
		.sz = sizeof(fuse_info.bootdev_sel),
		.start_off = SB_DEVSEL_START_OFFSET,
		.start_bit = SB_DEVSEL_START_BIT,
		.nbits = 3,
		.data_offset = 4,
		.sysfs_name = "sec_boot_dev_sel",
	},
	[SBK] = {
		.addr = fuse_info.sbk,
		.sz = sizeof(fuse_info.sbk),
		.start_off = SBK_START_OFFSET,
		.start_bit = SBK_START_BIT,
		.nbits = 128,
		.data_offset = 5,
		.sysfs_name = "secure_boot_key",
	},
	[SW_RSVD] = {
		.addr = &fuse_info.sw_rsvd,
		.sz = sizeof(fuse_info.sw_rsvd),
		.start_off = SW_RESERVED_START_OFFSET,
		.start_bit = SW_RESERVED_START_BIT,
		.nbits = 4,
		.data_offset = 9,
		.sysfs_name = "sw_reserved",
	},
	[IGNORE_DEV_SEL_STRAPS] = {
		.addr = &fuse_info.ignore_devsel_straps,
		.sz = sizeof(fuse_info.ignore_devsel_straps),
		.start_off = IGNORE_DEVSEL_START_OFFSET,
		.start_bit = IGNORE_DEVSEL_START_BIT,
		.nbits = 1,
		.data_offset = 10,
		.sysfs_name = "ignore_dev_sel_straps",
	},
	[ODM_RSVD] = {
		.addr = fuse_info.odm_rsvd,
		.sz = sizeof(fuse_info.odm_rsvd),
		.start_off = ODM_RESERVED_DEVSEL_START_OFFSET,
		.start_bit = ODM_RESERVED_START_BIT,
		.nbits = 256,
		.data_offset = 11,
		.sysfs_name = "odm_reserved",
	},
	[PUBLIC_KEY] = {
		.addr = fuse_info.public_key,
		.sz = sizeof(fuse_info.public_key),
		.start_off = PUBLIC_KEY_START_OFFSET,
		.start_bit = PUBLIC_KEY_START_BIT,
		.nbits = 256,
		.data_offset = 12,
		.sysfs_name = "public_key",
	},
	[PKC_DISABLE] = {
		.addr = &fuse_info.pkc_disable,
		.sz = sizeof(fuse_info.pkc_disable),
		.start_off = PKC_DISABLE_START_OFFSET,
		.start_bit = PKC_DISABLE_START_BIT,
		.nbits = 1,
		.data_offset = 13,
		.sysfs_name = "pkc_disable",
	},
	[VP8_ENABLE] = {
		.addr = &fuse_info.vp8_enable,
		.sz = sizeof(fuse_info.vp8_enable),
		.start_off = VP8_ENABLE_START_OFFSET,
		.start_bit = VP8_ENABLE_START_BIT,
		.nbits = 1,
		.data_offset = 14,
		.sysfs_name = "vp8_enable",
	},
	[ODM_LOCK] = {
		.addr = &fuse_info.odm_lock,
		.sz = sizeof(fuse_info.odm_lock),
		.start_off = ODM_LOCK_START_OFFSET,
		.start_bit = ODM_LOCK_START_BIT,
		.nbits = 4,
		.data_offset = 15,
		.sysfs_name = "odm_lock",
	},
	[SBK_DEVKEY_STATUS] = {
		.sz = SBK_DEVKEY_STATUS_SZ,
	},
};

u32 tegra_fuse_readl(unsigned long offset)
{
	return tegra_apb_readl(TEGRA_FUSE_BASE + offset);
}

void tegra_fuse_writel(u32 val, unsigned long offset)
{
	tegra_apb_writel(val, TEGRA_FUSE_BASE + offset);
}

static inline bool get_spare_fuse(int bit)
{
	return tegra_fuse_readl(FUSE_SPARE_BIT + bit * 4);
}

const char *tegra_get_revision_name(void)
{
	return tegra_revision_name[tegra_revision];
}

#define TEGRA_READ_AGE_BIT(n, bit, age) {\
	bit = tegra_fuse_readl(TEGRA_AGE_0_##n);\
	bit |= tegra_fuse_readl(TEGRA_AGE_1_##n);\
	bit = bit << n;\
	age |= bit;\
}

int tegra_get_age(void)
{
	int linear_age, age_bit;
	linear_age = age_bit = 0;

	TEGRA_READ_AGE_BIT(6, age_bit, linear_age);
	TEGRA_READ_AGE_BIT(5, age_bit, linear_age);
	TEGRA_READ_AGE_BIT(4, age_bit, linear_age);
	TEGRA_READ_AGE_BIT(3, age_bit, linear_age);
	TEGRA_READ_AGE_BIT(2, age_bit, linear_age);
	TEGRA_READ_AGE_BIT(1, age_bit, linear_age);
	TEGRA_READ_AGE_BIT(0, age_bit, linear_age);

	/*Default Aug, 2012*/
	if (linear_age <= 0)
		linear_age = 8;

	pr_info("TEGRA: Linear age: %d\n", linear_age);

	return linear_age;
}

unsigned int tegra_spare_fuse(int bit)
{
	BUG_ON(bit < 0 || bit > 61);
	return tegra_fuse_readl(FUSE_SPARE_BIT + bit * 4);
}

int tegra_gpu_register_sets(void)
{
#ifdef CONFIG_ARCH_TEGRA_HAS_DUAL_3D
	u32 reg = readl(IO_TO_VIRT(TEGRA_CLK_RESET_BASE + FUSE_GPU_INFO));
	if (reg & FUSE_GPU_INFO_MASK)
		return 1;
	else
		return 2;
#else
	return 1;
#endif
}

void tegra_gpu_get_info(struct gpu_info *pInfo)
{
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11) {
		pInfo->num_pixel_pipes = 4;
		pInfo->num_alus_per_pixel_pipe = 3;
	} else {
		pInfo->num_pixel_pipes = 1;
		pInfo->num_alus_per_pixel_pipe = 1;
	}
}

static int get_gpu_num_pixel_pipes(char *val, const struct kernel_param *kp)
{
	struct gpu_info gpu_info;

	tegra_gpu_get_info(&gpu_info);
	tegra_gpu_num_pixel_pipes = gpu_info.num_pixel_pipes;
	return param_get_uint(val, kp);
}

static int get_gpu_num_alus_per_pixel_pipe(char *val,
						const struct kernel_param *kp)
{
	struct gpu_info gpu_info;

	tegra_gpu_get_info(&gpu_info);
	tegra_gpu_num_alus_per_pixel_pipe = gpu_info.num_alus_per_pixel_pipe;

	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_gpu_num_pixel_pipes_ops = {
	.get = get_gpu_num_pixel_pipes,
};

static struct kernel_param_ops tegra_gpu_num_alus_per_pixel_pipe_ops = {
	.get = get_gpu_num_alus_per_pixel_pipe,
};

module_param_cb(tegra_gpu_num_pixel_pipes, &tegra_gpu_num_pixel_pipes_ops,
		&tegra_gpu_num_pixel_pipes, 0444);
module_param_cb(tegra_gpu_num_alus_per_pixel_pipe,
		&tegra_gpu_num_alus_per_pixel_pipe_ops,
		&tegra_gpu_num_alus_per_pixel_pipe, 0444);

#if defined(CONFIG_TEGRA_SILICON_PLATFORM)
struct chip_revision {
	enum tegra_chipid	chipid;
	unsigned int		major;
	unsigned int		minor;
	char			prime;
	enum tegra_revision	revision;
};

#define CHIP_REVISION(id, m, n, p, rev) {	\
	.chipid = TEGRA_CHIPID_##id,		\
	.major = m,				\
	.minor = n,				\
	.prime = p,				\
	.revision = TEGRA_REVISION_##rev }

static struct chip_revision tegra_chip_revisions[] = {
	CHIP_REVISION(TEGRA2,  1, 2, 0,   A02),
	CHIP_REVISION(TEGRA2,  1, 3, 0,   A03),
	CHIP_REVISION(TEGRA2,  1, 3, 'p', A03p),
	CHIP_REVISION(TEGRA2,  1, 4, 0,   A04),
	CHIP_REVISION(TEGRA2,  1, 4, 'p', A04p),
	CHIP_REVISION(TEGRA3,  1, 1, 0,   A01),
	CHIP_REVISION(TEGRA3,  1, 2, 0,   A02),
	CHIP_REVISION(TEGRA3,  1, 3, 0,   A03),
	CHIP_REVISION(TEGRA11, 1, 1, 0,   A01),
	CHIP_REVISION(TEGRA11, 1, 2, 0,   A02),
};
#endif

static enum tegra_revision tegra_decode_revision(const struct tegra_id *id)
{
	enum tegra_revision revision = TEGRA_REVISION_UNKNOWN;

#if defined(CONFIG_TEGRA_SILICON_PLATFORM)
	int i ;
	char prime;

	if (id->priv == NULL)
		prime = 0;
	else
		prime = *(id->priv);

	for (i = 0; i < ARRAY_SIZE(tegra_chip_revisions); i++) {
		if ((id->chipid != tegra_chip_revisions[i].chipid) ||
		    (id->minor != tegra_chip_revisions[i].minor) ||
		    (id->major != tegra_chip_revisions[i].major) ||
		    (prime != tegra_chip_revisions[i].prime))
			continue;

		revision = tegra_chip_revisions[i].revision;
		break;
	}

#elif defined(CONFIG_TEGRA_FPGA_PLATFORM)
	if (id->major == 0) {
		if (id->minor == 1)
			revision = TEGRA_REVISION_A01;
		else
			revision = TEGRA_REVISION_QT;
	}
#elif defined(CONFIG_TEGRA_SIMULATION_PLATFORM)
	if ((id->chipid & 0xff) == TEGRA_CHIPID_TEGRA11)
		revision = TEGRA_REVISION_A01;
#endif

	return revision;
}

static void tegra_set_tegraid(u32 chipid,
					u32 major, u32 minor,
					u32 nlist, u32 patch, const char *priv)
{
	tegra_id.chipid  = (enum tegra_chipid) chipid;
	tegra_id.major   = major;
	tegra_id.minor   = minor;
	tegra_id.netlist = nlist;
	tegra_id.patch   = patch;
	tegra_id.priv    = (char *)priv;
	tegra_id.revision = tegra_decode_revision(&tegra_id);
}

static void tegra_get_tegraid_from_hw(void)
{
	void __iomem *chip_id = IO_ADDRESS(TEGRA_APB_MISC_BASE) + 0x804;
	void __iomem *netlist = IO_ADDRESS(TEGRA_APB_MISC_BASE) + 0x860;
	u32 cid = readl(chip_id);
	u32 nlist = readl(netlist);
	char *priv = NULL;

	tegra_fuse_get_priv(priv);
	tegra_set_tegraid((cid >> 8) & 0xff,
			  (cid >> 4) & 0xf,
			  (cid >> 16) & 0xf,
			  (nlist >> 0) & 0xffff,
			  (nlist >> 16) & 0xffff,
			  priv);
}

enum tegra_chipid tegra_get_chipid(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();

	return tegra_id.chipid;
}

enum tegra_revision tegra_get_revision(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();

	return tegra_id.revision;
}

unsigned int tegra_get_minor_rev(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();

	return tegra_id.minor;
}

#ifdef CONFIG_TEGRA_PRE_SILICON_SUPPORT
void tegra_get_netlist_revision(u32 *netlist, u32 *patchid)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN) {
		/* Boot loader did not pass a valid chip ID.
		 * Get it from hardware */
		tegra_get_tegraid_from_hw();
	}
	*netlist = tegra_id.netlist;
	*patchid = tegra_id.patch & 0xF;
}
#endif

static int get_chip_id(char *val, const struct kernel_param *kp)
{
	return param_get_uint(val, kp);
}

static int get_revision(char *val, const struct kernel_param *kp)
{
	return param_get_uint(val, kp);
}

static unsigned int get_fuse_vp8_enable(char *val, struct kernel_param *kp)
{
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA2 ||
		 tegra_get_chipid() == TEGRA_CHIPID_TEGRA3)
		tegra_fuse_vp8_enable = 0;
	else
		tegra_fuse_vp8_enable =  tegra_fuse_readl(FUSE_VP8_ENABLE_0);

	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_chip_id_ops = {
	.get = get_chip_id,
};

static struct kernel_param_ops tegra_revision_ops = {
	.get = get_revision,
};

module_param_cb(tegra_chip_id, &tegra_chip_id_ops, &tegra_id.chipid, 0444);
module_param_cb(tegra_chip_rev, &tegra_revision_ops, &tegra_id.revision, 0444);

module_param_call(tegra_fuse_vp8_enable, NULL, get_fuse_vp8_enable,
		&tegra_fuse_vp8_enable, 0444);
__MODULE_PARM_TYPE(tegra_fuse_vp8_enable, "uint");

static void wait_for_idle(void)
{
	u32 reg;

	do {
		udelay(1);
		reg = tegra_fuse_readl(FUSE_CTRL);
	} while ((reg & (0xF << 16)) != STATE_IDLE);
}

static u32 fuse_cmd_read(u32 addr)
{
	u32 reg;

	wait_for_idle();
	tegra_fuse_writel(addr, FUSE_REG_ADDR);
	reg = tegra_fuse_readl(FUSE_CTRL);
	reg &= ~FUSE_CMD_MASK;
	reg |= FUSE_READ;
	tegra_fuse_writel(reg, FUSE_CTRL);
	wait_for_idle();

	reg = tegra_fuse_readl(FUSE_REG_READ);
	return reg;
}

static void fuse_cmd_write(u32 value, u32 addr)
{
	u32 reg;

	wait_for_idle();
	tegra_fuse_writel(addr, FUSE_REG_ADDR);
	tegra_fuse_writel(value, FUSE_REG_WRITE);

	reg = tegra_fuse_readl(FUSE_CTRL);
	reg &= ~FUSE_CMD_MASK;
	reg |= FUSE_WRITE;
	tegra_fuse_writel(reg, FUSE_CTRL);
	wait_for_idle();
}

static void fuse_cmd_sense(void)
{
	u32 reg;

	wait_for_idle();
	reg = tegra_fuse_readl(FUSE_CTRL);
	reg &= ~FUSE_CMD_MASK;
	reg |= FUSE_SENSE;
	tegra_fuse_writel(reg, FUSE_CTRL);
	wait_for_idle();
}

static void get_fuse(enum fuse_io_param io_param, u32 *out)
{
	int start_bit = fuse_info_tbl[io_param].start_bit;
	int nbits = fuse_info_tbl[io_param].nbits;
	int offset = fuse_info_tbl[io_param].start_off;
	u32 *dst = fuse_info_tbl[io_param].addr;
	int dst_bit = 0;
	int i;
	u32 val;
	int loops;

	if (out)
		dst = out;

	do {
		val = fuse_cmd_read(offset);
		loops = min(nbits, 32 - start_bit);
		for (i = 0; i < loops; i++) {
			if (val & (BIT(start_bit + i)))
				*dst |= BIT(dst_bit);
			else
				*dst &= ~BIT(dst_bit);
			dst_bit++;
			if (dst_bit == 32) {
				dst++;
				dst_bit = 0;
			}
		}
		nbits -= loops;
		offset += 2;
		start_bit = 0;
	} while (nbits > 0);
}

int tegra_fuse_read(enum fuse_io_param io_param, u32 *data, int size)
{
	int nbits;
	u32 sbk[4], devkey = 0;

	if (IS_ERR_OR_NULL(clk_fuse)) {
		pr_err("fuse read disabled");
		return -ENODEV;
	}

	if (!data)
		return -EINVAL;

	if (size != fuse_info_tbl[io_param].sz) {
		pr_err("%s: size mismatch(%d), %d vs %d\n", __func__,
			(int)io_param, size, fuse_info_tbl[io_param].sz);
		return -EINVAL;
	}

	mutex_lock(&fuse_lock);

	clk_enable(clk_fuse);
	fuse_cmd_sense();

	if (io_param == SBK_DEVKEY_STATUS) {
		*data = 0;

		get_fuse(SBK, sbk);
		get_fuse(DEVKEY, &devkey);
		nbits = sizeof(sbk) * BITS_PER_BYTE;
		if (find_first_bit((unsigned long *)sbk, nbits) != nbits)
			*data = 1;
		else if (devkey)
			*data = 1;
	} else {
		get_fuse(io_param, data);
	}

	clk_disable(clk_fuse);
	mutex_unlock(&fuse_lock);

	return 0;
}

static bool fuse_odm_prod_mode(void)
{
	u32 odm_prod_mode = 0;

	clk_enable(clk_fuse);
	get_fuse(ODM_PROD_MODE, &odm_prod_mode);
	clk_disable(clk_fuse);
	return odm_prod_mode ? true : false;
}

static void set_fuse(enum fuse_io_param io_param, u32 *data)
{
	int i, start_bit = fuse_info_tbl[io_param].start_bit;
	int nbits = fuse_info_tbl[io_param].nbits, loops;
	int offset = fuse_info_tbl[io_param].start_off >> 1;
	int src_bit = 0;
	u32 val;

	do {
		val = *data;
		loops = min(nbits, 32 - start_bit);
		for (i = 0; i < loops; i++) {
			fuse_pgm_mask[offset] |= BIT(start_bit + i);
			if (val & BIT(src_bit))
				fuse_pgm_data[offset] |= BIT(start_bit + i);
			else
				fuse_pgm_data[offset] &= ~BIT(start_bit + i);
			src_bit++;
			if (src_bit == 32) {
				data++;
				val = *data;
				src_bit = 0;
			}
		}
		nbits -= loops;
		offset++;
		start_bit = 0;
	} while (nbits > 0);
}

static void populate_fuse_arrs(struct fuse_data *info, u32 flags)
{
	u32 *src = (u32 *)info;
	int i;

	memset(fuse_pgm_data, 0, sizeof(fuse_pgm_data));
	memset(fuse_pgm_mask, 0, sizeof(fuse_pgm_mask));

	if ((flags & FLAGS_ODMRSVD)) {
		set_fuse(ODM_RSVD, info->odm_rsvd);
		flags &= ~FLAGS_ODMRSVD;
	}

	/* do not burn any more if secure mode is set */
	if (fuse_odm_prod_mode())
		goto out;

	for_each_set_bit(i, (unsigned long *)&flags, MAX_PARAMS)
		set_fuse(i, src + fuse_info_tbl[i].data_offset);

out:
	pr_debug("ready to program");
}

static void fuse_power_enable(void)
{
	tegra_fuse_writel(0x1, FUSE_PWR_GOOD_SW);
	udelay(1);
}

static void fuse_power_disable(void)
{
	tegra_fuse_writel(0, FUSE_PWR_GOOD_SW);
	udelay(1);
}

static void fuse_program_array(int pgm_cycles)
{
	u32 reg, fuse_val[2];
	u32 *data = tmp_fuse_pgm_data, addr = 0, *mask = fuse_pgm_mask;
	int i = 0;

	fuse_cmd_sense();

	/* get the first 2 fuse bytes */
	fuse_val[0] = fuse_cmd_read(0);
	fuse_val[1] = fuse_cmd_read(1);

	fuse_power_enable();

	/*
	 * The fuse macro is a high density macro. Fuses are
	 * burned using an addressing mechanism, so no need to prepare
	 * the full list, but more write to control registers are needed.
	 * The only bit that can be written at first is bit 0, a special write
	 * protection bit by assumptions all other bits are at 0
	 *
	 * The programming pulse must have a precise width of
	 * [9000, 11000] ns.
	 */
	if (pgm_cycles > 0) {
		reg = pgm_cycles;
		tegra_fuse_writel(reg, FUSE_TIME_PGM2);
	}
	fuse_val[0] = (0x1 & ~fuse_val[0]);
	fuse_val[1] = (0x1 & ~fuse_val[1]);
	fuse_cmd_write(fuse_val[0], 0);
	fuse_cmd_write(fuse_val[1], 1);

	fuse_power_disable();

	/*
	 * this will allow programming of other fuses
	 * and the reading of the existing fuse values
	 */
	fuse_cmd_sense();

	/* Clear out all bits that have already been burned or masked out */
	memcpy(data, fuse_pgm_data, sizeof(fuse_pgm_data));

	for (addr = 0; addr < NFUSES; addr += 2, data++, mask++) {
		reg = fuse_cmd_read(addr);
		pr_debug("%d: 0x%x 0x%x 0x%x\n", addr, (u32)(*data),
			~reg, (u32)(*mask));
		*data = (*data & ~reg) & *mask;
	}

	fuse_power_enable();

	/*
	 * Finally loop on all fuses, program the non zero ones.
	 * Words 0 and 1 are written last and they contain control fuses. We
	 * need to invalidate after writing to a control word (with the
	 * exception of the master enable). This is also the reason we write
	 * them last.
	 */
	for (i = ARRAY_SIZE(fuse_pgm_data) - 1; i >= 0; i--) {
		if (tmp_fuse_pgm_data[i]) {
			fuse_cmd_write(tmp_fuse_pgm_data[i], i * 2);
			fuse_cmd_write(tmp_fuse_pgm_data[i], (i * 2) + 1);
		}

		if (i < 2) {
			wait_for_idle();
			fuse_power_disable();
			fuse_cmd_sense();
			fuse_power_enable();
		}
	}

	fuse_power_disable();

	/*
	 * Wait until done (polling)
	 * this one needs to use fuse_sense done, the FSM follows a periodic
	 * sequence that includes idle
	 */
	do {
		udelay(1);
		reg = tegra_fuse_readl(FUSE_CTRL);
	} while ((reg & BIT(30)) != SENSE_DONE);

}

static int fuse_set(enum fuse_io_param io_param, u32 *param, int size)
{
	int i, nwords = size / sizeof(u32);
	u32 *data;

	if (io_param > MAX_PARAMS)
		return -EINVAL;

	data = kzalloc(size, GFP_KERNEL);
	if (!data) {
		pr_err("failed to alloc %d bytes\n", size);
		return -ENOMEM;
	}

	get_fuse(io_param, data);

	/* set only new fuse bits */
	for (i = 0; i < nwords; i++)
		param[i] = (~data[i] & param[i]);

	kfree(data);
	return 0;
}

/*
 * Function pointer to optional board specific function
 */
int (*tegra_fuse_regulator_en)(int);
EXPORT_SYMBOL(tegra_fuse_regulator_en);


int tegra_fuse_program(struct fuse_data *pgm_data, u32 flags)
{
	u32 reg;
	int i = 0;
	int index;
	int ret;
	int delay = FUSE_PGM_TIMEOUT_MS;

	if (!pgm_data || !flags) {
		pr_err("invalid parameter");
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(clk_fuse) ||
	   (!tegra_fuse_regulator_en && IS_ERR_OR_NULL(fuse_regulator))) {
		pr_err("fuse write disabled");
		return -ENODEV;
	}

	if (fuse_odm_prod_mode() && (flags != FLAGS_ODMRSVD)) {
		pr_err("Non ODM reserved fuses cannot be burnt after ODM"
			"production mode/secure mode fuse is burnt");
		return -EPERM;
	}

	if ((flags & FLAGS_ODM_PROD_MODE) &&
		(flags & (FLAGS_SBK | FLAGS_DEVKEY))) {
		pr_err("odm production mode and sbk/devkey not allowed");
		return -EPERM;
	}

	/* calculate the number of program cycles from the oscillator freq */
	reg = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_PLLP_OVERRIDE);
	if (reg & PMC_OSC_OVERRIDE) {
		index = (reg & PMC_OSC_FREQ_MASK) >> PMC_OSC_FREQ_SHIFT;
	} else {
		reg = readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + CAR_OSC_CTRL);
		index = reg >> CAR_OSC_FREQ_SHIFT;
	}

	pr_debug("%s: use %d programming cycles\n", __func__,
						fuse_pgm_cycles[index]);
	if (fuse_pgm_cycles[index] == 0)
		return -EPERM;

	clk_enable(clk_fuse);

	/* check that fuse options write access hasn't been disabled */
	mutex_lock(&fuse_lock);
	reg = tegra_fuse_readl(FUSE_DIS_PGM);
	mutex_unlock(&fuse_lock);
	if (reg) {
		pr_err("fuse programming disabled");
		clk_disable(clk_fuse);
		return -EACCES;
	}

	/* enable software writes to the fuse registers */
	tegra_fuse_writel(0, FUSE_WRITE_ACCESS);

	mutex_lock(&fuse_lock);
	memcpy(&fuse_info, pgm_data, sizeof(fuse_info));
	for_each_set_bit(i, (unsigned long *)&flags, MAX_PARAMS) {
		fuse_set((u32)i, fuse_info_tbl[i].addr,
			fuse_info_tbl[i].sz);
	}

	if (tegra_fuse_regulator_en)
		ret = tegra_fuse_regulator_en(1);
	else
		ret = regulator_enable(fuse_regulator);

	if (ret)
		BUG_ON("regulator enable fail\n");

	populate_fuse_arrs(&fuse_info, flags);

	/* FIXME: Ideally, this delay should not be present */
	mdelay(1);

	fuse_program_array(fuse_pgm_cycles[index]);

	memset(&fuse_info, 0, sizeof(fuse_info));

	if (tegra_fuse_regulator_en)
		tegra_fuse_regulator_en(0);
	else
		regulator_disable(fuse_regulator);

	mutex_unlock(&fuse_lock);

	/* disable software writes to the fuse registers */
	tegra_fuse_writel(1, FUSE_WRITE_ACCESS);

	if (!tegra_apply_fuse()) {

		fuse_cmd_sense();
		tegra_fuse_writel(START_DATA | SKIP_RAMREPAIR, FUSE_PRIV2INTFC);

		/* check sense and shift done in addition to IDLE */
		do {
			mdelay(1);
			reg = tegra_fuse_readl(FUSE_CTRL);
			reg &= (FUSE_SENSE_DONE_BIT | STATE_IDLE);
		} while ((reg != (FUSE_SENSE_DONE_BIT | STATE_IDLE))
						&& (--delay > 0));
	}

	clk_disable(clk_fuse);

	return ((delay > 0) ? 0 : -ETIMEDOUT);
}

static int fuse_name_to_param(const char *str)
{
	int i;

	for (i = DEVKEY; i < ARRAY_SIZE(fuse_info_tbl); i++) {
		if (!strcmp(str, fuse_info_tbl[i].sysfs_name))
			return i;
	}

	return -ENODATA;
}

static int char_to_xdigit(char c)
{
	return (c >= '0' && c <= '9') ? c - '0' :
		(c >= 'a' && c <= 'f') ? c - 'a' + 10 :
		(c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
}

#define CHK_ERR(x) \
{ \
	if (x) { \
		pr_err("%s: sysfs_create_file fail(%d)!", __func__, x); \
		return x; \
	} \
}

ssize_t tegra_fuse_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	enum fuse_io_param param = fuse_name_to_param(attr->attr.name);
	int ret, i = 0;
	int orig_count = count;
	struct fuse_data data = {0};
	u32 *raw_data;
	u8 *raw_byte_data;
	struct wake_lock fuse_wk_lock;

	if ((param == -1) || (param == -ENODATA)) {
		pr_err("%s: invalid fuse\n", __func__);
		return -EINVAL;
	}

	raw_data = ((u32 *)&data) + fuse_info_tbl[param].data_offset;
	raw_byte_data = (u8 *)raw_data;

	if (fuse_odm_prod_mode() && (param != ODM_RSVD)) {
		pr_err("%s: Non ODM reserved fuses cannot be burnt after"
		"ODM production mode/secure mode fuse is burnt\n", __func__);
		return -EPERM;
	}

	count--;
	if (DIV_ROUND_UP(count, 2) > fuse_info_tbl[param].sz) {
		pr_err("%s: fuse parameter too long, should be %d character(s)\n",
			__func__, fuse_info_tbl[param].sz * 2);
		return -EINVAL;
	}

	/* see if the string has 0x/x at the start */
	if (*buf == 'x') {
		count -= 1;
		buf++;
	} else if (*(buf + 1) == 'x') {
		count -= 2;
		buf += 2;
	}

	/* wakelock to avoid device powering down while programming */
	wake_lock_init(&fuse_wk_lock, WAKE_LOCK_SUSPEND, "fuse_wk_lock");
	wake_lock(&fuse_wk_lock);

	/* we need to fit each character into a single nibble */
	raw_byte_data += DIV_ROUND_UP(count, 2) - 1;

	/* in case of odd number of writes, write the first one here */
	if (count & BIT(0)) {
		*raw_byte_data = char_to_xdigit(*buf);
		buf++;
		raw_byte_data--;
		count--;
	}

	for (i = 1; i <= count; i++, buf++) {
		if (i & BIT(0)) {
			*raw_byte_data = char_to_xdigit(*buf);
		} else {
			*raw_byte_data <<= 4;
			*raw_byte_data |= char_to_xdigit(*buf);
			raw_byte_data--;
		}
	}

	ret = tegra_fuse_program(&data, BIT(param));
	if (ret) {
		pr_err("%s: fuse program fail(%d)\n", __func__, ret);
		orig_count = ret;
		goto done;
	}

	/* if odm prodn mode fuse is burnt, change file permissions to 0440 */
	if (param == ODM_PROD_MODE) {
		CHK_ERR(sysfs_chmod_file(&dev->kobj, &attr->attr, 0440));
		CHK_ERR(sysfs_chmod_file(&dev->kobj, &dev_attr_device_key.attr,
								0440));
		CHK_ERR(sysfs_chmod_file(&dev->kobj,
					&dev_attr_jtag_disable.attr, 0440));
		CHK_ERR(sysfs_chmod_file(&dev->kobj,
					&dev_attr_sec_boot_dev_cfg.attr, 0440));
		CHK_ERR(sysfs_chmod_file(&dev->kobj,
					&dev_attr_sec_boot_dev_sel.attr, 0440));
		CHK_ERR(sysfs_chmod_file(&dev->kobj,
					&dev_attr_secure_boot_key.attr, 0440));
		CHK_ERR(sysfs_chmod_file(&dev->kobj,
					&dev_attr_sw_reserved.attr, 0440));
		CHK_ERR(sysfs_chmod_file(&dev->kobj,
				&dev_attr_ignore_dev_sel_straps.attr, 0440));
		tegra_fuse_ch_sysfs_perm(&dev->kobj);
	}


done:
	wake_unlock(&fuse_wk_lock);
	wake_lock_destroy(&fuse_wk_lock);
	return orig_count;
}

ssize_t tegra_fuse_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	enum fuse_io_param param = fuse_name_to_param(attr->attr.name);
	u32 data[8];
	char str[9]; /* extra byte for null character */
	int ret, i;

	if ((param == -1) || (param == -ENODATA)) {
		pr_err("%s: invalid fuse\n", __func__);
		return -EINVAL;
	}

	if ((param == SBK) && fuse_odm_prod_mode()) {
		pr_err("device locked. sbk read not allowed\n");
		return 0;
	}

	memset(data, 0, sizeof(data));
	ret = tegra_fuse_read(param, data, fuse_info_tbl[param].sz);
	if (ret) {
		pr_err("%s: read fail(%d)\n", __func__, ret);
		return ret;
	}

	strcpy(buf, "0x");
	for (i = (fuse_info_tbl[param].sz/sizeof(u32)) - 1; i >= 0 ; i--) {
		sprintf(str, "%08x", data[i]);
		strcat(buf, str);
	}

	strcat(buf, "\n");
	return strlen(buf);
}

void tegra_init_fuse(void)
{
	u32 id;

	u32 reg = readl(IO_TO_VIRT(TEGRA_CLK_RESET_BASE + 0x48));
	reg |= BIT(28);
	writel(reg, IO_TO_VIRT(TEGRA_CLK_RESET_BASE + 0x48));

	reg = tegra_fuse_readl(FUSE_SKU_INFO);
	tegra_sku_id = reg & 0xFF;

	reg = tegra_apb_readl(TEGRA_APB_MISC_BASE + STRAP_OPT);
	tegra_bct_strapping = (reg & RAM_ID_MASK) >> RAM_CODE_SHIFT;

	id = readl_relaxed(IO_ADDRESS(TEGRA_APB_MISC_BASE) + 0x804);
	tegra_chip_id = (id >> 8) & 0xff;

	tegra_revision = tegra_get_revision();

	tegra_init_speedo_data();

	pr_info("Tegra Revision: %s SKU: 0x%x CPU Process: %d Core Process: %d\n",
		tegra_revision_name[tegra_revision],
		tegra_sku_id, tegra_cpu_process_id(),
		tegra_core_process_id());
}

static int tegra_fuse_probe(struct platform_device *pdev)
{
	if (!tegra_fuse_regulator_en) {
		/* get fuse_regulator regulator */
		fuse_regulator = regulator_get(&pdev->dev, TEGRA_FUSE_SUPPLY);
		if (IS_ERR_OR_NULL(fuse_regulator))
			pr_err("%s: no fuse_regulator. fuse write disabled\n",
				__func__);
	}

	clk_fuse = clk_get_sys("fuse-tegra", "fuse_burn");
	if (IS_ERR_OR_NULL(clk_fuse)) {
		pr_err("%s: no clk_fuse. fuse read/write disabled\n", __func__);
		if (!IS_ERR_OR_NULL(fuse_regulator)) {
			regulator_put(fuse_regulator);
			fuse_regulator = NULL;
		}
		return -ENODEV;
	}

	mutex_init(&fuse_lock);

	/* change fuse file permissions, if ODM production fuse is not blown */
	if (!fuse_odm_prod_mode()) {
		dev_attr_device_key.attr.mode = 0640;
		dev_attr_jtag_disable.attr.mode = 0640;
		dev_attr_sec_boot_dev_cfg.attr.mode = 0640;
		dev_attr_sec_boot_dev_sel.attr.mode = 0640;
		dev_attr_secure_boot_key.attr.mode = 0640;
		dev_attr_sw_reserved.attr.mode = 0640;
		dev_attr_ignore_dev_sel_straps.attr.mode = 0640;
		dev_attr_odm_production_mode.attr.mode = 0644;
	}
	dev_attr_odm_reserved.attr.mode = 0640;

	CHK_ERR(sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_odm_production_mode.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj, &dev_attr_device_key.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj,
						&dev_attr_jtag_disable.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_sec_boot_dev_cfg.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_sec_boot_dev_sel.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_secure_boot_key.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj,
					&dev_attr_sw_reserved.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_ignore_dev_sel_straps.attr));
	CHK_ERR(sysfs_create_file(&pdev->dev.kobj,
					&dev_attr_odm_reserved.attr));
	tegra_fuse_add_sysfs_variables(pdev, fuse_odm_prod_mode());
	return 0;
}

static int tegra_fuse_remove(struct platform_device *pdev)
{
	fuse_power_disable();

	if (!IS_ERR_OR_NULL(fuse_regulator))
		regulator_put(fuse_regulator);

	if (!IS_ERR_OR_NULL(clk_fuse))
		clk_put(clk_fuse);

	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_odm_production_mode.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_device_key.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_jtag_disable.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_sec_boot_dev_cfg.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_sec_boot_dev_sel.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_secure_boot_key.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_sw_reserved.attr);
	sysfs_remove_file(&pdev->dev.kobj,
				&dev_attr_ignore_dev_sel_straps.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_sw_reserved.attr);
	tegra_fuse_rm_sysfs_variables(pdev);
	return 0;
}

static int tegra_fuse_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int tegra_fuse_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver fuse_driver = {
	.probe = tegra_fuse_probe,
	.remove = tegra_fuse_remove,
	.suspend = tegra_fuse_suspend,
	.resume = tegra_fuse_resume,
	.driver = {
			.name = "tegra-fuse",
			.owner = THIS_MODULE,
		},
};

static int __init tegra_fuse_init(void)
{
	return platform_driver_register(&fuse_driver);
}
fs_initcall_sync(tegra_fuse_init);

static void __exit tegra_fuse_exit(void)
{
	platform_driver_unregister(&fuse_driver);
}
module_exit(tegra_fuse_exit);

MODULE_DESCRIPTION("Fuse driver for tegra SOCs");
