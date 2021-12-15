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

struct fscmd_s {
	struct fscmd_log_s trace[MAX_LOG_NR]; /* Cycle buffer in MAX_LOG_NR */
	int cur_idx; /* the top idx we store log currently */
	spinlock_t lock; /* access lock */
};

static struct fscmd_s gHis_Log;

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

	spin_lock(&gHis_Log.lock);
	next_idx = (gHis_Log.cur_idx + 1) % MAX_LOG_NR;
	cmdlog = gHis_Log.trace + next_idx;
	cmdlog->time = log->time;
	cmdlog->type = log->type;
	cmdlog->syscall_nr = log->syscall_nr;
	cmdlog->pid = log->pid;
	get_task_comm(cmdlog->comm, current);
	gHis_Log.cur_idx = next_idx;
	spin_unlock(&gHis_Log.lock);

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
	for (i = 0; i < MAX_LOG_NR; i++) {
		pt = fscmd_get_nr_table(gHis_Log.trace[idx].syscall_nr);
		idx = (gHis_Log.cur_idx + i + 1) % MAX_LOG_NR;
		SPREAD_PRINTF(buff, size, seq,
			"%llu,%s,%s,%d,%s\n",
			gHis_Log.trace[idx].time,
			(gHis_Log.trace[idx].type == FSCMD_SYSCALL_ENTRY)?"i":"o",
			pt->name,
			gHis_Log.trace[idx].pid,
			gHis_Log.trace[idx].comm);
	}
	return size_l;
}

int mtk_fscmd_init(void)
{
	gHis_Log.cur_idx = -1;
	spin_lock_init(&gHis_Log.lock);
	return 0;
}
