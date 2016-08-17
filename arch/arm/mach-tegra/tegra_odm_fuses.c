/*
 * arch/arm/mach-tegra/tegra_odm_fuses.c
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * Fuses are one time programmable bits on the chip which are used by
 * the chip manufacturer and device manufacturers to store chip/device
 * configurations. The fuse bits are encapsulated in a 32 x 64 array.
 * If a fuse bit is programmed to 1, it cannot be reverted to 0. Either
 * another fuse bit has to be used for the same purpose or a new chip
 * needs to be used.
 *
 * Each and every fuse word has its own shadow word which resides adjacent to
 * a particular fuse word. e.g. Fuse words 0-1 form a fuse-shadow pair.
 * So in theory we have only 32 fuse words to work with.
 * The shadow fuse word is a mirror of the actual fuse word at all times
 * and this is maintained while programming a particular fuse.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/regulator/consumer.h>
#include <linux/ctype.h>
#include <linux/wakelock.h>
#include <linux/clk.h>
#include <linux/export.h>

#include <mach/tegra_odm_fuses.h>
#include <mach/iomap.h>
#include "fuse.h"

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
#include "tegra2_fuse_offsets.h"
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
#include "tegra3_fuse_offsets.h"
#else
#include "tegra11x_fuse_offsets.h"
#endif

#define NFUSES	64
#define STATE_IDLE	(0x4 << 16)
#define SENSE_DONE	(0x1 << 30)

/* since fuse burning is irreversible, use this for testing */
#define ENABLE_FUSE_BURNING 1

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

static struct kobject *fuse_kobj;

static ssize_t fuse_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t fuse_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count);

static struct kobj_attribute devkey_attr =
	__ATTR(device_key, 0440, fuse_show, fuse_store);

static struct kobj_attribute jtagdis_attr =
	__ATTR(jtag_disable, 0440, fuse_show, fuse_store);

static struct kobj_attribute odm_prod_mode_attr =
	__ATTR(odm_production_mode, 0444, fuse_show, fuse_store);

static struct kobj_attribute sec_boot_dev_cfg_attr =
	__ATTR(sec_boot_dev_cfg, 0440, fuse_show, fuse_store);

static struct kobj_attribute sec_boot_dev_sel_attr =
	__ATTR(sec_boot_dev_sel, 0440, fuse_show, fuse_store);

static struct kobj_attribute sbk_attr =
	__ATTR(secure_boot_key, 0440, fuse_show, fuse_store);

static struct kobj_attribute sw_rsvd_attr =
	__ATTR(sw_reserved, 0440, fuse_show, fuse_store);

static struct kobj_attribute ignore_dev_sel_straps_attr =
	__ATTR(ignore_dev_sel_straps, 0440, fuse_show, fuse_store);

static struct kobj_attribute odm_rsvd_attr =
	__ATTR(odm_reserved, 0440, fuse_show, fuse_store);

static u32 fuse_pgm_data[NFUSES / 2];
static u32 fuse_pgm_mask[NFUSES / 2];
static u32 tmp_fuse_pgm_data[NFUSES / 2];

DEFINE_MUTEX(fuse_lock);

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define TEGRA_FUSE_SUPPLY	"vdd_fuse"
#else
#define TEGRA_FUSE_SUPPLY	"vpp_fuse"
#endif

static struct fuse_data fuse_info;
struct regulator *fuse_regulator;
struct clk *clk_fuse;

#define FUSE_NAME_LEN	30

struct param_info {
	u32 *addr;
	int sz;
	u32 start_off;
	int start_bit;
	int nbits;
	int data_offset;
	char sysfs_name[FUSE_NAME_LEN];
};

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
	[SBK_DEVKEY_STATUS] = {
		.sz = SBK_DEVKEY_STATUS_SZ,
	},
};

static void wait_for_idle(void)
{
	u32 reg;

	do {
		udelay(1);
		reg = tegra_fuse_readl(FUSE_CTRL);
	} while ((reg & (0xF << 16)) != STATE_IDLE);
}

#define FUSE_READ	0x1
#define FUSE_WRITE	0x2
#define FUSE_SENSE	0x3
#define FUSE_CMD_MASK	0x3

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

	clk_prepare_enable(clk_fuse);
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

	clk_disable_unprepare(clk_fuse);
	mutex_unlock(&fuse_lock);

	return 0;
}

static bool fuse_odm_prod_mode(void)
{
	u32 odm_prod_mode = 0;

	clk_prepare_enable(clk_fuse);
	get_fuse(ODM_PROD_MODE, &odm_prod_mode);
	clk_disable_unprepare(clk_fuse);
	return (odm_prod_mode ? true : false);
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
#if ENABLE_FUSE_BURNING
	tegra_fuse_writel(0x1, FUSE_PWR_GOOD_SW);
	udelay(1);
#endif
}

static void fuse_power_disable(void)
{
#if ENABLE_FUSE_BURNING
	tegra_fuse_writel(0, FUSE_PWR_GOOD_SW);
	udelay(1);
#endif
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
	 * need to invalidate after writing to a control word (with the exception
	 * of the master enable). This is also the reason we write them last.
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
	} while ((reg & (0x1 << 30)) != SENSE_DONE);

}

static int fuse_set(enum fuse_io_param io_param, u32 *param, int size)
{
	int i, nwords = size / sizeof(u32);
	u32 *data;

	if (io_param > MAX_PARAMS)
		return -EINVAL;

	data = (u32*)kzalloc(size, GFP_KERNEL);
	if (!data) {
		pr_err("failed to alloc %d bytes\n", size);
		return -ENOMEM;
	}

	get_fuse(io_param, data);

	/* set only new fuse bits */
	for (i = 0; i < nwords; i++) {
		param[i] = (~data[i] & param[i]);
	}

	kfree(data);
	return 0;
}

/*
 * Function pointer to optional board specific function
 */
int (*tegra_fuse_regulator_en)(int);
EXPORT_SYMBOL(tegra_fuse_regulator_en);

#define CAR_OSC_CTRL		0x50
#define PMC_PLLP_OVERRIDE	0xF8
#define PMC_OSC_OVERRIDE	BIT(0)
#define PMC_OSC_FREQ_MASK	(BIT(2) | BIT(3))
#define PMC_OSC_FREQ_SHIFT	2
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
#define CAR_OSC_FREQ_SHIFT	30
#else
#define CAR_OSC_FREQ_SHIFT	28
#endif

#define FUSE_SENSE_DONE_BIT	BIT(30)
#define START_DATA		BIT(0)
#define SKIP_RAMREPAIR		BIT(1)
#define FUSE_PGM_TIMEOUT_MS	50

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
/* cycles corresponding to 13MHz, 19.2MHz, 12MHz, 26MHz */
static int fuse_pgm_cycles[] = {130, 192, 120, 260};
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
/* cycles corresponding to 13MHz, 16.8MHz, 19.2MHz, 38.4MHz, 12MHz, 48MHz, 26MHz */
static int fuse_pgm_cycles[] = {130, 168, 0, 0, 192, 384, 0, 0, 120, 480, 0, 0, 260};
#else
/* cycles corresponding to 13MHz, 16.8MHz, 19.2MHz, 38.4MHz, 12MHz, 48MHz, 26MHz */
static int fuse_pgm_cycles[] = {143, 185, 0, 0, 212, 423, 0, 0, 132, 528, 0, 0, 286};
#endif

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
		pr_err("Non ODM reserved fuses cannot be burnt after "
			"ODM production mode/secure mode fuse is burnt");
		return -EPERM;
	}

	if ((flags & FLAGS_ODM_PROD_MODE) &&
		(flags & (FLAGS_SBK | FLAGS_DEVKEY))) {
		pr_err("odm production mode and sbk/devkey not allowed");
		return -EPERM;
	}

	clk_prepare_enable(clk_fuse);

	/* check that fuse options write access hasn't been disabled */
	mutex_lock(&fuse_lock);
	reg = tegra_fuse_readl(FUSE_DIS_PGM);
	mutex_unlock(&fuse_lock);
	if (reg) {
		pr_err("fuse programming disabled");
		clk_disable_unprepare(clk_fuse);
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

#if ENABLE_FUSE_BURNING
	if (tegra_fuse_regulator_en)
		ret = tegra_fuse_regulator_en(1);
	else
		ret = regulator_enable(fuse_regulator);

	if (ret)
		BUG_ON("regulator enable fail\n");

	populate_fuse_arrs(&fuse_info, flags);

	/* calculate the number of program cycles from the oscillator freq */
	reg = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_PLLP_OVERRIDE);
	if (reg & PMC_OSC_OVERRIDE) {
		index = (reg & PMC_OSC_FREQ_MASK) >> PMC_OSC_FREQ_SHIFT;
	} else {
		reg = readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + CAR_OSC_CTRL);
		index = reg >> CAR_OSC_FREQ_SHIFT;
	}

	pr_debug("%s: use %d programming cycles\n", __func__, fuse_pgm_cycles[index]);

	/* FIXME: Ideally, this delay should not be present */
	mdelay(1);

	fuse_program_array(fuse_pgm_cycles[index]);

	memset(&fuse_info, 0, sizeof(fuse_info));

	if (tegra_fuse_regulator_en)
		tegra_fuse_regulator_en(0);
	else
		regulator_disable(fuse_regulator);
#endif

	mutex_unlock(&fuse_lock);

	/* disable software writes to the fuse registers */
	tegra_fuse_writel(1, FUSE_WRITE_ACCESS);

	/* apply the fuse values immediately instead of resetting the chip */
	fuse_cmd_sense();

	tegra_fuse_writel(START_DATA | SKIP_RAMREPAIR, FUSE_PRIV2INTFC);

	/* check sense and shift done in addition to IDLE */
	do {
		mdelay(1);
		reg = tegra_fuse_readl(FUSE_CTRL);
		reg &= (FUSE_SENSE_DONE_BIT | STATE_IDLE);
	} while ((reg != (FUSE_SENSE_DONE_BIT | STATE_IDLE)) && (--delay > 0));

	clk_disable_unprepare(clk_fuse);

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
	return (c>='0' && c<='9') ? c - '0' :
		(c>='a' && c<='f') ? c - 'a' + 10 :
		(c>='A' && c<='F') ? c - 'A' + 10 : -1;
}

#define CHK_ERR(x) \
{ \
	if (x) \
	{ \
		pr_err("%s: sysfs_create_file fail(%d)!", __func__, x); \
		return x; \
	} \
}

static ssize_t fuse_store(struct kobject *kobj, struct kobj_attribute *attr,
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
		pr_err("%s: Non ODM reserved fuses cannot be burnt "
			"after ODM production mode/secure mode fuse is burnt\n"
			, __func__);

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
		CHK_ERR(sysfs_chmod_file(kobj, &attr->attr, 0440));
		CHK_ERR(sysfs_chmod_file(kobj, &devkey_attr.attr, 0440));
		CHK_ERR(sysfs_chmod_file(kobj, &jtagdis_attr.attr, 0440));
		CHK_ERR(sysfs_chmod_file(kobj, &sec_boot_dev_cfg_attr.attr, 0440));
		CHK_ERR(sysfs_chmod_file(kobj, &sec_boot_dev_sel_attr.attr, 0440));
		CHK_ERR(sysfs_chmod_file(kobj, &sbk_attr.attr, 0440));
		CHK_ERR(sysfs_chmod_file(kobj, &sw_rsvd_attr.attr, 0440));
		CHK_ERR(sysfs_chmod_file(kobj, &ignore_dev_sel_straps_attr.attr, 0440));
	}

done:
	wake_unlock(&fuse_wk_lock);
	wake_lock_destroy(&fuse_wk_lock);
	return orig_count;
}

static ssize_t fuse_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
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


static int __init tegra_fuse_program_init(void)
{
	if (!tegra_fuse_regulator_en) {
		/* get fuse_regulator regulator */
		fuse_regulator = regulator_get(NULL, TEGRA_FUSE_SUPPLY);
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

	fuse_kobj = kobject_create_and_add("fuse", firmware_kobj);
	if (!fuse_kobj) {
		pr_err("%s: fuse_kobj create fail\n", __func__);
		regulator_put(fuse_regulator);
		clk_put(clk_fuse);
		return -ENODEV;
	}

	mutex_init(&fuse_lock);

	/* change fuse file permissions, if ODM production fuse is not blown */
	if (!fuse_odm_prod_mode())
	{
		devkey_attr.attr.mode = 0640;
		jtagdis_attr.attr.mode = 0640;
		sec_boot_dev_cfg_attr.attr.mode = 0640;
		sec_boot_dev_sel_attr.attr.mode = 0640;
		sbk_attr.attr.mode = 0640;
		sw_rsvd_attr.attr.mode = 0640;
		ignore_dev_sel_straps_attr.attr.mode = 0640;
		odm_prod_mode_attr.attr.mode = 0644;
	}
	odm_rsvd_attr.attr.mode = 0640;

	CHK_ERR(sysfs_create_file(fuse_kobj, &odm_prod_mode_attr.attr));
	CHK_ERR(sysfs_create_file(fuse_kobj, &devkey_attr.attr));
	CHK_ERR(sysfs_create_file(fuse_kobj, &jtagdis_attr.attr));
	CHK_ERR(sysfs_create_file(fuse_kobj, &sec_boot_dev_cfg_attr.attr));
	CHK_ERR(sysfs_create_file(fuse_kobj, &sec_boot_dev_sel_attr.attr));
	CHK_ERR(sysfs_create_file(fuse_kobj, &sbk_attr.attr));
	CHK_ERR(sysfs_create_file(fuse_kobj, &sw_rsvd_attr.attr));
	CHK_ERR(sysfs_create_file(fuse_kobj, &ignore_dev_sel_straps_attr.attr));
	CHK_ERR(sysfs_create_file(fuse_kobj, &odm_rsvd_attr.attr));

	return 0;
}

static void __exit tegra_fuse_program_exit(void)
{

	fuse_power_disable();

	if (!IS_ERR_OR_NULL(fuse_regulator))
		regulator_put(fuse_regulator);

	if (!IS_ERR_OR_NULL(clk_fuse))
		clk_put(clk_fuse);

	sysfs_remove_file(fuse_kobj, &odm_prod_mode_attr.attr);
	sysfs_remove_file(fuse_kobj, &devkey_attr.attr);
	sysfs_remove_file(fuse_kobj, &jtagdis_attr.attr);
	sysfs_remove_file(fuse_kobj, &sec_boot_dev_cfg_attr.attr);
	sysfs_remove_file(fuse_kobj, &sec_boot_dev_sel_attr.attr);
	sysfs_remove_file(fuse_kobj, &sbk_attr.attr);
	sysfs_remove_file(fuse_kobj, &sw_rsvd_attr.attr);
	sysfs_remove_file(fuse_kobj, &ignore_dev_sel_straps_attr.attr);
	sysfs_remove_file(fuse_kobj, &odm_rsvd_attr.attr);
	kobject_del(fuse_kobj);
}

late_initcall(tegra_fuse_program_init);
module_exit(tegra_fuse_program_exit);
