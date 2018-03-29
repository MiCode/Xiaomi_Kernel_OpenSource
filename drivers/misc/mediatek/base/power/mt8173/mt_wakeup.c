/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/capability.h>
#include <linux/export.h>
#include <linux/suspend.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/syscore_ops.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>

#include "mt_wakeup.h"

static struct wakeup_event *wakeup_events;
static struct wakeup_event *last_wakeup_ev;
static unsigned total_wakeup_events = WEV_MAX;
static struct dentry *wakeup_events_stats_dentry;

/**
 * weak function for BSPs to map irq to wakeup_event_t
 */
__weak wakeup_event_t irq_to_wakeup_ev(int irq)
{
	return WEV_MAX;
}

/**
 * pm_rport_resume_ev - report a 'resume from suspend' event with it's
 * corresponding irq.
 *
 * @ev : wakeup_event_t value corresponding to the event
 * @irq : Interrupt number for the event
 *
 * Events can be of type {WEV_RTC, WEV_WIFI etc}. They can also be *not* listed
 * in the wakeup_event_t enum. This function can record as much as 19 *unique*
 * wakeup events and it will consolidate all other events into a single one at
 * the end. If ev is >= EV_MAX, the mapping of irq to event depends on
 * irq_to_wakeup_ev() function defined by the each arch. In future this may
 * change and the function will be using the data extracted from a device tree
 * node instead. For now, we allocate a new slot for every unique event, so we
 * are not architecture dependent. We find the event name from its 'irq' for
 * each unknown event.
 *
 * Absence of any locks is because this is exclusively supposed to be called
 * from syscore_ops->resume() where all non-boot cpus are shutdown and local
 * irqs are disabled.
 *
 */
void pm_report_resume_ev(wakeup_event_t ev, int irq)
{
	struct wakeup_event *we = NULL;
	int slot;

	if (unlikely(!wakeup_events))
		return;

	if (ev >= WEV_MAX) {
		for (slot = ev; slot < WEV_TOTAL; slot++) {
			if (wakeup_events[slot].irq == irq) {
				we = &wakeup_events[slot];
				break;
			}
		}
	} else {
		we = &wakeup_events[ev];
	}

	/* This is a new event */
	if (!we) {
		we = &wakeup_events[total_wakeup_events];
		total_wakeup_events = (total_wakeup_events < (WEV_TOTAL - 1)) ?
					total_wakeup_events + 1 :
					total_wakeup_events;
		we->event = ev;
		if (ev >= WEV_MAX) {
			struct irq_desc *desc;

			we->name = "null";

			desc = irq_to_desc(irq);
			if (desc == NULL)
				we->name = "spurious";
			else if (desc->action && desc->action->name)
				we->name = desc->action->name;
		}
	}

	BUG_ON(!we);

	we->event = ev;
	we->last_time = ns_to_ktime(sched_clock());
	if (unlikely(!we->irq))
		we->irq = irq;

	if (!last_wakeup_ev ||
		(last_wakeup_ev && (last_wakeup_ev != we)))
		we->count++;

	last_wakeup_ev = we;
}
EXPORT_SYMBOL(pm_report_resume_ev);

/**
 * pm_rport_resume_irq - report a 'resume from suspend' irq.
 *
 * @irq : Interrupt number that caused the SoC to come out of power collapse
 *
 * This is a wrapper to pm_report_resume_ev(). The purpose is to allow
 * architectures to start reporting resume irqs w/o having to define their own
 * irq_to_wakeup_ev()
 */

void pm_report_resume_irq(int irq)
{
	wakeup_event_t ev;

	if (unlikely(!wakeup_events))
		return;

	/* if arch doesn't know about this wakeup irq
	 * create a new entry and pickup the name from
	 * irq_desc->action->name
	 */
	ev = irq_to_wakeup_ev(irq);
	if (ev == WEV_NONE)
		ev = WEV_MAX;

	pm_report_resume_ev(ev, irq);
}
EXPORT_SYMBOL(pm_report_resume_irq);

wakeup_event_t pm_get_resume_ev(ktime_t *ts)
{
	if (unlikely(!wakeup_events))
		return WEV_NONE;

	if (!last_wakeup_ev)
		return WEV_NONE;

	*ts = last_wakeup_ev->last_time;
	return last_wakeup_ev->event;
}

/**
 * print_wakeup_events_stats - Print wakeup events statistics information.
 * @m: seq_file to print the statistics into.
 * @we: Wakeup event object to print the statistics for.
 */
static int print_wakeup_events_stats(struct seq_file *m,
				     struct wakeup_event *we)
{
	ktime_t now;
	ktime_t awake_time, total_time;

	/* print only if there are actually wakeup events */
	if (we->count) {
		now = ns_to_ktime(sched_clock());
		if (we != last_wakeup_ev) {
			total_time = we->total_time;
		} else {
			awake_time = ktime_sub(now, we->last_time);
			total_time = ktime_add(awake_time, we->total_time);
		}

		return seq_printf(m, "%-12s\t%d\t%lu\t\t%lld\n",
				we->name, we->irq, we->count,
				ktime_to_ms(total_time));
	}

	return 0;
}

/**
 * wakeup_events_stats_show - Print wakeup events statistics information.
 * @m: seq_file to print the statistics into.
 */
static int wakeup_events_stats_show(struct seq_file *m, void *unused)
{
	int i;

	seq_puts(m, "name\t\tirq\tevent_count\tawake_time\t\n");

	for (i = 0; i <= total_wakeup_events; i++)
		print_wakeup_events_stats(m, &wakeup_events[i]);

	return 0;
}

static int wakeup_events_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, wakeup_events_stats_show, NULL);
}


static const struct file_operations wakeup_events_stats_fops = {
	.owner = THIS_MODULE,
	.open = wakeup_events_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int wakeup_event_suspend(void)
{
	ktime_t now;
	ktime_t awake_time;


	if (likely(last_wakeup_ev)) {
		now = ns_to_ktime(sched_clock());
		awake_time = ktime_sub(now, last_wakeup_ev->last_time);
		last_wakeup_ev->total_time =
			ktime_add(last_wakeup_ev->total_time, awake_time);
	}

	last_wakeup_ev = NULL;

	return 0;
}


static struct syscore_ops we_syscore_ops = {
	.suspend = wakeup_event_suspend,
};

static int __init wakeup_events_init(void)
{
	int i;

	wakeup_events = kcalloc(WEV_TOTAL, sizeof(*wakeup_events),
					GFP_KERNEL);
	if (!wakeup_events)
		return -ENOMEM;

	/* Init known wakeup events */
	wakeup_events[WEV_RTC].name = "Rtc";
	wakeup_events[WEV_WIFI].name = "WiFi";
	wakeup_events[WEV_WAN].name = "Wan";
	wakeup_events[WEV_USB].name = "USB plug";
	wakeup_events[WEV_PWR].name = "Pon Key";
	wakeup_events[WEV_HALL].name = "Hall Sens";
	wakeup_events[WEV_BT].name = "BT";
	wakeup_events[WEV_CHARGER].name = "Charger";
	wakeup_events[WEV_TOTAL - 1].name = "Unknown (grp)";

	for (i = WEV_RTC; i < WEV_MAX; i++)
		wakeup_events[i].event = i;

	register_syscore_ops(&we_syscore_ops);

	return 0;
}

static int __init mt_wakeup_event_debugfs_init(void)
{
	if (wakeup_events_init())
		goto out;

	wakeup_events_stats_dentry = debugfs_create_file("wakeup_events",
			S_IRUGO, NULL, NULL, &wakeup_events_stats_fops);
out:
	return 0;
}

postcore_initcall(mt_wakeup_event_debugfs_init);

