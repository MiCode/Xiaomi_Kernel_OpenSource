/*
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
*/

#ifndef _LINUXOS_H_
#define _LINUXOS_H_

// defines for options parameter of samp_load_image_notify_routine()
#define LOPTS_1ST_MODREC     0x1
#define LOPTS_GLOBAL_MODULE  0x2
#define LOPTS_EXE            0x4

#define FOR_EACH_TASK        for_each_process

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
#define D_PATH(vm_file, name, maxlen)     \
    d_path((vm_file)->f_dentry, (vm_file)->f_vfsmnt, (name), (maxlen))
#else
#define D_PATH(vm_file, name, maxlen)     \
    d_path(&((vm_file)->f_path), (name), (maxlen))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
#define DRV_VM_MOD_EXECUTABLE(vma)    \
    (vma->vm_flags & VM_EXECUTABLE)
#else
#define DRV_VM_MOD_EXECUTABLE(vma)    \
    (linuxos_Equal_VM_Exe_File(vma))
#define DRV_MM_EXE_FILE_PRESENT
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
#define DRV_ALLOW_VDSO
#endif

#if defined(DRV_IA32)
#define FIND_VMA(mm, data)   find_vma ((mm), (U32)(data));
#endif
#if defined(DRV_EM64T)
#define FIND_VMA(mm, data)   find_vma ((mm), (U64)(data));
#endif

extern VOID
LINUXOS_Install_Hooks (
    VOID
);

extern VOID
LINUXOS_Uninstall_Hooks (
    VOID
);

extern OS_STATUS
LINUXOS_Enum_Process_Modules (
    DRV_BOOL at_end
);

#endif 
