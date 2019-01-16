#ifndef _MTLBPROF_MTLBPROF_H
#define _MTLBPROF_MTLBPROF_H

/* load balance status*/
#define MT_LBPROF_UPDATE_STATE				(-1)
#define MT_LBPROF_NO_TASK_STATE				0x0
#define MT_LBPROF_IDLE_STATE				0x1
#define MT_LBPROF_N_TASK_STATE				0x2
#define MT_LBPROF_AFFINITY_STATE			0x3
#define MT_LBPROF_FAILURE_STATE				0x4
#define MT_LBPROF_ONE_TASK_STATE			0x5
#define MT_LBPROF_HOTPLUG_STATE				0x6
#define MT_LBPROF_BALANCE_FAIL_STATE		        0x7
#define MT_LBPROF_ALLPINNED				0x8
#define MT_LBPROF_ALLOW_UNBLANCE_STATE                  0x9

/* to prevent print too much log, print the log info*/
#define MT_LBPROF_NR_BALANCED_FAILED_UPPER_BOUND	0x3

#ifdef CONFIG_MT_LOAD_BALANCE_PROFILER

/* load balance status */
#define MT_LBPROF_NO_TRIGGER			0x0
#define MT_LBPROF_SUCCESS			0x1
#define MT_LBPROF_NOBUSYG_NO_LARGER_THAN	0x2
#define MT_LBPROF_NOBUSYG_NO_BUSIEST		0x4
#define MT_LBPROF_NOBUSYG_BUSIEST_NO_TASK	0x8
#define MT_LBPROF_NOBUSYG_CHECK_FAIL		0x10
#define MT_LBPROF_NOBUSYQ			0x20
#define MT_LBPROF_FAILED			0x40
#define MT_LBPROF_DO_LB				0x80
#define MT_LBPROF_BALANCE			0x100
#define MT_LBPROF_PICK_BUSIEST_FAIL_1		0x200
#define MT_LBPROF_PICK_BUSIEST_FAIL_2		0x400
#define MT_LBPROF_AFFINITY			0x800
#define MT_LBPROF_CACHEHOT			0x1000
#define MT_LBPROF_RUNNING			0x2000
#define MT_LBPROF_HISTORY			0x4000

#define mt_lbprof_stat_inc(sd, field)			do { } while (0)
#define mt_lbprof_stat_add(sd, field, amt)		do { } while (0)

#define mt_lbprof_stat_or(var, val)	\
	do {				\
		var |= (val);		\
	} while (0)
#define mt_lbprof_stat_set(var, val)	\
	do {				\
		var = (val);		\
	} while (0)
#define mt_lbprof_test(var, val)		(val == (var & val))
#define mt_lbprof_lt(var, val)			(var < val)

void mt_lbprof_rqinfo(char *strings);

#else				/* CONFIG_MT_LOAD_BALANCE_PROFILER */
#define mt_lbprof_stat_inc(rq, field)		do { } while (0)
#define mt_lbprof_stat_add(rq, field, amt)	do { } while (0)
#define mt_lbprof_stat_set(var, val)		do { } while (0)
#define mt_lbprof_stat_or(var, val)			do { } while (0)
#define mt_lbprof_test(var, val)			0
#define mt_lbprof_gt(var, val)				do { } while (0)
#define mt_lbprof_lt(var, val)				0
#define mt_lbprof_rqinfo(val)			do { } while {0}

#endif				/* CONFIG_MT_LOAD_BALANCE_PROFILER */

extern int mt_lbprof_enable(void);
extern int mt_lbprof_disable(void);
extern void mt_lbprof_update_status(void);
extern void mt_lbprof_update_state(int cpu, int rq_cnt);
extern void mt_lbprof_update_state_has_lock(int cpu, int rq_cnt);
#endif
