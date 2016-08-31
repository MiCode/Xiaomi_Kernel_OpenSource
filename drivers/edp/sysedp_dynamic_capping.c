/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/debugfs.h>
#include <linux/edp.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/workqueue.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/debugfs.h>
#include <trace/events/sysedp.h>

#include "sysedp_internal.h"

struct freqcap {
	unsigned int cpu;
	unsigned int gpu;
	unsigned int emc;
};

static unsigned int gpu_high_threshold = 500;
static unsigned int gpu_window = 80;
static unsigned int gpu_high_hist;
static unsigned int gpu_high_count = 2;
static unsigned int online_cpu_count;
static bool gpu_busy;
static unsigned int avail_power;
static unsigned int avail_oc_relax;
static unsigned int cap_method;

static struct tegra_sysedp_corecap *cur_corecap;
static struct clk *emc_cap_clk;
static struct clk *gpu_cap_clk;
static struct pm_qos_request cpufreq_qos;
static unsigned int cpu_power_balance;
static unsigned int force_gpu_pri;
static struct delayed_work capping_work;
static struct tegra_sysedp_platform_data *capping_device_platdata;
static struct freqcap core_policy;
static struct freqcap forced_caps;
static struct freqcap cur_caps;
static DEFINE_MUTEX(core_lock);

static int init_done;

/* To save some cycles from a linear search */
static unsigned int cpu_lut_match(unsigned int power,
		struct tegra_system_edp_entry *lut, unsigned int lutlen)
{
	unsigned int fv;
	unsigned int lv;
	unsigned int step;
	unsigned int i;

	if (lutlen == 1)
		return 0;

	fv = lut[0].power_limit_100mW * 100;
	lv = lut[lutlen - 1].power_limit_100mW * 100;
	step = (lv - fv) / (lutlen - 1);

	i = (power - fv + step - 1) / step;
	i = min_t(unsigned int, i, lutlen - 1);
	if (lut[i].power_limit_100mW * 100 >= power)
		return i;

	/* Didn't work, search back from the end */
	return lutlen - 1;
}

static unsigned int get_cpufreq_lim(unsigned int power)
{
	struct tegra_system_edp_entry *p;
	int i;

	i = cpu_lut_match(power, capping_device_platdata->cpufreq_lim,
			capping_device_platdata->cpufreq_lim_size);
	p = capping_device_platdata->cpufreq_lim + i;

	for (; i > 0; i--, p--) {
		if (p->power_limit_100mW * 100 <= power)
			break;
	}

	WARN_ON(p->power_limit_100mW > power);
	return p->freq_limits[online_cpu_count - 1];
}

static void pr_caps(struct freqcap *old, struct freqcap *new,
		unsigned int cpu_power)
{
	if (!IS_ENABLED(CONFIG_DEBUG_KERNEL))
		return;

	if (new->cpu == old->cpu &&
			new->gpu == old->gpu &&
			new->emc == old->emc)
		return;

	pr_debug("sysedp: ncpus %u, gpupri %d, core %5u mW, "
			"cpu %5u mW %u kHz, gpu %u kHz, emc %u kHz\n",
			online_cpu_count, gpu_busy, cur_corecap->power,
			cpu_power, new->cpu, new->gpu, new->emc);
}

static void apply_caps(struct tegra_sysedp_devcap *devcap)
{
	struct freqcap new;
	int r;
	int do_trace = 0;

	core_policy.cpu = get_cpufreq_lim(devcap->cpu_power +
			cpu_power_balance);
	core_policy.gpu = devcap->gpufreq;
	core_policy.emc = devcap->emcfreq;

	new.cpu = forced_caps.cpu ?: core_policy.cpu;
	new.gpu = forced_caps.gpu ?: core_policy.gpu;
	new.emc = forced_caps.emc ?: core_policy.emc;

	if (new.cpu != cur_caps.cpu) {
		pm_qos_update_request(&cpufreq_qos, new.cpu);
		do_trace = 1;
	}

	if (new.emc != cur_caps.emc) {
		r = clk_set_rate(emc_cap_clk, new.emc * 1000);
		WARN_ON(r);
		do_trace = 1;
	}

	if (new.gpu != cur_caps.gpu) {
		r = clk_set_rate(gpu_cap_clk, new.gpu * 1000);
		WARN_ON(r && (r != -ENOENT));
		do_trace = 1;
	}

	if (do_trace)
		trace_sysedp_dynamic_capping(new.cpu, new.gpu,
					     new.emc, gpu_busy);
	pr_caps(&cur_caps, &new, devcap->cpu_power);
	cur_caps = new;
}

static inline bool gpu_priority(void)
{
	return gpu_busy || force_gpu_pri;
}

static inline struct tegra_sysedp_devcap *get_devcap(void)
{
	return gpu_priority() ? &cur_corecap->gpupri : &cur_corecap->cpupri;
}

static void __do_cap_control(void)
{
	struct tegra_sysedp_devcap *cap;

	if (!cur_corecap)
		return;

	cap = get_devcap();
	apply_caps(cap);
}

static void do_cap_control(void)
{
	mutex_lock(&core_lock);
	__do_cap_control();
	mutex_unlock(&core_lock);
}

static void update_cur_corecap(void)
{
	struct tegra_sysedp_corecap *cap;
	unsigned int power;
	unsigned int relaxed_power;
	int i;

	if (!capping_device_platdata)
		return;

	power = avail_power * capping_device_platdata->core_gain / 100;

	i = capping_device_platdata->corecap_size - 1;
	cap = capping_device_platdata->corecap + i;

	for (; i >= 0; i--, cap--) {
		switch (cap_method) {
		default:
			pr_warn("%s: Unknown cap_method, %x!  Assuming direct.\n",
					__func__, cap_method);
			cap_method = TEGRA_SYSEDP_CAP_METHOD_DIRECT;
			/* Intentional fall-through*/
		case TEGRA_SYSEDP_CAP_METHOD_DIRECT:
			relaxed_power = 0;
			break;

		case TEGRA_SYSEDP_CAP_METHOD_SIGNAL:
			relaxed_power = min(avail_oc_relax, cap->pthrot);
			break;

		case TEGRA_SYSEDP_CAP_METHOD_RELAX:
			relaxed_power = cap->pthrot;
			break;
		}

		if (cap->power <= power + relaxed_power) {
			cur_corecap = cap;
			cpu_power_balance = power + relaxed_power
				- cap->power;
			return;
		}
	}

	cur_corecap = capping_device_platdata->corecap;
	cpu_power_balance = 0;
}

/* set the available power budget for cpu/gpu/emc (in mW) */
void sysedp_set_dynamic_cap(unsigned int power, unsigned int oc_relax)
{
	if (!init_done)
		return;

	mutex_lock(&core_lock);
	avail_power = power;
	avail_oc_relax = oc_relax;
	update_cur_corecap();
	__do_cap_control();
	mutex_unlock(&core_lock);
}

static void capping_worker(struct work_struct *work)
{
	if (!gpu_busy)
		do_cap_control();
}

/*
 * Return true if load was above threshold for at least
 * gpu_high_count number of notifications
 */
static bool calc_gpu_busy(unsigned int load)
{
	unsigned int mask;

	mask = (1 << gpu_high_count) - 1;

	gpu_high_hist <<= 1;
	if (load >= gpu_high_threshold)
		gpu_high_hist |= 1;

	return (gpu_high_hist & mask) == mask;
}

void tegra_edp_notify_gpu_load(unsigned int load)
{
	bool old;

	old = gpu_busy;
	gpu_busy = calc_gpu_busy(load);

	if (gpu_busy == old || force_gpu_pri || !capping_device_platdata)
		return;

	cancel_delayed_work(&capping_work);

	if (gpu_busy)
		do_cap_control();
	else
		schedule_delayed_work(&capping_work,
				msecs_to_jiffies(gpu_window));
}

static int tegra_edp_cpu_notify(struct notifier_block *nb,
		unsigned long action, void *data)
{
	switch (action) {
	case CPU_UP_PREPARE:
		online_cpu_count = num_online_cpus() + 1;
		break;
	case CPU_DEAD:
		online_cpu_count = num_online_cpus();
		break;
	default:
		return NOTIFY_OK;
	}

	do_cap_control();
	return NOTIFY_OK;
}

static struct notifier_block tegra_edp_cpu_nb = {
	.notifier_call = tegra_edp_cpu_notify
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *capping_debugfs_dir;

static int core_set(void *data, u64 val)
{
	unsigned int *pdata = data;
	unsigned int old;

	old = *pdata;
	*pdata = val;

	if (old != *pdata) {
		/* Changes to core_gain and cap_method require corecap update */
		if ((pdata == &capping_device_platdata->core_gain) ||
			(pdata == &cap_method))
			update_cur_corecap();
		do_cap_control();
	}

	return 0;
}

static int core_get(void *data, u64 *val)
{
	unsigned int *pdata = data;
	*val = *pdata;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(core_fops, core_get, core_set, "%lld\n");

static void create_attr(const char *name, unsigned int *data)
{
	struct dentry *d;

	d = debugfs_create_file(name, S_IRUGO | S_IWUSR, capping_debugfs_dir,
			data, &core_fops);
	WARN_ON(IS_ERR_OR_NULL(d));
}

static inline void edp_show_2core_cpucaps(struct seq_file *file)
{
	int i;
	struct tegra_system_edp_entry *p = capping_device_platdata->cpufreq_lim;

	seq_printf(file, "%5s %10s %10s\n",
			"Power", "1-core", "2-cores");

	for (i = 0; i < capping_device_platdata->cpufreq_lim_size; i++, p++) {
		seq_printf(file, "%5d %10u %10u\n",
				p->power_limit_100mW * 100,
				p->freq_limits[0],
				p->freq_limits[1]);
	}
}

static inline void edp_show_4core_cpucaps(struct seq_file *file)
{
	int i;
	struct tegra_system_edp_entry *p = capping_device_platdata->cpufreq_lim;

	seq_printf(file, "%5s %10s %10s %10s %10s\n",
			"Power", "1-core", "2-cores", "3-cores", "4-cores");

	for (i = 0; i < capping_device_platdata->cpufreq_lim_size; i++, p++) {
		seq_printf(file, "%5d %10u %10u %10u %10u\n",
				p->power_limit_100mW * 100,
				p->freq_limits[0],
				p->freq_limits[1],
				p->freq_limits[2],
				p->freq_limits[3]);
	}
}

static int cpucaps_show(struct seq_file *file, void *data)
{
	unsigned int max_nr_cpus = num_possible_cpus();

	if (!capping_device_platdata || !capping_device_platdata->cpufreq_lim)
		return -ENODEV;

	if (max_nr_cpus == 2)
		edp_show_2core_cpucaps(file);
	else if (max_nr_cpus == 4)
		edp_show_4core_cpucaps(file);

	return 0;
}

static int corecaps_show(struct seq_file *file, void *data)
{
	int i;
	struct tegra_sysedp_corecap *p;
	struct tegra_sysedp_devcap *c;
	struct tegra_sysedp_devcap *g;

	if (!capping_device_platdata || !capping_device_platdata->corecap)
		return -ENODEV;

	p = capping_device_platdata->corecap;

	seq_printf(file, "%s %s { %s %9s %9s } %s { %s %9s %9s } %7s\n",
		   "E-state",
		   "CPU-pri", "CPU-mW", "GPU-kHz", "EMC-kHz",
		   "GPU-pri", "CPU-mW", "GPU-kHz", "EMC-kHz",
		   "Pthrot");

	for (i = 0; i < capping_device_platdata->corecap_size; i++, p++) {
		c = &p->cpupri;
		g = &p->gpupri;
		seq_printf(file, "%7u %16u %9u %9u %18u %9u %9u %7u\n",
			   p->power,
			   c->cpu_power, c->gpufreq, c->emcfreq,
			   g->cpu_power, g->gpufreq, g->emcfreq,
			   p->pthrot);
	}

	return 0;
}

static int status_show(struct seq_file *file, void *data)
{
	mutex_lock(&core_lock);

	seq_printf(file, "cpus online : %u\n", online_cpu_count);
	seq_printf(file, "gpu priority: %u\n", gpu_priority());
	seq_printf(file, "gain        : %u\n", capping_device_platdata->core_gain);
	seq_printf(file, "core cap    : %u\n", cur_corecap->power);
	seq_printf(file, "max throttle: %u\n", cur_corecap->pthrot);
	seq_printf(file, "cpu balance : %u\n", cpu_power_balance);
	seq_printf(file, "cpu power   : %u\n", get_devcap()->cpu_power +
			cpu_power_balance);
	seq_printf(file, "cpu cap     : %u kHz\n", cur_caps.cpu);
	seq_printf(file, "gpu cap     : %u kHz\n", cur_caps.gpu);
	seq_printf(file, "emc cap     : %u kHz\n", cur_caps.emc);
	seq_printf(file, "cc method   : %u\n", cap_method);

	mutex_unlock(&core_lock);
	return 0;
}

static int longattr_open(struct inode *inode, struct file *file)
{
	return single_open(file, inode->i_private, NULL);
}

static const struct file_operations longattr_fops = {
	.open = longattr_open,
	.read = seq_read,
};

static void create_longattr(const char *name,
		int (*show)(struct seq_file *, void *))
{
	struct dentry *d;

	d = debugfs_create_file(name, S_IRUGO, capping_debugfs_dir, show,
			&longattr_fops);
	WARN_ON(IS_ERR_OR_NULL(d));
}

static void init_debug(void)
{
	struct dentry *d;

	if (!sysedp_debugfs_dir)
		return;

	d = debugfs_create_dir("capping", sysedp_debugfs_dir);
	if (IS_ERR_OR_NULL(d)) {
		WARN_ON(1);
		return;
	}

	capping_debugfs_dir = d;


	create_attr("favor_gpu", &force_gpu_pri);
	create_attr("gpu_threshold", &gpu_high_threshold);
	create_attr("force_cpu", &forced_caps.cpu);
	create_attr("force_gpu", &forced_caps.gpu);
	create_attr("force_emc", &forced_caps.emc);
	create_attr("gpu_window", &gpu_window);
	create_attr("gain", &capping_device_platdata->core_gain);
	create_attr("gpu_high_count", &gpu_high_count);
	create_attr("cap_method", &cap_method);

	create_longattr("corecaps", corecaps_show);
	create_longattr("cpucaps", cpucaps_show);
	create_longattr("status", status_show);
}
#else
static inline void init_debug(void) {}
#endif

static int init_clks(void)
{
	emc_cap_clk = clk_get_sys("battery_edp", "emc");
	if (IS_ERR(emc_cap_clk))
		return -ENODEV;

	gpu_cap_clk = clk_get_sys("battery_edp", "gpu");
	if (IS_ERR(gpu_cap_clk)) {
		clk_put(emc_cap_clk);
		return -ENODEV;
	}

	return 0;
}

static int sysedp_dynamic_capping_probe(struct platform_device *pdev)
{
	int r;
	struct tegra_sysedp_corecap *cap;
	int i;

	if (!pdev->dev.platform_data)
		return -EINVAL;

	online_cpu_count = num_online_cpus();
	INIT_DELAYED_WORK(&capping_work, capping_worker);
	pm_qos_add_request(&cpufreq_qos, PM_QOS_CPU_FREQ_MAX,
			   PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);

	r = register_cpu_notifier(&tegra_edp_cpu_nb);
	if (r)
		return r;

	r = init_clks();
	if (r)
		return r;


	mutex_lock(&core_lock);
	capping_device_platdata = pdev->dev.platform_data;
	avail_power = capping_device_platdata->init_req_watts;
	cap_method = capping_device_platdata->cap_method;
	switch (cap_method) {
	case TEGRA_SYSEDP_CAP_METHOD_DEFAULT:
		cap_method = TEGRA_SYSEDP_CAP_METHOD_SIGNAL;
		break;
	case TEGRA_SYSEDP_CAP_METHOD_DIRECT:
	case TEGRA_SYSEDP_CAP_METHOD_SIGNAL:
	case TEGRA_SYSEDP_CAP_METHOD_RELAX:
		break;
	default:
		pr_warn("%s: Unknown cap_method, %x!  Assuming direct.\n",
				__func__, cap_method);
		cap_method = TEGRA_SYSEDP_CAP_METHOD_DIRECT;
		break;
	}

	/* scale pthrot value in capping table */
	i = capping_device_platdata->corecap_size - 1;
	cap = capping_device_platdata->corecap + i;
	for (; i >= 0; i--, cap--) {
		cap->pthrot *= capping_device_platdata->pthrot_ratio;
		cap->pthrot /= 100;
	}
	update_cur_corecap();
	__do_cap_control();
	mutex_unlock(&core_lock);

	init_debug();

	init_done = 1;
	return 0;
}

static struct platform_driver sysedp_dynamic_capping_driver = {
	.probe = sysedp_dynamic_capping_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sysedp_dynamic_capping"
	}
};

static __init int sysedp_dynamic_capping_init(void)
{
	return platform_driver_register(&sysedp_dynamic_capping_driver);
}
late_initcall(sysedp_dynamic_capping_init);
