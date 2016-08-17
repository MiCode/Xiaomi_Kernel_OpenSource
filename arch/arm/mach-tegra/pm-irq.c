/*
 * Copyright (C) 2011 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/seq_file.h>
#include <linux/syscore_ops.h>

#include <mach/iomap.h>

#include "pm-irq.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_LATCH_WAKEUPS	(1 << 5)
#define PMC_WAKE_MASK		0xc
#define PMC_WAKE_LEVEL		0x10
#define PMC_WAKE_STATUS		0x14
#define PMC_SW_WAKE_STATUS	0x18
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
#define PMC_WAKE2_MASK		0x160
#define PMC_WAKE2_LEVEL		0x164
#define PMC_WAKE2_STATUS	0x168
#define PMC_SW_WAKE2_STATUS	0x16C
#endif

#define PMC_MAX_WAKE_COUNT 64

/* wake level/polarity constants */
enum {
	WAKE_LEVEL_LO = 0,
	WAKE_LEVEL_HI,
	WAKE_LEVEL_ANY
};

static void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);

static u64 tegra_lp0_wake_enb;
static u64 tegra_lp0_wake_level;
static u64 tegra_lp0_wake_level_any;
static int tegra_prevent_lp0;

static unsigned int tegra_wake_irq_count[PMC_MAX_WAKE_COUNT];

/*
 * List of internal any wake sources returned from chip-specific
 * implementation of function tegra_get_internal_any_wake_list
 * any_wake_count - size of list
 * any_wake - array of wake index
 * remote_usb_wake_index - index of USB1 remote wake source
 */
static u8 any_wake_count; /* non-zero value indicates any wake support */
static u8 *any_wake;
static u8 remote_usb_wake_index;

#ifdef DEBUG_WAKE_SOURCE
/*
 * define DEBUG_WAKE_SOURCE to enable -
 * Code that uses sysfs nodes to test LP0 wake for wake sources
 * with option to select wake levels: lo-0, hi-1, any-2
 */
static long test_wake_src_index = -1;
static long test_wake_src_polarity;
#endif

static bool debug_lp0;
module_param(debug_lp0, bool, S_IRUGO | S_IWUSR);

static bool warn_prevent_lp0;
module_param(warn_prevent_lp0, bool, S_IRUGO | S_IWUSR);

bool tegra_pm_irq_lp0_allowed(void)
{
	return (tegra_prevent_lp0 == 0);
}

/* ensures that sufficient time is passed for a register write to
 * serialize into the 32KHz domain */
static void pmc_32kwritel(u32 val, unsigned long offs)
{
	writel(val, pmc + offs);
	udelay(130);
}

static inline void write_pmc_wake_mask(u64 value)
{
	pr_info("Wake[31-0] enable=0x%x\n", (u32)(value & 0xFFFFFFFF));
	writel((u32)value, pmc + PMC_WAKE_MASK);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	pr_info("Tegra3 wake[63-32] enable=0x%x\n", (u32)((value >> 32) &
		0xFFFFFFFF));
	__raw_writel((u32)(value >> 32), pmc + PMC_WAKE2_MASK);
#endif
}

static inline u64 read_pmc_wake_level(void)
{
	u64 reg;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	reg = readl(pmc + PMC_WAKE_LEVEL);
#else
	reg = __raw_readl(pmc + PMC_WAKE_LEVEL);
	reg |= ((u64)readl(pmc + PMC_WAKE2_LEVEL)) << 32;
#endif
	return reg;
}

static inline void write_pmc_wake_level(u64 value)
{
	pr_info("Wake[31-0] level=0x%x\n", (u32)(value & 0xFFFFFFFF));
	writel((u32)value, pmc + PMC_WAKE_LEVEL);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	pr_info("Tegra3 wake[63-32] level=0x%x\n", (u32)((value >> 32) &
		0xFFFFFFFF));
	__raw_writel((u32)(value >> 32), pmc + PMC_WAKE2_LEVEL);
#endif
}

static inline u64 read_pmc_wake_status(void)
{
	u64 reg;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	reg = readl(pmc + PMC_WAKE_STATUS);
#else
	reg = __raw_readl(pmc + PMC_WAKE_STATUS);
	reg |= ((u64)readl(pmc + PMC_WAKE2_STATUS)) << 32;
#endif
	return reg;
}

u64 tegra_read_pmc_wake_status(void)
{
	return read_pmc_wake_status();
}

static inline u64 read_pmc_sw_wake_status(void)
{
	u64 reg;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	reg = readl(pmc + PMC_SW_WAKE_STATUS);
#ifdef DEBUG_WAKE_SOURCE
	pr_info("PMC_SW_WAKE_STATUS[31-0] level=0x%x\n",
		(u32)(reg & 0xFFFFFFFF));
#endif
#else
	reg = __raw_readl(pmc + PMC_SW_WAKE_STATUS);
#ifdef DEBUG_WAKE_SOURCE
	pr_info("PMC_SW_WAKE_STATUS[31-0] level=0x%x\n",
		(u32)(reg & 0xFFFFFFFF));
#endif
	reg |= ((u64)readl(pmc + PMC_SW_WAKE2_STATUS)) << 32;
#ifdef DEBUG_WAKE_SOURCE
	pr_info("PMC_SW_WAKE_STATUS[63-32] level=0x%x\n",
		(u32)((reg >> 32) & 0xFFFFFFFF));
#endif
#endif
	return reg;
}

static inline void clear_pmc_sw_wake_status(void)
{
	pmc_32kwritel(0, PMC_SW_WAKE_STATUS);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	pmc_32kwritel(0, PMC_SW_WAKE2_STATUS);
#endif
}

int tegra_pm_irq_set_wake(int wake, int enable)
{
	if (wake < 0)
		return -EINVAL;

	if (enable) {
		tegra_lp0_wake_enb |= 1ull << wake;
		pr_info("Enabling wake%d\n", wake);
	} else {
		tegra_lp0_wake_enb &= ~(1ull << wake);
		pr_info("Disabling wake%d\n", wake);
	}

	return 0;
}

int tegra_pm_irq_set_wake_type(int wake, int flow_type)
{
	if (wake < 0)
		return 0;

	switch (flow_type) {
	case IRQF_TRIGGER_FALLING:
	case IRQF_TRIGGER_LOW:
		tegra_lp0_wake_level &= ~(1ull << wake);
		tegra_lp0_wake_level_any &= ~(1ull << wake);
		break;
	case IRQF_TRIGGER_HIGH:
	case IRQF_TRIGGER_RISING:
		tegra_lp0_wake_level |= (1ull << wake);
		tegra_lp0_wake_level_any &= ~(1ull << wake);
		break;

	case IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING:
		tegra_lp0_wake_level_any |= (1ull << wake);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* translate lp0 wake sources back into irqs to catch edge triggered wakeups */
static void tegra_pm_irq_syscore_resume_helper(
	unsigned long wake_status,
	unsigned int index)
{
	int wake;
	int irq;
	struct irq_desc *desc;

	for_each_set_bit(wake, &wake_status, sizeof(wake_status) * 8) {
		irq = tegra_wake_to_irq(wake + 32 * index);
		if (!irq) {
			pr_info("Resume caused by WAKE%d\n",
				(wake + 32 * index));
			continue;
		}

		desc = irq_to_desc(irq);
		if (!desc || !desc->action || !desc->action->name) {
			pr_info("Resume caused by WAKE%d, irq %d\n",
				(wake + 32 * index), irq);
			continue;
		}

		pr_info("Resume caused by WAKE%d, %s\n", (wake + 32 * index),
			desc->action->name);

		tegra_wake_irq_count[wake + 32 * index]++;

		generic_handle_irq(irq);
	}
}

static void tegra_pm_irq_syscore_resume(void)
{
	unsigned long long wake_status = read_pmc_wake_status();

	pr_info(" legacy wake status=0x%x\n", (u32)wake_status);
	tegra_pm_irq_syscore_resume_helper((unsigned long)wake_status, 0);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	pr_info(" tegra3 wake status=0x%x\n", (u32)(wake_status >> 32));
	tegra_pm_irq_syscore_resume_helper(
		(unsigned long)(wake_status >> 32), 1);
#endif
}

int tegra_pm_irq_get_wakeup_irq(void)
{
	unsigned long long wake_status_ll = read_pmc_wake_status();
	unsigned long wake_status;
	int wake;
	int irq;
	int index;
	struct irq_desc *desc;

	for (index = 0; index < 2; ++index) {
		wake_status = (u32)(wake_status_ll >> (index * 32));
		for_each_set_bit(wake, &wake_status, sizeof(wake_status) * 8) {
			irq = tegra_wake_to_irq(wake + 32 * index);
			if (!irq)
				continue;
			desc = irq_to_desc(irq);
			if (!desc || !desc->action || !desc->action->name)
				continue;
			return irq;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL(tegra_pm_irq_get_wakeup_irq);

#ifdef DEBUG_WAKE_SOURCE
static void print_val64(char *name, u64 val)
{
	pr_info("%s[31-0]=%#x\n", name, (u32)(val & 0xFFFFFFFF));
	pr_info("%s[63-32]=%#x\n", name, (u32)((val >> 32) & 0xFFFFFFFF));
}
#endif

#ifndef CONFIG_TEGRA_INTERNAL_USB_CABLE_WAKE_SUPPORT
inline void tegra_get_internal_any_wake_list(u8 *wake_count,
	u8 **any_wake, u8 *remote_usb_index)
{
	*wake_count = 0;
}

inline int get_vbus_id_cable_connect_state(bool *is_vbus_connected,
	bool *is_id_connected)
{
	return -EIO;
}
#endif

/*
 * static variables - tegra_usb_vbus_internal_wake and
 * tegra_usb_id_internal_wake are false without need to initialize
 */
static bool tegra_usb_vbus_internal_wake; /* support for internal vbus wake */
static bool tegra_usb_id_internal_wake; /* support for internal id wake */
void tegra_set_usb_vbus_internal_wake(bool enable)
{
	tegra_usb_vbus_internal_wake = enable;
}

void tegra_set_usb_id_internal_wake(bool enable)
{
	tegra_usb_id_internal_wake = enable;
}

/* handles special case of first time wake for VBUS and ID
 * We see that in one cable connect mode the wakeup
 * works when wake level is toggled
 */
static void handle_first_wake(u64 *wak_lvl, u64 *wak_enb, u32 indx)
{
	u32 lvl_tmp;
	bool is_vbus_connected;
	bool is_id_connected;
	int err;

	/* function to be called only if internal any wake supported */
	if (!any_wake_count)
		return;

	err = get_vbus_id_cable_connect_state(&is_vbus_connected,
		&is_id_connected);
	if (err)
		return;

	lvl_tmp = (*wak_lvl & (1ULL << indx)) ?  1 : 0;
#ifdef DEBUG_WAKE_SOURCE
	pr_info("%s: wake_src_index=%d, level=%d\n", __func__, indx, lvl_tmp);
#endif
	/* ID cable disconnected LP0 entry case */
	/* or VBUS cable connected LP0 entry case */
	if ((tegra_usb_id_internal_wake && (indx ==
		*(any_wake + ANY_WAKE_INDEX_ID)) && !is_id_connected) ||
		(tegra_usb_vbus_internal_wake && (indx ==
		*(any_wake + ANY_WAKE_INDEX_VBUS)) &&
		is_vbus_connected)) {
		lvl_tmp = !lvl_tmp;
		/* toggle wake level for these cases */
		*wak_lvl = *wak_lvl & ~(1ULL << indx);
		*wak_lvl |= (lvl_tmp << indx);
	}
	/* disable WAKE39 as we see repeated wakeups due to WAKE39 */
	if (is_id_connected)
		*wak_enb |= (1ULL << remote_usb_wake_index);
	else
		*wak_enb &= ~(1ULL << remote_usb_wake_index);
}

/* set up lp0 wake sources */
static int tegra_pm_irq_syscore_suspend(void)
{
	u32 temp;
	u64 status;
	u64 lvl;
	u64 wake_level;
	u64 wake_enb;
	int j;

	clear_pmc_sw_wake_status();

	temp = readl(pmc + PMC_CTRL);
	temp |= PMC_CTRL_LATCH_WAKEUPS;
	pmc_32kwritel(temp, PMC_CTRL);

	temp &= ~PMC_CTRL_LATCH_WAKEUPS;
	pmc_32kwritel(temp, PMC_CTRL);

	status = read_pmc_sw_wake_status();

	lvl = read_pmc_wake_level();

	/* flip the wakeup trigger for any-edge triggered pads
	 * which are currently asserting as wakeups */
	lvl ^= status;

	lvl &= tegra_lp0_wake_level_any;

	wake_level = lvl | tegra_lp0_wake_level;
	wake_enb = tegra_lp0_wake_enb;

	if (debug_lp0) {
		wake_level = lvl ^ status;
		wake_enb = 0xffffffff;
	}

	/* Clear PMC Wake Status registers while going to suspend */
	temp = readl(pmc + PMC_WAKE_STATUS);
	if (temp)
		pmc_32kwritel(temp, PMC_WAKE_STATUS);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	temp = readl(pmc + PMC_WAKE2_STATUS);
	if (temp)
		pmc_32kwritel(temp, PMC_WAKE2_STATUS);
#endif

#ifdef DEBUG_WAKE_SOURCE
	if ((test_wake_src_index > 0) &&
		(test_wake_src_index < PMC_MAX_WAKE_COUNT)) {
		pr_info("%s: wake_src_index=%ld, should wake with wake level=%d as per sw_wake_status\n",
			__func__, test_wake_src_index,
			(status & (1ULL << test_wake_src_index)) ? 1 : 0);
		pr_info("%s: wake_src_index=%ld, desired polarity=%ld, old level=%d\n",
			__func__, test_wake_src_index, test_wake_src_polarity,
			((wake_level & (1ULL << test_wake_src_index)) ? 1 : 0));
	}
	print_val64("wake_level", wake_level);
#endif

	if (!tegra_usb_vbus_internal_wake && !tegra_usb_id_internal_wake) {
		if (any_wake_count) {
			/* ensure that WAKE19 and WAKE21 are disabled */
			wake_enb &= ~(1ULL << *(any_wake +
				ANY_WAKE_INDEX_VBUS));
			wake_enb &= ~(1ULL << *(any_wake +
				ANY_WAKE_INDEX_ID));
		}
#ifndef DEBUG_WAKE_SOURCE
		goto skip_usb_any_wake;
#endif
	}
	/*
	 * ANY polarity for USB1 VBUS and USB1 ID wake is implemented
	 * These are handled as special case here
	 */

	for (j = 0; j < any_wake_count; j++) {
		if (wake_enb && (1ULL << *(any_wake + j))) {
#ifdef DEBUG_WAKE_SOURCE
			pr_info("%s: wake level ANY sources: WAKE%d=%s\n",
				__func__, *(any_wake + j),
				((wake_enb && (1ULL << *(any_wake + j))) ?
				"enabled" : "disabled"));
#endif
			handle_first_wake(&wake_level, &wake_enb,
				*(any_wake + j));
		}
	}

#ifdef DEBUG_WAKE_SOURCE
	/*
	 * Test code uses sysfs nodes to test LP0 wake for wake sources
	 * with option to select wake levels: lo-0, hi-1, any-2
	 *
	 * moved down in this function so that assumed wake
	 * levels for WAKE19 and WAKE21
	 * could be overridden for debug
	 */
	if ((test_wake_src_index > 0) &&
		(test_wake_src_index < PMC_MAX_WAKE_COUNT)) {
		if (test_wake_src_polarity == WAKE_LEVEL_HI) {
			pr_info("Test wake level HI\n");
			wake_level |= (1ULL << test_wake_src_index);
		} else if (test_wake_src_polarity == WAKE_LEVEL_LO) {
			pr_info("Test wake level LO\n");
			wake_level &= ~(1ULL << test_wake_src_index);
		} else if (test_wake_src_polarity == WAKE_LEVEL_ANY) {
			pr_info("Test wake level ANY\n");
			handle_first_wake(&wake_level, &wake_enb,
				test_wake_src_index);
		}
	}
#else
skip_usb_any_wake:
#endif
	write_pmc_wake_level(wake_level);

	write_pmc_wake_mask(wake_enb);

	return 0;
}

static struct syscore_ops tegra_pm_irq_syscore_ops = {
	.suspend = tegra_pm_irq_syscore_suspend,
	.resume = tegra_pm_irq_syscore_resume,
};

static int tegra_pm_irq_syscore_init(void)
{
	register_syscore_ops(&tegra_pm_irq_syscore_ops);

	return 0;
}
subsys_initcall(tegra_pm_irq_syscore_init);

#ifdef DEBUG_WAKE_SOURCE
static ssize_t wake_index_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%ld\n", test_wake_src_index);
}

static ssize_t wake_index_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t n)
{
	if (kstrtol(buf, 10, &test_wake_src_index)) {
		pr_err("\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}
	return n;
}

static struct kobj_attribute wake_index_data_attribute =
	__ATTR(wake_src_index, 0644, wake_index_show, wake_index_store);

static ssize_t wak_polarity_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%ld\n", test_wake_src_polarity);
}

static ssize_t wak_polarity_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t n)
{
	if (kstrtol(buf, 10, &test_wake_src_polarity)) {
		pr_err("\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}
	return n;
}

static struct kobj_attribute wake_polarity_data_attribute =
	__ATTR(wake_src_polarity, 0644, wak_polarity_show, wak_polarity_store);

static struct kobject *wake_data_kobj;
#endif

#ifdef CONFIG_DEBUG_FS
static int tegra_pm_irq_debug_show(struct seq_file *s, void *data)
{
	int wake;
	int irq;
	struct irq_desc *desc;
	const char *irq_name;

	seq_printf(s, "wake  irq  count  name\n");
	seq_printf(s, "----------------------\n");
	for (wake = 0; wake < PMC_MAX_WAKE_COUNT; wake++) {
		irq = tegra_wake_to_irq(wake);
		if (irq < 0)
			continue;

		desc = irq_to_desc(irq);
		if (tegra_wake_irq_count[wake] == 0 && desc->action == NULL)
			continue;

		irq_name = (desc->action && desc->action->name) ?
			desc->action->name : "???";

		seq_printf(s, "%4d  %3d  %5d  %s\n",
			wake, irq, tegra_wake_irq_count[wake], irq_name);
	}
	return 0;
}

static int tegra_pm_irq_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_pm_irq_debug_show, NULL);
}

static const struct file_operations tegra_pm_irq_debug_fops = {
	.open		= tegra_pm_irq_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_pm_irq_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("wake_irq", S_IRUGO, NULL, NULL,
		&tegra_pm_irq_debug_fops);
	if (!d) {
		pr_err("Failed to create suspend_mode debug file\n");
		return -ENOMEM;
	}

#ifdef DEBUG_WAKE_SOURCE
	wake_data_kobj = kobject_create_and_add("wakedata", kernel_kobj);
	if (wake_data_kobj) {
		if (sysfs_create_file(wake_data_kobj, \
					&wake_index_data_attribute.attr))
			pr_err("%s: sysfs_create_file wake_index failed!\n",
								__func__);
		if (sysfs_create_file(wake_data_kobj, \
					&wake_polarity_data_attribute.attr))
			pr_err("%s: sysfs_create_file wake_polarity failed!\n",
								__func__);
	}
#endif

	/*
	 * tegra list of any wake sources needed in functions
	 * accessing any_wake list
	 */
	tegra_get_internal_any_wake_list(&any_wake_count, &any_wake,
		&remote_usb_wake_index);

	return 0;
}

late_initcall(tegra_pm_irq_debug_init);
#endif
