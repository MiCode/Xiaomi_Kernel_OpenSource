/**
 *******************************************************************************
 * @file        dfs.c
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Implements a set of DebugFS accessors
 *
 * Software License Agreement:
 *
 * The software supplied herewith by ON Semiconductor (the Company)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN AS IS CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *******************************************************************************
 */

#ifdef FSC_DEBUG

#include "dfs.h"

#include <linux/seq_file.h>

#include "fusb30x_global.h"
#include "sysfs_header.h"

/* Type-C state log formatted output */
static int typec_log_show(struct seq_file *m, void *v)
{
    struct Port *p = (struct Port *)m->private;

    FSC_U16 state;
    FSC_U16 TimeStampSeconds, TimeStampMS10ths;

    if (!p) {
      return -1;
    }

    while (ReadStateLog(&p->TypeCStateLog, &state,
                        &TimeStampMS10ths, &TimeStampSeconds))
    {
        seq_printf(m, "[%4u.%04u] %s\n", TimeStampSeconds, TimeStampMS10ths,
            TYPEC_STATE_TBL[state]);
    }

    return 0;
}

static int typec_log_open(struct inode *inode, struct file *file) {
    return single_open(file, typec_log_show, inode->i_private);
}

static const struct file_operations typec_log_fops = {
    .open     = typec_log_open,
    .read     = seq_read,
    .llseek   = seq_lseek,
    .release  = single_release
};

/* Policy Engine state log formatted output */
static int pe_log_show(struct seq_file *m, void *v)
{
    struct Port *p = (struct Port *)m->private;

    FSC_U16 state;
    FSC_U16 TimeStampSeconds, TimeStampMS10ths;

    if (!p) {
      return -1;
    }

    while (ReadStateLog(&p->PDStateLog, &state,
                        &TimeStampMS10ths, &TimeStampSeconds))
    {
        seq_printf(m, "[%4u.%04u] %s\n", TimeStampSeconds, TimeStampMS10ths,
            PE_STATE_TBL[state]);
    }

    return 0;
}

static int pe_log_open(struct inode *inode, struct file *file) {
    return single_open(file, pe_log_show, inode->i_private);
}

static const struct file_operations pe_log_fops = {
    .open     = pe_log_open,
    .read     = seq_read,
    .llseek   = seq_lseek,
    .release  = single_release
};

/*
 * Initialize DebugFS file objects
 */
FSC_S32 fusb_DFS_Init(void)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
        return -ENOMEM;
    }

    /* Try to create our top level dir */
    chip->debugfs_parent = debugfs_create_dir("fusb302", NULL);

    if (!chip->debugfs_parent)
    {
        pr_err("FUSB  %s - Couldn't create DebugFS dir!\n", __func__);
        return -ENOMEM;
    }

    debugfs_create_file("tc_log", 0444, chip->debugfs_parent,
                        &(chip->port), &typec_log_fops);

    debugfs_create_file("pe_log", 0444, chip->debugfs_parent,
                        &(chip->port), &pe_log_fops);

    return 0;
}

/*
 * Cleanup/remove unneeded DebugFS file objects
 */
FSC_S32 fusb_DFS_Cleanup(void)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
        return -ENOMEM;
    }

    if (chip->debugfs_parent != NULL) {
        debugfs_remove_recursive(chip->debugfs_parent);
    }

    return 0;
}

#endif /* FSC_DEBUG */

