/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_data/tegra_edp.h>

struct freqcap {
	unsigned int cpu;
	unsigned int gpu;
	unsigned int emc;
};

static unsigned int gpu_high_threshold = 700;
static unsigned int gpu_window = 80;
static unsigned int gain_factor = 130;
static unsigned int core_profile = TEGRA_SYSEDP_PROFILE_NORMAL;
static unsigned int online_cpu_count;
static bool gpu_busy;
static unsigned int core_state;
static unsigned int core_loan;
static struct tegra_sysedp_corecap *cur_corecap;
static struct clk *emc_cap_clk;
static struct clk *gpu_cap_clk;
static struct pm_qos_request cpufreq_qos;
static unsigned int cpu_power_offset;
static unsigned int force_gpu_pri;
static struct delayed_work core_work;
static unsigned int *core_edp_states;
static struct tegra_sysedp_platform_data *core_platdata;
static struct freqcap core_policy;
static struct freqcap forced_caps;
static struct freqcap cur_caps;
static DEFINE_MUTEX(core_lock);

static const char *profile_names[TEGRA_SYSEDP_PROFILE_NUM] = {
	[TEGRA_SYSEDP_PROFILE_NORMAL]	= "profile_normal",
	[TEGRA_SYSEDP_PROFILE_HIGHCORE]	= "profile_highcore"
};

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

	i = cpu_lut_match(power, core_platdata->cpufreq_lim,
			core_platdata->cpufreq_lim_size);
	p = core_platdata->cpufreq_lim + i;

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

	core_policy.cpu = get_cpufreq_lim(devcap->cpu_power +
			cpu_power_offset);
	core_policy.gpu = devcap->gpufreq * 1000;
	core_policy.emc = devcap->emcfreq * 1000;

	new.cpu = forced_caps.cpu ?: core_policy.cpu;
	new.gpu = forced_caps.gpu ?: core_policy.gpu;
	new.emc = forced_caps.emc ?: core_policy.emc;

	if (new.cpu != cur_caps.cpu)
		pm_qos_update_request(&cpufreq_qos, new.cpu);

	if (new.emc != cur_caps.emc) {
		r = clk_set_rate(emc_cap_clk, new.emc * 1000);
		WARN_ON(r);
	}

	if (new.gpu != cur_caps.gpu) {
		r = clk_set_rate(gpu_cap_clk, new.gpu * 1000);
		WARN_ON(r);
	}

	pr_caps(&cur_caps, &new, devcap->cpu_power);
	cur_caps = new;
}

static inline bool gpu_priority(void)
{
	return gpu_busy || force_gpu_pri;
}

static void __do_cap_control(void)
{
	struct tegra_sysedp_devcap *cap;

	if (!cur_corecap)
		return;

	cap = gpu_priority() ? &cur_corecap->gpupri : &cur_corecap->cpupri;
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
	int i;

	if (!core_platdata)
		return;

	power = core_edp_states[core_state] * gain_factor / 100;
	power += core_loan;
	i = core_platdata->corecap_size - 1;

	cap = core_profile == TEGRA_SYSEDP_PROFILE_HIGHCORE ?
			core_platdata->high_corecap : core_platdata->corecap;
	cap += i;

	for (; i >= 0; i--, cap--) {
		if (cap->power <= power) {
			cur_corecap = cap;
			return;
		}
	}

	WARN_ON(1);
	cur_corecap = core_platdata->corecap;
}

static void state_change_cb(unsigned int new_state, void *priv_data)
{
	mutex_lock(&core_lock);
	core_state = new_state;
	update_cur_corecap();
	__do_cap_control();
	mutex_unlock(&core_lock);
}

static unsigned int loan_update_cb(unsigned int new_size,
		struct edp_client *lender, void *priv_data)
{
	mutex_lock(&core_lock);
	core_loan = new_size;
	update_cur_corecap();
	__do_cap_control();
	mutex_unlock(&core_lock);
	return new_size;
}

static void loan_close_cb(struct edp_client *lender, void *priv_data)
{
	loan_update_cb(0, lender, priv_data);
}

static void core_worker(struct work_struct *work)
{
	if (!gpu_busy)
		do_cap_control();
}

void tegra_edp_notify_gpu_load(unsigned int load)
{
	bool old;

	old = gpu_busy;
	gpu_busy = load >= gpu_high_threshold;

	if (gpu_busy == old || force_gpu_pri || !core_platdata)
		return;

	cancel_delayed_work(&core_work);

	if (gpu_busy)
		do_cap_control();
	else
		schedule_delayed_work(&core_work,
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

static ssize_t core_request_store(struct edp_client *c,
		struct edp_client_attribute *attr, const char *s, size_t count)
{
	unsigned int id;
	unsigned int approved;
	int r;

	if (sscanf(s, "%u", &id) != 1)
		return -EINVAL;

	mutex_lock(&core_lock);

	r = edp_update_client_request(c, id, &approved);
	if (r)
		goto out;

	core_state = approved;
	update_cur_corecap();
	__do_cap_control();

out:
	mutex_unlock(&core_lock);
	return r ?: count;
}

static ssize_t core_profile_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", profile_names[core_profile]);
}

static ssize_t core_profile_store(struct edp_client *c,
		struct edp_client_attribute *attr, const char *buf,
		size_t count)
{
	int i;
	size_t l;
	const char *name;

	for (i = 0; i < ARRAY_SIZE(profile_names); i++) {
		name = profile_names[i];
		l = strlen(name);
		if ((l <= count) && (strncmp(buf, name, l) == 0))
			break;
	}

	if (i == ARRAY_SIZE(profile_names))
		return -ENOENT;

	if (i == TEGRA_SYSEDP_PROFILE_HIGHCORE && !core_platdata->high_corecap)
		return -ENODEV;

	mutex_lock(&core_lock);

	core_profile = i;
	update_cur_corecap();
	__do_cap_control();

	mutex_unlock(&core_lock);

	return count;
}

struct edp_client_attribute core_attrs[] = {
	__ATTR(set_request, 0200, NULL, core_request_store),
	__ATTR(profile, 0644, core_profile_show, core_profile_store),
	__ATTR_NULL
};

static struct edp_client core_client = {
	.name = "core",
	.priority = EDP_MIN_PRIO,
	.throttle = state_change_cb,
	.attrs = core_attrs,
	.notify_promotion = state_change_cb,
	.notify_loan_update = loan_update_cb,
	.notify_loan_close = loan_close_cb
};

#ifdef CONFIG_DEBUG_FS
static int core_set(void *data, u64 val)
{
	unsigned int *pdata = data;
	unsigned int old;

	old = *pdata;
	*pdata = val;

	if (old != *pdata) {
		if (pdata == &gain_factor)
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

static void create_attr(const char *name, struct dentry *parent,
		unsigned int *data)
{
	struct dentry *d;

	d = debugfs_create_file(name, S_IRUGO | S_IWUSR, parent, data,
			&core_fops);
	WARN_ON(IS_ERR_OR_NULL(d));
}

static __devinit void init_debug(void)
{
	if (!core_client.dentry) {
		WARN_ON(1);
		return;
	}

	create_attr("cpu_offset", core_client.dentry, &cpu_power_offset);
	create_attr("favor_gpu", core_client.dentry, &force_gpu_pri);
	create_attr("gpu_threshold", core_client.dentry, &gpu_high_threshold);
	create_attr("force_cpu", core_client.dentry, &forced_caps.cpu);
	create_attr("force_gpu", core_client.dentry, &forced_caps.gpu);
	create_attr("force_emc", core_client.dentry, &forced_caps.emc);
	create_attr("gpu_window", core_client.dentry, &gpu_window);
	create_attr("gain", core_client.dentry, &gain_factor);
}
#else
static inline void init_debug(void) {}
#endif

/* Ignore missing modem */
static __devinit void register_loan(void)
{
	struct edp_client *c;
	int r;

	c = edp_get_client("modem");
	if (!c) {
		pr_info("Could not access modem EDP client\n");
		return;
	}

	r = edp_register_loan(c, &core_client);
	WARN_ON(r);
}

static __devinit unsigned int get_num_states(
		struct tegra_sysedp_platform_data *pdata)
{
	unsigned int power = 0;
	unsigned int num = 0;
	unsigned int i;

	for (i = 0; i < pdata->corecap_size; i++) {
		if (pdata->corecap[i].power != power) {
			power = pdata->corecap[i].power;
			num++;
		}
	}

	return num;
}

static __devinit void get_states(struct tegra_sysedp_platform_data *pdata,
		unsigned int num, unsigned int *states)
{
	unsigned int power = 0;
	unsigned int e0i = 0;
	unsigned int i;

	for (i = 0; i < pdata->corecap_size; i++) {
		if (pdata->corecap[i].power == power)
			continue;

		power = pdata->corecap[i].power;
		states[num - e0i - 1] = power;
		e0i++;
	}
}

static __devinit unsigned int initial_req(struct edp_client *client,
		unsigned int watts)
{
	int i;

	for (i = 0; i < client->num_states; i++) {
		if (client->states[i] == watts)
			return i;
	}

	WARN_ON(1);
	return 0;
}

static __devinit int init_client(struct tegra_sysedp_platform_data *pdata)
{
	struct edp_manager *m;
	unsigned int cnt;
	unsigned int ei;
	int r;

	m = edp_get_manager("battery");
	if (!m)
		return -ENODEV;

	cnt = get_num_states(pdata);
	if (!cnt)
		return -EINVAL;

	core_edp_states = kzalloc(sizeof(*core_edp_states) * cnt, GFP_KERNEL);
	if (!core_edp_states)
		return -ENOMEM;

	get_states(pdata, cnt, core_edp_states);

	core_client.states = core_edp_states;
	core_client.num_states = cnt;
	core_client.e0_index = cnt - 1;
	core_client.private_data = &core_client;

	r = edp_register_client(m, &core_client);
	if (r)
		goto fail;

	ei = initial_req(&core_client, pdata->init_req_watts);
	r = edp_update_client_request(&core_client, ei, &core_state);
	if (r)
		return r;

	register_loan();
	return 0;

fail:
	kfree(core_edp_states);
	core_edp_states = NULL;
	return r;
}

static __devinit int init_clks(void)
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

static __devinit int tegra_sysedp_probe(struct platform_device *pdev)
{
	int r;

	if (!pdev->dev.platform_data)
		return -EINVAL;

	online_cpu_count = num_online_cpus();
	INIT_DELAYED_WORK(&core_work, core_worker);
	pm_qos_add_request(&cpufreq_qos, PM_QOS_CPU_FREQ_MAX,
			PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);

	r = register_cpu_notifier(&tegra_edp_cpu_nb);
	if (r)
		return r;

	r = init_clks();
	if (r)
		return r;

	r = init_client(pdev->dev.platform_data);
	if (r)
		return r;

	mutex_lock(&core_lock);
	core_platdata = pdev->dev.platform_data;
	update_cur_corecap();
	__do_cap_control();
	mutex_unlock(&core_lock);

	init_debug();

	return 0;
}

static struct platform_driver tegra_sysedp_driver = {
	.probe = tegra_sysedp_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "tegra_sysedp"
	}
};

static __init int tegra_sysedp_init(void)
{
	return platform_driver_register(&tegra_sysedp_driver);
}
late_initcall(tegra_sysedp_init);
