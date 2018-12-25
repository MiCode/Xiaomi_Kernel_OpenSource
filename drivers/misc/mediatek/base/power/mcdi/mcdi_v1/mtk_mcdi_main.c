/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/cpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_qos.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tick.h>
#include <linux/uaccess.h>

#include <mtk_cpuidle.h>
#include <mtk_idle.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_profile.h>
#include <mtk_mcdi_util.h>

#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_api.h>

#include <mtk_mcdi_governor_hint.h>

#include <trace/events/mtk_idle_event.h>
#include <linux/irqchip/mtk-gic-extend.h>
/* #define USING_TICK_BROADCAST */

#define MCDI_DEBUG_INFO_MAGIC_NUM           0x1eef9487
#define MCDI_DEBUG_INFO_NON_REPLACE_OFFSET  0x0008

static unsigned long mcdi_cnt_wfi[NF_CPU];
static unsigned long mcdi_cnt_cpu[NF_CPU];
static unsigned long mcdi_cnt_cluster[NF_CLUSTER];

void __iomem *mcdi_sysram_base;
#define MCDI_SYSRAM (mcdi_sysram_base + MCDI_DEBUG_INFO_NON_REPLACE_OFFSET)

static unsigned long mcdi_cnt_cpu_last[NF_CPU];
static unsigned long mcdi_cnt_cluster_last[NF_CLUSTER];

static unsigned long ac_cpu_cond_info_last[NF_ANY_CORE_CPU_COND_INFO];

static const char *ac_cpu_cond_name[NF_ANY_CORE_CPU_COND_INFO] = {
	"pause",
	"multi core",
	"residency",
	"last core"
};

static unsigned long long mcdi_heart_beat_log_prev;
static DEFINE_SPINLOCK(mcdi_heart_beat_spin_lock);

static unsigned int mcdi_heart_beat_log_dump_thd = 5000;          /* 5 sec */

int __attribute__((weak)) mtk_enter_idle_state(int mode)
{
	return 0;
}

int __attribute__((weak)) soidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) dpidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) soidle3_enter(int cpu)
{
	return 1;
}

unsigned long long __attribute__((weak)) idle_get_current_time_ms(void)
{
	return 0;
}

void __attribute__((weak)) aee_rr_rec_mcdi_val(int id, u32 val)
{
}

void __attribute__((weak)) mtk_idle_dump_cnt_in_interval(void)
{
}

void wakeup_all_cpu(void)
{
	int cpu = 0;

	for (cpu = 0; cpu < NF_CPU; cpu++) {
		if (cpu_online(cpu))
			smp_send_reschedule(cpu);
	}
}

void wait_until_all_cpu_powered_on(void)
{
	while (!(mcdi_get_gov_data_num_mcusys() == 0x0))
		;
}

void mcdi_wakeup_all_cpu(void)
{
	wakeup_all_cpu();

	wait_until_all_cpu_powered_on();
}

/* debugfs */
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

/* mcdi_state */
static int _mcdi_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int mcdi_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _mcdi_state_open, inode->i_private);
}

static ssize_t mcdi_state_read(struct file *filp,
		char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	int i;
	char *p = dbg_buf;
	unsigned long ac_cpu_cond_info[NF_ANY_CORE_CPU_COND_INFO];
	int latency_req = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);

	struct mcdi_feature_status feature_stat;

	get_mcdi_feature_status(&feature_stat);

	mcdi_log("Feature:\n");
	mcdi_log("\tenable = %d\n", feature_stat.enable);
	mcdi_log("\tpause = %d\n", feature_stat.pause);
	mcdi_log("\tmax s_state = %d\n", feature_stat.s_state);
	mcdi_log("\tcluster_off = %d\n", feature_stat.cluster_off);
	mcdi_log("\tbuck_off = %d\n", feature_stat.buck_off);
	mcdi_log("\tany_core = %d\n", feature_stat.any_core);

	mcdi_log("\n");

	mcdi_log("mcdi_cnt_wfi: ");
	for (i = 0; i < NF_CPU; i++)
		mcdi_log("%lu ", mcdi_cnt_wfi[i]);
	mcdi_log("\n");

	mcdi_log("mcdi_cnt_cpu: ");
	for (i = 0; i < NF_CPU; i++)
		mcdi_log("%lu ", mcdi_cnt_cpu[i]);
	mcdi_log("\n");

	mcdi_log("mcdi_cnt_cluster: ");
	for (i = 0; i < NF_CLUSTER; i++) {
		mcdi_cnt_cluster[i] =
				mcdi_mbox_read(MCDI_MBOX_CLUSTER_0_CNT + i);
		mcdi_log("%lu ", mcdi_cnt_cluster[i]);
	}
	mcdi_log("\n");

	any_core_cpu_cond_get(ac_cpu_cond_info);

	for (i = 0; i < NF_ANY_CORE_CPU_COND_INFO; i++) {
		mcdi_log("%s = %lu\n",
			ac_cpu_cond_name[i],
			ac_cpu_cond_info[i]
		);
	}

	mcdi_log("pm_qos latency_req = %d\n", latency_req);

	mcdi_log("system_idle_hint = %08x\n", system_idle_hint_result_raw());

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t mcdi_state_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");

	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");

	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);

	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "enable", sizeof("enable"))) {
		set_mcdi_enable_status(param != 0);
		return count;
	} else if (!strncmp(cmd_str, "s_state", sizeof("s_state"))) {
		set_mcdi_s_state(param);
		return count;
	} else if (!strncmp(cmd_str, "buck_off", sizeof("buck_off"))) {
		set_mcdi_buck_off_mask(param);
		return count;
	} else if (!strncmp(cmd_str, "hint", sizeof("hint"))) {
		_system_idle_hint_request(SYSTEM_IDLE_HINT_USER_MCDI_TEST,
								param != 0);
		return count;
	} else {
		return -EINVAL;
	}
}

static const struct file_operations mcdi_state_fops = {
	.open = mcdi_state_open,
	.read = mcdi_state_read,
	.write = mcdi_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* procfs entry */
static const char mcdi_procfs_dir_name[] = "mcdi";
struct proc_dir_entry *mcdi_dir;
static int mcdi_procfs_init(void)
{
	mcdi_dir = proc_mkdir(mcdi_procfs_dir_name, NULL);

	if (!mcdi_dir) {
		pr_notice("fail to create /proc/mcdi @ %s()\n", __func__);
		return -ENOMEM;
	}

	if (!proc_create("state", 0644, mcdi_dir, &mcdi_state_fops))
		pr_notice("%s(), create /proc/mcdi/%s failed\n",
			__func__, "state");

	mcdi_procfs_profile_init(mcdi_dir);

	return 0;
}

static void __go_to_wfi(int cpu)
{
	remove_cpu_from_prefer_schedule_domain(cpu);
	trace_rgidle_rcuidle(cpu, 1);
	isb();
	/* memory barrier before WFI */
	mb();
	__asm__ __volatile__("wfi" : : : "memory");

	trace_rgidle_rcuidle(cpu, 0);
	add_cpu_to_prefer_schedule_domain(cpu);
}

void mcdi_cpu_off(int cpu)
{
	int state = 0;

	state = get_residency_latency_result(cpu);

	switch (state) {
	case MCDI_STATE_CPU_OFF:

#ifdef USING_TICK_BROADCAST
		tick_broadcast_enter();
#endif

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_ENTER);

		mtk_enter_idle_state(MTK_MCDI_CPU_MODE);

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_LEAVE);

#ifdef USING_TICK_BROADCAST
		tick_broadcast_exit();
#endif

		break;
	case MCDI_STATE_CLUSTER_OFF:
	case MCDI_STATE_SODI:
	case MCDI_STATE_DPIDLE:
	case MCDI_STATE_SODI3:

#ifdef USING_TICK_BROADCAST
		tick_broadcast_enter();
#endif

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_ENTER);

		mtk_enter_idle_state(MTK_MCDI_CLUSTER_MODE);

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_LEAVE);

#ifdef USING_TICK_BROADCAST
		tick_broadcast_exit();
#endif

		break;
	default:
		/* should NOT happened */
		__go_to_wfi(cpu);

		break;
	}
}

void mcdi_cluster_off(int cpu)
{
	int cluster_idx = cluster_idx_get(cpu);

	/* Notify SSPM: cluster can be OFF */
	mcdi_mbox_write(MCDI_MBOX_CLUSTER_0_CAN_POWER_OFF + cluster_idx, 1);

	mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_ENTER);

	mtk_enter_idle_state(MTK_MCDI_CLUSTER_MODE);

	mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_LEAVE);
}

void mcdi_heart_beat_log_dump(void)
{
	static struct mtk_mcdi_buf buf;
	int i;
	unsigned long long mcdi_heart_beat_log_curr = 0;
	unsigned long flags;
	bool dump_log = false;
	unsigned long mcdi_cnt;
	unsigned long any_core_info = 0;
	unsigned long ac_cpu_cond_info[NF_ANY_CORE_CPU_COND_INFO];
	unsigned int cpu_mask = 0;
	unsigned int cluster_mask = 0;
	struct mcdi_feature_status feature_stat;

	spin_lock_irqsave(&mcdi_heart_beat_spin_lock, flags);

	mcdi_heart_beat_log_curr = idle_get_current_time_ms();

	if (mcdi_heart_beat_log_prev == 0)
		mcdi_heart_beat_log_prev = mcdi_heart_beat_log_curr;

	if ((mcdi_heart_beat_log_curr - mcdi_heart_beat_log_prev)
			> mcdi_heart_beat_log_dump_thd) {
		dump_log = true;
		mcdi_heart_beat_log_prev = mcdi_heart_beat_log_curr;
	}

	spin_unlock_irqrestore(&mcdi_heart_beat_spin_lock, flags);

	if (!dump_log)
		return;

	reset_mcdi_buf(buf);

	mcdi_buf_append(buf, "mcdi cpu: ");

	for (i = 0; i < NF_CPU; i++) {
		mcdi_cnt = mcdi_cnt_cpu[i] - mcdi_cnt_cpu_last[i];
		mcdi_buf_append(buf, "%lu, ", mcdi_cnt);
		mcdi_cnt_cpu_last[i] = mcdi_cnt_cpu[i];
	}

	mcdi_buf_append(buf, "cluster : ");

	for (i = 0; i < NF_CLUSTER; i++) {
		mcdi_cnt_cluster[i] =
				mcdi_mbox_read(MCDI_MBOX_CLUSTER_0_CNT + i);

		mcdi_cnt = mcdi_cnt_cluster[i] - mcdi_cnt_cluster_last[i];
		mcdi_buf_append(buf, "%lu, ", mcdi_cnt);
		mcdi_cnt_cluster_last[i] = mcdi_cnt_cluster[i];
	}

	any_core_cpu_cond_get(ac_cpu_cond_info);

	for (i = 0; i < NF_ANY_CORE_CPU_COND_INFO; i++) {
		any_core_info =
			ac_cpu_cond_info[i] - ac_cpu_cond_info_last[i];
		mcdi_buf_append(buf, "%s = %lu, ",
			ac_cpu_cond_name[i], any_core_info);
		ac_cpu_cond_info_last[i] = ac_cpu_cond_info[i];
	}

	get_mcdi_avail_mask(&cpu_mask, &cluster_mask);

	mcdi_buf_append(buf, "avail cpu = %04x, cluster = %04x",
		cpu_mask, cluster_mask);

	get_mcdi_feature_status(&feature_stat);

	mcdi_buf_append(buf, ", enabled = %d, max_s_state = %d (buck_off = %d)",
						feature_stat.enable,
						feature_stat.s_state,
						feature_stat.buck_off);

	mcdi_buf_append(buf, ", system_idle_hint = %08x\n",
						system_idle_hint_result_raw());

	pr_info("%s\n", get_mcdi_buf(buf));
}

int wfi_enter(int cpu)
{
	idle_refcnt_inc();

	__go_to_wfi(cpu);

	idle_refcnt_dec();

	mcdi_cnt_wfi[cpu]++;

	return 0;
}

int mcdi_enter(int cpu)
{
	int cluster_idx = cluster_idx_get(cpu);
	int state = -1;
	struct cpuidle_state *mcdi_sta;

	/* Note: [DVT] Enter mtk idle state w/o mcdi enable
	 * Include mtk_idle.h for MTK_IDLE_DVT_TEST_ONLY
	 */
	#if defined(MTK_IDLE_DVT_TEST_ONLY)
	mtk_idle_enter_dvt(cpu);
	return 0;
	#endif

	idle_refcnt_inc();
	mcdi_profile_ts(MCDI_PROFILE_GOV_SEL_ENTER);

	if (likely(mcdi_fw_is_ready()))
		state = mcdi_governor_select(cpu, cluster_idx);
	else
		state = MCDI_STATE_WFI;

	mcdi_profile_ts(MCDI_PROFILE_GOV_SEL_LEAVE);

	if (state >= MCDI_STATE_WFI && state <= MCDI_STATE_CLUSTER_OFF) {
		mcdi_sta = &(mcdi_state_tbl_get(cpu)->states[state]);
		sched_idle_set_state(mcdi_sta, state);
	}

	switch (state) {
	case MCDI_STATE_WFI:
		__go_to_wfi(cpu);

		break;
	case MCDI_STATE_CPU_OFF:

		trace_mcdi_rcuidle(cpu, 1);

		aee_rr_rec_mcdi_val(cpu, 0xff);

		mcdi_cpu_off(cpu);

		aee_rr_rec_mcdi_val(cpu, 0x0);

		trace_mcdi_rcuidle(cpu, 0);

		mcdi_cnt_cpu[cpu]++;

		break;
	case MCDI_STATE_CLUSTER_OFF:

		trace_mcdi_rcuidle(cpu, 1);

		aee_rr_rec_mcdi_val(cpu, 0xff);

		mcdi_cluster_off(cpu);

		aee_rr_rec_mcdi_val(cpu, 0x0);

		trace_mcdi_rcuidle(cpu, 0);

		mcdi_cnt_cpu[cpu]++;

		break;
	case MCDI_STATE_SODI:

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_ENTER);

		soidle_enter(cpu);

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_LEAVE);

		break;
	case MCDI_STATE_DPIDLE:

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_ENTER);

		dpidle_enter(cpu);

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_LEAVE);

		break;
	case MCDI_STATE_SODI3:

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_ENTER);

		soidle3_enter(cpu);

		mcdi_profile_ts(MCDI_PROFILE_CPU_DORMANT_LEAVE);

		break;
	}

	if (state >= MCDI_STATE_WFI && state <= MCDI_STATE_CLUSTER_OFF)
		sched_idle_set_state(NULL, -1);

	mcdi_governor_reflect(cpu, state);

	idle_refcnt_dec();
	mcdi_profile_ts(MCDI_PROFILE_LEAVE);

	if (state == get_mcdi_profile_state())
		mcdi_profile_calc();

	return 0;
}

bool __mcdi_pause(unsigned int id, bool paused)
{
	struct mcdi_feature_status feature_stat;

	get_mcdi_feature_status(&feature_stat);

	if (!feature_stat.enable)
		return true;

	if (!mcdi_get_boot_time_check())
		return true;

	if (paused) {
		mcdi_state_pause(id, true);
		mcdi_wakeup_all_cpu();

	} else {
		mcdi_state_pause(id, false);
	}

	return true;
}

bool _mcdi_task_pause(bool paused)
{
	bool ret = false;

	if (!is_mcdi_working())
		return ret;

	/* TODO */
#if 1
	if (paused) {

		trace_mcdi_task_pause_rcuidle(smp_processor_id(), true);

		/* Notify SSPM to disable MCDI */
		mcdi_mbox_write(MCDI_MBOX_PAUSE_ACTION, 1);

		/* Polling until MCDI Task stopped */
		while (!(mcdi_mbox_read(MCDI_MBOX_PAUSE_ACK) == 1))
			;
	} else {
		/* Notify SSPM to enable MCDI */
		mcdi_mbox_write(MCDI_MBOX_PAUSE_ACTION, 0);

		/* Polling until MCDI Task resume */
		while (!(mcdi_mbox_read(MCDI_MBOX_PAUSE_ACK) == 0))
			;

		trace_mcdi_task_pause_rcuidle(smp_processor_id(), 0);
	}

	ret = true;

#endif
	return ret;
}

void mcdi_avail_cpu_mask(unsigned int cpu_mask)
{
	mcdi_mbox_write(MCDI_MBOX_AVAIL_CPU_MASK, cpu_mask);
}

void _mcdi_cpu_iso_mask(unsigned int iso_mask)
{
	iso_mask &= 0xff;

	/*
	 * If isolation bit of ALL CPU are set, means iso_mask is not reasonable
	 * Do NOT update iso_mask to mcdi controller
	 */
	if (iso_mask == 0xff)
		return;

	mcdi_mbox_write(MCDI_MBOX_CPU_ISOLATION_MASK, iso_mask);
}

bool is_cpu_pwr_on_event_pending(void)
{
	return (!(mcdi_mbox_read(MCDI_MBOX_PENDING_ON_EVENT) == 0));
}

/* Disable MCDI during cpu_up/cpu_down period */
static int mcdi_cpu_callback(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		__mcdi_pause(MCDI_PAUSE_BY_HOTPLUG, true);
		break;
	}

	return NOTIFY_OK;
}

static int mcdi_cpu_callback_leave_hotplug(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		mcdi_avail_cpu_cluster_update();

		__mcdi_pause(MCDI_PAUSE_BY_HOTPLUG, false);

		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mcdi_cpu_notifier = {
	.notifier_call = mcdi_cpu_callback,
	.priority   = INT_MAX,
};

static struct notifier_block mcdi_cpu_notifier_leave_hotplug = {
	.notifier_call = mcdi_cpu_callback_leave_hotplug,
	.priority   = INT_MIN,
};

static int mcdi_hotplug_cb_init(void)
{
	register_cpu_notifier(&mcdi_cpu_notifier);
	register_cpu_notifier(&mcdi_cpu_notifier_leave_hotplug);

	return 0;
}

static void __init mcdi_pm_qos_init(void)
{
}

static int __init mcdi_sysram_init(void)
{
	/* of init */
	mcdi_of_init(&mcdi_sysram_base);

	if (!mcdi_sysram_base)
		return -1;

	memset_io((void __iomem *)MCDI_SYSRAM,
		0,
		MCDI_SYSRAM_SIZE - MCDI_DEBUG_INFO_NON_REPLACE_OFFSET);

	return 0;
}

subsys_initcall(mcdi_sysram_init);

static int __init mcdi_init(void)
{
	/* Activate MCDI after SMP */
	pr_info("mcdi_init\n");

	/* Register CPU up/down callbacks */
	mcdi_hotplug_cb_init();

	/* procfs init */
	mcdi_procfs_init();

	/* MCDI governor init */
	mcdi_governor_init();

	mcdi_pm_qos_init();

	_mcdi_cpu_iso_mask(0x0);

	return 0;
}

late_initcall(mcdi_init);
