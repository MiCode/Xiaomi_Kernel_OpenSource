// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 */
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/tracepoint.h>
#include "ufshcd.h"
#include "ufs-mediatek.h"
#include "ufs-mediatek-dbg.h"

#define MAX_CMD_HIST_ENTRY_CNT (500)
#define UFS_AEE_BUFFER_SIZE (100 * 1024)

/*
 * Currently only global variables are used.
 *
 * For scalability, may introduce multiple history
 * instances bound to each device in the future.
 */
static bool cmd_hist_initialized;
static bool cmd_hist_enabled;
static spinlock_t cmd_hist_lock;
static unsigned int cmd_hist_cnt;
static unsigned int cmd_hist_ptr = MAX_CMD_HIST_ENTRY_CNT - 1;
static struct cmd_hist_struct *cmd_hist;
static struct ufs_hba *ufshba;
static char *ufs_aee_buffer;

static void ufs_mtk_dbg_print_evt(char **buff, unsigned long *size,
				  struct seq_file *m,
				  struct ufs_hba *hba, u32 id,
				  char *err_name)
{
	int i;
	bool found = false;
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];


	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		if (e->tstamp[p] == 0)
			continue;
		SPREAD_PRINTF(buff, size, m,
			      "%s[%d] = 0x%x at %lld us\n", err_name, p,
			      e->val[p], ktime_to_us(e->tstamp[p]));
		found = true;
	}

	if (!found)
		SPREAD_PRINTF(buff, size, m,
			      "No record of %s errors\n", err_name);
}

void ufs_mtk_dbg_print_info(char **buff, unsigned long *size,
			    struct seq_file *m)
{
	struct ufs_mtk_host *host;
	struct ufs_hba *hba = ufshba;

	if (!hba)
		return;

	host = ufshcd_get_variant(hba);

	/* Host state */
	SPREAD_PRINTF(buff, size, m,
		      "UFS Host state=%d\n", hba->ufshcd_state);
	SPREAD_PRINTF(buff, size, m,
		      "outstanding reqs=0x%lx tasks=0x%lx\n",
		      hba->outstanding_reqs, hba->outstanding_tasks);
	SPREAD_PRINTF(buff, size, m,
		      "saved_err=0x%x, saved_uic_err=0x%x\n",
		      hba->saved_err, hba->saved_uic_err);
	SPREAD_PRINTF(buff, size, m,
		      "Device power mode=%d, UIC link state=%d\n",
		      hba->curr_dev_pwr_mode, hba->uic_link_state);
	SPREAD_PRINTF(buff, size, m,
		      "PM in progress=%d, sys. suspended=%d\n",
		      hba->pm_op_in_progress, hba->is_sys_suspended);
	SPREAD_PRINTF(buff, size, m,
		      "Auto BKOPS=%d, Host self-block=%d\n",
		      hba->auto_bkops_enabled,
		      hba->host->host_self_blocked);
	if (ufshcd_is_clkgating_allowed(hba))
		SPREAD_PRINTF(buff, size, m,
			      "Clk gate=%d, suspended=%d, active_reqs=%d\n",
			      hba->clk_gating.state,
			      hba->clk_gating.is_suspended,
			      hba->clk_gating.active_reqs);
	else
		SPREAD_PRINTF(buff, size, m,
			      "clk_gating is disabled\n");
#ifdef CONFIG_PM
	SPREAD_PRINTF(buff, size, m,
		      "Runtime PM: req=%d, status:%d, err:%d\n",
		      hba->dev->power.request,
		      hba->dev->power.runtime_status,
		      hba->dev->power.runtime_error);
#endif
	SPREAD_PRINTF(buff, size, m,
		      "error handling flags=0x%x, req. abort count=%d\n",
		      hba->eh_flags, hba->req_abort_count);
	SPREAD_PRINTF(buff, size, m,
		      "Host capabilities=0x%x, hba-caps=0x%x, mtk-caps:0x%x\n",
		      hba->capabilities, hba->caps, host->caps);
	SPREAD_PRINTF(buff, size, m,
		      "quirks=0x%x, dev. quirks=0x%x\n", hba->quirks,
		      hba->dev_quirks);
	SPREAD_PRINTF(buff, size, m,
		      "hba->ufs_version = 0x%x, hba->capabilities = 0x%x\n",
		      hba->ufs_version, hba->capabilities);
	SPREAD_PRINTF(buff, size, m,
		      "last_hibern8_exit_tstamp at %lld us, hibern8_exit_cnt = %d\n",
		      ktime_to_us(hba->ufs_stats.last_hibern8_exit_tstamp),
		      hba->ufs_stats.hibern8_exit_cnt);

	/* PWR info */
	SPREAD_PRINTF(buff, size, m,
		      "[RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%d, %d], rate = %d\n",
		      hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		      hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		      hba->pwr_info.pwr_rx,
		      hba->pwr_info.pwr_tx,
		      hba->pwr_info.hs_rate);

	/* Device info */
	SPREAD_PRINTF(buff, size, m,
		      "Device vendor=0x%X, model=%s\n",
		      hba->dev_info.wmanufacturerid,
		      hba->dev_info.model);

	/* Error history */
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_PA_ERR, "pa_err");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_DL_ERR, "dl_err");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_NL_ERR, "nl_err");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_TL_ERR, "tl_err");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_DME_ERR, "dme_err");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_AUTO_HIBERN8_ERR, "auto_hibern8_err");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_FATAL_ERR, "fatal_err");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_LINK_STARTUP_FAIL, "link_startup_fail");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_RESUME_ERR, "resume_fail");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_SUSPEND_ERR, "suspend_fail");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_DEV_RESET, "dev_reset");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_HOST_RESET, "host_reset");
	ufs_mtk_dbg_print_evt(buff, size, m, hba,
			UFS_EVT_ABORT, "task_abort");
}

static int cmd_hist_advance_ptr(void)
{
	cmd_hist_ptr++;
	if (cmd_hist_ptr >= MAX_CMD_HIST_ENTRY_CNT)
		cmd_hist_ptr = 0;

	return cmd_hist_ptr;
}

static int cmd_hist_get_prev_ptr(int ptr)
{
	if (ptr == 0)
		return MAX_CMD_HIST_ENTRY_CNT - 1;
	else
		return (ptr - 1);
}

static bool cmd_hist_ptr_is_wraparound(int ptr)
{
	if (cmd_hist_ptr == 0) {
		if (ptr == (MAX_CMD_HIST_ENTRY_CNT - 1))
			return true;
	} else {
		if (ptr == (cmd_hist_ptr - 1))
			return true;
	}
	return false;
}


static void cmd_hist_init_common_info(int ptr)
{
	cmd_hist[ptr].cpu = smp_processor_id();
	cmd_hist[ptr].duration = 0;
	cmd_hist[ptr].pid = current->pid;
	cmd_hist[ptr].time = sched_clock();
}

static void probe_ufshcd_command(void *data, const char *dev_name,
				 enum ufs_trace_str_t str_t, unsigned int tag,
				 u32 doorbell, int transfer_len,
				 u32 intr, u64 lba, u8 opcode)
{
	int ptr;
	unsigned long flags;
	enum cmd_hist_event event;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist_enabled)
		goto out_unlock;

	ptr = cmd_hist_advance_ptr();

	if (str_t == UFS_CMD_SEND)
		event = CMD_SEND;
	else if (str_t == UFS_CMD_COMP)
		event = CMD_COMPLETED;
	else if (str_t == UFS_DEV_COMP)
		event = CMD_DEV_COMPLETED;
	else
		event = CMD_GENERIC;

	cmd_hist_init_common_info(ptr);
	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.utp.tag = tag;
	cmd_hist[ptr].cmd.utp.transfer_len = transfer_len;
	cmd_hist[ptr].cmd.utp.lba = lba;
	cmd_hist[ptr].cmd.utp.opcode = opcode;
	cmd_hist[ptr].cmd.utp.doorbell = doorbell;
	cmd_hist[ptr].cmd.utp.intr = intr;

	/* Need patch trace_ufshcd_command() first */
	cmd_hist[ptr].cmd.utp.crypt_en = 0;
	cmd_hist[ptr].cmd.utp.crypt_keyslot = 0;

	if (event == CMD_COMPLETED || event == CMD_DEV_COMPLETED) {
		ptr = cmd_hist_get_prev_ptr(cmd_hist_ptr);
		while (1) {
			if (cmd_hist[ptr].cmd.utp.tag == tag) {
				cmd_hist[cmd_hist_ptr].duration =
					sched_clock() -
					cmd_hist[ptr].time;
				break;
			}
			ptr = cmd_hist_get_prev_ptr(ptr);
			if (cmd_hist_ptr_is_wraparound(ptr))
				break;
		}
	}

	if (cmd_hist_cnt <= MAX_CMD_HIST_ENTRY_CNT)
		cmd_hist_cnt++;

out_unlock:
	spin_unlock_irqrestore(&cmd_hist_lock, flags);
}

static void probe_ufshcd_uic_command(void *data, const char *dev_name,
				     enum ufs_trace_str_t str_t, u32 cmd,
				     u32 arg1, u32 arg2, u32 arg3)
{
	int ptr;
	unsigned long flags;
	enum cmd_hist_event event;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist_enabled)
		goto out_unlock;

	ptr = cmd_hist_advance_ptr();

	if (str_t == UFS_CMD_SEND)
		event = CMD_UIC_SEND;
	else
		event = CMD_UIC_CMPL_GENERAL;

	cmd_hist_init_common_info(ptr);
	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.uic.cmd = cmd;
	cmd_hist[ptr].cmd.uic.arg1 = arg1;
	cmd_hist[ptr].cmd.uic.arg2 = arg2;
	cmd_hist[ptr].cmd.uic.arg3 = arg3;

	if (event == CMD_UIC_CMPL_GENERAL) {
		ptr = cmd_hist_get_prev_ptr(cmd_hist_ptr);
		while (1) {
			if (cmd_hist[ptr].cmd.uic.cmd == cmd) {
				cmd_hist[cmd_hist_ptr].duration =
					sched_clock() -
					cmd_hist[ptr].time;
				break;
			}
			ptr = cmd_hist_get_prev_ptr(ptr);
			if (cmd_hist_ptr_is_wraparound(ptr))
				break;
		}
	}
	if (cmd_hist_cnt <= MAX_CMD_HIST_ENTRY_CNT)
		cmd_hist_cnt++;

out_unlock:
	spin_unlock_irqrestore(&cmd_hist_lock, flags);
}

static void probe_ufshcd_clk_gating(void *data, const char *dev_name,
				    int state)
{
	int ptr;
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist_enabled)
		goto out_unlock;

	ptr = cmd_hist_advance_ptr();

	cmd_hist_init_common_info(ptr);
	cmd_hist[ptr].event = CMD_CLK_GATING;
	cmd_hist[ptr].cmd.clk_gating.state = state;

	if (cmd_hist_cnt <= MAX_CMD_HIST_ENTRY_CNT)
		cmd_hist_cnt++;

out_unlock:
	spin_unlock_irqrestore(&cmd_hist_lock, flags);
}

/**
 * Data structures to store tracepoints information
 */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static struct tracepoints_table interests[] = {
	{.name = "ufshcd_command", .func = probe_ufshcd_command},
	{.name = "ufshcd_uic_command", .func = probe_ufshcd_uic_command},
	{.name = "ufshcd_clk_gating", .func = probe_ufshcd_clk_gating},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

/**
 * Find the struct tracepoint* associated with a given tracepoint
 * name.
 */
static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

int cmd_hist_enable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	if (!cmd_hist) {
		cmd_hist_enabled = false;
		spin_unlock_irqrestore(&cmd_hist_lock, flags);
		return -ENOMEM;
	}

	cmd_hist_enabled = true;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return 0;
}

int cmd_hist_disable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	cmd_hist_enabled = false;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return 0;
}

static void cmd_hist_cleanup(void)
{
	cmd_hist_disable();
	vfree(cmd_hist);
}

#define CLK_GATING_STATE_MAX (4)

static char *clk_gating_state_str[CLK_GATING_STATE_MAX + 1] = {
	"clks_off",
	"clks_on",
	"req_clks_off",
	"req_clks_on",
	"unknown"
};

static void ufs_mtk_dbg_print_clk_gating_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.clk_gating.state;

	if (idx < 0 || idx >= CLK_GATING_STATE_MAX)
		idx = CLK_GATING_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-c(%d),%6llu.%lu,%5d,%2d,%13s\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		clk_gating_state_str[idx]
		);
}

static void ufs_mtk_dbg_print_device_reset_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.clk_gating.state;

	if (idx < 0 || idx >= CLK_GATING_STATE_MAX)
		idx = CLK_GATING_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-c(%d),%6llu.%lu,%5d,%2d,%13s\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		"device reset"
		);
}

static void ufs_mtk_dbg_print_uic_event(char **buff, unsigned long *size,
				   struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-u(%d),%6llu.%lu,%5d,%2d,0x%2x,arg1=0x%X,arg2=0x%X,arg3=0x%X,\t%llu\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.uic.cmd,
		cmd_hist[ptr].cmd.uic.arg1,
		cmd_hist[ptr].cmd.uic.arg2,
		cmd_hist[ptr].cmd.uic.arg3,
		cmd_hist[ptr].duration
		);
}

static void ufs_mtk_dbg_print_utp_event(char **buff, unsigned long *size,
				   struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	if (cmd_hist[ptr].cmd.utp.lba == 0xFFFFFFFFFFFFFFFF)
		cmd_hist[ptr].cmd.utp.lba = 0;
	SPREAD_PRINTF(buff, size, m,
		"%3d-r(%d),%6llu.%lu,%5d,%2d,0x%2x,t=%2d,db:0x%8x,is:0x%8x,crypt:%d,%d,lba=%10llu,len=%6d,\t%llu\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.utp.opcode,
		cmd_hist[ptr].cmd.utp.tag,
		cmd_hist[ptr].cmd.utp.doorbell,
		cmd_hist[ptr].cmd.utp.intr,
		cmd_hist[ptr].cmd.utp.crypt_en,
		cmd_hist[ptr].cmd.utp.crypt_keyslot,
		cmd_hist[ptr].cmd.utp.lba >> 3,
		cmd_hist[ptr].cmd.utp.transfer_len,
		cmd_hist[ptr].duration
		);
}

static void ufs_mtk_dbg_print_cmd_hist(char **buff, unsigned long *size,
				  u32 latest_cnt, struct seq_file *m)
{
	int ptr;
	int cnt;
	unsigned long flags;

	if (!cmd_hist_initialized)
		return;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist)
		return;

	cnt = min_t(u32, cmd_hist_cnt, MAX_CMD_HIST_ENTRY_CNT);
	if (latest_cnt)
		cnt = min_t(u32, latest_cnt, cnt);

	ptr = cmd_hist_ptr;

	SPREAD_PRINTF(buff, size, m,
		      "UFS CMD History: Latest %d of total %d entries, ptr=%d\n",
		      latest_cnt, cnt, ptr);

	while (cnt) {
		if (cmd_hist[ptr].event < CMD_UIC_SEND)
			ufs_mtk_dbg_print_utp_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event < CMD_REG_TOGGLE)
			ufs_mtk_dbg_print_uic_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_CLK_GATING)
			ufs_mtk_dbg_print_clk_gating_event(buff, size,
							   m, ptr);
		else if (cmd_hist[ptr].event == CMD_ABORTING)
			ufs_mtk_dbg_print_utp_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_DEVICE_RESET)
			ufs_mtk_dbg_print_device_reset_event(buff, size,
							     m, ptr);
		cnt--;
		ptr--;
		if (ptr < 0)
			ptr = MAX_CMD_HIST_ENTRY_CNT - 1;
	}

	spin_unlock_irqrestore(&cmd_hist_lock, flags);

}

void ufs_mediatek_dbg_dump(void)
{
	ufs_mtk_dbg_print_info(NULL, NULL, NULL);

	ufs_mtk_dbg_print_cmd_hist(NULL, NULL, MAX_CMD_HIST_ENTRY_CNT,
				   NULL);
}

void ufs_mtk_dbg_get_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long free_size = UFS_AEE_BUFFER_SIZE;
	char *buff;

	if (!cmd_hist) {
		pr_info("failed to dump UFS: null cmd history buffer");
		return;
	}

	if (!ufs_aee_buffer) {
		pr_info("failed to dump UFS: null AEE buffer");
		return;
	}

	buff = ufs_aee_buffer;
	ufs_mtk_dbg_print_info(&buff, &free_size, NULL);
	ufs_mtk_dbg_print_cmd_hist(&buff, &free_size,
				   MAX_CMD_HIST_ENTRY_CNT, NULL);

	/* retrun start location */
	*vaddr = (unsigned long)ufs_aee_buffer;
	*size = UFS_AEE_BUFFER_SIZE - free_size;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_get_aee_buffer);

#ifndef USER_BUILD_KERNEL
#define PROC_PERM		0660
#else
#define PROC_PERM		0440
#endif

static ssize_t ufs_debug_proc_write(struct file *file, const char *buf,
				 size_t count, loff_t *data)
{
	unsigned long op = UFSDBG_UNKNOWN;
	char cmd_buf[16];

	if (count == 0 || count > 15)
		return -EINVAL;

	if (copy_from_user(cmd_buf, buf, count))
		return -EINVAL;

	cmd_buf[count] = '\0';
	if (kstrtoul(cmd_buf, 15, &op))
		return -EINVAL;

	if (op == UFSDBG_CMD_LIST_ENABLE) {
		cmd_hist_enable();
		pr_info("ufs_mtk_dbg: cmd history on\n");
	} else if (op == UFSDBG_CMD_LIST_DISABLE) {
		cmd_hist_disable();
		pr_info("ufs_mtk_dbg: cmd history off\n");
	} else
		return -EINVAL;

	return count;
}

static int ufs_debug_proc_show(struct seq_file *m, void *v)
{
	ufs_mtk_dbg_print_info(NULL, NULL, m);
	ufs_mtk_dbg_print_cmd_hist(NULL, NULL, MAX_CMD_HIST_ENTRY_CNT,
				   m);
	return 0;
}

static int ufs_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_debug_proc_show, inode->i_private);
}

static const struct proc_ops ufs_debug_proc_fops = {
	.proc_open = ufs_debug_proc_open,
	.proc_write = ufs_debug_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int ufs_mtk_dbg_init_procfs(void)
{
	struct proc_dir_entry *prEntry;
	kuid_t uid;
	kgid_t gid;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	/* Create "ufs_debug" node */
	prEntry = proc_create("ufs_debug", PROC_PERM, NULL,
			      &ufs_debug_proc_fops);

	if (prEntry)
		proc_set_user(prEntry, uid, gid);
	else
		pr_info("%s: failed to create ufs_debugn", __func__);

	return 0;
}

static void ufs_mtk_dbg_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
		}
	}

	cmd_hist_cleanup();
}

int ufs_mtk_dbg_register(struct ufs_hba *hba)
{
	int i, ret;

	/*
	 * Ignore any failure of AEE buffer allocation to still allow
	 * command history dump in procfs.
	 */
	ufs_aee_buffer = kzalloc(UFS_AEE_BUFFER_SIZE, GFP_NOFS);

	spin_lock_init(&cmd_hist_lock);
	ufshba = hba;
	cmd_hist_initialized = true;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("Error: %s not found\n",
				interests[i].name);
			/* Unload previously loaded */
			ufs_mtk_dbg_cleanup();
			return -EINVAL;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	/* Create control nodes in procfs */
	ret = ufs_mtk_dbg_init_procfs();

	/* Enable command history feature by default */
	if (!ret)
		cmd_hist_enable();
	else
		ufs_mtk_dbg_cleanup();

	return ret;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_register);

static void __exit ufs_mtk_dbg_exit(void)
{
	kfree(cmd_hist);
}

static int __init ufs_mtk_dbg_init(void)
{
	cmd_hist = kcalloc(MAX_CMD_HIST_ENTRY_CNT,
			   sizeof(struct cmd_hist_struct),
			   GFP_KERNEL);
	return 0;
}

module_init(ufs_mtk_dbg_init)
module_exit(ufs_mtk_dbg_exit)

MODULE_DESCRIPTION("MediaTek UFS Debugging Facility");
MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_LICENSE("GPL v2");
