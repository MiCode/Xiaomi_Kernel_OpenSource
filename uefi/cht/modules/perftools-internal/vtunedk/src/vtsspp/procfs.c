/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#include "vtss_config.h"
#include "procfs.h"
#include "globals.h"
#include "collector.h"
#include "cpuevents.h"
#include "nmiwd.h"

#include <linux/list.h>         /* for struct list_head */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>           /* for struct file_operations */
#include <linux/namei.h>        /* for struct nameidata       */
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define VTSS_PROCFS_CTRL_NAME      ".control"
#define VTSS_PROCFS_DEBUG_NAME     ".debug"
#define VTSS_PROCFS_CPUMASK_NAME   ".cpu_mask"
#define VTSS_PROCFS_DEFSAV_NAME    ".def_sav"
#define VTSS_PROCFS_TARGETS_NAME   ".targets"
#define VTSS_PROCFS_TIMESRC_NAME   ".time_source"
#define VTSS_PROCFS_TIMELIMIT_NAME ".time_limit"


#ifndef VTSS_AUTOCONF_CPUMASK_PARSELIST_USER
#include "cpumask_parselist_user.c"
#endif

extern int uid;
extern int gid;
extern int mode;
extern unsigned int vtss_client_major_ver;
extern unsigned int vtss_client_minor_ver;
        
struct vtss_procfs_ctrl_data
{
    struct list_head list;
    size_t           size;
    char             buf[0];
};

static cpumask_t vtss_procfs_cpumask_ = CPU_MASK_NONE;
static int       vtss_procfs_defsav_  = 0;

static struct proc_dir_entry *vtss_procfs_root = NULL;

struct proc_dir_entry *vtss_procfs_get_root(void)
{
    return vtss_procfs_root;
}

const char *vtss_procfs_path(void)
{
    static char buf[MODULE_NAME_LEN + 7 /* strlen("/proc/") */];
    snprintf(buf, sizeof(buf)-1, "/proc/%s", THIS_MODULE->name);
    return buf;
}

static ssize_t vtss_procfs_ctrl_write(struct file *file, const char __user * buf, size_t count, loff_t * ppos)
{
    char chr;
    size_t buf_size = count;
    unsigned long flags = 0;
/*    char in[200];
    if (count >=200){
           return -1;
    }
    if (vtss_copy_from_user(&in[0], buf, (int)count)){
        return -1;
    }
    in[count]= '\0';

    printk("income: %s, count = %d", in, (int)count);
*/
    while (buf_size > 0) {
        if (get_user(chr, buf))
            return -EFAULT;

        buf += sizeof(char);
        buf_size -= sizeof(char);
        TRACE("chr=%c", chr);
        switch (chr) {
        case 'V': { /* VXXXXX.XXXXX client version */
                int major = 1;
                vtss_client_major_ver = 0;
                vtss_client_minor_ver = 0;
//                return -EINVAL;
                while (buf_size > 0) {
                    if (get_user(chr, buf))
                        return -EFAULT;
                    if (chr >= '0' && chr <= '9') {
                        buf += sizeof(char);
                        buf_size -= sizeof(char);
                        if (major) vtss_client_major_ver = vtss_client_major_ver * 10 + (chr - '0');
                        else vtss_client_minor_ver = vtss_client_minor_ver * 10 + (chr - '0');
                    } else{
                        if (major && chr == '.'){
                            major = 0;
                            buf += sizeof(char);
                            buf_size -= sizeof(char);
                        }
                        else {
                            break;
                        }
                    }
                }
//                vtss_client_minor_ver = 0;
                break;
        }
        case 'T': { /* T<pid> - Set target PID */
                unsigned long pid = 0;

                while (buf_size > 0) {
                    if (get_user(chr, buf))
                        return -EFAULT;
                    if (chr >= '0' && chr <= '9') {
                        buf += sizeof(char);
                        buf_size -= sizeof(char);
                        pid = pid * 10 + (chr - '0');
                    } else
                        break;
                }
                TRACE("TARGET: pid=%lu", pid);
                if (pid != 0) {
                    if (vtss_cmd_set_target((pid_t)pid)) {
                        ERROR("Unable to find a target pid=%lu", pid);                                                    				
                        vtss_procfs_ctrl_wake_up(NULL, 0);
                    }
                }
            }
            break;
        case 'I': { /* I<flags> - Initialize */
                while (buf_size > 0) {
                    if (get_user(chr, buf))
                        return -EFAULT;
                    if (chr >= '0' && chr <= '9') {
                        buf += sizeof(char);
                        buf_size -= sizeof(char);
                        flags = flags * 10 + (chr - '0');
                    } else
                        break;
                }
                if (flags) {
                    /* TODO: For compatibility with old implementation !!! */
                    reqcfg.trace_cfg.trace_flags = flags;
                }
                TRACE("INIT: flags=0x%0lX (%lu)", flags, flags);
                if (vtss_cmd_start() !=0 )
                {
                    ERROR("ERROR: Unable to start collection. Initialization failed.");
                    return VTSS_ERR_INIT_FAILED;
                }
            }
            break;
        case 'E': { /* E<size>=... - configuration request */
                unsigned long size = 0;
                while (buf_size > 0) {
                    if (get_user(chr, buf))
                        return -EFAULT;
                    buf += sizeof(char);
                    buf_size -= sizeof(char);
                    if (chr >= '0' && chr <= '9') {
                        size = size * 10 + (chr - '0');
                    } else
                        break;
                }
                vtss_collection_cfg_init();
                TRACE("chr2=%c, size = %d", chr, (int)size);
                if (chr == '=' && size <= buf_size) {
                    int i, j, mux_cnt;
                    int namespace_size = 0;
                    TRACE("BEGIN: size=%lu, buf_size=%zu", size, buf_size);
                    while (size != 0) {
                        int cfgreq, fake_shift = 0;
                        trace_cfg_t trace_currreq;
                        stk_cfg_t stk_req;
                        cpuevent_cfg_v1_t* cpuevent_currreq = NULL;

                        if (flags) {
                            /* TODO: For compatibility with old implementation !!! */
                            fake_shift = sizeof(int);
                            buf -= fake_shift;
                            buf_size += fake_shift;
                            cfgreq = VTSS_CFGREQ_CPUEVENT_V1;
                        } else {
                            if (get_user(cfgreq, (const int __user *)buf)) {
                                ERROR("Error in get_user()");
                                return -EFAULT;
                            }
                        }
                        TRACE("cfgreq = %lx", (unsigned long)cfgreq);
                        switch (cfgreq) {
                        case VTSS_CFGREQ_VOID:
                            TRACE("VTSS_CFGREQ_VOID");
                            size = 0;
                            break;
                        case VTSS_CFGREQ_CPUEVENT_V1:
                            INFO("in reading VTSS_CFGREQ_CPUEVENT_V1, reqcfg.cpuevent_count_v1=%d", (int)reqcfg.cpuevent_count_v1);
                            if (reqcfg.cpuevent_count_v1 < VTSS_CFG_CHAIN_SIZE){
                                if (vtss_copy_from_user(&reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1], buf, sizeof(cpuevent_cfg_v1_t))) {
                                    ERROR("Error in copy_from_user()");
                                    return -EFAULT;
                                }
                                cpuevent_currreq = &reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1];
                                if (namespace_size + cpuevent_currreq->name_len + cpuevent_currreq->desc_len < VTSS_CFG_SPACE_SIZE * 16) {
                                    /// copy CPU event name
                                    if (vtss_copy_from_user(&reqcfg.cpuevent_namespace_v1[namespace_size], &buf[cpuevent_currreq->name_off+fake_shift], cpuevent_currreq->name_len)) {
                                        ERROR("Error in copy_from_user()");
                                        return -EFAULT;
                                    }
                                    TRACE("Load event[%02d]: '%s'", reqcfg.cpuevent_count_v1, &reqcfg.cpuevent_namespace_v1[namespace_size]);
                                    /// adjust CPU event record
                                    cpuevent_currreq->name_off = (int)((size_t)&reqcfg.cpuevent_namespace_v1[namespace_size] - (size_t)&reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1]);
                                    /// adjust namespace size
                                    namespace_size += cpuevent_currreq->name_len;
                                    /// copy event description
                                    if (vtss_copy_from_user(&reqcfg.cpuevent_namespace_v1[namespace_size], &buf[cpuevent_currreq->desc_off+fake_shift], cpuevent_currreq->desc_len)) {
                                        ERROR("Error in copy_from_user()");
                                        return -EFAULT;
                                    }
                                    /// adjust CPU event record
                                    cpuevent_currreq->desc_off = (int)((size_t)&reqcfg.cpuevent_namespace_v1[namespace_size] - (size_t)&reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1]);
                                    /// adjust namespace size
                                    namespace_size += cpuevent_currreq->desc_len;
                                    /// copy CPU event record
                                    /* TODO: For compatibility with old implementation !!! */
                                    reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].reqtype = VTSS_CFGREQ_CPUEVENT_V1;
                                    /// adjust record size (as it may differ from initial request size)
                                    reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].reqsize = sizeof(cpuevent_cfg_v1_t) + cpuevent_currreq->name_len + cpuevent_currreq->desc_len;
                                    reqcfg.cpuevent_count_v1++;
                                }
                            }
                            buf += cpuevent_currreq->reqsize+fake_shift;
                            buf_size -= cpuevent_currreq->reqsize+fake_shift;
                            size -= cpuevent_currreq->reqsize;
                            break;
                        case VTSS_CFGREQ_OSEVENT:
                            if (reqcfg.osevent_count < VTSS_CFG_CHAIN_SIZE) {
                                /// copy OS event record
                                if (vtss_copy_from_user(&reqcfg.osevent_cfg[reqcfg.osevent_count], buf, sizeof(osevent_cfg_t))) {
                                    ERROR("Error in copy_from_user()");
                                    return -EFAULT;
                                }
                                TRACE("VTSS_CFGREQ_OSEVENT[%d]: event_id=%d", reqcfg.osevent_count, reqcfg.osevent_cfg[reqcfg.osevent_count].event_id);
                                reqcfg.osevent_count++;
                            }
                            buf += sizeof(osevent_cfg_t);
                            buf_size -= sizeof(osevent_cfg_t);
                            size -= sizeof(osevent_cfg_t);
                            break;
                        case VTSS_CFGREQ_BTS:
                            if (vtss_copy_from_user(&reqcfg.bts_cfg, buf, sizeof(bts_cfg_t))) {
                                ERROR("Error in copy_from_user()");
                                return -EFAULT;
                            }
                            TRACE("VTSS_CFGREQ_BTS: brcount=%d, modifier=0x%0X", reqcfg.bts_cfg.brcount, reqcfg.bts_cfg.modifier);
                            buf += sizeof(bts_cfg_t);
                            buf_size -= sizeof(bts_cfg_t);
                            size -= sizeof(bts_cfg_t);
                            break;
                        case VTSS_CFGREQ_LBR:
                            if (vtss_copy_from_user(&reqcfg.lbr_cfg, buf, sizeof(lbr_cfg_t))) {
                                ERROR("Error in copy_from_user()");
                                return -EFAULT;
                            }
                            TRACE("VTSS_CFGREQ_LBR: brcount=%d, modifier=0x%0X", reqcfg.lbr_cfg.brcount, reqcfg.lbr_cfg.modifier);
                            buf += sizeof(lbr_cfg_t);
                            buf_size -= sizeof(lbr_cfg_t);
                            size -= sizeof(lbr_cfg_t);
                            break;
                        case VTSS_CFGREQ_TRACE:
                            if (vtss_copy_from_user(&trace_currreq, buf, sizeof(trace_cfg_t))) {
                                ERROR("Error in copy_from_user()");
                                return -EFAULT;
                            }
                            if (trace_currreq.namelen < VTSS_CFG_SPACE_SIZE) {
                                if (vtss_copy_from_user(&reqcfg.trace_cfg, buf, sizeof(trace_cfg_t)+trace_currreq.namelen)) {
                                    ERROR("Error in copy_from_user()");
                                    return -EFAULT;
                                }
                            }
                            TRACE("VTSS_CFGREQ_TRACE: trace_flags=0x%0X, namelen=%d", reqcfg.trace_cfg.trace_flags, trace_currreq->namelen);
                            buf += sizeof(trace_cfg_t)+(trace_currreq.namelen-1);
                            buf_size -= sizeof(trace_cfg_t)+(trace_currreq.namelen-1);
                            size -= sizeof(trace_cfg_t)+(trace_currreq.namelen-1);
                            break;
                        case VTSS_CFGREQ_STK:
                            if (vtss_copy_from_user(&stk_req, buf, sizeof(stk_cfg_t))) {
                                ERROR("Error in copy_from_user()");
                                return -EFAULT;
                            }
                            if (stk_req.stktype >= vtss_stk_last){
                                ERROR("The stack settings is not supported in current driver version. Please update the driver.");
                                break;
                            }
                            reqcfg.stk_pg_sz[stk_req.stktype] = (unsigned long)stk_req.stk_pg_sz;
                            if (reqcfg.stk_pg_sz[stk_req.stktype] == 0) reqcfg.stk_pg_sz[stk_req.stktype] = PAGE_SIZE;

                            reqcfg.stk_sz[stk_req.stktype]=(unsigned long)(reqcfg.stk_sz[stk_req.stktype]);
                            if (reqcfg.stk_sz[stk_req.stktype] == 0) reqcfg.stk_sz[stk_req.stktype]=(unsigned long)-1;
                            while (reqcfg.stk_pg_sz[stk_req.stktype] > reqcfg.stk_sz[stk_req.stktype]){
                                 reqcfg.stk_pg_sz[stk_req.stktype] = (reqcfg.stk_pg_sz[stk_req.stktype] >> 1);
                            }
                            TRACE("VTSS_CFGREQ_STK: stk_sz=0x%lx", reqcfg.stk_pg_sz[stk_req.stktype]);
                            buf += sizeof(stk_cfg_t);
                            buf_size -= sizeof(stk_cfg_t);
                            size -= sizeof(stk_cfg_t);
                            break;
                        default:
                            ERROR("Incorrect config request 0x%X", cfgreq);
                            return -EFAULT;
                        }
                        TRACE("LOOP: size=%lu, buf_size=%zu", size, buf_size);
                    } /* while (size != 0) */
                    if ((reqcfg.cpuevent_count_v1 == 0 && !(reqcfg.trace_cfg.trace_flags & (VTSS_CFGTRACE_CTX|VTSS_CFGTRACE_PWRACT|VTSS_CFGTRACE_PWRIDLE)))||
                        (reqcfg.cpuevent_count_v1 == 0 && hardcfg.family == 0x0b))
                            vtss_cpuevents_reqcfg_default(0, vtss_procfs_defsav());
                    vtss_sysevents_reqcfg_append();
                    if(hardcfg.family != 0x06 || (hardcfg.model != 0x3d /* BDW */ && hardcfg.model != 0x4e /* SKL */)){
                        reqcfg.trace_cfg.trace_flags &= ~VTSS_CFGTRACE_IPT;
                    } else {
                        /// silently replace BTS with IPT on BDW
                        if(reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_BRANCH)
                        {
                            if(hardcfg.family == 0x06 && (hardcfg.model == 0x3d /* BDW */ || hardcfg.model == 0x4e /* SKL */))
                            {
                                reqcfg.trace_cfg.trace_flags |= VTSS_CFGTRACE_IPT;  /// TODO: uncomment when the user-mode part is ready
                            }
                        }
                        /// mutually exclude LBRs and IPT
                        if(reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_IPT)
                        {
                            reqcfg.trace_cfg.trace_flags &= ~(VTSS_CFGTRACE_BRANCH | VTSS_CFGTRACE_LASTBR | VTSS_CFGTRACE_LBRCSTK);
                        }
                    }
                } else {
                    ERROR("Invalid command: E%lu=...", size);
                    return -EINVAL;
                }
            }
            break;
        case 'F': /* F - Finish or Stop */
            INFO("STOP");
            vtss_cmd_stop();
            break;
        case 'P': /* P - Pause */
            INFO("PAUSE");
            vtss_cmd_pause();
            break;
        case 'R': /* R - Resume */
            INFO("RESUME");
            vtss_cmd_resume();
            break;
        case 'M': /* M - Mark */
            INFO("MARK");
            vtss_cmd_mark();
            break;
        case 'W': /* W - Watchdog */
            //W0 - disable watchdog
            //W1 - enable watchdog
            {
              int st = 0;
//              INFO("Watchdog");
              if (get_user(chr, buf))
                return -EFAULT;
              buf += sizeof(char);
              buf_size -= sizeof(char);
              if (chr == '0') {
              st = vtss_nmi_watchdog_disable(0);
              } else if (chr == '1') {
                st = vtss_nmi_watchdog_enable(0);
              } else if (chr == 'd') { //internal API
	        st = vtss_nmi_watchdog_disable(1); //for tests.this mode will not increment counter
              } else if (chr == 'e') { //internal API
                st = vtss_nmi_watchdog_enable(1); //for test. this mode will not decrement counter
              } else {
	        st = -1;
                ERROR("Watchdog command is not recognized");
              }
	      if (st < 0) return -EINVAL;
	      if (st > 0) count = count - 1; //ignore the command
	    }
            break;
         case ' ':
        case '\n':
            break;
        default:
            ERROR("Invalid command: '%c'", chr);
            return -EINVAL;
        }
    }
    return count;
}

static DECLARE_WAIT_QUEUE_HEAD(vtss_procfs_ctrl_waitq);
#ifdef CONFIG_PREEMPT_RT
static DEFINE_RAW_SPINLOCK(vtss_procfs_ctrl_list_lock);
#else
static DEFINE_SPINLOCK(vtss_procfs_ctrl_list_lock);
#endif
static LIST_HEAD(vtss_procfs_ctrl_list);

int vtss_procfs_ctrl_wake_up(void *msg, size_t size)
{
    unsigned long flags;
    struct vtss_procfs_ctrl_data *ctld = (struct vtss_procfs_ctrl_data*)kmalloc(sizeof(struct vtss_procfs_ctrl_data)+size, GFP_ATOMIC);

    if (ctld == NULL) {
        ERROR("Unable to allocate memory for message");
        return -ENOMEM;
    }
    if (size) {
        memcpy(ctld->buf, msg, size);
        TRACE("msg=['%s', %d]", (char*)msg, (int)size);
    } else {
        TRACE("[EOF]");
    }
    ctld->size = size;
    spin_lock_irqsave(&vtss_procfs_ctrl_list_lock, flags);
    list_add_tail(&ctld->list, &vtss_procfs_ctrl_list);
    spin_unlock_irqrestore(&vtss_procfs_ctrl_list_lock, flags);
    if (waitqueue_active(&vtss_procfs_ctrl_waitq))
        wake_up_interruptible(&vtss_procfs_ctrl_waitq);
    return 0;
}

int vtss_procfs_ctrl_wake_up_2(void *msg1, size_t size1, void *msg2, size_t size2)
{
    unsigned long flags;
    struct vtss_procfs_ctrl_data *ctld1 = NULL;
    struct vtss_procfs_ctrl_data *ctld2 = NULL;
    ctld1 = (struct vtss_procfs_ctrl_data*)kmalloc(sizeof(struct vtss_procfs_ctrl_data)+size1, GFP_ATOMIC);
    if (ctld1 == NULL ) {
        ERROR("Unable to allocate memory for message");
        return -ENOMEM;
    }
    ctld2 = (struct vtss_procfs_ctrl_data*)kmalloc(sizeof(struct vtss_procfs_ctrl_data)+size2, GFP_ATOMIC);
    if (ctld2 == NULL ) {
        ERROR("Unable to allocate memory for message");
        return -ENOMEM;
    }
    if (size1) {
        memcpy(ctld1->buf, msg1, size1);
        TRACE("msg=['%s', %d]", (char*)msg1, (int)size1);
    } else {
        TRACE("[EOF]");
    }
    if (size2) {
        memcpy(ctld2->buf, msg2, size2);
        TRACE("msg=['%s', %d]", (char*)msg2, (int)size2);
    } else {
        TRACE("[EOF]");
    }
    ctld1->size = size1;
    ctld2->size = size2;
    spin_lock_irqsave(&vtss_procfs_ctrl_list_lock, flags);
    list_add_tail(&ctld2->list, &vtss_procfs_ctrl_list);
    list_add_tail(&ctld1->list, &vtss_procfs_ctrl_list);
    spin_unlock_irqrestore(&vtss_procfs_ctrl_list_lock, flags);
    if (waitqueue_active(&vtss_procfs_ctrl_waitq))
    {
        wake_up_interruptible(&vtss_procfs_ctrl_waitq);
        wake_up_interruptible(&vtss_procfs_ctrl_waitq);
    }
    return 0;
}
void vtss_procfs_ctrl_flush(void)
{
    unsigned long flags;
    struct list_head *p, *tmp;
    struct vtss_procfs_ctrl_data *ctld;
    spin_lock_irqsave(&vtss_procfs_ctrl_list_lock, flags);
    list_for_each_safe(p, tmp, &vtss_procfs_ctrl_list) {
        ctld = list_entry(p, struct vtss_procfs_ctrl_data, list);
        list_del_init(p);
        kfree(ctld);
    }
    spin_unlock_irqrestore(&vtss_procfs_ctrl_list_lock, flags);
}

static ssize_t vtss_procfs_ctrl_read(struct file* file, char __user* buf, size_t size, loff_t* ppos)
{
    ssize_t rsize = 0;
    unsigned long flags;
    struct vtss_procfs_ctrl_data *ctld;

    TRACE("file=0x%p", file);
    /* wait for nonempty ready queue */
    spin_lock_irqsave(&vtss_procfs_ctrl_list_lock, flags);
    while (list_empty(&vtss_procfs_ctrl_list)) {
        spin_unlock_irqrestore(&vtss_procfs_ctrl_list_lock, flags);
        TRACE("file=0x%p: WAIT", file);
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;
        if (wait_event_interruptible(vtss_procfs_ctrl_waitq, !list_empty(&vtss_procfs_ctrl_list)))
            return -ERESTARTSYS;
        spin_lock_irqsave(&vtss_procfs_ctrl_list_lock, flags);
    }
    /* get the first message from list */
#ifdef list_first_entry
    ctld = list_first_entry(&vtss_procfs_ctrl_list, struct vtss_procfs_ctrl_data, list);
#else
    ctld = list_entry(vtss_procfs_ctrl_list.next, struct vtss_procfs_ctrl_data, list);
#endif
    list_del_init(&ctld->list);
    spin_unlock_irqrestore(&vtss_procfs_ctrl_list_lock, flags);
    /* write it out */
    rsize = ctld->size;
    if (rsize == 0) {
        TRACE("file=0x%p: EOF", file);
        kfree(ctld);
        return 0;
    }
    TRACE("file=0x%p, copy_to_user=['%s', %zu of %zu]", file, ctld->buf, ctld->size, size);
    if (rsize > size) {
        ERROR("Not enough buffer size for whole message");
        kfree(ctld);
        return -EINVAL;
    }
    *ppos += rsize;
    if (copy_to_user(buf, ctld->buf, rsize)) {
        kfree(ctld);
        return -EFAULT;
    }
    kfree(ctld);
    return rsize;
}

static unsigned int vtss_procfs_ctrl_poll(struct file *file, poll_table * poll_table)
{
    unsigned int rc = 0;
    unsigned long flags;

    poll_wait(file, &vtss_procfs_ctrl_waitq, poll_table);
    spin_lock_irqsave(&vtss_procfs_ctrl_list_lock, flags);
    if (!list_empty(&vtss_procfs_ctrl_list))
        rc = (POLLIN | POLLRDNORM);
    spin_unlock_irqrestore(&vtss_procfs_ctrl_list_lock, flags);
    TRACE("file=0x%p: %s", file, (rc ? "READY" : "-----"));
    return rc;
}

static atomic_t vtss_procfs_attached = ATOMIC_INIT(0);

static int vtss_procfs_ctrl_open(struct inode *inode, struct file *file)
{
    /* Increase the priority for trace reader to avoid lost events */
    set_user_nice(current, -19);
    atomic_inc(&vtss_procfs_attached);
    vtss_cmd_open();
    return 0;
}

static int vtss_procfs_ctrl_close(struct inode *inode, struct file *file)
{
    if (atomic_dec_and_test(&vtss_procfs_attached)) {
        TRACE("Nobody is attached");
        vtss_procfs_ctrl_flush();
        vtss_cmd_stop_async();
        /* set defaults for next session */
        cpumask_copy(&vtss_procfs_cpumask_, cpu_present_mask);
        vtss_procfs_defsav_ = 0;
    }
    vtss_cmd_close();
    /* Restore default priority for trace reader */
    set_user_nice(current, 0);
    return 0;
}

static const struct file_operations vtss_procfs_ctrl_fops = {
    .owner   = THIS_MODULE,
    .read    = vtss_procfs_ctrl_read,
    .write   = vtss_procfs_ctrl_write,
    .open    = vtss_procfs_ctrl_open,
    .release = vtss_procfs_ctrl_close,
    .poll    = vtss_procfs_ctrl_poll,
};

/* ************************************************************************* */

static void *debug_info = NULL;

static int vtss_procfs_debug_show(struct seq_file *s, void *v)
{
    return vtss_debug_info(s);
}

static void *vtss_procfs_debug_start(struct seq_file *s, loff_t *pos)
{
    return (*pos) ? NULL : &debug_info;
}

static void *vtss_procfs_debug_next(struct seq_file *s, void *v, loff_t *pos)
{
    return NULL;
}

static void vtss_procfs_debug_stop(struct seq_file *s, void *v)
{
}

static const struct seq_operations vtss_procfs_debug_sops = {
    .start = vtss_procfs_debug_start,
    .next  = vtss_procfs_debug_next,
    .stop  = vtss_procfs_debug_stop,
    .show  = vtss_procfs_debug_show,
};

static int vtss_procfs_debug_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &vtss_procfs_debug_sops);
}

static const struct file_operations vtss_procfs_debug_fops = {
    .owner   = THIS_MODULE,
    .open    = vtss_procfs_debug_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

/* ************************************************************************* */

const struct cpumask* vtss_procfs_cpumask(void)
{
    return &vtss_procfs_cpumask_;
}

static ssize_t vtss_procfs_cpumask_read(struct file* file, char __user* buf, size_t size, loff_t* ppos)
{
    ssize_t rc = 0;

    if (*ppos == 0) {
        char *page = (char*)__get_free_page(GFP_TEMPORARY  | __GFP_NORETRY | __GFP_NOWARN);

        /* buf currently PAGE_SIZE, need 9 chars per 32 bits. */
        BUILD_BUG_ON((NR_CPUS/32 * 9) > (PAGE_SIZE-1));

        if (page == NULL)
            return -ENOMEM;
        rc = cpulist_scnprintf(page, PAGE_SIZE-2, &vtss_procfs_cpumask_);
        page[rc++] = '\n';
        page[rc]   = '\0';
        *ppos += rc;
        if (rc <= size) {
            if (copy_to_user(buf, page, rc)) {
                rc = -EFAULT;
            }
        } else {
            rc = -EINVAL;
        }
        free_page((unsigned long)page);
    }
    return rc;
}

static ssize_t vtss_procfs_cpumask_write(struct file *file, const char __user * buf, size_t count, loff_t * ppos)
{
    ssize_t rc = -EINVAL;
    cpumask_var_t new_value;

    if (!alloc_cpumask_var(&new_value, (GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN)))
        return -ENOMEM;
    if (!cpumask_parselist_user(buf, count, new_value)) {
        cpumask_and(&vtss_procfs_cpumask_, new_value, cpu_present_mask);
        rc = count;
    }
    free_cpumask_var(new_value);
    return rc;
}

static int vtss_procfs_cpumask_open(struct inode *inode, struct file *file)
{
    /* TODO: ... */
    return 0;
}

static int vtss_procfs_cpumask_close(struct inode *inode, struct file *file)
{
    /* TODO: ... */
    return 0;
}

static const struct file_operations vtss_procfs_cpumask_fops = {
    .owner   = THIS_MODULE,
    .read    = vtss_procfs_cpumask_read,
    .write   = vtss_procfs_cpumask_write,
    .open    = vtss_procfs_cpumask_open,
    .release = vtss_procfs_cpumask_close,
};

/* ************************************************************************* */

int vtss_procfs_defsav(void)
{
    return vtss_procfs_defsav_;
}

static ssize_t vtss_procfs_defsav_read(struct file* file, char __user* buf, size_t size, loff_t* ppos)
{
    ssize_t rc = 0;

    if (*ppos == 0) {
        char buff[32]; /* enough for <int> */
        rc = snprintf(buff, sizeof(buff)-2, "%d", vtss_procfs_defsav_);
        rc = (rc < 0) ? 0 : rc;
        buff[rc++] = '\n';
        buff[rc]   = '\0';
        *ppos += rc;
        if (rc <= size) {
            if (copy_to_user(buf, buff, rc)) {
                rc = -EFAULT;
            }
        } else {
            rc = -EINVAL;
        }
    }
    return rc;
}

static ssize_t vtss_procfs_defsav_write(struct file *file, const char __user * buf, size_t count, loff_t * ppos)
{
    char chr;
    size_t i;
    unsigned long val = 0;

    for (i = 0; i < count; i++, buf += sizeof(char)) {
        if (get_user(chr, buf))
            return -EFAULT;
        if (chr >= '0' && chr <= '9') {
            val = val * 10 + (chr - '0');
        } else
            break;
    }
    vtss_procfs_defsav_ = (int)(val < 1000000)    ? 1000000    : val;
    vtss_procfs_defsav_ = (int)(val > 2000000000) ? 2000000000 : val;
    return count;
}

static int vtss_procfs_defsav_open(struct inode *inode, struct file *file)
{
    /* TODO: ... */
    return 0;
}

static int vtss_procfs_defsav_close(struct inode *inode, struct file *file)
{
    /* TODO: ... */
    return 0;
}

static const struct file_operations vtss_procfs_defsav_fops = {
    .owner   = THIS_MODULE,
    .read    = vtss_procfs_defsav_read,
    .write   = vtss_procfs_defsav_write,
    .open    = vtss_procfs_defsav_open,
    .release = vtss_procfs_defsav_close,
};

/* ************************************************************************* */

static void *targets_info = NULL;

static int vtss_procfs_targets_show(struct seq_file *s, void *v)
{
    return vtss_target_pids(s);
}

static void *vtss_procfs_targets_start(struct seq_file *s, loff_t *pos)
{
    return (*pos) ? NULL : &targets_info;
}

static void *vtss_procfs_targets_next(struct seq_file *s, void *v, loff_t *pos)
{
    return NULL;
}

static void vtss_procfs_targets_stop(struct seq_file *s, void *v)
{
}

static const struct seq_operations vtss_procfs_targets_sops = {
    .start = vtss_procfs_targets_start,
    .next  = vtss_procfs_targets_next,
    .stop  = vtss_procfs_targets_stop,
    .show  = vtss_procfs_targets_show,
};

static int vtss_procfs_targets_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &vtss_procfs_targets_sops);
}

static const struct file_operations vtss_procfs_targets_fops = {
    .owner   = THIS_MODULE,
    .open    = vtss_procfs_targets_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

/* ************************************************************************* */

static ssize_t vtss_procfs_timesrc_read(struct file* file, char __user* buf, size_t size, loff_t* ppos)
{
    ssize_t rc = 0;

    if (*ppos == 0) {
        char buff[8]; /* enough for "tsc" or "sys" string */
        rc = snprintf(buff, sizeof(buff)-2, "%s", vtss_time_source ? "tsc" : "sys");
        rc = (rc < 0) ? 0 : rc;
        buff[rc++] = '\n';
        buff[rc]   = '\0';
        *ppos += rc;
        if (rc <= size) {
            if (copy_to_user(buf, buff, rc)) {
                rc = -EFAULT;
            }
        } else {
            rc = -EINVAL;
        }
    }
    return rc;
}

static ssize_t vtss_procfs_timesrc_write(struct file *file, const char __user * buf, size_t count, loff_t * ppos)
{
    char val[8];

    if (count < 3 || vtss_copy_from_user(val, buf, 3)) {
        ERROR("Error in copy_from_user()");
        return -EFAULT;
    }
    val[3] = '\0';
    if (!strncmp(val, "tsc", 3)) {
        if (check_tsc_unstable()){
            ERROR("TSC timesource is unstable. Switching to system time...");
            //TODO: It's better to return error for the case.This change require testing on systems with TSC reliable and not reliable.
        } else {
            vtss_time_source = 1;
        }
    }
    if (!strncmp(val, "sys", 3))
        vtss_time_source = 0;
    TRACE("time source=%s", vtss_time_source ? "tsc" : "sys");
    return count;
}

static int vtss_procfs_timesrc_open(struct inode *inode, struct file *file)
{
    /* TODO: ... */
    return 0;
}

static int vtss_procfs_timesrc_close(struct inode *inode, struct file *file)
{
    /* TODO: ... */
    return 0;
}

static const struct file_operations vtss_procfs_timesrc_fops = {
    .owner   = THIS_MODULE,
    .read    = vtss_procfs_timesrc_read,
    .write   = vtss_procfs_timesrc_write,
    .open    = vtss_procfs_timesrc_open,
    .release = vtss_procfs_timesrc_close,
};

/* ************************************************************************* */

static ssize_t vtss_procfs_timelimit_read(struct file* file, char __user* buf, size_t size, loff_t* ppos)
{
    ssize_t rc = 0;

    if (*ppos == 0) {
        char buff[32]; /* enough for <unsigned long long> */
        rc = snprintf(buff, sizeof(buff)-2, "%llu", vtss_time_limit);
        rc = (rc < 0) ? 0 : rc;
        buff[rc++] = '\n';
        buff[rc]   = '\0';
        *ppos += rc;
        if (rc <= size) {
            if (copy_to_user(buf, buff, rc)) {
                rc = -EFAULT;
            }
        } else {
            rc = -EINVAL;
        }
    }
    return rc;
}

static ssize_t vtss_procfs_timelimit_write(struct file *file, const char __user * buf, size_t count, loff_t * ppos)
{
    char chr;
    size_t i;
    unsigned long long val = 0;

    for (i = 0; i < count; i++, buf += sizeof(char)) {
        if (get_user(chr, buf))
            return -EFAULT;
        if (chr >= '0' && chr <= '9') {
            val = val * 10ULL + (chr - '0');
        } else
            break;
    }
    vtss_time_limit = (cycles_t)val;
    TRACE("vtss_time_limit=%llu", vtss_time_limit);
    return count;
}

static int vtss_procfs_timelimit_open(struct inode *inode, struct file *file)
{
    /* TODO: ... */
    return 0;
}

static int vtss_procfs_timelimit_close(struct inode *inode, struct file *file)
{
    /* TODO: ... */
    return 0;
}

static const struct file_operations vtss_procfs_timelimit_fops = {
    .owner   = THIS_MODULE,
    .read    = vtss_procfs_timelimit_read,
    .write   = vtss_procfs_timelimit_write,
    .open    = vtss_procfs_timelimit_open,
    .release = vtss_procfs_timelimit_close,
};

/* ************************************************************************* */

static void vtss_procfs_rmdir(void)
{
    if (vtss_procfs_root != NULL) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        if (atomic_read(&vtss_procfs_root->count) == 1) {
            remove_proc_entry(THIS_MODULE->name, NULL);
#else
        remove_proc_subtree(THIS_MODULE->name, NULL);
#endif
            vtss_procfs_root = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        } else {
            ERROR("Entry '%s' is busy", vtss_procfs_path());
        }
#endif
    }
}

static int vtss_procfs_mkdir(void)
{
    struct path path;

    if (vtss_procfs_root == NULL) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        if (kern_path(vtss_procfs_path(), 0, &path)) {
            /* doesn't exist, so create it */
            vtss_procfs_root = proc_mkdir(THIS_MODULE->name, NULL);
        } else {
            /* if exist, attach to it */
            vtss_procfs_root = PDE(path.dentry->d_inode);
            path_put(&path);
        }
#else
        if (kern_path(vtss_procfs_path(), 0, &path) == 0) {
            /* if exist, remove it */
            remove_proc_subtree(THIS_MODULE->name, NULL);
         }
        /* doesn't exist, so create it */
        vtss_procfs_root = proc_mkdir(THIS_MODULE->name, NULL);
#endif
        if (vtss_procfs_root != NULL) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#ifdef VTSS_AUTOCONF_PROCFS_OWNER
            vtss_procfs_root->owner = THIS_MODULE;
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        vtss_procfs_root->uid = uid;
        vtss_procfs_root->gid = gid;
#else
#if defined CONFIG_UIDGID_STRICT_TYPE_CHECKS || (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
       {
          kuid_t kuid = KUIDT_INIT(uid);
          kgid_t kgid = KGIDT_INIT(gid);
          proc_set_user(vtss_procfs_root, kuid, kgid);
       }
#else
          proc_set_user(vtss_procfs_root, uid, gid);
#endif
#endif
        }
    }
    return (vtss_procfs_root != NULL) ? 0 : -ENOENT;
}

void vtss_procfs_fini(void)
{
    if (atomic_read(&vtss_procfs_attached)) {
        ERROR("procfs is still attached");
    }
    vtss_procfs_ctrl_flush();
    if (vtss_procfs_root != NULL) {
        remove_proc_entry(VTSS_PROCFS_CTRL_NAME,      vtss_procfs_root);
        remove_proc_entry(VTSS_PROCFS_DEBUG_NAME,     vtss_procfs_root);
        remove_proc_entry(VTSS_PROCFS_CPUMASK_NAME,   vtss_procfs_root);
        remove_proc_entry(VTSS_PROCFS_DEFSAV_NAME,    vtss_procfs_root);
        remove_proc_entry(VTSS_PROCFS_TARGETS_NAME,   vtss_procfs_root);
        remove_proc_entry(VTSS_PROCFS_TIMESRC_NAME,   vtss_procfs_root);
        remove_proc_entry(VTSS_PROCFS_TIMELIMIT_NAME, vtss_procfs_root);
        vtss_procfs_rmdir();
    }
}

static int vtss_procfs_create_entry(const char* name, const struct file_operations* fops)
{
    struct proc_dir_entry *pde = proc_create(name, (mode_t)(mode ? (mode & 0666) : 0660), vtss_procfs_root, fops);
    if (pde == NULL) {
        ERROR("Could not create '%s/%s'", vtss_procfs_path(), name);
        vtss_procfs_fini();
        return -ENOENT;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
// 3 lines below not supported anymore
#ifdef VTSS_AUTOCONF_PROCFS_OWNER
    pde->owner = THIS_MODULE;
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    pde->uid = uid;
    pde->gid = gid;
#else
#if defined CONFIG_UIDGID_STRICT_TYPE_CHECKS || (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
{
      kuid_t kuid = KUIDT_INIT(uid);
      kgid_t kgid = KGIDT_INIT(gid);
      proc_set_user(pde, kuid, kgid);
}
#else
          proc_set_user(pde, uid, gid);
#endif
#endif
    return 0;
}

int vtss_procfs_init(void)
{
    int rc = 0;
    unsigned long flags;

    atomic_set(&vtss_procfs_attached, 0);
    spin_lock_irqsave(&vtss_procfs_ctrl_list_lock, flags);
    INIT_LIST_HEAD(&vtss_procfs_ctrl_list);
    spin_unlock_irqrestore(&vtss_procfs_ctrl_list_lock, flags);
    cpumask_copy(&vtss_procfs_cpumask_, cpu_present_mask);
    vtss_procfs_defsav_ = 0;

    if (vtss_procfs_mkdir()) {
        ERROR("Could not create or find root directory '%s'", vtss_procfs_path());
        return -ENOENT;
    }
    rc |= vtss_procfs_create_entry(VTSS_PROCFS_CTRL_NAME,      &vtss_procfs_ctrl_fops);
    rc |= vtss_procfs_create_entry(VTSS_PROCFS_DEBUG_NAME,     &vtss_procfs_debug_fops);
    rc |= vtss_procfs_create_entry(VTSS_PROCFS_CPUMASK_NAME,   &vtss_procfs_cpumask_fops);
    rc |= vtss_procfs_create_entry(VTSS_PROCFS_DEFSAV_NAME,    &vtss_procfs_defsav_fops);
    rc |= vtss_procfs_create_entry(VTSS_PROCFS_TARGETS_NAME,   &vtss_procfs_targets_fops);
    rc |= vtss_procfs_create_entry(VTSS_PROCFS_TIMESRC_NAME,   &vtss_procfs_timesrc_fops);
    rc |= vtss_procfs_create_entry(VTSS_PROCFS_TIMELIMIT_NAME, &vtss_procfs_timelimit_fops);
    return rc;
}
