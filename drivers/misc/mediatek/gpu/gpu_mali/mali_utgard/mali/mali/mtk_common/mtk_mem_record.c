/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include "mtk_mem_record.h"
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/hardirq.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define NAME_MEM_USAGE  "mem_usage"
#define NAME_MEM_USAGES "mem_usages"

// For per-process memory usage query
typedef struct _MTK_MEM_PROC_REC
{
	pid_t					    pid;
	int		                    i32Bytes;
    struct _MTK_MEM_PROC_REC    *psNext;
	struct _MTK_MEM_PROC_REC    **ppsThis;
} MTK_MEM_PROC_REC;

static MTK_MEM_PROC_REC *g_MemProcRecords;
static struct proc_dir_entry *g_gpu_pentry;
static struct proc_dir_entry *g_mem_usage_pentry;
static struct mutex g_sDebugMutex;
//-----------------------------------------------------------------------------
static int proc_mem_usage_show(struct seq_file *m, void *v)
{
    MTK_MEM_PROC_REC *psHead;

    mutex_lock(&g_sDebugMutex);
    psHead = (MTK_MEM_PROC_REC*)m->private;
    if (psHead)
    {
        seq_printf(m, "%d\n", psHead->i32Bytes);
    }
    else
    {
        seq_printf(m, "No Record\n");
    }
    mutex_unlock(&g_sDebugMutex);
    return 0;
}
//-----------------------------------------------------------------------------
static int proc_mem_usage_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    return single_open(file, proc_mem_usage_show, PDE_DATA(inode));
#else
	struct proc_dir_entry* pentry = PDE(inode);
    return single_open(file, proc_mem_usage_show, pentry ? pentry->data : NULL);
#endif
}
//-----------------------------------------------------------------------------
static const struct file_operations proc_mem_usage_operations = {
    .open    = proc_mem_usage_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};
//-----------------------------------------------------------------------------
static int proc_mem_usages_show(struct seq_file *m, void *v)
{
    MTK_MEM_PROC_REC *psHead, *psNextNode;

    mutex_lock(&g_sDebugMutex);
    seq_printf(m, "     PID              3D Mem\n");
    psHead = g_MemProcRecords;
    while(psHead)
    {
        psNextNode = psHead->psNext;
        seq_printf(m, "%8u    %16d\n", psHead->pid, psHead->i32Bytes);
        psHead = psNextNode;
    }
    mutex_unlock(&g_sDebugMutex);

    return 0;
}
//-----------------------------------------------------------------------------
static int proc_mem_usages_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_mem_usages_show, NULL);
}
//-----------------------------------------------------------------------------
static const struct file_operations proc_mem_usages_operations = {
    .open    = proc_mem_usages_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};
//-----------------------------------------------------------------------------
static MTK_MEM_PROC_REC* List_MTK_MEM_PROC_REC_Search(MTK_MEM_PROC_REC *psHead, pid_t pid)
{
    MTK_MEM_PROC_REC *psNextNode;
    while(psHead)
    {
        psNextNode = psHead->psNext;
        if (psHead->pid == pid)
        {
            return psHead;
        }
        psHead = psNextNode;
    }
    return NULL;
}
//-----------------------------------------------------------------------------
static void List_MTK_MEM_PROC_REC_Insert(MTK_MEM_PROC_REC **ppsHead, MTK_MEM_PROC_REC *psNewNode)
{
    psNewNode->ppsThis = ppsHead;
    psNewNode->psNext = *ppsHead;
    *ppsHead = psNewNode;
    if(psNewNode->psNext)
    {
        psNewNode->psNext->ppsThis = &(psNewNode->psNext);
    }
}
//-----------------------------------------------------------------------------
static void List_MTK_MEM_PROC_REC_Remove(MTK_MEM_PROC_REC *psNode)
{
	(*psNode->ppsThis) = psNode->psNext;
	if(psNode->psNext)
	{
		psNode->psNext->ppsThis = psNode->ppsThis;
	}
}
//-----------------------------------------------------------------------------
void MTKMemRecordAdd(int i32Bytes)
{
    char szName[32];
    MTK_MEM_PROC_REC *psRecord;
    pid_t pid;

    mutex_lock(&g_sDebugMutex);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    pid = (unsigned long)current->pgrp;
#else
    #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
        pid = (unsigned long)task_tgid_nr(current);
    #else
        pid = (unsigned long)current->tgid;
    #endif
#endif
	psRecord = List_MTK_MEM_PROC_REC_Search(g_MemProcRecords, pid);
    if (!psRecord)
    {
        psRecord = kmalloc(sizeof(MTK_MEM_PROC_REC), GFP_KERNEL);
        if (!psRecord)
        {
            /* Out of memory */
            mutex_unlock(&g_sDebugMutex);
            return;
        }
        psRecord->pid      = pid;
        psRecord->i32Bytes = 0;
        List_MTK_MEM_PROC_REC_Insert(&g_MemProcRecords, psRecord);

        // create /proc/gpu/mem_usage/<PID>
        sprintf(szName, "%u", pid);
        proc_create_data(szName, 0, g_mem_usage_pentry, &proc_mem_usage_operations, psRecord);
    }

    psRecord->i32Bytes += i32Bytes;
    mutex_unlock(&g_sDebugMutex);
}
//-----------------------------------------------------------------------------
void MTKMemRecordRemove(int i32Bytes)
{
    char szName[32];
    MTK_MEM_PROC_REC *psRecord;
    pid_t pid;

    mutex_lock(&g_sDebugMutex);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    pid = (unsigned long)current->pgrp;
#else
    #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
        pid = (unsigned long)task_tgid_nr(current);
    #else
        pid = (unsigned long)current->tgid;
    #endif
#endif
	 psRecord = List_MTK_MEM_PROC_REC_Search(g_MemProcRecords, pid);
    if (!psRecord)
    {
        /* Not found */
        mutex_unlock(&g_sDebugMutex);
        return;
    }
    psRecord->i32Bytes -= i32Bytes;
    if (psRecord->i32Bytes < 1)
    {
        // remove /proc/gpu/mem_usage/<PID>
        sprintf(szName, "%u", pid);
        remove_proc_entry(szName, g_mem_usage_pentry);

		List_MTK_MEM_PROC_REC_Remove(psRecord);
        kfree(psRecord);
    }
    mutex_unlock(&g_sDebugMutex);
}
//-----------------------------------------------------------------------------
int MTKMemRecordInit(void)
{
    static struct proc_dir_entry *pentry;
	mutex_init(&g_sDebugMutex);

    g_MemProcRecords = NULL;

    g_gpu_pentry = proc_mkdir("gpu", NULL);
    if (!g_gpu_pentry)
    {
        pr_warn("unable to create /proc/gpu entry\n");
        return -ENOMEM;
    }

    g_mem_usage_pentry = proc_mkdir(NAME_MEM_USAGE, g_gpu_pentry);
    if (!g_gpu_pentry)
    {
        pr_warn("unable to create /proc/gpu/%s entry\n", NAME_MEM_USAGE);
        return -ENOMEM;
    }
 
    pentry = proc_create(NAME_MEM_USAGES, 0, g_gpu_pentry, &proc_mem_usages_operations);
    if (pentry == NULL)
    {
        pr_warn("unable to create /proc/gpu/%s entry\n", NAME_MEM_USAGES);
        return -ENOMEM;
    }

    return 0;
}
//-----------------------------------------------------------------------------
void MTKMemRecordDeInit(void)
{
    char szName[32];
    MTK_MEM_PROC_REC *psNextNode, *psHead;
    if (!g_gpu_pentry)
        return;

    psHead = g_MemProcRecords;
    while(psHead)
    {
        psNextNode = psHead->psNext;

        // remove /proc/gpu/mem_usage/<PID>
        sprintf(szName, "%u", psHead->pid);
        remove_proc_entry(szName, g_gpu_pentry);

		List_MTK_MEM_PROC_REC_Remove(psHead);
        kfree(psHead);
        psHead = psNextNode;
    }

    remove_proc_entry(NAME_MEM_USAGE, g_gpu_pentry);
    remove_proc_entry(NAME_MEM_USAGES, g_gpu_pentry);
    g_gpu_pentry = g_mem_usage_pentry = NULL;
}
//-----------------------------------------------------------------------------
