#ifndef __MISYSINFOFREADER_H__
#define __MISYSINFOFREADER_H__

#ifdef CONFIG_MISYSINFOFREADER
extern u32 *misysinfo_jiffies;
static __always_inline void update_misysinfo_jiffies(void)
{
	if (likely(misysinfo_jiffies != NULL)) {
		*misysinfo_jiffies = jiffies + ((u32)(0xFFFFFFFFU) - (u32)INITIAL_JIFFIES + 1);
	}
}
void misysinfo_update_slowpath(u64 time_stamp_ns, u64 time_ns);
#else
static __always_inline void update_misysinfo_jiffies(void) {}
static __always_inline void misysinfo_update_slowpath(u64 time_stamp_ns, u64 time_ns) {}
#endif

#endif // __MISYSINFOFREADER_H__
