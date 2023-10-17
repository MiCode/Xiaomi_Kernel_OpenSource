// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "qcom-bwprof: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include "bwprof.h"
#include "trace-dcvs.h"
#include <linux/slab.h>

static LIST_HEAD(bwprof_list);
static DEFINE_MUTEX(bwprof_lock);

enum bwprof_type {
	BWPROF_DEV,
	BWPROF_MON,
	NUM_BWPROF_TYPES
};

enum read_mode {
	ZONE_CNT_READ,
	BYTE_CNT_READ
};

struct bwprof_spec {
	enum bwprof_type type;
};

struct bwprof_sample {
	u64			ts;
	u32			meas_mbps;
	u32			max_mbps;
	u16			mem_freq;
} __packed;

struct bwmon_node {
	bool			enabled;
	u16			prev_count;
	u16			unread_samples;
	u16			iterator;
	int			timer_count;
	u64			curr_sample_bytes;
	ktime_t			last_sample_ts;
	ktime_t			curr_sample_ts;
	u64			curr_sample_qtimer_cnt;
	const char		*client;
	void __iomem		*base;
	struct device		*dev;
	struct kobject		kobj;
	struct bwprof_sample	*buffer_data;
	struct list_head	list;
};

struct bwprof_dev_data {
	struct device		*dev;
	struct kobject		kobj;
	void __iomem		*memfreq_base;
	struct work_struct	work;
	struct workqueue_struct	*bwprof_wq;
	u32			sample_ms;
	u32			sample_cnt;
	struct hrtimer		bwprof_hrtimer;
	u32			sw_sample_ms;
	u32			hw_sample_ticks;
	u16			max_samples;
	u16			size_of_line;
	u16			read_mode;
	u16			reset_count;
	u32			mon_count;
	u32			bus_width;
};

static struct bwprof_dev_data *bwprof_data;
static void start_bwmon_node(struct bwmon_node *bw_node);
static void stop_bwmon_node(struct bwmon_node *bw_node);

struct qcom_bwprof_attr {
	struct attribute	attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};

#define to_bwprof_attr(_attr) \
	container_of(_attr, struct qcom_bwprof_attr, attr)
#define to_bwmon_node(k)	container_of(k, struct bwmon_node, kobj)

#define BWPROF_ATTR_RW(_name)						\
struct qcom_bwprof_attr _name =						\
__ATTR(_name, 0644, show_##_name, store_##_name)			\

#define BWPROF_ATTR_RO(_name)						\
struct qcom_bwprof_attr _name =						\
__ATTR(_name, 0444, show_##_name, NULL)					\


#define SAMPLE_MIN_MS	1U
#define SAMPLE_MAX_MS	2000U
static ssize_t store_sample_ms(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		return ret;

	val = max(val, SAMPLE_MIN_MS);
	val = min(val, SAMPLE_MAX_MS);
	mutex_lock(&bwprof_lock);
	bwprof_data->sample_ms = val;
	/*sample_ms greater than DEFAULT_SAMPLE_MS supported in multiple of DEFAULT_SAMPLE_MS*/
	if (bwprof_data->sample_ms > DEFAULT_SAMPLE_MS)
		bwprof_data->sample_ms = roundup(bwprof_data->sample_ms, DEFAULT_SAMPLE_MS);
	bwprof_data->sw_sample_ms = min(bwprof_data->sample_ms, DEFAULT_SAMPLE_MS);
	if (bwprof_data->sample_ms <= (MAX_HW_SAMPLE_MS - CNTR_RESET_TIMER_HEADROOM)) {
		bwprof_data->hw_sample_ticks = MAX_HW_SAMPLE_TICKS;
		bwprof_data->read_mode = BYTE_CNT_READ;
	} else {
		bwprof_data->hw_sample_ticks = mult_frac(bwprof_data->sw_sample_ms,
					HW_HZ, MSEC_PER_SEC);
		bwprof_data->read_mode = ZONE_CNT_READ;
	}
	bwprof_data->reset_count = (MAX_HW_SAMPLE_MS - CNTR_RESET_TIMER_HEADROOM) /
			bwprof_data->sample_ms;
	mutex_unlock(&bwprof_lock);
	return count;
}

static ssize_t show_sample_ms(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", bwprof_data->sample_ms);
}

static ssize_t store_mon_enabled(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	struct bwmon_node *bw_node = to_bwmon_node(kobj);
	bool val;
	int ret;

	ret = kstrtobool(buf, &val);
	if (ret)
		return ret;

	if (bw_node->enabled == val)
		return count;

	mutex_lock(&bwprof_lock);
	bw_node->enabled = val;
	if (val)
		start_bwmon_node(bw_node);
	else
		stop_bwmon_node(bw_node);
	mutex_unlock(&bwprof_lock);

	return count;
}

static ssize_t show_mon_enabled(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct bwmon_node *bw_node = to_bwmon_node(kobj);

	return scnprintf(buf, PAGE_SIZE, "%u\n", bw_node->enabled);
}

static ssize_t show_last_sample(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	int size = 0, i = 0, index = 0;
	struct bwmon_node *bw_node = to_bwmon_node(kobj);

	mutex_lock(&bwprof_lock);
	index = (bw_node->iterator - bw_node->unread_samples +
				bwprof_data->max_samples) % bwprof_data->max_samples;
	if (bw_node->enabled) {
		for (i = 0; i < bw_node->unread_samples; i++) {
			size += scnprintf(buf + size, PAGE_SIZE - size,
			"%llx\t%x\t%x\t%x\n", bw_node->buffer_data[index].ts,
			bw_node->buffer_data[index].meas_mbps,
			bw_node->buffer_data[index].max_mbps,
			bw_node->buffer_data[index].mem_freq);
			index = (index + 1) % bwprof_data->max_samples;
		}
	} else {

		dev_err(bw_node->dev, "mon is not enabled.\n");
		size = 0;
	}
	bw_node->unread_samples = 0;
	mutex_unlock(&bwprof_lock);
	return size;
}

static ssize_t show_client(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct bwmon_node *bw_node = to_bwmon_node(kobj);

	return scnprintf(buf, PAGE_SIZE, "%s\n", bw_node->client);
}

static BWPROF_ATTR_RW(sample_ms);
static BWPROF_ATTR_RW(mon_enabled);
static BWPROF_ATTR_RO(last_sample);
static BWPROF_ATTR_RO(client);

static struct attribute *bwprof_attr[] = {
	&sample_ms.attr,
	NULL,
};

static struct attribute *mon_attr[] = {
	&mon_enabled.attr,
	&last_sample.attr,
	&client.attr,
	NULL,
};

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct qcom_bwprof_attr *bwprof_attr = to_bwprof_attr(attr);
	ssize_t ret = -EIO;

	if (bwprof_attr->show)
		ret = bwprof_attr->show(kobj, attr, buf);

	return ret;
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct qcom_bwprof_attr *bwprof_attr = to_bwprof_attr(attr);
	ssize_t ret = -EIO;

	if (bwprof_attr->store)
		ret = bwprof_attr->store(kobj, attr, buf, count);

	return ret;
}

static const struct sysfs_ops bwprof_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static struct kobj_type bwprof_ktype = {
	.sysfs_ops	= &bwprof_sysfs_ops,
	.default_attrs	= bwprof_attr,

};

static struct kobj_type mon_ktype = {
	.sysfs_ops	= &bwprof_sysfs_ops,
	.default_attrs	= mon_attr,

};

static inline void bwmon_node_resume(struct bwmon_node *bw_node)
{
	writel_relaxed(1, BWMON_EN(bw_node));
}

static inline void bwmon_node_pause(struct bwmon_node *bw_node)
{
	writel_relaxed(0, BWMON_EN(bw_node));
}

#define BWMON_CLEAR_BIT		0x1
#define BWMON_CLEAR_ALL_BIT	0x2
static inline void bwmon_node_clear(struct bwmon_node *bw_node, bool clear_all)
{
	if (clear_all)
		writel_relaxed(BWMON_CLEAR_ALL_BIT, BWMON_CLEAR(bw_node));
	else
		writel_relaxed(BWMON_CLEAR_BIT, BWMON_CLEAR(bw_node));
	/*
	 * In some hardware versions since BWMON_CLEAR(m) register does not have
	 * self-clearing capability it needs to be cleared explicitly. But we also
	 * need to ensure the writes to it are successful before clearing it.
	 */
	wmb();
	writel_relaxed(0, BWMON_CLEAR(bw_node));
	writel_relaxed(bwprof_data->hw_sample_ticks, BWMON_SW(bw_node));
}

#define ZONE_THRES_LIM		0xFFFF
#define ZONE_CNT_THRES_LIM	0xFFFFFFFF
static void configure_bwmon_node(struct bwmon_node *bw_node)
{
	bwmon_node_pause(bw_node);
	bwmon_node_clear(bw_node, true);

	writel_relaxed(ZONE_THRES_LIM, BWMON_THRES_HI(bw_node));
	writel_relaxed(ZONE_THRES_LIM, BWMON_THRES_MED(bw_node));
	writel_relaxed(0, BWMON_THRES_LO(bw_node));
	writel_relaxed(ZONE_CNT_THRES_LIM, BWMON_ZONE_CNT_THRES(bw_node));
	writel_relaxed(0, BWMON_ZONE_ACTIONS(bw_node));
	writel_relaxed(bwprof_data->hw_sample_ticks, BWMON_SW(bw_node));
}

/* Note: bwprof_lock must be held before calling this function */
static void start_bwmon_node(struct bwmon_node *bw_node)
{
	configure_bwmon_node(bw_node);
	bw_node->prev_count = 0;
	bw_node->last_sample_ts = ktime_get();
	bwmon_node_resume(bw_node);
	if (!hrtimer_active(&bwprof_data->bwprof_hrtimer))
		hrtimer_start(&bwprof_data->bwprof_hrtimer,
			ms_to_ktime(bwprof_data->sw_sample_ms), HRTIMER_MODE_REL_PINNED);
}

/* Note: bwprof_lock must be held before calling this function */
static void stop_bwmon_node(struct bwmon_node *bw_node)
{
	bool all_disabled = true;
	struct bwmon_node *itr;

	bwmon_node_pause(bw_node);
	bwmon_node_clear(bw_node, true);

	list_for_each_entry(itr, &bwprof_list, list) {
		if (itr->enabled) {
			all_disabled = false;
			break;
		}
	}

	if (all_disabled) {
		hrtimer_cancel(&bwprof_data->bwprof_hrtimer);
		cancel_work_sync(&bwprof_data->work);
	}
}

#define PICOSECONDS_TO_MHZ(t)	((1000000 / t))
static inline u32 get_memfreq(void)
{
	u32 memfreq;

	memfreq = readl_relaxed(DDR_FREQ(bwprof_data));
	memfreq = PICOSECONDS_TO_MHZ(memfreq);
	return memfreq;
}

static bool get_bw_and_update_last_sample(struct bwmon_node *bw_node, bool update_last_sample)
{
	u32 count_diff;
	bool cntr_auto_reset = false;
	unsigned long count;

	bwmon_node_pause(bw_node);
	if (bwprof_data->read_mode == BYTE_CNT_READ)
		count = readl_relaxed(BWMON_BYTE_CNT(bw_node)) &
				BYTE_COUNT_MASK;
	else
		count = readl_relaxed(BWMON_ZONE1_MAX_BYTE_COUNT(bw_node)) &
				BYTE_COUNT_MASK;

	if (count < bw_node->prev_count)
		cntr_auto_reset = true;
	bw_node->timer_count++;
	if (bw_node->timer_count >= bwprof_data->reset_count) {
		bw_node->timer_count = 0;
		if (bwprof_data->read_mode == BYTE_CNT_READ)
			bw_node->curr_sample_bytes += (count - bw_node->prev_count);
		else
			bw_node->curr_sample_bytes += count;
		bw_node->prev_count =  0;
		bwmon_node_clear(bw_node, true);
		count = 0;
	}

	if (update_last_sample) {
		bw_node->curr_sample_ts = ktime_get();
		bw_node->curr_sample_qtimer_cnt = __arch_counter_get_cntvct_stable();
		count_diff = count + bw_node->curr_sample_bytes
						- bw_node->prev_count;
		bw_node->curr_sample_bytes = count_diff;
		bw_node->curr_sample_bytes <<= BYTE_COUNT_SHIFT;
		bw_node->prev_count = count;
	}
	bwmon_node_resume(bw_node);
	return cntr_auto_reset;
}

static void bwprof_update_work(struct work_struct *work)
{
	bool update_last_sample = false, cntr_auto_reset = false;
	u16 last_sample_idx;
	u32 mem_freq, max_mbps;
	s64 sample_time;
	struct bwmon_node *bw_node;

	mutex_lock(&bwprof_lock);
	bwprof_data->sample_cnt++;
	if (bwprof_data->sample_cnt * DEFAULT_SAMPLE_MS >= bwprof_data->sample_ms) {
		update_last_sample = true;
		bwprof_data->sample_cnt = 0;
		mem_freq = get_memfreq();
		max_mbps = bwprof_data->bus_width * mem_freq;
	}

	list_for_each_entry(bw_node, &bwprof_list, list) {
		if (!bw_node->enabled)
			continue;
		cntr_auto_reset = get_bw_and_update_last_sample(bw_node, update_last_sample);
		if (!update_last_sample)
			continue;
		if (likely(!cntr_auto_reset)) {
			if (bwprof_data->read_mode == BYTE_CNT_READ) {
				sample_time = ktime_us_delta(bw_node->curr_sample_ts,
						bw_node->last_sample_ts);
				do_div(bw_node->curr_sample_bytes,
							sample_time);
			} else {
				do_div(bw_node->curr_sample_bytes,
						bwprof_data->sample_ms * USEC_PER_MSEC);
			}
			bw_node->buffer_data[bw_node->iterator].meas_mbps =
						bw_node->curr_sample_bytes;
		} else {
			 /* Error in reading current sample, hence copy previous one */
			last_sample_idx = bw_node->iterator ?  bw_node->iterator - 1 :
				bwprof_data->max_samples - 1;
			bw_node->buffer_data[bw_node->iterator].meas_mbps =
				bw_node->buffer_data[last_sample_idx].meas_mbps;
		}
		bw_node->buffer_data[bw_node->iterator].ts = bw_node->curr_sample_qtimer_cnt;
		bw_node->buffer_data[bw_node->iterator].max_mbps = max_mbps;
		bw_node->buffer_data[bw_node->iterator].mem_freq = mem_freq;
		bw_node->unread_samples = min(++bw_node->unread_samples,
			bwprof_data->max_samples);
		trace_bwprof_last_sample(
			dev_name(bw_node->dev),
			bw_node->client,
			bw_node->curr_sample_ts,
			bw_node->buffer_data[bw_node->iterator].meas_mbps,
			bw_node->buffer_data[bw_node->iterator].max_mbps,
			bw_node->buffer_data[bw_node->iterator].mem_freq
		);
		bw_node->curr_sample_bytes = 0;
		bw_node->last_sample_ts = bw_node->curr_sample_ts;
		bw_node->iterator = (bw_node->iterator + 1) % bwprof_data->max_samples;
	}
	mutex_unlock(&bwprof_lock);
}

#define MAX_NAME_LEN	20
#define QCOM_BWPROF_CLIENT_PROP	"client"
static int bwprof_mon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bwmon_node *bw_node;
	char name[MAX_NAME_LEN];
	struct resource *res;
	int ret;

	if (!bwprof_data) {
		dev_err(dev, "Missing bwprof dev data!\n");
		return -ENODEV;
	}

	bw_node = devm_kzalloc(dev, sizeof(*bw_node), GFP_KERNEL);
	if (!bw_node)
		return -ENOMEM;
	bw_node->dev = dev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_err(dev, "base not found!\n");
		return -EINVAL;
	}
	bw_node->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!bw_node->base) {
		dev_err(dev, "Unable map base!\n");
		return -ENOMEM;
	}

	ret = of_property_read_string(dev->of_node, QCOM_BWPROF_CLIENT_PROP,
					&bw_node->client);
	if (ret < 0) {
		dev_err(dev, "client not found!\n", ret);
		return ret;
	}

	bw_node->buffer_data = devm_kzalloc(dev,
		bwprof_data->max_samples * bwprof_data->size_of_line, GFP_KERNEL);
	if (!bw_node->buffer_data)
		return -ENOMEM;

	snprintf(name, MAX_NAME_LEN, "bwmon%d", bwprof_data->mon_count);
	ret = kobject_init_and_add(&bw_node->kobj, &mon_ktype,
			&bwprof_data->kobj, name);
	if (ret < 0) {
		dev_err(dev, "failed to init bwprof mon kobj: %d\n", ret);
		kobject_put(&bw_node->kobj);
		return ret;
	}

	configure_bwmon_node(bw_node);

	mutex_lock(&bwprof_lock);
	list_add_tail(&bw_node->list, &bwprof_list);
	mutex_unlock(&bwprof_lock);

	bwprof_data->mon_count++;

	return 0;
}

static enum hrtimer_restart bwprof_hrtimer_handler(struct hrtimer *timer)
{
	ktime_t now = ktime_get();

	queue_work(bwprof_data->bwprof_wq, &bwprof_data->work);
	hrtimer_forward(timer, now, ms_to_ktime(bwprof_data->sw_sample_ms));

	return HRTIMER_RESTART;
}

static int bwprof_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	bwprof_data = devm_kzalloc(dev, sizeof(*bwprof_data), GFP_KERNEL);
	if (!bwprof_data)
		return -ENOMEM;

	bwprof_data->dev = dev;
	bwprof_data->sample_ms =  DEFAULT_SAMPLE_MS;
	bwprof_data->mon_count = 0;

	ret = of_property_read_u32(dev->of_node, "qcom,bus-width",
							&bwprof_data->bus_width);
	if (ret < 0 || !bwprof_data->bus_width) {
		dev_err(dev, "Missing or invalid bus-width: %d\n", ret);
		return -EINVAL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mem-freq");
	if (!res) {
		dev_err(dev, "mem-freq not found!\n");
		return -EINVAL;
	}
	bwprof_data->memfreq_base = devm_ioremap(dev, res->start,
					resource_size(res));
	if (!bwprof_data->memfreq_base) {
		dev_err(dev, "Unable map memfreq base!\n");
		return -ENOMEM;
	}

	hrtimer_init(&bwprof_data->bwprof_hrtimer, CLOCK_MONOTONIC,
				HRTIMER_MODE_REL);
	bwprof_data->bwprof_hrtimer.function = bwprof_hrtimer_handler;

	bwprof_data->bwprof_wq = create_freezable_workqueue("bwprof_wq");
	if (!bwprof_data->bwprof_wq) {
		dev_err(dev, "Couldn't create bwprof workqueue.\n");
		return -ENOMEM;
	}
	/*
	 * to get no of hex char in a line multiplying size of struct bwprof_sample by 2
	 * and adding 4 for tabs.
	 */
	bwprof_data->size_of_line = sizeof(struct bwprof_sample) * 2 + 4;
	bwprof_data->max_samples = PAGE_SIZE / bwprof_data->size_of_line;
	bwprof_data->sw_sample_ms = DEFAULT_SAMPLE_MS;
	bwprof_data->hw_sample_ticks = mult_frac(bwprof_data->sw_sample_ms, HW_HZ,
				MSEC_PER_SEC);
	bwprof_data->read_mode = ZONE_CNT_READ;
	bwprof_data->reset_count = (MAX_HW_SAMPLE_MS - CNTR_RESET_TIMER_HEADROOM) /
			bwprof_data->sample_ms;
	INIT_WORK(&bwprof_data->work, &bwprof_update_work);

	ret = kobject_init_and_add(&bwprof_data->kobj, &bwprof_ktype,
			&cpu_subsys.dev_root->kobj, "bw_prof");
	if (ret < 0) {
		dev_err(dev, "failed to init bwprof kobj: %d\n", ret);
		kobject_put(&bwprof_data->kobj);
		return ret;
	}

	return 0;
}

static int qcom_bwprof_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	const struct bwprof_spec *spec;
	enum bwprof_type type = NUM_BWPROF_TYPES;

	spec = of_device_get_match_data(dev);
	if (spec)
		type = spec->type;

	switch (type) {
	case BWPROF_DEV:
		if (bwprof_data) {
			dev_err(dev, "only one bwprof device allowed\n");
			ret = -ENODEV;
			break;
		}
		ret = bwprof_dev_probe(pdev);
		if (!ret && of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case BWPROF_MON:
		ret = bwprof_mon_probe(pdev);
		break;
	default:
		/*
		 * This should never happen.
		 */
		dev_err(dev, "Invalid bwprof type specified: %u\n", type);
		return -EINVAL;
	}

	if (ret < 0) {
		dev_err(dev, "Failure to probe bwprof device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct bwprof_spec spec[] = {
	[0] = { BWPROF_DEV },
	[1] = { BWPROF_MON },
};

static const struct of_device_id qcom_bwprof_match_table[] = {
	{ .compatible = "qcom,bwprof", .data = &spec[0] },
	{ .compatible = "qcom,bwprof-mon", .data = &spec[1] },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_bwprof_match_table);

static struct platform_driver qcom_bwprof_driver = {
	.probe = qcom_bwprof_driver_probe,
	.driver = {
		.name = "qcom-bwprof",
		.of_match_table = qcom_bwprof_match_table,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(qcom_bwprof_driver);

MODULE_DESCRIPTION("QCOM BWPROF driver");
MODULE_LICENSE("GPL");
