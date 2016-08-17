/*
 * arch/arm/mach-tegra/tegra2_statmon.c
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/device.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/io.h>
#include <mach/clk.h>

#include "clock.h"
#include "tegra2_statmon.h"

#define COP_MON_CTRL		0x120
#define COP_MON_STATUS		0x124

#define SAMPLE_PERIOD_SHIFT	20
#define SAMPLE_PERIOD_MASK	(0xFF << SAMPLE_PERIOD_SHIFT)
#define INT_STATUS		BIT(29) /* write 1 to clear */
#define INT_ENABLE		BIT(30)
#define MON_ENABLE		BIT(31)

#define WINDOW_SIZE		128
#define FREQ_MULT		1000
#define UPPER_BAND		1000
#define LOWER_BAND		1000
#define BOOST_FRACTION_BITS	8

struct sampler {
	struct clk	*clock;
	unsigned long	active_cycles[WINDOW_SIZE];
	unsigned long	total_active_cycles;
	unsigned long	avg_freq;
	unsigned long	*last_sample;
	unsigned long	idle_cycles;
	unsigned long	boost_freq;
	unsigned long	bumped_freq;
	unsigned long	*table;
	int		table_size;
	u32		sample_count;
	bool		enable;
	int		sample_time;
	int		window_ms;
	int		min_samples;
	unsigned long	boost_step;
	u8		boost_inc_coef;
	u8		boost_dec_coef;
};

struct tegra2_stat_mon {
	void __iomem	*stat_mon_base;
	void __iomem	*vde_mon_base;
	struct clk	*stat_mon_clock;
	struct mutex	stat_mon_lock;
	struct sampler	avp_sampler;
};

static unsigned long sclk_table[] = {
	300000,
	240000,
	200000,
	150000,
	120000,
	100000,
	80000,
	75000,
	60000,
	50000,
	48000,
	40000
};

static struct tegra2_stat_mon *stat_mon;

static inline u32 tegra2_stat_mon_read(u32 offset)
{
	return readl(stat_mon->stat_mon_base + offset);
}

static inline void tegra2_stat_mon_write(u32 value, u32 offset)
{
	writel(value, stat_mon->stat_mon_base + offset);
}

static inline u32 tegra2_vde_mon_read(u32 offset)
{
	return readl(stat_mon->vde_mon_base + offset);
}

static inline void tegra2_vde_mon_write(u32 value, u32 offset)
{
	writel(value, stat_mon->vde_mon_base + offset);
}

/* read the ticks in ISR and store */
static irqreturn_t stat_mon_isr(int irq, void *data)
{
	u32 reg_val;

	/* disable AVP monitor */
	reg_val = tegra2_stat_mon_read(COP_MON_CTRL);
	reg_val |= INT_STATUS;
	tegra2_stat_mon_write(reg_val, COP_MON_CTRL);

	stat_mon->avp_sampler.idle_cycles =
			tegra2_stat_mon_read(COP_MON_STATUS);

	return IRQ_WAKE_THREAD;
}


static void add_active_sample(struct sampler *s, unsigned long cycles)
{
	if (s->last_sample == &s->active_cycles[WINDOW_SIZE - 1])
		s->last_sample = &s->active_cycles[0];
	else
		s->last_sample++;

	s->total_active_cycles -= *s->last_sample;
	*s->last_sample = cycles;
	s->total_active_cycles += *s->last_sample;
}

static unsigned long round_rate(struct sampler *s, unsigned long rate)
{
	int i;
	unsigned long *table = s->table;

	if (rate >= table[0])
		return table[0];

	for (i = 1; i < s->table_size; i++) {
		if (rate <= table[i])
			continue;
		else {
			return table[i-1];
			break;
		}
	}
	if (rate <= table[s->table_size - 1])
		return table[s->table_size - 1];
	return rate;
}

static void set_target_freq(struct sampler *s)
{
	unsigned long clock_rate;
	unsigned long target_freq;
	unsigned long active_count;

	clock_rate = clk_get_rate(s->clock) / FREQ_MULT;
	active_count = (s->sample_time + 1) * clock_rate;
	active_count = (active_count > s->idle_cycles) ?
				(active_count - s->idle_cycles) : (0);

	s->sample_count++;

	add_active_sample(s, active_count);

	s->avg_freq = s->total_active_cycles / s->window_ms;

	if ((s->idle_cycles >= (1 + (active_count >> 3))) &&
		(s->bumped_freq >= s->avg_freq)) {
		s->boost_freq = (s->boost_freq *
			((0x1 << BOOST_FRACTION_BITS) - s->boost_dec_coef))
			>> BOOST_FRACTION_BITS;
		if (s->boost_freq < s->boost_step)
			s->boost_freq = 0;
	} else if (s->sample_count < s->min_samples) {
		s->sample_count++;
	} else {
		s->boost_freq = ((s->boost_freq *
			((0x1 << BOOST_FRACTION_BITS) + s->boost_inc_coef))
			>> BOOST_FRACTION_BITS) + s->boost_step;
		if (s->boost_freq > s->clock->max_rate)
			s->boost_freq = s->clock->max_rate;
	}

	if ((s->avg_freq + LOWER_BAND) < s->bumped_freq)
		s->bumped_freq = s->avg_freq + LOWER_BAND;
	else if (s->avg_freq > (s->bumped_freq + UPPER_BAND))
		s->bumped_freq = s->avg_freq - UPPER_BAND;

	s->bumped_freq += (s->bumped_freq >> 3);

	target_freq = max(s->bumped_freq, s->clock->min_rate);
	target_freq += s->boost_freq;

	active_count = target_freq;
	target_freq = round_rate(s, target_freq) * FREQ_MULT;
	clk_set_rate(s->clock, target_freq);
}

/* - process ticks in thread context
 */
static irqreturn_t stat_mon_isr_thread_fn(int irq, void *data)
{
	u32 reg_val = 0;

	mutex_lock(&stat_mon->stat_mon_lock);
	set_target_freq(&stat_mon->avp_sampler);
	mutex_unlock(&stat_mon->stat_mon_lock);

	/* start AVP sampler */
	reg_val = tegra2_stat_mon_read(COP_MON_CTRL);
	reg_val |= MON_ENABLE;
	tegra2_stat_mon_write(reg_val, COP_MON_CTRL);
	return IRQ_HANDLED;
}

void tegra2_statmon_stop(void)
{
	u32 reg_val = 0;

	/* disable AVP monitor */
	reg_val |= INT_STATUS;
	tegra2_stat_mon_write(reg_val, COP_MON_CTRL);

	clk_disable(stat_mon->stat_mon_clock);
	clk_disable(stat_mon->avp_sampler.clock);
}

int tegra2_statmon_start(void)
{
	u32 reg_val = 0;

	clk_enable(stat_mon->avp_sampler.clock);
	clk_enable(stat_mon->stat_mon_clock);

	/* disable AVP monitor */
	reg_val |= INT_STATUS;
	tegra2_stat_mon_write(reg_val, COP_MON_CTRL);

	/* start AVP sampler. also enable INT to CPU */
	reg_val = 0;
	reg_val |= MON_ENABLE;
	reg_val |= INT_ENABLE;
	reg_val |= ((stat_mon->avp_sampler.sample_time \
		<< SAMPLE_PERIOD_SHIFT) & SAMPLE_PERIOD_MASK);
	tegra2_stat_mon_write(reg_val, COP_MON_CTRL);
	return 0;
}

static ssize_t tegra2_statmon_enable_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", stat_mon->avp_sampler.enable);
}

static ssize_t tegra2_statmon_enable_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int value;

	mutex_lock(&stat_mon->stat_mon_lock);
	sscanf(buf, "%d", &value);

	if (value == 0 || value == 1)
		stat_mon->avp_sampler.enable = value;
	else {
		mutex_unlock(&stat_mon->stat_mon_lock);
		return -EINVAL;
	}
	mutex_unlock(&stat_mon->stat_mon_lock);

	if (stat_mon->avp_sampler.enable)
		tegra2_statmon_start();
	else
		tegra2_statmon_stop();

	return 0;
}

static ssize_t tegra2_statmon_sample_time_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", stat_mon->avp_sampler.sample_time);
}

static ssize_t tegra2_statmon_sample_time_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int value;

	mutex_lock(&stat_mon->stat_mon_lock);
	sscanf(buf, "%d", &value);
	stat_mon->avp_sampler.sample_time = value;
	mutex_unlock(&stat_mon->stat_mon_lock);

	return count;
}

static struct class tegra2_statmon_class = {
	.name = "tegra2_statmon",
};

#define TEGRA2_STATMON_ATTRIBUTE_EXPAND(_attr, _mode) \
	static CLASS_ATTR(_attr, _mode, \
		tegra2_statmon_##_attr##_show, tegra2_statmon_##_attr##_store)

TEGRA2_STATMON_ATTRIBUTE_EXPAND(enable, 0666);
TEGRA2_STATMON_ATTRIBUTE_EXPAND(sample_time, 0666);

#define TEGRA2_STATMON_ATTRIBUTE(_name) (&class_attr_##_name)

static struct class_attribute *tegra2_statmon_attrs[] = {
	TEGRA2_STATMON_ATTRIBUTE(enable),
	TEGRA2_STATMON_ATTRIBUTE(sample_time),
	NULL,
};

static int sampler_init(struct sampler *s)
{
	int i;
	struct clk *clock;
	unsigned long clock_rate;
	unsigned long active_count;

	s->enable = false;
	s->sample_time = 9;

	clock = tegra_get_clock_by_name("mon.sclk");
	if (IS_ERR(clock)) {
		pr_err("%s: Couldn't get mon.sckl\n", __func__);
		return -1;
	}

	if (clk_set_rate(clock, clock->min_rate)) {
		pr_err("%s: Failed to set rate\n", __func__);
		return -1;
	}
	clock_rate = clk_get_rate(clock) / FREQ_MULT;
	active_count = clock_rate * (s->sample_time + 1);

	for (i = 0; i < WINDOW_SIZE; i++)
		s->active_cycles[i] = active_count;

	s->clock = clock;
	s->last_sample = &s->active_cycles[0];
	s->total_active_cycles = active_count << 7;
	s->window_ms = (s->sample_time + 1) << 7;
	s->avg_freq = s->total_active_cycles / s->window_ms;
	s->bumped_freq = s->avg_freq;
	s->boost_freq = 0;

	return 0;
}

static int tegra2_stat_mon_init(void)
{
	int rc, i;
	int ret_val = 0;

	stat_mon = kzalloc(sizeof(struct tegra2_stat_mon), GFP_KERNEL);
	if (stat_mon == NULL) {
		pr_err("%s: unable to alloc data struct.\n", __func__);
		return -ENOMEM;
	}

	stat_mon->stat_mon_base = IO_ADDRESS(TEGRA_STATMON_BASE);
	stat_mon->vde_mon_base = IO_ADDRESS(TEGRA_VDE_BASE);

	stat_mon->stat_mon_clock = tegra_get_clock_by_name("stat_mon");
	if (stat_mon->stat_mon_clock == NULL) {
		pr_err("Failed to get stat mon clock");
		return -1;
	}

	if (sampler_init(&stat_mon->avp_sampler))
		return -1;

	stat_mon->avp_sampler.table = sclk_table;
	stat_mon->avp_sampler.table_size = ARRAY_SIZE(sclk_table);
	stat_mon->avp_sampler.boost_step = 1000;
	stat_mon->avp_sampler.boost_inc_coef = 255;
	stat_mon->avp_sampler.boost_dec_coef = 128;
	stat_mon->avp_sampler.min_samples = 3;

	mutex_init(&stat_mon->stat_mon_lock);

	/* /sys/devices/system/tegra2_statmon */
	rc = class_register(&tegra2_statmon_class);
	if (rc) {
		pr_err("%s : Couldn't create statmon sysfs entry\n", __func__);
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(tegra2_statmon_attrs) - 1; i++) {
		rc = class_create_file(&tegra2_statmon_class,
			tegra2_statmon_attrs[i]);
		if (rc) {
			pr_err("%s: Failed to create sys class\n", __func__);
			class_unregister(&tegra2_statmon_class);
			kfree(stat_mon);
			return 0;
		}
	}

	ret_val = request_threaded_irq(INT_SYS_STATS_MON, stat_mon_isr,
		stat_mon_isr_thread_fn, 0, "stat_mon_int", NULL);
	if (ret_val) {
		pr_err("%s: cannot register INT_SYS_STATS_MON handler, \
				ret_val = 0x%x\n", __func__, ret_val);
		tegra2_statmon_stop();
		stat_mon->avp_sampler.enable = false;
		kfree(stat_mon);
		return ret_val;
	}

	return 0;
}

late_initcall(tegra2_stat_mon_init);
