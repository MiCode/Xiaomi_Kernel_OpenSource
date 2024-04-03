// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kmemleak.h>
#include <trace/hooks/sched.h>
#include <linux/kprobes.h>
#include <linux/delay.h>

#include "walt.h"
#include "trace_penalty.h"
#include "penalty.h"

#define DEBUG 0

#define  PENALTY_DEF    (1)
#define  PENALTY_SCHED  (2)
#define  PENALTY_CTL    (4)
#define  PENALTY_LOG    (128)

#define ENABLE_SCHED (PENALTY_SCHED|PENALTY_DEF)
#define ENABLE_CTL (PENALTY_CTL|PENALTY_DEF)

#define PENALTY_DEBUG_VER  1
#define PENALTY_DEBUG_PROF 2

#define MAX_CLUSTER 5
#define MAX_KEY_TASK 2

struct walt_task_struct_penalty {
	bool           high_load;
	u32            penaty_scale;
};
#define wts2p(wts) ({  \
	void *__mptr = (void *)(wts); \
	((struct walt_task_struct_penalty *) (__mptr + sizeof(struct walt_task_struct) )); } )

struct walt_rq_penalty {
	u64	prev_penaty_scale;     /*for penalize of cpu load for frame headroom, scale against 1024 */
	u64 cur_penaty_scale;
};

#define wrq2p(cpu)   (&per_cpu(wrqpd, cpu) )

struct walt_sched_cluster_penalty {
	u64         penaty_scale;
	u64         boost_scale;
	u64         clock_start;
};
#define wsc2p(cluster)   (&wscpd[cluster] )

/* per task info for yield */
struct penalty_status {
	pid_t          pid;                /* task pid */
	u64 	       count;
	u64            last_yield_time;    /* last trigger time of sched_yield request */
	u64            est_yield_end;       /*end of last yield */
	atomic64_t       yield_total;          /* yield time in last window,sleep+running */	
	u32            prev_yield_total;     /* yield time of prev */
	u32            last_sleep_ns;        /* decay of sleep */
};
#define PENALTY_UNACTIVE_NS  (5000000000)
static DEFINE_RAW_SPINLOCK(penalty_lock); /* protect pstatus */
static struct penalty_status pstatus[MAX_KEY_TASK];
static inline  struct penalty_status * get_penalty_status_lru(pid_t pid_, u64 clock)
{
	int i;
	int min_idx=0;
	u64 min_count=UINT_MAX;
	for(i=0; i<MAX_KEY_TASK; i++)
	{
		if(pid_ == pstatus[i].pid && pstatus[i].count > 0)
		{
			return &pstatus[i];
		}
		/*find long time not used */
		if(pstatus[i].last_yield_time >0 && clock > pstatus[i].last_yield_time &&
		(clock-pstatus[i].last_yield_time)>PENALTY_UNACTIVE_NS ){
			pstatus[i].count = 1;
		}
		if(pstatus[i].count <= min_count)
		{
			min_idx=i;
			min_count=pstatus[i].count;
		}
	}

	pstatus[min_idx].pid = pid_;
	pstatus[min_idx].count = 1;
	pstatus[min_idx].last_yield_time = 0;
	pstatus[min_idx].est_yield_end = 0;
	atomic64_set(&pstatus[min_idx].yield_total, 0);
	pstatus[min_idx].prev_yield_total = 0;
	pstatus[min_idx].last_sleep_ns = 0;
	return  &pstatus[min_idx];
}
static inline  struct penalty_status * get_penalty_status(pid_t pid_)
{
	int i;
	for(i=0; i<MAX_KEY_TASK; i++)
	{
		if(pid_ == pstatus[i].pid && pstatus[i].count > 0)
		{
			return &pstatus[i];
		}
	}
	return  0;
}


static int __maybe_unused large_penalty = PENALTY_LOG;
static unsigned int __read_mostly sysctl_sched_auto_penaty = 0;
static unsigned int __read_mostly sysctl_penalty_debug = 0;
/* allow at most 750*1000/1024=73% scale down */

#define TASK_SWITCH_THRESH 10
#define MAX_SYMBOL_LEN	64
#define PENATY_HEADROOM (800000)

#define PER_TASK_SCALE_MAX   (1200)
#define BOOST_SCALE_125    (1200)
#define MAX_CONTINOUS_DROP   (7)

#define MAX_YIELD_SLEEP  (2000000ULL)
#define MIN_YIELD_SLEEP   (200000ULL)
#define MIN_YIELD_SLEEP_HEADROOM  (100000ULL)


/* lower value, better power, worse perf, for Unity games */
static atomic_t scale_min_percpu = ATOMIC_INIT(900);
/* lower value, better power, worse perf, for UnReal games */
static atomic_t scale_min_pertask = ATOMIC_INIT(850);
/*1: for more power saving */
static int penalty_pl = 1;
/*
 *if 1: use per task penalty ( scale % down for task load)
 *if 0: use per cpu penalty (scale % down for cpu load)
 */
static int sleep_penalty_per_task = 0;
/* minimap sleep time in ns */
static int penalty_headroom =   100000;

static u64 target_fps = 0;
static u64 max_fps_drop = 2;  /* we allow at most 2fps drop instantly,  slow_thresh depends on this*/
static u64 continous_drop = 0;

static long total_win = 0;            /* total windows applied */
static long total_penalty_win = 0;    /* sleep time > penalty_headroom  */
static long total_low = 0;            /* low fps */
static long total_very_low = 0;            /* low fps */
static atomic_t yield_ns_total = ATOMIC_INIT(0);
static atomic_t yield_cnt_total = ATOMIC_INIT(0);


/************** V3 related part */
#define FPS_HEALTHY 		(0)
#define FPS_LOW    		(1)
#define FPS_VERY_LOW    (2)

static atomic_t healthy = ATOMIC_INIT(FPS_LOW);
static int max_frame = 30;
static u64 frame_time_ns = 0;
static u64 frame_num = 0;
static s64 total_frame_time = 0;
static u64 first_frame_ts = 0;
static u64 last_frame_ts = 0;
static u64 slow_thresh =  3000000 /* 3ms, depend on max_fps_drop and max_frame*/;
static u64 fast_thresh =   500000 ; /* in a period, if 0.5ms faster, then it's headroom */
static void sched_update_frame_stat( int update_typ, int type, pid_t pid, uint32_t ctx_id,
	uint32_t timestamp, bool end_of_frame );
static char symbol_msm_update[MAX_SYMBOL_LEN] = "msm_perf_events_update";
/* For each probe you need to allocate a kprobe structure */
static struct kretprobe kp_msm_update = {
    .kp.symbol_name =symbol_msm_update
};

static atomic_t top_task_pid = ATOMIC_INIT(0); /*if it's non-zero, we got heavy task hint from walt */
static atomic_t game_pid = ATOMIC_INIT(0);

static DEFINE_RAW_SPINLOCK(key_wts_lock); /* protect bellow, key_wts_* */
static pid_t key_tid[MAX_KEY_TASK] ;  /* tid list of key task */
static atomic_t key_wts_len = ATOMIC_INIT(0);

static u64 key_wts_util[MAX_KEY_TASK];   /* normalized toward 1024 */
#define WTS_SUM_NOT_DEF 0
#define WTS_SUM_NOT_START 1
#define WTS_SUM_STARTED   2

static u64 key_wts_sum_started[MAX_KEY_TASK];  /* sum of sleep */
static u64 key_wts_sum_run_start_ts[MAX_KEY_TASK] ;  /* sum of sleep */
static u64 key_wts_sum_run[MAX_KEY_TASK] ;  /* sum run + runnable */
static u64 key_wts_sum_run_first[MAX_KEY_TASK] ;  /* last run + runnable */


static int  msm_update_kprobe_init(void);
static int  msm_update_kprobe_deinit(void);
static inline int add_key_tid(struct task_struct *p, int slot);
static inline int add_update_key_tid(struct task_struct *p);
static int remove_old_heavy(struct task_struct *p, struct walt_task_struct_penalty *wtsp);
static inline int remove_key_tid(struct task_struct *p);
static u64 update_task_running_locked(struct task_struct *p, u64 ts);

static DEFINE_PER_CPU(u64, switch_count) = -1;
static void penalty_init_sysctl(void);

static void rollover_cpu_window_internal(struct walt_rq *wrq, struct  rq *rq,
	bool full_window);
static void (*run_callback)(struct rq *rq, unsigned int flags);
static u64 cur_window_size = 0;
static u64 boost_dur = 0;

DEFINE_PER_CPU(struct walt_rq_penalty, wrqpd);
struct walt_sched_cluster_penalty wscpd[MAX_CLUSTER];


static void update_threshold(int win_size){
	if(win_size == cur_window_size)
		return;
	cur_window_size = win_size;
	boost_dur = win_size >> 1;
}

void tracing_mark_write(int pid, char* counter, long scale){
	if(sysctl_penalty_debug&PENALTY_DEBUG_VER)
		trace_printk("C|%d|%s|%lu\n", pid, counter, scale);
}

static void update_rq_penaty(int cpu, u64 penaty_scale){
	struct walt_rq_penalty * wrqp = wrq2p(cpu);
	wrqp->cur_penaty_scale =  min(penaty_scale,  wrqp->cur_penaty_scale);
}

static void update_task_sleep_penaty(struct walt_rq *wrq, int cpu,
	struct walt_task_struct_penalty *wtsp, u64 scale)
{
	struct walt_sched_cluster_penalty *wscp;
	total_win++;
	/* skip if not healthy */
	if(atomic_read(&healthy) > FPS_HEALTHY)
	{
		scale =  SCHED_CAPACITY_SCALE;
		/*boost cpufreq if fps slow a lot */
		if(atomic_read(&healthy) > FPS_LOW)
		{
			wscp = wsc2p(wrq->cluster->id);
			wscp->boost_scale = BOOST_SCALE_125  ;
			wscp->clock_start = sched_clock();
			total_very_low++;
		}else
		{
			total_low++;
		}
	}else
	{
		total_penalty_win++;
	}
	/*per task penalty */
	if(sleep_penalty_per_task)
	{
		wtsp->penaty_scale = scale;
		return ;
	}
	/*cpu based penalty */
    update_rq_penaty(cpu, scale);
}

#define HEAVY_NO_DETECT_NS      (5000000000ULL)
#define HEAVY_DETECT_THRESH_NS  (5040000000ULL)
#define HEAVY_COMMIT_THRESH_NS  (5080000000ULL)
#define MAX_TOP_TASK            (1)
static DEFINE_RAW_SPINLOCK(top_wts_lock); /* protect bellow, key_wts_* */
static struct walt_task_struct *top_wts[MAX_TOP_TASK]; 
static u64 demand_top[MAX_TOP_TASK];  /*top demand task*/
static atomic64_t last_clock = ATOMIC_INIT(0);
static void update_top_task(struct walt_task_struct *wts, struct walt_task_struct_penalty * wtsp){
	unsigned long flags;
	int i=0;
	int min=-1;
	u64 min_demand=UINT_MAX;
	u64 clock = sched_clock();
	u64 delta = clock - atomic64_read(&last_clock);
	if(delta < HEAVY_NO_DETECT_NS)
	{
		return ;
	}else if (delta < HEAVY_DETECT_THRESH_NS){
		//compare heavy
		raw_spin_lock_irqsave(&top_wts_lock, flags);
		for(i=0; i<MAX_TOP_TASK; i++)
		{
			if(wts == top_wts[i] ){
				min = -1;
				break;
			}
			if(demand_top[i] < min_demand )
			{
				min_demand = demand_top[i];
				min = i;
			}
		}
		if(min>=0 && wts->demand_scaled> min_demand)
		{
			demand_top[min] = wts->demand_scaled;
			top_wts[min] = wts;
		}
		raw_spin_unlock_irqrestore(&top_wts_lock, flags);
	}else if (delta < HEAVY_COMMIT_THRESH_NS)
	{
		raw_spin_lock_irqsave(&top_wts_lock, flags);
		/* slience do nothing */
		for(i=0; i<MAX_TOP_TASK; i++)
		{
			if(wts == top_wts[i] )
			{
				break;
			}
			demand_top[i] = 0; /* reset top */
		}
		if(i<MAX_TOP_TASK)
		{
			wtsp->high_load = true;	
			
		}else
		{
			wtsp->high_load = false;

		}
		raw_spin_unlock_irqrestore(&top_wts_lock, flags);
	}else
	{
		atomic64_set(&last_clock,clock) ;
	}
}


static void update_task_high_load (struct walt_rq *wrq, 
	struct walt_task_struct *wts, u64 yield_ns)
{
	struct task_struct * p = wts_to_ts(wts);
	struct walt_task_struct_penalty * wtsp = wts2p(wts);
	struct walt_related_thread_group *rtg = wts->grp;
	if (!rtg || rtg->id != DEFAULT_CGROUP_COLOC_ID)
	{
		wtsp->high_load = false;
		return;
	}
	tracing_mark_write(p->pid, "low_lat", (wts->low_latency & WALT_LOW_LATENCY_HEAVY));
	update_top_task(wts, wtsp);
}

/* export API */
static int penalize_yield( u64 *delta, struct walt_task_struct *wts)
{
	if(sysctl_sched_auto_penaty & ENABLE_SCHED){
		struct walt_task_struct_penalty * wtsp = wts2p(wts);
		u64 delta_prev = *delta;
		if(wtsp->penaty_scale !=SCHED_CAPACITY_SCALE && wtsp->penaty_scale >0 &&
			wtsp->penaty_scale<= PER_TASK_SCALE_MAX)
		{
			*delta =
			(delta_prev*wtsp->penaty_scale) >> SCHED_CAPACITY_SHIFT;
			if(sysctl_penalty_debug&PENALTY_DEBUG_VER && wtsp->penaty_scale <SCHED_CAPACITY_SCALE)
			{
				tracing_mark_write(wts_to_ts(wts)->pid, "penalty", wtsp->penaty_scale);
			}
		}
	}
  return 0;
}
/* export API */
static void rollover_task_window(struct walt_rq *wrq, struct  rq *rq, struct task_struct *p)
{
	u64 yield_total ;
	unsigned long flags;
	struct penalty_status * ps;

	u64 clock;
	u64 scale;
	long scale_min;
	bool old_highload;
	bool highload;
	pid_t top_task_pid_;
	pid_t tgid = atomic_read(&game_pid);
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct walt_task_struct_penalty * wtsp = wts2p(wts);
	struct walt_rq_penalty * wrqp = wrq2p(cpu_of(rq));
	if(!sysctl_sched_auto_penaty )
		return ;
	if (tgid == 0 || (tgid >0 && tgid != p->tgid))
	{
		wtsp->penaty_scale = SCHED_CAPACITY_SCALE;
		return ;
	}
	yield_total = 0;
	wtsp->penaty_scale = SCHED_CAPACITY_SCALE;
	if(wtsp->high_load)
	{
		raw_spin_lock_irqsave(&penalty_lock, flags);
		ps = get_penalty_status(p->pid);
		if(ps)
		{
			yield_total = atomic64_read(&ps->yield_total);
			if(yield_total>0 && wrq->prev_window_size > yield_total )
			{
				scale_min = atomic_read(&scale_min_pertask);
				wtsp->penaty_scale = div64_u64((wrq->prev_window_size-yield_total) << 10,
					wrq->prev_window_size);
				wtsp->penaty_scale = max((u32)scale_min, wtsp->penaty_scale);
				if(atomic_read(&healthy) > FPS_HEALTHY)
				{
					wtsp->penaty_scale = SCHED_CAPACITY_SCALE;
				}
				/* in this case, we penalize by task, so disable cpu freq level penalization */
				wrqp->cur_penaty_scale = 0;
			}else
			{
				wtsp->penaty_scale = SCHED_CAPACITY_SCALE;
			}
			ps->prev_yield_total = yield_total;
			atomic64_set(&ps->yield_total, 0);
		}
		tracing_mark_write(p->pid, "prev_yield_total", yield_total);
		raw_spin_unlock_irqrestore(&penalty_lock, flags);
	}

	top_task_pid_ = atomic_read(&top_task_pid);
	tracing_mark_write(p->pid, "low_lat", (wts->low_latency & WALT_LOW_LATENCY_HEAVY));
	if(sysctl_sched_auto_penaty >0 && top_task_pid_ == 0  )
	{
		old_highload = wtsp->high_load;
		update_task_high_load(wrq, wts, yield_total);
		highload = wtsp->high_load;
		/* detect if heavy changed */
		if(!old_highload && highload)
		{
			add_update_key_tid(p);
		}else if (old_highload && !highload)
		{
			remove_key_tid(p);
		}
	}else if(sysctl_sched_auto_penaty >0 && top_task_pid_ > 0)
	{
		/*previously, it's heavy, but not now */
		if(wtsp->high_load && !(wts->low_latency & WALT_LOW_LATENCY_HEAVY))
		{
			remove_old_heavy(p, wtsp);
		}
	}
	if(sysctl_sched_auto_penaty >0)
	{
		/* skip if yield dected ; only look at top heavy task*/
		if(wtsp->high_load >0 && yield_total == 0)
		{
			scale_min = atomic_read(&scale_min_percpu);
			clock = sched_clock();
			scale = update_task_running_locked(p, clock);

			/* clip to penalty threshold */
			scale = max((u32)scale_min, (u32)scale);
			/*only update top one */
			if(top_task_pid_ == 0 || (top_task_pid_>0 && top_task_pid_ == p->pid))
			{
				tracing_mark_write(p->pid, "task_sleep_penalty", scale);
				update_task_sleep_penaty(wrq, cpu_of(rq), wtsp, scale);
			}
		}
	}
	#if DEBUG
	if(wtsp->high_load >0)
	{
		trace_printk("task=%s yield_total=%llu prev_window=%llu\
		penaty_scale=%llu  prev_window_size=%llu\n", p->comm,
		yield_total, wts->prev_window, wtsp->penaty_scale,
		wrq->prev_window_size );
		tracing_mark_write(p->pid, "high_load", wtsp->high_load);
	}
	#endif
	if(!wtsp->high_load)
		return ;
	trace_sched_penalty(p, yield_total,
		SCHED_CAPACITY_SCALE-wtsp->penaty_scale);
	update_threshold(wrq->prev_window_size);
	if(sysctl_penalty_debug&PENALTY_DEBUG_VER){
		tracing_mark_write(cpu_of(rq), "cur_penaty_scale",
		wrqp->cur_penaty_scale);
	}
}
static inline u64 scale_exec_time_def(u64 delta, struct walt_rq *wrq,
	struct walt_task_struct *wts)
{
	delta = (delta * wrq->task_exec_scale) >> SCHED_CAPACITY_SHIFT;
	if (wts->load_boost && wts->grp && wts->grp->skip_min)
		delta = (delta * (1024 + wts->boosted_task_load) >> 10);
	return delta;
}

static int update_running_after_idle_wakeup(struct walt_task_struct *wts,
	u64 clock)
{
    return 0;
}

/*export API, reserved */
static void update_yield_util(struct walt_rq *wrq,
	struct walt_task_struct *wts)
{

}

/*export API, reserved */
static void account_wakeup(struct task_struct *p)
{

}

/*export API: reserved*/
static int  update_yield_ts(struct walt_task_struct *wts, u64 clock)
{
	if(!sysctl_sched_auto_penaty )
	{
		return 0;
	}
	/*specical case from android_rvh_schedule, clock=0, wts=prev task */
	update_running_after_idle_wakeup(wts, clock);
	return 0;
}

/*export API: */
static void cluster_init(struct walt_sched_cluster *cluster)
{
	struct walt_sched_cluster_penalty *wscp = wsc2p(cluster->id);
	wscp->penaty_scale = SCHED_CAPACITY_SCALE;
}
static void cluster_update(struct walt_sched_cluster *cluster, struct rq *rq)
{
	struct walt_sched_cluster_penalty *wscp = wsc2p(cluster->id);
	struct walt_rq_penalty * wrqp = wrq2p(cpu_of(rq));

	if(wrqp->prev_penaty_scale >0 &&
		wrqp->prev_penaty_scale <SCHED_CAPACITY_SCALE)
	{
		wscp->penaty_scale = min(wscp->penaty_scale,
			wrqp->prev_penaty_scale);

	}
}
/*export API: */
static void policy_load(struct walt_sched_cluster *cluster, u64 *pload,
	u64 *ppload)
{
	u64 eff_util ;
	int cpu ;
	long scale_min = atomic_read(&scale_min_percpu);
	struct walt_sched_cluster_penalty *wscp = wsc2p(cluster->id);
	u64 penaty_scale = max((u64)scale_min, wscp->penaty_scale);
	if(wscp->boost_scale)
	{
		if( (wscp->clock_start + boost_dur) > sched_clock())
		{
			*pload = ((*pload) * wscp->boost_scale) >> SCHED_CAPACITY_SHIFT;
			cpu = cpumask_first(&cluster->cpus);
			/*we allow to boost update to 90% */
			eff_util = arch_scale_cpu_capacity(cpu) *cur_window_size  >> SCHED_CAPACITY_SHIFT;
			/* at most boost to 90% capacity */
			eff_util = eff_util *921 >> SCHED_CAPACITY_SHIFT;
			if(*pload > eff_util)
				*pload = eff_util;
			if(sysctl_penalty_debug&PENALTY_DEBUG_VER)
				tracing_mark_write(cpu, "boost", wscp->boost_scale );
			if(sysctl_penalty_debug&(PENALTY_DEBUG_VER|PENALTY_DEBUG_PROF))
			{
				tracing_mark_write(cpu, "cpu_load_scale", wscp->boost_scale );
			}
			return ;
		}
		wscp->boost_scale = 0;
	}
	if(sysctl_penalty_debug&PENALTY_DEBUG_VER)
		tracing_mark_write(cpumask_first(&cluster->cpus), "boost", wscp->boost_scale );
	if((sysctl_sched_auto_penaty & ENABLE_SCHED) && penaty_scale > 0 &&
		penaty_scale < SCHED_CAPACITY_SCALE)
	{
		*pload = (*pload * penaty_scale) >> SCHED_CAPACITY_SHIFT ;
		if(penalty_pl && ppload!=0)
			*ppload = (*ppload * penaty_scale) >> SCHED_CAPACITY_SHIFT ;
		}
		if(sysctl_penalty_debug&(PENALTY_DEBUG_VER|PENALTY_DEBUG_PROF))
		{
				tracing_mark_write(cpumask_first(&cluster->cpus),
				"cpu_load_scale", penaty_scale);
		}

}

static void rollover_cpu_window_internal(struct walt_rq *wrq, struct rq *rq,
	bool full_window)
{
	struct walt_rq_penalty * wrqp = wrq2p(cpu_of(rq));
	wrqp->prev_penaty_scale = wrqp->cur_penaty_scale;
	wrqp->cur_penaty_scale = SCHED_CAPACITY_SCALE;
}

/*export API: */
static void rollover_cpu_window(struct walt_rq *wrq __maybe_unused, struct rq *rq,
	bool full_window __maybe_unused)
{
   rollover_cpu_window_internal(wrq, rq, full_window);
}

/*export API: */
int eval_need(struct cpu_busy_data *data,int data_len,
	unsigned int *pneed_cpus)
{
	int i;
	struct cpu_busy_data *c;
	struct rq *rq;
	unsigned int increased = 0;
	unsigned int busy_pct = 0;
	u64 estimate_nr_task = 0;
	if( !(sysctl_sched_auto_penaty & ENABLE_CTL))
	{
		return -1;
	}
	for(i=0; i<data_len; i++)
	{
		c = &data[i];
		rq = cpu_rq(c->cpu);
		estimate_nr_task = rq->nr_switches -
			per_cpu(switch_count, c->cpu);
		per_cpu(switch_count, c->cpu) = rq->nr_switches;
		if(c->busy_pct > busy_pct)
		{
			if(c->is_busy && estimate_nr_task>TASK_SWITCH_THRESH)
			{
				increased = 1;
			}else{
				increased = 0;
			}
			busy_pct = c->busy_pct;
		}
	}
	*pneed_cpus += increased;
	return 0;
}

static int yield_penaty(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos)
{
	int ret;
	int enabled;
	unsigned int *data = (unsigned int *)table->data;
	static DEFINE_MUTEX(mutex);
	mutex_lock(&mutex);
	enabled = sysctl_sched_auto_penaty;
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret || !write)
		goto done;
	if(0 == *data)
	{
		if(enabled > 0)
		{
			msm_update_kprobe_deinit();
		}
	}else
	{
		if(0 == enabled )
		{
			msm_update_kprobe_init();
			pr_info("after  msm_update_kprobe_init\n");
		}
	}
done:
	mutex_unlock(&mutex);
	return ret;
}

#define USE_WALT_HEAVY 0
#if USE_WALT_HEAVY
static int update_heavy(struct walt_task_struct **heavy_wts, int heavy_nr)
{
	int i;
	struct task_struct *p;
	struct walt_task_struct_penalty * wtsp ;
	if(throttle_update++ < 10)
		return 0;

	throttle_update = 0;
	for(i=0; i< heavy_nr; i++){
		p = wts_to_ts(heavy_wts[i]);
		wtsp = wts2p(heavy_wts[i]);
		if(i<MAX_KEY_TASK){
			add_update_key_tid(p);
			wtsp->high_load = true;
		}else{
			wtsp->high_load = false;
		}
		tracing_mark_write(1, "heavy_pid", p->pid);
		tracing_mark_write(1, "heavy_demand", heavy_wts[i]->demand_scaled);
	}
	if(heavy_nr>0){
		atomic_set(&top_task_pid,  wts_to_ts(heavy_wts[0])->pid);
		tracing_mark_write(1, "heavy_pid_set", wts_to_ts(heavy_wts[0])->pid);
	}else{
		atomic_set(&top_task_pid,  0);
	}
	return 0;
}
#else
static int update_heavy(struct walt_task_struct **heavy_wts, int heavy_nr)
{
	return 0;
}
#endif

static inline int add_key_tid(struct task_struct *p,  int slot)
{
	int i;
	unsigned long flags;
	pid_t tgid;
	pid_t tid = p->pid;
	unsigned long slot_num = 0;
	tgid = atomic_read(&game_pid);
	if(slot>=MAX_KEY_TASK || !tgid || tgid != p->tgid)
		return 0;
	tracing_mark_write(p->tgid, "remove", tid);
	raw_spin_lock_irqsave(&key_wts_lock, flags);
	if(key_tid[slot] == 0)
	{
		key_tid[slot] = tid;
		for(i=0; i<MAX_KEY_TASK; i++){
			if(key_tid[i]!=0)
				slot_num++;
		}
		atomic_set(&key_wts_len, slot_num);
	}
	raw_spin_unlock_irqrestore(&key_wts_lock, flags);
	return 0;
}

/*
 * if task already in list, do nothing
 * if new task, LRU replace
 *
 */
static inline int add_update_key_tid(struct task_struct *p)
{
	int i;
	unsigned long slot_num = 0;
	unsigned long flags;
	pid_t tgid;
	pid_t tid = p->pid;
	tgid = atomic_read(&game_pid);
	if(!tgid || tgid != p->tgid || !tid)
		return 0;

	raw_spin_lock_irqsave(&key_wts_lock, flags);
	for(i=0; i<MAX_KEY_TASK; i++)
	{
		if(key_tid[i] == tid || !key_tid[i])
			break;
	}
	/* empty slot */
	if((i < MAX_KEY_TASK) && !key_tid[i])
	{
		goto empty_slot;
	}else if((i < MAX_KEY_TASK) && key_tid[i])
	{
		goto done;
	}

	/* place into last one */
	for(i=0; i<MAX_KEY_TASK-1; i++)
	{
		key_tid[i] = key_tid[i+1];
		key_wts_sum_started[i] = key_wts_sum_started[i+1];
		key_wts_sum_run[i] = key_wts_sum_run[i+1];
		key_wts_sum_run_first[i] = key_wts_sum_run_first[i+1];
		key_wts_sum_run_start_ts[i] = key_wts_sum_run_start_ts[i+1];
	}
empty_slot:
	key_tid[i] = tid;
	key_wts_sum_started[i] = WTS_SUM_NOT_DEF;
	key_wts_sum_run[i] = 0;
	key_wts_sum_run_first[i] = 0;
	key_wts_sum_run_start_ts[i] = 0;
	for(i=0; i<MAX_KEY_TASK; i++){
		if(key_tid[i]!=0)
			slot_num++;
	}
	atomic_set(&key_wts_len, slot_num);
done:
	raw_spin_unlock_irqrestore(&key_wts_lock, flags);
	return 0;
}

static int remove_old_heavy(struct task_struct *p, struct walt_task_struct_penalty *wtsp)
{
	int i;
	unsigned long flags;
	pid_t tid = p->pid;
	if(!tid)
		return 0;
	raw_spin_lock_irqsave(&key_wts_lock, flags);
	for(i=0; i<MAX_KEY_TASK; i++)
	{
		if(key_tid[i] == tid)
			break;
	}
	if(i>=MAX_KEY_TASK)
	{
		wtsp->high_load = false;
	}
	raw_spin_unlock_irqrestore(&key_wts_lock, flags);
	return 0;
}

static inline int remove_key_tid(struct task_struct *p)
{
	int i;
	unsigned long flags;
	int slot_num  ;
	int slot = -1;

	pid_t tid = p->pid;
	tracing_mark_write(p->tgid, "remove", tid);
	raw_spin_lock_irqsave(&key_wts_lock, flags);
	slot_num = atomic_read(&key_wts_len);
	for(i=0; i<MAX_KEY_TASK; i++)
	{
		if(tid >0 && key_tid[i] == tid )
		{
			slot=i;
			break;
		}
	}
	if(slot<0)  goto done;

	slot_num--;
	if(slot_num<0)
		atomic_set(&key_wts_len, 0);
	else
		atomic_set(&key_wts_len, slot_num);

	key_tid[slot] = 0;
	key_wts_sum_started[slot] = WTS_SUM_NOT_DEF;
	key_wts_sum_run[slot] = 0;
	key_wts_sum_run_first[slot] = 0;
	key_wts_sum_run_start_ts[slot] = 0;

done:
   raw_spin_unlock_irqrestore(&key_wts_lock, flags);
   return 0;
}


static void reset_key_wts_unlocked(void)
{
	int i;
	for(i=0; i<MAX_KEY_TASK; i++)
	{
		key_tid[i] = 0;
		key_wts_sum_started[i] = WTS_SUM_NOT_DEF;
		key_wts_sum_run[i] = 0;
		key_wts_sum_run_first[i] = 0;
		key_wts_sum_run_start_ts[i] = 0;
	}
	atomic_set(&key_wts_len, 0);
}
/*
 * return <0, means couner ovreflow, need to reset
 */
static u64 update_task_running_unlocked(struct task_struct *p, u64 ts)
{
	int i;
	int slot = -1;
	pid_t tid = p->pid;
	for(i=0; i<MAX_KEY_TASK; i++){
		if(key_tid[i] == tid )
		{
			slot = i;
			break;
		}
	}
	if(slot<0){
		return SCHED_CAPACITY_SCALE;
	}
	if(key_wts_sum_started[slot]< WTS_SUM_NOT_START )
	{
		return key_wts_util[slot];
	}
	tracing_mark_write(p->pid, "stat", key_wts_sum_started[slot]);
	/* init */
	if(key_wts_sum_started[slot] == WTS_SUM_NOT_START)
	{
		key_wts_sum_started[slot] = WTS_SUM_STARTED;
		tracing_mark_write(p->pid, "stat", key_wts_sum_started[slot]);
		key_wts_util[slot] = SCHED_CAPACITY_SCALE;
		/* udpate if expired */
		if(key_wts_sum_run_first[slot]>0)
		{
			key_wts_sum_run[slot] = p->sched_info.run_delay +
			p->se.sum_exec_runtime - key_wts_sum_run_first[slot];
			tracing_mark_write(p->pid, "key_wts_sum_run", key_wts_sum_run[slot]);
			if( key_wts_sum_run_start_ts[slot] < ts)
			{
				key_wts_util[slot] = div64_u64(key_wts_sum_run[slot]<<SCHED_CAPACITY_SHIFT,
				(ts- key_wts_sum_run_start_ts[slot]));

			}

		}
		key_wts_sum_run_first[slot] = p->sched_info.run_delay +
			p->se.sum_exec_runtime;
		key_wts_sum_run [slot] = 0;
		key_wts_sum_run_start_ts[slot] =  ts;
		return key_wts_util[slot];
	}
	tracing_mark_write(p->pid, "key_wts_sum_run_first", key_wts_sum_run_first[slot]);
	tracing_mark_write(p->pid, "key_wts_sum_run", key_wts_sum_run[slot]);
	tracing_mark_write(p->pid, "key_wts_sum_run_start_ts", key_wts_sum_run_start_ts[slot]);

	return key_wts_util[slot];
}

static u64 update_task_running_locked(struct task_struct *p, u64 ts)
{
	unsigned long flags;
	u64 ret;
	raw_spin_lock_irqsave(&key_wts_lock, flags);
	ret = update_task_running_unlocked(p, ts);
	raw_spin_unlock_irqrestore(&key_wts_lock, flags);
	return ret;
}

static int reset_wts_stat(pid_t pid){
	unsigned long flags;
	raw_spin_lock_irqsave(&key_wts_lock, flags);
	frame_num = 0;
	atomic_set(&game_pid, pid);
	atomic_set(&top_task_pid, 0);
	reset_key_wts_unlocked();
	raw_spin_unlock_irqrestore(&key_wts_lock, flags);
	return 0;
}
/* monitor process is die */
static void android_rvh_flush_task(void *unused, struct task_struct *p){
	pid_t tgid;
	tgid = atomic_read(&game_pid);
	if( tgid >0 && (tgid == p->pid))
	{
		reset_wts_stat(0);
	}else if (tgid >0 && (tgid == p->tgid))
	{
		remove_key_tid(p);
	}
}
static void android_rvh_before_do_sched_yield(void *unused, long *skip)
{
	u64 delta_total;
	u64 clock;
	u64 frame_time_ns_;
	unsigned long flags;
	struct penalty_status * ps;

	pid_t tgid;
	u64 sleep_ns = 0;
	struct	task_struct *p = current;
	frame_time_ns_ = frame_time_ns;
	if(!(sysctl_sched_auto_penaty & ENABLE_SCHED) || 0 == frame_time_ns_ )
	{
		return ;
	}

	tgid = atomic_read(&game_pid);
	if (tgid >0 && tgid != p->tgid){
		return ;
	}
	*skip = 1;
	raw_spin_lock_irqsave(&penalty_lock, flags);

	clock = sched_clock();
	ps = get_penalty_status_lru(p->pid, clock);
	ps->count++;
	tracing_mark_write(p->pid, "last_yield_time", ps->last_yield_time);
	tracing_mark_write(p->pid, "clock", clock);
	if(ps->last_yield_time>0 && (clock - ps->last_yield_time) > MAX_YIELD_SLEEP)   /*refresh start of yield group*/
	{
		ps->last_sleep_ns = ps->prev_yield_total ;
		sleep_ns = ps->last_sleep_ns >> 1; /* try to sleep 1/2 */
		ps->last_sleep_ns = min(ps->last_sleep_ns, (u32)MAX_YIELD_SLEEP);
		ps->est_yield_end = ps->last_yield_time + frame_time_ns_ - MIN_YIELD_SLEEP_HEADROOM;
	}else
	{
		sleep_ns = ps->last_sleep_ns >> 1; /* try to sleep 1/4 */
	}
	sleep_ns = max(sleep_ns, MIN_YIELD_SLEEP);
	sleep_ns = min(sleep_ns, MAX_YIELD_SLEEP);
	ps->last_sleep_ns = sleep_ns;

	sleep_ns = max(sleep_ns, (u64)penalty_headroom);
	if( clock >= ps->est_yield_end)
	{
		sleep_ns = 0;
	}else if ((clock + sleep_ns) > ps->est_yield_end)
	{
		sleep_ns = ps->est_yield_end -clock;
	}

	delta_total = div64_u64(sleep_ns, 1000);


	raw_spin_unlock_irqrestore(&penalty_lock, flags);

	if(sleep_ns >0)
		usleep_range_state(delta_total, delta_total, TASK_IDLE);

	raw_spin_lock_irqsave(&penalty_lock, flags);
	ps = get_penalty_status_lru(p->pid, clock);
	ps->last_yield_time = sched_clock();
	delta_total = ps->last_yield_time  - clock;
	atomic64_add(delta_total, &ps->yield_total);
	raw_spin_unlock_irqrestore(&penalty_lock, flags); 
	
	tracing_mark_write(p->pid, "yield_total", atomic64_read(&ps->yield_total));
	
	/*account stat of yield */
	atomic_add(delta_total, &yield_ns_total);
	atomic_inc(&yield_cnt_total);
}

static int msm_update_handler_post(struct kretprobe_instance *p, struct pt_regs *regs)
{
	sched_update_frame_stat(regs->regs[0], regs->regs[1],
		regs->regs[2], regs->regs[3],
		regs->regs[4],  regs->regs[5]);
	return 0;
}

static int  msm_update_kprobe_init(void)
{
	int ret;
	kp_msm_update.kp.addr = NULL;
	kp_msm_update.kp.flags = 0;
	kp_msm_update.handler = NULL;
	kp_msm_update.entry_handler = msm_update_handler_post ;

	ret = register_kretprobe(&kp_msm_update);
	if (ret < 0)
	{
		pr_info("register_kretprobe failed in %s , returned %d\n", __FUNCTION__, ret);
		return ret;
	}
	pr_info("register_kretprobe sucess =%d %s \n", ret, __FUNCTION__);
	return 0;
}

static int  msm_update_kprobe_deinit(void)
{
	unregister_kretprobe(&kp_msm_update);
	pr_info("unregister_kretprobe  %s", __FUNCTION__);
	return 0;
}

/* update in context of current queueTask
 *
 * update_typ should be MSM_PERF_GFX (0)
 * type : MSM_PERF_QUEUE (1), ....
 *
 *
 */
static void sched_update_frame_stat( int update_typ, int type, pid_t pid, uint32_t ctx_id,
	uint32_t timestamp, bool end_of_frame )
{
	unsigned long flags;
	int i;
	u64 ts, avg, total_fast ;
	u64 frame_time_expected_total_;
	if( sysctl_sched_auto_penaty < 1 )
	{
		return ;
	}

	if( pid!=atomic_read(&game_pid) || type != 1 ||
		timestamp != 0 ||end_of_frame != 0)
		return ;
	if(atomic_read(&key_wts_len) < 1)
	{
		return ;
	}
	tracing_mark_write(current->pid, "type", type);
	tracing_mark_write(current->pid, "ctx_id", ctx_id);
	tracing_mark_write(current->pid, "update_typ", update_typ);
	tracing_mark_write(current->pid, "timestamp", timestamp);
	tracing_mark_write(current->pid, "end_of_frame", end_of_frame);

	raw_spin_lock_irqsave(&key_wts_lock, flags);
 
	if(atomic_read(&key_wts_len) < 1)
	{
		tracing_mark_write(current->pid, "skip", 1);
		frame_num = 0;
		goto done ;
	}

	if(!frame_time_ns)
	{
		tracing_mark_write(current->pid, "skip", 2);
		frame_num = 0;
		goto done;
	}
	tracing_mark_write(current->pid, "skip", 0);
	ts = sched_clock();
	if(frame_num == 0)
	{
		total_frame_time = 0;
		last_frame_ts = ts;
		first_frame_ts = last_frame_ts;
		frame_num++;
		goto done;
	}

	frame_time_expected_total_ = frame_num*frame_time_ns;
	total_frame_time = ts - first_frame_ts;
	/* to tell fps healty or not */
	atomic_set(&healthy, FPS_HEALTHY);
	if(total_frame_time > frame_time_expected_total_)
	{
		#if DEBUG
		tracing_mark_write(current->pid, "total_slow_us",
			div64_u64( (total_frame_time-frame_time_expected_total_), 1000));
		#endif
		continous_drop++;
		/*if we fall behind too much, if lag too much, migh not righ fps is set */
		if((total_frame_time - frame_time_expected_total_) > slow_thresh)
		{
			if((total_frame_time - frame_time_expected_total_) < frame_time_ns)
				atomic_set(&healthy, FPS_VERY_LOW);
			else
				atomic_set(&healthy, FPS_LOW);
		}else if(continous_drop >= MAX_CONTINOUS_DROP){ /*keep frame drop for long time */
			atomic_set(&healthy, FPS_LOW);
		}
	}else
	{
		total_fast = frame_time_expected_total_ - total_frame_time;
		/* if no much headroom, consider it's not health */
		if(total_fast < fast_thresh )
			atomic_set(&healthy, FPS_LOW);
		#if DEBUG
		tracing_mark_write(current->pid, "total_fast_us",
			div64_u64((frame_time_expected_total_ - total_frame_time), 1000));
		#endif
		continous_drop = 0;
	}
	avg = 0;
	#if DEBUG
	if( (ts-last_frame_ts) > frame_time_ns)
	{
		tracing_mark_write(current->pid, "slow_us",
			div64_u64( (ts-last_frame_ts-frame_time_ns), 1000));
	}else
	{
		tracing_mark_write(current->pid, "fast_us",
			div64_u64( (frame_time_ns- (ts-last_frame_ts)), 1000));
	}
	avg = div64_u64(total_frame_time, frame_num*1000);
	tracing_mark_write(current->pid, "frame_num", frame_num);
	tracing_mark_write(current->pid, "avg_frame_time_us", avg);
	tracing_mark_write(current->pid, "frame_num", frame_num);
	#endif
	tracing_mark_write(current->pid, "healthy", atomic_read(&healthy));

	frame_num++;
	last_frame_ts = ts;


	if(frame_num > max_frame)
	{
		frame_num = 1;
		first_frame_ts = last_frame_ts;
		/* rollover */
		for(i=0; i< MAX_KEY_TASK; i++){
		    key_wts_sum_started[i] = WTS_SUM_NOT_START;
		}
	}
done:
	raw_spin_unlock_irqrestore(&key_wts_lock, flags);
}


static struct ctl_table penalty_table[] =
{
    {
		.procname	= "sched_auto_penalty",
		.data		= &sysctl_sched_auto_penaty,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= yield_penaty,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &large_penalty,
	},
	{ }
};

static struct ctl_table penalty_base_table[] = {
	{
		.procname	= "walt",
		.mode		= 0555,
		.child		= penalty_table,
	},
	{ },
};

static void penalty_init_sysctl(void)
{
    struct ctl_table_header *hdr;
    hdr = register_sysctl_table(penalty_base_table);
	kmemleak_not_leak(hdr);
}

static struct kset *penalty_kset;
static struct kobject *param_kobj;

static ssize_t set_min_penalty_percpu(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret)
	{
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return ret;
	}
	if(usr_val<410)
		usr_val = 410;
	if(usr_val > 1024)
		usr_val = 1024;
	atomic_set(&scale_min_percpu, usr_val);
	return count;
}
static ssize_t get_min_penalty_percpu(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	long usr_val  = atomic_read(&scale_min_percpu);
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}

static ssize_t set_min_penalty_pertask(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret) {
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return ret;
	}
	if(usr_val<410)
		usr_val = 410;
	if(usr_val > 1024)
		usr_val = 1024;
	atomic_set(&scale_min_pertask, usr_val);
	return count;
}
static ssize_t get_min_penalty_pertask(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	long usr_val  = atomic_read(&scale_min_pertask);
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}

static ssize_t set_penalty_debug(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret) {
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return ret;
	}
	sysctl_penalty_debug = usr_val;

	return count;
}
static ssize_t get_penalty_debug(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	long usr_val  = sysctl_penalty_debug;
	int i = 0;
	long total_win_ = total_win;
	long yield_cnt_total_ = atomic_read(&yield_cnt_total);
	long yield_ns_total_ = atomic_read(&yield_ns_total);
	if(total_win_<1)
		total_win_ =1;
	if(yield_cnt_total_<1)
		yield_cnt_total_ = 1;
	i += scnprintf(buf+i, PAGE_SIZE, "%ld\n", usr_val);
	i += scnprintf(buf+i, PAGE_SIZE,
	"total_win=%lu total_penalty_win=%lu total_low=%lu total_very_low=%lu  \n",
	total_win_, total_penalty_win, total_low, total_very_low );
	i += scnprintf(buf+i, PAGE_SIZE,
	"percent:  total_penalty_win=%lu  total_low=%lu total_very_low=%lu \n",
	div64_u64(total_penalty_win*100, total_win_),
	div64_u64(total_low*100, total_win_),
	div64_u64(total_very_low*100, total_win_));
	i += scnprintf(buf+i, PAGE_SIZE, "_yield_total=%luus cnt=%lu avg=%luus per=%lu\n", div64_u64(yield_ns_total_,1000),
		yield_cnt_total_, div64_u64(yield_ns_total_,yield_cnt_total_*1000), div64_u64(yield_ns_total_,50000000UL) );
	i += scnprintf(buf+i, PAGE_SIZE,
	"target_fps=%d: fast_thresh=%lu slow_thresh=%lu  max_frame=%lu \n",
			 target_fps, fast_thresh, slow_thresh, max_frame);
	i += scnprintf(buf+i, PAGE_SIZE,
	"ut_scale=%lu ur_sc=%lu penalty_pl=%lu  penalty_per_task=%lu \n",
	 atomic_read(&scale_min_percpu),  atomic_read(&scale_min_pertask),
	 penalty_pl, sleep_penalty_per_task);

	total_win = 0;
	total_penalty_win = 0;
	total_low = 0;
	total_very_low = 0;
	atomic_set(&yield_cnt_total, 0);
	atomic_set(&yield_ns_total, 0);
	return i;
}

static ssize_t set_penalty_pl(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret) {
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return ret;
	}
	penalty_pl = usr_val;
	return count;
}
static ssize_t get_penalty_pl(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	long usr_val  = penalty_pl;
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}


static ssize_t set_max_fps_drop(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret) {
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return ret;
	}
	max_fps_drop = usr_val;
	return count;
}
static ssize_t get_max_fps_drop(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	long usr_val  = max_fps_drop;
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}


static ssize_t set_sleep_penalty_per_task(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret) {
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return ret;
	}
	sleep_penalty_per_task = usr_val;
	return count;
}
static ssize_t get_sleep_penalty_per_task(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	long usr_val  = sleep_penalty_per_task;
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}


static ssize_t set_penalty_headroom(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret) {
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return count;
	}
	penalty_headroom = usr_val;
	return count;
}
static ssize_t get_penalty_headroom(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	long usr_val  = penalty_headroom;
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}


static ssize_t set_target_fps(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u64 low, high;
	int ret;
	unsigned long flags;
	long usr_val = 0;

	ret = kstrtol(buf, 0, &usr_val);
	if (ret)
	{
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return count;
	}
	if(usr_val< 0 || usr_val >240)
	{
		return count;
	}
	raw_spin_lock_irqsave(&key_wts_lock, flags);
	target_fps = usr_val;
	if(target_fps>0)
	{
		frame_time_ns = div64_u64(1000000000UL, target_fps);
		if(max_fps_drop > 1 && max_fps_drop*2 < target_fps)
		{
			high = div64_u64(1000000000UL, (target_fps-max_fps_drop))-
			frame_time_ns;
			low = div64_u64(1000000000UL,
				(target_fps-max_fps_drop + 1))-frame_time_ns;
			slow_thresh = ((high + low )*max_frame)  >> 1;
		}else if (max_fps_drop == 1)
		{
			high = div64_u64(1000000000UL,
				(target_fps-max_fps_drop))-frame_time_ns;
			slow_thresh = high*max_frame;
		}
	}else if (target_fps ==0)
	{
		frame_time_ns = 0;
	}
	raw_spin_unlock_irqrestore(&key_wts_lock, flags);

	return count;
}
static ssize_t get_target_fps(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf){
	long usr_val  = target_fps;
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}

static ssize_t set_target_pid(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret)
	{
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return count;
	}
	if(usr_val< 0 )
	{
		return count;
	}
	reset_wts_stat(usr_val);

	return count;
}
static ssize_t get_target_pid(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	pid_t usr_val  = atomic_read(&game_pid);
	return scnprintf(buf, PAGE_SIZE, "%d\n", usr_val);
}

static struct kobj_attribute attr1 =
	__ATTR(scale_min_percpu, 0644, get_min_penalty_percpu,
	set_min_penalty_percpu);
static struct kobj_attribute attr2 =
	__ATTR(scale_min_pertask, 0644, get_min_penalty_pertask,
	set_min_penalty_pertask);
static struct kobj_attribute attr3 =
	__ATTR(penalty_debug, 0644, get_penalty_debug, set_penalty_debug);
static struct kobj_attribute attr4 =
	__ATTR(penalty_pl, 0644, get_penalty_pl, set_penalty_pl);
static struct kobj_attribute attr5 =
	__ATTR(penalty_per_task, 0644, get_sleep_penalty_per_task,
	set_sleep_penalty_per_task);
static struct kobj_attribute attr6 =
	__ATTR(penalty_headroom, 0644, get_penalty_headroom,
	set_penalty_headroom);
static struct kobj_attribute attr7 =
	__ATTR(fps, 0644, get_target_fps, set_target_fps);
static struct kobj_attribute attr8 =
	__ATTR(target_pid, 0644, get_target_pid, set_target_pid);
static struct kobj_attribute attr9 =
	__ATTR(max_fps_drop, 0644, get_max_fps_drop, set_max_fps_drop);


static struct attribute *param_attrs[] =
{
	&attr1.attr,
	&attr2.attr,
	&attr3.attr,
	&attr4.attr,
	&attr5.attr,
	&attr6.attr,
	&attr7.attr,
	&attr8.attr,
	&attr9.attr,
	NULL };
static struct attribute_group param_attr_group =
{
	.attrs = param_attrs,
};

static int init_module_params(void)
{
	int ret;
	struct kobject *module_kobj;
	penalty_kset = kset_create_and_add("sched_penalty", NULL, kernel_kobj);

	if (!penalty_kset)
	{
		pr_err("Failed to create sched_penalty root object\n");
		return -1;
	}
	module_kobj = &penalty_kset->kobj;
	param_kobj = kobject_create_and_add("parameters", module_kobj);
	if (!param_kobj)
	{
		pr_err("sched_penalty: Failed to add param_kobj\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(param_kobj, &param_attr_group);
	if (ret)
	{
		pr_err("sched_penalty: Failed to create sysfs\n");
		return ret;
	}
	return 0;
}

static int deinit_module_params(void)
{
	if(param_kobj)
	{
		kobject_del(param_kobj);
		kobject_put(param_kobj);
	}
	if(penalty_kset)
		kset_unregister(penalty_kset);
	return 0;
}


static int __init penalty_init(void){
	struct sched_opt opt;
	pr_info("in penalty_init\n");
	if(sysctl_sched_auto_penaty>0)
	{
		msm_update_kprobe_init();
	}

	opt.update_yield_ts = update_yield_ts;
	opt.update_yield_util = update_yield_util;
	opt.penalize_yield = penalize_yield;
	opt.account_wakeup = account_wakeup;
	opt.rollover_task_window = rollover_task_window;
	opt.cluster_init = cluster_init;
	opt.cluster_update = cluster_update;
	opt.policy_load = policy_load;
	opt.rollover_cpu_window = rollover_cpu_window;
	opt.eval_need = eval_need;
	opt.update_heavy = update_heavy;

	update_sched_opt(&opt, true);
	run_callback = opt.waltgov_run_callback;

	penalty_init_sysctl();
	init_module_params();
	register_trace_android_rvh_flush_task(android_rvh_flush_task, NULL);
	register_trace_android_rvh_before_do_sched_yield(android_rvh_before_do_sched_yield, NULL);
    return 0;
}

static void __exit penalty_exit(void)
{
	struct sched_opt opt;
	pr_info("in penalty_exit\n");
	if(sysctl_sched_auto_penaty)
	{
		msm_update_kprobe_deinit();
	}
	update_sched_opt(&opt, false);
	run_callback = 0;
	deinit_module_params();
}

module_init(penalty_init);
module_exit(penalty_exit);

MODULE_DESCRIPTION("QTI WALT optimization module");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("post: sched-walt");
MODULE_SOFTDEP("post: msm_performance");

