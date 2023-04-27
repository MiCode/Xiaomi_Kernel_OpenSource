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
	if (evt) { \
		seq_printf(evt, fmt, ##args); \
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
	int cp_reason;            /*checkpoint reason*/
};

struct fscmd_s {
	struct fscmd_log_s *trace;
	int max_count; /* the max entries */
	int cur_idx; /* the top idx we store log currently */
	struct nr_table_s *pt;
	int initial;
	void (*record)(struct fscmd_s *fscmd_log, struct fscmd_log_s *log);
	void (*dump)(struct fscmd_s *fscmd_log, char **buff, unsigned long *size,
		struct seq_file *seq, struct fscmd_log_s *log);
};

static struct fscmd_s ghistory_log[LOG_TYPE_ENTRY];
static spinlock_t ghis_spinlock;

//system call we trace on
struct nr_table_s {
	char name[8];
	int id;
};

static void fscmd_add_log(struct fscmd_s *fscmd_log, struct fscmd_log_s *log);
static void syscall_dump(struct fscmd_s *fscmd_log, char **buff, unsigned long *size,
	struct seq_file *seq, struct fscmd_log_s *log);
static void f2fs_add_log(struct fscmd_s *fscmd_log, struct fscmd_log_s *log);
static void f2fs_dump_log(struct fscmd_s *fscmd_log, char **buff, unsigned long *size,
	struct seq_file *seq, struct fscmd_log_s *log);

struct nr_table_s NR_rw_table[] = {
#ifdef __NR_read
	{"read", __NR_read},
#endif
#ifdef __NR_write
	{"write", __NR_write},
#endif
	{"none", -1},
};

struct nr_table_s NR_other_table[] = {
// to reduce log for setprop issue
// #ifdef __NR_openat2
//  {"open", __NR_openat2},
// #endif
// #ifdef __NR_close
//  {"close", __NR_close},
// #endif
// #ifdef __NR3264_lseek
//  {"lseek", __NR3264_lseek},
// #endif
#ifdef __NR_sync
	{"sync", __NR_sync},
#endif
#ifdef __NR_fsync
	{"fsync", __NR_fsync},
#endif
#ifdef __NR_fdatasync
	{"fdsync", __NR_fdatasync},
#endif
#ifdef __NR_renameat
	{"rename", __NR_renameat},
#endif
	{"none", -1},
};

enum {
	F2FS_TBLID_CK = 0,
	F2FS_TBLID_GC,
};

struct nr_table_s NR_f2fs_table[] = {
	{"ckpt", F2FS_TBLID_CK},
	{"gc", F2FS_TBLID_GC},
	{"none", -1},
};

static int ghistory_struct_init(void)
{
	int buffer_sz = 0;

	spin_lock_init(&ghis_spinlock);
	// init LOG_TYPE_RW history log
	ghistory_log[LOG_TYPE_RW].max_count = MAX_RW_LOG_NR;
	buffer_sz = (ghistory_log[LOG_TYPE_RW].max_count) * sizeof(struct fscmd_log_s);
	ghistory_log[LOG_TYPE_RW].trace = kmalloc(buffer_sz, GFP_NOFS);
	if (!ghistory_log[LOG_TYPE_RW].trace)
		return -1;

	ghistory_log[LOG_TYPE_RW].cur_idx = -1;
	ghistory_log[LOG_TYPE_RW].record = fscmd_add_log;
	ghistory_log[LOG_TYPE_RW].dump = syscall_dump;
	ghistory_log[LOG_TYPE_RW].pt = NR_rw_table;
	memset(ghistory_log[LOG_TYPE_RW].trace, 0x0, buffer_sz);
	ghistory_log[LOG_TYPE_RW].initial = 1;
	// init LOG_TYPE_OTHER history log
	ghistory_log[LOG_TYPE_OTHER].max_count = MAX_OTHER_LOG_NR;
	buffer_sz = (ghistory_log[LOG_TYPE_OTHER].max_count) * sizeof(struct fscmd_log_s);
	ghistory_log[LOG_TYPE_OTHER].trace = kmalloc(buffer_sz, GFP_NOFS);
	if (!ghistory_log[LOG_TYPE_OTHER].trace)
		return -1;

	ghistory_log[LOG_TYPE_OTHER].cur_idx = -1;
	ghistory_log[LOG_TYPE_OTHER].record = fscmd_add_log;
	ghistory_log[LOG_TYPE_OTHER].dump = syscall_dump;
	ghistory_log[LOG_TYPE_OTHER].pt = NR_other_table;
	memset(ghistory_log[LOG_TYPE_OTHER].trace, 0x0, buffer_sz);
	ghistory_log[LOG_TYPE_OTHER].initial = 1;
	// init LOG_TYPE_F2FS history log
	ghistory_log[LOG_TYPE_F2FS].max_count = MAX_F2FS_LOG_NR;
	buffer_sz = (ghistory_log[LOG_TYPE_F2FS].max_count) * sizeof(struct fscmd_log_s);
	ghistory_log[LOG_TYPE_F2FS].trace = kmalloc(buffer_sz, GFP_NOFS);
	if (!ghistory_log[LOG_TYPE_F2FS].trace)
		return -1;

	ghistory_log[LOG_TYPE_F2FS].cur_idx = -1;
	ghistory_log[LOG_TYPE_F2FS].record = f2fs_add_log;
	ghistory_log[LOG_TYPE_F2FS].dump = f2fs_dump_log;
	ghistory_log[LOG_TYPE_F2FS].pt = NR_f2fs_table;
	memset(ghistory_log[LOG_TYPE_F2FS].trace, 0x0, buffer_sz);
	ghistory_log[LOG_TYPE_F2FS].initial = 1;
	return 0;
}

static struct nr_table_s *check_nr_table(struct nr_table_s *pt, int target_idx)
{
	if (!pt)
		return NULL;

	while (pt->id != -1) {
		if (target_idx == pt->id)
			return pt;
		pt++;
	}

	return NULL;
}

static void fscmd_add_log(struct fscmd_s *fscmd_log, struct fscmd_log_s *log)
{
	int next_idx = 0;
	struct fscmd_log_s *cmdlog = NULL;
	unsigned long flags;

	if (!fscmd_log || !log || !fscmd_log->initial)
		return;

	if (!check_nr_table(fscmd_log->pt, log->syscall_nr))
		return;

	if (!spin_trylock_irqsave(&ghis_spinlock, flags))
		return;

	next_idx = fscmd_log->cur_idx + 1;
	next_idx = (next_idx == fscmd_log->max_count) ? 0 : next_idx;
	fscmd_log->cur_idx = next_idx;
	cmdlog = fscmd_log->trace + next_idx;
	cmdlog->time = log->time;
	cmdlog->type = log->type;
	cmdlog->syscall_nr = log->syscall_nr;
	cmdlog->pid = log->pid;
	get_task_comm(cmdlog->comm, current);
	spin_unlock_irqrestore(&ghis_spinlock, flags);

}

static void syscall_dump(struct fscmd_s *fscmd_log, char **buff, unsigned long *size,
	struct seq_file *seq, struct fscmd_log_s *log)
{
	struct nr_table_s *pt = NULL;

	if (!fscmd_log || !log || !fscmd_log->initial)
		return;

	pt = check_nr_table(fscmd_log->pt, log->syscall_nr);
	if (!pt)
		return;

	SPREAD_PRINTF(buff, size, seq,
		"%llu,%s,%s,%d,%s\n",
		log->time,
		(log->type == FSCMD_SYSCALL_ENTRY)?"i":"o",
		pt->name,
		log->pid,
		log->comm);
}

void fscmd_trace_sys_enter(void *data,
		struct pt_regs *regs, long id)
{
	struct fscmd_log_s syscall;

	syscall.syscall_nr = id;
	syscall.time = sched_clock();
	syscall.pid = current->pid;
	syscall.type = FSCMD_SYSCALL_ENTRY;
	if (!ghistory_log[LOG_TYPE_RW].record || !ghistory_log[LOG_TYPE_OTHER].record)
		return;

	if (id == __NR_read || id == __NR_write)
		ghistory_log[LOG_TYPE_RW].record(&ghistory_log[LOG_TYPE_RW], &syscall);
	else
		ghistory_log[LOG_TYPE_OTHER].record(&ghistory_log[LOG_TYPE_OTHER], &syscall);
}

void fscmd_trace_sys_exit(void *data,
		struct pt_regs *regs, long ret)
{
	struct fscmd_log_s syscall;

	syscall.syscall_nr = regs->syscallno;
	syscall.time = sched_clock();
	syscall.pid = current->pid;
	syscall.type = FSCMD_SYSCALL_EXIT;
	if (!ghistory_log[LOG_TYPE_RW].record || !ghistory_log[LOG_TYPE_OTHER].record)
		return;

	if (syscall.pid == __NR_read || syscall.pid == __NR_write)
		ghistory_log[LOG_TYPE_RW].record(&ghistory_log[LOG_TYPE_RW], &syscall);
	else
		ghistory_log[LOG_TYPE_OTHER].record(&ghistory_log[LOG_TYPE_OTHER], &syscall);
}

static void f2fs_add_log(struct fscmd_s *fscmd_log, struct fscmd_log_s *log)
{
	int next_idx = 0;
	struct fscmd_log_s *cmdlog = NULL;
	unsigned long flags;

	if (!fscmd_log || !log || !fscmd_log->initial)
		return;

	if (!check_nr_table(fscmd_log->pt, log->syscall_nr))
		return;

	if (!spin_trylock_irqsave(&ghis_spinlock, flags))
		return;

	next_idx = fscmd_log->cur_idx + 1;
	next_idx = (next_idx == fscmd_log->max_count) ? 0 : next_idx;
	fscmd_log->cur_idx = next_idx;
	cmdlog = fscmd_log->trace + next_idx;
	cmdlog->time = log->time;
	cmdlog->type = log->type;
	cmdlog->syscall_nr = log->syscall_nr;
	cmdlog->pid = log->pid;
	cmdlog->cp_reason = log->cp_reason;
	get_task_comm(cmdlog->comm, current);
	spin_unlock_irqrestore(&ghis_spinlock, flags);
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

static void f2fs_dump_log(struct fscmd_s *fscmd_log, char **buff, unsigned long *size,
	struct seq_file *seq, struct fscmd_log_s *log)
{
	struct nr_table_s *pt = NULL;

	if (!fscmd_log || !log || !fscmd_log->initial)
		return;

	pt = check_nr_table(fscmd_log->pt, log->syscall_nr);
	if (!pt)
		return;

	if (pt->id == F2FS_TBLID_CK) {
		SPREAD_PRINTF(buff, size, seq,
			"%llu,%s,%d,%d,%d,%s\n",
			log->time,
			pt->name,
			log->type,
			log->cp_reason,
			log->pid,
			log->comm);
	} else if (pt->id == F2FS_TBLID_GC) {
		SPREAD_PRINTF(buff, size, seq,
			"%llu,%s,%d,%d,%s\n",
			log->time,
			pt->name,
			log->type,
			log->pid,
			log->comm);
	}
}

void fscmd_trace_f2fs_write_checkpoint(void *data,
	struct super_block *sb, int reason, char *msg)
{
	struct fscmd_log_s syscall;

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
	ghistory_log[LOG_TYPE_F2FS].record(&ghistory_log[LOG_TYPE_F2FS], &syscall);
}

void fscmd_trace_f2fs_gc_begin(void *data,
		struct super_block *sb, bool sync, bool background,
		long long dirty_nodes, long long dirty_dents,
		long long dirty_imeta, unsigned int free_sec,
		unsigned int free_seg, int reserved_seg,
		unsigned int prefree_seg)
{
	struct fscmd_log_s syscall;

	syscall.syscall_nr = F2FS_TBLID_GC;
	syscall.time = sched_clock();
	syscall.pid = current->pid;
	syscall.type = GC_TPYE_BEGIN; // for different ckpt step
	ghistory_log[LOG_TYPE_F2FS].record(&ghistory_log[LOG_TYPE_F2FS], &syscall);

}

void fscmd_trace_f2fs_gc_end(void *data,
	struct super_block *sb, int ret, int seg_freed,
	int sec_freed, long long dirty_nodes,
	long long dirty_dents, long long dirty_imeta,
	unsigned int free_sec, unsigned int free_seg,
	int reserved_seg, unsigned int prefree_seg)
{
	struct fscmd_log_s syscall;

	syscall.syscall_nr = F2FS_TBLID_GC;
	syscall.time = sched_clock();
	syscall.pid = current->pid;
	syscall.type = GC_TPYE_END; // for different ckpt step
	ghistory_log[LOG_TYPE_F2FS].record(&ghistory_log[LOG_TYPE_F2FS], &syscall);
}

void mtk_fscmd_show(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	int i, idx, log_type;
	unsigned long flags;

	// start dump the farthest log in ring-buffer
	spin_lock_irqsave(&ghis_spinlock, flags);
	SPREAD_PRINTF(buff, size, seq,
		"time,entry/exit,syscall,pid,func,\n");
	for (log_type = 0; log_type < LOG_TYPE_ENTRY; log_type++) {
		for (i = 0, idx = 0; i < (ghistory_log[log_type].max_count); i++) {
			idx = (ghistory_log[log_type].cur_idx + i + 1);
			idx = idx % (ghistory_log[log_type].max_count);
			ghistory_log[log_type].dump(&ghistory_log[log_type],
				buff, size, seq, ghistory_log[log_type].trace + idx);
		}
	}
	spin_unlock_irqrestore(&ghis_spinlock, flags);
}

int mtk_fscmd_init(void)
{
	return ghistory_struct_init();
}
