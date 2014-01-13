/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/percpu.h>
#include <linux/mm.h>

#include <asm/localtimer.h>
#include <asm/mach/time.h>
#include <asm/sched_clock.h>
#include <asm/smp_plat.h>
#include <asm/user_accessible_timer.h>
#include <mach/msm_iomap.h>
#include <mach/irqs.h>
#include <soc/qcom/socinfo.h>

#include <soc/qcom/smem.h>
#if defined(CONFIG_MSM_SMD)
#include <mach/msm_smsm.h>
#endif
#include "timer.h"

enum {
	MSM_TIMER_DEBUG_SYNC = 1U << 0,
};
static int msm_timer_debug_mask;
module_param_named(debug_mask, msm_timer_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#ifdef CONFIG_MSM7X00A_USE_GP_TIMER
	#define DG_TIMER_RATING 100
#else
	#define DG_TIMER_RATING 300
#endif

#ifndef MSM_TMR0_BASE
#define MSM_TMR0_BASE MSM_TMR_BASE
#endif

#define MSM_DGT_SHIFT (5)

#define TIMER_MATCH_VAL         0x0000
#define TIMER_COUNT_VAL         0x0004
#define TIMER_ENABLE            0x0008
#define TIMER_CLEAR             0x000C
#define DGT_CLK_CTL             0x0034
enum {
	DGT_CLK_CTL_DIV_1 = 0,
	DGT_CLK_CTL_DIV_2 = 1,
	DGT_CLK_CTL_DIV_3 = 2,
	DGT_CLK_CTL_DIV_4 = 3,
};
#define TIMER_STATUS            0x0088
#define TIMER_ENABLE_EN              1
#define TIMER_ENABLE_CLR_ON_MATCH_EN 2

#define LOCAL_TIMER 0
#define GLOBAL_TIMER 1

/*
 * global_timer_offset is added to the regbase of a timer to force the memory
 * access to come from the CPU0 region.
 */
static int global_timer_offset;
static int msm_global_timer;

#define NR_TIMERS ARRAY_SIZE(msm_clocks)

unsigned int gpt_hz = 32768;
unsigned int sclk_hz = 32768;

static struct msm_clock *clockevent_to_clock(struct clock_event_device *evt);
static irqreturn_t msm_timer_interrupt(int irq, void *dev_id);
static cycle_t msm_gpt_read(struct clocksource *cs);
static cycle_t msm_dgt_read(struct clocksource *cs);
static void msm_timer_set_mode(enum clock_event_mode mode,
			       struct clock_event_device *evt);
static int msm_timer_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt);

enum {
	MSM_CLOCK_FLAGS_UNSTABLE_COUNT = 1U << 0,
	MSM_CLOCK_FLAGS_ODD_MATCH_WRITE = 1U << 1,
	MSM_CLOCK_FLAGS_DELAYED_WRITE_POST = 1U << 2,
};

struct msm_clock {
	struct clock_event_device   clockevent;
	struct clocksource          clocksource;
	unsigned int		    irq;
	void __iomem                *regbase;
	uint32_t                    freq;
	uint32_t                    shift;
	uint32_t                    flags;
	uint32_t                    write_delay;
	uint32_t                    rollover_offset;
	uint32_t                    index;
	void __iomem                *global_counter;
	void __iomem                *local_counter;
	uint32_t		    status_mask;
	union {
		struct clock_event_device		*evt;
		struct clock_event_device __percpu	**percpu_evt;
	};
};

enum {
	MSM_CLOCK_GPT,
	MSM_CLOCK_DGT,
};

struct msm_clock_percpu_data {
	uint32_t                  last_set;
	uint32_t                  sleep_offset;
	uint32_t                  alarm_vtime;
	uint32_t                  alarm;
	uint32_t                  non_sleep_offset;
	uint32_t                  in_sync;
	cycle_t                   stopped_tick;
	int                       stopped;
	uint32_t                  last_sync_gpt;
	u64                       last_sync_jiffies;
};

struct msm_timer_sync_data_t {
	struct msm_clock *clock;
	uint32_t         timeout;
	int              exit_sleep;
};

static struct msm_clock msm_clocks[] = {
	[MSM_CLOCK_GPT] = {
		.clockevent = {
			.name           = "gp_timer",
			.features       = CLOCK_EVT_FEAT_ONESHOT,
			.shift          = 32,
			.rating         = 200,
			.set_next_event = msm_timer_set_next_event,
			.set_mode       = msm_timer_set_mode,
		},
		.clocksource = {
			.name           = "gp_timer",
			.rating         = 200,
			.read           = msm_gpt_read,
			.mask           = CLOCKSOURCE_MASK(32),
			.flags          = CLOCK_SOURCE_IS_CONTINUOUS,
		},
		.irq = INT_GP_TIMER_EXP,
		.regbase = MSM_TMR_BASE + 0x4,
		.freq = 32768,
		.index = MSM_CLOCK_GPT,
		.write_delay = 9,
	},
	[MSM_CLOCK_DGT] = {
		.clockevent = {
			.name           = "dg_timer",
			.features       = CLOCK_EVT_FEAT_ONESHOT,
			.shift          = 32,
			.rating         = DG_TIMER_RATING,
			.set_next_event = msm_timer_set_next_event,
			.set_mode       = msm_timer_set_mode,
		},
		.clocksource = {
			.name           = "dg_timer",
			.rating         = DG_TIMER_RATING,
			.read           = msm_dgt_read,
			.mask           = CLOCKSOURCE_MASK(32),
			.flags          = CLOCK_SOURCE_IS_CONTINUOUS,
		},
		.irq = INT_DEBUG_TIMER_EXP,
		.regbase = MSM_TMR_BASE + 0x24,
		.index = MSM_CLOCK_DGT,
		.write_delay = 9,
	}
};

static DEFINE_PER_CPU(struct msm_clock_percpu_data[NR_TIMERS],
    msm_clocks_percpu);

static DEFINE_PER_CPU(struct msm_clock *, msm_active_clock);

static irqreturn_t msm_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = *(struct clock_event_device **)dev_id;
	if (evt->event_handler == NULL)
		return IRQ_HANDLED;
	evt->event_handler(evt);
	return IRQ_HANDLED;
}

static uint32_t msm_read_timer_count(struct msm_clock *clock, int global)
{
	uint32_t t1, t2, t3;
	int loop_count = 0;
	void __iomem *addr = clock->regbase + TIMER_COUNT_VAL +
		global*global_timer_offset;

	if (!(clock->flags & MSM_CLOCK_FLAGS_UNSTABLE_COUNT))
		return __raw_readl_no_log(addr);

	t1 = __raw_readl_no_log(addr);
	t2 = __raw_readl_no_log(addr);
	if ((t2-t1) <= 1)
		return t2;
	while (1) {
		t1 = __raw_readl_no_log(addr);
		t2 = __raw_readl_no_log(addr);
		t3 = __raw_readl_no_log(addr);
		cpu_relax();
		if ((t3-t2) <= 1)
			return t3;
		if ((t2-t1) <= 1)
			return t2;
		if ((t2 >= t1) && (t3 >= t2))
			return t2;
		if (++loop_count == 5) {
			pr_err("msm_read_timer_count timer %s did not "
			       "stabilize: %u -> %u -> %u\n",
			       clock->clockevent.name, t1, t2, t3);
			return t3;
		}
	}
}

static cycle_t msm_gpt_read(struct clocksource *cs)
{
	struct msm_clock *clock = &msm_clocks[MSM_CLOCK_GPT];
	struct msm_clock_percpu_data *clock_state =
		&per_cpu(msm_clocks_percpu, 0)[MSM_CLOCK_GPT];

	if (clock_state->stopped)
		return clock_state->stopped_tick;

	return msm_read_timer_count(clock, GLOBAL_TIMER) +
		clock_state->sleep_offset;
}

static cycle_t msm_dgt_read(struct clocksource *cs)
{
	struct msm_clock *clock = &msm_clocks[MSM_CLOCK_DGT];
	struct msm_clock_percpu_data *clock_state =
		&per_cpu(msm_clocks_percpu, 0)[MSM_CLOCK_DGT];

	if (clock_state->stopped)
		return clock_state->stopped_tick >> clock->shift;

	return (msm_read_timer_count(clock, GLOBAL_TIMER) +
		clock_state->sleep_offset) >> clock->shift;
}

static struct msm_clock *clockevent_to_clock(struct clock_event_device *evt)
{
	int i;

	if (!is_smp())
		return container_of(evt, struct msm_clock, clockevent);

	for (i = 0; i < NR_TIMERS; i++)
		if (evt == &(msm_clocks[i].clockevent))
			return &msm_clocks[i];
	return &msm_clocks[msm_global_timer];
}

static int msm_timer_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
	int i;
	struct msm_clock *clock;
	struct msm_clock_percpu_data *clock_state;
	uint32_t now;
	uint32_t alarm;
	int late;

	clock = clockevent_to_clock(evt);
	clock_state = &__get_cpu_var(msm_clocks_percpu)[clock->index];
	now = msm_read_timer_count(clock, LOCAL_TIMER);
	alarm = now + (cycles << clock->shift);
	if (clock->flags & MSM_CLOCK_FLAGS_ODD_MATCH_WRITE)
		while (now == clock_state->last_set)
			now = msm_read_timer_count(clock, LOCAL_TIMER);

	clock_state->alarm = alarm;
	__raw_writel(alarm, clock->regbase + TIMER_MATCH_VAL);

	if (clock->flags & MSM_CLOCK_FLAGS_DELAYED_WRITE_POST) {
		/* read the counter four extra times to make sure write posts
		   before reading the time */
		for (i = 0; i < 4; i++)
			__raw_readl_no_log(clock->regbase + TIMER_COUNT_VAL);
	}
	now = msm_read_timer_count(clock, LOCAL_TIMER);
	clock_state->last_set = now;
	clock_state->alarm_vtime = alarm + clock_state->sleep_offset;
	late = now - alarm;
	if (late >= (int)(-clock->write_delay << clock->shift) &&
	    late < clock->freq*5)
		return -ETIME;

	return 0;
}

static void msm_timer_set_mode(enum clock_event_mode mode,
			       struct clock_event_device *evt)
{
	struct msm_clock *clock;
	struct msm_clock **cur_clock;
	struct msm_clock_percpu_data *clock_state, *gpt_state;
	unsigned long irq_flags;
	struct irq_chip *chip;

	clock = clockevent_to_clock(evt);
	clock_state = &__get_cpu_var(msm_clocks_percpu)[clock->index];
	gpt_state = &__get_cpu_var(msm_clocks_percpu)[MSM_CLOCK_GPT];

	local_irq_save(irq_flags);

	switch (mode) {
	case CLOCK_EVT_MODE_RESUME:
	case CLOCK_EVT_MODE_PERIODIC:
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		clock_state->stopped = 0;
		clock_state->sleep_offset =
			-msm_read_timer_count(clock, LOCAL_TIMER) +
			clock_state->stopped_tick;
		get_cpu_var(msm_active_clock) = clock;
		put_cpu_var(msm_active_clock);
		__raw_writel(TIMER_ENABLE_EN, clock->regbase + TIMER_ENABLE);
		chip = irq_get_chip(clock->irq);
		if (chip && chip->irq_unmask)
			chip->irq_unmask(irq_get_irq_data(clock->irq));
		if (clock != &msm_clocks[MSM_CLOCK_GPT])
			__raw_writel(TIMER_ENABLE_EN,
				msm_clocks[MSM_CLOCK_GPT].regbase +
			       TIMER_ENABLE);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		cur_clock = &get_cpu_var(msm_active_clock);
		if (*cur_clock == clock)
			*cur_clock = NULL;
		put_cpu_var(msm_active_clock);
		clock_state->in_sync = 0;
		clock_state->stopped = 1;
		clock_state->stopped_tick =
			msm_read_timer_count(clock, LOCAL_TIMER) +
			clock_state->sleep_offset;
		__raw_writel(0, clock->regbase + TIMER_MATCH_VAL);
		chip = irq_get_chip(clock->irq);
		if (chip && chip->irq_mask)
			chip->irq_mask(irq_get_irq_data(clock->irq));

		if (!is_smp() || clock != &msm_clocks[MSM_CLOCK_DGT]
				|| smp_processor_id())
			__raw_writel(0, clock->regbase + TIMER_ENABLE);

		if (msm_global_timer == MSM_CLOCK_DGT &&
		    clock != &msm_clocks[MSM_CLOCK_GPT]) {
			gpt_state->in_sync = 0;
			__raw_writel(0, msm_clocks[MSM_CLOCK_GPT].regbase +
			       TIMER_ENABLE);
		}
		break;
	}
	wmb();
	local_irq_restore(irq_flags);
}

void __iomem *msm_timer_get_timer0_base(void)
{
	return MSM_TMR_BASE + global_timer_offset;
}

#define MPM_SCLK_COUNT_VAL    0x0024

#ifdef CONFIG_PM
/*
 * Retrieve the cycle count from sclk and optionally synchronize local clock
 * with the sclk value.
 *
 * time_start and time_expired are callbacks that must be specified.  The
 * protocol uses them to detect timeout.  The update callback is optional.
 * If not NULL, update will be called so that it can update local clock.
 *
 * The function does not use the argument data directly; it passes data to
 * the callbacks.
 *
 * Return value:
 *      0: the operation failed
 *      >0: the slow clock value after time-sync
 */
static void (*msm_timer_sync_timeout)(void);
#if defined(CONFIG_MSM_DIRECT_SCLK_ACCESS)
uint32_t msm_timer_get_sclk_ticks(void)
{
	uint32_t t1, t2;
	int loop_count = 10;
	int loop_zero_count = 3;
	int tmp = USEC_PER_SEC;
	do_div(tmp, sclk_hz);
	tmp /= (loop_zero_count-1);

	while (loop_zero_count--) {
		t1 = __raw_readl_no_log(MSM_RPM_MPM_BASE + MPM_SCLK_COUNT_VAL);
		do {
			udelay(1);
			t2 = t1;
			t1 = __raw_readl_no_log(
				MSM_RPM_MPM_BASE + MPM_SCLK_COUNT_VAL);
		} while ((t2 != t1) && --loop_count);

		if (!loop_count) {
			printk(KERN_EMERG "SCLK  did not stabilize\n");
			return 0;
		}

		if (t1)
			break;

		udelay(tmp);
	}

	if (!loop_zero_count) {
		printk(KERN_EMERG "SCLK reads zero\n");
		return 0;
	}

	return t1;
}

static uint32_t msm_timer_do_sync_to_sclk(
	void (*time_start)(struct msm_timer_sync_data_t *data),
	bool (*time_expired)(struct msm_timer_sync_data_t *data),
	void (*update)(struct msm_timer_sync_data_t *, uint32_t, uint32_t),
	struct msm_timer_sync_data_t *data)
{
	unsigned t1 = msm_timer_get_sclk_ticks();

	if (t1 && update != NULL)
		update(data, t1, sclk_hz);
	return t1;
}
#else

/* Time Master State Bits */
#define MASTER_BITS_PER_CPU        1
#define MASTER_TIME_PENDING \
	(0x01UL << (MASTER_BITS_PER_CPU * SMSM_APPS_STATE))

/* Time Slave State Bits */
#define SLAVE_TIME_REQUEST         0x0400
#define SLAVE_TIME_POLL            0x0800
#define SLAVE_TIME_INIT            0x1000

static uint32_t msm_timer_do_sync_to_sclk(
	void (*time_start)(struct msm_timer_sync_data_t *data),
	bool (*time_expired)(struct msm_timer_sync_data_t *data),
	void (*update)(struct msm_timer_sync_data_t *, uint32_t, uint32_t),
	struct msm_timer_sync_data_t *data)
{
	uint32_t *smem_clock;
	uint32_t smem_clock_val;
	uint32_t state;

	smem_clock = smem_find(SMEM_SMEM_SLOW_CLOCK_VALUE,
				sizeof(uint32_t), 0, SMEM_ANY_HOST_FLAG);
	if (smem_clock == NULL) {
		printk(KERN_ERR "no smem clock\n");
		return 0;
	}

	state = smsm_get_state(SMSM_MODEM_STATE);
	if ((state & SMSM_INIT) == 0) {
		printk(KERN_ERR "smsm not initialized\n");
		return 0;
	}

	time_start(data);
	while ((state = smsm_get_state(SMSM_TIME_MASTER_DEM)) &
		MASTER_TIME_PENDING) {
		if (time_expired(data)) {
			printk(KERN_EMERG "get_smem_clock: timeout 1 still "
				"invalid state %x\n", state);
			msm_timer_sync_timeout();
		}
	}

	smsm_change_state(SMSM_APPS_DEM, SLAVE_TIME_POLL | SLAVE_TIME_INIT,
		SLAVE_TIME_REQUEST);

	time_start(data);
	while (!((state = smsm_get_state(SMSM_TIME_MASTER_DEM)) &
		MASTER_TIME_PENDING)) {
		if (time_expired(data)) {
			printk(KERN_EMERG "get_smem_clock: timeout 2 still "
				"invalid state %x\n", state);
			msm_timer_sync_timeout();
		}
	}

	smsm_change_state(SMSM_APPS_DEM, SLAVE_TIME_REQUEST, SLAVE_TIME_POLL);

	time_start(data);
	do {
		smem_clock_val = *smem_clock;
	} while (smem_clock_val == 0 && !time_expired(data));

	state = smsm_get_state(SMSM_TIME_MASTER_DEM);

	if (smem_clock_val) {
		if (update != NULL)
			update(data, smem_clock_val, sclk_hz);

		if (msm_timer_debug_mask & MSM_TIMER_DEBUG_SYNC)
			printk(KERN_INFO
				"get_smem_clock: state %x clock %u\n",
				state, smem_clock_val);
	} else {
		printk(KERN_EMERG
			"get_smem_clock: timeout state %x clock %u\n",
			state, smem_clock_val);
		msm_timer_sync_timeout();
	}

	smsm_change_state(SMSM_APPS_DEM, SLAVE_TIME_REQUEST | SLAVE_TIME_POLL,
		SLAVE_TIME_INIT);
	return smem_clock_val;
}
#endif /* CONFIG_MSM_DIRECT_SCLK_ACCESS */

/*
 * Callback function that initializes the timeout value.
 */
static void msm_timer_sync_to_sclk_time_start(
	struct msm_timer_sync_data_t *data)
{
	/* approx 2 seconds */
	uint32_t delta = data->clock->freq << data->clock->shift << 1;
	data->timeout = msm_read_timer_count(data->clock, LOCAL_TIMER) + delta;
}

/*
 * Callback function that checks the timeout.
 */
static bool msm_timer_sync_to_sclk_time_expired(
	struct msm_timer_sync_data_t *data)
{
	uint32_t delta = msm_read_timer_count(data->clock, LOCAL_TIMER) -
		data->timeout;
	return ((int32_t) delta) > 0;
}

/*
 * Callback function that updates local clock from the specified source clock
 * value and frequency.
 */
static void msm_timer_sync_update(struct msm_timer_sync_data_t *data,
	uint32_t src_clk_val, uint32_t src_clk_freq)
{
	struct msm_clock *dst_clk = data->clock;
	struct msm_clock_percpu_data *dst_clk_state =
		&__get_cpu_var(msm_clocks_percpu)[dst_clk->index];
	uint32_t dst_clk_val = msm_read_timer_count(dst_clk, LOCAL_TIMER);
	uint32_t new_offset;

	if ((dst_clk->freq << dst_clk->shift) == src_clk_freq) {
		new_offset = src_clk_val - dst_clk_val;
	} else {
		uint64_t temp;

		/* separate multiplication and division steps to reduce
		   rounding error */
		temp = src_clk_val;
		temp *= dst_clk->freq << dst_clk->shift;
		do_div(temp, src_clk_freq);

		new_offset = (uint32_t)(temp) - dst_clk_val;
	}

	if (dst_clk_state->sleep_offset + dst_clk_state->non_sleep_offset !=
	    new_offset) {
		if (data->exit_sleep)
			dst_clk_state->sleep_offset =
				new_offset - dst_clk_state->non_sleep_offset;
		else
			dst_clk_state->non_sleep_offset =
				new_offset - dst_clk_state->sleep_offset;

		if (msm_timer_debug_mask & MSM_TIMER_DEBUG_SYNC)
			printk(KERN_INFO "sync clock %s: "
				"src %u, new offset %u + %u\n",
				dst_clk->clocksource.name, src_clk_val,
				dst_clk_state->sleep_offset,
				dst_clk_state->non_sleep_offset);
	}
}

/*
 * Synchronize GPT clock with sclk.
 */
static void msm_timer_sync_gpt_to_sclk(int exit_sleep)
{
	struct msm_clock *gpt_clk = &msm_clocks[MSM_CLOCK_GPT];
	struct msm_clock_percpu_data *gpt_clk_state =
		&__get_cpu_var(msm_clocks_percpu)[MSM_CLOCK_GPT];
	struct msm_timer_sync_data_t data;
	uint32_t ret;

	if (gpt_clk_state->in_sync)
		return;

	data.clock = gpt_clk;
	data.timeout = 0;
	data.exit_sleep = exit_sleep;

	ret = msm_timer_do_sync_to_sclk(
		msm_timer_sync_to_sclk_time_start,
		msm_timer_sync_to_sclk_time_expired,
		msm_timer_sync_update,
		&data);

	if (ret)
		gpt_clk_state->in_sync = 1;
}

/*
 * Synchronize clock with GPT clock.
 */
static void msm_timer_sync_to_gpt(struct msm_clock *clock, int exit_sleep)
{
	struct msm_clock *gpt_clk = &msm_clocks[MSM_CLOCK_GPT];
	struct msm_clock_percpu_data *gpt_clk_state =
		&__get_cpu_var(msm_clocks_percpu)[MSM_CLOCK_GPT];
	struct msm_clock_percpu_data *clock_state =
		&__get_cpu_var(msm_clocks_percpu)[clock->index];
	struct msm_timer_sync_data_t data;
	uint32_t gpt_clk_val;
	u64 gpt_period = (1ULL << 32) * HZ;
	u64 now = get_jiffies_64();

	do_div(gpt_period, gpt_hz);

	BUG_ON(clock == gpt_clk);

	if (clock_state->in_sync &&
		(now - clock_state->last_sync_jiffies < (gpt_period >> 1)))
		return;

	gpt_clk_val = msm_read_timer_count(gpt_clk, LOCAL_TIMER)
		+ gpt_clk_state->sleep_offset + gpt_clk_state->non_sleep_offset;

	if (exit_sleep && gpt_clk_val < clock_state->last_sync_gpt)
		clock_state->non_sleep_offset -= clock->rollover_offset;

	data.clock = clock;
	data.timeout = 0;
	data.exit_sleep = exit_sleep;

	msm_timer_sync_update(&data, gpt_clk_val, gpt_hz);

	clock_state->in_sync = 1;
	clock_state->last_sync_gpt = gpt_clk_val;
	clock_state->last_sync_jiffies = now;
}

static void msm_timer_reactivate_alarm(struct msm_clock *clock)
{
	struct msm_clock_percpu_data *clock_state =
		&__get_cpu_var(msm_clocks_percpu)[clock->index];
	long alarm_delta = clock_state->alarm_vtime -
		clock_state->sleep_offset -
		msm_read_timer_count(clock, LOCAL_TIMER);
	alarm_delta >>= clock->shift;
	if (alarm_delta < (long)clock->write_delay + 4)
		alarm_delta = clock->write_delay + 4;
	while (msm_timer_set_next_event(alarm_delta, &clock->clockevent))
		;
}

int64_t msm_timer_enter_idle(void)
{
	struct msm_clock *gpt_clk = &msm_clocks[MSM_CLOCK_GPT];
	struct msm_clock *clock = __get_cpu_var(msm_active_clock);
	struct msm_clock_percpu_data *clock_state =
		&__get_cpu_var(msm_clocks_percpu)[clock->index];
	uint32_t alarm;
	uint32_t count;
	int32_t delta;

	BUG_ON(clock != &msm_clocks[MSM_CLOCK_GPT] &&
		clock != &msm_clocks[MSM_CLOCK_DGT]);

	msm_timer_sync_gpt_to_sclk(0);
	if (clock != gpt_clk)
		msm_timer_sync_to_gpt(clock, 0);

	count = msm_read_timer_count(clock, LOCAL_TIMER);
	if (clock_state->stopped++ == 0)
		clock_state->stopped_tick = count + clock_state->sleep_offset;
	alarm = clock_state->alarm;
	delta = alarm - count;
	if (delta <= -(int32_t)((clock->freq << clock->shift) >> 10)) {
		/* timer should have triggered 1ms ago */
		printk(KERN_ERR "msm_timer_enter_idle: timer late %d, "
			"reprogram it\n", delta);
		msm_timer_reactivate_alarm(clock);
	}
	if (delta <= 0)
		return 0;
	return clocksource_cyc2ns((alarm - count) >> clock->shift,
		      clock->clocksource.mult,
		      clock->clocksource.shift);
}

void msm_timer_exit_idle(int low_power)
{
	struct msm_clock *gpt_clk = &msm_clocks[MSM_CLOCK_GPT];
	struct msm_clock *clock = __get_cpu_var(msm_active_clock);
	struct msm_clock_percpu_data *gpt_clk_state =
		&__get_cpu_var(msm_clocks_percpu)[MSM_CLOCK_GPT];
	struct msm_clock_percpu_data *clock_state =
		&__get_cpu_var(msm_clocks_percpu)[clock->index];
	uint32_t enabled;

	BUG_ON(clock != &msm_clocks[MSM_CLOCK_GPT] &&
		clock != &msm_clocks[MSM_CLOCK_DGT]);

	if (!low_power)
		goto exit_idle_exit;

	enabled = __raw_readl(gpt_clk->regbase + TIMER_ENABLE) &
			      TIMER_ENABLE_EN;
	if (!enabled)
		__raw_writel(TIMER_ENABLE_EN, gpt_clk->regbase + TIMER_ENABLE);

#if defined(CONFIG_ARCH_MSM_SCORPION) || defined(CONFIG_ARCH_MSM_KRAIT)
	gpt_clk_state->in_sync = 0;
#else
	gpt_clk_state->in_sync = gpt_clk_state->in_sync && enabled;
#endif
	/* Make sure timer is actually enabled before we sync it */
	wmb();
	msm_timer_sync_gpt_to_sclk(1);

	if (clock == gpt_clk)
		goto exit_idle_alarm;

	enabled = __raw_readl(clock->regbase + TIMER_ENABLE) & TIMER_ENABLE_EN;
	if (!enabled)
		__raw_writel(TIMER_ENABLE_EN, clock->regbase + TIMER_ENABLE);

#if defined(CONFIG_ARCH_MSM_SCORPION) || defined(CONFIG_ARCH_MSM_KRAIT)
	clock_state->in_sync = 0;
#else
	clock_state->in_sync = clock_state->in_sync && enabled;
#endif
	/* Make sure timer is actually enabled before we sync it */
	wmb();
	msm_timer_sync_to_gpt(clock, 1);

exit_idle_alarm:
	msm_timer_reactivate_alarm(clock);

exit_idle_exit:
	clock_state->stopped--;
}

/*
 * Callback function that initializes the timeout value.
 */
static void msm_timer_get_sclk_time_start(
	struct msm_timer_sync_data_t *data)
{
	data->timeout = 200000;
}

/*
 * Callback function that checks the timeout.
 */
static bool msm_timer_get_sclk_time_expired(
	struct msm_timer_sync_data_t *data)
{
	udelay(10);
	return --data->timeout <= 0;
}

/*
 * Retrieve the cycle count from the sclk and convert it into
 * nanoseconds.
 *
 * On exit, if period is not NULL, it contains the period of the
 * sclk in nanoseconds, i.e. how long the cycle count wraps around.
 *
 * Return value:
 *      0: the operation failed; period is not set either
 *      >0: time in nanoseconds
 */
int64_t msm_timer_get_sclk_time(int64_t *period)
{
	struct msm_timer_sync_data_t data;
	uint32_t clock_value;
	int64_t tmp;

	memset(&data, 0, sizeof(data));
	clock_value = msm_timer_do_sync_to_sclk(
		msm_timer_get_sclk_time_start,
		msm_timer_get_sclk_time_expired,
		NULL,
		&data);

	if (!clock_value)
		return 0;

	if (period) {
		tmp = 1LL << 32;
		tmp *= NSEC_PER_SEC;
		do_div(tmp, sclk_hz);
		*period = tmp;
	}

	tmp = (int64_t)clock_value;
	tmp *= NSEC_PER_SEC;
	do_div(tmp, sclk_hz);
	return tmp;
}

int __init msm_timer_init_time_sync(void (*timeout)(void))
{
#if !defined(CONFIG_MSM_DIRECT_SCLK_ACCESS)
	int ret = smsm_change_intr_mask(SMSM_TIME_MASTER_DEM, 0xFFFFFFFF, 0);

	if (ret) {
		printk(KERN_ERR	"%s: failed to clear interrupt mask, %d\n",
			__func__, ret);
		return ret;
	}

	smsm_change_state(SMSM_APPS_DEM,
		SLAVE_TIME_REQUEST | SLAVE_TIME_POLL, SLAVE_TIME_INIT);
#endif

	BUG_ON(timeout == NULL);
	msm_timer_sync_timeout = timeout;

	return 0;
}

#endif

static u32 notrace msm_read_sched_clock(void)
{
	struct msm_clock *clock = &msm_clocks[msm_global_timer];
	struct clocksource *cs = &clock->clocksource;
	return cs->read(NULL);
}

static struct delay_timer msm_delay_timer;

static unsigned long msm_read_current_timer(void)
{
	struct msm_clock *dgt = &msm_clocks[MSM_CLOCK_DGT];
	return msm_read_timer_count(dgt, GLOBAL_TIMER);
}

static void __init msm_sched_clock_init(void)
{
	struct msm_clock *clock = &msm_clocks[msm_global_timer];

	setup_sched_clock(msm_read_sched_clock, 32 - clock->shift, clock->freq);
}

#ifdef CONFIG_LOCAL_TIMERS
int __cpuinit local_timer_setup(struct clock_event_device *evt)
{
	static DEFINE_PER_CPU(bool, first_boot) = true;
	struct msm_clock *clock = &msm_clocks[msm_global_timer];

	/* Use existing clock_event for cpu 0 */
	if (!smp_processor_id())
		return 0;

	if (cpu_is_msm8x60() || soc_class_is_msm8960() ||
	    soc_class_is_apq8064() || soc_class_is_msm8930())
		__raw_writel(DGT_CLK_CTL_DIV_4, MSM_TMR_BASE + DGT_CLK_CTL);

	if (__get_cpu_var(first_boot)) {
		__raw_writel(0, clock->regbase  + TIMER_ENABLE);
		__raw_writel(0, clock->regbase + TIMER_CLEAR);
		__raw_writel(~0, clock->regbase + TIMER_MATCH_VAL);
		__get_cpu_var(first_boot) = false;
		if (clock->status_mask)
			while (__raw_readl(MSM_TMR_BASE + TIMER_STATUS) &
			       clock->status_mask)
				;
	}
	evt->irq = clock->irq;
	evt->name = "local_timer";
	evt->features = CLOCK_EVT_FEAT_ONESHOT;
	evt->rating = clock->clockevent.rating;
	evt->set_mode = msm_timer_set_mode;
	evt->set_next_event = msm_timer_set_next_event;

	*__this_cpu_ptr(clock->percpu_evt) = evt;
	clockevents_config_and_register(evt, gpt_hz, 4, 0xf0000000);
	enable_percpu_irq(evt->irq, IRQ_TYPE_EDGE_RISING);

	return 0;
}

void local_timer_stop(struct clock_event_device *evt)
{
	evt->set_mode(CLOCK_EVT_MODE_UNUSED, evt);
	disable_percpu_irq(evt->irq);
}

static struct local_timer_ops msm_lt_ops = {
	local_timer_setup,
	local_timer_stop,
};
#endif /* CONFIG_LOCAL_TIMERS */

#ifdef CONFIG_ARCH_MSM8625
static void fixup_msm8625_timer(void)
{
	struct msm_clock *dgt = &msm_clocks[MSM_CLOCK_DGT];
	struct msm_clock *gpt = &msm_clocks[MSM_CLOCK_GPT];
	dgt->irq = MSM8625_INT_DEBUG_TIMER_EXP;
	gpt->irq = MSM8625_INT_GP_TIMER_EXP;
	global_timer_offset =  MSM_TMR0_BASE - MSM_TMR_BASE;
}
#else
static inline void fixup_msm8625_timer(void) { };
#endif

void __init msm_timer_init(void)
{
	int i;
	int res;
	struct irq_chip *chip;
	struct msm_clock *dgt = &msm_clocks[MSM_CLOCK_DGT];
	struct msm_clock *gpt = &msm_clocks[MSM_CLOCK_GPT];

	if (cpu_is_msm7x01() || cpu_is_msm7x25() || cpu_is_msm7x27() ||
	    cpu_is_msm7x25a() || cpu_is_msm7x27a() || cpu_is_msm7x25aa() ||
	    cpu_is_msm7x27aa() || cpu_is_msm8625() || cpu_is_msm7x25ab() ||
	    cpu_is_msm8625q()) {
		dgt->shift = MSM_DGT_SHIFT;
		dgt->freq = 19200000 >> MSM_DGT_SHIFT;
		dgt->clockevent.shift = 32 + MSM_DGT_SHIFT;
		dgt->clocksource.mask = CLOCKSOURCE_MASK(32 - MSM_DGT_SHIFT);
		gpt->regbase = MSM_TMR_BASE;
		dgt->regbase = MSM_TMR_BASE + 0x10;
		gpt->flags |= MSM_CLOCK_FLAGS_UNSTABLE_COUNT
			   |  MSM_CLOCK_FLAGS_ODD_MATCH_WRITE
			   |  MSM_CLOCK_FLAGS_DELAYED_WRITE_POST;
		if (cpu_is_msm8625() || cpu_is_msm8625q())
			fixup_msm8625_timer();
	} else if (cpu_is_qsd8x50()) {
		dgt->freq = 4800000;
		gpt->regbase = MSM_TMR_BASE;
		dgt->regbase = MSM_TMR_BASE + 0x10;
	} else if (cpu_is_fsm9xxx())
		dgt->freq = 4800000;
	else if (cpu_is_msm7x30() || cpu_is_msm8x55()) {
		gpt->status_mask = BIT(10);
		dgt->status_mask = BIT(2);
		dgt->freq = 6144000;
	} else if (cpu_is_msm8x60()) {
		global_timer_offset = MSM_TMR0_BASE - MSM_TMR_BASE;
		gpt->status_mask = BIT(10);
		dgt->status_mask = BIT(2);
		dgt->freq = 6750000;
		__raw_writel(DGT_CLK_CTL_DIV_4, MSM_TMR_BASE + DGT_CLK_CTL);
	} else if (cpu_is_msm9615()) {
		dgt->freq = 6750000;
		__raw_writel(DGT_CLK_CTL_DIV_4, MSM_TMR_BASE + DGT_CLK_CTL);
		gpt->status_mask = BIT(10);
		dgt->status_mask = BIT(2);
		gpt->freq = 32765;
		gpt_hz = 32765;
		sclk_hz = 32765;
		gpt->flags |= MSM_CLOCK_FLAGS_UNSTABLE_COUNT;
		dgt->flags |= MSM_CLOCK_FLAGS_UNSTABLE_COUNT;
	} else if (soc_class_is_msm8960() || soc_class_is_apq8064() ||
		   soc_class_is_msm8930()) {
		global_timer_offset = MSM_TMR0_BASE - MSM_TMR_BASE;
		dgt->freq = 6750000;
		__raw_writel(DGT_CLK_CTL_DIV_4, MSM_TMR_BASE + DGT_CLK_CTL);
		gpt->status_mask = BIT(10);
		dgt->status_mask = BIT(2);
		if (!soc_class_is_apq8064()) {
			gpt->freq = 32765;
			gpt_hz = 32765;
			sclk_hz = 32765;
		}
		if (!soc_class_is_msm8930() && !cpu_is_msm8960ab()) {
			gpt->flags |= MSM_CLOCK_FLAGS_UNSTABLE_COUNT;
			dgt->flags |= MSM_CLOCK_FLAGS_UNSTABLE_COUNT;
		}
	} else {
		WARN(1, "Timer running on unknown hardware. Configure this! "
			"Assuming default configuration.\n");
		dgt->freq = 6750000;
	}

	if (msm_clocks[MSM_CLOCK_GPT].clocksource.rating > DG_TIMER_RATING)
		msm_global_timer = MSM_CLOCK_GPT;
	else
		msm_global_timer = MSM_CLOCK_DGT;

	for (i = 0; i < ARRAY_SIZE(msm_clocks); i++) {
		struct msm_clock *clock = &msm_clocks[i];
		struct clock_event_device *ce = &clock->clockevent;
		struct clocksource *cs = &clock->clocksource;
		__raw_writel(0, clock->regbase + TIMER_ENABLE);
		__raw_writel(0, clock->regbase + TIMER_CLEAR);
		__raw_writel(~0, clock->regbase + TIMER_MATCH_VAL);

		if ((clock->freq << clock->shift) == gpt_hz) {
			clock->rollover_offset = 0;
		} else {
			uint64_t temp;

			temp = clock->freq << clock->shift;
			temp <<= 32;
			do_div(temp, gpt_hz);

			clock->rollover_offset = (uint32_t) temp;
		}

		ce->mult = div_sc(clock->freq, NSEC_PER_SEC, ce->shift);
		/* allow at least 10 seconds to notice that the timer wrapped */
		ce->max_delta_ns =
			clockevent_delta2ns(0xf0000000 >> clock->shift, ce);
		/* ticks gets rounded down by one */
		ce->min_delta_ns =
			clockevent_delta2ns(clock->write_delay + 4, ce);
		ce->cpumask = cpumask_of(0);

		res = clocksource_register_hz(cs, clock->freq);
		if (res)
			printk(KERN_ERR "msm_timer_init: clocksource_register "
			       "failed for %s\n", cs->name);

		ce->irq = clock->irq;
		if (cpu_is_msm8x60() || cpu_is_msm9615() || cpu_is_msm8625() ||
		    cpu_is_msm8625q() || soc_class_is_msm8960() ||
		    soc_class_is_apq8064() || soc_class_is_msm8930()) {
			clock->percpu_evt = alloc_percpu(struct clock_event_device *);
			if (!clock->percpu_evt) {
				pr_err("msm_timer_init: memory allocation "
				       "failed for %s\n", ce->name);
				continue;
			}

			*__this_cpu_ptr(clock->percpu_evt) = ce;
			res = request_percpu_irq(ce->irq, msm_timer_interrupt,
						 ce->name, clock->percpu_evt);
			if (!res)
				enable_percpu_irq(ce->irq,
						 IRQ_TYPE_EDGE_RISING);
		} else {
			clock->evt = ce;
			res = request_irq(ce->irq, msm_timer_interrupt,
					  IRQF_TIMER | IRQF_NOBALANCING | IRQF_TRIGGER_RISING,
					  ce->name, &clock->evt);
		}

		if (res)
			pr_err("msm_timer_init: request_irq failed for %s\n",
			       ce->name);

		chip = irq_get_chip(clock->irq);
		if (chip && chip->irq_mask)
			chip->irq_mask(irq_get_irq_data(clock->irq));

		if (clock->status_mask)
			while (__raw_readl(MSM_TMR_BASE + TIMER_STATUS) &
			       clock->status_mask)
				;

		clockevents_register_device(ce);
	}
	msm_sched_clock_init();

	if (use_user_accessible_timers()) {
		if (cpu_is_msm8960() || cpu_is_msm8930() || cpu_is_apq8064()) {
			struct msm_clock *gtclock = &msm_clocks[MSM_CLOCK_GPT];
			void __iomem *addr = gtclock->regbase +
				TIMER_COUNT_VAL + global_timer_offset;
			setup_user_timer_offset(virt_to_phys(addr)&0xfff);
			set_user_accessible_timer_flag(true);
		}
	}

	if (is_smp()) {
		__raw_writel(1,
			msm_clocks[MSM_CLOCK_DGT].regbase + TIMER_ENABLE);
		msm_delay_timer.freq = dgt->freq;
		msm_delay_timer.read_current_timer = &msm_read_current_timer;
		register_current_timer_delay(&msm_delay_timer);
	}

#ifdef CONFIG_LOCAL_TIMERS
	local_timer_register(&msm_lt_ops);
#endif
}
