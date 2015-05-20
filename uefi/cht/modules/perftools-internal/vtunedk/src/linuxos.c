/*COPYRIGHT**
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.
 
    This file is part of SEP Development Kit
 
    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.
 
    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 
    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
**COPYRIGHT*/

#include "lwpmudrv_defines.h"
#include <linux/version.h>

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/profile.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/fs.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"
#include "inc/lwpmudrv.h"
#include "lwpmudrv_ioctl.h"
#include "inc/control.h"
#include "inc/utility.h"
#include "inc/output.h"

#include "inc/linuxos.h"

extern uid_t          uid;
extern volatile pid_t control_pid;
extern volatile S32   abnormal_terminate;
static volatile S32   hooks_installed = 0;

extern int
LWPMUDRV_Abnormal_Terminate(void);

#define MY_TASK  PROFILE_TASK_EXIT
#define MY_UNMAP PROFILE_MUNMAP

#if defined(DRV_IA32)
static U16
linuxos_Get_Exec_Mode (
    struct task_struct *p
)
{
    return ((unsigned short) MODE_32BIT);
}
#endif

#if defined(DRV_EM64T)
static U16
linuxos_Get_Exec_Mode (
    struct task_struct *p
)
{
    if (!p) {
        return MODE_UNKNOWN;
    }
    if (test_tsk_thread_flag(p,TIF_IA32)) {
        return ((unsigned short) MODE_32BIT);
    }
    return ((unsigned short) MODE_64BIT);
}
#endif

static S32
linuxos_Load_Image_Notify_Routine (
    char           *name,
    PVOID           base,
    U32             size,
    U64             page_offset,
    U32             pid,
    U32             parent_pid,
    U32             options,
    unsigned short  mode,
    S32             load_event
)
{
    char          *raw_path;
    ModuleRecord  *mra;
    char           buf[sizeof(ModuleRecord) + MAXNAMELEN + 32];
    U64            tsc_read;
    S32            local_load_event = (load_event==-1) ? 0 : load_event;
    U64            page_offset_shift;

    mra = (ModuleRecord *) buf;
    memset(mra, '\0', sizeof(buf));
    raw_path = (char*) mra + sizeof(ModuleRecord);

    page_offset_shift                              = page_offset << PAGE_SHIFT;
    MR_page_offset_Set(mra, page_offset_shift);
    MODULE_RECORD_processed(mra)                   = 0;
    MODULE_RECORD_segment_type(mra)                = mode;
    MODULE_RECORD_load_addr64(mra)                 = (U64)(size_t)base;
    MODULE_RECORD_length64(mra)                    = size;
    MODULE_RECORD_segment_number(mra)              = 1; // for user modules
    MODULE_RECORD_global_module_tb5(mra)           = options & LOPTS_GLOBAL_MODULE;
    MODULE_RECORD_first_module_rec_in_process(mra) = options & LOPTS_1ST_MODREC;
    MODULE_RECORD_tsc_used(mra)                    = 1;
    MODULE_RECORD_exe(mra)                         = 0;
    MODULE_RECORD_parent_pid(mra)                  = parent_pid;

    UTILITY_Read_TSC(&tsc_read);
    preempt_disable();
    tsc_read -= TSC_SKEW(CONTROL_THIS_CPU());
    preempt_enable();

    if (local_load_event) {
        MR_unloadTscSet(mra, tsc_read);
    }
    else {
        MR_unloadTscSet(mra, (U64)(-1));
    }
    MODULE_RECORD_pid_rec_index(mra)     = pid;
    MODULE_RECORD_pid_rec_index_raw(mra) = 1; // raw pid
#if defined(DEBUG)
    if (total_loads_init) {
        SEP_PRINT_DEBUG("samp_load_image_notify: setting pid_rec_index_raw pid 0x%x %s \n", 
                         pid, name);
    }
#endif

    strncpy(raw_path, name, MAXNAMELEN);
    raw_path[MAXNAMELEN]              = 0;
    MODULE_RECORD_path_length(mra)    =  (U16) strlen(raw_path) + 1;
    MODULE_RECORD_rec_length(mra)     =  (U16) ALIGN_8(sizeof (ModuleRecord) + 
                                                       MODULE_RECORD_path_length(mra));

#if defined(DRV_IA32)
    MODULE_RECORD_selector(mra)       = (pid==0) ? __KERNEL_CS : __USER_CS;
#endif
#if defined(DRV_EM64T)
    if (mode == MODE_64BIT) {
        MODULE_RECORD_selector(mra)   = (pid==0) ? __KERNEL_CS : __USER_CS;
    }
    else if (mode == MODE_32BIT) {
        MODULE_RECORD_selector(mra)   = (pid==0) ? __KERNEL32_CS : __USER32_CS;
    }
#endif

    if (LOPTS_EXE & options) {
        MODULE_RECORD_exe(mra) = 1;
    }

    OUTPUT_Module_Fill((PVOID)mra, MODULE_RECORD_rec_length(mra));

    return OS_SUCCESS;
}

#ifdef DRV_MM_EXE_FILE_PRESENT
static DRV_BOOL
linuxos_Equal_VM_Exe_File (
    struct vm_area_struct *vma 
)          
{         
    S8   name_vm_file[MAXNAMELEN];
    S8   name_exe_file[MAXNAMELEN]; 
    S8  *pname_vm_file = NULL; 
    S8  *pname_exe_file = NULL; 
              
    if (vma == NULL) {
        return FALSE; 
    } 
     
    if (vma->vm_file == NULL) { 
        return FALSE;
    } 
     
    if (vma->vm_mm->exe_file == NULL) { 
        return FALSE; 
    } 
 
    pname_vm_file = D_PATH(vma->vm_file, name_vm_file, MAXNAMELEN);
    pname_exe_file = D_PATH(vma->vm_mm->exe_file, name_exe_file, MAXNAMELEN);
    return (strcmp (pname_vm_file, pname_exe_file) == 0);
}
#endif


//
// Register the module for a process.  The task_struct and mm
// should be locked if necessary to make sure they don't change while we're
// iterating...
// Used as a service routine
//
static S32
linuxos_VMA_For_Process (
    struct task_struct    *p,
    struct vm_area_struct *vma,
    S32                    load_event,
    U32                   *first
)
{
    U32  options = 0;
    S8   name[MAXNAMELEN];
    S8  *pname = NULL;
    U32  ppid  = 0;
    U16  exec_mode; 
    U64 page_offset = 0;

#if defined(DRV_ANDROID)
    char andr_app[MAXNAMELEN +1];
#endif

    if (p == NULL) {
       SEP_PRINT_ERROR("linuxos_VMA_For_Process skipped p=NULL\n")
       return OS_SUCCESS;
    }

    if (vma->vm_file) pname = D_PATH(vma->vm_file, name, MAXNAMELEN);

    page_offset = vma->vm_pgoff;

    if (!IS_ERR(pname) && pname != NULL) {
    SEP_PRINT_DEBUG("enum: %s, %d, %lx, %lx %llu\n",
                         pname, p->pid, vma->vm_start, (vma->vm_end - vma->vm_start), page_offset);

        // if the VM_EXECUTABLE flag is set then this is the module
        // that is being used to name the module
        if (DRV_VM_MOD_EXECUTABLE(vma)) {
            options |= LOPTS_EXE;
#if defined(DRV_ANDROID)
            if(!strcmp (pname, "/system/bin/app_process")){
               memset(andr_app, '\0', MAXNAMELEN + 1);
               strncpy(andr_app, p->comm, MAXNAMELEN);
               pname = andr_app;
            }
#endif
        }
        // mark the first of the bunch...
        if (*first == 1) {
            options |= LOPTS_1ST_MODREC;
            *first = 0;
        }
    }
#if defined(DRV_IA32) || defined(DRV_EM64T)
#if defined(DRV_ALLOW_VDSO)
    else if (vma->vm_mm && 
             vma->vm_start == (long)vma->vm_mm->context.vdso) {
        pname = "[vdso]";
    }
#endif
#if defined(DRV_ALLOW_SYSCALL)
    else if (vma->vm_start == VSYSCALL_START) {
        pname = "[vsyscall]";
    }
#endif
#endif

    if (pname != NULL) {
        options = 0;
        if (DRV_VM_MOD_EXECUTABLE(vma)) {
            options |= LOPTS_EXE;
        }

        if (p && p->parent) {
            ppid = p->parent->tgid;
        }
        exec_mode = linuxos_Get_Exec_Mode(p);
        // record this module
        linuxos_Load_Image_Notify_Routine(pname,
                                          (PVOID)vma->vm_start,
                                          (vma->vm_end - vma->vm_start),
                                          page_offset,
                                          p->pid,
                                          ppid,
                                          options,
                                          exec_mode,
                                          load_event);
    }
    return OS_SUCCESS;
}

//
// Common loop to enumerate all modules for a process.  The task_struct and mm
// should be locked if necessary to make sure they don't change while we're
// iterating...
//
static S32
linuxos_Enum_Modules_For_Process (
    struct task_struct *p, 
    struct mm_struct   *mm,
    S32                 load_event
)
{
    struct vm_area_struct *mmap;
    U32                    first = 1;

#if defined(SECURE_SEP)
    uid_t                  l_uid;

    l_uid = DRV_GET_UID(p);
    /*
     * Check for:  same uid, or root uid
     */
    if (l_uid != uid && l_uid != 0) {
        return OS_SUCCESS;
    }
#endif
    for (mmap = mm->mmap; mmap; mmap = mmap->vm_next) {
        /* We have 3 distinct conditions here.
         * 1) Is the page executable?
         * 2) Is is a part of the vdso area?
         * 3) Is it the vsyscall area?
         */
        if (((mmap->vm_flags & VM_EXEC) && 
              mmap->vm_file             && 
              mmap->vm_file->f_dentry) 
#if defined(DRV_IA32) || defined(DRV_EM64T)
#if defined(DRV_ALLOW_VDSO)
             ||
             (mmap->vm_mm          &&
              mmap->vm_start == (long)mmap->vm_mm->context.vdso)
#endif
#endif
#if defined(DRV_ALLOW_VSYSCALL)
             ||
             (mmap->vm_start == VSYSCALL_START)
#endif
             ) {

            linuxos_VMA_For_Process(p, 
                                    mmap, 
                                    load_event, 
                                    &first);
        }
    }

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn          static int linuxos_Exec_Unmap_Notify(
 *                  struct notifier_block  *self, 
 *                  unsigned long           val, 
 *                  VOID                   *data) 
 *
 * @brief       this function is called whenever a task exits 
 *
 * @param       self IN  - not used 
 *              val  IN  - not used 
 *              data IN  - this is cast in the mm_struct of the task that is call unmap 
 *
 * @return      none
 *
 * <I>Special Notes:</I>
 *
 * This notification is called from do_munmap(mm/mmap.c). This is called when ever 
 * a module is loaded or unloaded. It looks like it is called right after a module is 
 * loaded or before its unloaded (if using dlopen,dlclose). 
 * However it is not called when a process is exiting instead exit_mmap is called 
 * (resulting in an EXIT_MMAP notification).
 */
static int
linuxos_Exec_Unmap_Notify (
    struct notifier_block *self, 
    unsigned long          val, 
    PVOID                  data
)
{
    struct mm_struct      *mm;
    struct vm_area_struct *mmap  = NULL;
    U32                    first = 1;         

#if defined(SECURE_SEP)
    uid_t                  l_uid;

    l_uid = DRV_GET_UID(current);
    /*
     * Check for:  same uid, or root uid
     */
    if (l_uid != uid && l_uid != 0) {
        return 0;
    }
#endif

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_UNINITIALIZED) {
        return 0;
    }

    mm = current->mm;
    down_read(&mm->mmap_sem);
    mmap = FIND_VMA (mm, data);
    if (mmap               && 
        mmap->vm_file      && 
        (mmap->vm_flags & VM_EXEC)) {

        linuxos_VMA_For_Process(current, mmap, TRUE, &first);
    }
    up_read(&mm->mmap_sem);

    return 0;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          OS_STATUS LINUXOS_Enum_Process_Modules(DRV_BOOL at_end) 
 *
 * @brief       gather all the process modules that are present.
 *
 * @param       at_end - the collection happens at the end of the sampling run 
 *
 * @return      OS_SUCCESS
 *
 * <I>Special Notes:</I>
 *              This routine gathers all the process modules that are present
 *              in the system at this time.  If at_end is set to be TRUE, then
 *              act as if all the modules are being unloaded.
 *
 */
extern OS_STATUS
LINUXOS_Enum_Process_Modules (
    DRV_BOOL  at_end
)
{
    int                 n = 0;
    struct task_struct *p;

    SEP_PRINT_DEBUG("Enum_Process_Modules begin tasks\n");

    if (abnormal_terminate == 1) {
        return OS_SUCCESS;
    }
    FOR_EACH_TASK(p) {
        SEP_PRINT_DEBUG("Enum_Process_Modules looking at task %d\n", n);
        /*
         *  Call driver notification routine for each module 
         *  that is mapped into the process created by the fork
         */
          
        if (p == NULL) { 
            SEP_PRINT_DEBUG("Enum_Process_Modules skipped p=NULL\n");
            continue;
        }
        if (p->mm == NULL) {
            SEP_PRINT_DEBUG("Enum_Process_Modules skipped p=0x%p (pid=%d), p->mm=NULL, p->comm=%s\n", p, p->pid, p->comm);
            if (p->comm) {
                linuxos_Load_Image_Notify_Routine(p->comm,
                                                  NULL,
                                                  0,
                                                  0,
                                                  p->pid,
                                                  (p->parent) ? p->parent->tgid : 0,
                                                  LOPTS_EXE | LOPTS_1ST_MODREC,
                                                  linuxos_Get_Exec_Mode(p),
                                                  1);
            }
            continue;
        }
        if (!UTILITY_down_read_mm(p)) {
            SEP_PRINT_ERROR("Linux_Enum_Process_Modules_End: unable to get lock on mmap_sem!\n");
            return OS_SUCCESS;
        }
        linuxos_Enum_Modules_For_Process(p, p->mm, at_end?-1:0);
        UTILITY_up_read_mm(p);
        n++;
    }
    SEP_PRINT_DEBUG("Enum_Process_Modules done with %d tasks\n", n);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          static int linuxos_Exit_Task_Notify(struct notifier_block * self, 
 *                  unsigned long val, PVOID data) 
 * @brief       this function is called whenever a task exits 
 *
 * @param       self IN  - not used 
 *              val IN  - not used 
 *              data IN  - this is cast into the task_struct of the exiting task 
 *
 * @return      none
 *
 * <I>Special Notes:</I>
 * this function is called whenever a task exits.  It is called right before
 * the virtual memory areas are freed.  We just enumerate through all the modules
 * of the task and set the unload sample count and the load event flag to 1 to
 * indicate this is a module unload
 */
static int
linuxos_Exit_Task_Notify (
    struct notifier_block *self,
    unsigned long          val,
    PVOID                  data
)
{
    struct task_struct *p     = (struct task_struct *)data;
    int                status = OS_SUCCESS;

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_UNINITIALIZED) {
        return status;
    }

    if (!p->mm) {
         return status;
    }

    SEP_PRINT_DEBUG("exit_task_notify pid = %d tgid = %d\n", p->pid, p->tgid);
    if (p->pid == control_pid) {
        SEP_PRINT_DEBUG("The collector task has been terminated via an uncatchable signal\n");

        if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_PREPARE_STOP ||
            GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_STOPPED) {
            return status;
        }
        status = LWPMUDRV_Abnormal_Terminate();
    }
    else if (abnormal_terminate == 0) {
        linuxos_Enum_Modules_For_Process(p, p->mm, 1);
    }

    return status;
}


/*
 *  The notifier block.  All the static entries have been defined at this point
 */
static struct notifier_block linuxos_exec_unmap_nb = {
    .notifier_call = linuxos_Exec_Unmap_Notify,
};

static struct notifier_block linuxos_exit_task_nb = {
    .notifier_call = linuxos_Exit_Task_Notify,
};

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID LINUXOS_Install_Hooks(VOID) 
 * @brief       registers the profiling callbacks 
 *
 * @param       none 
 *
 * @return      none
 *
 * <I>Special Notes:</I>
 *
 * None
 */
extern VOID
LINUXOS_Install_Hooks (
    VOID
)
{
    int err = 0;
    int err2 = 0;

    if (hooks_installed == 1) {
        SEP_PRINT_DEBUG("The OS Hooks are already installed\n");
        return;
    }
    err = profile_event_register(MY_UNMAP, &linuxos_exec_unmap_nb);
    err2= profile_event_register(MY_TASK,  &linuxos_exit_task_nb);
    if (err || err2) {
        if (err == OS_NO_SYSCALL) {
            SEP_PRINT_WARNING("This kernel does not implement kernel profiling hooks.  "
                              "Task termination and image unloads will not be tracked "
                              "during sampling session!!\n");
        }
    }
    hooks_installed = 1;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID LINUXOS_Uninstall_Hooks(VOID) 
 * @brief       unregisters the profiling callbacks 
 *
 * @param       none 
 *
 * @return      
 *
 * <I>Special Notes:</I>
 *
 * None
 */
extern VOID
LINUXOS_Uninstall_Hooks (
    VOID
)
{
    if (hooks_installed == 0) {
        return;
    }
    SEP_PRINT_DEBUG("Uninstalling OS Hooks\n");
    hooks_installed = 0;
    profile_event_unregister(MY_UNMAP, &linuxos_exec_unmap_nb);
    profile_event_unregister(MY_TASK,  &linuxos_exit_task_nb);

    return;
}
