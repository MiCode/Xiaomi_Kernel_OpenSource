/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#ifndef _TMS_DEBUGER_H_
#define _TMS_DEBUGER_H_

/*********** PART0: Head files ***********/
#include <linux/proc_fs.h>
#include <linux/sched.h>

/*********** PART1: Define Area ***********/
#define TMS_MOUDLE "debuger"

/*********** PART2: Struct Area ***********/

/*********** PART3: Function or variables for other files ***********/
extern uint8_t tms_log_level;
void tms_buffer_dump(const char *tag, const uint8_t *src, int16_t len);
int tms_debuger_proc_create(struct proc_dir_entry *prEntry);
int tms_debuger_init(void);
void tms_debuger_deinit(void);

/*********** PART4: Log Define Area ***********/
#define LOG_LEVEL_ERROR   0 /* print dirver error info */
#define LOG_LEVEL_WARN    1 /* print driver warning info */
#define LOG_LEVEL_INFO    2 /* print basic debug info */
#define LOG_LEVEL_DEBUG   3 /* print all debug info */
#define LOG_LEVEL_DUMP    4 /* print I/O buffer info */

#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#ifdef TMS_DEBUGER_LOGGER
void logger_cache(const char *fmt, ...);
#define LOG_CACHE(fmt, arg...) \
    logger_cache("<TMS-%s>%s: " fmt, TMS_MOUDLE, __func__, ##arg)
#else
#define LOG_CACHE(fmt, arg...)
#endif

#ifdef TMS_DEBUGER_CHECKER
void checker_trace(const char *file, const char *func, int line, pid_t pid);
#define CHECKER_INFO(file, func, line, pid) \
    checker_trace(file, func, line, pid)
#else
#define CHECKER_INFO(file, func, line, pid)
#endif

#define TMS_PRINT(fmt, arg...) \
    do{ \
        printk(KERN_ERR "<TMS-%s>%s: " fmt, TMS_MOUDLE, __func__, ##arg); \
    }while(0)

#define TMS_LOG(fmt, arg...) \
    do{ \
        pr_err("<TMS-%s>%s: " fmt, TMS_MOUDLE, __func__, ##arg); \
        LOG_CACHE(fmt, ##arg); \
    }while(0)

#define TMS_ERR(fmt, arg...) \
    do{ \
        TMS_LOG(fmt, ##arg); \
        CHECKER_INFO(__FILENAME__, __func__, __LINE__, current->pid); \
    }while(0)

#define TMS_WARN(fmt, arg...) \
    do{ \
        if (tms_log_level >= LOG_LEVEL_WARN) \
            TMS_LOG(fmt, ##arg); \
    }while(0)

#define TMS_INFO(fmt, arg...) \
    do{ \
        if (tms_log_level >= LOG_LEVEL_INFO) \
            TMS_LOG(fmt, ##arg); \
    }while(0)

#define TMS_DEBUG(fmt, arg...) \
    do{ \
        if (tms_log_level >= LOG_LEVEL_DEBUG) \
            TMS_LOG(fmt, ##arg); \
    }while(0)

#define TMS_DUMP(level, tag, src, len) \
    do{ \
        TMS_DEBUG("%s[%zu] bytes", tag, len); \
        if (tms_log_level >= level) \
            tms_buffer_dump(tag, src, len); \
    }while(0)

#endif /* _TMS_DEBUGER_H_ */
