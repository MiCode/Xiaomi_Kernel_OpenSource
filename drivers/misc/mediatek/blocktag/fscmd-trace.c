// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include "fake_f2fs.h"
#include <linux/f2fs_fs.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/tracepoint.h>
#include <linux/types.h>
#include <trace/events/android_fs.h>
#include <trace/events/f2fs.h>

#include "fscmd-trace.h"

#define MAX_LOG_NR       (10000) /* entries size of total log */
#define MAX_RW_LOG_NR    (2000) /* entries size of total log */
#define MAX_OTHER_LOG_NR (6000) /* entries size of total log */
#define MAX_F2FS_LOG_NR  (2000) /* entries size of total log */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
		if (!buff && !evt) { \
			pr_info(fmt, ##args); \
		} \
} while (0)

enum {
	FSCMD_SYSCALL_ENTRY = 0,
	FSCMD_SYSCALL_EXIT,
};
// fscmd_log_s -> type
enum {
	LOG_TYPE_RW = 0,
	LOG_TYPE_OTHER,
	LOG_TYPE_F2FS,
	LOG_TYPE_ENTRY,
};
/*
 * struct fscmd_log_s: log structure to store each syscall
 */
struct fscmd_log_s {
	long syscall_nr;          /*type of log*/
	char comm[TASK_COMM_LEN]; /*process name*/
	short pid;                /*current pid*/
	uint64_t time;            /*syscall ktime*/
	int type;                 /* msg of log */
	/* for checkpoint */
	int cp_reason; /*checkpoint reason*/
};
// fscmd_log_s -> msg_type
enum {
	LOG_CP_START_BLKOPS = 0, /*start block_ops*/
	LOG_CP_END_BLKOPS, /*finish block_ops*/
	LOG_CP_FINISH, /*finish checkpoint*/
	LOG_CP_TYPE_ENTRY,
};


struct fscmd_s {
	struct fscmd_log_s *trace[LOG_TYPE_ENTRY]; /* Cycle buffer in MAX_LOG_NR */
	int cout[LOG_TYPE_ENTRY]; /* the top idx we store log currently */
	int cur_idx[LOG_TYPE_ENTRY]; /* the top idx we store log currently */
};

static struct fscmd_s gHis_Log;
static DEFINE_RWLOCK(gHis_rwlock);
static atomic_t nr_dump = ATOMIC_INIT(0);

//system call we trace on
struct nr_talbe_s {
	char name[8];
	int id;
	void (*record)(struct fscmd_log_s *log);
	void (*dump)(struct nr_talbe_s *pt, char **buff, unsigned long *size,
	struct seq_file *seq, int log_type, int idx);
};

static void fscmd_add_log(struct fscmd_log_s *log);
static void syscall_dump(struct nr_talbe_s *pt, char **buff, unsigned long *size,
	struct seq_file *seq, int log_type, int idx);
static void f2fs_add_log(struct fscmd_log_s *log);
static void f2fs_dump_log(struct nr_talbe_s *pt, char **buff, unsigned long *size,
	struct seq_file *seq, int log_type, int idx);

struct nr_talbe_s NR_table[] = {
#ifdef __NR_read
	{"read", __NR_read, fscmd_add_log, syscall_dump},
#endif
#ifdef __NR_write
	{"write", __NR_write, fscmd_add_log, syscall_dump},
#endif
// to reduce log for setprop issue
// #ifdef __NR_openat2
//  {"open", __NR_openat2, fscmd_add_log, syscall_dump},
// #endif
// #ifdef __NR_close
//  {"close", __NR_close, fscmd_add_log, syscall_dump},
// #endif
// #ifdef __NR3264_lseek
//  {"lseek", __NR3264_lseek, fscmd_add_log, syscall_dump},
// #endif
#ifdef __NR_sync
	{"sync", __NR_sync, fscmd_add_log, syscall_dump},
#endif
#ifdef __NR_fsync
	{"fsync", __NR_fsync, fscmd_add_log, syscall_dump},
#endif
#ifdef __NR_fdatasync
	{"fdsync", __NR_fdatasync, fscmd_add_log, syscall_dump},
#endif
#ifdef __NR_renameat
	{"rename", __NR_renameat, fscmd_add_log, syscall_dump},
#endif
	{"none", -1, NULL, NULL},
};
enum {
	F2FS_TBLID_CK = 0,
	F2FS_TBLID_GC,
};

struct nr_talbe_s f2fs_NR_table[] = {
	{"ckpt", F2FS_TBLID_CK, f2fs_add_log, f2fs_dump_log},
	{"gc", F2FS_TBLID_GC, f2fs_add_log, f2fs_dump_log},
	{"none", -1, NULL, NULL},
};

static int ghistory_struct_init(void)
{
	int buffer_sz = 0;

	gHis_Log.cout[LOG_TYPE_RW] = MAX_RW_LOG_NR;
	gHis_Log.cout[LOG_TYPE_OTHER] = (MAX_OTHER_LOG_NR);
	gHis_Log.cout[LOG_TYPE_F2FS] = (MAX_F2FS_LOG_NR);

	buffer_sz = (gHis_Log.cout[LOG_TYPE_RW]) * sizeof(struct fscmd_log_s);
	gHis_Log.trace[LOG_TYPE_RW] = kmalloc(buffer_sz, GFP_NOFS);
	if (!gHis_Log.trace[LOG_TYPE_RW])
		return -1;

	buffer_sz = (gHis_Log.cout[LOG_TYPE_OTHER]) * sizeof(struct fscmd_log_s);
	gHis_Log.trace[LOG_TYPE_OTHER] = kmalloc(buffer_sz, GFP_NOFS);
	if (!gHis_Log.trace[LOG_TYPE_OTHER])
		return -1;

	buffer_sz = (gHis_Log.cout[LOG_TYPE_F2FS]) * sizeof(struct fscmd_log_s);
	gHis_Log.trace[LOG_TYPE_F2FS] = kmalloc(buffer_sz, GFP_NOFS);
	if (!gHis_Log.trace[LOG_TYPE_F2FS])
		return -1;

	gHis_Log.cur_idx[LOG_TYPE_OTHER] = -1;
	gHis_Log.cur_idx[LOG_TYPE_RW] = -1;
	gHis_Log.cur_idx[LOG_TYPE_F2FS] = -1;

	return 0;
}

static struct nr_talbe_s *fscmd_get_nr_table(int log_type, int target_idx)
{
	struct nr_talbe_s *pt = NULL;

	if (log_type == LOG_TYPE_RW || log_type == LOG_TYPE_OTHER)
		pt = NR_table;
	else if (log_type == LOG_TYPE_F2FS)
		pt = f2fs_NR_table;
	else
		return NULL;
	while (pt->id != -1) {
		if (target_idx == pt->id)
			return pt;
		pt++;
	}

	return NULL;
}

static void fscmd_add_log(struct fscmd_log_s *log)
{
	int next_idx = 0;
	struct fscmd_log_s *cmdlog = NULL;

	if (log->syscall_nr == __NR_read || log->syscall_nr == __NR_write) {
		next_idx = (gHis_Log.cur_idx[LOG_TYPE_RW] + 1);
		next_idx %= (gHis_Log.cout[LOG_TYPE_RW]);
		gHis_Log.cur_idx[LOG_TYPE_RW] = next_idx;
		cmdlog = gHis_Log.trace[LOG_TYPE_RW] + next_idx;
	} else {
		next_idx = (gHis_Log.cur_idx[LOG_TYPE_OTHER] + 1);
		next_idx %= (gHis_Log.cout[LOG_TYPE_OTHER]);
		gHis_Log.cur_idx[LOG_TYPE_OTHER] = next_idx;
		cmdlog = gHis_Log.trace[LOG_TYPE_OTHER] + next_idx;
	}

	cmdlog->time = log->time;
	cmdlog->type = log->type;
	cmdlog->syscall_nr = log->syscall_nr;
	cmdlog->pid = log->pid;
	get_task_comm(cmdlog->comm, current);

}

void fscmd_trace_sys_enter(void *data,
		struct pt_regs *regs, long id)
{
	struct nr_talbe_s *pt;
	unsigned long flags;
	struct fscmd_log_s syscall;

	pt = fscmd_get_nr_table(LOG_TYPE_RW, id);
	if (pt != NULL && !atomic_read(&nr_dump)) {
		write_lock_irqsave(&gHis_rwlock, flags);
		syscall.syscall_nr = id;
		syscall.time = sched_clock();
		syscall.pid = current->pid;
		syscall.type = FSCMD_SYSCALL_ENTRY;
		if (pt->record != NULL)
			pt->record(&syscall);
		write_unlock_irqrestore(&gHis_rwlock, flags);
	}
}

void fscmd_trace_sys_exit(void *data,
		struct pt_regs *regs, long ret)
{
	struct nr_talbe_s *pt = fscmd_get_nr_table(LOG_TYPE_RW, regs->syscallno);
	unsigned long flags;
	struct fscmd_log_s syscall;

	if (pt != NULL && !atomic_read(&nr_dump)) {
		write_lock_irqsave(&gHis_rwlock, flags);
		syscall.syscall_nr = regs->syscallno;
		syscall.time = sched_clock();
		syscall.pid = current->pid;
		syscall.type = FSCMD_SYSCALL_EXIT;
		if (pt->record != NULL)
			pt->record(&syscall);
		write_unlock_irqrestore(&gHis_rwlock, flags);
	}
}
static void f2fs_add_log(struct fscmd_log_s *log)
{
	int next_idx = 0;
	struct fscmd_log_s *cmdlog = NULL;

	next_idx = (gHis_Log.cur_idx[LOG_TYPE_F2FS] + 1);
	next_idx %= (gHis_Log.cout[LOG_TYPE_F2FS]);
	gHis_Log.cur_idx[LOG_TYPE_F2FS] = next_idx;
	cmdlog = gHis_Log.trace[LOG_TYPE_F2FS] + next_idx;
	cmdlog->time = log->time;
	cmdlog->type = log->type;
	cmdlog->syscall_nr = log->syscall_nr;
	cmdlog->pid = log->pid;
	get_task_comm(cmdlog->comm, current);
}

enum {
	CP_TPYE_START_BLOCK = 0,//"start block_ops"
	CP_TPYE_FINISH_BLOCK,//"finish block_ops"
	CP_TPYE_FINISH_CKPT,//"finish checkpoint"
	CP_TPYE_UNKNOWN,
};

enum {
	GC_TPYE_BEGIN = 0,
	GC_TPYE_END,
};

static void f2fs_dump_log(struct nr_talbe_s *pt, char **buff, unsigned long *size,
	struct seq_file *seq, int log_type, int idx)
{
	if (pt->id == F2FS_TBLID_CK)
		SPREAD_PRINTF(buff, size, seq,
			"%llu,%s,%d,%d,%d,%s\n",
			gHis_Log.trace[log_type][idx].time,
			pt->name,
			gHis_Log.trace[log_type][idx].type,
			gHis_Log.trace[log_type][idx].cp_reason,
			gHis_Log.trace[log_type][idx].pid,
			gHis_Log.trace[log_type][idx].comm);
	else if (pt->id == F2FS_TBLID_GC)
		SPREAD_PRINTF(buff, size, seq,
			"%llu,%s,%d,%d,%s\n",
			gHis_Log.trace[log_type][idx].time,
			pt->name,
			(gHis_Log.trace[log_type][idx].type == GC_TPYE_BEGIN),
			gHis_Log.trace[log_type][idx].pid,
			gHis_Log.trace[log_type][idx].comm);
}


void fscmd_trace_f2fs_write_checkpoint(void *data,
	struct super_block *sb, int reason, char *msg)
{
	struct nr_talbe_s *pt;
	unsigned long flags;
	struct fscmd_log_s syscall;

	pt = fscmd_get_nr_table(LOG_TYPE_F2FS, F2FS_TBLID_CK);
	if (pt != NULL && !atomic_read(&nr_dump)) {
		write_lock_irqsave(&gHis_rwlock, flags);
		syscall.syscall_nr = F2FS_TBLID_CK;
		syscall.time = sched_clock();
		syscall.pid = current->pid;
		if (!strncmp(msg, "start block_ops", 10*sizeof(char)))
			syscall.type = CP_TPYE_START_BLOCK; // for different ckpt step
		else if (!strncmp(msg, "finish block_ops", 10*sizeof(char)))
			syscall.type = CP_TPYE_FINISH_BLOCK; // for different ckpt step
		else if (!strncmp(msg, "finish checkpoint", 10*sizeof(char)))
			syscall.type = CP_TPYE_FINISH_CKPT; // for different ckpt step
		else
			syscall.type = CP_TPYE_UNKNOWN;

		syscall.cp_reason = reason;
		if (pt->record != NULL)
			pt->record(&syscall);
		write_unlock_irqrestore(&gHis_rwlock, flags);
	}
}

void fscmd_trace_f2fs_gc_begin(void *data,
		struct super_block *sb, bool sync, bool background,
		long long dirty_nodes, long long dirty_dents,
		long long dirty_imeta, unsigned int free_sec,
		unsigned int free_seg, int reserved_seg,
		unsigned int prefree_seg)
{
	struct nr_talbe_s *pt;
	unsigned long flags;
	struct fscmd_log_s syscall;

	pt = fscmd_get_nr_table(LOG_TYPE_F2FS, F2FS_TBLID_GC);
	if (pt != NULL && !atomic_read(&nr_dump)) {
		write_lock_irqsave(&gHis_rwlock, flags);
		syscall.syscall_nr = F2FS_TBLID_GC;
		syscall.time = sched_clock();
		syscall.pid = current->pid;
		syscall.type = GC_TPYE_BEGIN; // for different ckpt step

		if (pt->record != NULL)
			pt->record(&syscall);
		write_unlock_irqrestore(&gHis_rwlock, flags);
	}
}

void fscmd_trace_f2fs_gc_end(void *data,
	struct super_block *sb, int ret, int seg_freed,
	int sec_freed, long long dirty_nodes,
	long long dirty_dents, long long dirty_imeta,
	unsigned int free_sec, unsigned int free_seg,
	int reserved_seg, unsigned int prefree_seg)
{
	struct nr_talbe_s *pt;
	unsigned long flags;
	struct fscmd_log_s syscall;

	pt = fscmd_get_nr_table(LOG_TYPE_F2FS, F2FS_TBLID_GC);
	if (pt != NULL && !atomic_read(&nr_dump)) {
		write_lock_irqsave(&gHis_rwlock, flags);
		syscall.syscall_nr = F2FS_TBLID_GC;
		syscall.time = sched_clock();
		syscall.pid = current->pid;
		syscall.type = GC_TPYE_END; // for different ckpt step

		if (pt->record != NULL)
			pt->record(&syscall);
		write_unlock_irqrestore(&gHis_rwlock, flags);
	}
}

static void syscall_dump(struct nr_talbe_s *pt, char **buff, unsigned long *size,
	struct seq_file *seq, int log_type, int idx)
{
	SPREAD_PRINTF(buff, size, seq,
		"%llu,%s,%s,%d,%s\n",
		gHis_Log.trace[log_type][idx].time,
		(gHis_Log.trace[log_type][idx].type == FSCMD_SYSCALL_ENTRY)?"i":"o",
		pt->name,
		gHis_Log.trace[log_type][idx].pid,
		gHis_Log.trace[log_type][idx].comm);
}

void mtk_fscmd_show(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	int i, idx, log_type;
	struct nr_talbe_s *pt = NULL;
	unsigned long flags;
	// start dump the farthest log in ring-buffer
	atomic_inc(&nr_dump);
	read_lock_irqsave(&gHis_rwlock, flags);
	SPREAD_PRINTF(buff, size, seq,
		"time,entry/exit,syscall,pid,func,\n");
	for (log_type = 0; log_type < LOG_TYPE_ENTRY; log_type++) {
		for (i = 0, idx = 0; i < (gHis_Log.cout[log_type]); i++) {
			pt = fscmd_get_nr_table(log_type, gHis_Log.trace[log_type][idx].syscall_nr);
			idx = (gHis_Log.cur_idx[log_type] + i + 1) % (gHis_Log.cout[log_type]);
			if (pt->dump != NULL)
				pt->dump(pt, buff, size, seq, log_type, idx);
		}
	}

	read_unlock_irqrestore(&gHis_rwlock, flags);
	atomic_dec(&nr_dump);
}

int mtk_fscmd_init(void)
{
	return ghistory_struct_init();
}
