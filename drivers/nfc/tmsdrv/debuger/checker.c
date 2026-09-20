// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include "../tms_common.h"
#include "checker.h"


#ifdef TMS_MOUDLE
#undef TMS_MOUDLE
#define TMS_MOUDLE "checker"
#endif

/*********** PART0: Global Variables Area ***********/
#ifdef TMS_DEBUGER_CHECKER
#define CHECKER_QUEUE_SIZE (32) // value must be a power of 2
#define CHECKER_QUEUE_MASK (CHECKER_QUEUE_SIZE - 1)
#if (CHECKER_QUEUE_SIZE & (CHECKER_QUEUE_SIZE - 1)) != 0 || CHECKER_QUEUE_SIZE == 0
#error "CHECKER_QUEUE_SIZE must be a power of 2"
#endif

typedef struct {
    int line;
    const char *file;
    const char *func;
    pid_t pid;
} checker_info_t;

static checker_info_t s_cq[CHECKER_QUEUE_SIZE] = {0};
static uint32_t s_cq_idx = 0;
static bool s_checker_enable = true;
static DEFINE_SPINLOCK(s_checker_lock);
#endif
/*********** PART1: Function Area ***********/
#ifdef TMS_DEBUGER_CHECKER
void checker_trace(const char *file, const char *func, int line, pid_t pid)
{
    unsigned long flags;
    uint32_t idx = 0;

    if (!file || !func) {
        return;
    }

    spin_lock_irqsave(&s_checker_lock, flags);
    if (!s_checker_enable) {
        goto unlock;
    }

    idx = s_cq_idx & CHECKER_QUEUE_MASK;
    s_cq[idx].file = file;
    s_cq[idx].func = func;
    s_cq[idx].line = line;
    s_cq[idx].pid = pid;
    s_cq_idx++;

unlock:
    spin_unlock_irqrestore(&s_checker_lock, flags);
}
#endif


static int proc_checker_show(struct seq_file *m, void *v)
{
#ifdef TMS_DEBUGER_CHECKER
    unsigned long flags;
    uint32_t cap = 0;
    uint32_t idx = 0;
    uint32_t start = 0;
    size_t i = 0;
#endif
    seq_printf(m, "Vendor : %s\n",
               IS_ERR_OR_NULL(tms_common_data_binding()->vendor) ?
               "NULL" : tms_common_data_binding()->vendor);
    seq_printf(m, "Kernel : %d.%d.%d\n",
                  (LINUX_VERSION_CODE >> 16) & 0xFFFF,
                  (LINUX_VERSION_CODE >> 8) & 0xFF,
                  LINUX_VERSION_CODE & 0xFF);
    seq_printf(m, "Driver : %06X\n", DRIVER_VERSION);
    seq_printf(m, "Log : L%u\n", tms_log_level);

#ifdef TMS_DEBUGER_CHECKER
    spin_lock_irqsave(&s_checker_lock, flags);
    if (!s_checker_enable) {
        goto exit;
    }

    if (0 == s_cq_idx) {
        seq_printf(m, "Checker PASSED\n");
        goto exit;
    }

    seq_printf(m, "Checker FAILED : \n");
    seq_printf(m, "\tpid\tline\tfunc(file)\n");

    cap = min_t(uint32_t, CHECKER_QUEUE_SIZE, s_cq_idx);
    start = (s_cq_idx - cap) & CHECKER_QUEUE_MASK;

    for (i = 0; i < cap; i++) {
        idx = (start + i) & CHECKER_QUEUE_MASK;
        seq_printf(m, "\t%d\t%d\t%s(%s)\n",
                   s_cq[idx].pid, s_cq[idx].line, s_cq[idx].func, s_cq[idx].file);
    }

    s_cq_idx = 0;
exit:
    spin_unlock_irqrestore(&s_checker_lock, flags);
#endif
    return 0;
}

static int proc_checker_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_checker_show, NULL);
}

DECLARE_PROC_OPS(proc_checker_ops, proc_checker_open, seq_read, NULL, seq_lseek, single_release);
/*********** PART3: TMS Checker Init Area ***********/
int checker_proc_create(struct proc_dir_entry *prEntry)
{
    int ret;
    struct proc_dir_entry *prEntry_tmp = NULL;

    prEntry_tmp = proc_create("checker", 0644, prEntry, &proc_checker_ops);
    if (IS_ERR_OR_NULL(prEntry_tmp)) {
        ret = IS_ERR(prEntry_tmp) ? PTR_ERR(prEntry_tmp) : -EINVAL;
        TMS_ERR("Couldn't create checker proc entry, ret = %d\n", ret);
        return ret;
    }

    return SUCCESS;
}

#ifdef TMS_DEBUGER_CHECKER
int checker_init(void)
{
    unsigned long flags;

    spin_lock_irqsave(&s_checker_lock, flags);
    s_checker_enable = true;
    spin_unlock_irqrestore(&s_checker_lock, flags);
    return 0;
}

void checker_deinit(void)
{
    unsigned long flags;

    spin_lock_irqsave(&s_checker_lock, flags);
    s_checker_enable = false;
    spin_unlock_irqrestore(&s_checker_lock, flags);
}
#endif
