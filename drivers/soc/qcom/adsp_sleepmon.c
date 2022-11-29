// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

/*
 * This driver exposes API for aDSP service layers to intimate session
 * start and stop. Based on the aDSP sessions and activity information
 * derived from SMEM statistics, the driver detects and acts on
 * possible aDSP sleep (voting related) issues.
 */

#define pr_fmt(fmt) "adsp_sleepmon: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/rpmsg.h>
#include <linux/debugfs.h>
#include <linux/soc/qcom/smem.h>
#include <asm/arch_timer.h>
#include <linux/jiffies.h>
#include <linux/suspend.h>
#include <uapi/misc/adsp_sleepmon.h>

#define ADSPSLEEPMON_SMEM_ADSP_PID						2
#define ADSPSLEEPMON_SLEEPSTATS_ADSP_SMEM_ID			606
#define ADSPSLEEPMON_SLEEPSTATS_ADSP_LPI_SMEM_ID		613
#define ADSPSLEEPMON_DSPPMSTATS_SMEM_ID					624
#define ADSPSLEEPMON_DSPPMSTATS_NUMPD					5
#define ADSPSLEEPMON_DSPPMSTATS_PID_FILTER				0x7F
#define ADSPSLEEPMON_DSPPMSTATS_AUDIO_PID				2
#define ADSPSLEEPMON_SYSMONSTATS_SMEM_ID				634
#define ADSPSLEEPMON_SYSMONSTATS_EVENTS_FEATURE_ID		2
#define ADSPSLEEPMON_SYS_CLK_TICKS_PER_SEC			19200000
#define ADSPSLEEPMON_SYS_CLK_TICKS_PER_MILLISEC		19200
#define ADSPSLEEPMON_LPI_WAIT_TIME			15
#define ADSPSLEEPMON_LPM_WAIT_TIME			5

#define ADSPSLEEPMON_AUDIO_CLIENT			1
#define ADSPSLEEPMON_DEVICE_NAME_LOCAL			 "msm_adsp_sleepmon"

struct sleep_stats {
	u32 stat_type;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
};

struct pd_clients {
	int pid;
	u32 num_active;
};

struct dsppm_stats {
	u32 version;
	u32 latency_us;
	u32 timestamp;
	struct pd_clients pd[ADSPSLEEPMON_DSPPMSTATS_NUMPD];
};

struct sysmon_event_stats {
	u32 core_clk;
	u32 ab_vote_lsb;
	u32 ab_vote_msb;
	u32 ib_vote_lsb;
	u32 ib_vote_msb;
	u32 sleep_latency;
	u32 timestamp_lsb;
	u32 timestamp_msb;
};

struct adspsleepmon_file {
	struct hlist_node hn;
	spinlock_t hlock;
	u32 b_connected;
	u32 num_sessions;
	u32 num_lpi_sessions;
};

struct adspsleepmon_audio {
	u32 num_sessions;
	u32 num_lpi_sessions;
};

struct adspsleepmon {
	bool b_enable;
	bool timer_event;
	bool timer_pending;
	bool suspend_event;
	bool smem_init_done;
	bool b_panic_lpm;
	bool b_panic_lpi;
	bool b_config_panic_lpm;
	bool b_config_panic_lpi;
	u32 lpm_wait_time;
	u32 lpi_wait_time;
	struct mutex lock;
	struct cdev cdev;
	struct class *class;
	dev_t devno;
	struct device *dev;
	struct task_struct *worker_task;
	struct adspsleepmon_audio audio_stats;
	struct hlist_head audio_clients;
	struct sleep_stats backup_lpm_stats;
	unsigned long long backup_lpm_timestamp;
	struct sleep_stats backup_lpi_stats;
	unsigned long long backup_lpi_timestamp;
	struct sleep_stats *lpm_stats;
	struct sleep_stats *lpi_stats;
	struct dsppm_stats *dsppm_stats;
	struct sysmon_event_stats *sysmon_event_stats;
	struct dentry *debugfs_dir;
	struct dentry *debugfs_panic_file;
	struct dentry *debugfs_master_stats;
	struct dentry *debugfs_read_panic_state;
};

static struct adspsleepmon g_adspsleepmon;
static void adspsleepmon_timer_cb(struct timer_list *unused);
static DEFINE_TIMER(adspsleep_timer, adspsleepmon_timer_cb);
static DECLARE_WAIT_QUEUE_HEAD(adspsleepmon_wq);

static int debugfs_panic_state_read(void *data, u64 *val)
{
	*val = g_adspsleepmon.b_panic_lpm | (g_adspsleepmon.b_panic_lpi << 1);

	return 0;
}

static int debugfs_panic_state_write(void *data, u64 val)
{
	if (!(val & 0x1))
		g_adspsleepmon.b_panic_lpm = false;
	else
		g_adspsleepmon.b_panic_lpm =
				g_adspsleepmon.b_config_panic_lpm;
	if (!(val & 0x2))
		g_adspsleepmon.b_panic_lpi = false;
	else
		g_adspsleepmon.b_panic_lpi =
					g_adspsleepmon.b_config_panic_lpi;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(panic_state_fops,
			debugfs_panic_state_read,
			debugfs_panic_state_write,
			"%u\n");

static int read_panic_state_show(struct seq_file *s, void *d)
{
	int val = g_adspsleepmon.b_panic_lpm | (g_adspsleepmon.b_panic_lpi << 1);

	if (val == 0)
		seq_puts(s, "\nPanic State: LPM and LPI panics Disabled\n");
	if (val == 1)
		seq_puts(s, "\nPanic State: LPM Panic enabled\n");
	if (val == 2)
		seq_puts(s, "\nPanic State: LPI Panic enabled\n");
	if (val == 3)
		seq_puts(s, "\nPanic State: LPI and LPM Panics enabled\n");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(read_panic_state);

static int master_stats_show(struct seq_file *s, void *d)
{
	int i = 0;
	u64 accumulated;

	if (g_adspsleepmon.sysmon_event_stats) {
		seq_puts(s, "\nsysMon stats:\n\n");
		seq_printf(s, "Core clock: %d\n",
				g_adspsleepmon.sysmon_event_stats->core_clk);
		seq_printf(s, "Ab vote: %llu\n",
				(((u64)g_adspsleepmon.sysmon_event_stats->ab_vote_msb << 32) |
					   g_adspsleepmon.sysmon_event_stats->ab_vote_lsb));
		seq_printf(s, "Ib vote: %llu\n",
				(((u64)g_adspsleepmon.sysmon_event_stats->ib_vote_msb << 32) |
					   g_adspsleepmon.sysmon_event_stats->ib_vote_lsb));
		seq_printf(s, "Sleep latency: %u\n",
				g_adspsleepmon.sysmon_event_stats->sleep_latency > 0 ?
				g_adspsleepmon.sysmon_event_stats->sleep_latency : U32_MAX);
		seq_printf(s, "Timestamp: %llu\n",
				(((u64)g_adspsleepmon.sysmon_event_stats->timestamp_msb << 32) |
					g_adspsleepmon.sysmon_event_stats->timestamp_lsb));
	}

	if (g_adspsleepmon.dsppm_stats) {
		seq_puts(s, "\nDSPPM stats:\n\n");
		seq_printf(s, "Version: %u\n", g_adspsleepmon.dsppm_stats->version);
		seq_printf(s, "Sleep latency: %u\n", g_adspsleepmon.dsppm_stats->latency_us);
		seq_printf(s, "Timestamp: %llu\n", g_adspsleepmon.dsppm_stats->timestamp);

		for (; i < ADSPSLEEPMON_DSPPMSTATS_NUMPD; i++) {
			seq_printf(s, "Pid: %d, Num active clients: %d\n",
						g_adspsleepmon.dsppm_stats->pd[i].pid,
						g_adspsleepmon.dsppm_stats->pd[i].num_active);
		}
	}

	if (g_adspsleepmon.lpm_stats) {
		accumulated = g_adspsleepmon.lpm_stats->accumulated;

		if (g_adspsleepmon.lpm_stats->last_entered_at >
					g_adspsleepmon.lpm_stats->last_exited_at)
			accumulated += arch_timer_read_counter() -
						g_adspsleepmon.lpm_stats->last_entered_at;

		seq_puts(s, "\nLPM stats:\n\n");
		seq_printf(s, "Count = %u\n", g_adspsleepmon.lpm_stats->count);
		seq_printf(s, "Last Entered At = %llu\n",
			g_adspsleepmon.lpm_stats->last_entered_at);
		seq_printf(s, "Last Exited At = %llu\n",
			g_adspsleepmon.lpm_stats->last_exited_at);
		seq_printf(s, "Accumulated Duration = %llu\n", accumulated);
	}

	if (g_adspsleepmon.lpi_stats) {
		accumulated = g_adspsleepmon.lpi_stats->accumulated;

		if (g_adspsleepmon.lpi_stats->last_entered_at >
					g_adspsleepmon.lpi_stats->last_exited_at)
			accumulated += arch_timer_read_counter() -
					g_adspsleepmon.lpi_stats->last_entered_at;

		seq_puts(s, "\nLPI stats:\n\n");
		seq_printf(s, "Count = %u\n", g_adspsleepmon.lpi_stats->count);
		seq_printf(s, "Last Entered At = %llu\n",
			g_adspsleepmon.lpi_stats->last_entered_at);
		seq_printf(s, "Last Exited At = %llu\n",
			g_adspsleepmon.lpi_stats->last_exited_at);
		seq_printf(s, "Accumulated Duration = %llu\n",
			accumulated);
	}

	seq_printf(s, "\n\nLPM stats pointer: 0x%llX, stored pointer: 0x%llX\n",
						qcom_smem_get(
						ADSPSLEEPMON_SMEM_ADSP_PID,
						ADSPSLEEPMON_SLEEPSTATS_ADSP_SMEM_ID,
						NULL), g_adspsleepmon.lpm_stats);

	seq_printf(s, "LPI stats pointer: 0x%llX, stored pointer: 0x%llX\n",
						qcom_smem_get(
						ADSPSLEEPMON_SMEM_ADSP_PID,
						ADSPSLEEPMON_SLEEPSTATS_ADSP_LPI_SMEM_ID,
						NULL), g_adspsleepmon.lpi_stats);

	seq_printf(s, "DSPPM stats pointer: 0x%llX, stored pointer: 0x%llX\n",
						qcom_smem_get(
						ADSPSLEEPMON_SMEM_ADSP_PID,
						ADSPSLEEPMON_DSPPMSTATS_SMEM_ID,
						NULL), g_adspsleepmon.dsppm_stats);

	seq_printf(s, "Sysmon stats pointer: 0x%llX, stored pointer: 0x%llX\n",
						qcom_smem_get(
						ADSPSLEEPMON_SMEM_ADSP_PID,
						ADSPSLEEPMON_SYSMONSTATS_SMEM_ID,
						NULL), g_adspsleepmon.sysmon_event_stats);
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(master_stats);

static void update_sysmon_event_stats_ptr(void *stats, size_t size)
{
	u32 feature_size, feature_id, version;

	g_adspsleepmon.sysmon_event_stats = NULL;
	feature_id = *(u32 *)stats;
	version = feature_id & 0xFFFF;
	feature_size = (feature_id >> 16) & 0xFFF;
	feature_id = feature_id >> 28;

	while (size >= feature_size) {
		switch (feature_id) {
		case ADSPSLEEPMON_SYSMONSTATS_EVENTS_FEATURE_ID:
			g_adspsleepmon.sysmon_event_stats =
					(struct sysmon_event_stats *)(stats + sizeof(u32));
			size = 0;
		break;

		default:
			/*
			 * Unrecognized, feature, jump through to the next
			 */
			stats = stats + feature_size;
			if (size >= feature_size)
				size = size - feature_size;
			feature_id = *(u32 *)stats;
			feature_size = (feature_id >> 16) & 0xFFF;
			feature_id = feature_id >> 28;
			version = feature_id & 0xFFFF;
		break;
		}
	}
}

static int adspsleepmon_suspend_notify(struct notifier_block *nb,
							unsigned long mode, void *_unused)
{
	switch (mode) {
	case PM_POST_SUSPEND:
	{
		/*
		 * Resume notification (previously in suspend)
		 * TODO
		 * Not acquiring mutex here, see if it is needed!
		 */
		pr_info("PM_POST_SUSPEND\n");
		if (!g_adspsleepmon.audio_stats.num_sessions ||
			(g_adspsleepmon.audio_stats.num_sessions ==
				g_adspsleepmon.audio_stats.num_lpi_sessions)) {
			g_adspsleepmon.suspend_event = true;
			wake_up_interruptible(&adspsleepmon_wq);
		}
		break;
	}
	default:
		/*
		 * Not handling other PM states, just return
		 */
		break;
	}

	return 0;
}

static struct notifier_block adsp_sleepmon_pm_nb = {
	.notifier_call = adspsleepmon_suspend_notify,
};

static int adspsleepmon_smem_init(void)
{
	size_t size;
	void *stats = NULL;

	g_adspsleepmon.lpm_stats = qcom_smem_get(
						ADSPSLEEPMON_SMEM_ADSP_PID,
						ADSPSLEEPMON_SLEEPSTATS_ADSP_SMEM_ID,
						NULL);

	if (IS_ERR_OR_NULL(g_adspsleepmon.lpm_stats)) {
		pr_err("Failed to get sleep stats from SMEM for ADSP: %d\n",
				PTR_ERR(g_adspsleepmon.lpm_stats));
		return -ENOMEM;
	}

	g_adspsleepmon.lpi_stats = qcom_smem_get(
						ADSPSLEEPMON_SMEM_ADSP_PID,
						ADSPSLEEPMON_SLEEPSTATS_ADSP_LPI_SMEM_ID,
						NULL);

	if (IS_ERR_OR_NULL(g_adspsleepmon.lpi_stats)) {
		pr_err("Failed to get LPI sleep stats from SMEM for ADSP: %d\n",
				PTR_ERR(g_adspsleepmon.lpi_stats));
		return -ENOMEM;
	}

	g_adspsleepmon.dsppm_stats = qcom_smem_get(
						   ADSPSLEEPMON_SMEM_ADSP_PID,
						ADSPSLEEPMON_DSPPMSTATS_SMEM_ID,
						NULL);

	if (IS_ERR_OR_NULL(g_adspsleepmon.dsppm_stats)) {
		pr_err("Failed to get DSPPM stats from SMEM for ADSP: %d\n",
				PTR_ERR(g_adspsleepmon.dsppm_stats));
		return -ENOMEM;
	}

	stats = qcom_smem_get(ADSPSLEEPMON_SMEM_ADSP_PID,
						ADSPSLEEPMON_SYSMONSTATS_SMEM_ID,
						&size);

	if (IS_ERR_OR_NULL(stats) || !size) {
		pr_err("Failed to get SysMon stats from SMEM for ADSP: %d, size: %d\n",
				PTR_ERR(stats), size);
		return -ENOMEM;
	}

	update_sysmon_event_stats_ptr(stats, size);

	if (IS_ERR_OR_NULL(g_adspsleepmon.sysmon_event_stats)) {
		pr_err("Failed to get SysMon event stats from SMEM for ADSP\n");
		return -ENOMEM;
	}

	/*
	 * Register for Resume notifications
	 */
	register_pm_notifier(&adsp_sleepmon_pm_nb);

	g_adspsleepmon.smem_init_done = true;

	return 0;
}

static void adspsleepmon_timer_cb(struct timer_list *unused)
{
	/*
	 * Configured timer has fired, wakeup the kernel thread
	 */
	g_adspsleepmon.timer_event = true;
	wake_up_interruptible(&adspsleepmon_wq);
}

static int adspsleepmon_driver_probe(struct platform_device *pdev)
{
	int result = 0;
	struct device *dev = &pdev->dev;

	g_adspsleepmon.b_config_panic_lpm = of_property_read_bool(dev->of_node,
			"qcom,enable_panic_lpm");

	g_adspsleepmon.b_config_panic_lpi = of_property_read_bool(dev->of_node,
			"qcom,enable_panic_lpi");

	of_property_read_u32(dev->of_node, "qcom,wait_time_lpm",
						 &g_adspsleepmon.lpm_wait_time);

	of_property_read_u32(dev->of_node, "qcom,wait_time_lpi",
						 &g_adspsleepmon.lpi_wait_time);

	g_adspsleepmon.b_panic_lpm = g_adspsleepmon.b_config_panic_lpm;
	g_adspsleepmon.b_panic_lpi = g_adspsleepmon.b_config_panic_lpi;

	if (g_adspsleepmon.b_config_panic_lpm ||
			g_adspsleepmon.b_config_panic_lpi) {
		g_adspsleepmon.debugfs_panic_file =
				debugfs_create_file("panic-state",
				 0644, g_adspsleepmon.debugfs_dir, NULL, &panic_state_fops);

		if (!g_adspsleepmon.debugfs_panic_file)
			pr_err("Unable to create file in debugfs\n");
	}

	g_adspsleepmon.debugfs_read_panic_state =
			debugfs_create_file("read_panic_state",
			 0444, g_adspsleepmon.debugfs_dir, NULL, &read_panic_state_fops);

	if (!g_adspsleepmon.debugfs_read_panic_state)
		pr_err("Unable to create read panic state file in debugfs\n");

	dev_dbg(dev, "ADSP sleep monitor probe called\n");

	return result;
}

static int current_audio_pid(struct dsppm_stats *curr_dsppm_stats)
{
	int i;
	int curr_pid_audio = 0, audio_pid_active = 0;

	for (i = 0; i < ADSPSLEEPMON_DSPPMSTATS_NUMPD; i++) {

		if (curr_dsppm_stats->pd[i].pid &&
		(curr_dsppm_stats->pd[i].num_active > 0)) {

			if ((curr_dsppm_stats->pd[i].pid &
				ADSPSLEEPMON_DSPPMSTATS_PID_FILTER) ==
				ADSPSLEEPMON_DSPPMSTATS_AUDIO_PID) {
				curr_pid_audio = 1;
				audio_pid_active = 1;
			} else {
				curr_pid_audio = 0;
			}

			pr_err("ADSP PID: %d (isAudio? %d), active clients: %d\n",
				curr_dsppm_stats->pd[i].pid,
				curr_pid_audio,
				curr_dsppm_stats->pd[i].num_active);
		}
	}

	return curr_pid_audio;
}

static int adspsleepmon_worker(void *data)
{
	int result = 0, audio_pid_active = 0, curr_pid_audio, num_active = 0;
	int i;
	struct sleep_stats curr_lpm_stats, curr_lpi_stats;
	struct dsppm_stats curr_dsppm_stats;
	struct sysmon_event_stats sysmon_event_stats;
	u64 curr_timestamp, elapsed_time;

	while (!kthread_should_stop()) {
		result = wait_event_interruptible(adspsleepmon_wq,
					(kthread_should_stop() ||
					g_adspsleepmon.timer_event ||
					g_adspsleepmon.suspend_event));

		if (kthread_should_stop())
			break;

		if (result)
			continue;


		pr_info("timer_event = %d,suspend_event = %d\n",
				g_adspsleepmon.timer_event,
				g_adspsleepmon.suspend_event);

		/*
		 * Handle timer event.
		 * 2 cases:
		 * - There is no active use case on ADSP, we
		 *   are expecting it to be in power collapse
		 * - There is an active LPI use case on ADSP,
		 *   we are expecting it to be in LPI.
		 *
		 * Critical section start
		 */
		mutex_lock(&g_adspsleepmon.lock);

		if (!g_adspsleepmon.audio_stats.num_sessions) {

			curr_timestamp = __arch_counter_get_cntvct();

			if (curr_timestamp >= g_adspsleepmon.backup_lpm_timestamp)
				elapsed_time = (curr_timestamp -
					 g_adspsleepmon.backup_lpm_timestamp);
			else
				elapsed_time = U64_MAX -
					g_adspsleepmon.backup_lpm_timestamp +
					curr_timestamp;

			if (!g_adspsleepmon.timer_event && g_adspsleepmon.suspend_event) {
				/*
				 * Check if we have elapsed enough duration
				 * to make a decision if it is not timer
				 * event
				 */

				if (elapsed_time <
					(g_adspsleepmon.lpm_wait_time *
					ADSPSLEEPMON_SYS_CLK_TICKS_PER_SEC)) {
					mutex_unlock(&g_adspsleepmon.lock);
					g_adspsleepmon.suspend_event = false;
					continue;
				}
			}

			/*
			 * Read ADSP sleep statistics and
			 * see if ADSP has entered sleep.
			 */
			memcpy(&curr_lpm_stats,
					g_adspsleepmon.lpm_stats,
			sizeof(struct sleep_stats));

			/*
			 * Check if ADSP didn't power collapse post
			 * no active client.
			 */
			if ((!curr_lpm_stats.count) ||
				(curr_lpm_stats.last_exited_at >
					curr_lpm_stats.last_entered_at) ||
				((curr_lpm_stats.last_exited_at <
					curr_lpm_stats.last_entered_at) &&
				(curr_lpm_stats.last_entered_at >= curr_timestamp))) {

				memcpy(&curr_dsppm_stats,
					g_adspsleepmon.dsppm_stats,
				sizeof(struct dsppm_stats));

				memcpy(&sysmon_event_stats,
					g_adspsleepmon.sysmon_event_stats,
					sizeof(struct sysmon_event_stats));

				for (i = 0; i < ADSPSLEEPMON_DSPPMSTATS_NUMPD; i++) {
					if (curr_dsppm_stats.pd[i].pid &&
							(curr_dsppm_stats.pd[i].num_active > 0))
						num_active++;
				}

				if ((curr_lpm_stats.accumulated ==
					g_adspsleepmon.backup_lpm_stats.accumulated) ||
					num_active) {

					pr_err("Detected ADSP sleep issue:\n");
					pr_err("ADSP clock: %u, sleep latency: %u\n",
							sysmon_event_stats.core_clk,
							sysmon_event_stats.sleep_latency);
					pr_err("Monitored duration (msec):%u,Sleep duration(msec): %u\n",
						(elapsed_time /
						ADSPSLEEPMON_SYS_CLK_TICKS_PER_MILLISEC),
						((curr_lpm_stats.accumulated -
						g_adspsleepmon.backup_lpm_stats.accumulated) /
						ADSPSLEEPMON_SYS_CLK_TICKS_PER_MILLISEC));

					curr_pid_audio = current_audio_pid(&curr_dsppm_stats);
					audio_pid_active = curr_pid_audio;

					if (g_adspsleepmon.b_panic_lpm &&
						audio_pid_active &&
						(curr_lpm_stats.accumulated ==
						g_adspsleepmon.backup_lpm_stats.accumulated))
						panic("ADSP sleep issue detected");

				}
			}

			memcpy(&g_adspsleepmon.backup_lpm_stats,
			&curr_lpm_stats,
			sizeof(struct sleep_stats));

			g_adspsleepmon.backup_lpm_timestamp = __arch_counter_get_cntvct();

		} else if (g_adspsleepmon.audio_stats.num_sessions ==
					g_adspsleepmon.audio_stats.num_lpi_sessions) {

			curr_timestamp = __arch_counter_get_cntvct();

			if (!g_adspsleepmon.timer_event && g_adspsleepmon.suspend_event) {
				/*
				 * Check if we have elapsed enough duration
				 * to make a decision if it is not timer
				 * event
				 */
				if (curr_timestamp >=
						g_adspsleepmon.backup_lpi_timestamp)
					elapsed_time = (curr_timestamp -
							g_adspsleepmon.backup_lpi_timestamp);
				else
					elapsed_time = U64_MAX -
							g_adspsleepmon.backup_lpi_timestamp +
							curr_timestamp;

				if (elapsed_time <
					(g_adspsleepmon.lpi_wait_time *
					ADSPSLEEPMON_SYS_CLK_TICKS_PER_SEC)) {
					mutex_unlock(&g_adspsleepmon.lock);
					g_adspsleepmon.suspend_event = false;
					continue;
				}
			}

			/*
			 * Read ADSP LPI statistics and see
			 * if ADSP is in LPI state.
			 */
			memcpy(&curr_lpi_stats,
					g_adspsleepmon.lpi_stats,
					sizeof(struct sleep_stats));

			/*
			 * Check if ADSP is not in LPI
			 */
			if ((!curr_lpi_stats.count) ||
				(curr_lpi_stats.last_exited_at >
						curr_lpi_stats.last_entered_at) ||
				((curr_lpi_stats.last_exited_at <
						curr_lpi_stats.last_entered_at) &&
				 (curr_lpi_stats.last_entered_at >= curr_timestamp))) {

				memcpy(&curr_dsppm_stats,
					g_adspsleepmon.dsppm_stats,
					sizeof(struct dsppm_stats));

				memcpy(&sysmon_event_stats,
					g_adspsleepmon.sysmon_event_stats,
					sizeof(struct sysmon_event_stats));

				for (i = 0; i < ADSPSLEEPMON_DSPPMSTATS_NUMPD; i++) {
					if (curr_dsppm_stats.pd[i].pid &&
							 (curr_dsppm_stats.pd[i].num_active > 0))
						num_active++;
				}

				if (curr_lpi_stats.accumulated ==
					  g_adspsleepmon.backup_lpi_stats.accumulated) {

					pr_err("Detected ADSP LPI issue:\n");
					pr_err("ADSP clock: %u, sleep latency: %u\n",
							sysmon_event_stats.core_clk,
							sysmon_event_stats.sleep_latency);

					curr_pid_audio = current_audio_pid(&curr_dsppm_stats);
					audio_pid_active = curr_pid_audio;

					if (g_adspsleepmon.b_panic_lpi && audio_pid_active)
						panic("ADSP LPI issue detected");

				}
			}

			memcpy(&g_adspsleepmon.backup_lpi_stats,
			&curr_lpi_stats,
			sizeof(struct sleep_stats));

			g_adspsleepmon.backup_lpi_timestamp = __arch_counter_get_cntvct();

		}

		if (g_adspsleepmon.timer_event) {
			g_adspsleepmon.timer_event = false;
			g_adspsleepmon.timer_pending = false;
		}

		g_adspsleepmon.suspend_event = false;

		mutex_unlock(&g_adspsleepmon.lock);
	}

	do_exit(0);
}

static int adspsleepmon_device_open(struct inode *inode, struct file *fp)
{
	struct adspsleepmon_file *fl = NULL;

	/*
	 * Check if SMEM side needs initialization
	 */
	if (!g_adspsleepmon.smem_init_done)
		adspsleepmon_smem_init();

	if (!g_adspsleepmon.smem_init_done)
		return -ENODEV;

	/*
	 * Check for device minor and return error if not matching
	 * May need to allocate (kzalloc) based on requirement and update
	 * fp->private_data.
	 */

	fl = kzalloc(sizeof(*fl), GFP_KERNEL);
	if (IS_ERR_OR_NULL(fl))
		return -ENOMEM;

	INIT_HLIST_NODE(&fl->hn);
	fl->num_sessions = 0;
	fl->num_lpi_sessions = 0;
	fl->b_connected = 0;
	spin_lock_init(&fl->hlock);
	fp->private_data = fl;
	return 0;
}

static int adspsleepmon_device_release(struct inode *inode, struct file *fp)
{
	struct adspsleepmon_file *fl = (struct adspsleepmon_file *)fp->private_data;
	u32 num_sessions = 0, num_lpi_sessions = 0, delay = 0;
	struct adspsleepmon_file *client = NULL;
	struct hlist_node *n;

	/*
	 * do tear down
	 */
	if (fl) {
		/*
		 * Critical section start
		 */
		mutex_lock(&g_adspsleepmon.lock);
		spin_lock(&fl->hlock);
		hlist_del_init(&fl->hn);

		/*
		 * Reaggregate num sessions
		 */
		hlist_for_each_entry_safe(client, n,
					&g_adspsleepmon.audio_clients, hn) {
			if (client->b_connected) {
				num_sessions += client->num_sessions;
				num_lpi_sessions += client->num_lpi_sessions;
			}
		}

		spin_unlock(&fl->hlock);
		kfree(fl);

		/*
		 * Start/stop the timer based on
		 *   Start -> No active session (from previous
		 *				   active session)
		 *   Stop -> An active session
		 */
		if (num_sessions != g_adspsleepmon.audio_stats.num_sessions) {
			if (!num_sessions ||
					(num_sessions == num_lpi_sessions)) {

				if (!num_sessions) {
					memcpy(&g_adspsleepmon.backup_lpm_stats,
							g_adspsleepmon.lpm_stats,
							sizeof(struct sleep_stats));
					g_adspsleepmon.backup_lpm_timestamp =
						__arch_counter_get_cntvct();
					delay = g_adspsleepmon.lpm_wait_time;
				} else {
					memcpy(&g_adspsleepmon.backup_lpi_stats,
							g_adspsleepmon.lpi_stats,
							sizeof(struct sleep_stats));
					g_adspsleepmon.backup_lpi_timestamp =
						__arch_counter_get_cntvct();
					delay = g_adspsleepmon.lpi_wait_time;
				}

				mod_timer(&adspsleep_timer, jiffies + delay * HZ);
				g_adspsleepmon.timer_pending = true;
			} else if (g_adspsleepmon.timer_pending) {
				del_timer(&adspsleep_timer);
				g_adspsleepmon.timer_pending = false;
			}

			g_adspsleepmon.audio_stats.num_sessions = num_sessions;
			g_adspsleepmon.audio_stats.num_lpi_sessions = num_lpi_sessions;
		}

		mutex_unlock(&g_adspsleepmon.lock);

		pr_info("Release: num_sessions=%d,num_lpi_sessions=%d,timer_pending=%d\n",
						g_adspsleepmon.audio_stats.num_sessions,
						g_adspsleepmon.audio_stats.num_lpi_sessions,
						g_adspsleepmon.timer_pending);
		/*
		 * Critical section Done
		 */
	} else {
		return -ENODATA;
	}

	return 0;
}

static long adspsleepmon_device_ioctl(struct file *file,
								unsigned int ioctl_num,
								unsigned long ioctl_param)
{
	int ret = 0;

	struct adspsleepmon_file *fl = (struct adspsleepmon_file *)file->private_data;

	switch (ioctl_num) {

	case ADSPSLEEPMON_IOCTL_CONFIGURE_PANIC:
	{
		struct adspsleepmon_ioctl_panic panic_param;

		if (copy_from_user(&panic_param, (void const __user *)ioctl_param,
					sizeof(struct adspsleepmon_ioctl_panic))) {
			ret = -ENOTTY;
			pr_err("IOCTL copy from user failed\n");
			goto bail;
		}

		if (panic_param.version <
			ADSPSLEEPMON_IOCTL_CONFIG_PANIC_VER_1) {
			ret = -ENOTTY;
			pr_err("Bad version (%d) in IOCTL (%d)\n",
					panic_param.version, ioctl_num);
			goto bail;
		}

		if (panic_param.command >= ADSPSLEEPMON_RESET_PANIC_MAX) {
			ret = -EINVAL;
			pr_err("Invalid command (%d) passed in IOCTL (%d)\n",
				   panic_param.command, ioctl_num);
			goto bail;
		}

		switch (panic_param.command) {
		case ADSPSLEEPMON_DISABLE_PANIC_LPM:
			g_adspsleepmon.b_panic_lpm = false;
		break;

		case ADSPSLEEPMON_DISABLE_PANIC_LPI:
			g_adspsleepmon.b_panic_lpi = false;
		break;

		case ADSPSLEEPMON_RESET_PANIC_LPM:
			g_adspsleepmon.b_panic_lpm =
						g_adspsleepmon.b_config_panic_lpm;
		break;

		case ADSPSLEEPMON_RESET_PANIC_LPI:
			g_adspsleepmon.b_panic_lpi =
						g_adspsleepmon.b_config_panic_lpi;
		break;
		}
	}
	break;

	case ADSPSLEEPMON_IOCTL_AUDIO_ACTIVITY:
	{

		struct adspsleepmon_ioctl_audio audio_param;
		u32 num_sessions = 0, num_lpi_sessions = 0, delay = 0;
		struct adspsleepmon_file *client = NULL;
		struct hlist_node *n;

		if (copy_from_user(&audio_param, (void const __user *)ioctl_param,
					sizeof(struct adspsleepmon_ioctl_audio))) {
			ret = -ENOTTY;
			pr_err("IOCTL copy from user failed\n");
			goto bail;
		}
		if (!fl) {
			pr_err("bad pointer to private data in ioctl\n");
			ret = -ENOMEM;

			goto bail;
		}

		if (fl->b_connected &&
				(fl->b_connected != ADSPSLEEPMON_AUDIO_CLIENT)) {
			pr_err("Restricted IOCTL (%d) called from %d client\n",
					ioctl_num, fl->b_connected);
			ret = -ENOMSG;

			goto bail;
		}

		/*
		 * Check version
		 */
		if (audio_param.version <
					ADSPSLEEPMON_IOCTL_AUDIO_VER_1) {
			ret = -ENOTTY;
			pr_err("Bad version (%d) in IOCTL (%d)\n",
					audio_param.version, ioctl_num);
			goto bail;
		}

		if (audio_param.command >= ADSPSLEEPMON_AUDIO_ACTIVITY_MAX) {
			ret = -EINVAL;
			pr_err("Invalid command (%d) passed in IOCTL (%d)\n",
				   audio_param.command, ioctl_num);
			goto bail;
		}

		/*
		 * Critical section start
		 */
		mutex_lock(&g_adspsleepmon.lock);

		if (!fl->b_connected) {
			hlist_add_head(&fl->hn, &g_adspsleepmon.audio_clients);
			fl->b_connected = ADSPSLEEPMON_AUDIO_CLIENT;
		}

		switch (audio_param.command) {
		case ADSPSLEEPMON_AUDIO_ACTIVITY_LPI_START:
			fl->num_lpi_sessions++;
		case ADSPSLEEPMON_AUDIO_ACTIVITY_START:
			fl->num_sessions++;
		break;

		case ADSPSLEEPMON_AUDIO_ACTIVITY_LPI_STOP:
			if (fl->num_lpi_sessions)
				fl->num_lpi_sessions--;
			else
				pr_info("Received AUDIO LPI activity stop when none active!\n");
		case ADSPSLEEPMON_AUDIO_ACTIVITY_STOP:
			if (fl->num_sessions)
				fl->num_sessions--;
			else
				pr_info("Received AUDIO activity stop when none active!\n");
		break;

		case ADSPSLEEPMON_AUDIO_ACTIVITY_RESET:
			fl->num_sessions = 0;
			fl->num_lpi_sessions = 0;
		break;
		}

		/*
		 * Iterate over the registered audio IOCTL clients and
		 * calculate total number of active sessions and LPI sessions
		 */
		spin_lock(&fl->hlock);

		hlist_for_each_entry_safe(client, n,
					&g_adspsleepmon.audio_clients, hn) {
			if (client->b_connected) {
				num_sessions += client->num_sessions;
				num_lpi_sessions += client->num_lpi_sessions;
			}
		}

		spin_unlock(&fl->hlock);

		/*
		 * Start/stop the timer based on
		 *   Start -> No active session (from previous
		 *				   active session)
		 *   Stop -> An active session
		 */
		if (!num_sessions ||
				(num_sessions == num_lpi_sessions)) {

			if (!num_sessions) {
				memcpy(&g_adspsleepmon.backup_lpm_stats,
						g_adspsleepmon.lpm_stats,
						sizeof(struct sleep_stats));
				g_adspsleepmon.backup_lpm_timestamp =
						__arch_counter_get_cntvct();
				delay = g_adspsleepmon.lpm_wait_time;
			} else {
				memcpy(&g_adspsleepmon.backup_lpi_stats,
						g_adspsleepmon.lpi_stats,
						sizeof(struct sleep_stats));
				g_adspsleepmon.backup_lpi_timestamp =
						__arch_counter_get_cntvct();
				delay = g_adspsleepmon.lpi_wait_time;
			}

			mod_timer(&adspsleep_timer, jiffies + delay * HZ);
			g_adspsleepmon.timer_pending = true;
		} else if (g_adspsleepmon.timer_pending) {
			del_timer(&adspsleep_timer);
			g_adspsleepmon.timer_pending = false;
		}

		g_adspsleepmon.audio_stats.num_sessions = num_sessions;
		g_adspsleepmon.audio_stats.num_lpi_sessions = num_lpi_sessions;

		pr_info("Audio: num_sessions=%d,num_lpi_sessions=%d,timer_pending=%d\n",
						g_adspsleepmon.audio_stats.num_sessions,
						g_adspsleepmon.audio_stats.num_lpi_sessions,
						g_adspsleepmon.timer_pending);

		mutex_unlock(&g_adspsleepmon.lock);
		/*
		 * Critical section end
		 */
	}
		break;

	default:
		ret = -ENOTTY;
		pr_info("Unidentified ioctl %d!\n", ioctl_num);
		break;
	}
bail:
	return ret;
}

static const struct file_operations fops = {
	.open = adspsleepmon_device_open,
	.release = adspsleepmon_device_release,
	.unlocked_ioctl = adspsleepmon_device_ioctl,
	.compat_ioctl = adspsleepmon_device_ioctl,
};

static const struct of_device_id adspsleepmon_match_table[] = {
	{ .compatible = "qcom,adsp-sleepmon" },
	{ },
};

static struct platform_driver adspsleepmon = {
	.probe = adspsleepmon_driver_probe,
	.driver = {
		.name = "adsp_sleepmon",
		.of_match_table = adspsleepmon_match_table,
	},
};

static int __init adspsleepmon_init(void)
{
	int ret = 0;
	struct adspsleepmon *me = &g_adspsleepmon;

	mutex_init(&g_adspsleepmon.lock);
	/*
	 * Initialize dtsi config
	 */
	g_adspsleepmon.lpm_wait_time = ADSPSLEEPMON_LPM_WAIT_TIME;
	g_adspsleepmon.lpi_wait_time = ADSPSLEEPMON_LPI_WAIT_TIME;
	g_adspsleepmon.b_config_panic_lpm = false;
	g_adspsleepmon.b_config_panic_lpi = false;

	g_adspsleepmon.worker_task = kthread_run(adspsleepmon_worker,
					NULL, "adspsleepmon-worker");

	if (!g_adspsleepmon.worker_task) {
		pr_err("Failed to create kernel thread\n");
		return -ENOMEM;
	}

	INIT_HLIST_HEAD(&g_adspsleepmon.audio_clients);
	ret = alloc_chrdev_region(&me->devno, 0, 1, ADSPSLEEPMON_DEVICE_NAME_LOCAL);

	if (ret != 0) {
		pr_err("Failed to allocate char device region\n");
		goto bail;
	}

	cdev_init(&me->cdev, &fops);
	me->cdev.owner = THIS_MODULE;
	ret = cdev_add(&me->cdev, MKDEV(MAJOR(me->devno), 0), 1);

	if (ret != 0) {
		pr_err("Failed to add cdev\n");
		goto cdev_bail;
	}

	me->class = class_create(THIS_MODULE, "adspsleepmon");

	if (IS_ERR(me->class)) {
		pr_err("Failed to create a class\n");
		goto class_bail;
	}

	me->dev = device_create(me->class, NULL,
			MKDEV(MAJOR(me->devno), 0), NULL, ADSPSLEEPMON_DEVICE_NAME_LOCAL);

	if (IS_ERR_OR_NULL(me->dev)) {
		pr_err("Failed to create a device\n");
		goto device_bail;
	}

	g_adspsleepmon.debugfs_dir = debugfs_create_dir("adspsleepmon", NULL);

	if (!g_adspsleepmon.debugfs_dir) {
		pr_err("Failed to create debugfs directory for adspsleepmon\n");
		goto debugfs_bail;
	}

	ret = platform_driver_register(&adspsleepmon);

	if (ret) {
		pr_err("Platform driver registration failed for adsp-sleepmon: %d\n", ret);
		goto debugfs_bail;
	}

	g_adspsleepmon.debugfs_master_stats =
			debugfs_create_file("master_stats",
			 0444, g_adspsleepmon.debugfs_dir, NULL, &master_stats_fops);

	if (!g_adspsleepmon.debugfs_master_stats)
		pr_err("Failed to create debugfs file for master stats\n");

	return 0;

debugfs_bail:
	device_destroy(g_adspsleepmon.class, g_adspsleepmon.cdev.dev);
device_bail:
	class_destroy(me->class);
class_bail:
	cdev_del(&me->cdev);
cdev_bail:
	unregister_chrdev_region(me->devno, 1);
bail:
	platform_driver_unregister(&adspsleepmon);
	return ret;
}

static void __exit adspsleepmon_exit(void)
{
	device_destroy(g_adspsleepmon.class, g_adspsleepmon.cdev.dev);
	class_destroy(g_adspsleepmon.class);
	cdev_del(&g_adspsleepmon.cdev);
	unregister_chrdev_region(g_adspsleepmon.devno, 1);
	platform_driver_unregister(&adspsleepmon);
	debugfs_remove_recursive(g_adspsleepmon.debugfs_dir);
	unregister_pm_notifier(&adsp_sleepmon_pm_nb);
}

module_init(adspsleepmon_init);
module_exit(adspsleepmon_exit);

MODULE_LICENSE("GPL v2");
