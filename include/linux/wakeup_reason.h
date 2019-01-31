/*
 * include/linux/wakeup_reason.h
 *
 * Logs the reason which caused the kernel to resume
 * from the suspend mode.
 *
 * Copyright (C) 2014 Google, Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_WAKEUP_REASON_H
#define _LINUX_WAKEUP_REASON_H

#define MAX_SUSPEND_ABORT_LEN 256

void log_wakeup_reason(int irq);
int check_wakeup_reason(int irq);

#ifdef CONFIG_SUSPEND
void log_suspend_abort_reason(const char *fmt, ...);
#else
static inline void log_suspend_abort_reason(const char *fmt, ...) { }
#endif
//xujia add for power analysis bugreport start
#define CCCIF_CH_LEN 200
extern int iavg4bugreport;
extern int expamp_val;
extern unsigned int wakereasons[32], wakecnt;
extern unsigned int ccb_wkcnt;
extern unsigned int cccif_wkcnt[CCCIF_CH_LEN];
extern unsigned int spm_info[6][2];
//xujia add for power analysis bugreport end
#endif /* _LINUX_WAKEUP_REASON_H */
