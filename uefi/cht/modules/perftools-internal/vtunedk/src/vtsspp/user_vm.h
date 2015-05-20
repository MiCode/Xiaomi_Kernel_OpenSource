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
#ifndef _VTSS_USER_VM_H_
#define _VTSS_USER_VM_H_

#include "vtss_autoconf.h"

#include <linux/sched.h>        /* for struct task_struct */
#include <linux/mm.h>           /* for struct vm_area_struct */

typedef struct user_vm_accessor
{
/* public: */
    int    (*trylock) (struct user_vm_accessor* this, struct task_struct* task);
    int    (*unlock)  (struct user_vm_accessor* this);
    size_t (*read)    (struct user_vm_accessor* this, void* from, void* to, size_t size);
    int    (*validate)(struct user_vm_accessor* this, unsigned long ip);

/* private: */
    struct task_struct*    m_task;
    struct mm_struct*      m_mm;
    struct page*           m_page;
    struct vm_area_struct* m_vma;
    void*                  m_maddr;
    unsigned long          m_page_id;
    int                    m_irq;
    cycles_t               m_limit;
#ifdef VTSS_VMA_TIME_LIMIT
    cycles_t               m_time;
#endif
#ifdef VTSS_VMA_CACHE
    char                   m_buffer[PAGE_SIZE];
#endif

    int mmap_reg_callcnt;

    struct vm_area_struct* m_vma_cache;

    unsigned long mmap_vdso_start;
    unsigned long mmap_vdso_end;
    unsigned long mmap_mms_start;
    unsigned long mmap_mms_end;
    unsigned long mmap_stack_start;
    unsigned long mmap_stack_end;

} user_vm_accessor_t;

user_vm_accessor_t* vtss_user_vm_accessor_init(int in_irq, cycles_t limit);
void vtss_user_vm_accessor_fini(user_vm_accessor_t* acc);

int  vtss_user_vm_init(void);
void vtss_user_vm_fini(void);

#endif /* _VTSS_USER_VM_H_ */
