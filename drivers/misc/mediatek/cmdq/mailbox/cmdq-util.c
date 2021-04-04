// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/arm-smccc.h>
#include <mt-plat/mtk_secure_api.h>

#include "cmdq-util.h"

#ifdef CMDQ_SECURE_SUPPORT
#include "cmdq-sec-mailbox.h"
#endif

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif
#ifdef CONFIG_MTK_DEVAPC
#include <devapc_public.h>
#endif

#define CMDQ_MBOX_NUM			2
#define CMDQ_HW_MAX			2
#define CMDQ_RECORD_NUM			512
#define CMDQ_FIRST_ERR_SIZE		524288	/* 512k */

#define CMDQ_CURR_IRQ_STATUS		0x10
#define CMDQ_CURR_LOADED_THR		0x18
#define CMDQ_THR_EXEC_CYCLES		0x34
#define CMDQ_THR_TIMEOUT_TIMER		0x38

#define GCE_DBG_CTL			0x3000
#define GCE_DBG0			0x3004
#define GCE_DBG2			0x300C
#define GCE_DBG3			0x3010

#define util_time_to_us(start, end, duration)	\
{	\
	u64 _duration = end - start;	\
	do_div(_duration, 1000);	\
	duration = (s32)_duration;	\
}

struct cmdq_util_error {
	spinlock_t	lock;
	bool		enable;
	char		*buffer;
	u32		length;
	u64		nsec;
	struct timeval	errtm;
	char		caller[TASK_COMM_LEN]; // TODO
};

struct cmdq_util_dentry {
	struct dentry	*status;
	struct dentry	*record;
	struct dentry	*log_feature;
	u8		bit_feature;
};

struct cmdq_record {
	unsigned long pkt;
	s32 priority;	/* task priority (not thread priority) */
	s8 id;
	s32 thread;	/* allocated thread */
	s32 reorder;
	u32 size;
	bool is_secure;	/* true for secure task */

	u64 submit;	/* epoch time of IOCTL/Kernel API call */
	u64 trigger;	/* epoch time of enable HW thread */
	/* epoch time of start waiting for task completion */
	u64 wait;
	u64 irq;	/* epoch time of IRQ event */
	u64 done;	/* epoch time of sw leaving wait and task finish */

	unsigned long start;	/* buffer start address */
	unsigned long end;	/* command end address */
	u64 last_inst;	/* last instruction, jump addr */

	u32 exec_begin;	/* task execute time in hardware thread */
	u32 exec_end;	/* task execute time in hardware thread */
};

struct cmdq_util {
	struct cmdq_util_error	err;
	struct cmdq_util_dentry	fs;
	struct cmdq_record record[CMDQ_RECORD_NUM];
	u16 record_idx;
	void *cmdq_mbox[CMDQ_MBOX_NUM];
	void *cmdq_sec_mbox[CMDQ_MBOX_NUM];
	u32 mbox_cnt;
	u32 mbox_sec_cnt;
	const char *first_err_mod[CMDQ_HW_MAX];
};
static struct cmdq_util	util;

static DEFINE_MUTEX(cmdq_record_mutex);
static DEFINE_MUTEX(cmdq_dump_mutex);

u32 cmdq_util_get_bit_feature(void)
{
	return util.fs.bit_feature;
}

bool cmdq_util_is_feature_en(u8 feature)
{
	return (util.fs.bit_feature & BIT(feature)) != 0;
}

void cmdq_util_error_enable(void)
{
	if (!util.err.nsec) {
		util.err.nsec = sched_clock();
		do_gettimeofday(&util.err.errtm);
	}
	util.err.enable = true;
}
EXPORT_SYMBOL(cmdq_util_error_enable);

void cmdq_util_error_disable(void)
{
	util.err.enable = false;
}
EXPORT_SYMBOL(cmdq_util_error_disable);

void cmdq_util_dump_lock(void)
{
	mutex_lock(&cmdq_dump_mutex);
}
EXPORT_SYMBOL(cmdq_util_dump_lock);

void cmdq_util_dump_unlock(void)
{
	mutex_unlock(&cmdq_dump_mutex);
}
EXPORT_SYMBOL(cmdq_util_dump_unlock);

s32 cmdq_util_error_save_lst(const char *format, va_list args)
{
	unsigned long flags;
	s32 size;

	if (!util.err.enable || !util.err.buffer)
		return -EFAULT;

	spin_lock_irqsave(&util.err.lock, flags);
	size = vsnprintf(util.err.buffer + util.err.length,
		CMDQ_FIRST_ERR_SIZE - util.err.length, format, args);
	if (size >= CMDQ_FIRST_ERR_SIZE - util.err.length)
		cmdq_log("size:%d over buf size:%d",
			size, CMDQ_FIRST_ERR_SIZE - util.err.length);
	util.err.length += size;
	spin_unlock_irqrestore(&util.err.lock, flags);

	if (util.err.length >= CMDQ_FIRST_ERR_SIZE) {
		cmdq_util_error_disable();
		cmdq_err("util.err.length:%u is over CMDQ_FIRST_ERR_SIZE:%u",
			util.err.length, CMDQ_FIRST_ERR_SIZE);
	}
	return 0;
}
EXPORT_SYMBOL(cmdq_util_error_save_lst);

s32 cmdq_util_error_save(const char *format, ...)
{
	va_list args;

	if (!util.err.enable || !util.err.buffer)
		return -EFAULT;

	va_start(args, format);
	cmdq_util_error_save_lst(format, args);
	va_end(args);
	return 0;
}
EXPORT_SYMBOL(cmdq_util_error_save);

static int cmdq_util_status_print(struct seq_file *seq, void *data)
{
	u64		sec = util.err.nsec;
	unsigned long	nsec = do_div(sec, 1000000000);
	struct tm	nowtm;
	u32		i;

	if (util.err.length) {
		time_to_tm(util.err.errtm.tv_sec, sys_tz.tz_minuteswest * 60,
			&nowtm);

		seq_printf(seq,
			"[cmdq] first error kernel time:[%5llu.%06lu] UTC time:[%04ld-%02d-%02d %02d:%02d:%02d.%06ld]\n",
			sec, nsec,
			nowtm.tm_year + 1900, nowtm.tm_mon + 1,
			nowtm.tm_mday, nowtm.tm_hour, nowtm.tm_min,
			nowtm.tm_sec, util.err.errtm.tv_usec);
		seq_printf(seq, "%s", util.err.buffer);
	}

	seq_puts(seq, "[cmdq] dump all thread current status\n");
	for (i = 0; i < util.mbox_cnt; i++)
		cmdq_thread_dump_all_seq(util.cmdq_mbox[i], seq);

	return 0;
}

static int cmdq_util_record_print(struct seq_file *seq, void *data)
{
	struct cmdq_record *rec;
	u32 acq_time, irq_time, begin_wait, exec_time, total_time, hw_time;
	u64 submit_sec;
	unsigned long submit_rem, hw_time_rem;
	s32 i, idx;

	mutex_lock(&cmdq_record_mutex);

	seq_puts(seq, "index,pkt,task priority,sec,size,gce,thread,");
	seq_puts(seq,
		"submit,acq_time(us),irq_time(us),begin_wait(us),exec_time(us),total_time(us),start,end,jump,");
	seq_puts(seq, "exec begin,exec end,hw_time(us),\n");

	idx = util.record_idx;
	for (i = 0; i < ARRAY_SIZE(util.record); i++) {
		idx--;
		if (idx < 0)
			idx = ARRAY_SIZE(util.record) - 1;

		rec = &util.record[idx];
		if (!rec->pkt)
			continue;

		seq_printf(seq, "%u,%#lx,%d,%d,%u,%hhd,%d,",
			idx, rec->pkt, rec->priority, (int)rec->is_secure,
			rec->size, rec->id, rec->thread);

		submit_sec = rec->submit;
		submit_rem = do_div(submit_sec, 1000000000);

		util_time_to_us(rec->submit, rec->trigger, acq_time);
		util_time_to_us(rec->trigger, rec->irq, irq_time);
		util_time_to_us(rec->submit, rec->wait, begin_wait);
		util_time_to_us(rec->trigger, rec->done, exec_time);
		util_time_to_us(rec->submit, rec->done, total_time);
		seq_printf(seq,
			"%llu.%06lu,%u,%u,%u,%u,%u,%#lx,%#lx,%#llx,",
			submit_sec, submit_rem / 1000, acq_time, irq_time,
			begin_wait, exec_time, total_time,
			rec->start, rec->end, rec->last_inst);

		hw_time = rec->exec_end > rec->exec_begin ?
			rec->exec_end - rec->exec_begin :
			~rec->exec_begin + 1 + rec->exec_end;
		hw_time_rem = (u32)CMDQ_TICK_TO_US(hw_time);

		seq_printf(seq, "%u,%u,%u.%06lu,\n",
			rec->exec_begin, rec->exec_end, hw_time, hw_time_rem);
	}

	mutex_unlock(&cmdq_record_mutex);

	return 0;
}

static int cmdq_util_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_status_print, inode->i_private);
}

static int cmdq_util_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_record_print, inode->i_private);
}

static const struct file_operations cmdq_util_status_fops = {
	.owner = THIS_MODULE,
	.open = cmdq_util_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations cmdq_util_record_fops = {
	.owner = THIS_MODULE,
	.open = cmdq_util_record_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int cmdq_util_log_feature_get(void *data, u64 *val)
{
	cmdq_msg("data:%p val:%#llx bit_feature:%#x",
		data, *val, util.fs.bit_feature);
	return util.fs.bit_feature;
}

static int cmdq_util_log_feature_set(void *data, u64 val)
{
	if (val == CMDQ_LOG_FEAT_NUM) {
		util.fs.bit_feature = 0;
		cmdq_msg("data:%p val:%#llx bit_feature:%#x reset",
			data, val, util.fs.bit_feature);
		return 0;
	}

	if (val >= CMDQ_LOG_FEAT_NUM) {
		cmdq_err("data:%p val:%#llx cannot be over %#x",
			data, val, CMDQ_LOG_FEAT_NUM);
		return -EINVAL;
	}

	util.fs.bit_feature |= (1 << val);
	cmdq_msg("data:%p val:%#llx bit_feature:%#x",
		data, val, util.fs.bit_feature);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cmdq_util_log_feature_fops,
	cmdq_util_log_feature_get, cmdq_util_log_feature_set, "%llu");

/* sync with request in atf */
enum cmdq_smc_request {
	CMDQ_ENABLE_DEBUG,
};

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static atomic_t cmdq_dbg_ctrl = ATOMIC_INIT(0);
#endif

void cmdq_util_dump_dbg_reg(void *chan)
{
	void *base = cmdq_mbox_get_base(chan);
	u32 dbg0[3], dbg2[6], dbg3, i;
	u32 id;

	if (!base) {
		cmdq_util_msg("no cmdq dbg since no base");
		return;
	}

	id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	if (atomic_cmpxchg(&cmdq_dbg_ctrl, 0, 1) == 0) {
		struct arm_smccc_res res;

		arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, CMDQ_ENABLE_DEBUG, id,
			0, 0, 0, 0, 0, &res);
	}
#endif

	/* debug select */
	for (i = 0; i < 6; i++) {
		if (i < 3) {
			writel((i << 8) | i, base + GCE_DBG_CTL);
			dbg0[i] = readl(base + GCE_DBG0);
		} else {
			/* only other part */
			writel(i << 8, base + GCE_DBG_CTL);
		}
		dbg2[i] = readl(base + GCE_DBG2);
	}

	dbg3 = readl(base + GCE_DBG3);

	cmdq_util_user_msg(chan,
		"id:%u dbg0:%#x %#x %#x dbg2:%#x %#x %#x %#x %#x %#x dbg3:%#x",
		id,
		dbg0[0], dbg0[1], dbg0[2],
		dbg2[0], dbg2[1], dbg2[2], dbg2[3], dbg2[4], dbg2[5],
		dbg3);
}

void cmdq_util_track(struct cmdq_pkt *pkt)
{
	struct cmdq_record *record;
	struct cmdq_client *cl = pkt->cl;
	struct cmdq_pkt_buffer *buf;
	u64 done = sched_clock();
	u32 offset, *perf;

	mutex_lock(&cmdq_record_mutex);

	record = &util.record[util.record_idx++];
	record->pkt = (unsigned long)pkt;
	record->priority = pkt->priority;
	record->size = pkt->cmd_buf_size;

	record->submit = pkt->rec_submit;
	record->trigger = pkt->rec_trigger;
	record->wait = pkt->rec_wait;
	record->irq = pkt->rec_irq;
	record->done = done;

	if (cl && cl->chan) {
		record->thread = cmdq_mbox_chan_id(cl->chan);
		record->id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(
			cl->chan));
	} else {
		record->thread = -1;
		record->id = -1;
	}

#ifdef CMDQ_SECURE_SUPPORT
	if (pkt->sec_data)
		record->is_secure = true;
#endif

	if (util.record_idx >= CMDQ_RECORD_NUM)
		util.record_idx = 0;

	if (!list_empty(&pkt->buf)) {
		buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
		record->start = buf->pa_base;

		buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
		offset = CMDQ_CMD_BUFFER_SIZE - (pkt->buf_size -
			pkt->cmd_buf_size);
		record->end = buf->pa_base + offset;
		record->last_inst = *(u64 *)(buf->va_base + offset -
			CMDQ_INST_SIZE);

		perf = cmdq_pkt_get_perf_ret(pkt);
		if (perf) {
			record->exec_begin = perf[0];
			record->exec_end = perf[1];
		}
	}

	mutex_unlock(&cmdq_record_mutex);
}

void cmdq_util_dump_smi(void)
{
#if defined(CONFIG_MTK_SMI_EXT) && !defined(CONFIG_FPGA_EARLY_PORTING) && \
	!defined(CONFIG_MTK_SMI_VARIANT)
	int smi_hang;

	smi_hang = smi_debug_bus_hang_detect(1, "CMDQ");
	cmdq_util_err("smi hang:%d", smi_hang);
#else
	cmdq_util_err("[WARNING]not enable SMI dump now");
#endif
}

#ifdef CONFIG_MTK_DEVAPC
static void cmdq_util_handle_devapc_vio(void)
{
	u32 i;

	cmdq_util_msg("%s mbox cnt:%u", __func__, util.mbox_cnt);
	for (i = 0; i < util.mbox_cnt; i++)
		cmdq_thread_dump_all(util.cmdq_mbox[i]);

#ifdef CMDQ_SECURE_SUPPORT
	cmdq_util_msg("%s mbox sec cnt:%u", __func__, util.mbox_sec_cnt);
	for (i = 0; i < util.mbox_sec_cnt; i++)
		cmdq_sec_dump_thread_all(util.cmdq_sec_mbox[i]);
#endif
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = INFRA_SUBSYS_GCE,
	.debug_dump = cmdq_util_handle_devapc_vio,
};
#endif

u8 cmdq_util_track_ctrl(void *cmdq, phys_addr_t base, bool sec)
{
	cmdq_msg("%s cmdq:%p sec:%s", __func__, cmdq, sec ? "true" : "false");
	if (sec)
		util.cmdq_sec_mbox[util.mbox_sec_cnt++] = cmdq;
	else
		util.cmdq_mbox[util.mbox_cnt++] = cmdq;

	return (u8)cmdq_util_hw_id((u32)base);
}

void cmdq_util_set_first_err_mod(void *chan, const char *mod)
{
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	util.first_err_mod[hw_id] = mod;
}

const char *cmdq_util_get_first_err_mod(void *chan)
{
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	return util.first_err_mod[hw_id];
}

static int __init cmdq_util_init(void)
{
	struct dentry	*dir;
	bool exists = false;

	cmdq_msg("%s begin", __func__);

	spin_lock_init(&util.err.lock);
	util.err.buffer = vzalloc(CMDQ_FIRST_ERR_SIZE);
	if (!util.err.buffer)
		return -ENOMEM;

	dir = debugfs_lookup("cmdq", NULL);
	if (!dir) {
		dir = debugfs_create_dir("cmdq", NULL);
		if (!dir) {
			cmdq_err("debugfs_create_dir cmdq failed");
			return -EINVAL;
		}
	} else
		exists = true;

	util.fs.status = debugfs_create_file(
		"cmdq-status", 0444, dir, &util, &cmdq_util_status_fops);
	if (IS_ERR(util.fs.status)) {
		cmdq_err("debugfs_create_file cmdq-status failed:%ld",
			PTR_ERR(util.fs.status));
		return PTR_ERR(util.fs.status);
	}

	util.fs.record = debugfs_create_file(
		"cmdq-record", 0444, dir, &util, &cmdq_util_record_fops);
	if (IS_ERR(util.fs.record)) {
		cmdq_err("debugfs_create_file cmdq-record failed:%ld",
			PTR_ERR(util.fs.record));
		return PTR_ERR(util.fs.record);
	}

	util.fs.log_feature = debugfs_create_file("cmdq-log-feature",
		0444, dir, &util, &cmdq_util_log_feature_fops);
	if (IS_ERR(util.fs.log_feature)) {
		cmdq_err("debugfs_create_file cmdq-log-feature failed:%ld",
			PTR_ERR(util.fs.log_feature));
		return PTR_ERR(util.fs.log_feature);
	}

	if (exists)
		dput(dir);

	cmdq_util_log_feature_set(NULL, CMDQ_LOG_FEAT_PERF);

#ifdef CONFIG_MTK_DEVAPC
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	cmdq_msg("%s end", __func__);

	return 0;
}
late_initcall(cmdq_util_init);
