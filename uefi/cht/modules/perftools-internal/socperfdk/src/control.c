/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2005-2014 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  BSD LICENSE 

  Copyright(c) 2005-2014 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  ***********************************************************************************************
*/


#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/mempool.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "socperfdrv.h"
#include "control.h"
#include <linux/sched.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#define SMP_CALL_FUNCTION(func,ctx,retry,wait)    smp_call_function((func),(ctx),(wait))
#else
#define SMP_CALL_FUNCTION(func,ctx,retry,wait)    smp_call_function((func),(ctx),(retry),(wait))
#endif

/*
 *  Global State Nodes - keep here for now.  Abstract out when necessary.
 */
GLOBAL_STATE_NODE  driver_state;
MSR_DATA           msr_data      = NULL;
MEM_TRACKER        mem_tr_head   = NULL;   // start of the mem tracker list
MEM_TRACKER        mem_tr_tail   = NULL;   // end of mem tracker list
spinlock_t         mem_tr_lock;            // spinlock for mem tracker list

/* ------------------------------------------------------------------------- */
/*!
 * @fn       VOID CONTROL_Invoke_Cpu (func, ctx, arg)
 *
 * @brief    Set up a DPC call and insert it into the queue
 *
 * @param    IN cpu_idx  - the core id to dispatch this function to
 *           IN func     - function to be invoked by the specified core(s)
 *           IN ctx      - pointer to the parameter block for each function
 *                         invocation
 *
 * @return   None
 *
 * <I>Special Notes:</I>
 *
 */
extern VOID
CONTROL_Invoke_Cpu (
    int     cpu_idx,
    VOID    (*func)(PVOID),
    PVOID   ctx
)
{
    CONTROL_Invoke_Parallel(func, ctx);

    return;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn VOID CONTROL_Invoke_Parallel_Service(func, ctx, blocking, exclude)
 *
 * @param    func     - function to be invoked by each core in the system
 * @param    ctx      - pointer to the parameter block for each function invocation
 * @param    blocking - Wait for invoked function to complete
 * @param    exclude  - exclude the current core from executing the code
 *
 * @returns  None
 *
 * @brief    Service routine to handle all kinds of parallel invoke on all CPU calls
 *
 * <I>Special Notes:</I>
 *           Invoke the function provided in parallel in either a blocking or
 *           non-blocking mode.  The current core may be excluded if desired.
 *           NOTE - Do not call this function directly from source code.
 *           Use the aliases CONTROL_Invoke_Parallel(), CONTROL_Invoke_Parallel_NB(),
 *           or CONTROL_Invoke_Parallel_XS().
 *
 */
extern VOID
CONTROL_Invoke_Parallel_Service (
    VOID   (*func)(PVOID),
    PVOID  ctx,
    int    blocking,
    int    exclude
)
{
    GLOBAL_STATE_cpu_count(driver_state) = 0;
    GLOBAL_STATE_dpc_count(driver_state) = 0;

    preempt_disable();
    SMP_CALL_FUNCTION (func, ctx, 0, blocking);

    if (!exclude) {
        func(ctx);
    }
    preempt_enable();

    return;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn VOID control_Memory_Tracker_Delete_Node(mem_tr)
 *
 * @param    IN mem_tr    - memory tracker node to delete
 *
 * @returns  None
 *
 * @brief    Delete specified node in the memory tracker
 *
 * <I>Special Notes:</I>
 *           Assumes mem_tr_lock is already held while calling this function!
 */
static VOID
control_Memory_Tracker_Delete_Node (
    MEM_TRACKER mem_tr
)
{
    MEM_TRACKER prev_tr = NULL;
    MEM_TRACKER next_tr = NULL;
    U32         size    = MEM_EL_MAX_ARRAY_SIZE * sizeof(MEM_EL_NODE);

    if (! mem_tr) {
        return;
    }

    // free the allocated mem_el array (if any)
    if (MEM_TRACKER_mem(mem_tr)) {
        if (size < MAX_KMALLOC_SIZE) {
            kfree(MEM_TRACKER_mem(mem_tr));
        }
        else {
            free_pages((unsigned long)MEM_TRACKER_mem(mem_tr), get_order(size));
        }
    }

    // update the linked list
    prev_tr = MEM_TRACKER_prev(mem_tr);
    next_tr = MEM_TRACKER_next(mem_tr);
    if (prev_tr) {
        MEM_TRACKER_next(prev_tr) = next_tr;
    }
    if (next_tr) {
        MEM_TRACKER_prev(next_tr) = prev_tr;
    }

    // free the mem_tracker node
    kfree(mem_tr);

    return;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn VOID control_Memory_Tracker_Create_Node(void)
 *
 * @param    None    - size of the memory to allocate
 *
 * @returns  OS_SUCCESS if successful, otherwise error
 *
 * @brief    Initialize the memory tracker
 *
 * <I>Special Notes:</I>
 *           Assumes mem_tr_lock is already held while calling this function!
 *
 *           Since this function can be called within either GFP_KERNEL or
 *           GFP_ATOMIC contexts, the most restrictive allocation is used
 *           (viz., GFP_ATOMIC).
 */
static U32
control_Memory_Tracker_Create_Node (
    void
)
{
    U32         size     = MEM_EL_MAX_ARRAY_SIZE * sizeof(MEM_EL_NODE);
    PVOID       location = NULL;
    MEM_TRACKER mem_tr   = NULL;

    // create a mem tracker node
    mem_tr = (MEM_TRACKER)kmalloc(sizeof(MEM_TRACKER_NODE), GFP_ATOMIC);
    if (!mem_tr) {
        SOCPERF_PRINT_ERROR("control_Initialize_Memory_Tracker: failed to allocate mem tracker node\n");
        return OS_FAULT;
    }

    // create an initial array of mem_el's inside the mem tracker node
    if (size < MAX_KMALLOC_SIZE) {
        location = (PVOID)kmalloc(size, GFP_ATOMIC);
        SOCPERF_PRINT_DEBUG("control_Memory_Tracker_Create_Node: allocated small memory (0x%p, %d)\n", location, (S32) size);
    }
    else {
        location = (PVOID)__get_free_pages(GFP_ATOMIC, get_order(size));
        SOCPERF_PRINT_DEBUG("control_Memory_Tracker_Create_Node: allocated large memory (0x%p, %d)\n", location, (S32) size);
    }

    // initialize new mem tracker node
    MEM_TRACKER_mem(mem_tr)  = location;
    MEM_TRACKER_prev(mem_tr) = NULL;
    MEM_TRACKER_next(mem_tr) = NULL;

    // if mem_el array allocation failed, then remove node
    if (!MEM_TRACKER_mem(mem_tr)) {
        control_Memory_Tracker_Delete_Node(mem_tr);
        SOCPERF_PRINT_ERROR("control_Memory_Tracker_Create_Node: failed to allocate mem_el array in tracker node ... deleting node\n");
        return OS_FAULT;
    }

    // initialize mem_tracker's mem_el array
    MEM_TRACKER_max_size(mem_tr) = MEM_EL_MAX_ARRAY_SIZE;
    memset(MEM_TRACKER_mem(mem_tr), 0, size);

    // update the linked list
    if (!mem_tr_head) {
        mem_tr_head = mem_tr;
    }
    else {
        MEM_TRACKER_prev(mem_tr) = mem_tr_tail;
        MEM_TRACKER_next(mem_tr_tail) = mem_tr;
    }
    mem_tr_tail = mem_tr;
    SOCPERF_PRINT_DEBUG("control_Memory_Tracker_Create_node: allocating new node=0x%p, max_elements=%d, size=%d\n",
                    MEM_TRACKER_mem(mem_tr_tail), MEM_EL_MAX_ARRAY_SIZE, size);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn VOID control_Memory_Tracker_Add(location, size, vmalloc_flag)
 *
 * @param    IN location     - memory location
 * @param    IN size         - size of the memory to allocate
 * @param    IN vmalloc_flag - flag that indicates if the allocation was done with vmalloc
 *
 * @returns  None
 *
 * @brief    Keep track of allocated memory with memory tracker
 *
 * <I>Special Notes:</I>
 *           Starting from first mem_tracker node, the algorithm
 *           finds the first "hole" in the mem_tracker list and
 *           tracks the memory allocation there.
 */
static U32
control_Memory_Tracker_Add (
    PVOID     location,
    ssize_t   size,
    DRV_BOOL  vmalloc_flag
)
{
    S32         i, n;
    U32         status;
    DRV_BOOL    found;
    MEM_TRACKER mem_tr;

    spin_lock(&mem_tr_lock);

    // check if there is space in ANY of mem_tracker's nodes for the memory item
    mem_tr = mem_tr_head;
    found = FALSE;
    status = OS_SUCCESS;
    i = n = 0;
    while (mem_tr && (!found)) {
        for (i = 0; i < MEM_TRACKER_max_size(mem_tr); i++) {
            if (!MEM_TRACKER_mem_address(mem_tr,i)) {
                SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Add: found index %d of %d available\n",
                                        i,
                                        MEM_TRACKER_max_size(mem_tr)-1);
                n = i;
                found = TRUE;
            }
        }
        if (!found) {
            mem_tr = MEM_TRACKER_next(mem_tr);
        }
    }

    if (!found) {
        // extend into (i.e., create new) mem_tracker node ...
        status = control_Memory_Tracker_Create_Node();
        if (status != OS_SUCCESS) {
            SOCPERF_PRINT_ERROR("Unable to create mem tracker node\n");
            goto finish_add;
        }
        // use mem tracker tail node and first available entry in mem_el array
        mem_tr = mem_tr_tail;
        n = 0;
    }

    // we now have a location in mem tracker to keep track of the memory item
    MEM_TRACKER_mem_address(mem_tr,n) = location;
    MEM_TRACKER_mem_size(mem_tr,n)    = size;
    MEM_TRACKER_mem_vmalloc(mem_tr,n) = vmalloc_flag;
    SOCPERF_PRINT_DEBUG("control_Memory_Tracker_Add: tracking (0x%p, %d) in node %d of %d\n",
                     location, (S32)size, n, MEM_TRACKER_max_size(mem_tr)-1);

finish_add:
    spin_unlock(&mem_tr_lock);

    return status;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn VOID CONTROL_Memory_Tracker_Init(void)
 *
 * @param    None
 *
 * @returns  None
 *
 * @brief    Initializes Memory Tracker
 *
 * <I>Special Notes:</I>
 *           This should only be called when the driver is being loaded.
 */
extern VOID
CONTROL_Memory_Tracker_Init (
    VOID
)
{
    SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Init: initializing mem tracker\n");

    mem_tr_head = NULL;
    mem_tr_tail = NULL;

    spin_lock_init(&mem_tr_lock);

    return;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn VOID CONTROL_Memory_Tracker_Free(void)
 *
 * @param    None
 *
 * @returns  None
 *
 * @brief    Frees memory used by Memory Tracker
 *
 * <I>Special Notes:</I>
 *           This should only be called when the driver is being unloaded.
 */
extern VOID
CONTROL_Memory_Tracker_Free (
    VOID
)
{
    S32         i;
    MEM_TRACKER temp;

    SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Free: destroying mem tracker\n");

    spin_lock(&mem_tr_lock);

    // check for any memory that was not freed, and free it
    while (mem_tr_head) {
        for (i = 0; i < MEM_TRACKER_max_size(mem_tr_head); i++) {
            if (MEM_TRACKER_mem_address(mem_tr_head,i)) {
                SOCPERF_PRINT_WARNING("CONTROL_Memory_Tracker_Free: index %d of %d, not freed (0x%p, %d) ... freeing now\n",
                                             i,
                                             MEM_TRACKER_max_size(mem_tr_head)-1,
                                             MEM_TRACKER_mem_address(mem_tr_head,i),
                                             MEM_TRACKER_mem_size(mem_tr_head,i));
                free_pages((unsigned long)MEM_TRACKER_mem_address(mem_tr_head,i), get_order(MEM_TRACKER_mem_size(mem_tr_head,i)));
                MEM_TRACKER_mem_address(mem_tr_head,i) = NULL;
                MEM_TRACKER_mem_size(mem_tr_head,i)    = 0;
                MEM_TRACKER_mem_vmalloc(mem_tr_head,i) = FALSE;
            }
        }
        temp = MEM_TRACKER_next(mem_tr_head);
        control_Memory_Tracker_Delete_Node(mem_tr_head);
        mem_tr_head = temp;
    }

    spin_unlock(&mem_tr_lock);

    SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Free: mem tracker destruction complete\n");

    return;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn VOID CONTROL_Memory_Tracker_Compaction(void)
 *
 * @param    None
 *
 * @returns  None
 *
 * @brief    Compacts the memory allocator if holes are detected
 *
 * <I>Special Notes:</I>
 *           The algorithm compacts mem_tracker nodes such that
 *           node entries are full starting from mem_tr_head
 *           up until the first empty node is detected, after
 *           which nodes up to mem_tr_tail will be empty.
 *           At end of collection (or at other safe sync point),
 *           we reclaim/compact space used by mem tracker.
 */
extern VOID
CONTROL_Memory_Tracker_Compaction (
    void
)
{
    S32         i, j, n, m, c, d;
    DRV_BOOL    found, overlap;
    MEM_TRACKER mem_tr1, mem_tr2;

    spin_lock(&mem_tr_lock);

    mem_tr1 = mem_tr_head;
    mem_tr2 = mem_tr_tail;

    // if memory tracker was never used, then no need to compact
    if (!mem_tr1 || !mem_tr2) {
        goto finish_compact;
    }

    i = j = n = c = d = 0;
    m = MEM_TRACKER_max_size(mem_tr2) - 1;
    overlap = FALSE;
    while (!overlap) {
        // find an empty node
        found = FALSE;
        while (!found && !overlap && mem_tr1) {
            SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Compaction: looking at mem_tr1 0x%p, index=%d\n", mem_tr1, n);
            for (i = n; i < MEM_TRACKER_max_size(mem_tr1); i++) {
                if (!MEM_TRACKER_mem_address(mem_tr1,i)) {
                    SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Compaction: found index %d of %d empty\n",
                                            i,
                                            MEM_TRACKER_max_size(mem_tr1)-1);
                    found = TRUE;
                }
            }
            // check for overlap
            overlap = (mem_tr1==mem_tr2) && (i>=m);

            // if no overlap and an empty node was not found, then advance to next node
            if (!found && !overlap) {
                mem_tr1 = MEM_TRACKER_next(mem_tr1);
                n = 0;
            }
        }
        // all nodes going in forward direction are full, so exit
        if (!found || overlap) {
            goto finish_compact;
        }

        // find a non-empty node
        found = FALSE;
        while (!found && !overlap && mem_tr2) {
            SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Compaction: looking at mem_tr2 0x%p, index=%d\n", mem_tr2, m);
            for (j = m; j >= 0; j--) {
                if (MEM_TRACKER_mem_address(mem_tr2,j)) {
                    SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Compaction: found index %d of %d non-empty\n",
                                            j,
                                            MEM_TRACKER_max_size(mem_tr2)-1);
                    found = TRUE;
                }
            }
            // check for overlap
            overlap = (mem_tr1==mem_tr2) && (j<=i);

            // if no overlap and no non-empty node was found, then retreat to prev node
            if (!found && !overlap) {
                MEM_TRACKER empty_tr = mem_tr2;  // keep track of empty node
                mem_tr2 = MEM_TRACKER_prev(mem_tr2);
                m = MEM_TRACKER_max_size(mem_tr2) - 1;
                mem_tr_tail = mem_tr2; // keep track of new tail
                // reclaim empty mem_tracker node
                control_Memory_Tracker_Delete_Node(empty_tr);
                // keep track of number of node deletions performed
                d++;
            }
        }
        // all nodes going in reverse direction are empty, so exit
        if (!found || overlap) {
            goto finish_compact;
        }

        // swap empty node with non-empty node so that "holes" get bubbled towards the end of list
        MEM_TRACKER_mem_address(mem_tr1,i) = MEM_TRACKER_mem_address(mem_tr2,j);
        MEM_TRACKER_mem_size(mem_tr1,i)    = MEM_TRACKER_mem_size(mem_tr2,j);
        MEM_TRACKER_mem_vmalloc(mem_tr1,i) = MEM_TRACKER_mem_vmalloc(mem_tr2,j);

        MEM_TRACKER_mem_address(mem_tr2,j) = NULL;
        MEM_TRACKER_mem_size(mem_tr2,j)    = 0;
        MEM_TRACKER_mem_vmalloc(mem_tr2,j) = FALSE;

        // keep track of number of memory compactions performed
        c++;

        // start new search starting from next element in mem_tr1
        n = i+1;

        // start new search starting from prev element in mem_tr2
        m = j-1;
    }

finish_compact:
    spin_unlock(&mem_tr_lock);

    SOCPERF_PRINT_DEBUG("CONTROL_Memory_Tracker_Compaction: number of elements compacted = %d, nodes deleted = %d\n", c, d);

    return;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn PVOID CONTROL_Allocate_Memory(size)
 *
 * @param    IN size     - size of the memory to allocate
 *
 * @returns  char*       - pointer to the allocated memory block
 *
 * @brief    Allocate and zero memory
 *
 * <I>Special Notes:</I>
 *           Allocate memory in the GFP_KERNEL pool.
 *
 *           Use this if memory is to be allocated within a context where
 *           the allocator can block the allocation (e.g., by putting
 *           the caller to sleep) while it tries to free up memory to
 *           satisfy the request.  Otherwise, if the allocation must
 *           occur atomically (e.g., caller cannot sleep), then use
 *           CONTROL_Allocate_KMemory instead.
 */
extern PVOID
CONTROL_Allocate_Memory (
    size_t size
)
{
    U32   status;
    PVOID location;

    if (size <= 0) {
        return NULL;
    }

    // determine whether to use mem_tracker or not
    if (size < MAX_KMALLOC_SIZE) {
        location = (PVOID)kmalloc(size, GFP_KERNEL);
        SOCPERF_PRINT_DEBUG("CONTROL_Allocate_Memory: allocated small memory (0x%p, %d)\n", location, (S32) size);
    }
    else {
        location = (PVOID)vmalloc(size);
        if (location) {
            status = control_Memory_Tracker_Add(location, size, TRUE);
            SOCPERF_PRINT_DEBUG("CONTROL_Allocate_Memory: - allocated *large* memory (0x%p, %d)\n", location, (S32) size);
            if (status != OS_SUCCESS) {
                // failed to track in mem_tracker, so free up memory and return NULL
                vfree(location);
                SOCPERF_PRINT_ERROR("CONTROL_Allocate_Memory: - able to allocate, but failed to track via MEM_TRACKER ... freeing\n");
                return NULL;
            }
        }
    }

    if (!location) {
        SOCPERF_PRINT_ERROR("CONTROL_Allocate_Memory: failed for size %d bytes\n", (S32) size);
        return NULL;
    }

    memset(location, 0, size);

    return location;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn PVOID CONTROL_Allocate_KMemory(size)
 *
 * @param    IN size     - size of the memory to allocate
 *
 * @returns  char*       - pointer to the allocated memory block
 *
 * @brief    Allocate and zero memory
 *
 * <I>Special Notes:</I>
 *           Allocate memory in the GFP_ATOMIC pool.
 *
 *           Use this if memory is to be allocated within a context where
 *           the allocator cannot block the allocation (e.g., by putting
 *           the caller to sleep) as it tries to free up memory to
 *           satisfy the request.  Examples include interrupt handlers,
 *           process context code holding locks, etc.
 */
extern PVOID
CONTROL_Allocate_KMemory (
    size_t size
)
{
    U32   status;
    PVOID location;

    if (size <= 0) {
        return NULL;
    }

    if (size < MAX_KMALLOC_SIZE) {
        location = (PVOID)kmalloc(size, GFP_ATOMIC);
        SOCPERF_PRINT_DEBUG("CONTROL_Allocate_KMemory: allocated small memory (0x%p, %d)\n", location, (S32) size);
    }
    else {
        location = (PVOID)__get_free_pages(GFP_ATOMIC, get_order(size));
        status = control_Memory_Tracker_Add(location, size, FALSE);
        SOCPERF_PRINT_DEBUG("CONTROL_Allocate_KMemory: allocated large memory (0x%p, %d)\n", location, (S32) size);
        if (status != OS_SUCCESS) {
            // failed to track in mem_tracker, so free up memory and return NULL
            free_pages((unsigned long)location, get_order(size));
            SOCPERF_PRINT_ERROR("CONTROL_Allocate_KMemory: - able to allocate, but failed to track via MEM_TRACKER ... freeing\n");
            return NULL;
        }
    }

    if (!location) {
        SOCPERF_PRINT_ERROR("CONTROL_Allocate_KMemory: failed for size %d bytes\n", (S32) size);
        return NULL;
    }

    memset(location, 0, size);

    return location;
}

/* ------------------------------------------------------------------------- */
/*
 * @fn PVOID CONTROL_Free_Memory(location)
 *
 * @param    IN location  - size of the memory to allocate
 *
 * @returns  pointer to the allocated memory block
 *
 * @brief    Frees the memory block
 *
 * <I>Special Notes:</I>
 *           Does not try to free memory if fed with a NULL pointer
 *           Expected usage:
 *               ptr = CONTROL_Free_Memory(ptr);
 *           Does not do compaction ... can have "holes" in
 *           mem_tracker list after this operation.
 */
extern PVOID
CONTROL_Free_Memory (
    PVOID  location
)
{
    S32         i;
    DRV_BOOL    found;
    MEM_TRACKER mem_tr;

    if (!location) {
        return NULL;
    }

    spin_lock(&mem_tr_lock);

    // scan through mem_tracker nodes for matching entry (if any)
    mem_tr = mem_tr_head;
    found = FALSE;
    while (mem_tr) {
        for (i = 0; i < MEM_TRACKER_max_size(mem_tr); i++) {
            if (location == MEM_TRACKER_mem_address(mem_tr,i)) {
                SOCPERF_PRINT_DEBUG("CONTROL_Free_Memory: freeing large memory location 0x%p\n", location);
                found = TRUE;
                if (MEM_TRACKER_mem_vmalloc(mem_tr, i)) {
                    vfree(location);
                }
                else {
                    free_pages((unsigned long)location, get_order(MEM_TRACKER_mem_size(mem_tr,i)));
                }
                MEM_TRACKER_mem_address(mem_tr,i) = NULL;
                MEM_TRACKER_mem_size(mem_tr,i)    = 0;
                MEM_TRACKER_mem_vmalloc(mem_tr,i) = FALSE;
                goto finish_free;
            }
        }
        mem_tr = MEM_TRACKER_next(mem_tr);
    }

finish_free:
    spin_unlock(&mem_tr_lock);

    // must have been of smaller than the size limit for mem tracker nodes
    if (!found) {
        SOCPERF_PRINT_DEBUG("CONTROL_Free_Memory: freeing small memory location 0x%p\n", location);
        kfree(location);
    }

    return NULL;
}
