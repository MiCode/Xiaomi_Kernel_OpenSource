#ifndef __CORE_CTL_H
#define __CORE_CTL_H

extern void core_ctl_tick(u64 wallclock);
extern int core_ctl_set_min_cpus(unsigned int cid, unsigned int min);
extern int core_ctl_set_max_cpus(unsigned int cid, unsigned int max);
extern int core_ctl_set_offline_throttle_ms(unsigned int cid,
					     unsigned int throttle_ms);
extern int core_ctl_set_limit_cpus(int unsigned cid,
				   int unsigned min,
				   int unsigned max);
extern int core_ctl_set_not_preferred(int cid, int cpu, bool enable);
extern int core_ctl_set_boost(bool boost);
#endif

