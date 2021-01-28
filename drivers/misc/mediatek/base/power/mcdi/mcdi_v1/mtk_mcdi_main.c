// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
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
#include <linux/kthread.h>
#include <linux/delay.h>

#include <mtk_cpuidle.h>
#include <mtk_idle.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_profile.h>
#include <mtk_mcdi_util.h>
#include <mtk_mcdi_cpc.h>

#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_api.h>

#include <mtk_mcdi_governor_hint.h>

/* #include <trace/events/mtk_idle_event.h> */
/* #include <linux/irqchip/mtk-gic-extend.h> */

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
	"latency",
	"residency",
	"last core"
};

static unsigned long long mcdi_heart_beat_log_prev;
static DEFINE_SPINLOCK(mcdi_heart_beat_spin_lock);

static unsigned int mcdi_heart_beat_log_dump_thd = 5000;          /* 5 sec */

static bool mcdi_stress_en;
static unsigned int mcdi_stress_us = 10 * 1000;
static struct task_struct *mcdi_stress_tsk[NF_CPU];

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

void __attribute__((weak))
mcdi_set_state_lat(int cpu_type, int state, unsigned int val)
{
}

void __attribute__((weak))
mcdi_set_state_res(int cpu_type, int state, unsigned int val)
{
}

void wakeup_all_cpu(void)
{
	int cpu = 0;

	/*
	 * smp_proccessor_id() will be called in the flow of
	 * smp_send_reschedule(), hence disable preemtion to
	 * avoid being scheduled out.
	 */
	preempt_disable();

	for (cpu = 0; cpu < NF_CPU; cpu++) {
		if (cpu_online(cpu))
			smp_send_reschedule(cpu);
	}

	preempt_enable();
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

static int mcdi_stress_task(void *arg)
{
	while (mcdi_stress_en)
		usleep_range(mcdi_stress_us - 10, mcdi_stress_us + 10);

	return 0;
}

static void mcdi_stress_start(void)
{
	int i;
	char name[16] = {0};

	if (mcdi_stress_en)
		return;

	mcdi_stress_en = true;

	for (i = 0; i < NF_CPU; i++) {
		snprintf(name, sizeof(name), "mcdi_stress_task%d", i);

		mcdi_stress_tsk[i] =
			kthread_create(mcdi_stress_task, NULL, name);

		if (!IS_ERR(mcdi_stress_tsk[i])) {
			kthread_bind(mcdi_stress_tsk[i], i);
			wake_up_process(mcdi_stress_tsk[i]);
		}
	}
}

static void mcdi_stress_stop(void)
{
	mcdi_stress_en = false;
	msleep(20);
}

static void mcdi_idle_state_setting(unsigned long idx, unsigned long enable)
{
	struct cpuidle_driver *tbl = NULL;
	int cpu;

	if (idx >= NF_MCDI_STATE)
		return;

	for (cpu = 0; cpu < NF_CPU; cpu++) {
		tbl = mcdi_state_tbl_get(cpu);
		if (tbl->states[idx].disabled != (!enable))
			tbl->states[idx].disabled = !enable;
	}
}

/* debugfs */
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

/* mcdi_state */
static ssize_t mcdi_state_read(struct file *filp,
		char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	int i;
	char *p = dbg_buf;
	unsigned long ac_cpu_cond_info[NF_ANY_CORE_CPU_COND_INFO] = {0};
	int latency_req = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);

	struct mcdi_feature_status feature_stat;

	get_mcdi_feature_status(&feature_stat);

	mcdi_log("Feature:\n");
	mcdi_log("\tenable = %d\n", feature_stat.enable);
	mcdi_log("\tpause = %d\n", feature_stat.pause);
	mcdi_log("\tmax s_state = %d\n", feature_stat.s_state);
	mcdi_log("\tcluster_off = %d\n", feature_stat.cluster_off);
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
		mcdi_cnt_cluster[i] = mcdi_get_cluster_off_cnt(i);
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

	mcdi_log("\n");

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
	} else if (!strncmp(cmd_str, "hint", sizeof("hint"))) {
		_system_idle_hint_request(SYSTEM_IDLE_HINT_USER_MCDI_TEST,
								param != 0);
		return count;
	} else {
		return -EINVAL;
	}
}

static ssize_t mcdi_info_read(struct file *filp,
		char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	int i, cpu;
	char *p = dbg_buf;
	struct cpuidle_driver *tbl = NULL;

	mcdi_log("mcdi stress test: %s (timer:%dus)",
			mcdi_stress_en ? "Enalbe" : "Disable",
			mcdi_stress_us);

	for (cpu = 0; cpu < NF_CPU; cpu++) {

		if (tbl == mcdi_state_tbl_get(cpu)) {
			mcdi_log(", %d", cpu);
			continue;
		}

		mcdi_log("\n\n");

		mcdi_log("%12s : %12s, %10s, %10s\n",
				"idle state",
				"name",
				"latency",
				"residency");

		tbl = mcdi_state_tbl_get(cpu);
		for (i = 0; i < tbl->state_count; i++) {
			struct cpuidle_state *s = &tbl->states[i];

			mcdi_log("%12d : %12s, %10u, %10u\n",
					i,
					s->name,
					s->exit_latency,
					s->target_residency);
		}
		mcdi_log("CPU Type = %d : cpu %d", cpu_type_idx_get(cpu), cpu);
	}

	if (mcdi_is_cpc_mode()) {
		unsigned int fail, cnt;

		fail = get_mcdi_cluster_dev()->chk_res_fail;
		cnt = get_mcdi_cluster_dev()->chk_res_cnt;
		get_mcdi_cluster_dev()->chk_res_fail = 0;
		get_mcdi_cluster_dev()->chk_res_cnt = 0;

		mcdi_log("\n\ncheck remain sleep time each core : %s\n",
				get_mcdi_cluster_dev()->chk_res_each_core ?
					"yes" : "no");
		mcdi_log("Each core residency check fail rate : %d%% (%d/%d)\n",
				(!cnt) ? 0 : (100 * fail) / cnt, fail, cnt);

		mcdi_log("cluster timer enable : %s",
				get_mcdi_cluster_dev()->tmr_en ? "yes" : "no");
	}
	mcdi_log("\n\n");

	mcdi_log("Usage: echo [command line] > /proc/mcdi/info\n");
	mcdi_log("command line:\n");
	mcdi_log("  %-40s : set idle state latency value\n",
				"latency [CPU Type] [state] [val(dec)]");
	mcdi_log("  %-40s : set idle state residency value\n",
				"residency [CPU Type] [state] [val(dec)]");
	mcdi_log("  %-40s : enable disable stress test\n",
				"stress[0|1]");
	mcdi_log("  %-40s : set stress timer interval\n",
				"stress_timer [time_us(dec)]");
	mcdi_log("  %-40s : check remain sleep each core(CPC mode)\n",
				"remain [0|1]");
	mcdi_log("  %-40s : enable/disable timer when enter cluster off\n",
				"mcdi_timer [0|1]");

	mcdi_log("\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t mcdi_info_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	unsigned long param_0 = 0;
	unsigned long param_1 = 0;
	unsigned long param_2 = 0;
	unsigned long param_cnt = 1;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_0_str = NULL;
	char *param_1_str = NULL;
	char *param_2_str = NULL;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");

	if (cmd_str == NULL)
		return -EINVAL;

	param_0_str = strsep(&cmd_ptr, " ");

	if (param_0_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_0_str, 10, &param_0);

	if (ret < 0)
		return -EINVAL;

	param_1_str = strsep(&cmd_ptr, " ");

	if (param_1_str == NULL)
		goto parse_cmd;

	ret = kstrtoul(param_1_str, 10, &param_1);

	if (ret < 0)
		goto parse_cmd;

	param_cnt++;

	param_2_str = strsep(&cmd_ptr, " ");

	if (param_2_str == NULL)
		goto parse_cmd;

	ret = kstrtoul(param_2_str, 10, &param_2);

	if (ret < 0)
		goto parse_cmd;

	param_cnt++;

parse_cmd:

	if (!strncmp(cmd_str, "latency", sizeof("latency"))) {

		if (param_cnt == 3)
			mcdi_set_state_lat(param_0, param_1, param_2);

		return count;

	} else if (!strncmp(cmd_str, "residency", sizeof("residency"))) {

		if (param_cnt == 3)
			mcdi_set_state_res(param_0, param_1, param_2);

		return count;

	} else if (!strncmp(cmd_str, "state", sizeof("state"))) {

		if (param_cnt == 2)
			mcdi_idle_state_setting(param_0, !!param_1);

		return count;

	} else if (!strncmp(cmd_str, "stress", sizeof("stress"))) {

		if (param_cnt == 1) {
			if (param_0)
				mcdi_stress_start();
			else
				mcdi_stress_stop();
		}

		return count;

	} else if (!strncmp(cmd_str, "remain", sizeof("remain"))) {

		if (param_cnt == 1 && mcdi_is_cpc_mode())
			get_mcdi_cluster_dev()->chk_res_each_core = !!param_0;

		return count;

	} else if (!strncmp(cmd_str, "mcdi_timer", sizeof("mcdi_timer"))) {

		if (param_cnt == 1 && mcdi_is_cpc_mode())
			get_mcdi_cluster_dev()->tmr_en = !!param_0;

		return count;

	} else if (!strncmp(cmd_str, "stress_timer", sizeof("stress_timer"))) {

		if (param_cnt == 1)
			mcdi_stress_us = clamp_val(param_0, 100, 20000);

		return count;

	} else {
		return -EINVAL;
	}
}

PROC_FOPS_MCDI(state);
PROC_FOPS_MCDI(info);

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

	PROC_CREATE_MCDI(mcdi_dir, state);
	PROC_CREATE_MCDI(mcdi_dir, info);

	mcdi_procfs_profile_init(mcdi_dir);
	mcdi_procfs_cpc_init(mcdi_dir);

	return 0;
}

static void __go_to_wfi(int cpu)
{
	/* remove_cpu_from_prefer_schedule_domain(cpu); */

	/* trace_rgidle_rcuidle(cpu, 1); */

	isb();
	/* memory barrier before WFI */
	mb();
	wfi();

	/* trace_rgidle_rcuidle(cpu, 0); */

	/* add_cpu_to_prefer_schedule_domain(cpu); */
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
	unsigned long ac_cpu_cond_info[NF_ANY_CORE_CPU_COND_INFO] = {0};
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
		mcdi_cnt_cluster[i] = mcdi_get_cluster_off_cnt(i);

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

	mcdi_buf_append(buf, ", enabled = %d, max_s_state = %d",
						feature_stat.enable,
						feature_stat.s_state);

	mcdi_buf_append(buf, ", system_idle_hint = %08x",
						system_idle_hint_result_raw());

	pr_info("%s\n", get_mcdi_buf(buf));
}

int wfi_enter(int cpu)
{
	idle_refcnt_inc();

	set_mcdi_idle_state(cpu, MCDI_STATE_WFI);

	mcdi_usage_time_start(cpu);

	__go_to_wfi(cpu);

	mcdi_usage_time_stop(cpu);

	idle_refcnt_dec();

	mcdi_cnt_wfi[cpu]++;

	mcdi_usage_calc(cpu);

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

	mcdi_profile_ts(cpu, MCDI_PROFILE_ENTER);

	idle_refcnt_inc();

	if (likely(mcdi_fw_is_ready()))
		state = mcdi_governor_select(cpu, cluster_idx);
	else
		state = MCDI_STATE_WFI;

	if (state >= MCDI_STATE_WFI && state <= MCDI_STATE_CLUSTER_OFF) {
		mcdi_sta = &(mcdi_state_tbl_get(cpu)->states[state]);
		sched_idle_set_state(mcdi_sta, state);
	}

	set_mcdi_idle_state(cpu, state);

	mcdi_profile_ts(cpu, MCDI_PROFILE_CPU_DORMANT_ENTER);

	mcdi_usage_time_start(cpu);

	switch (state) {
	case MCDI_STATE_WFI:
		__go_to_wfi(cpu);

		break;
	case MCDI_STATE_CPU_OFF:

		/* trace_mcdi_rcuidle(cpu, 1); */

		aee_rr_rec_mcdi_val(cpu, MCDI_STATE_CPU_OFF << 16 | 0xff);

		mtk_enter_idle_state(MTK_MCDI_CPU_MODE);

		aee_rr_rec_mcdi_val(cpu, 0x0);

		/* trace_mcdi_rcuidle(cpu, 0); */

		mcdi_cnt_cpu[cpu]++;

		break;
	case MCDI_STATE_CLUSTER_OFF:

		/* trace_mcdi_rcuidle(cpu, 1); */

		aee_rr_rec_mcdi_val(cpu, MCDI_STATE_CLUSTER_OFF << 16 | 0xff);

		mtk_enter_idle_state(MTK_MCDI_CLUSTER_MODE);

		aee_rr_rec_mcdi_val(cpu, 0x0);

		/* trace_mcdi_rcuidle(cpu, 0); */

		mcdi_cnt_cpu[cpu]++;

		break;
	case MCDI_STATE_SODI:

		soidle_enter(cpu);

		break;
	case MCDI_STATE_DPIDLE:

		dpidle_enter(cpu);

		break;
	case MCDI_STATE_SODI3:

		soidle3_enter(cpu);

		break;
	}

	mcdi_usage_time_stop(cpu);

	mcdi_profile_ts(cpu, MCDI_PROFILE_CPU_DORMANT_LEAVE);

	mcdi_usage_calc(cpu);

	if (state >= MCDI_STATE_WFI && state <= MCDI_STATE_CLUSTER_OFF)
		sched_idle_set_state(NULL, -1);

	mcdi_governor_reflect(cpu, state);

	idle_refcnt_dec();

	mcdi_profile_ts(cpu, MCDI_PROFILE_LEAVE);
	mcdi_profile_calc(cpu);

	return 0;
}

bool __mcdi_pause(unsigned int id, bool paused)
{
	mcdi_state_pause(id, paused);

	if (!(get_mcdi_feature_stat()->enable))
		return true;

	if (!mcdi_get_boot_time_check())
		return true;

	if (paused)
		mcdi_wakeup_all_cpu();

	return true;
}

bool _mcdi_task_pause(bool paused)
{
	if (!is_mcdi_working())
		return false;

	if (paused) {

		/* trace_mcdi_task_pause_rcuidle(smp_processor_id(), true); */

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

		/* trace_mcdi_task_pause_rcuidle(smp_processor_id(), 0); */
	}

	return true;
}

void mcdi_avail_cpu_mask(unsigned int cpu_mask)
{
	mcdi_mbox_write(MCDI_MBOX_AVAIL_CPU_MASK, cpu_mask);
}

/* Disable MCDI before cpu up/cpu down */
static int mcdi_cpuhp_notify_enter(unsigned int cpu)
{
	__mcdi_pause(MCDI_PAUSE_BY_HOTPLUG, true);

	return 0;
}

/* Enable MCDI after cpu up/cpu down */
static int mcdi_cpuhp_notify_leave(unsigned int cpu)
{
	mcdi_avail_cpu_cluster_update();
	__mcdi_pause(MCDI_PAUSE_BY_HOTPLUG, false);

	return 0;
}

static int mcdi_hotplug_cb_init(void)
{
	cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN_END, "mcdi_cb",
				mcdi_cpuhp_notify_enter,
				mcdi_cpuhp_notify_leave);
	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "mcdi_cb",
				mcdi_cpuhp_notify_enter,
				mcdi_cpuhp_notify_leave);

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
	pr_info("%s\n", __func__);

	/* Register CPU up/down callbacks */
	mcdi_hotplug_cb_init();

	/* procfs init */
	mcdi_procfs_init();

	/* CPC init */
	mcdi_cpc_init();

	/* MCDI governor init */
	mcdi_governor_init();

	mcdi_pm_qos_init();

	mcdi_cpu_iso_mask(0x0);

	mcdi_prof_init();

	return 0;
}

late_initcall(mcdi_init);
