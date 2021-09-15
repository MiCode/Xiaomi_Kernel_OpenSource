// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

/*
 * This driver uses rpmsg to communicate with CDSP and receive requests
 * for CPU L3 frequency and QoS along with Cx Limit management and
 * thermal cooling handling.
 */

#define pr_fmt(fmt) "cdsprm: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rpmsg.h>
#include <linux/thermal.h>
#include <linux/debugfs.h>
#include <asm/arch_timer.h>
#include <linux/soc/qcom/cdsprm.h>
#include <linux/soc/qcom/cdsprm_cxlimit.h>

#define SYSMON_CDSP_FEATURE_L3_RX		1
#define SYSMON_CDSP_FEATURE_RM_RX		2
#define SYSMON_CDSP_FEATURE_COMPUTE_PRIO_TX	3
#define SYSMON_CDSP_FEATURE_NPU_LIMIT_TX	4
#define SYSMON_CDSP_FEATURE_NPU_LIMIT_RX	5
#define SYSMON_CDSP_FEATURE_NPU_ACTIVITY_TX	6
#define SYSMON_CDSP_FEATURE_NPU_ACTIVITY_RX	7
#define SYSMON_CDSP_FEATURE_NPU_CORNER_TX	8
#define SYSMON_CDSP_FEATURE_NPU_CORNER_RX	9
#define SYSMON_CDSP_FEATURE_THERMAL_LIMIT_TX	10
#define SYSMON_CDSP_FEATURE_CAMERA_ACTIVITY_TX	11
#define SYSMON_CDSP_FEATURE_VERSION_RX		12

#define SYSMON_CDSP_QOS_FLAG_IGNORE	0
#define SYSMON_CDSP_QOS_FLAG_ENABLE	1
#define SYSMON_CDSP_QOS_FLAG_DISABLE	2
#define QOS_LATENCY_DISABLE_VALUE	-1
#define SYS_CLK_TICKS_PER_MS		19200
#define CDSPRM_MSG_QUEUE_DEPTH		50
#define CDSP_THERMAL_MAX_STATE		10
#define HVX_THERMAL_MAX_STATE		10

struct sysmon_l3_msg {
	unsigned int l3_clock_khz;
};

struct sysmon_rm_msg {
	unsigned int b_qos_flag;
	unsigned int timetick_low;
	unsigned int timetick_high;
};

struct sysmon_npu_limit_msg {
	unsigned int corner;
};

struct sysmon_npu_limit_ack {
	unsigned int corner;
};

struct sysmon_compute_prio_msg {
	unsigned int priority_idx;
};

struct sysmon_npu_activity_msg {
	unsigned int b_enabled;
};

struct sysmon_npu_corner_msg {
	unsigned int corner;
};

struct sysmon_thermal_msg {
	unsigned short hvx_level;
	unsigned short cdsp_level;
};

struct sysmon_camera_msg {
	unsigned int b_enabled;
};

struct sysmon_version_msg {
	unsigned int id;
};

struct sysmon_msg {
	unsigned int feature_id;
	union {
		struct sysmon_l3_msg l3_struct;
		struct sysmon_rm_msg rm_struct;
		struct sysmon_npu_limit_msg npu_limit;
		struct sysmon_npu_activity_msg npu_activity;
		struct sysmon_npu_corner_msg npu_corner;
		struct sysmon_version_msg version;
	} fs;
	unsigned int size;
};

struct sysmon_msg_tx {
	unsigned int feature_id;
	union {
		struct sysmon_npu_limit_ack npu_limit_ack;
		struct sysmon_compute_prio_msg compute_prio;
		struct sysmon_npu_activity_msg npu_activity;
		struct sysmon_thermal_msg thermal;
		struct sysmon_npu_corner_msg npu_corner;
		struct sysmon_camera_msg camera;
	} fs;
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
	unsigned int			cdsp_version;
	unsigned int			event;
	struct completion		msg_avail;
	struct cdsprm_request		msg_queue[CDSPRM_MSG_QUEUE_DEPTH];
	unsigned int			msg_queue_idx;
	struct task_struct		*cdsprm_wq_task;
	struct workqueue_struct		*delay_work_queue;
	struct work_struct		cdsprm_delay_work;
	struct mutex			rm_lock;
	spinlock_t			l3_lock;
	spinlock_t			list_lock;
	struct mutex			rpmsg_lock;
	struct rpmsg_device		*rpmsgdev;
	enum delay_state		dt_state;
	unsigned long long		timestamp;
	struct pm_qos_request		pm_qos_req;
	unsigned int			qos_latency_us;
	unsigned int			qos_max_ms;
	unsigned int			compute_prio_idx;
	struct mutex			npu_activity_lock;
	bool				b_cx_limit_en;
	unsigned int			b_npu_enabled;
	unsigned int			b_camera_enabled;
	unsigned int			b_npu_activity_waiting;
	unsigned int			b_npu_corner_waiting;
	struct completion		npu_activity_complete;
	struct completion		npu_corner_complete;
	unsigned int			npu_enable_cnt;
	enum cdsprm_npu_corner		npu_corner;
	enum cdsprm_npu_corner		allowed_npu_corner;
	enum cdsprm_npu_corner		npu_corner_limit;
	struct mutex			thermal_lock;
	unsigned int			thermal_cdsp_level;
	unsigned int			thermal_hvx_level;
	struct thermal_cooling_device	*cdsp_tcdev;
	struct thermal_cooling_device	*hvx_tcdev;
	bool				qos_request;
	bool				b_rpmsg_register;
	bool				b_qosinitdone;
	bool				b_applyingNpuLimit;
	bool				b_silver_en;
	int					latency_request;
	struct dentry			*debugfs_dir;
	struct dentry			*debugfs_file;
	int (*set_l3_freq)(unsigned int freq_khz);
	int (*set_l3_freq_cached)(unsigned int freq_khz);
	int (*set_corner_limit)(enum cdsprm_npu_corner);
	int (*set_corner_limit_cached)(enum cdsprm_npu_corner);
	u32 *coreno;
	u32 corecount;
	struct dev_pm_qos_request *dev_pm_qos_req;
};

static struct cdsprm gcdsprm;
static LIST_HEAD(cdsprm_list);
static DECLARE_WAIT_QUEUE_HEAD(cdsprm_wq);

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

int cdsprm_cxlimit_npu_limit_register(
	const struct cdsprm_npu_limit_cbs *npu_limit_cb)
{
	if (!npu_limit_cb)
		return -EINVAL;

	gcdsprm.set_corner_limit = npu_limit_cb->set_corner_limit;

	return 0;
}
EXPORT_SYMBOL(cdsprm_cxlimit_npu_limit_register);

int cdsprm_cxlimit_npu_limit_deregister(void)
{
	if (!gcdsprm.set_corner_limit)
		return -EINVAL;

	gcdsprm.set_corner_limit = NULL;

	return 0;
}
EXPORT_SYMBOL(cdsprm_cxlimit_npu_limit_deregister);

int cdsprm_compute_core_set_priority(unsigned int priority_idx)
{
	struct sysmon_msg_tx rpmsg_msg_tx;

	gcdsprm.compute_prio_idx = priority_idx;

	if (gcdsprm.rpmsgdev && gcdsprm.cdsp_version) {
		rpmsg_msg_tx.feature_id =
			SYSMON_CDSP_FEATURE_COMPUTE_PRIO_TX;
		rpmsg_msg_tx.fs.compute_prio.priority_idx =
				priority_idx;
		rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
		rpmsg_send(gcdsprm.rpmsgdev->ept,
			&rpmsg_msg_tx,
			sizeof(rpmsg_msg_tx));
		pr_debug("Compute core priority set to %d\n",
			priority_idx);
	}

	return 0;
}
EXPORT_SYMBOL(cdsprm_compute_core_set_priority);

int cdsprm_cxlimit_npu_activity_notify(unsigned int b_enabled)
{
	int result = -EINVAL;
	struct sysmon_msg_tx rpmsg_msg_tx;

	if (!gcdsprm.b_cx_limit_en)
		return result;

	mutex_lock(&gcdsprm.npu_activity_lock);
	if (b_enabled)
		gcdsprm.npu_enable_cnt++;
	else if (gcdsprm.npu_enable_cnt)
		gcdsprm.npu_enable_cnt--;

	if ((gcdsprm.npu_enable_cnt &&
		gcdsprm.b_npu_enabled) ||
		(!gcdsprm.npu_enable_cnt &&
			!gcdsprm.b_npu_enabled)) {
		mutex_unlock(&gcdsprm.npu_activity_lock);
		return 0;
	}

	gcdsprm.b_npu_enabled = b_enabled;

	if (gcdsprm.rpmsgdev && gcdsprm.cdsp_version) {
		if (gcdsprm.b_npu_enabled)
			gcdsprm.b_npu_activity_waiting++;
		rpmsg_msg_tx.feature_id =
			SYSMON_CDSP_FEATURE_NPU_ACTIVITY_TX;
		rpmsg_msg_tx.fs.npu_activity.b_enabled =
			gcdsprm.b_npu_enabled;
		rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
		result = rpmsg_send(gcdsprm.rpmsgdev->ept,
				&rpmsg_msg_tx,
				sizeof(rpmsg_msg_tx));
		if (gcdsprm.b_npu_enabled && result)
			gcdsprm.b_npu_activity_waiting--;
	}

	if (gcdsprm.b_npu_enabled && !result) {
		mutex_unlock(&gcdsprm.npu_activity_lock);
		wait_for_completion(&gcdsprm.npu_activity_complete);
		mutex_lock(&gcdsprm.npu_activity_lock);
		gcdsprm.b_npu_activity_waiting--;
	}

	mutex_unlock(&gcdsprm.npu_activity_lock);
	return result;
}
EXPORT_SYMBOL(cdsprm_cxlimit_npu_activity_notify);

enum cdsprm_npu_corner cdsprm_cxlimit_npu_corner_notify(
				enum cdsprm_npu_corner corner)
{
	int result = -EINVAL;
	enum cdsprm_npu_corner past_npu_corner;
	enum cdsprm_npu_corner return_npu_corner = corner;
	struct sysmon_msg_tx rpmsg_msg_tx;

	if (gcdsprm.b_applyingNpuLimit || !gcdsprm.b_cx_limit_en)
		return corner;

	mutex_lock(&gcdsprm.npu_activity_lock);
	past_npu_corner = gcdsprm.npu_corner;
	gcdsprm.npu_corner = corner;

	if (gcdsprm.rpmsgdev && gcdsprm.cdsp_version) {
		if ((gcdsprm.npu_corner > past_npu_corner) ||
			!gcdsprm.npu_corner)
			gcdsprm.b_npu_corner_waiting++;
		rpmsg_msg_tx.feature_id =
			SYSMON_CDSP_FEATURE_NPU_CORNER_TX;
		rpmsg_msg_tx.fs.npu_corner.corner =
			(unsigned int)gcdsprm.npu_corner;
		rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
		result = rpmsg_send(gcdsprm.rpmsgdev->ept,
				&rpmsg_msg_tx,
				sizeof(rpmsg_msg_tx));
		if (((gcdsprm.npu_corner > past_npu_corner) ||
			!gcdsprm.npu_corner) && result)
			gcdsprm.b_npu_corner_waiting--;
	}

	if (((gcdsprm.npu_corner > past_npu_corner) ||
		!gcdsprm.npu_corner) && !result) {
		mutex_unlock(&gcdsprm.npu_activity_lock);
		wait_for_completion(&gcdsprm.npu_corner_complete);
		mutex_lock(&gcdsprm.npu_activity_lock);
		if (gcdsprm.allowed_npu_corner) {
			return_npu_corner = gcdsprm.allowed_npu_corner;
			gcdsprm.npu_corner = gcdsprm.allowed_npu_corner;
		}
		gcdsprm.b_npu_corner_waiting--;
	}

	mutex_unlock(&gcdsprm.npu_activity_lock);
	return return_npu_corner;
}
EXPORT_SYMBOL(cdsprm_cxlimit_npu_corner_notify);

int cdsprm_cxlimit_camera_activity_notify(unsigned int b_enabled)
{
	struct sysmon_msg_tx rpmsg_msg_tx;

	if (!gcdsprm.b_cx_limit_en)
		return -EINVAL;

	gcdsprm.b_camera_enabled = b_enabled;

	if (gcdsprm.rpmsgdev && gcdsprm.cdsp_version) {
		rpmsg_msg_tx.feature_id =
			SYSMON_CDSP_FEATURE_CAMERA_ACTIVITY_TX;
		rpmsg_msg_tx.fs.camera.b_enabled =
				b_enabled;
		rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
		rpmsg_send(gcdsprm.rpmsgdev->ept,
			&rpmsg_msg_tx,
			sizeof(rpmsg_msg_tx));
	}

	return 0;
}
EXPORT_SYMBOL(cdsprm_cxlimit_camera_activity_notify);

static int cdsprm_thermal_cdsp_clk_limit(unsigned int level)
{
	int result = -EINVAL;
	struct sysmon_msg_tx rpmsg_msg_tx;

	mutex_lock(&gcdsprm.thermal_lock);

	if (gcdsprm.rpmsgdev && gcdsprm.cdsp_version) {
		rpmsg_msg_tx.feature_id =
			SYSMON_CDSP_FEATURE_THERMAL_LIMIT_TX;
		rpmsg_msg_tx.fs.thermal.hvx_level =
			gcdsprm.thermal_hvx_level;
		rpmsg_msg_tx.fs.thermal.cdsp_level = level;
		rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
		result = rpmsg_send(gcdsprm.rpmsgdev->ept,
					&rpmsg_msg_tx,
					sizeof(rpmsg_msg_tx));
	}

	if (result == 0)
		gcdsprm.thermal_cdsp_level = level;

	mutex_unlock(&gcdsprm.thermal_lock);

	return result;
}

static int cdsprm_thermal_hvx_instruction_limit(unsigned int level)
{
	int result = -EINVAL;
	struct sysmon_msg_tx rpmsg_msg_tx;

	mutex_lock(&gcdsprm.thermal_lock);

	if (gcdsprm.rpmsgdev && gcdsprm.cdsp_version) {
		rpmsg_msg_tx.feature_id =
			SYSMON_CDSP_FEATURE_THERMAL_LIMIT_TX;
		rpmsg_msg_tx.fs.thermal.hvx_level = level;
		rpmsg_msg_tx.fs.thermal.cdsp_level =
				gcdsprm.thermal_cdsp_level;
		rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
		result = rpmsg_send(gcdsprm.rpmsgdev->ept,
				&rpmsg_msg_tx,
				sizeof(rpmsg_msg_tx));
	}

	if (result == 0)
		gcdsprm.thermal_hvx_level = level;

	mutex_unlock(&gcdsprm.thermal_lock);

	return result;
}

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

static void qos_cores_init(struct device *dev)
{
	int i, err = 0;
	u32 *cpucores = NULL;

	of_find_property(dev->of_node,
					"qcom,qos-cores", &gcdsprm.corecount);

	if (gcdsprm.corecount) {
		gcdsprm.corecount /= sizeof(u32);

		cpucores = kcalloc(gcdsprm.corecount,
					sizeof(u32), GFP_KERNEL);

		if (cpucores == NULL) {
			dev_err(dev,
					"kcalloc failed for cpucores\n");
			gcdsprm.b_silver_en = false;
		} else {
			for (i = 0; i < gcdsprm.corecount; i++) {
				err = of_property_read_u32_index(dev->of_node,
					"qcom,qos-cores", i, &cpucores[i]);
				if (err) {
					dev_err(dev,
						"%s: failed to read QOS coree for core:%d\n",
							__func__, i);
					gcdsprm.b_silver_en = false;
				}
			}

			gcdsprm.coreno = cpucores;

			gcdsprm.dev_pm_qos_req = kcalloc(gcdsprm.corecount,
				sizeof(struct dev_pm_qos_request), GFP_KERNEL);

			if (gcdsprm.dev_pm_qos_req == NULL) {
				dev_err(dev,
						"kcalloc failed for dev_pm_qos_req\n");
				gcdsprm.b_silver_en = false;
			}
		}
	}
}

static void set_qos_latency(int latency)
{
	int err = 0;
	u32 ii = 0;
	int cpu;

	if (gcdsprm.b_silver_en) {

		for (ii = 0; ii < gcdsprm.corecount; ii++) {
			cpu = gcdsprm.coreno[ii];

			if (!gcdsprm.qos_request) {
				err = dev_pm_qos_add_request(
						get_cpu_device(cpu),
						&gcdsprm.dev_pm_qos_req[ii],
						DEV_PM_QOS_RESUME_LATENCY,
						latency);
			} else {
				err = dev_pm_qos_update_request(
						&gcdsprm.dev_pm_qos_req[ii],
						latency);
			}

			if (err < 0) {
				pr_err("%s: %s: PM voting cpu:%d fail,err %d,QoS update %d\n",
					current->comm, __func__, cpu,
					err, gcdsprm.qos_request);
				break;
			}
		}

		if (err >= 0)
			gcdsprm.qos_request = true;
	} else {
		if (!gcdsprm.qos_request) {
			pm_qos_add_request(&gcdsprm.pm_qos_req,
			PM_QOS_CPU_DMA_LATENCY, latency);
			gcdsprm.qos_request = true;
		} else {
			pm_qos_update_request(&gcdsprm.pm_qos_req,
			latency);
		}
	}
}

static void process_rm_request(struct sysmon_msg *msg)
{
	struct sysmon_rm_msg *rm_msg;

	if (!msg)
		return;

	if (msg->feature_id == SYSMON_CDSP_FEATURE_RM_RX) {
		mutex_lock(&gcdsprm.rm_lock);
		rm_msg = &msg->fs.rm_struct;
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
					PM_QOS_RESUME_LATENCY_DEFAULT_VALUE)) {
			set_qos_latency(PM_QOS_RESUME_LATENCY_DEFAULT_VALUE);
			gcdsprm.latency_request =
					PM_QOS_RESUME_LATENCY_DEFAULT_VALUE;
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
			time_ms = ((unsigned int)(timestamp - curr_timestamp)) /
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

	set_qos_latency(PM_QOS_RESUME_LATENCY_DEFAULT_VALUE);
	gcdsprm.latency_request = PM_QOS_RESUME_LATENCY_DEFAULT_VALUE;
	pr_debug("Set qos latency to %d\n", gcdsprm.latency_request);
	gcdsprm.dt_state = CDSP_DELAY_THREAD_EXITING;

	mutex_unlock(&gcdsprm.rm_lock);
}

static void cdsprm_rpmsg_send_details(void)
{
	struct sysmon_msg_tx rpmsg_msg_tx;

	if (!gcdsprm.cdsp_version)
		return;

	if (gcdsprm.b_cx_limit_en) {
		reinit_completion(&gcdsprm.npu_activity_complete);
		reinit_completion(&gcdsprm.npu_corner_complete);

		if (gcdsprm.npu_corner) {
			rpmsg_msg_tx.feature_id =
				SYSMON_CDSP_FEATURE_NPU_CORNER_TX;
			rpmsg_msg_tx.fs.npu_corner.corner =
				(unsigned int)gcdsprm.npu_corner;
			rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
			rpmsg_send(gcdsprm.rpmsgdev->ept,
					&rpmsg_msg_tx,
					sizeof(rpmsg_msg_tx));
		}

		if (gcdsprm.b_npu_enabled) {
			rpmsg_msg_tx.feature_id =
				SYSMON_CDSP_FEATURE_NPU_ACTIVITY_TX;
			rpmsg_msg_tx.fs.npu_activity.b_enabled =
				gcdsprm.b_npu_enabled;
			rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
			rpmsg_send(gcdsprm.rpmsgdev->ept,
				&rpmsg_msg_tx,
				sizeof(rpmsg_msg_tx));
		}

		cdsprm_compute_core_set_priority(gcdsprm.compute_prio_idx);

		if (gcdsprm.b_camera_enabled) {
			rpmsg_msg_tx.feature_id =
				SYSMON_CDSP_FEATURE_CAMERA_ACTIVITY_TX;
			rpmsg_msg_tx.fs.camera.b_enabled =
					gcdsprm.b_camera_enabled;
			rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
			rpmsg_send(gcdsprm.rpmsgdev->ept,
				&rpmsg_msg_tx,
				sizeof(rpmsg_msg_tx));
		}
	}

	if (gcdsprm.thermal_cdsp_level) {
		cdsprm_thermal_cdsp_clk_limit(
			gcdsprm.thermal_cdsp_level);
	} else if (gcdsprm.thermal_hvx_level) {
		cdsprm_thermal_hvx_instruction_limit(
			gcdsprm.thermal_hvx_level);
	}
}

static struct cdsprm_request *get_next_request(void)
{
	struct cdsprm_request *req = NULL;
	unsigned long flags;

	spin_lock_irqsave(&gcdsprm.list_lock, flags);
	req = list_first_entry_or_null(&cdsprm_list,
				struct cdsprm_request, node);
	spin_unlock_irqrestore(&gcdsprm.list_lock,
					flags);

	return req;
}

static int process_cdsp_request_thread(void *data)
{
	struct cdsprm_request *req = NULL;
	struct sysmon_msg *msg = NULL;
	unsigned int l3_clock_khz;
	unsigned long flags;
	int result = 0;
	struct sysmon_msg_tx rpmsg_msg_tx;

	while (!kthread_should_stop()) {
		result = wait_event_interruptible(cdsprm_wq,
						(req = get_next_request()));

		if (kthread_should_stop())
			break;

		if (result)
			continue;

		if (!req)
			break;

		msg = &req->msg;

		if (msg && (msg->feature_id == SYSMON_CDSP_FEATURE_RM_RX) &&
			gcdsprm.b_qosinitdone) {
			process_rm_request(msg);
		} else if (msg && (msg->feature_id ==
			SYSMON_CDSP_FEATURE_L3_RX)) {
			l3_clock_khz = msg->fs.l3_struct.l3_clock_khz;

			spin_lock_irqsave(&gcdsprm.l3_lock, flags);
			gcdsprm.set_l3_freq_cached = gcdsprm.set_l3_freq;
			spin_unlock_irqrestore(&gcdsprm.l3_lock, flags);

			if (gcdsprm.set_l3_freq_cached) {
				gcdsprm.set_l3_freq_cached(l3_clock_khz);
				pr_debug("Set L3 clock %d done\n",
					l3_clock_khz);
			}
		} else if (msg && (msg->feature_id ==
				SYSMON_CDSP_FEATURE_NPU_LIMIT_RX)) {
			mutex_lock(&gcdsprm.npu_activity_lock);

			gcdsprm.set_corner_limit_cached =
						gcdsprm.set_corner_limit;

			if (gcdsprm.set_corner_limit_cached) {
				gcdsprm.npu_corner_limit =
					msg->fs.npu_limit.corner;
				gcdsprm.b_applyingNpuLimit = true;
				result = gcdsprm.set_corner_limit_cached(
						gcdsprm.npu_corner_limit);
				gcdsprm.b_applyingNpuLimit = false;
				pr_debug("Set NPU limit to %d\n",
					msg->fs.npu_limit.corner);
			} else {
				result = -ENOMSG;
				pr_debug("NPU limit not registered\n");
			}

			mutex_unlock(&gcdsprm.npu_activity_lock);
			/*
			 * Send Limit ack back to DSP
			 */
			rpmsg_msg_tx.feature_id =
				SYSMON_CDSP_FEATURE_NPU_LIMIT_TX;

			if (result == 0) {
				rpmsg_msg_tx.fs.npu_limit_ack.corner =
					msg->fs.npu_limit.corner;
			} else {
				rpmsg_msg_tx.fs.npu_limit_ack.corner =
						CDSPRM_NPU_CLK_OFF;
			}

			rpmsg_msg_tx.size = sizeof(rpmsg_msg_tx);
			result = rpmsg_send(gcdsprm.rpmsgdev->ept,
				&rpmsg_msg_tx,
				sizeof(rpmsg_msg_tx));

			if (result)
				pr_err("rpmsg send failed %d\n", result);
			else
				pr_debug("NPU limit ack sent\n");
		} else if (msg && (msg->feature_id ==
				SYSMON_CDSP_FEATURE_VERSION_RX)) {
			cdsprm_rpmsg_send_details();
			pr_debug("Sent preserved data to DSP\n");
		}

		spin_lock_irqsave(&gcdsprm.list_lock, flags);
		list_del(&req->node);
		req->busy = false;
		spin_unlock_irqrestore(&gcdsprm.list_lock, flags);
	}

	do_exit(0);
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
	gcdsprm.cdsp_version = 0;

	if (gcdsprm.b_cx_limit_en) {
		mutex_lock(&gcdsprm.npu_activity_lock);
		complete_all(&gcdsprm.npu_activity_complete);
		complete_all(&gcdsprm.npu_corner_complete);
		mutex_unlock(&gcdsprm.npu_activity_lock);

		gcdsprm.set_corner_limit_cached = gcdsprm.set_corner_limit;

		if ((gcdsprm.npu_corner_limit < CDSPRM_NPU_TURBO_L1) &&
			gcdsprm.set_corner_limit_cached)
			gcdsprm.set_corner_limit_cached(CDSPRM_NPU_TURBO_L1);
	}
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
		"Invalid message in rpmsg callback, length: %d, expected: %lu\n",
				len, sizeof(*msg));
		return -EINVAL;
	}

	if ((msg->feature_id == SYSMON_CDSP_FEATURE_RM_RX) &&
			gcdsprm.b_qosinitdone) {
		dev_dbg(&dev->dev, "Processing RM request\n");
		b_valid = true;
	} else if (msg->feature_id == SYSMON_CDSP_FEATURE_L3_RX) {
		dev_dbg(&dev->dev, "Processing L3 request\n");
		spin_lock_irqsave(&gcdsprm.l3_lock, flags);
		gcdsprm.set_l3_freq_cached = gcdsprm.set_l3_freq;
		spin_unlock_irqrestore(&gcdsprm.l3_lock, flags);
		if (gcdsprm.set_l3_freq_cached)
			b_valid = true;
	} else if ((msg->feature_id == SYSMON_CDSP_FEATURE_NPU_CORNER_RX) &&
			(gcdsprm.b_cx_limit_en)) {
		gcdsprm.allowed_npu_corner = msg->fs.npu_corner.corner;
		dev_dbg(&dev->dev,
			"Processing NPU corner request ack for %d\n",
			gcdsprm.allowed_npu_corner);
		if (gcdsprm.b_npu_corner_waiting)
			complete(&gcdsprm.npu_corner_complete);
	} else if ((msg->feature_id == SYSMON_CDSP_FEATURE_NPU_LIMIT_RX) &&
			(gcdsprm.b_cx_limit_en)) {
		dev_dbg(&dev->dev, "Processing NPU limit request for %d\n",
			msg->fs.npu_limit.corner);
		b_valid = true;
	} else if ((msg->feature_id == SYSMON_CDSP_FEATURE_NPU_ACTIVITY_RX) &&
			(gcdsprm.b_cx_limit_en)) {
		dev_dbg(&dev->dev, "Processing NPU activity request ack\n");
		if (gcdsprm.b_npu_activity_waiting)
			complete(&gcdsprm.npu_activity_complete);
	} else if (msg->feature_id == SYSMON_CDSP_FEATURE_VERSION_RX) {
		gcdsprm.cdsp_version = msg->fs.version.id;
		b_valid = true;
		dev_dbg(&dev->dev, "Received CDSP version 0x%x\n",
			gcdsprm.cdsp_version);
	} else {
		dev_err(&dev->dev, "Received incorrect msg feature %d\n",
		msg->feature_id);
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
			dev_dbg(&dev->dev,
				"Unable to queue cdsp request, no memory\n");
			return -ENOMEM;
		}

		list_add_tail(&req->node, &cdsprm_list);
		spin_unlock_irqrestore(&gcdsprm.list_lock, flags);
		wake_up_interruptible(&cdsprm_wq);
	}

	return 0;
}

static int cdsp_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = CDSP_THERMAL_MAX_STATE;

	return 0;
}

static int cdsp_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = gcdsprm.thermal_cdsp_level;

	return 0;
}

static int cdsp_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	if (state > CDSP_THERMAL_MAX_STATE)
		return -EINVAL;

	if (gcdsprm.thermal_cdsp_level == state)
		return 0;

	cdsprm_thermal_cdsp_clk_limit(state);

	return 0;
}

static const struct thermal_cooling_device_ops cdsp_cooling_ops = {
	.get_max_state = cdsp_get_max_state,
	.get_cur_state = cdsp_get_cur_state,
	.set_cur_state = cdsp_set_cur_state,
};

static int hvx_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = HVX_THERMAL_MAX_STATE;

	return 0;
}

static int hvx_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = gcdsprm.thermal_hvx_level;

	return 0;
}

static int hvx_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	if (state > HVX_THERMAL_MAX_STATE)
		return -EINVAL;

	if (gcdsprm.thermal_hvx_level == state)
		return 0;

	cdsprm_thermal_hvx_instruction_limit(state);

	return 0;
}

static int cdsprm_compute_prio_read(void *data, u64 *val)
{
	*val = gcdsprm.compute_prio_idx;

	return 0;
}

static int cdsprm_compute_prio_write(void *data, u64 val)
{
	cdsprm_compute_core_set_priority((unsigned int)val);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cdsprm_debugfs_fops,
			cdsprm_compute_prio_read,
			cdsprm_compute_prio_write,
			"%llu\n");

static const struct thermal_cooling_device_ops hvx_cooling_ops = {
	.get_max_state = hvx_get_max_state,
	.get_cur_state = hvx_get_cur_state,
	.set_cur_state = hvx_set_cur_state,
};

static int cdsp_rm_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct thermal_cooling_device *tcdev = 0;
	unsigned int cooling_cells = 0;

	gcdsprm.b_silver_en = of_property_read_bool(dev->of_node,
					"qcom,qos-cores");

	if (gcdsprm.b_silver_en)
		qos_cores_init(dev);

	if (of_property_read_u32(dev->of_node,
			"qcom,qos-latency-us", &gcdsprm.qos_latency_us)) {
		return -EINVAL;
	}

	if (of_property_read_u32(dev->of_node,
			"qcom,qos-maxhold-ms", &gcdsprm.qos_max_ms)) {
		return -EINVAL;
	}

	gcdsprm.compute_prio_idx = CDSPRM_COMPUTE_AIX_OVER_HVX;
	of_property_read_u32(dev->of_node,
				"qcom,compute-priority-mode",
				&gcdsprm.compute_prio_idx);

	gcdsprm.b_cx_limit_en = of_property_read_bool(dev->of_node,
				"qcom,compute-cx-limit-en");

	if (gcdsprm.b_cx_limit_en) {
		gcdsprm.debugfs_dir = debugfs_create_dir("compute", NULL);

		if (!gcdsprm.debugfs_dir) {
			dev_err(dev,
			"Failed to create debugfs directory for cdsprm\n");
		} else {
			gcdsprm.debugfs_file = debugfs_create_file("priority",
						0644, gcdsprm.debugfs_dir,
						NULL, &cdsprm_debugfs_fops);
			if (!gcdsprm.debugfs_file) {
				debugfs_remove_recursive(gcdsprm.debugfs_dir);
				dev_err(dev,
					"Failed to create debugfs file\n");
			}
		}
	}

	of_property_read_u32(dev->of_node,
				"#cooling-cells",
				&cooling_cells);

	if (cooling_cells && IS_ENABLED(CONFIG_THERMAL)) {
		tcdev = thermal_of_cooling_device_register(dev->of_node,
							"cdsp", NULL,
							&cdsp_cooling_ops);
		if (IS_ERR(tcdev)) {
			dev_err(dev,
				"CDSP thermal driver reg failed\n");
		}
		gcdsprm.cdsp_tcdev = tcdev;
		thermal_cdev_update(tcdev);
	}

	dev_dbg(dev, "CDSP request manager driver probe called\n");
	gcdsprm.b_qosinitdone = true;

	return 0;
}

static int hvx_rm_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct thermal_cooling_device *tcdev = 0;
	unsigned int cooling_cells = 0;

	of_property_read_u32(dev->of_node,
				"#cooling-cells",
				&cooling_cells);

	if (cooling_cells && IS_ENABLED(CONFIG_THERMAL)) {
		tcdev = thermal_of_cooling_device_register(dev->of_node,
							"hvx", NULL,
							&hvx_cooling_ops);
		if (IS_ERR(tcdev)) {
			dev_err(dev,
				"HVX thermal driver reg failed\n");
		}
		gcdsprm.hvx_tcdev = tcdev;
		thermal_cdev_update(tcdev);
	}

	dev_dbg(dev, "HVX request manager driver probe called\n");

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

static const struct of_device_id hvx_rm_match_table[] = {
	{ .compatible = "qcom,msm-hvx-rm" },
	{ },
};

static struct platform_driver hvx_rm = {
	.probe = hvx_rm_driver_probe,
	.driver = {
		.name = "msm_hvx_rm",
		.of_match_table = hvx_rm_match_table,
	},
};

static int __init cdsprm_init(void)
{
	int err;

	mutex_init(&gcdsprm.rm_lock);
	mutex_init(&gcdsprm.rpmsg_lock);
	mutex_init(&gcdsprm.npu_activity_lock);
	mutex_init(&gcdsprm.thermal_lock);
	spin_lock_init(&gcdsprm.l3_lock);
	spin_lock_init(&gcdsprm.list_lock);
	init_completion(&gcdsprm.msg_avail);
	init_completion(&gcdsprm.npu_activity_complete);
	init_completion(&gcdsprm.npu_corner_complete);

	gcdsprm.cdsprm_wq_task = kthread_run(process_cdsp_request_thread,
					NULL, "cdsprm-wq");

	if (!gcdsprm.cdsprm_wq_task) {
		pr_err("Failed to create kernel thread\n");
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
	err = platform_driver_register(&cdsp_rm);

	if (err) {
		pr_err("Failed to register cdsprm platform driver: %d\n",
				err);
		goto bail;
	}


	err = platform_driver_register(&hvx_rm);

	if (err) {
		pr_err("Failed to register hvxrm platform driver: %d\n",
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
	kthread_stop(gcdsprm.cdsprm_wq_task);

	return err;
}

late_initcall(cdsprm_init);
