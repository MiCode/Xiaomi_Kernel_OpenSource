/*
 * Based on arch/arm/kernel/irq.c
 *
 * Copyright (C) 1992 Linus Torvalds
 * Modifications for ARM processor Copyright (C) 1995-2000 Russell King.
 * Support for Dynamic Tick Timer Copyright (C) 2004-2005 Nokia Corporation.
 * Dynamic Tick Timer written by Tony Lindgren <tony@atomide.com> and
 * Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>.
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/irqchip.h>
#include <linux/seq_file.h>
#include <linux/ratelimit.h>

unsigned long irq_err_count;

int arch_show_interrupts(struct seq_file *p, int prec)
{
#ifdef CONFIG_SMP
	show_ipi_list(p, prec);
#endif
	seq_printf(p, "%*s: %10lu\n", prec, "Err", irq_err_count);
	return 0;
}

void (*handle_arch_irq)(struct pt_regs *) = NULL;

void __init set_handle_irq(void (*handle_irq)(struct pt_regs *))
{
	if (handle_arch_irq)
		return;

	handle_arch_irq = handle_irq;
}

void __init init_IRQ(void)
{
	irqchip_init();
	if (!handle_arch_irq)
		panic("No interrupt controller found.");
}

#ifdef CONFIG_HOTPLUG_CPU

#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
#include <linux/slab.h>
#include <linux/bitmap.h>

static inline bool mt_cpumask_equal(const struct cpumask *src1p, const struct cpumask *src2p)
{
	return bitmap_equal(cpumask_bits(src1p), cpumask_bits(src2p), num_possible_cpus());
}

struct thread_safe_list irq_need_migrate_list[CONFIG_NR_CPUS];

void fixup_update_irq_need_migrate_list(struct irq_desc *desc)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	struct irq_chip *c = irq_data_get_irq_chip(d);

	/* if this IRQ is not per-cpu IRQ => force to target all*/
	if (!irqd_is_per_cpu(d)) {
		/* gic affinity to target all and update the smp affinity */
		if (!c->irq_set_affinity)
			pr_err("IRQ%u: unable to set affinity\n", d->irq);
		else if (c->irq_set_affinity(d, cpu_possible_mask, true) == IRQ_SET_MASK_OK)
			cpumask_copy(d->affinity, cpu_possible_mask);
	}
}

bool check_consistency_of_irq_settings(struct irq_desc *desc)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	struct per_cpu_irq_desc *node;
	struct list_head *pos, *temp;
	cpumask_t per_cpu_list_affinity, gic_target_affinity, tmp_affinity;
	bool ret = true;
	int cpu;

	/* if this IRQ is per-cpu IRQ: only check gic setting */
	if (irqd_is_per_cpu(d))
		goto check_gic;

	/* get the setting in per-cpu irq-need-lists */
	cpumask_clear(&per_cpu_list_affinity);

	rcu_read_lock();
	for_each_cpu(cpu, cpu_possible_mask)
		list_for_each_safe(pos, temp, &(irq_need_migrate_list[cpu].list)) {
			node = list_entry_rcu(pos, struct per_cpu_irq_desc, list);
			if (node->desc == desc) {
				cpumask_set_cpu(cpu, &per_cpu_list_affinity);
				break;
			}
		}
	rcu_read_unlock();

	/* compare with the setting of smp affinity */
	if (mt_cpumask_equal(d->affinity, cpu_possible_mask)) {
		/*
		 * if smp affinity is set to all CPUs
		 * AND this IRQ is not be found in any per-cpu list -> success
		 */
		ret = (cpumask_empty(&per_cpu_list_affinity)) ? true : false;
	} else if (!mt_cpumask_equal(&per_cpu_list_affinity, d->affinity)) {
		/* smp affinity should be the same as per-cpu list */
		ret = false;
	}

	/* print out to error logs */
	if (!ret) {
		pr_err("[IRQ] IRQ %d: smp affinity is not consistent with per-cpu list\n", d->irq);
		cpumask_xor(&tmp_affinity, &per_cpu_list_affinity, d->affinity);

		/* iterates on cpus with inconsitent setting */
		for_each_cpu(cpu, &tmp_affinity)
			if (cpumask_test_cpu(cpu, d->affinity))
				pr_err("[IRQ] @CPU%u: smp affinity is set, but per-cpu list is not set\n", cpu);
			else
				pr_err("[IRQ] @CPU%u: smp affinity is not set, but per-cpu list is set\n", cpu);
	}


check_gic:

	if (mt_is_secure_irq(d)) {
		/* no need to check WDT */
		ret = true;
		goto out;
	}

	/* get the setting in gic setting and compare with the setting in gic setting*/
	cpumask_clear(&gic_target_affinity);

	if (!mt_get_irq_gic_targets(d, &gic_target_affinity)) {
		/* failed to get GICD_ITARGETSR the setting */
		pr_err("[IRQ] unable to get GICD_ITARGETSR setting of IRQ %d\n", d->irq);
		ret = false;
	} else if (!mt_cpumask_equal(&gic_target_affinity, d->affinity)) {
		pr_err("[IRQ] IRQ %d: smp affinity is not consistent with GICD_ITARGETSR\n", d->irq);
		cpumask_xor(&tmp_affinity, &gic_target_affinity, d->affinity);

		/* iterates on cpus with inconsitent setting */
		for_each_cpu(cpu, &tmp_affinity)
			if (cpumask_test_cpu(cpu, d->affinity))
				pr_err("[IRQ] @CPU%u: smp affinity is set, but gic reg is not set\n", cpu);
			else
				pr_err("[IRQ] @CPU%u: smp affinity is not set, but gic reg is set\n", cpu);
		ret = false;
	}

out:

	if (!ret)
		pr_err("[IRQ] IRQ %d: the affintiy setting is INCONSITENT\n", d->irq);

	return ret;
}

void dump_irq_need_migrate_list(const struct cpumask *mask)
{
	struct per_cpu_irq_desc *node;
	struct list_head *pos, *temp;
	int cpu;

	rcu_read_lock();
	for_each_cpu(cpu, mask) {
		pr_debug("[IRQ] dump per-cpu irq-need-migrate list of CPU%u\n", cpu);
		list_for_each_safe(pos, temp, &(irq_need_migrate_list[cpu].list)) {
			node = list_entry_rcu(pos, struct per_cpu_irq_desc, list);
			pr_debug("[IRQ] IRQ %d\n", (node->desc->irq_data).irq);
		}
	}
	rcu_read_unlock();
}

static void del_from_irq_need_migrate_list(struct irq_desc *desc, const struct cpumask *cpumask_to_del)
{
	struct per_cpu_irq_desc *node, *next;
	int cpu;

	for_each_cpu(cpu, cpumask_to_del) {
		spin_lock(&(irq_need_migrate_list[cpu].lock));
		list_for_each_entry_safe(node, next,
			&(irq_need_migrate_list[cpu].list), list) {
			if (node->desc != desc)
				continue;
			pr_debug("[IRQ] list_del to cpu %d\n", cpu);
			list_del_rcu(&node->list);
			kfree(node);
			break;
		}
		spin_unlock(&(irq_need_migrate_list[cpu].lock));
	}
}

/* return false for error */
static bool add_to_irq_need_migrate_list(struct irq_desc *desc, const struct cpumask *cpumask_to_add)
{
	struct per_cpu_irq_desc *node;
	int cpu;
	bool ret = true;

	for_each_cpu(cpu, cpumask_to_add) {
		spin_lock(&(irq_need_migrate_list[cpu].lock));

		node = kmalloc(sizeof(struct per_cpu_irq_desc), GFP_ATOMIC);
		if (node == NULL) {
			spin_unlock(&(irq_need_migrate_list[cpu].lock));
			ret = false;
			break;
		}

		node->desc = desc;
		pr_debug("[IRQ] list_add to cpu %d\n", cpu);
		list_add_rcu(&node->list, &(irq_need_migrate_list[cpu].list));
		spin_unlock(&(irq_need_migrate_list[cpu].lock));
	}

	/* delete what we have added when failed */
	if (!ret) {
		pr_err("[IRQ] kmalloc failed: cannot add node into CPU%d per-cpu IRQ list\n", cpu);
		del_from_irq_need_migrate_list(desc, cpumask_to_add);
	}

	return ret;
}

/*
 * must be invoked before the cpumask_copy of irq_desc for getting the original smp affinity
 * return @true when success
 */
bool update_irq_need_migrate_list(struct irq_desc *desc, const struct cpumask *new_affinity)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	cpumask_t need_update_affinity, tmp_affinity;

	pr_debug("[IRQ] update per-cpu list (IRQ %d)\n", d->irq);

	/* find out the per-cpu irq-need-migrate lists to be updated */
	cpumask_xor(&need_update_affinity, d->affinity, new_affinity);

	/* return if there is no need to update the per-cpu irq-need-migrate lists */
	if (cpumask_empty(&need_update_affinity))
		return true;

	/* special cases */
	if (mt_cpumask_equal(new_affinity, cpu_possible_mask)) {
		/*
		 * case 1: new affinity is to all cpus
		 * clear this IRQs from all per-cpu irq-need-migrate lists of old affinity
		 */
		del_from_irq_need_migrate_list(desc, d->affinity);
		return true;
	} else if (mt_cpumask_equal(d->affinity, cpu_possible_mask)) {
		/*
		 * case 2: old affinity is to all cpus
		 * add this IRQs to per-cpu irq-need-migrate lists of new affinity
		 */
		return add_to_irq_need_migrate_list(desc, new_affinity);
	}

	/* needs to be update AND is in new affinity -> list_add */
	cpumask_and(&tmp_affinity, &need_update_affinity, new_affinity);
	if (!add_to_irq_need_migrate_list(desc, &tmp_affinity))
		return false;

	/* needs to be update AND is in old affinity -> list_del */
	cpumask_and(&tmp_affinity, &need_update_affinity, d->affinity);
	del_from_irq_need_migrate_list(desc, &tmp_affinity);

	return true;
}

/* update smp affinity and per-cpu irq-need-migrate lists */
void update_affinity_settings(struct irq_desc *desc, const struct cpumask *new_affinity, bool update_smp_affinity)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	bool need_fix = false;

	need_fix = !update_irq_need_migrate_list(desc, new_affinity);
	if (update_smp_affinity)
		cpumask_copy(d->affinity, new_affinity);
	if (need_fix)
		fixup_update_irq_need_migrate_list(desc);

#ifdef CONFIG_MTK_IRQ_NEW_DESIGN_DEBUG
	/* verify the consistency of IRQ setting after updating */
	WARN_ON(!check_consistency_of_irq_settings(desc));
#endif
}
#endif

static bool migrate_one_irq(struct irq_desc *desc)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	const struct cpumask *affinity = d->affinity;
	struct irq_chip *c;
	bool ret = false;

	/*
	 * If this is a per-CPU interrupt, or the affinity does not
	 * include this CPU, then we have nothing to do.
	 */
	if (irqd_is_per_cpu(d) || !cpumask_test_cpu(smp_processor_id(), affinity))
		return false;

	if (cpumask_any_and(affinity, cpu_online_mask) >= nr_cpu_ids) {
		affinity = cpu_online_mask;
		ret = true;
	}

	c = irq_data_get_irq_chip(d);
	if (!c->irq_set_affinity)
		pr_debug("IRQ%u: unable to set affinity\n", d->irq);
	else if (c->irq_set_affinity(d, affinity, false) == IRQ_SET_MASK_OK && ret)
#ifndef CONFIG_MTK_IRQ_NEW_DESIGN
		cpumask_copy(d->affinity, affinity);
#else
		update_affinity_settings(desc, affinity, true);
#endif

	return ret;
}

/*
 * The current CPU has been marked offline.  Migrate IRQs off this CPU.
 * If the affinity settings do not allow other CPUs, force them onto any
 * available CPU.
 *
 * Note: we must iterate over all IRQs, whether they have an attached
 * action structure or not, as we need to get chained interrupts too.
 */
void migrate_irqs(void)
{
#ifndef CONFIG_MTK_IRQ_NEW_DESIGN
	unsigned int i;
#endif
	struct irq_desc *desc;
	unsigned long flags;
#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
	struct list_head *pos, *temp;
#endif

	local_irq_save(flags);

#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
	rcu_read_lock();
	list_for_each_safe(pos, temp, &(irq_need_migrate_list[smp_processor_id()].list)) {
		struct per_cpu_irq_desc *ptr = list_entry_rcu(pos, struct per_cpu_irq_desc, list);
		bool affinity_broken;

		desc = ptr->desc;
		pr_debug("[IRQ] CPU%u is going down, IRQ%u needs to be migrated\n",
					    smp_processor_id(), (desc->irq_data).irq);

		raw_spin_lock(&desc->lock);
		affinity_broken = migrate_one_irq(desc);
		raw_spin_unlock(&desc->lock);

		if (affinity_broken)
			pr_warn_ratelimited("IRQ%u no longer affine to CPU%u\n",
					    (desc->irq_data).irq, smp_processor_id());
	}
	rcu_read_unlock();
#else
	for_each_irq_desc(i, desc) {
		bool affinity_broken;

		raw_spin_lock(&desc->lock);
		affinity_broken = migrate_one_irq(desc);
		raw_spin_unlock(&desc->lock);

		if (affinity_broken)
			pr_warn_ratelimited("IRQ%u no longer affine to CPU%u\n",
					    i, smp_processor_id());
	}
#endif
	local_irq_restore(flags);
}
#endif /* CONFIG_HOTPLUG_CPU */
