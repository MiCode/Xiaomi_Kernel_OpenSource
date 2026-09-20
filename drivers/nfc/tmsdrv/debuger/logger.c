// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include "../tms_common.h"
#include "logger.h"

#ifdef TMS_MOUDLE
#undef TMS_MOUDLE
#define TMS_MOUDLE "logger"
#endif

/*********** PART0: Global Variables Area ***********/
#ifdef TMS_DEBUGER_LOGGER
#define LOG_CACHE_SIZE     (8 * 1024) // 8KB
#define CHUNK_SIZE         3072       // 3KB per output
#define LOG_LINE_SIZE      2048       // limit 2KB per line
#define TIMESTAMP_FMT_SIZE 64

static bool s_logger_enabled = true;
static char s_log_buf[LOG_LINE_SIZE];
static DEFINE_SPINLOCK(s_logger_lock);
DEFINE_KFIFO(s_logger_fifo, char, LOG_CACHE_SIZE);
#endif

/*********** PART1: Function Area ***********/
#ifdef TMS_DEBUGER_LOGGER
void logger_cache(const char *fmt, ...)
{
    va_list args;
    struct timespec64 ts;
    unsigned long flags;
    int time_len, log_len;
    char time_buf[TIMESTAMP_FMT_SIZE];
    unsigned int total_len;

    if (!fmt) {
        TMS_PRINT("Logger cache format is NULL\n");
        return;
    }

    ktime_get_real_ts64(&ts);

    // Generate timestamp
    time_len = snprintf(time_buf, TIMESTAMP_FMT_SIZE, "[%lld.%09ld]", (long long)ts.tv_sec, ts.tv_nsec);
    if (time_len >= TIMESTAMP_FMT_SIZE) {
        TMS_PRINT("Log timestamp length error\n");
        return;
    }

    // Calculate log content length
    va_start(args, fmt);
    log_len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (log_len < 0) {
        TMS_PRINT("Log content length error\n");
        return;
    }

    total_len = min_t(unsigned int, LOG_LINE_SIZE, log_len + time_len);

    spin_lock_irqsave(&s_logger_lock, flags);
    if (!s_logger_enabled) {
        TMS_PRINT("Logger cache is disabled\n");
        goto exit;
    }
    // Discard old data if needed
    while (kfifo_avail(&s_logger_fifo) < total_len) {
        unsigned int discarded;
        unsigned int to_discard = min_t(unsigned int, LOG_LINE_SIZE, kfifo_len(&s_logger_fifo));
        discarded = kfifo_out(&s_logger_fifo, s_log_buf, to_discard);
        if (discarded != to_discard) {
            TMS_PRINT("Failed to discard old data, discarded = %u, to_discard = %u\n", discarded, to_discard);
            goto exit;
        }
    }

    // Copy timestamp and format log
    memcpy(s_log_buf, time_buf, time_len);
    va_start(args, fmt);
    vsnprintf(s_log_buf + time_len, LOG_LINE_SIZE - time_len, fmt, args);
    va_end(args);
    // Write to FIFO
    kfifo_in(&s_logger_fifo, s_log_buf, total_len);
exit:
    spin_unlock_irqrestore(&s_logger_lock, flags);
}
#endif


/***********​ PART2: ProcFS Interface ​***********/
static int logger_show(struct seq_file *m, void *v)
{
#ifdef TMS_DEBUGER_LOGGER
    unsigned long flags;
    char *snapshot = NULL;
    size_t chunk;
    int count;

    spin_lock_irqsave(&s_logger_lock, flags);
    if (!s_logger_enabled) {
        TMS_PRINT("Logger is disabled\n");
        spin_unlock_irqrestore(&s_logger_lock, flags);
        return 0;
    }
    chunk = min_t(size_t, CHUNK_SIZE, kfifo_len(&s_logger_fifo));
    spin_unlock_irqrestore(&s_logger_lock, flags);

    if (0 == chunk) {
        return 0;
    }

    snapshot = kmalloc(chunk, GFP_KERNEL);
    if (IS_ERR_OR_NULL(snapshot)) {
        TMS_PRINT("log buffer alloc failed, ret = %ld\n",
                  IS_ERR(snapshot) ? PTR_ERR(snapshot) : -ENOMEM);
        return IS_ERR(snapshot) ? PTR_ERR(snapshot) : -ENOMEM;
    }

    spin_lock_irqsave(&s_logger_lock, flags);
    count = kfifo_out(&s_logger_fifo, snapshot, chunk);
    spin_unlock_irqrestore(&s_logger_lock, flags);
    if (count > 0) {
        seq_write(m, snapshot, count);
    } else {
        TMS_PRINT("Failed to show log message, count = %d\n", count);
    }
    kfree(snapshot);
    return 0;
#else
    return 0;
#endif
}

static ssize_t logger_set(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos) {
    int ret;
    uint8_t level;

    if (count == 0) {
        return 0;
    }

    ret = kstrtou8_from_user(buf, count, 10, &level);
    if (ret != 0) {
        TMS_ERR("kstrtou8_from_user failed, ret = %d\n", ret);
        return -EINVAL;
    }

    if (level > LOG_LEVEL_DUMP) {
        TMS_ERR("Invalid debug level: %u\n", level);
        return -EINVAL;
    }

    tms_log_level = level;
    return count;
}

static int logger_open(struct inode *inode, struct file *file) {
    return single_open(file, logger_show, NULL);
}

DECLARE_PROC_OPS(proc_logger_ops, logger_open, seq_read, logger_set, seq_lseek, single_release);

/*********** PART3: TMS Logger Init Area ***********/
int logger_proc_create(struct proc_dir_entry *prEntry)
{
    int ret;
    struct proc_dir_entry *prEntry_tmp = NULL;

    prEntry_tmp = proc_create("logger", 0644, prEntry, &proc_logger_ops);
    if (IS_ERR_OR_NULL(prEntry_tmp)) {
        ret = IS_ERR(prEntry_tmp) ? PTR_ERR(prEntry_tmp) : -EINVAL;
        TMS_ERR("Couldn't create logger proc entry, ret = %d\n", ret);
        return ret;
    }

    return SUCCESS;
}

#ifdef TMS_DEBUGER_LOGGER
int logger_init(void)
{
    unsigned long flags;

    spin_lock_irqsave(&s_logger_lock, flags);
    s_logger_enabled = true;
    spin_unlock_irqrestore(&s_logger_lock, flags);
    return SUCCESS;
}

void logger_deinit(void)
{
    unsigned long flags;

    spin_lock_irqsave(&s_logger_lock, flags);
    s_logger_enabled = false;
    spin_unlock_irqrestore(&s_logger_lock, flags);
}
#endif
