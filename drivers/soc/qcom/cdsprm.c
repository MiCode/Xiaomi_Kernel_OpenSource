/*
 * CDSP Request Manager
 *
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* This module uses rpmsg to communicate with CDSP and receive requests
 * for CPU L3 frequency and QoS.
 */

#define pr_fmt(fmt) "cdsprm: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/workqueue.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rpmsg.h>
#include <asm/arch_timer.h>
#include <linux/soc/qcom/cdsprm.h>

#define SYSMON_CDSP_FEATURE_L3		1
#define SYSMON_CDSP_FEATURE_RM		2
#define SYSMON_CDSP_QOS_FLAG_IGNORE	0
#define SYSMON_CDSP_QOS_FLAG_ENABLE	1
#define SYSMON_CDSP_QOS_FLAG_DISABLE	2
#define QOS_LATENCY_DISABLE_VALUE	-1
#define SYS_CLK_TICKS_PER_MS		19200
#define CDSPRM_MSG_QUEUE_DEPTH		10

struct sysmon_l3_msg {
	unsigned int l3_clock_khz;
};

struct sysmon_rm_msg {
	unsigned int b_qos_flag;
	unsigned int timetick_low;
	unsigned int timetick_high;
};

struct sysmon_msg {
	unsigned int feature_id;
	union {
		struct sysmon_l3_msg l3_struct;
		struct sysmon_rm_msg rm_struct;
	} feature_struct;
	unsigned int size;
};

enum delay_state {
	CDSP_DELAY_THREAD_NOT_STARTED = 0,
	CDSP_DELAY_THREAD_STARTED = 1,
	CDSP_DELAY_THREAD_BEFORE_SLEEP = 2,
	CDSP_DELAY_THREAD_AFTER_SLEEP = 3,
	CDSP_DELAY_THREAD_EXITING = 4,
};

struct cdsprm_request {
	struct list_head node;
	struct sysmon_msg msg;
	bool busy;
};

struct cdsprm {
	unsigned int			event;
	struct completion		msg_avail;
	struct cdsprm_request		msg_queue[CDSPRM_MSG_QUEUE_DEPTH];
	unsigned int			msg_queue_idx;
	struct workqueue_struct		*work_queue;
	struct workqueue_struct		*delay_work_queue;
	struct work_struct		cdsprm_work;
	struct work_struct		cdsprm_delay_work;
	struct mutex			rm_lock;
	spinlock_t			l3_lock;
	spinlock_t			list_lock;
	struct rpmsg_device		*rpmsgdev;
	enum delay_state		dt_state;
	enum delay_state		work_state;
	unsigned long long		timestamp;
	struct pm_qos_request		pm_qos_req;
	unsigned int			qos_latency_us;
	unsigned int			qos_max_ms;
	bool				qos_request;
	bool				b_rpmsg_register;
	bool				b_qosinitdone;
	int				latency_request;
	int (*set_l3_freq)(unsigned int freq_khz);
	int (*set_l3_freq_cached)(unsigned int freq_khz);
};

static struct cdsprm gcdsprm;
static LIST_HEAD(cdsprm_list);

/**
 * cdsprm_register_cdspl3gov() - Register a method to set L3 clock
 *                               frequency
 * @arg: cdsprm_l3 structure with set L3 clock frequency method
 *
 * Note: To be called from cdspl3 governor only. Called when the governor is
 *       started.
 */
void cdsprm_register_cdspl3gov(struct cdsprm_l3 *arg)
{
	unsigned long flags;

	if (!arg)
		return;

	spin_lock_irqsave(&gcdsprm.l3_lock, flags);
	gcdsprm.set_l3_freq = arg->set_l3_freq;
	spin_unlock_irqrestore(&gcdsprm.l3_lock, flags);
}
EXPORT_SYMBOL(cdsprm_register_cdspl3gov);

/**
 * cdsprm_unregister_cdspl3gov() - Unregister the method to set L3 clock
 *                                 frequency
 *
 * Note: To be called from cdspl3 governor only. Called when the governor is
 *       stopped
 */
void cdsprm_unregister_cdspl3gov(void)
{
	unsigned long flags;

	spin_lock_irqsave(&gcdsprm.l3_lock, flags);
	gcdsprm.set_l3_freq = NULL;
	spin_unlock_irqrestore(&gcdsprm.l3_lock, flags);
}
EXPORT_SYMBOL(cdsprm_unregister_cdspl3gov);

static void set_qos_latency(int latency)
{
	if (!gcdsprm.qos_request) {
		pm_qos_add_request(&gcdsprm.pm_qos_req,
			PM_QOS_CPU_DMA_LATENCY, latency);
		gcdsprm.qos_request = true;
	} else {
		pm_qos_update_request(&gcdsprm.pm_qos_req,
			latency);
	}
}

static void process_rm_request(struct sysmon_msg *msg)
{
	struct sysmon_rm_msg *rm_msg;

	if (!msg)
		return;

	if (msg->feature_id == SYSMON_CDSP_FEATURE_RM) {
		mutex_lock(&gcdsprm.rm_lock);
		rm_msg = &msg->feature_struct.rm_struct;
		if (rm_msg->b_qos_flag ==
			SYSMON_CDSP_QOS_FLAG_ENABLE) {
			if (gcdsprm.latency_request !=
					gcdsprm.qos_latency_us) {
				set_qos_latency(gcdsprm.qos_latency_us);
				gcdsprm.latency_request =
					gcdsprm.qos_latency_us;
				pr_debug("Set qos latency to %d\n",
						gcdsprm.latency_request);
			}
			gcdsprm.timestamp = ((rm_msg->timetick_low) |
			 ((unsigned long long)rm_msg->timetick_high << 32));
			if (gcdsprm.dt_state >= CDSP_DELAY_THREAD_AFTER_SLEEP) {
				flush_workqueue(gcdsprm.delay_work_queue);
				if (gcdsprm.dt_state ==
						CDSP_DELAY_THREAD_EXITING) {
					gcdsprm.dt_state =
						CDSP_DELAY_THREAD_STARTED;
					queue_work(gcdsprm.delay_work_queue,
					  &gcdsprm.cdsprm_delay_work);
				}
			} else if (gcdsprm.dt_state ==
						CDSP_DELAY_THREAD_NOT_STARTED) {
				gcdsprm.dt_state = CDSP_DELAY_THREAD_STARTED;
				queue_work(gcdsprm.delay_work_queue,
					&gcdsprm.cdsprm_delay_work);
			}
		} else if ((rm_msg->b_qos_flag ==
					SYSMON_CDSP_QOS_FLAG_DISABLE) &&
				(gcdsprm.latency_request !=
					QOS_LATENCY_DISABLE_VALUE)) {
			set_qos_latency(QOS_LATENCY_DISABLE_VALUE);
			gcdsprm.latency_request = QOS_LATENCY_DISABLE_VALUE;
			pr_debug("Set qos latency to %d\n",
					gcdsprm.latency_request);
		}
		mutex_unlock(&gcdsprm.rm_lock);
	} else {
		pr_err("Received incorrect msg on rm queue: %d\n",
				msg->feature_id);
	}
}

static void process_delayed_rm_request(struct work_struct *work)
{
	unsigned long long timestamp, curr_timestamp;
	unsigned int time_ms = 0;

	mutex_lock(&gcdsprm.rm_lock);

	timestamp = gcdsprm.timestamp;
	curr_timestamp = arch_counter_get_cntvct();

	while ((gcdsprm.latency_request ==
					gcdsprm.qos_latency_us) &&
			(curr_timestamp < timestamp)) {
		if ((timestamp - curr_timestamp) <
		(gcdsprm.qos_max_ms * SYS_CLK_TICKS_PER_MS))
			time_ms = (timestamp - curr_timestamp) /
						SYS_CLK_TICKS_PER_MS;
		else
			break;
		gcdsprm.dt_state = CDSP_DELAY_THREAD_BEFORE_SLEEP;

		mutex_unlock(&gcdsprm.rm_lock);
		usleep_range(time_ms * 1000, (time_ms + 2) * 1000);
		mutex_lock(&gcdsprm.rm_lock);

		gcdsprm.dt_state = CDSP_DELAY_THREAD_AFTER_SLEEP;
		timestamp = gcdsprm.timestamp;
		curr_timestamp = arch_counter_get_cntvct();
	}

	set_qos_latency(QOS_LATENCY_DISABLE_VALUE);
	gcdsprm.latency_request = QOS_LATENCY_DISABLE_VALUE;
	pr_debug("Set qos latency to %d\n", gcdsprm.latency_request);
	gcdsprm.dt_state = CDSP_DELAY_THREAD_EXITING;

	mutex_unlock(&gcdsprm.rm_lock);
}

static void process_cdsp_request(struct work_struct *work)
{
	struct cdsprm_request *req = NULL;
	struct sysmon_msg *msg = NULL;
	unsigned int l3_clock_khz;
	unsigned long flags;

	while (gcdsprm.work_state ==
			CDSP_DELAY_THREAD_STARTED) {
		req = list_first_entry_or_null(&cdsprm_list,
				struct cdsprm_request, node);
		if (req) {
			msg = &req->msg;
			if (!msg) {
				spin_lock_irqsave(&gcdsprm.list_lock, flags);
				list_del(&req->node);
				req->busy = false;
				spin_unlock_irqrestore(&gcdsprm.list_lock,
					flags);
				continue;
			}
			if ((msg->feature_id == SYSMON_CDSP_FEATURE_RM) &&
				gcdsprm.b_qosinitdone) {
				process_rm_request(msg);
			} else if (msg->feature_id == SYSMON_CDSP_FEATURE_L3) {
				l3_clock_khz =
				msg->feature_struct.l3_struct.l3_clock_khz;
				spin_lock_irqsave(&gcdsprm.l3_lock, flags);
				gcdsprm.set_l3_freq_cached =
							gcdsprm.set_l3_freq;
				spin_unlock_irqrestore(&gcdsprm.l3_lock, flags);
				if (gcdsprm.set_l3_freq_cached) {
					gcdsprm.set_l3_freq_cached(
						l3_clock_khz);
					pr_debug("Set L3 clock %d done\n",
					l3_clock_khz);
				}
			}
			spin_lock_irqsave(&gcdsprm.list_lock, flags);
			list_del(&req->node);
			req->busy = false;
			spin_unlock_irqrestore(&gcdsprm.list_lock, flags);
		} else {
			wait_for_completion(&gcdsprm.msg_avail);
		}
	}
}

static int cdsprm_rpmsg_probe(struct rpmsg_device *dev)
{
	/* Populate child nodes as platform devices */
	of_platform_populate(dev->dev.of_node, NULL, NULL, &dev->dev);
	gcdsprm.rpmsgdev = dev;
	dev_dbg(&dev->dev, "rpmsg probe called for cdsp\n");
	return 0;
}

static void cdsprm_rpmsg_remove(struct rpmsg_device *dev)
{
	gcdsprm.rpmsgdev = NULL;
}

static int cdsprm_rpmsg_callback(struct rpmsg_device *dev, void *data,
		int len, void *priv, u32 addr)
{
	struct sysmon_msg *msg = (struct sysmon_msg *)data;
	bool b_valid = false;
	struct cdsprm_request *req;
	unsigned long flags;

	if (!data || (len < sizeof(*msg))) {
		dev_err(&dev->dev,
		"Invalid message in rpmsg callback, length: %d, expected: %d\n",
				len, sizeof(*msg));
		return -EINVAL;
	}

	if ((msg->feature_id == SYSMON_CDSP_FEATURE_RM) &&
			gcdsprm.b_qosinitdone) {
		dev_dbg(&dev->dev, "Processing RM request\n");
		b_valid = true;
	} else if (msg->feature_id == SYSMON_CDSP_FEATURE_L3) {
		dev_dbg(&dev->dev, "Processing L3 request\n");
		spin_lock_irqsave(&gcdsprm.l3_lock, flags);
		gcdsprm.set_l3_freq_cached = gcdsprm.set_l3_freq;
		spin_unlock_irqrestore(&gcdsprm.l3_lock, flags);
		if (gcdsprm.set_l3_freq_cached)
			b_valid = true;
	}

	if (b_valid) {
		spin_lock_irqsave(&gcdsprm.list_lock, flags);
		if (!gcdsprm.msg_queue[gcdsprm.msg_queue_idx].busy) {
			req = &gcdsprm.msg_queue[gcdsprm.msg_queue_idx];
			req->busy = true;
			req->msg = *msg;
			if (gcdsprm.msg_queue_idx <
					(CDSPRM_MSG_QUEUE_DEPTH - 1))
				gcdsprm.msg_queue_idx++;
			else
				gcdsprm.msg_queue_idx = 0;
		} else {
			spin_unlock_irqrestore(&gcdsprm.list_lock, flags);
			dev_err(&dev->dev,
				"Unable to queue cdsp request, no memory\n");
			return -ENOMEM;
		}
		list_add_tail(&req->node, &cdsprm_list);
		spin_unlock_irqrestore(&gcdsprm.list_lock, flags);
		if (gcdsprm.work_state ==
				CDSP_DELAY_THREAD_NOT_STARTED) {
			gcdsprm.work_state =
				CDSP_DELAY_THREAD_STARTED;
			queue_work(gcdsprm.work_queue,
					&gcdsprm.cdsprm_work);
		} else {
			complete(&gcdsprm.msg_avail);
		}
	}

	return 0;
}

static int cdsp_rm_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	if (of_property_read_u32(dev->of_node,
			"qcom,qos-latency-us", &gcdsprm.qos_latency_us)) {
		return -EINVAL;
	}

	if (of_property_read_u32(dev->of_node,
			"qcom,qos-maxhold-ms", &gcdsprm.qos_max_ms)) {
		return -EINVAL;
	}

	dev_info(dev, "CDSP request manager driver probe called\n");
	gcdsprm.b_qosinitdone = true;

	return 0;
}

static const struct rpmsg_device_id cdsprm_rpmsg_match[] = {
	{ "cdsprmglink-apps-dsp" },
	{ },
};

static const struct of_device_id cdsprm_rpmsg_of_match[] = {
	{ .compatible = "qcom,msm-cdsprm-rpmsg" },
	{ },
};
MODULE_DEVICE_TABLE(of, cdsprm_rpmsg_of_match);

static struct rpmsg_driver cdsprm_rpmsg_client = {
	.id_table = cdsprm_rpmsg_match,
	.probe = cdsprm_rpmsg_probe,
	.remove = cdsprm_rpmsg_remove,
	.callback = cdsprm_rpmsg_callback,
	.drv = {
		.name = "qcom,msm_cdsprm_rpmsg",
		.of_match_table = cdsprm_rpmsg_of_match,
	},
};

static const struct of_device_id cdsp_rm_match_table[] = {
	{ .compatible = "qcom,msm-cdsp-rm" },
	{ },
};

static struct platform_driver cdsp_rm = {
	.probe = cdsp_rm_driver_probe,
	.driver = {
		.name = "msm_cdsp_rm",
		.of_match_table = cdsp_rm_match_table,
	},
};

static int __init cdsprm_init(void)
{
	int err;

	mutex_init(&gcdsprm.rm_lock);
	spin_lock_init(&gcdsprm.l3_lock);
	spin_lock_init(&gcdsprm.list_lock);
	init_completion(&gcdsprm.msg_avail);
	gcdsprm.work_queue = create_singlethread_workqueue("cdsprm-wq");
	if (!gcdsprm.work_queue) {
		pr_err("Failed to create rm work queue\n");
		return -ENOMEM;
	}

	gcdsprm.delay_work_queue =
			create_singlethread_workqueue("cdsprm-wq-delay");
	if (!gcdsprm.delay_work_queue) {
		err = -ENOMEM;
		pr_err("Failed to create rm delay work queue\n");
		goto err_wq;
	}

	INIT_WORK(&gcdsprm.cdsprm_delay_work, process_delayed_rm_request);
	INIT_WORK(&gcdsprm.cdsprm_work, process_cdsp_request);
	err = platform_driver_register(&cdsp_rm);
	if (err) {
		pr_err("Failed to register cdsprm platform driver: %d\n",
				err);
		goto bail;
	}

	err = register_rpmsg_driver(&cdsprm_rpmsg_client);
	if (err) {
		pr_err("Failed registering rpmsg driver with return %d\n",
				err);
		goto bail;
	}

	gcdsprm.b_rpmsg_register = true;
	pr_debug("Init successful\n");
	return 0;
bail:
	destroy_workqueue(gcdsprm.delay_work_queue);
err_wq:
	destroy_workqueue(gcdsprm.work_queue);
	return err;
}

static void __exit cdsprm_exit(void)
{
	if (gcdsprm.b_rpmsg_register)
		unregister_rpmsg_driver(&cdsprm_rpmsg_client);

	platform_driver_unregister(&cdsp_rm);
	gcdsprm.work_state = CDSP_DELAY_THREAD_NOT_STARTED;
	complete(&gcdsprm.msg_avail);
	destroy_workqueue(gcdsprm.work_queue);
	destroy_workqueue(gcdsprm.delay_work_queue);
}

module_init(cdsprm_init);
module_exit(cdsprm_exit);

MODULE_LICENSE("GPL v2");
