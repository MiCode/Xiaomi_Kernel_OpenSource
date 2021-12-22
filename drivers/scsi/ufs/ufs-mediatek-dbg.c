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
static char ufs_aee_buffer[UFS_AEE_BUFFER_SIZE];

void ufsdbg_print_info(char **buff, unsigned long *size, struct seq_file *m)
{
	struct ufs_hba *hba = ufs_mtk_get_hba();

	if (!hba)
		return;

	/* Host state */
	SPREAD_PRINTF(buff, size, m,
		      "UFS Host state=%d\n", hba->ufshcd_state);
	SPREAD_PRINTF(buff, size, m,
		      "lrb in use=0x%lx, outstanding reqs=0x%lx tasks=0x%lx\n",
		      hba->lrb_in_use, hba->outstanding_reqs,
		      hba->outstanding_tasks);
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
		      hba->auto_bkops_enabled, hba->host->host_self_blocked);
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
		      hba->dev->power.request, hba->dev->power.runtime_status,
		      hba->dev->power.runtime_error);
#endif
	SPREAD_PRINTF(buff, size, m,
		      "error handling flags=0x%x, req. abort count=%d\n",
		      hba->eh_flags, hba->req_abort_count);
	SPREAD_PRINTF(buff, size, m,
		      "Host capabilities=0x%x, caps=0x%x\n",
		      hba->capabilities, hba->caps);
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
		      "Device vendor=0x%X, model=%s, ufs version=0x%X\n",
		      hba->dev_info.wmanufacturerid,
		      hba->dev_info.model,
			  hba->dev_info.wspecversion);

	/* Error history */
	ufshcd_print_all_evt_hist(hba, m, buff, size);
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

#if BITS_PER_LONG == 32
#define ufshcd_update_evt_hist_perf_warn(cmd, op, len) \
do { \
	struct ufs_hba *h = ufs_mtk_get_hba(); \
	if (h && (cmd).duration >= 1000000000) { \
		ufshcd_update_evt_hist(h, UFS_EVT_PERF_WARN, \
				(u32) ((op << 24) | \
					(((len >> 12) & 0xFF) << 16) | \
					(div_u64((cmd).duration, 1000000)) \
				)); \
	} \
} while(0)
#else
#define ufshcd_update_evt_hist_perf_warn(cmd, op, len) \
do { \
	struct ufs_hba *h = ufs_mtk_get_hba(); \
	if (h && (cmd).duration >= 1000000000) { \
		ufshcd_update_evt_hist(h, UFS_EVT_PERF_WARN, \
			    (u32) ((op << 24) | \
					(((len >> 12) & 0xFF) << 16) | \
					((cmd).duration / 1000000) \
				));\
	} \
} while (0)
#endif

static void probe_ufshcd_command(void *data, const char *dev_name,
				 const char *str, unsigned int tag,
				 u32 doorbell, int transfer_len, u32 intr,
				 u64 lba, u8 opcode,
				 u8 crypt_en, u8 crypt_keyslot)
{
	int ptr;
	unsigned long flags;
	enum cmd_hist_event event;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist_enabled)
		goto out_unlock;

	ptr = cmd_hist_advance_ptr();

	if (!strcmp(str, "send"))
		event = CMD_SEND;
	else if (!strcmp(str, "complete"))
		event = CMD_COMPLETED;
	else if (!strcmp(str, "dev_complete"))
		event = CMD_DEV_COMPLETED;
	else if (!strcmp(str, "abort"))
		event = CMD_ABORTING;
	else if (!strcmp(str, "perf_mode"))
		event = CMD_PERF_MODE;
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
	cmd_hist[ptr].cmd.utp.crypt_en = crypt_en;
	cmd_hist[ptr].cmd.utp.crypt_keyslot = crypt_keyslot;

	if (event == CMD_COMPLETED || event == CMD_DEV_COMPLETED) {
		ptr = cmd_hist_get_prev_ptr(cmd_hist_ptr);
		while (1) {
			if (cmd_hist[ptr].cmd.utp.tag == tag) {
				cmd_hist[cmd_hist_ptr].duration =
					sched_clock() - cmd_hist[ptr].time;
				break;
			}
			ptr = cmd_hist_get_prev_ptr(ptr);
			if (cmd_hist_ptr_is_wraparound(ptr))
				break;
		}
		/* Over 1 second, record performance warning */
		ufshcd_update_evt_hist_perf_warn(cmd_hist[cmd_hist_ptr], opcode, transfer_len);
	}

	if (cmd_hist_cnt <= MAX_CMD_HIST_ENTRY_CNT)
		cmd_hist_cnt++;

out_unlock:
	spin_unlock_irqrestore(&cmd_hist_lock, flags);
}

static void probe_ufshcd_uic_command(void *data, const char *dev_name,
				     const char *str, u32 cmd,
				     u32 arg1, u32 arg2, u32 arg3)
{
	int ptr;
	unsigned long flags;
	enum cmd_hist_event event;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist_enabled)
		goto out_unlock;

	ptr = cmd_hist_advance_ptr();

	if (!strcmp(str, "send"))
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
					sched_clock() - cmd_hist[ptr].time;
				break;
			}
			ptr = cmd_hist_get_prev_ptr(ptr);
			if (cmd_hist_ptr_is_wraparound(ptr))
				break;
		}
		/* Over 1 second, record performance warning */
		ufshcd_update_evt_hist_perf_warn(cmd_hist[cmd_hist_ptr], cmd, arg2);
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

static void probe_ufshcd_device_reset(void)
{
	int ptr;
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist_enabled)
		goto out_unlock;

	ptr = cmd_hist_advance_ptr();
	cmd_hist_init_common_info(ptr);
	cmd_hist[ptr].event = CMD_DEVICE_RESET;

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
	{.name = "ufshcd_device_reset", .func = probe_ufshcd_device_reset},
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
	int ret = 0;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	cmd_hist_enabled = true;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return ret;
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
	cmd_hist = NULL;
}

void ufs_mtk_dbg_add_trace(const char *dev_name,
				 const char *str, unsigned int tag,
				 u32 doorbell, int transfer_len, u32 intr,
				 u64 lba, u8 opcode,
				 u8 crypt_en, u8 crypt_keyslot) {
	probe_ufshcd_command(NULL, dev_name, str, tag,
			doorbell, transfer_len, intr, lba, opcode,
			crypt_en, crypt_keyslot);
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_add_trace);

#define CLK_GATING_STATE_MAX (4)

static char *clk_gating_state_str[CLK_GATING_STATE_MAX + 1] = {
	"clks_off",
	"clks_on",
	"req_clks_off",
	"req_clks_on",
	"unknown"
};

static void ufsdbg_print_clk_gating_event(char **buff, unsigned long *size,
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

static void ufsdbg_print_device_reset_event(char **buff, unsigned long *size,
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

static void ufsdbg_print_uic_event(char **buff, unsigned long *size,
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

static void ufsdbg_print_utp_event(char **buff, unsigned long *size,
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

static void ufsdbg_print_cmd_hist(char **buff, unsigned long *size,
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
			ufsdbg_print_utp_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event < CMD_REG_TOGGLE)
			ufsdbg_print_uic_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_CLK_GATING)
			ufsdbg_print_clk_gating_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_ABORTING)
			ufsdbg_print_utp_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_DEVICE_RESET)
			ufsdbg_print_device_reset_event(buff, size, m, ptr);
		cnt--;
		ptr--;
		if (ptr < 0)
			ptr = MAX_CMD_HIST_ENTRY_CNT - 1;
	}

	spin_unlock_irqrestore(&cmd_hist_lock, flags);

}

void ufs_mediatek_dbg_dump(void)
{
	ufsdbg_print_info(NULL, NULL, NULL);

	ufsdbg_print_cmd_hist(NULL, NULL, MAX_CMD_HIST_ENTRY_CNT, NULL);
}

void get_ufs_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long free_size = UFS_AEE_BUFFER_SIZE;
	char *buff;

	if (!cmd_hist) {
		pr_info("====== Null cmd_hist, dump skipped ======\n");
		return;
	}

	buff = ufs_aee_buffer;
	ufsdbg_print_info(&buff, &free_size, NULL);
	ufsdbg_print_cmd_hist(&buff, &free_size, MAX_CMD_HIST_ENTRY_CNT, NULL);

	/* retrun start location */
	*vaddr = (unsigned long)ufs_aee_buffer;
	*size = UFS_AEE_BUFFER_SIZE - free_size;
}
EXPORT_SYMBOL(get_ufs_aee_buffer);

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
	struct ufs_hba *hba = ufs_mtk_get_hba();
	struct ufs_mtk_host *host = NULL;

	if (hba)
		host = ufshcd_get_variant(hba);

	if (count == 0 || count > 15)
		return -EINVAL;

	if (copy_from_user(cmd_buf, buf, count))
		return -EINVAL;

	cmd_buf[count] = '\0';
	if (kstrtoul(cmd_buf, 15, &op))
		return -EINVAL;

	if (op == UFSDBG_CMD_LIST_ENABLE) {
		cmd_hist_enable();
		pr_info("ufsdbg: cmd history on\n");
	} else if (op == UFSDBG_CMD_LIST_DISABLE) {
		cmd_hist_disable();
		pr_info("ufsdbg: cmd history off\n");
	} else if (op == UFS_CMD_QOS_ON) {
		if (host && host->qos_allowed) {
			host->qos_enabled = true;
			pr_info("ufsdbg: QoS on\n");
		}
	} else if (op == UFS_CMD_QOS_OFF) {
		if (host && host->qos_allowed) {
			host->qos_enabled = false;
			pr_info("ufsdbg: QoS off\n");
		}
	} else {
		return -EINVAL;
	}

	return count;
}

static int ufs_debug_proc_show(struct seq_file *m, void *v)
{
	ufsdbg_print_info(NULL, NULL, m);
	ufsdbg_print_cmd_hist(NULL, NULL, MAX_CMD_HIST_ENTRY_CNT, m);
	return 0;
}

static int ufs_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_debug_proc_show, inode->i_private);
}

static int ufs_perf_proc_show(struct seq_file *m, void *v)
{
	struct ufs_mtk_host *host = NULL;
	struct ufs_hba *hba = ufs_mtk_get_hba();

	if (hba) {
		host = ufshcd_get_variant(hba);
	}

	if (!host) {
		seq_puts(m, "UFS Performance Mode\n");
		seq_puts(m, "UFS instance is invalid\n");
		return 0;
	}

	seq_puts(m, "UFS Performance Mode\n");
	seq_printf(m, "  - supported: %d\n", ufs_mtk_perf_is_supported(host));
	seq_printf(m, "  - enabled: %d\n", host->perf_enable);
	seq_printf(m, "  - mode: %d\n", host->perf_mode);
	seq_printf(m, "  - crypto_vcore_opp: %d\n", host->crypto_vcore_opp);

	return 0;
}

static ssize_t ufs_perf_proc_write(struct file *file, const char *ubuf,
				   size_t count, loff_t *data)
{
	struct ufs_mtk_host *host = NULL;
	unsigned long op;
	char cmd[16] = {0};
	loff_t buff_pos = 0;
	int ret = 0;
	int last_mode;
	struct ufs_hba *hba = ufs_mtk_get_hba();

	if (hba)
		host = ufshcd_get_variant(hba);

	if (!host)
		return 0;

	ret = simple_write_to_buffer(cmd, 16, &buff_pos, ubuf, count);
	if (ret < 0) {
		dev_info(host->hba->dev, "%s: failed to read user data\n",
			__func__);
		return -EINVAL;
	}

	cmd[ret] = '\0';
	if (kstrtoul(cmd, 10, &op))
		return -EINVAL;
	last_mode = host->perf_mode;
	if (op == PERF_AUTO) {
		host->perf_mode = op;
		ret = 0;
	} else if (op == PERF_FORCE_ENABLE &&
			host->perf_mode != PERF_FORCE_ENABLE) {
		host->perf_mode = PERF_FORCE_ENABLE;
		ret = ufs_mtk_perf_setup_crypto_clk(host, true);
	} else if (op == PERF_FORCE_DISABLE &&
			host->perf_mode != PERF_FORCE_DISABLE) {
		host->perf_mode = PERF_FORCE_DISABLE;
		ret = ufs_mtk_perf_setup_crypto_clk(host, false);
	} else
		return -EINVAL;
	if (ret)
		host->perf_mode = last_mode;
	return count;
}

static int ufs_perf_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_perf_proc_show, inode->i_private);
}

static const struct file_operations ufs_debug_proc_fops = {
	.open = ufs_debug_proc_open,
	.write = ufs_debug_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations ufs_debug_perf_fops = {
	.open = ufs_perf_proc_open,
	.write = ufs_perf_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int ufsdbg_init_procfs(void)
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

	/* Create ufs_perf for performance mode*/
	prEntry = proc_create("ufs_perf", 0660, NULL, &ufs_debug_perf_fops);

	if (prEntry)
		proc_set_user(prEntry, uid, gid);
	else
		pr_info("%s: failed to create /proc/ufs_perf\n", __func__);

	return 0;
}

static void ufsdbg_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func, NULL);
		}
	}

	cmd_hist_cleanup();
}

int ufsdbg_register(struct device *dev)
{
	int i, ret;

	spin_lock_init(&cmd_hist_lock);
	cmd_hist_initialized = true;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("Error: %s not found\n", interests[i].name);
			ret = -EINVAL;
			goto out;
		}

		tracepoint_probe_register(interests[i].tp, interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	/* Create control nodes in procfs */
	ret = ufsdbg_init_procfs();

	/* Enable command history feature by default */
	if (ret) {
		goto out;
	}
	cmd_hist = kcalloc(MAX_CMD_HIST_ENTRY_CNT,
				sizeof(struct cmd_hist_struct), GFP_NOFS);
	if (!cmd_hist) {
		ret = -ENOMEM;
		goto out;
	}
	cmd_hist_enable();
	return ret;
out:
	ufsdbg_cleanup();
	return ret;
}
EXPORT_SYMBOL_GPL(ufsdbg_register);

static void __exit ufsdbg_exit(void)
{
	ufsdbg_cleanup();
}

static int __init ufsdbg_init(void)
{
	return 0;
}

module_init(ufsdbg_init)
module_exit(ufsdbg_exit)
MODULE_DESCRIPTION("MediaTek UFS Debugging Facility");
MODULE_LICENSE("GPL v2");
