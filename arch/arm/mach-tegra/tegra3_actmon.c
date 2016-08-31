/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <mach/irqs.h>

#include "clock.h"
#include "iomap.h"

#define ACTMON_GLB_STATUS			0x00
#define ACTMON_GLB_PERIOD_CTRL			0x04

#define ACTMON_DEV_CTRL				0x00
#define ACTMON_DEV_CTRL_ENB			(0x1 << 31)
#define ACTMON_DEV_CTRL_UP_WMARK_ENB		(0x1 << 30)
#define ACTMON_DEV_CTRL_DOWN_WMARK_ENB		(0x1 << 29)
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT	26
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK	(0x7 <<	26)
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT	23
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK	(0x7 <<	23)
#define ACTMON_DEV_CTRL_AVG_UP_WMARK_ENB	(0x1 << 21)
#define ACTMON_DEV_CTRL_AVG_DOWN_WMARK_ENB	(0x1 << 20)
#define ACTMON_DEV_CTRL_PERIODIC_ENB		(0x1 << 18)
#define ACTMON_DEV_CTRL_K_VAL_SHIFT		10
#define ACTMON_DEV_CTRL_K_VAL_MASK		(0x7 << 10)

#define ACTMON_DEV_UP_WMARK			0x04
#define ACTMON_DEV_DOWN_WMARK			0x08
#define ACTMON_DEV_INIT_AVG			0x0c
#define ACTMON_DEV_AVG_UP_WMARK			0x10
#define ACTMON_DEV_AVG_DOWN_WMARK			0x14

#define ACTMON_DEV_COUNT_WEGHT			0x18
#define ACTMON_DEV_COUNT			0x1c
#define ACTMON_DEV_AVG_COUNT			0x20

#define ACTMON_DEV_INTR_STATUS			0x24
#define ACTMON_DEV_INTR_UP_WMARK		(0x1 << 31)
#define ACTMON_DEV_INTR_DOWN_WMARK		(0x1 << 30)
#define ACTMON_DEV_INTR_AVG_DOWN_WMARK		(0x1 << 25)
#define ACTMON_DEV_INTR_AVG_UP_WMARK		(0x1 << 24)

#define ACTMON_DEFAULT_AVG_WINDOW_LOG2		6
#define ACTMON_DEFAULT_AVG_BAND			6	/* 1/10 of % */

enum actmon_type {
	ACTMON_LOAD_SAMPLER,
	ACTMON_FREQ_SAMPLER,
};

enum actmon_state {
	ACTMON_UNINITIALIZED = -1,
	ACTMON_OFF = 0,
	ACTMON_ON  = 1,
	ACTMON_SUSPENDED = 2,
};

#define ACTMON_DEFAULT_SAMPLING_PERIOD		12
static u8 actmon_sampling_period;

static unsigned long actmon_clk_freq;

/* Maximum frequency EMC is running at when sourced from PLLP. This is
 * really a short-cut, but it is true for all Tegra3  platforms
 */
#define EMC_PLLP_FREQ_MAX			204000

/* Units:
 * - frequency in kHz
 * - coefficients, and thresholds in %
 * - sampling period in ms
 * - window in sample periods (value = setting + 1)
 */
struct actmon_dev {
	u32		reg;
	u32		glb_status_irq_mask;
	const char	*dev_id;
	const char	*con_id;
	struct clk	*clk;

	unsigned long	max_freq;
	unsigned long	target_freq;
	unsigned long	cur_freq;
	unsigned long	suspend_freq;

	unsigned long	avg_actv_freq;
	unsigned long	avg_band_freq;
	unsigned int	avg_sustain_coef;
	u32		avg_count;
	u32		avg_dependency_threshold;

	unsigned long	boost_freq;
	unsigned long	boost_freq_step;
	unsigned int	boost_up_coef;
	unsigned int	boost_down_coef;
	unsigned int	boost_up_threshold;
	unsigned int	boost_down_threshold;

	u8		up_wmark_window;
	u8		down_wmark_window;
	u8		avg_window_log2;
	u32		count_weight;

	enum actmon_type	type;
	enum actmon_state	state;
	enum actmon_state	saved_state;

	spinlock_t	lock;

	struct notifier_block	rate_change_nb;
};

static void __iomem *actmon_base = IO_ADDRESS(TEGRA_ACTMON_BASE);

static inline u32 actmon_readl(u32 offset)
{
	return __raw_readl(actmon_base + offset);
}
static inline void actmon_writel(u32 val, u32 offset)
{
	__raw_writel(val, actmon_base + offset);
}
static inline void actmon_wmb(void)
{
	wmb();
	actmon_readl(ACTMON_GLB_STATUS);
}

#define offs(x)		(dev->reg + x)

static inline unsigned long do_percent(unsigned long val, unsigned int pct)
{
	return val * pct / 100;
}

static inline void actmon_dev_up_wmark_set(struct actmon_dev *dev)
{
	u32 val;
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
		dev->cur_freq : actmon_clk_freq;

	val = freq * actmon_sampling_period;
	actmon_writel(do_percent(val, dev->boost_up_threshold),
		      offs(ACTMON_DEV_UP_WMARK));
}

static inline void actmon_dev_down_wmark_set(struct actmon_dev *dev)
{
	u32 val;
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
		dev->cur_freq : actmon_clk_freq;

	val = freq * actmon_sampling_period;
	actmon_writel(do_percent(val, dev->boost_down_threshold),
		      offs(ACTMON_DEV_DOWN_WMARK));
}

static inline void actmon_dev_wmark_set(struct actmon_dev *dev)
{
	u32 val;
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
		dev->cur_freq : actmon_clk_freq;

	val = freq * actmon_sampling_period;
	actmon_writel(do_percent(val, dev->boost_up_threshold),
		      offs(ACTMON_DEV_UP_WMARK));
	actmon_writel(do_percent(val, dev->boost_down_threshold),
		      offs(ACTMON_DEV_DOWN_WMARK));
}

static inline void actmon_dev_avg_wmark_set(struct actmon_dev *dev)
{
	u32 avg = dev->avg_count;
	u32 band = dev->avg_band_freq * actmon_sampling_period;

	actmon_writel(avg + band, offs(ACTMON_DEV_AVG_UP_WMARK));
	avg = max(avg, band);
	actmon_writel(avg - band, offs(ACTMON_DEV_AVG_DOWN_WMARK));
}

static unsigned long actmon_dev_avg_freq_get(struct actmon_dev *dev)
{
	u64 val;

	if (dev->type == ACTMON_FREQ_SAMPLER)
		return dev->avg_count / actmon_sampling_period;

	val = (u64)dev->avg_count * dev->cur_freq;
	do_div(val, actmon_clk_freq * actmon_sampling_period);
	return (u32)val;
}

/* Activity monitor sampling operations */
irqreturn_t actmon_dev_isr(int irq, void *dev_id)
{
	u32 val;
	unsigned long flags;
	struct actmon_dev *dev = (struct actmon_dev *)dev_id;

	val = actmon_readl(ACTMON_GLB_STATUS) & dev->glb_status_irq_mask;
	if (!val)
		return IRQ_NONE;

	spin_lock_irqsave(&dev->lock, flags);

	dev->avg_count = actmon_readl(offs(ACTMON_DEV_AVG_COUNT));
	actmon_dev_avg_wmark_set(dev);

	val = actmon_readl(offs(ACTMON_DEV_INTR_STATUS));
	if (val & ACTMON_DEV_INTR_UP_WMARK) {
		val = actmon_readl(offs(ACTMON_DEV_CTRL)) |
			ACTMON_DEV_CTRL_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_DOWN_WMARK_ENB;

		dev->boost_freq = dev->boost_freq_step +
			do_percent(dev->boost_freq, dev->boost_up_coef);
		if (dev->boost_freq >= dev->max_freq) {
			dev->boost_freq = dev->max_freq;
			val &= ~ACTMON_DEV_CTRL_UP_WMARK_ENB;
		}
		actmon_writel(val, offs(ACTMON_DEV_CTRL));
	} else if (val & ACTMON_DEV_INTR_DOWN_WMARK) {
		val = actmon_readl(offs(ACTMON_DEV_CTRL)) |
			ACTMON_DEV_CTRL_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_DOWN_WMARK_ENB;

		dev->boost_freq =
			do_percent(dev->boost_freq, dev->boost_down_coef);
		if (dev->boost_freq < (dev->boost_freq_step >> 1)) {
			dev->boost_freq = 0;
			val &= ~ACTMON_DEV_CTRL_DOWN_WMARK_ENB;
		}
		actmon_writel(val, offs(ACTMON_DEV_CTRL));
	}
	if (dev->avg_dependency_threshold) {
		val = actmon_readl(offs(ACTMON_DEV_CTRL));
		if (dev->avg_count >= dev->avg_dependency_threshold)
			val |= ACTMON_DEV_CTRL_DOWN_WMARK_ENB;
		else if (dev->boost_freq == 0)
			val &= ~ACTMON_DEV_CTRL_DOWN_WMARK_ENB;
		actmon_writel(val, offs(ACTMON_DEV_CTRL));
	}

	actmon_writel(0xffffffff, offs(ACTMON_DEV_INTR_STATUS)); /* clr all */
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return IRQ_WAKE_THREAD;
}

irqreturn_t actmon_dev_fn(int irq, void *dev_id)
{
	unsigned long flags, freq;
	struct actmon_dev *dev = (struct actmon_dev *)dev_id;
	unsigned long cpu_freq = 0;
	unsigned long static_cpu_emc_freq = 0;

	if (dev->avg_dependency_threshold) {
		cpu_freq = clk_get_rate(tegra_get_clock_by_name("cpu")) / 1000;
		static_cpu_emc_freq = tegra_emc_to_cpu_ratio(cpu_freq) / 1000;
		pr_debug("dev->avg_count%u, cpu_freq: %lu, static_cpu_emc_freq:%lu\n",
			dev->avg_count, cpu_freq, static_cpu_emc_freq);
	}

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state != ACTMON_ON) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return IRQ_HANDLED;
	}

	freq = actmon_dev_avg_freq_get(dev);
	dev->avg_actv_freq = freq;
	freq = do_percent(freq, dev->avg_sustain_coef);
	freq += dev->boost_freq;

	if (dev->avg_dependency_threshold &&
		((dev->avg_count >= dev->avg_dependency_threshold)
			|| (!static_cpu_emc_freq)))
		freq = static_cpu_emc_freq;

	dev->target_freq = freq;

	spin_unlock_irqrestore(&dev->lock, flags);

	pr_debug("%s.%s(kHz): avg: %lu,  boost: %lu, target: %lu, current: %lu\n",
	dev->dev_id, dev->con_id, dev->avg_actv_freq, dev->boost_freq,
	dev->target_freq, dev->cur_freq);

	clk_set_rate(dev->clk, freq * 1000);

	return IRQ_HANDLED;
}

static int actmon_rate_notify_cb(
	struct notifier_block *nb, unsigned long rate, void *v)
{
	unsigned long flags;
	struct actmon_dev *dev = container_of(
		nb, struct actmon_dev, rate_change_nb);

	spin_lock_irqsave(&dev->lock, flags);

	dev->cur_freq = rate / 1000;
	if (dev->type == ACTMON_FREQ_SAMPLER) {
		actmon_dev_wmark_set(dev);
		actmon_wmb();
	}

	spin_unlock_irqrestore(&dev->lock, flags);
	return NOTIFY_OK;
};

/* Activity monitor configuration and control */
static void actmon_dev_configure(struct actmon_dev *dev, unsigned long freq)
{
	u32 val;

	dev->cur_freq = freq;
	dev->target_freq = freq;
	dev->avg_actv_freq = freq;

	if (dev->type == ACTMON_FREQ_SAMPLER) {
		dev->avg_count = dev->cur_freq * actmon_sampling_period;
		dev->avg_band_freq = dev->max_freq *
			ACTMON_DEFAULT_AVG_BAND / 1000;
	} else {
		dev->avg_count = actmon_clk_freq * actmon_sampling_period;
		dev->avg_band_freq = actmon_clk_freq *
			ACTMON_DEFAULT_AVG_BAND / 1000;
	}
	actmon_writel(dev->avg_count, offs(ACTMON_DEV_INIT_AVG));

	BUG_ON(!dev->boost_up_threshold);
	dev->avg_sustain_coef = 100 * 100 / dev->boost_up_threshold;
	actmon_dev_avg_wmark_set(dev);
	actmon_dev_wmark_set(dev);

	actmon_writel(dev->count_weight, offs(ACTMON_DEV_COUNT_WEGHT));
	actmon_writel(0xffffffff, offs(ACTMON_DEV_INTR_STATUS)); /* clr all */

	val = ACTMON_DEV_CTRL_PERIODIC_ENB | ACTMON_DEV_CTRL_AVG_UP_WMARK_ENB |
		ACTMON_DEV_CTRL_AVG_DOWN_WMARK_ENB;
	val |= ((dev->avg_window_log2 - 1) << ACTMON_DEV_CTRL_K_VAL_SHIFT) &
		ACTMON_DEV_CTRL_K_VAL_MASK;
	val |= ((dev->down_wmark_window - 1) <<
		ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT) &
		ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK;
	val |=  ((dev->up_wmark_window - 1) <<
		ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT) &
		ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK;
	val |= ACTMON_DEV_CTRL_DOWN_WMARK_ENB | ACTMON_DEV_CTRL_UP_WMARK_ENB;
	actmon_writel(val, offs(ACTMON_DEV_CTRL));
	actmon_wmb();
}

static void actmon_dev_enable(struct actmon_dev *dev)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state == ACTMON_OFF) {
		dev->state = ACTMON_ON;

		val = actmon_readl(offs(ACTMON_DEV_CTRL));
		val |= ACTMON_DEV_CTRL_ENB;
		actmon_writel(val, offs(ACTMON_DEV_CTRL));
		actmon_wmb();
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void actmon_dev_disable(struct actmon_dev *dev)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state == ACTMON_ON) {
		dev->state = ACTMON_OFF;

		val = actmon_readl(offs(ACTMON_DEV_CTRL));
		val &= ~ACTMON_DEV_CTRL_ENB;
		actmon_writel(val, offs(ACTMON_DEV_CTRL));
		actmon_writel(0xffffffff, offs(ACTMON_DEV_INTR_STATUS));
		actmon_wmb();
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void actmon_dev_save(struct actmon_dev *dev)
{
	u32 val;

	if ((dev->state == ACTMON_ON) || (dev->state == ACTMON_OFF)){
		dev->saved_state = dev->state;
		dev->state = ACTMON_SUSPENDED;

		val = actmon_readl(offs(ACTMON_DEV_CTRL));
		val &= ~ACTMON_DEV_CTRL_ENB;
		actmon_writel(val, offs(ACTMON_DEV_CTRL));
		actmon_writel(0xffffffff, offs(ACTMON_DEV_INTR_STATUS));
		actmon_wmb();
	}
}

static void actmon_dev_suspend(struct actmon_dev *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	actmon_dev_save(dev);

	spin_unlock_irqrestore(&dev->lock, flags);

	if (dev->suspend_freq)
		clk_set_rate(dev->clk, dev->suspend_freq * 1000);
}

static void actmon_dev_restore(struct actmon_dev *dev, ulong freq)
{
	u32 val;

	if (dev->state == ACTMON_SUSPENDED) {
		actmon_dev_configure(dev, freq);
		dev->state = dev->saved_state;
		if (dev->state == ACTMON_ON) {
			val = actmon_readl(offs(ACTMON_DEV_CTRL));
			val |= ACTMON_DEV_CTRL_ENB;
			actmon_writel(val, offs(ACTMON_DEV_CTRL));
			actmon_wmb();
		}
	}
}

static void actmon_dev_resume(struct actmon_dev *dev)
{
	unsigned long flags;
	unsigned long freq = clk_get_rate(dev->clk) / 1000;

	spin_lock_irqsave(&dev->lock, flags);

	actmon_dev_restore(dev, freq);

	spin_unlock_irqrestore(&dev->lock, flags);
}

static int __init actmon_dev_init(struct actmon_dev *dev)
{
	int ret;
	struct clk *p;
	unsigned long freq;

	spin_lock_init(&dev->lock);

	dev->clk = clk_get_sys(dev->dev_id, dev->con_id);
	if (IS_ERR(dev->clk)) {
		pr_err("Failed to find %s.%s clock\n",
		       dev->dev_id, dev->con_id);
		return -ENODEV;
	}
	dev->max_freq = clk_round_rate(dev->clk, ULONG_MAX);
	clk_set_rate(dev->clk, dev->max_freq);
	dev->max_freq /= 1000;
	freq = clk_get_rate(dev->clk) / 1000;
	actmon_dev_configure(dev, freq);

	/* actmon device controls shared bus user clock, but rate
	   change notification should come from bus clock itself */
	p = clk_get_parent(dev->clk);
	BUG_ON(!p);

	if (dev->rate_change_nb.notifier_call) {
		ret = tegra_register_clk_rate_notifier(p, &dev->rate_change_nb);
		if (ret) {
			pr_err("Failed to register %s rate change notifier"
			       " for %s\n", p->name, dev->dev_id);
			return ret;
		}
	}

	ret = request_threaded_irq(INT_ACTMON, actmon_dev_isr, actmon_dev_fn,
				   IRQF_SHARED, dev->dev_id, dev);
	if (ret) {
		pr_err("Failed irq %d request for %s.%s\n",
		       INT_ACTMON, dev->dev_id, dev->con_id);
		tegra_unregister_clk_rate_notifier(p, &dev->rate_change_nb);
		return ret;
	}

	dev->state = ACTMON_OFF;
	actmon_dev_enable(dev);
	tegra_clk_prepare_enable(dev->clk);
	return 0;
}

/* EMC activity monitor: frequency sampling device:
 * activity counter is incremented every 256 memory transactions, and
 * each transaction takes 2 EMC clocks; count_weight = 512 on Tegra3.
 * On Tegra11 there is only 1 clock per transaction, hence weight = 256.
 */
static struct actmon_dev actmon_dev_emc = {
	.reg	= 0x1c0,
	.glb_status_irq_mask = (0x1 << 26),
	.dev_id = "tegra_actmon",
	.con_id = "emc",

	/* EMC suspend floor to guarantee suspend entry on PLLM */
	.suspend_freq		= EMC_PLLP_FREQ_MAX + 2000,

	.boost_freq_step	= 16000,
	.boost_up_coef		= 200,
	.boost_down_coef	= 50,
#if defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	.boost_up_threshold	= 70,
	.boost_down_threshold	= 50,
#else
	.boost_up_threshold	= 60,
	.boost_down_threshold	= 40,
#endif

	.up_wmark_window	= 1,
	.down_wmark_window	= 3,
	.avg_window_log2	= ACTMON_DEFAULT_AVG_WINDOW_LOG2,
#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	.count_weight		= 0x200,
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
	.count_weight		= 0x400,
#else
	.count_weight		= 0x100,
#endif

	.type			= ACTMON_FREQ_SAMPLER,
	.state			= ACTMON_UNINITIALIZED,

	.rate_change_nb = {
		.notifier_call = actmon_rate_notify_cb,
	},
};

/* AVP activity monitor: load sampling device:
 * activity counter is incremented on every actmon clock pulse while
 * AVP is not halted by flow controller; count_weight = 1.
 */
static struct actmon_dev actmon_dev_avp = {
	.reg	= 0x0c0,
	.glb_status_irq_mask = (0x1 << 30),
	.dev_id = "tegra_actmon",
	.con_id = "avp",

	/* AVP/SCLK suspend activity floor */
	.suspend_freq		= 40000,

	.boost_freq_step	= 8000,
	.boost_up_coef		= 200,
	.boost_down_coef	= 50,
	.boost_up_threshold	= 85,
	.boost_down_threshold	= 50,

	.up_wmark_window	= 1,
	.down_wmark_window	= 3,
	.avg_window_log2	= ACTMON_DEFAULT_AVG_WINDOW_LOG2,
	.count_weight		= 0x1,

	.type			= ACTMON_LOAD_SAMPLER,
	.state			= ACTMON_UNINITIALIZED,

	.rate_change_nb = {
		.notifier_call = actmon_rate_notify_cb,
	},
};


#define CPU_AVG_ACT_THRESHOLD 50000
/* EMC-cpu activity monitor: frequency sampling device:
 * activity counter is incremented every 256 memory transactions, and
 * each transaction takes 2 EMC clocks; count_weight = 512 on Tegra3.
 * On Tegra11 there is only 1 clock per transaction, hence weight = 256.
 */
static struct actmon_dev actmon_dev_cpu_emc = {
	.reg = 0x200,
	.glb_status_irq_mask = (0x1 << 25),
	.dev_id = "tegra_mon",
	.con_id = "cpu_emc",

	.boost_freq_step	= 16000,
	.boost_up_coef		= 800,
	.boost_down_coef	= 90,
	.boost_up_threshold	= 27,
	.boost_down_threshold	= 10,
	.avg_dependency_threshold	= CPU_AVG_ACT_THRESHOLD,

	.up_wmark_window	= 1,
	.down_wmark_window	= 3,
	.avg_window_log2	= ACTMON_DEFAULT_AVG_WINDOW_LOG2,
#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	.count_weight		= 0x200,
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
	.count_weight		= 0x400,
#else
	.count_weight		= 0x100,
#endif

	.type			= ACTMON_FREQ_SAMPLER,
	.state			= ACTMON_UNINITIALIZED,

	.rate_change_nb = {
		.notifier_call = actmon_rate_notify_cb,
	},
};

static struct actmon_dev *actmon_devices[] = {
	&actmon_dev_emc,
	&actmon_dev_avp,
	&actmon_dev_cpu_emc,
};

int tegra_actmon_save(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(actmon_devices); i++)
		actmon_dev_save(actmon_devices[i]);

	return 0;
}
/* Needs to be called from IRQ enabled as clk_get_rate can
 * sleep for emc.
 */
int tegra_actmon_restore(void)
{
	int i;
	unsigned long flags;

	actmon_writel(actmon_sampling_period - 1,
		      ACTMON_GLB_PERIOD_CTRL);

	for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
		/*
		 * Using clk_get_rate_all_locked() here, because all other cpus
		 * except cpu0 are in LP2 state and irqs are disabled.
		 */
		struct actmon_dev *dev = actmon_devices[i];
		unsigned long freq = clk_get_rate_all_locked(dev->clk) / 1000;

		spin_lock_irqsave(&dev->lock, flags);
		actmon_dev_restore(dev, freq);
		spin_unlock_irqrestore(&dev->lock, flags);
	}

	return 0;
}

/* Activity monitor suspend/resume */
static int actmon_pm_notify(struct notifier_block *nb,
			    unsigned long event, void *data)
{
	int i;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		for (i = 0; i < ARRAY_SIZE(actmon_devices); i++)
			actmon_dev_suspend(actmon_devices[i]);
		break;
	case PM_POST_SUSPEND:
		actmon_writel(actmon_sampling_period - 1,
			      ACTMON_GLB_PERIOD_CTRL);
		for (i = 0; i < ARRAY_SIZE(actmon_devices); i++)
			actmon_dev_resume(actmon_devices[i]);
		break;
	}

	return NOTIFY_OK;
};

static struct notifier_block actmon_pm_nb = {
	.notifier_call = actmon_pm_notify,
};

#ifdef CONFIG_DEBUG_FS

#define RW_MODE (S_IWUSR | S_IRUGO)
#define RO_MODE	S_IRUGO

static struct dentry *clk_debugfs_root;

static int type_show(struct seq_file *s, void *data)
{
	struct actmon_dev *dev = s->private;

	seq_printf(s, "%s\n", (dev->type == ACTMON_LOAD_SAMPLER) ?
		   "Load Activity Monitor" : "Frequency Activity Monitor");
	return 0;
}
static int type_open(struct inode *inode, struct file *file)
{
	return single_open(file, type_show, inode->i_private);
}
static const struct file_operations type_fops = {
	.open		= type_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actv_get(void *data, u64 *val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;

	spin_lock_irqsave(&dev->lock, flags);
	*val = actmon_dev_avg_freq_get(dev);
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(actv_fops, actv_get, NULL, "%llu\n");

static int step_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->boost_freq_step * 100 / dev->max_freq;
	return 0;
}
static int step_set(void *data, u64 val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;

	if (val > 100)
		val = 100;

	spin_lock_irqsave(&dev->lock, flags);
	dev->boost_freq_step = do_percent(dev->max_freq, (unsigned int)val);
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(step_fops, step_get, step_set, "%llu\n");

static int up_threshold_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->boost_up_threshold;
	return 0;
}
static int up_threshold_set(void *data, u64 val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;
	unsigned int up_threshold = (unsigned int)val;

	if (up_threshold > 100)
		up_threshold = 100;

	spin_lock_irqsave(&dev->lock, flags);

	if (up_threshold <= dev->boost_down_threshold)
		up_threshold = dev->boost_down_threshold;
	if (up_threshold)
		dev->avg_sustain_coef = 100 * 100 / up_threshold;
	dev->boost_up_threshold = up_threshold;

	actmon_dev_up_wmark_set(dev);
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(up_threshold_fops, up_threshold_get,
			up_threshold_set, "%llu\n");

static int down_threshold_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->boost_down_threshold;
	return 0;
}
static int down_threshold_set(void *data, u64 val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;
	unsigned int down_threshold = (unsigned int)val;

	spin_lock_irqsave(&dev->lock, flags);

	if (down_threshold >= dev->boost_up_threshold)
		down_threshold = dev->boost_up_threshold;
	dev->boost_down_threshold = down_threshold;

	actmon_dev_down_wmark_set(dev);
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(down_threshold_fops, down_threshold_get,
			down_threshold_set, "%llu\n");

static int state_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->state;
	return 0;
}
static int state_set(void *data, u64 val)
{
	struct actmon_dev *dev = data;

	if (val)
		actmon_dev_enable(dev);
	else
		actmon_dev_disable(dev);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(state_fops, state_get, state_set, "%llu\n");

static int period_get(void *data, u64 *val)
{
	*val = actmon_sampling_period;
	return 0;
}
static int period_set(void *data, u64 val)
{
	int i;
	unsigned long flags;
	u8 period = (u8)val;

	if (period) {
		actmon_sampling_period = period;
		actmon_writel(period - 1, ACTMON_GLB_PERIOD_CTRL);

		for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
			struct actmon_dev *dev = actmon_devices[i];
			spin_lock_irqsave(&dev->lock, flags);
			actmon_dev_wmark_set(dev);
			spin_unlock_irqrestore(&dev->lock, flags);
		}
		actmon_wmb();
		return 0;
	}
	return -EINVAL;
}
DEFINE_SIMPLE_ATTRIBUTE(period_fops, period_get, period_set, "%llu\n");


static int actmon_debugfs_create_dev(struct actmon_dev *dev)
{
	struct dentry *dir, *d;

	if (dev->state == ACTMON_UNINITIALIZED)
		return 0;

	dir = debugfs_create_dir(dev->con_id, clk_debugfs_root);
	if (!dir)
		return -ENOMEM;

	d = debugfs_create_file(
		"actv_type", RO_MODE, dir, dev, &type_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"avg_activity", RO_MODE, dir, dev, &actv_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"boost_step", RW_MODE, dir, dev, &step_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_u32(
		"boost_rate_dec", RW_MODE, dir, (u32 *)&dev->boost_down_coef);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_u32(
		"boost_rate_inc", RW_MODE, dir, (u32 *)&dev->boost_up_coef);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"boost_threshold_dn", RW_MODE, dir, dev, &down_threshold_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"boost_threshold_up", RW_MODE, dir, dev, &up_threshold_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"state", RW_MODE, dir, dev, &state_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_u32(
		"avg_act_threshold", RW_MODE, dir,
		(u32 *)&dev->avg_dependency_threshold);

	if (!d)
		return -ENOMEM;

	return 0;
}

static int __init actmon_debugfs_init(void)
{
	int i;
	int ret = -ENOMEM;
	struct dentry *d;

	d = debugfs_create_dir("tegra_actmon", NULL);
	if (!d)
		return ret;
	clk_debugfs_root = d;

	d = debugfs_create_file("period", RW_MODE, d, NULL, &period_fops);
	if (!d)
		goto err_out;

	for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
		ret = actmon_debugfs_create_dev(actmon_devices[i]);
		if (ret)
			goto err_out;
	}
	return 0;

err_out:
	debugfs_remove_recursive(clk_debugfs_root);
	return ret;
}

#endif

static int __init tegra_actmon_init(void)
{
	int i, ret;
	struct clk *c = tegra_get_clock_by_name("actmon");

	if (!c) {
		pr_err("%s: Failed to find actmon clock\n", __func__);
		return 0;
	}
	actmon_clk_freq = clk_get_rate(c) / 1000;
	ret = tegra_clk_prepare_enable(c);
	if (ret) {
		pr_err("%s: Failed to enable actmon clock\n", __func__);
		return 0;
	}
	actmon_sampling_period = ACTMON_DEFAULT_SAMPLING_PERIOD;
	actmon_writel(actmon_sampling_period - 1, ACTMON_GLB_PERIOD_CTRL);

	for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
		ret = actmon_dev_init(actmon_devices[i]);
		pr_info("%s.%s: %s initialization (%d)\n",
			actmon_devices[i]->dev_id, actmon_devices[i]->con_id,
			ret ? "Failed" : "Completed", ret);
	}
	register_pm_notifier(&actmon_pm_nb);

#ifdef CONFIG_DEBUG_FS
	actmon_debugfs_init();
#endif
	return 0;
}
late_initcall(tegra_actmon_init);
