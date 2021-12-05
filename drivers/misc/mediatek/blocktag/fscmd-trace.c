// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/tracepoint.h>
#include <linux/types.h>
#include <trace/events/android_fs.h>

#include "fscmd-trace.h"

#define MAX_LOG_NR (10000) /* entries size of total log */
#define MAX_RW_LOG_NR (6000) /* entries size of total log */
#define LOG_LIST_MAX (10) /* the buffer for log list */
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
/*
 * struct fscmd_log_s: log structure to store each syscall
 */
struct fscmd_log_s {
	long syscall_nr;          /*number of syscall*/
	char comm[TASK_COMM_LEN]; /*process name*/
	short pid;                /*current pid*/
	uint64_t time;          /*syscall ktime*/
	int type;
};

enum {
	LOG_TYPE_RW = 0,
	LOG_TYPE_OTHER,
	LOG_TYPE_ENTRY,
};

struct fscmd_s {
	struct fscmd_log_s *trace[LOG_TYPE_ENTRY]; /* Cycle buffer in MAX_LOG_NR */
	int cout[LOG_TYPE_ENTRY]; /* the top idx we store log currently */
	int cur_idx[LOG_TYPE_ENTRY]; /* the top idx we store log currently */
};

static struct fscmd_s gHis_Log;
static spinlock_t gHis_lock;
static int dump_bit; /* the top idx we store log currently */
//system call we trace on
struct nr_talbe_s {
	char name[8];
	int id;
} NR_table[] = {
#ifdef __NR_read
	{"read", __NR_read},
#endif
#ifdef __NR_write
	{"write", __NR_write},
#endif
#ifdef __NR_openat2
	{"open", __NR_openat2},
#endif
#ifdef __NR_close
	{"close", __NR_close},
#endif
#ifdef __NR3264_lseek
	{"lseek", __NR3264_lseek},
#endif
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
static int ghistory_struct_init(void)
{
	int buffer_sz = 0;

	gHis_Log.cout[LOG_TYPE_RW] = MAX_RW_LOG_NR;
	gHis_Log.cout[LOG_TYPE_OTHER] = (MAX_LOG_NR - MAX_RW_LOG_NR);

	buffer_sz = (gHis_Log.cout[LOG_TYPE_RW]) * sizeof(struct fscmd_log_s);
	gHis_Log.trace[LOG_TYPE_RW] = kmalloc(buffer_sz, GFP_NOFS);
	if (!gHis_Log.trace[LOG_TYPE_RW])
		return -1;

	buffer_sz = (gHis_Log.cout[LOG_TYPE_OTHER]) * sizeof(struct fscmd_log_s);
	gHis_Log.trace[LOG_TYPE_OTHER] = kmalloc(buffer_sz, GFP_NOFS);
	if (!gHis_Log.trace[LOG_TYPE_OTHER])
		return -1;

	gHis_Log.cur_idx[LOG_TYPE_OTHER] = -1;
	gHis_Log.cur_idx[LOG_TYPE_RW] = -1;

	return 0;
}

static struct nr_talbe_s *fscmd_get_nr_table(int target_idx)
{
	struct nr_talbe_s *pt = NR_table;

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

	if (!dump_bit) {
		spin_lock(&gHis_lock);
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
		spin_unlock(&gHis_lock);
	}
}

void fscmd_trace_sys_enter(void *data,
		struct pt_regs *regs, long id)
{
	struct nr_talbe_s *pt = fscmd_get_nr_table(id);

	if (pt != NULL) {
		struct fscmd_log_s syscall;

		syscall.syscall_nr = id;
		syscall.time = sched_clock();
		syscall.pid = current->pid;
		syscall.type = FSCMD_SYSCALL_ENTRY;
		fscmd_add_log(&syscall);
	}
}

void fscmd_trace_sys_exit(void *data,
		struct pt_regs *regs, long ret)
{
	struct nr_talbe_s *pt = fscmd_get_nr_table(regs->syscallno);

	if (pt != NULL) {
		struct fscmd_log_s syscall;

		syscall.syscall_nr = regs->syscallno;
		syscall.time = sched_clock();
		syscall.pid = current->pid;
		syscall.type = FSCMD_SYSCALL_EXIT;
		fscmd_add_log(&syscall);
	}
}

size_t mtk_fscmd_usedmem(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	int i = 0, idx = 0;
	size_t size_l = sizeof(gHis_Log);
	struct nr_talbe_s *pt = NULL;
	// start dump the farthest log in ring-buffer
	SPREAD_PRINTF(buff, size, seq,
			"time,entry/exit,syscall,pid,func,\n");
	spin_lock(&gHis_lock);
	dump_bit = 1;
	spin_unlock(&gHis_lock);
	for (i = 0, idx = 0; i < (gHis_Log.cout[LOG_TYPE_OTHER]); i++) {
		pt = fscmd_get_nr_table(gHis_Log.trace[LOG_TYPE_OTHER][idx].syscall_nr);
		idx = (gHis_Log.cur_idx[LOG_TYPE_OTHER] + i + 1) % (gHis_Log.cout[LOG_TYPE_RW]);
		SPREAD_PRINTF(buff, size, seq,
			"%llu,%s,%s,%d,%s\n",
			gHis_Log.trace[LOG_TYPE_OTHER][idx].time,
			(gHis_Log.trace[LOG_TYPE_OTHER][idx].type == FSCMD_SYSCALL_ENTRY)?"i":"o",
			pt->name,
			gHis_Log.trace[LOG_TYPE_OTHER][idx].pid,
			gHis_Log.trace[LOG_TYPE_OTHER][idx].comm);
	}

	for (i = 0, idx = 0; i < gHis_Log.cout[LOG_TYPE_RW]; i++) {
		pt = fscmd_get_nr_table(gHis_Log.trace[LOG_TYPE_RW][idx].syscall_nr);
		idx = (gHis_Log.cur_idx[LOG_TYPE_RW] + i + 1) % gHis_Log.cout[LOG_TYPE_RW];
		SPREAD_PRINTF(buff, size, seq,
			"%llu,%s,%s,%d,%s\n",
			gHis_Log.trace[LOG_TYPE_RW][idx].time,
			(gHis_Log.trace[LOG_TYPE_RW][idx].type == FSCMD_SYSCALL_ENTRY)?"i":"o",
			pt->name,
			gHis_Log.trace[LOG_TYPE_RW][idx].pid,
			gHis_Log.trace[LOG_TYPE_RW][idx].comm);
	}

	spin_lock(&gHis_lock);
	dump_bit = 0;
	spin_unlock(&gHis_lock);
	return size_l;
}

int mtk_fscmd_init(void)
{
	int ret = 0;

	ret = ghistory_struct_init();
	if (ret)
		return ret;

	dump_bit = 0;
	spin_lock_init(&gHis_lock);
	return ret;
}
