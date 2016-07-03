#ifndef __LINUX_STATE_HELPER_H
#define __LINUX_STATE_HELPER_H

#include <linux/state_notifier.h>

extern void reschedule_helper(void);
extern void batt_level_notify(int);
extern void thermal_notify(int cpu, int status);
extern void thermal_level_relay(long);

#endif /* _LINUX_STATE_HELPER_H */
