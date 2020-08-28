// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2010-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/dma-mapping.h>
#include <linux/math64.h>
#include <linux/of_platform.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/ftrace.h>
#include <linux/mm.h>
#include <linux/qcom_scm.h>
#include <asm/cacheflush.h>
#include <linux/qtee_shmbridge.h>

#include "../../devfreq/governor.h"
#include "msm_adreno_devfreq.h"

static DEFINE_SPINLOCK(tz_lock);
static DEFINE_SPINLOCK(sample_lock);
static DEFINE_SPINLOCK(suspend_lock);
/*
 * FLOOR is 5msec to capture up to 3 re-draws
 * per frame for 60fps content.
 */
#define FLOOR		        5000
/*
 * MIN_BUSY is 1 msec for the sample to be sent
 */
#define MIN_BUSY		1000
#define MAX_TZ_VERSION		0

/*
 * CEILING is 50msec, larger than any standard
 * frame length, but less than the idle timer.
 */
#define CEILING			50000
#define TZ_RESET_ID		0x3
#define TZ_UPDATE_ID		0x4
#define TZ_INIT_ID		0x6

#define TZ_RESET_ID_64          0x7
#define TZ_UPDATE_ID_64         0x8
#define TZ_INIT_ID_64           0x9

#define TZ_V2_UPDATE_ID_64         0xA
#define TZ_V2_INIT_ID_64           0xB
#define TZ_V2_INIT_CA_ID_64        0xC
#define TZ_V2_UPDATE_WITH_CA_ID_64 0xD

#define TAG "msm_adreno_tz: "

static u64 suspend_time;
static u64 suspend_start;
static unsigned long acc_total, acc_relative_busy;

static struct msm_adreno_extended_profile *partner_gpu_profile;
static void do_partner_start_event(struct work_struct *work);
static void do_partner_stop_event(struct work_struct *work);
static void do_partner_suspend_event(struct work_struct *work);
static void do_partner_resume_event(struct work_struct *work);

static struct workqueue_struct *workqueue;

/*
 * Returns GPU suspend time in millisecond.
 */
u64 suspend_time_ms(void)
{
	u64 suspend_sampling_time;
	u64 time_diff = 0;

	if (suspend_start == 0)
		return 0;

	suspend_sampling_time = (u64)ktime_to_ms(ktime_get());
	time_diff = suspend_sampling_time - suspend_start;
	/* Update the suspend_start sample again */
	suspend_start = suspend_sampling_time;
	return time_diff;
}

static ssize_t gpu_load_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned long sysfs_busy_perc = 0;
	/*
	 * Average out the samples taken since last read
	 * This will keep the average value in sync with
	 * with the client sampling duration.
	 */
	spin_lock(&sample_lock);
	if (acc_total)
		sysfs_busy_perc = (acc_relative_busy * 100) / acc_total;

	/* Reset the parameters */
	acc_total = 0;
	acc_relative_busy = 0;
	spin_unlock(&sample_lock);
	return snprintf(buf, PAGE_SIZE, "%lu\n", sysfs_busy_perc);
}

/*
 * Returns the time in ms for which gpu was in suspend state
 * since last time the entry is read.
 */
static ssize_t suspend_time_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	u64 time_diff = 0;

	spin_lock(&suspend_lock);
	time_diff = suspend_time_ms();
	/*
	 * Adding the previous suspend time also as the gpu
	 * can go and come out of suspend states in between
	 * reads also and we should have the total suspend
	 * since last read.
	 */
	time_diff += suspend_time;
	suspend_time = 0;
	spin_unlock(&suspend_lock);

	return snprintf(buf, PAGE_SIZE, "%llu\n", time_diff);
}

static ssize_t mod_percent_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	struct devfreq *devfreq = to_devfreq(dev);
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	priv->mod_percent = clamp_t(u32, val, 10, 1000);

	return count;
}

static ssize_t mod_percent_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", priv->mod_percent);
}

static DEVICE_ATTR_RO(gpu_load);

static DEVICE_ATTR_RO(suspend_time);
static DEVICE_ATTR_RW(mod_percent);

static const struct device_attribute *adreno_tz_attr_list[] = {
		&dev_attr_gpu_load,
		&dev_attr_suspend_time,
		&dev_attr_mod_percent,
		NULL
};

void compute_work_load(struct devfreq_dev_status *stats,
		struct devfreq_msm_adreno_tz_data *priv,
		struct devfreq *devfreq)
{
	u64 busy;

	spin_lock(&sample_lock);
	/*
	 * Keep collecting the stats till the client
	 * reads it. Average of all samples and reset
	 * is done when the entry is read
	 */
	acc_total += stats->total_time;
	busy = (u64)stats->busy_time * stats->current_frequency;
	do_div(busy, devfreq->profile->freq_table[0]);
	acc_relative_busy += busy;

	spin_unlock(&sample_lock);
}

/* Trap into the TrustZone, and call funcs there. */
static int __secure_tz_reset_entry2(unsigned int *scm_data, u32 size_scm_data,
					bool is_64)
{
	int ret;
	/* sync memory before sending the commands to tz */
	__iowmb();

	if (!is_64) {
		spin_lock(&tz_lock);
		ret = qcom_scm_io_reset();
		spin_unlock(&tz_lock);
	} else {
		ret = qcom_scm_dcvs_reset();
	}

	return ret;
}

static int __secure_tz_update_entry3(int level, s64 total_time, s64 busy_time,
		int context_count, struct devfreq_msm_adreno_tz_data *priv)
{
	int ret;
	/* sync memory before sending the commands to tz */
	__iowmb();

	if (!priv->is_64) {
		spin_lock(&tz_lock);
		ret = qcom_scm_dcvs_update(level, total_time, busy_time);
		spin_unlock(&tz_lock);
	} else if (!priv->ctxt_aware_enable) {
		ret = qcom_scm_dcvs_update_v2(level, total_time, busy_time);
	} else {
		ret = qcom_scm_dcvs_update_ca_v2(level, total_time, busy_time,
			context_count);
	}

	return ret;
}

static int tz_init_ca(struct device *dev,
	struct devfreq_msm_adreno_tz_data *priv)
{
	unsigned int tz_ca_data[2];
	phys_addr_t paddr;
	u8 *tz_buf;
	int ret;
	struct qtee_shm shm;

	/* Set data for TZ */
	tz_ca_data[0] = priv->bin.ctxt_aware_target_pwrlevel;
	tz_ca_data[1] = priv->bin.ctxt_aware_busy_penalty;

	if (!qtee_shmbridge_is_enabled()) {
		tz_buf = kzalloc(PAGE_ALIGN(sizeof(tz_ca_data)), GFP_KERNEL);
		if (!tz_buf)
			return -ENOMEM;
		paddr = virt_to_phys(tz_buf);
	} else {
		ret = qtee_shmbridge_allocate_shm(
				PAGE_ALIGN(sizeof(tz_ca_data)), &shm);
		if (ret)
			return -ENOMEM;
		tz_buf = shm.vaddr;
		paddr = shm.paddr;
	}

	memcpy(tz_buf, tz_ca_data, sizeof(tz_ca_data));
	/* Ensure memcpy completes execution */
	mb();
	dma_sync_single_for_device(dev, paddr,
		PAGE_ALIGN(sizeof(tz_ca_data)), DMA_BIDIRECTIONAL);

	ret = qcom_scm_dcvs_init_ca_v2(paddr, sizeof(tz_ca_data));

	if (!qtee_shmbridge_is_enabled())
		kzfree(tz_buf);
	else
		qtee_shmbridge_free_shm(&shm);

	return ret;
}

static int tz_init(struct device *dev, struct devfreq_msm_adreno_tz_data *priv,
			unsigned int *tz_pwrlevels, u32 size_pwrlevels,
			unsigned int *version, u32 size_version)
{
	int ret;
	phys_addr_t paddr;

	if (qcom_scm_dcvs_core_available()) {
		u8 *tz_buf;
		struct qtee_shm shm;

		if (!qtee_shmbridge_is_enabled()) {
			tz_buf = kzalloc(PAGE_ALIGN(size_pwrlevels),
						GFP_KERNEL);
			if (!tz_buf)
				return -ENOMEM;
			paddr = virt_to_phys(tz_buf);
		} else {
			ret = qtee_shmbridge_allocate_shm(
					PAGE_ALIGN(size_pwrlevels), &shm);
			if (ret)
				return -ENOMEM;
			tz_buf = shm.vaddr;
			paddr = shm.paddr;
		}

		memcpy(tz_buf, tz_pwrlevels, size_pwrlevels);
		/* Ensure memcpy completes execution */
		mb();
		dma_sync_single_for_device(dev, paddr,
			PAGE_ALIGN(size_pwrlevels), DMA_BIDIRECTIONAL);

		ret = qcom_scm_dcvs_init_v2(paddr, size_pwrlevels, version);
		if (!ret)
			priv->is_64 = true;
		if (!qtee_shmbridge_is_enabled())
			kzfree(tz_buf);
		else
			qtee_shmbridge_free_shm(&shm);
	} else
		ret = -EINVAL;

	 /* Initialize context aware feature, if enabled. */
	if (!ret && priv->ctxt_aware_enable) {
		if (priv->is_64 && qcom_scm_dcvs_ca_available()) {
			ret = tz_init_ca(dev, priv);
			/*
			 * If context aware feature initialization fails,
			 * just print an error message and return
			 * success as normal DCVS will still work.
			 */
			if (ret) {
				pr_err(TAG "tz: context aware DCVS init failed\n");
				priv->ctxt_aware_enable = false;
				return 0;
			}
		} else {
			pr_warn(TAG "tz: context aware DCVS not supported\n");
			priv->ctxt_aware_enable = false;
		}
	}

	return ret;
}

static inline int devfreq_get_freq_level(struct devfreq *devfreq,
	unsigned long freq)
{
	int lev;

	for (lev = 0; lev < devfreq->profile->max_state; lev++)
		if (freq == devfreq->profile->freq_table[lev])
			return lev;

	return -EINVAL;
}

static int tz_get_target_freq(struct devfreq *devfreq, unsigned long *freq)
{
	int result = 0;
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;
	struct devfreq_dev_status *stats = &devfreq->last_status;
	int val, level = 0;
	int context_count = 0;
	u64 busy_time;

	/* keeps stats.private_data == NULL   */
	result = devfreq_update_stats(devfreq);
	if (result) {
		pr_err(TAG "get_status failed %d\n", result);
		return result;
	}

	*freq = stats->current_frequency;
	priv->bin.total_time += stats->total_time;

	/* Update gpu busy time as per mod_percent */
	busy_time = stats->busy_time * priv->mod_percent;
	do_div(busy_time, 100);

	/* busy_time should not go over total_time */
	stats->busy_time = min_t(u64, busy_time, stats->total_time);

	priv->bin.busy_time += stats->busy_time;

	if (stats->private_data)
		context_count =  *((int *)stats->private_data);

	/* Update the GPU load statistics */
	compute_work_load(stats, priv, devfreq);
	/*
	 * Do not waste CPU cycles running this algorithm if
	 * the GPU just started, or if less than FLOOR time
	 * has passed since the last run or the gpu hasn't been
	 * busier than MIN_BUSY.
	 */
	if ((stats->total_time == 0) ||
		(priv->bin.total_time < FLOOR) ||
		(unsigned int) priv->bin.busy_time < MIN_BUSY) {
		return 0;
	}

	level = devfreq_get_freq_level(devfreq, stats->current_frequency);
	if (level < 0) {
		pr_err(TAG "bad freq %ld\n", stats->current_frequency);
		return level;
	}

	/*
	 * If there is an extended block of busy processing,
	 * increase frequency.  Otherwise run the normal algorithm.
	 */
	if (!priv->disable_busy_time_burst &&
			priv->bin.busy_time > CEILING) {
		val = -1 * level;
	} else {
		val = __secure_tz_update_entry3(level, priv->bin.total_time,
			priv->bin.busy_time, context_count, priv);
	}

	priv->bin.total_time = 0;
	priv->bin.busy_time = 0;

	/*
	 * If the decision is to move to a different level, make sure the GPU
	 * frequency changes.
	 */
	if (val) {
		level += val;
		level = max(level, 0);
		level = min_t(int, level, devfreq->profile->max_state - 1);
	}

	*freq = devfreq->profile->freq_table[level];
	return 0;
}

static int tz_notify(struct notifier_block *nb, unsigned long type, void *devp)
{
	int result = 0;
	struct devfreq *devfreq = devp;

	switch (type) {
	case ADRENO_DEVFREQ_NOTIFY_IDLE:
	case ADRENO_DEVFREQ_NOTIFY_RETIRE:
		mutex_lock(&devfreq->lock);
		result = update_devfreq(devfreq);
		mutex_unlock(&devfreq->lock);
		/* Nofifying partner bus governor if any */
		if (partner_gpu_profile && partner_gpu_profile->bus_devfreq) {
			mutex_lock(&partner_gpu_profile->bus_devfreq->lock);
			update_devfreq(partner_gpu_profile->bus_devfreq);
			mutex_unlock(&partner_gpu_profile->bus_devfreq->lock);
		}
		break;
	/* ignored by this governor */
	case ADRENO_DEVFREQ_NOTIFY_SUBMIT:
	default:
		break;
	}
	return notifier_from_errno(result);
}

static int tz_start(struct devfreq *devfreq)
{
	struct devfreq_msm_adreno_tz_data *priv;
	unsigned int tz_pwrlevels[MSM_ADRENO_MAX_PWRLEVELS + 1];
	int i, out, ret;
	unsigned int version;

	struct msm_adreno_extended_profile *gpu_profile = container_of(
					(devfreq->profile),
					struct msm_adreno_extended_profile,
					profile);

	/*
	 * Assuming that we have only one instance of the adreno device
	 * connected to this governor,
	 * can safely restore the pointer to the governor private data
	 * from the container of the device profile
	 */
	devfreq->data = gpu_profile->private_data;
	partner_gpu_profile = gpu_profile;

	priv = devfreq->data;
	priv->nb.notifier_call = tz_notify;

	out = 1;
	if (devfreq->profile->max_state < ARRAY_SIZE(tz_pwrlevels)) {
		for (i = 0; i < devfreq->profile->max_state; i++)
			tz_pwrlevels[out++] = devfreq->profile->freq_table[i];
		tz_pwrlevels[0] = i;
	} else {
		pr_err(TAG "tz_pwrlevels[] is too short\n");
		return -EINVAL;
	}

	INIT_WORK(&gpu_profile->partner_start_event_ws,
					do_partner_start_event);
	INIT_WORK(&gpu_profile->partner_stop_event_ws,
					do_partner_stop_event);
	INIT_WORK(&gpu_profile->partner_suspend_event_ws,
					do_partner_suspend_event);
	INIT_WORK(&gpu_profile->partner_resume_event_ws,
					do_partner_resume_event);

	ret = tz_init(&devfreq->dev, priv, tz_pwrlevels, sizeof(tz_pwrlevels),
			&version, sizeof(version));
	if (ret != 0 || version > MAX_TZ_VERSION) {
		pr_err(TAG "tz_init failed\n");
		return ret;
	}

	for (i = 0; adreno_tz_attr_list[i] != NULL; i++)
		device_create_file(&devfreq->dev, adreno_tz_attr_list[i]);

	return kgsl_devfreq_add_notifier(devfreq->dev.parent, &priv->nb);
}

static int tz_stop(struct devfreq *devfreq)
{
	int i;
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;

	kgsl_devfreq_del_notifier(devfreq->dev.parent, &priv->nb);

	for (i = 0; adreno_tz_attr_list[i] != NULL; i++)
		device_remove_file(&devfreq->dev, adreno_tz_attr_list[i]);

	flush_workqueue(workqueue);

	/* leaving the governor and cleaning the pointer to private data */
	devfreq->data = NULL;
	partner_gpu_profile = NULL;
	return 0;
}

static int tz_suspend(struct devfreq *devfreq)
{
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;
	unsigned int scm_data[2] = {0, 0};

	__secure_tz_reset_entry2(scm_data, sizeof(scm_data), priv->is_64);

	priv->bin.total_time = 0;
	priv->bin.busy_time = 0;
	return 0;
}

static int tz_handler(struct devfreq *devfreq, unsigned int event, void *data)
{
	int result;
	struct msm_adreno_extended_profile *gpu_profile;
	struct device_node *node = devfreq->dev.parent->of_node;

	if (!of_device_is_compatible(node, "qcom,kgsl-3d0"))
		return -EINVAL;

	gpu_profile = container_of((devfreq->profile),
			struct msm_adreno_extended_profile,
			profile);

	switch (event) {
	case DEVFREQ_GOV_START:
		result = tz_start(devfreq);
		break;

	case DEVFREQ_GOV_STOP:
		/* Queue the stop work before the TZ is stopped */
		if (partner_gpu_profile && partner_gpu_profile->bus_devfreq)
			queue_work(workqueue,
				&gpu_profile->partner_stop_event_ws);
		spin_lock(&suspend_lock);
		suspend_start = 0;
		spin_unlock(&suspend_lock);
		result = tz_stop(devfreq);
		break;

	case DEVFREQ_GOV_SUSPEND:
		result = tz_suspend(devfreq);
		if (!result) {
			spin_lock(&suspend_lock);
			/* Collect the start sample for suspend time */
			suspend_start = (u64)ktime_to_ms(ktime_get());
			spin_unlock(&suspend_lock);
		}
		break;

	case DEVFREQ_GOV_RESUME:
		spin_lock(&suspend_lock);
		suspend_time += suspend_time_ms();
		/* Reset the suspend_start when gpu resumes */
		suspend_start = 0;
		spin_unlock(&suspend_lock);
		/* fallthrough */
	case DEVFREQ_GOV_INTERVAL:
		/* fallthrough, this governor doesn't use polling */
	default:
		result = 0;
		break;
	}

	if (partner_gpu_profile && partner_gpu_profile->bus_devfreq)
		switch (event) {
		case DEVFREQ_GOV_START:
			queue_work(workqueue,
					&gpu_profile->partner_start_event_ws);
			break;
		case DEVFREQ_GOV_SUSPEND:
			queue_work(workqueue,
					&gpu_profile->partner_suspend_event_ws);
			break;
		case DEVFREQ_GOV_RESUME:
			queue_work(workqueue,
					&gpu_profile->partner_resume_event_ws);
			break;
		}

	return result;
}

static void _do_partner_event(struct work_struct *work, unsigned int event)
{
	struct devfreq *bus_devfreq;

	if (partner_gpu_profile == NULL)
		return;

	bus_devfreq = partner_gpu_profile->bus_devfreq;

	if (bus_devfreq != NULL &&
		bus_devfreq->governor &&
		bus_devfreq->governor->event_handler)
		bus_devfreq->governor->event_handler(bus_devfreq, event, NULL);
}

static void do_partner_start_event(struct work_struct *work)
{
	_do_partner_event(work, DEVFREQ_GOV_START);
}

static void do_partner_stop_event(struct work_struct *work)
{
	_do_partner_event(work, DEVFREQ_GOV_STOP);
}

static void do_partner_suspend_event(struct work_struct *work)
{
	_do_partner_event(work, DEVFREQ_GOV_SUSPEND);
}

static void do_partner_resume_event(struct work_struct *work)
{
	_do_partner_event(work, DEVFREQ_GOV_RESUME);
}


static struct devfreq_governor msm_adreno_tz = {
	.name = "msm-adreno-tz",
	.get_target_freq = tz_get_target_freq,
	.event_handler = tz_handler,
};

static int __init msm_adreno_tz_init(void)
{
	workqueue = create_freezable_workqueue("governor_msm_adreno_tz_wq");

	if (workqueue == NULL)
		return -ENOMEM;

	return devfreq_add_governor(&msm_adreno_tz);
}
subsys_initcall(msm_adreno_tz_init);

static void __exit msm_adreno_tz_exit(void)
{
	int ret = devfreq_remove_governor(&msm_adreno_tz);

	if (ret)
		pr_err(TAG "failed to remove governor %d\n", ret);

	if (workqueue != NULL)
		destroy_workqueue(workqueue);
}

module_exit(msm_adreno_tz_exit);

MODULE_LICENSE("GPL v2");
