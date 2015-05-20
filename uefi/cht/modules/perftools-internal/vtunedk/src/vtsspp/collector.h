/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#ifndef _VTSS_COLLECTOR_H_
#define _VTSS_COLLECTOR_H_

#include "vtss_autoconf.h"
#include "task_map.h"
#include "regs.h"

#include <linux/fs.h>           /* for struct file        */
#include <linux/mm.h>           /* for struct mm_struct   */
#include <linux/sched.h>        /* for struct task_struct */
#include <linux/seq_file.h>     /* for struct seq_file    */

#define TASK_PID(task)    (task->tgid)
#define TASK_TID(task)    (task->pid)
#ifdef  VTSS_AUTOCONF_TASK_REAL_PARENT
#define TASK_PARENT(task) (task->real_parent)
#else
#define TASK_PARENT(task) (task->parent)
#endif

#define VTSS_FILENAME_SIZE 128
#define VTSS_TASKNAME_SIZE TASK_COMM_LEN + 1

struct pt_regs;

void vtss_collection_cfg_init(void);

void vtss_target_fork(struct task_struct* task, struct task_struct* child);
void vtss_target_exec_enter(struct task_struct* task, const char *filename, const char *config);
void vtss_target_exec_leave(struct task_struct* task, const char *filename, const char *config, int rc);
void vtss_target_exit(struct task_struct* task);
void vtss_syscall_enter(struct pt_regs *regs);
void vtss_syscall_leave(struct pt_regs *regs);
void vtss_kmap(struct task_struct* task, const char* name, unsigned long addr, unsigned long pgoff, unsigned long size);
void vtss_mmap(struct file *file, unsigned long addr, unsigned long pgoff, unsigned long size);
void vtss_mmap_reload(struct file *file, unsigned long addr);
void vtss_sched_switch(struct task_struct *prev, struct task_struct *next, void* prev_bp, void* next_ip);

int vtss_cmd_open(void);
int vtss_cmd_close(void);
int vtss_cmd_set_target(pid_t pid);
int vtss_cmd_start(void);
int vtss_cmd_stop(void);
int vtss_cmd_stop_async(void);
int vtss_cmd_pause(void);
int vtss_cmd_resume(void);
int vtss_cmd_mark(void);

int vtss_debug_info(struct seq_file *s);
int vtss_target_pids(struct seq_file *s);

int  vtss_init(void);
void vtss_fini(void);


                                                        
#endif /* _VTSS_COLLECTOR_H_ */
