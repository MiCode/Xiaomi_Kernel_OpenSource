/*
 * The file implements ioctl interface for vIDT driver.
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#include "include/vmm_hsym_common.h"
#include "include/vmm_hsym.h"
#include "include/vidt_ioctl.h"
#include <linux/uaccess.h>
#include <linux/module.h>
#include "include/kern_ta.h"
#include <linux/slab.h>
#include <asm/siginfo.h>    //siginfo
#include <linux/rcupdate.h> //rcu_read_lock
#include <linux/sched.h>    //find_task_by_pid_type
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>

int setup_vidt(void);
int restore_os_idt(void);
extern int init_view_map_list(struct file *file);
extern int map_pid_viewId(void *private_data, struct entry_pid_viewid view_new);
extern int unmap_pid_viewId(void *private_data, struct entry_pid_viewid view_entry);
extern int clean_ta_view( void *private_data);
extern void clean_view_map_list(struct file *file);

#ifdef __x86_64__
extern inline int cpuid_asm64(uint32_t leaf, uint32_t b_val, uint64_t c,
        uint64_t d, uint64_t S, uint64_t D);
#else
extern inline int cpuid_asm(uint32_t leaf, uint32_t b_val, uint32_t c,
        uint32_t d, uint32_t S, uint32_t D);
#endif

#ifdef __x86_64__
extern inline int vmcall_asm64(uint32_t leaf, uint32_t b_val, uint64_t c,
			       uint64_t d, uint64_t S, uint64_t D);
#else
extern inline int vmcall_asm(uint32_t leaf, uint32_t b_val, uint32_t c,
			     uint32_t d, uint32_t S, uint32_t D);
#endif

#define SIG_CLEAN 44 // we choose 44 as our signal number (real-time signals are in the range of 33 to 64)
#define DEBUG_LEAF 42
#define EPTP_SWITCH_LEAF 0
#define D_VIEW           0
unsigned int g_view = 0;
#define ENABLE_EPT 1
#define DISABLE_EPT 0

#define VMM_PROT_ACTION 0
#define VMM_UNPROT_ACTION 1
static int hspid = 0;

inline void vmfunc_emul(uint32_t a, uint32_t c, uint32_t d)
{
    asm volatile ("vmcall": : "a"(a), "c"(c), "d"(d));
}

inline void vmfunc(uint32_t a, uint32_t c)
{

    asm volatile ("nop"
                  : : "a"(a), "c"(c));
    asm volatile (".byte 0x0f");
    asm volatile (".byte 0x01");
    asm volatile (".byte 0xd4");
}

static int chk_hsim_version()
{
    sl_info_t info;
    int ret;

    memset(&info, 0, sizeof(sl_info_t));

#ifdef __x86_64__
	ret = cpuid_asm64(SL_CMD_HSEC_GET_INFO, CMD_GET,
			   (uint64_t)&info, sizeof(info), 0, 0);
#else
	ret = cpuid_asm(SL_CMD_HSEC_GET_INFO, CMD_GET,
			 (uint32_t)&info, sizeof(info), 0, 0);
#endif
    if (ret)
        return SL_EUNKNOWN;
    if (info.major_version != MAJOR_VERSION
            && info.minor_version != MINOR_VERSION
            && info.vendor_id[0] != 0x53696c65
            && info.vendor_id[1] != 0x6e742020
            && info.vendor_id[2] != 0x4c616b65)
    {
        printk("Error: Hsim Version mismatch\n");
        return SL_EVERSION_MISMATCH;
    }
    return SL_SUCCESS;
}

static int vidt_ioctl(struct file *file,unsigned int vidt_command,unsigned long vidt_param)
{
    struct entry_pid_viewid  param_entry;
    int err = -1;
    static uint32_t setup_vidt_done = 0;
    switch(vidt_command)
    {
       case VIDT_PID_VIEW_MAP :
            if((struct entry_pid_viewid __user  *)vidt_param != NULL) {
               err = copy_from_user(&param_entry,(struct entry_pid_viewid __user  *)vidt_param,sizeof(struct entry_pid_viewid));
               if(err)
               {
                  printk("copy_from_user err = %d\n",err);
                  return -EFAULT;
               }
            }
            return map_pid_viewId(file->private_data, param_entry);

       case VIDT_PID_VIEW_UNMAP :
            if((struct entry_pid_viewid __user  *)vidt_param != NULL) {
               err = copy_from_user(&param_entry,(struct entry_pid_viewid __user  *)vidt_param,sizeof(struct entry_pid_viewid));
               if(err)
               {
                  printk("copy_from_user err = %d\n",err);
                  return -EFAULT;
               }
            }
            return unmap_pid_viewId(file->private_data, param_entry);

       case VIDT_REGISTER:
             if(!setup_vidt_done) {
                err = setup_vidt();
                if(err == 0) {
                   setup_vidt_done = 1;
                }
             }
             return err;
       case VIDT_REGISTER_HP_PID:
            if((pid_t *)vidt_param != NULL) {
               err = copy_from_user(&hspid,(pid_t*)vidt_param,sizeof(pid_t));
               printk("copy_from_user hspid = %d\n",(int)hspid);
               if(err)
               {
                  printk("copy_from_user err = %d\n",err);
                  return -EFAULT;
               }
            }
            return 0;
       default :
            printk("INVALID %d\n", vidt_command);
       break;
    }
    return err;
}

static int vidt_open(struct inode *inode,struct file *file)
{
    int status = 0;
    file->private_data = NULL;
    status = init_view_map_list(file);
    if (status) {
        printk("view map list initialization failed\n");
        return -1;
    }
    return 0;
}

static int vidt_close(struct inode *inode,struct file *file)
{
    struct siginfo info;
    struct task_struct *t;
    int ret;

    clean_ta_view(file->private_data);
    clean_view_map_list(file);


    /* send the signal */
    memset(&info, 0, sizeof(struct siginfo));
    info.si_signo = SIG_CLEAN;
    info.si_code = SI_QUEUE;    // this is bit of a trickery: SI_QUEUE is normally used by sigqueue from user space,
    // and kernel space should use SI_KERNEL. But if SI_KERNEL is used the real_time data 
    // is not delivered to the user space signal handler function.
    info.si_pid = current->pid;         //real time signals may have 32 bits of data, so send pid
    info.si_int = 0xDEAD;

    rcu_read_lock();
    t = pid_task(find_pid_ns(hspid, &init_pid_ns), PIDTYPE_PID);//find the task_struct associated with this pid
    if(t == NULL){
        printk("no such pid\n");
        rcu_read_unlock();
        return -ENODEV;
    }
    rcu_read_unlock();
    ret = send_sig_info(SIG_CLEAN, &info, t);    //send the signal
    if (ret < 0) {
        printk("error sending signal\n");
        return ret;
    }
       return 0;
}

static const struct file_operations vidt_fops = {
    .owner = THIS_MODULE,
    .open  = vidt_open,
    .release = vidt_close,
    .compat_ioctl = vidt_ioctl
};


static struct miscdevice vidt_device = {
    .minor  =  MISC_DYNAMIC_MINOR,
    .name   =  "vidt",
    .fops   =  &vidt_fops,

};
int __init vmm_mod_init(void) {
    int ret = 0;

    ret = chk_hsim_version();
    if (ret != 0)
        return SL_EVERSION_MISMATCH;

    ret = misc_register(&vidt_device);
    if(ret)
        printk("errot ret = %d\n ",ret);

    printk("initing vmmctrl module \n");


    return ret;
}

void __exit vmm_mod_exit(void) {
    misc_deregister(&vidt_device);
    restore_os_idt();
}

module_init(vmm_mod_init);
module_exit(vmm_mod_exit);

MODULE_LICENSE("GPL v2");
