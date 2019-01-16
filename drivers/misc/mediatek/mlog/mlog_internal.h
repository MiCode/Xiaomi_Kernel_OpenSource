#ifndef _MLOG_INTERNAL_H
#define _MLOG_INTERNAL_H

#include <linux/printk.h>

#define MLOG_DEBUG

#ifdef MLOG_DEBUG
#define MLOG_PRINTK(args...)    pr_debug(args)
#else
#define MLOG_PRINTK(args...)    do { } while (0)
#endif

#endif
