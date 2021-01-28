// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/nmi.h>

#include <linux/uaccess.h>
#ifdef CONFIG_MTK_AEE_IPANIC
#include <mt-plat/mboot_params.h>
#endif

#include "tick-internal.h"

static char print_at_AEE_buffer[256];
#define SEQ_printf_at_AEE(m, x...)	do { \
	if (snprintf(print_at_AEE_buffer, sizeof(print_at_AEE_buffer), x) > 0) \
		aee_sram_fiq_log(print_at_AEE_buffer);\
	} while (0)

static void print_name_offset(struct seq_file *m, void *sym,
				  struct hrtimer *timer)
{
	char symname[KSYM_NAME_LEN];

	if (lookup_symbol_name((unsigned long)sym, symname) < 0) {
		SEQ_printf_at_AEE(m, "<%pK>", sym);
	} else {
		SEQ_printf_at_AEE(m, "%s", symname);
		if (timer && !strncmp(symname, "hrtimer_wakeup",
		    strlen("hrtimer_wakeup"))) {
			struct hrtimer_sleeper *t =
				container_of(timer, struct hrtimer_sleeper,
					      timer);
			SEQ_printf_at_AEE(m, " (task: %s)", t->task->comm);
		}
	}
}

static void
print_timer(struct seq_file *m, struct hrtimer *taddr, struct hrtimer *timer,
	    int idx, u64 now)
{
#ifdef CONFIG_TIMER_STATS
	char tmp[TASK_COMM_LEN + 1];
#endif
	SEQ_printf_at_AEE(m, " #%d: ", idx);
	print_name_offset(m, taddr, NULL);
	SEQ_printf_at_AEE(m, ", ");
	print_name_offset(m, timer->function, taddr);
	SEQ_printf_at_AEE(m, ", S:%02x", timer->state);
#ifdef CONFIG_TIMER_STATS
	SEQ_printf_at_AEE(m, ", ");
	print_name_offset(m, timer->start_site, NULL);
	memcpy(tmp, timer->start_comm, TASK_COMM_LEN);
	tmp[TASK_COMM_LEN] = 0;
	SEQ_printf_at_AEE(m, ", %s/%d", tmp, timer->start_pid);
#endif
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m,
		" # expires at %llu-%llu nsecs [in %lld to %lld nsecs]\n",
		(unsigned long long)ktime_to_ns(hrtimer_get_softexpires(timer)),
		(unsigned long long)ktime_to_ns(hrtimer_get_expires(timer)),
		(long long)(ktime_to_ns(hrtimer_get_softexpires(timer)) - now),
		(long long)(ktime_to_ns(hrtimer_get_expires(timer)) - now));
}

static void
print_active_timers(struct seq_file *m, struct hrtimer_clock_base *base,
		    u64 now)
{
	struct hrtimer *timer = NULL, tmp;
	unsigned long next = 0, i;
	struct timerqueue_node *curr = NULL;
	unsigned long flags;

next_one:
	i = 0;

	touch_nmi_watchdog();

	raw_spin_lock_irqsave(&base->cpu_base->lock, flags);

	curr = timerqueue_getnext(&base->active);
	/*
	 * Crude but we have to do this O(N*N) thing, because
	 * we have to unlock the base when printing:
	 */
	while (curr && i < next) {
		curr = timerqueue_iterate_next(curr);
		i++;
	}

	if (curr) {

		timer = container_of(curr, struct hrtimer, node);
		tmp = *timer;
		raw_spin_unlock_irqrestore(&base->cpu_base->lock, flags);

		print_timer(m, timer, &tmp, i, now);
		next++;
		goto next_one;
	}
	raw_spin_unlock_irqrestore(&base->cpu_base->lock, flags);
}

static void
print_base(struct seq_file *m, struct hrtimer_clock_base *base, u64 now)
{
	SEQ_printf_at_AEE(m, "  .base:       %pK\n", base);
	SEQ_printf_at_AEE(m, "  .index:      %d\n", base->index);

	SEQ_printf_at_AEE(m, "  .resolution: %u nsecs\n", hrtimer_resolution);

	SEQ_printf_at_AEE(m,   "  .get_time:   ");
	print_name_offset(m, base->get_time, NULL);
	SEQ_printf_at_AEE(m,   "\n");
#ifdef CONFIG_HIGH_RES_TIMERS
	SEQ_printf_at_AEE(m, "  .offset:     %llu nsecs\n",
		   (unsigned long long) ktime_to_ns(base->offset));
#endif
	SEQ_printf_at_AEE(m,   "active timers:\n");
	print_active_timers(m, base, now + ktime_to_ns(base->offset));
}

static void print_cpu(struct seq_file *m, int cpu, u64 now)
{
	struct hrtimer_cpu_base *cpu_base = &per_cpu(hrtimer_bases, cpu);
	int i;

	SEQ_printf_at_AEE(m, "cpu: %d\n", cpu);
	for (i = 0; i < HRTIMER_MAX_CLOCK_BASES; i++) {
		SEQ_printf_at_AEE(m, " clock %d:\n", i);
		print_base(m, cpu_base->clock_base + i, now);
	}
#define P(x) \
	SEQ_printf_at_AEE(m, "  .%-15s: %llu\n", #x, \
		   (unsigned long long)(cpu_base->x))
#define P_ns(x) \
	SEQ_printf_at_AEE(m, "  .%-15s: %llu nsecs\n", #x, \
		   (unsigned long long)(ktime_to_ns(cpu_base->x)))

#ifdef CONFIG_HIGH_RES_TIMERS
	P_ns(expires_next);
	P(hres_active);
	P(nr_events);
	P(nr_retries);
	P(nr_hangs);
	P(max_hang_time);
#endif
#undef P
#undef P_ns

#ifdef CONFIG_TICK_ONESHOT
# define P(x) \
	SEQ_printf_at_AEE(m, "  .%-15s: %llu\n", #x, \
		   (unsigned long long)(ts->x))
# define P_ns(x) \
	SEQ_printf_at_AEE(m, "  .%-15s: %llu nsecs\n", #x, \
		   (unsigned long long)(ktime_to_ns(ts->x)))
	{
		struct tick_sched *ts = tick_get_tick_sched(cpu);

		P(nohz_mode);
		P_ns(last_tick);
		P(tick_stopped);
		P(idle_jiffies);
		P(idle_calls);
		P(idle_sleeps);
		P_ns(idle_entrytime);
		P_ns(idle_waketime);
		P_ns(idle_exittime);
		P_ns(idle_sleeptime);
		P_ns(iowait_sleeptime);
		P(last_jiffies);
		P(next_timer);
		P_ns(idle_expires);
		SEQ_printf_at_AEE(m, "jiffies: %llu\n",
			   (unsigned long long)jiffies);
	}
#endif

#undef P
#undef P_ns
	SEQ_printf_at_AEE(m, "\n");
}

#ifdef CONFIG_GENERIC_CLOCKEVENTS
static void
print_tickdevice(struct seq_file *m, struct tick_device *td, int cpu)
{
	struct clock_event_device *dev = td->evtdev;

	touch_nmi_watchdog();

	SEQ_printf_at_AEE(m, "Tick Device: mode:     %d\n", td->mode);
	if (cpu < 0)
		SEQ_printf_at_AEE(m, "Broadcast device\n");
	else
		SEQ_printf_at_AEE(m, "Per CPU device: %d\n", cpu);

	SEQ_printf_at_AEE(m, "Clock Event Device: ");
	if (!dev) {
		SEQ_printf_at_AEE(m, "<NULL>\n");
		return;
	}
	SEQ_printf_at_AEE(m, "%s\n", dev->name);
	SEQ_printf_at_AEE(m, " max_delta_ns:   %llu\n",
		   (unsigned long long) dev->max_delta_ns);
	SEQ_printf_at_AEE(m, " min_delta_ns:   %llu\n",
		   (unsigned long long) dev->min_delta_ns);
	SEQ_printf_at_AEE(m, " mult:           %u\n", dev->mult);
	SEQ_printf_at_AEE(m, " shift:          %u\n", dev->shift);
	SEQ_printf_at_AEE(m, " mode:           %d\n",
			clockevent_get_state(dev));
	SEQ_printf_at_AEE(m, " next_event:     %lld nsecs\n",
		   (unsigned long long) ktime_to_ns(dev->next_event));

	SEQ_printf_at_AEE(m, " set_next_event: ");
	print_name_offset(m, dev->set_next_event, NULL);
	SEQ_printf_at_AEE(m, "\n");

	if (dev->set_state_shutdown) {
		SEQ_printf_at_AEE(m, " shutdown: ");
		print_name_offset(m, dev->set_state_shutdown, NULL);
		SEQ_printf_at_AEE(m, "\n");
	}

	if (dev->set_state_periodic) {
		SEQ_printf_at_AEE(m, " periodic: ");
		print_name_offset(m, dev->set_state_periodic, NULL);
		SEQ_printf_at_AEE(m, "\n");
	}

	if (dev->set_state_oneshot) {
		SEQ_printf_at_AEE(m, " oneshot:  ");
		print_name_offset(m, dev->set_state_oneshot, NULL);
		SEQ_printf_at_AEE(m, "\n");
	}

	if (dev->set_state_oneshot_stopped) {
		SEQ_printf_at_AEE(m, " oneshot stopped: ");
		print_name_offset(m, dev->set_state_oneshot_stopped, NULL);
		SEQ_printf_at_AEE(m, "\n");
	}

	if (dev->tick_resume) {
		SEQ_printf_at_AEE(m, " resume:   ");
		print_name_offset(m, dev->tick_resume, NULL);
		SEQ_printf_at_AEE(m, "\n");
	}

	SEQ_printf_at_AEE(m, " event_handler:  ");
	print_name_offset(m, dev->event_handler, NULL);
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, " retries:        %lu\n", dev->retries);
	SEQ_printf_at_AEE(m, "\n");
}

static void timer_list_show_tickdevices_header(struct seq_file *m)
{
#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
	print_tickdevice(m, tick_get_broadcast_device(), -1);
	SEQ_printf_at_AEE(m, "tick_broadcast_mask: %*pb\n",
			cpumask_pr_args(tick_get_broadcast_mask()));
#ifdef CONFIG_TICK_ONESHOT
	SEQ_printf_at_AEE(m, "tick_broadcast_oneshot_mask: %*pb\n",
			cpumask_pr_args(tick_get_broadcast_oneshot_mask()));
#endif
	SEQ_printf_at_AEE(m, "\n");
#endif
}
#endif

static inline void timer_list_header(struct seq_file *m, u64 now)
{
	SEQ_printf_at_AEE(m, "Timer List Version: v0.8\n");
	SEQ_printf_at_AEE(m, "HRTIMER_MAX_CLOCK_BASES: %d\n",
			HRTIMER_MAX_CLOCK_BASES);
	SEQ_printf_at_AEE(m, "now at %lld nsecs\n", (unsigned long long)now);
	SEQ_printf_at_AEE(m, "\n");
}

void timer_list_aee_dump(int exclude_cpus)
{
	u64 now = ktime_to_ns(ktime_get());
	int cpu;

	timer_list_header(NULL, now);

	for_each_online_cpu(cpu)
		if ((exclude_cpus & (1 << cpu)) == 0)
			print_cpu(NULL, cpu, now);

#ifdef CONFIG_GENERIC_CLOCKEVENTS
	timer_list_show_tickdevices_header(NULL);
	for_each_online_cpu(cpu)
		if ((exclude_cpus & (1 << cpu)) == 0)
			print_tickdevice(NULL, tick_get_device(cpu), cpu);
#endif
}
