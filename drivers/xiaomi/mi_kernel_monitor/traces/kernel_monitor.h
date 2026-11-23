/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM kernel_monitor
  
#if !defined(_TRACE_KERNEL_MONITOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_KERNEL_MONITOR_H
#include <linux/tracepoint.h>
////#include <trace/events/block.h>
#include <linux/blktrace_api.h>
#include "internal.h"

#define RWBS_LEN_EX	10

TRACE_EVENT(task_wokenup,
    TP_PROTO(struct task_struct* task, struct task_struct* curr, int delay, int cpu),
    TP_ARGS(task, curr, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
        __field(    int,    curr_pid )
        __array( char,	curr_comm,	TASK_COMM_LEN	)
        __array( char,	curr_proc,	TASK_COMM_LEN	)
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
        __entry->curr_pid    = curr->pid;
        memcpy(__entry->curr_comm, curr->comm, TASK_COMM_LEN);
        memcpy(__entry->curr_proc, curr->group_leader->comm, TASK_COMM_LEN);
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms curr_pid=%d cur_comm=%s cur_proc=%s", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay,
        __entry->curr_pid, __entry->curr_comm, __entry->curr_proc)        
);


TRACE_EVENT(task_migrate,
    TP_PROTO(struct task_struct* task, struct task_struct* curr, int delay, int cpu, int curr_cpu),
    TP_ARGS(task, curr, delay, cpu, curr_cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
        __field(    int,    curr_pid )
        __field(    int,    curr_cpu )
        __array( char,	curr_comm,	TASK_COMM_LEN	)
        __array( char,	curr_proc,	TASK_COMM_LEN	)
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
        __entry->curr_pid    = curr->pid;
        __entry->curr_cpu    = curr_cpu;
        memcpy(__entry->curr_comm, curr->comm, TASK_COMM_LEN);
        memcpy(__entry->curr_proc, curr->group_leader->comm, TASK_COMM_LEN);
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms curr_pid=%d curr_comm=%s curr_proc=%s cpu_curr=%d", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay, 
        __entry->curr_pid, __entry->curr_comm, __entry->curr_proc, __entry->curr_cpu)        
);


TRACE_EVENT(task_wokenup_new,
    TP_PROTO(struct task_struct* task, struct task_struct* curr, int delay, int cpu),
    TP_ARGS(task, curr, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
        __field(    int,    curr_pid )
        __array( char,	curr_comm,	TASK_COMM_LEN	)
        __array( char,	curr_proc,	TASK_COMM_LEN	)
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
        __entry->curr_pid    = curr->pid;
        memcpy(__entry->curr_comm, curr->comm, TASK_COMM_LEN);
        memcpy(__entry->curr_proc, curr->group_leader->comm, TASK_COMM_LEN);
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms curr_pid=%d curr_comm=%s curr_proc=%s", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay, 
        __entry->curr_pid, __entry->curr_comm, __entry->curr_proc)        
);


TRACE_EVENT(task_schedin,
    TP_PROTO(struct task_struct* next, struct task_struct* prev, u64 delay, int cpu),
    TP_ARGS(next, prev, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    u64,    delay )
        __field(    int,    cpu )
        __field(    int,    prev_pid )
        __array( char,	prev_comm,	TASK_COMM_LEN	)
        __array( char,	prev_proc,	TASK_COMM_LEN	)
    ),
    TP_fast_assign(
        __entry->pid    = next->pid;
        memcpy(__entry->comm, next->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
        __entry->prev_pid    = prev->pid;
        memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
        memcpy(__entry->prev_proc, prev->group_leader->comm, TASK_COMM_LEN);
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%llums prev_pid=%d prev_comm=%s prev_proc=%s", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay, 
        __entry->prev_pid, __entry->prev_comm, __entry->prev_proc)        
);


TRACE_EVENT(task_schedout,
    TP_PROTO(struct task_struct* curr, struct task_struct* next, int state, u64 delay, int cpu, int next_prio),
    TP_ARGS(curr, next, state, delay, cpu, next_prio),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    state  )
        __field(    u64,    delay )
        __field(    int,    cpu )
        __field(    int,    next_pid )
        __array( char,	next_comm,	TASK_COMM_LEN	)
        __array( char,	next_proc,	TASK_COMM_LEN	)
        __field(    int,    next_prio )
    ),
    TP_fast_assign(
        __entry->pid    = curr->pid;
        memcpy(__entry->comm, curr->comm, TASK_COMM_LEN);
        __entry->state    = state;
        __entry->delay    = delay;
        __entry->cpu    = cpu;
        __entry->next_pid    = next->pid;
        memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
        memcpy(__entry->next_proc, next->group_leader->comm, TASK_COMM_LEN);
        __entry->next_prio    = next_prio;
    ),

    TP_printk("[%d] pid=%d comm=%s state=%d delay=%llums next_pid=%d next_comm=%s next_proc=%s next_prio=%d", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->state, __entry->delay, 
        __entry->next_pid, __entry->next_comm, __entry->next_proc, __entry->next_prio)        
);

TRACE_EVENT(task_mutex_wait_start,
    TP_PROTO(struct task_struct* task, int cpu),
    TP_ARGS(task, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s", 
        __entry->cpu, __entry->pid, __entry->comm)        
);

TRACE_EVENT(task_mutex_wait_finish,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_rwsem_read_wait_start,
    TP_PROTO(struct task_struct* task, int cpu),
    TP_ARGS(task, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s", 
        __entry->cpu, __entry->pid, __entry->comm)        
);

TRACE_EVENT(task_rwsem_read_wait_finish,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_rwsem_write_wait_start,
    TP_PROTO(struct task_struct* task, int cpu),
    TP_ARGS(task, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s", 
        __entry->cpu, __entry->pid, __entry->comm)        
);

TRACE_EVENT(task_rwsem_write_wait_finish,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
////kernel latency 
TRACE_EVENT(task_mutex_wait_long,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_rwsem_read_wait_long,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_rwsem_write_wait_long,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_futex_wait_long,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_sched_lat_long,
    TP_PROTO(struct task_struct* prev, struct task_struct* task, int delay, int cpu),
    TP_ARGS(prev, task, delay, cpu),

    TP_STRUCT__entry(
         __field(int,   prev_pid  )
        __array( char,	prev_comm,	TASK_COMM_LEN	)       
        __field( int,   pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field( int,   delay )
        __field( int,   cpu )
    ),
    TP_fast_assign(
        __entry->prev_pid    = task->pid;
        memcpy(__entry->prev_comm, task->comm, TASK_COMM_LEN);        
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] prev_pid=%d prev_comm=%s pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->prev_pid, __entry->prev_comm, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_irqoff_long,
    TP_PROTO(struct task_struct* task, u64 delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    u64,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%llums", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_rt_running_long,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_preemptoff_long,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_iowait_long,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);

TRACE_EVENT(task_binder_lat_long,
    TP_PROTO(struct task_struct* task, int delay, int cpu),
    TP_ARGS(task, delay, cpu),

    TP_STRUCT__entry(
        __field(    int,    pid  )
        __array( char,	comm,	TASK_COMM_LEN	)
        __field(    int,    delay )
        __field(    int,    cpu )
    ),
    TP_fast_assign(
        __entry->pid    = task->pid;
        memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
        __entry->delay    = delay;
        __entry->cpu    = cpu;
    ),

    TP_printk("[%d] pid=%d comm=%s delay=%dms", 
        __entry->cpu, __entry->pid, __entry->comm, __entry->delay)        
);
/////////////////////////////////////////////////////////////////////////////////////////////////////////

TRACE_EVENT(io_read_delay,
	TP_PROTO(dev_t devid, pid_t pid, blk_opf_t opf, struct inode *inode,
		 char *pathname, sector_t sector, unsigned int nr_sector,
		 time64_t issue_ts, time64_t complete_ts, int error),

	TP_ARGS(devid, pid, opf, inode, pathname, sector, nr_sector, issue_ts,
		complete_ts, error),

	TP_STRUCT__entry(
		__field(dev_t, devid)
		__field(pid_t, pid)
		__field(blk_opf_t, opf)
		__field(ino_t, ino)
		__string(pathbuf, pathname)
		__field(sector_t, sector)
		__field(unsigned int, nr_sector)
		__field(time64_t, issue_ts)
		__field(time64_t, complete_ts)
		__field(int, error)
		__array(char, rwbs, RWBS_LEN_EX)
	),

	TP_fast_assign(
		__entry->devid = devid;
		__entry->pid = pid;
		__entry->opf = opf;
		__entry->ino = inode ? inode->i_ino : 0;
		__assign_str(pathbuf);
		__entry->sector = sector;
		__entry->nr_sector = nr_sector;
		__entry->issue_ts = issue_ts;
		__entry->complete_ts = complete_ts;
		__entry->error = error;
		blk_fill_rwbs(__entry->rwbs, opf);
	),

	TP_printk("dev=%d,%d, pid=%d, rwbs=%s, opf=%d, ino=%lu, path=%s, sector=%llu, "
		  "nr=%u, issue=%lld, complete=%lld, err=%d\n",
		  MAJOR(__entry->devid), MINOR(__entry->devid), __entry->pid,
		  __entry->rwbs, __entry->opf, __entry->ino, __get_str(pathbuf),
		  __entry->sector, __entry->nr_sector, __entry->issue_ts,
		  __entry->complete_ts, __entry->error)
);

#endif /* _TRACE_KERNEL_MONITOR_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/xiaomi/mi_kernel_monitor/traces
  
/* This part must be outside protection */
#include <trace/define_trace.h>