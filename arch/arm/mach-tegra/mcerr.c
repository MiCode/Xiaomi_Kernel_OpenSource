/*
 * arch/arm/mach-tegra/mcerr.c
 *
 * MC error code common to T3x and T11x. T20 has been left alone.
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation. All rights reserved.
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

#define pr_fmt(fmt) "mc-err: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/moduleparam.h>
#include <linux/spinlock_types.h>

#include <mach/mc.h>
#include <mach/irqs.h>
#include <mach/mcerr.h>

#include "iomap.h"

static bool mcerr_throttle_enabled = true;

static int arb_intr_mma_set(const char *arg, const struct kernel_param *kp);
static int arb_intr_mma_get(char *buff, const struct kernel_param *kp);
static void unthrottle_prints(struct work_struct *work);

static int spurious_intrs;

static struct arb_emem_intr_info arb_intr_info = {
	.lock = __SPIN_LOCK_UNLOCKED(arb_intr_info.lock),
};
static int arb_intr_count;

static struct kernel_param_ops arb_intr_mma_ops = {
	.get = arb_intr_mma_get,
	.set = arb_intr_mma_set,
};

module_param_cb(arb_intr_mma_in_ms, &arb_intr_mma_ops,
		&arb_intr_info.arb_intr_mma, S_IRUGO | S_IWUSR);
module_param(arb_intr_count, int, S_IRUGO | S_IWUSR);
module_param(spurious_intrs, int, S_IRUGO | S_IWUSR);

static const char *const smmu_page_attrib[] = {
	"nr-nw-s",
	"nr-nw-ns",
	"nr-wr-s",
	"nr-wr-ns",
	"rd-nw-s",
	"rd-nw-ns",
	"rd-wr-s",
	"rd-wr-ns"
};

/*
 * Table of known errors and their interrupt signatures.
 */
static const struct mc_error mc_errors[] = {
	MC_ERR(MC_INT_DECERR_EMEM,
	       "EMEM address decode error",
	       0, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_DECERR_VPR,
	       "MC request violates VPR requirments",
	       0, MC_ERR_VPR_STATUS, MC_ERR_VPR_ADR),
	MC_ERR(MC_INT_SECURITY_VIOLATION,
	       "non secure access to secure region",
	       0, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_SECERR_SEC,
	       "MC request violated SEC carveout requirements",
	       0, MC_ERR_SEC_STATUS, MC_ERR_SEC_ADR),

	/*
	 * SMMU related faults.
	 */
	MC_ERR(MC_INT_INVALID_SMMU_PAGE,
	       "SMMU address translation fault",
	       E_SMMU, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_INVALID_SMMU_PAGE | MC_INT_DECERR_EMEM,
	       "EMEM decode error on PDE or PTE entry",
	       E_SMMU, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_INVALID_SMMU_PAGE | MC_INT_SECERR_SEC,
	       "secure SMMU address translation fault",
	       E_SMMU, MC_ERR_SEC_STATUS, MC_ERR_SEC_ADR),
	MC_ERR(MC_INT_INVALID_SMMU_PAGE | MC_INT_DECERR_VPR,
	       "VPR SMMU address translation fault",
	       E_SMMU, MC_ERR_VPR_STATUS, MC_ERR_VPR_ADR),

	/*
	 * Baseband controller related faults.
	 */
	MC_ERR(MC_INT_BBC_PRIVATE_MEM_VIOLATION,
	       "client accessed BBC aperture",
	       0, MC_ERR_BBC_STATUS, MC_ERR_BBC_ADR),
	MC_ERR(MC_INT_DECERR_BBC,
	       "BBC accessed memory outside of its aperture",
	       E_NO_STATUS, 0, 0),

	/* NULL terminate. */
	MC_ERR(0, NULL, 0, 0, 0),
};

static DEFINE_SPINLOCK(mc_lock);
static unsigned long error_count;

static DECLARE_DELAYED_WORK(unthrottle_prints_work, unthrottle_prints);

static struct dentry *mcerr_debugfs_dir;

/*
 * Chip specific functions.
 */
static struct mcerr_chip_specific chip_specific;

static int arb_intr_mma_set(const char *arg, const struct kernel_param *kp)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&arb_intr_info.lock, flags);
	ret = param_set_int(arg, kp);
	spin_unlock_irqrestore(&arb_intr_info.lock, flags);
	return ret;
}

static int arb_intr_mma_get(char *buff, const struct kernel_param *kp)
{
	return param_get_int(buff, kp);
}

static void arb_intr(void)
{
	u64 time;
	u32 time_diff_ms;
	unsigned long flags;

	spin_lock_irqsave(&arb_intr_info.lock, flags);
	arb_intr_count++;
	time = sched_clock();
	time_diff_ms = (time - arb_intr_info.time) >> 20;
	arb_intr_info.time = time;
	arb_intr_info.arb_intr_mma =
		((MMA_HISTORY_SAMPLES - 1) * time_diff_ms +
		 arb_intr_info.arb_intr_mma) / MMA_HISTORY_SAMPLES;
	spin_unlock_irqrestore(&arb_intr_info.lock, flags);
}

static void unthrottle_prints(struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&mc_lock, flags);
	error_count = 0;
	spin_unlock_irqrestore(&mc_lock, flags);
}

/*
 * Common MC error handling code.
 */
static irqreturn_t tegra_mc_error_isr(int irq, void *data)
{
	int err_mc = 0;
	struct mc_client *client = NULL;
	const struct mc_error *fault;
	const char *smmu_info;
	unsigned long count;
	phys_addr_t addr;
	u32 status, intr;
	u32 write, secure;
	u32 client_id;

	intr = mc_readl(MC_INT_STATUS);

	cancel_delayed_work(&unthrottle_prints_work);

#ifdef MC_DUAL_CHANNEL
	/*
	 * Interrupts can come from either MC; handle the case in which the
	 * interrupt is generated by the second MC.
	 */
	if (intr & MC_INT_EXT_INTR_IN) {
		err_mc = 1;
		intr = __mc_readl(err_mc, MC_INT_STATUS);
	}
#endif

	/*
	 * Sometimes the MC seems to generate spurious interrupts - that
	 * is interrupts with an interrupt status register equal to 0.
	 * Not much we can do other than keep a count of them.
	 */
	if (!intr) {
		spurious_intrs++;
		goto out;
	}

	intr &= MC_INT_EN_MASK;
	if (intr & MC_INT_ARBITRATION_EMEM) {
		arb_intr();
		if (intr == MC_INT_ARBITRATION_EMEM)
			goto out;
		intr &= ~MC_INT_ARBITRATION_EMEM;
	}

	spin_lock(&mc_lock);
	count = ++error_count;
	spin_unlock(&mc_lock);

	fault = chip_specific.mcerr_info(intr & MC_INT_EN_MASK);
	if (WARN(!fault, "[mcerr] Unknown error! intr sig: 0x%08x\n",
		 intr & MC_INT_EN_MASK))
		goto out;

	if (fault->flags & E_NO_STATUS) {
		pr_err("[mcerr] MC fault - no status: %s\n", fault->msg);
		goto out;
	}

	status = __mc_readl(err_mc, fault->stat_reg);
	addr = __mc_readl(err_mc, fault->addr_reg);
	secure = !!(status & MC_ERR_STATUS_SECURE);
	write = !!(status & MC_ERR_STATUS_WRITE);
	client_id = status & 0x7f;
	client = &mc_clients[client_id <= mc_client_last
			     ? client_id : mc_client_last];

#ifdef MC_ERR_34BIT_PHYS_ADDR
	/*
	 * LPAE: make sure we get the extra 2 physical address bits available
	 * and pass them down to the printing function.
	 */
	addr |= (((phys_addr_t)(status & MC_ERR_STATUS_ADR_HI)) << 12);
#endif

	if (fault->flags & E_SMMU)
		smmu_info = smmu_page_attrib[MC_ERR_SMMU_BITS(status)];
	else
		smmu_info = NULL;

	chip_specific.mcerr_info_update(client, intr & MC_INT_EN_MASK);

	if (mcerr_throttle_enabled && count >= MAX_PRINTS) {
		schedule_delayed_work(&unthrottle_prints_work, HZ/2);
		if (count == MAX_PRINTS)
			pr_err("Too many MC errors; throttling prints\n");
		goto out;
	}

	chip_specific.mcerr_print(fault, client, status, addr, secure, write,
				  smmu_info);
out:
	__mc_writel(err_mc, intr, MC_INT_STATUS);
	if (err_mc != 0) {
		__mc_readl(err_mc, MC_INT_STATUS);
		mc_writel(MC_INT_EXT_INTR_IN, MC_INT_STATUS);
	}
	return IRQ_HANDLED;
}

static const struct mc_error *mcerr_default_info(u32 intr)
{
	const struct mc_error *err;

	for (err = mc_errors; err->sig && err->msg; err++) {
		if (intr != err->sig)
			continue;
		return err;
	}

	return NULL;
}

static void mcerr_default_info_update(struct mc_client *c, u32 stat)
{
	if (stat & MC_INT_DECERR_EMEM)
		c->intr_counts[0]++;
	if (stat & MC_INT_SECURITY_VIOLATION)
		c->intr_counts[1]++;
	if (stat & MC_INT_INVALID_SMMU_PAGE)
		c->intr_counts[2]++;

	/* Unknown interrupts. */
	if (stat & ~MC_INT_EN_MASK)
		c->intr_counts[3]++;
}

/*
 * This will print at least 8 hex digits for address. If the address is bigger
 * then more digits will be printed but the full 16 hex digits for a 64 bit
 * address will not get printed by the current code.
 */
static void mcerr_default_print(const struct mc_error *err,
				const struct mc_client *client,
				u32 status, phys_addr_t addr,
				int secure, int rw, const char *smmu_info)
{
	pr_err("[mcerr] (%s) %s: %s\n", client->swgid, client->name, err->msg);
	pr_err("[mcerr]   status = 0x%08x; addr = 0x%08llx", status,
	       (long long unsigned int)addr);
	pr_err("[mcerr]   secure: %s, access-type: %s, SMMU fault: %s\n",
	       secure ? "yes" : "no", rw ? "write" : "read",
	       smmu_info ? smmu_info : "none");
}

/*
 * Print the MC err stats for each client.
 */
static int mcerr_default_debugfs_show(struct seq_file *s, void *v)
{
	int i, j;
	int do_print;

	seq_printf(s, "%-24s %-24s %-9s %-9s %-9s %-9s\n", "swgid", "client",
		   "decerr", "secerr", "smmuerr", "unknown");
	for (i = 0; i < chip_specific.nr_clients; i++) {
		do_print = 0;

		/* Only print clients who actually have errors. */
		for (j = 0; j < INTR_COUNT; j++) {
			if (mc_clients[i].intr_counts[j]) {
				do_print = 1;
				break;
			}
		}

		if (do_print)
			seq_printf(s, "%-24s %-24s %-9u %-9u %-9u %-9u\n",
				   mc_clients[i].name,
				   mc_clients[i].swgid,
				   mc_clients[i].intr_counts[0],
				   mc_clients[i].intr_counts[1],
				   mc_clients[i].intr_counts[2],
				   mc_clients[i].intr_counts[3]);
	}
	return 0;
}

static int mcerr_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, chip_specific.mcerr_debugfs_show, NULL);
}

static const struct file_operations mcerr_debugfs_fops = {
	.open           = mcerr_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int __get_throttle(void *data, u64 *val)
{
	*val = mcerr_throttle_enabled;
	return 0;
}

static int __set_throttle(void *data, u64 val)
{
	unsigned long flags;

	spin_lock_irqsave(&mc_lock, flags);
	error_count = 0;
	spin_unlock_irqrestore(&mc_lock, flags);

	mcerr_throttle_enabled = (bool) val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(mcerr_throttle_debugfs_fops, __get_throttle,
			__set_throttle, "%llu\n");

/*
 * This will always e successful. However, if something goes wrong in the
 * init a message will be printed to the kernel log. Since this is a
 * non-essential piece of the kernel no reason to fail the entire MC init
 * if this fails.
 */
int __init tegra_mcerr_init(struct dentry *mc_parent)
{
	u32 reg;

	chip_specific.mcerr_info         = mcerr_default_info;
	chip_specific.mcerr_info_update  = mcerr_default_info_update;
	chip_specific.mcerr_print        = mcerr_default_print;
	chip_specific.mcerr_debugfs_show = mcerr_default_debugfs_show;
	chip_specific.nr_clients = 0;

	/*
	 * mcerr_chip_specific_setup() can override any of the default
	 * functions as it wishes.
	 */
	mcerr_chip_specific_setup(&chip_specific);

	if (request_irq(INT_MC_GENERAL, tegra_mc_error_isr, 0,
			"mc_status", NULL)) {
		pr_err("%s: unable to register MC error interrupt\n", __func__);
		goto done;
	} else {
		reg = MC_INT_EN_MASK;
		mc_writel(reg, MC_INT_MASK);
	}

	mcerr_debugfs_dir = debugfs_create_dir("err", mc_parent);
	if (mcerr_debugfs_dir == NULL) {
		pr_err("Failed to make debugfs node: %ld\n",
		       PTR_ERR(mcerr_debugfs_dir));
		goto done;
	}
	debugfs_create_file("mcerr", 0644, mcerr_debugfs_dir, NULL,
			    &mcerr_debugfs_fops);
	debugfs_create_file("mcerr_throttle", S_IRUGO | S_IWUSR,
			    mcerr_debugfs_dir, NULL,
			    &mcerr_throttle_debugfs_fops);

	pr_info("Started MC error interface!\n");

done:
	return 0;
}
