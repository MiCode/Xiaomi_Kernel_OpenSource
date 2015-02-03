/*
 * CPU ConCurrency (CC) is measures the CPU load by averaging
 * the number of running tasks. Using CC, the scheduler can
 * evaluate the load of CPUs to improve load balance for power
 * efficiency without sacrificing performance.
 *
 * Copyright (C) 2013 Intel, Inc.,
 *
 * Author: Du, Yuyang <yuyang.du@intel.com>
 *
 */

#ifdef CONFIG_CPU_CONCURRENCY

#include "sched.h"

/*
 * the sum period of time is 2^26 ns (~64) by default
 */
unsigned long sysctl_concurrency_sum_period = 26UL;

/*
 * the number of sum periods, after which the original
 * will be reduced/decayed to half
 */
unsigned long sysctl_concurrency_decay_rate = 1UL;

/*
 * the contrib period of time is 2^10 (~1us) by default,
 * us has better precision than ms, and
 * 1024 makes use of faster shift than div
 */
static unsigned long cc_contrib_period = 10UL;

/*
 * the concurrency is scaled up for decaying,
 * thus, concurrency 1 is effectively 2^cc_resolution (1024),
 * which can be halved by 10 half-life periods
 */
static unsigned long cc_resolution = 10UL;

/*
 * after this number of half-life periods, even
 * (1>>32)-1 (which is sufficiently large) is less than 1
 */
static unsigned long cc_decay_max_pds = 32UL;

static inline unsigned long cc_scale_up(unsigned long c)
{
	return c << cc_resolution;
}

static inline unsigned long cc_scale_down(unsigned long c)
{
	return c >> cc_resolution;
}

/* from nanoseconds to sum periods */
static inline u64 cc_sum_pds(u64 n)
{
	return n >> sysctl_concurrency_sum_period;
}

/* from sum period to timestamp in ns */
static inline u64 cc_timestamp(u64 p)
{
	return p << sysctl_concurrency_sum_period;
}

/*
 * from nanoseconds to contrib periods, because
 * ns so risky that can overflow cc->contrib
 */
static inline u64 cc_contrib_pds(u64 n)
{
	return n >> cc_contrib_period;
}

/*
 * cc_decay_factor only works for 32bit integer,
 * cc_decay_factor_x, x indicates the number of periods
 * as half-life (sysctl_concurrency_decay_rate)
 */
static const unsigned long cc_decay_factor_1[] = {
	0xFFFFFFFF,
};

static const unsigned long cc_decay_factor_2[] = {
	0xFFFFFFFF, 0xB504F333,
};

static const unsigned long cc_decay_factor_4[] = {
	0xFFFFFFFF, 0xD744FCCA, 0xB504F333, 0x9837F051,
};

static const unsigned long cc_decay_factor_8[] = {
	0xFFFFFFFF, 0xEAC0C6E7, 0xD744FCCA, 0xC5672A11,
	0xB504F333, 0xA5FED6A9, 0x9837F051, 0x8B95C1E3,
};

/* by default sysctl_concurrency_decay_rate */
static const unsigned long *cc_decay_factor =
	cc_decay_factor_1;

/*
 * cc_decayed_sum depends on cc_resolution (fixed 10),
 * cc_decayed_sum_x, x indicates the number of periods
 * as half-life (sysctl_concurrency_decay_rate)
 */
static const unsigned long cc_decayed_sum_1[] = {
	0, 512, 768, 896, 960, 992,
	1008, 1016, 1020, 1022, 1023,
};

static const unsigned long cc_decayed_sum_2[] = {
	0, 724, 1235, 1597, 1853, 2034, 2162, 2252,
	2316, 2361, 2393, 2416, 2432, 2443, 2451,
	2457, 2461, 2464, 2466, 2467, 2468, 2469,
};

static const unsigned long cc_decayed_sum_4[] = {
	0, 861, 1585, 2193, 2705, 3135, 3497, 3801, 4057,
	4272, 4453, 4605, 4733, 4840, 4930, 5006, 5070,
	5124, 5169, 5207, 5239, 5266, 5289, 5308, 5324,
	5337, 5348, 5358, 5366, 5373, 5379, 5384, 5388,
	5391, 5394, 5396, 5398, 5400, 5401, 5402, 5403,
	5404, 5405, 5406,
};

static const unsigned long cc_decayed_sum_8[] = {
	0, 939, 1800, 2589, 3313, 3977, 4585, 5143,
	5655, 6124, 6554, 6949, 7311, 7643, 7947, 8226,
	8482, 8717, 8932, 9129, 9310, 9476, 9628, 9767,
	9895, 10012, 10120, 10219, 10309, 10392, 10468, 10538,
	10602, 10661, 10715, 10764, 10809, 10850, 10888, 10923,
	10955, 10984, 11011, 11036, 11059, 11080, 11099, 11116,
	11132, 11147, 11160, 11172, 11183, 11193, 11203, 11212,
	11220, 11227, 11234, 11240, 11246, 11251, 11256, 11260,
	11264, 11268, 11271, 11274, 11277, 11280, 11282, 11284,
	11286, 11288, 11290, 11291, 11292, 11293, 11294, 11295,
	11296, 11297, 11298, 11299, 11300, 11301, 11302,
};

/* by default sysctl_concurrency_decay_rate */
static const unsigned long *cc_decayed_sum = cc_decayed_sum_1;

/*
 * the last index of cc_decayed_sum array
 */
static unsigned long cc_decayed_sum_len =
	sizeof(cc_decayed_sum_1) / sizeof(cc_decayed_sum_1[0]) - 1;

/*
 * sysctl handler to update decay rate
 */
int concurrency_decay_rate_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (ret || !write)
		return ret;

	switch (sysctl_concurrency_decay_rate) {
	case 1:
		cc_decay_factor = cc_decay_factor_1;
		cc_decayed_sum = cc_decayed_sum_1;
		cc_decayed_sum_len = sizeof(cc_decayed_sum_1) /
			sizeof(cc_decayed_sum_1[0]) - 1;
		break;
	case 2:
		cc_decay_factor = cc_decay_factor_2;
		cc_decayed_sum = cc_decayed_sum_2;
		cc_decayed_sum_len = sizeof(cc_decayed_sum_2) /
			sizeof(cc_decayed_sum_2[0]) - 1;
		break;
	case 4:
		cc_decay_factor = cc_decay_factor_4;
		cc_decayed_sum = cc_decayed_sum_4;
		cc_decayed_sum_len = sizeof(cc_decayed_sum_4) /
			sizeof(cc_decayed_sum_4[0]) - 1;
		break;
	case 8:
		cc_decay_factor = cc_decay_factor_8;
		cc_decayed_sum = cc_decayed_sum_8;
		cc_decayed_sum_len = sizeof(cc_decayed_sum_8) /
			sizeof(cc_decayed_sum_8[0]) - 1;
		break;
	default:
		return -EINVAL;
	}

	cc_decay_max_pds *= sysctl_concurrency_decay_rate;

	return 0;
}

/*
 * decay concurrency at some decay rate
 */
static inline u64 decay_cc(u64 cc, u64 periods)
{
	u32 periods_l;

	if (periods <= 0)
		return cc;

	if (unlikely(periods >= cc_decay_max_pds))
		return 0;

	/* now period is not too large */
	periods_l = (u32)periods;
	if (periods_l >= sysctl_concurrency_decay_rate) {
		cc >>= periods_l / sysctl_concurrency_decay_rate;
		periods_l %= sysctl_concurrency_decay_rate;
	}

	if (!periods_l)
		return cc;

	cc *= cc_decay_factor[periods_l];

	return cc >> 32;
}

/*
 * add missed periods by predefined constants
 */
static inline u64 cc_missed_pds(u64 periods)
{
	if (periods <= 0)
		return 0;

	if (periods > cc_decayed_sum_len)
		periods = cc_decayed_sum_len;

	return cc_decayed_sum[periods];
}

/*
 * scale up nr_running, because we decay
 */
static inline unsigned long cc_weight(unsigned long nr_running)
{
	/*
	 * scaling factor, this should be tunable
	 */
	return cc_scale_up(nr_running);
}

static inline void
__update_concurrency(struct rq *rq, u64 now, struct cpu_concurrency_t *cc)
{
	u64 sum_pds, sum_pds_s, sum_pds_e;
	u64 contrib_pds, ts_contrib, contrib_pds_one;
	u64 sum_now;
	unsigned long weight;
	int updated = 0;

	/*
	 * guarantee contrib_timestamp always >= sum_timestamp,
	 * and sum_timestamp is at period boundary
	 */
	if (now <= cc->sum_timestamp) {
		cc->sum_timestamp = cc_timestamp(cc_sum_pds(now));
		cc->contrib_timestamp = now;
		return;
	}

	weight = cc_weight(cc->nr_running);

	/* start and end of sum periods */
	sum_pds_s = cc_sum_pds(cc->sum_timestamp);
	sum_pds_e = cc_sum_pds(now);
	sum_pds = sum_pds_e - sum_pds_s;
	/* number of contrib periods in one sum period */
	contrib_pds_one = cc_contrib_pds(cc_timestamp(1));

	/*
	 * if we have passed at least one period,
	 * we need to do four things:
	 */
	if (sum_pds) {
		/* 1) complete the last period */
		ts_contrib = cc_timestamp(sum_pds_s + 1);
		contrib_pds = cc_contrib_pds(ts_contrib);
		contrib_pds -= cc_contrib_pds(cc->contrib_timestamp);

		if (likely(contrib_pds))
			cc->contrib += weight * contrib_pds;

		cc->contrib = div64_u64(cc->contrib, contrib_pds_one);

		cc->sum += cc->contrib;
		cc->contrib = 0;

		/* 2) update/decay them */
		cc->sum = decay_cc(cc->sum, sum_pds);
		sum_now = decay_cc(cc->sum, sum_pds - 1);

		/* 3) compensate missed periods if any */
		sum_pds -= 1;
		cc->sum += cc->nr_running * cc_missed_pds(sum_pds);
		sum_now += cc->nr_running * cc_missed_pds(sum_pds - 1);
		updated = 1;

		/* 4) update contrib timestamp to period boundary */
		ts_contrib = cc_timestamp(sum_pds_e);

		cc->sum_timestamp = ts_contrib;
		cc->contrib_timestamp = ts_contrib;
	}

	/* current period */
	contrib_pds = cc_contrib_pds(now);
	contrib_pds -= cc_contrib_pds(cc->contrib_timestamp);

	if (likely(contrib_pds))
		cc->contrib += weight * contrib_pds;

	/* new nr_running for next update */
	cc->nr_running = rq->nr_running;

	/*
	 * we need to account for the current sum period,
	 * if now has passed 1/2 of sum period, we contribute,
	 * otherwise, we use the last complete sum period
	 */
	contrib_pds = cc_contrib_pds(now - cc->sum_timestamp);

	if (contrib_pds > contrib_pds_one / 2) {
		sum_now = div64_u64(cc->contrib, contrib_pds);
		sum_now += cc->sum;
		updated = 1;
	}

	if (updated == 1)
		cc->sum_now = sum_now;
	cc->contrib_timestamp = now;
}

void init_cpu_concurrency(struct rq *rq)
{
	rq->concurrency.sum = 0;
	rq->concurrency.sum_now = 0;
	rq->concurrency.contrib = 0;
	rq->concurrency.nr_running = 0;
	rq->concurrency.sum_timestamp = ULLONG_MAX;
	rq->concurrency.contrib_timestamp = ULLONG_MAX;
}

/*
 * we update cpu concurrency at:
 * 1) enqueue task, which increases concurrency
 * 2) dequeue task, which decreases concurrency
 * 3) periodic scheduler tick, in case no en/dequeue for long
 * 4) enter and exit idle (necessary?)
 */
void update_cpu_concurrency(struct rq *rq)
{
	/*
	 * protected under rq->lock
	 */
	struct cpu_concurrency_t *cc = &rq->concurrency;
	u64 now = rq->clock;

	__update_concurrency(rq, now, cc);
}

#endif
