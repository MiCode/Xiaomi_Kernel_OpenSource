// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#ifndef _XM_LOCKING_MAIN_H_
#define _XM_LOCKING_MAIN_H_

#define cond_trace_printk(cond, fmt, ...)	\
do {										\
	if (cond)								\
		trace_printk(fmt, ##__VA_ARGS__);	\
} while (0)


#define CONFIG_XM_LOCKING_MONITOR   XIAOMI_LOCKING_MONITOR_FEATURE
#define	CONFIG_XM_LOCKING_OSQ       XIAOMI_LOCKING_OSQ_FEATURE
//#define	CONFIG_XM_INTERNAL_VERSION  XIAOMI_INTERNAL_VERSION_FEATURE

#define MAGIC_NUM       (0xdead0000)
#define MAGIC_MASK      (0xffff0000)
#define MAGIC_SHIFT     (16)
#define OWNER_BIT       (1 << 0)
#define THREAD_INFO_BIT (1 << 1)
#define TYPE_BIT        (1 << 2)

#ifndef MI_LOCK_LOG_TAG
#define MI_LOCK_LOG_TAG Log_tag
#endif

#define ml_err(fmt, ...) \
		pr_err("[mi_locking][MI_LOCK_LOG_TAG][%s]"fmt, __func__, ##__VA_ARGS__)
#define ml_warn(fmt, ...) \
		pr_warn("[mi_locking][MI_LOCK_LOG_TAG][%s]"fmt, __func__, ##__VA_ARGS__)
#define ml_info(fmt, ...) \
		pr_info("[mi_locking][MI_LOCK_LOG_TAG][%s]"fmt, __func__, ##__VA_ARGS__)

#define WAIT_LK_ENABLE (1 << 0)
//#define WAIT_LK_RWSEM_ENABLE (1 << 1)
#define HOLD_LK_ENABLE (1 << 1)
//#define HOLD_LK_RWSEM_ENABLE (1 << 3)


#ifdef CONFIG_XM_LOCKING_MONITOR
/*
 * The bit definitions of the g_opt_enable:
 * bit 0-7: reserved bits for other locking optimation.
 * bit8 ~ bit10(each monitor version is exclusive):
 * 1 : monitor control, level-0(internal version).
 * 2 : monitor control, level-1(trial version).
 * 3 : monitor control, level-2(official version).
 */
#define LK_MONITOR_SHIFT  (8)
#define LK_MONITOR_MASK   (7 << LK_MONITOR_SHIFT)
#define LK_MONITOR_LEVEL0 (1 << LK_MONITOR_SHIFT)
#define LK_MONITOR_LEVEL1 (2 << LK_MONITOR_SHIFT)
#define LK_MONITOR_LEVEL2 (3 << LK_MONITOR_SHIFT)
#endif

#define LK_DEBUG_PRINTK (1 << 0)
//#define LK_DEBUG_FTRACE (1 << 1)

extern unsigned int g_opt_enable;
extern unsigned int g_opt_debug;
extern unsigned int g_opt_stack;
extern unsigned int g_opt_sort;
extern unsigned int g_opt_nvcsw;

static inline bool locking_opt_enable(unsigned int enable)
{
	return g_opt_enable & enable;
}

static inline bool lock_supp_level(int level)
{
	return (g_opt_enable & LK_MONITOR_MASK) == level;
}

static inline bool locking_opt_debug(int debug)
{
	return g_opt_debug & debug;
}

int kern_lstat_init(void);
void kern_lstat_exit(void);



#endif /* _XM_LOCKING_MAIN_H_ */
