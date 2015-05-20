/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2013 Intel Corporation. All rights reserved.

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

  Contact Information:
  SOCWatch Developer Team <socwatchdevelopers@intel.com>

  BSD LICENSE 

  Copyright(c) 2013 Intel Corporation. All rights reserved.
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

/*
 * Description: file containing memory management routines
 * used by the power driver.
 */

#ifndef _PW_MEM_H_
#define _PW_MEM_H_ 1

#include <linux/slab.h>
#include <linux/list.h>

#include "pw_lock_defs.h"

/*
 * How do we behave if we ever
 * get an allocation error? 
 * (a) Setting to '1' REFUSES ANY FURTHER 
 * allocation requests. 
 * (b) Setting to '0' treats each
 * allocation request as separate, and
 * handles them on an on-demand basis
 */
#define DO_MEM_PANIC_ON_ALLOC_ERROR 0

#if DO_MEM_PANIC_ON_ALLOC_ERROR
/*
 * If we ever run into memory allocation errors then
 * stop (and drop) everything.
 */
static atomic_t pw_mem_should_panic = ATOMIC_INIT(0);
/*
 * Macro to check if PANIC is on.
 */
#define MEM_PANIC() do{ atomic_set(&pw_mem_should_panic, 1); smp_mb(); }while(0)
#define SHOULD_TRACE() ({bool __tmp = false; smp_mb(); __tmp = (atomic_read(&pw_mem_should_panic) == 0); __tmp;})

#else // if !DO_MEM_PANIC_ON_ALLOC_ERROR

#define MEM_PANIC()
#define SHOULD_TRACE() (true)

#endif


/*
 * Toggle memory debugging.
 * In memory debugging mode we track
 * memory usage statistics.
 */
#define DO_MEM_DEBUGGING 0

#if DO_MEM_DEBUGGING
/*
 * Variables to track memory usage.
 */
/*
 * TOTAL num bytes allocated.
 */
static u64 total_num_bytes_alloced = 0;
/*
 * Num of allocated bytes that have
 * not yet been freed.
 */
static u64 curr_num_bytes_alloced = 0;
/*
 * Max # of allocated bytes that
 * have not been freed at any point
 * in time.
 */
static u64 max_num_bytes_alloced = 0;
/*
 * Lock to guard access to memory
 * debugging stats.
 */
static DEFINE_SPINLOCK(pw_kmalloc_lock);

/*
 * Helper macros to print out
 * mem debugging stats.
 */
#define TOTAL_NUM_BYTES_ALLOCED() total_num_bytes_alloced
#define CURR_NUM_BYTES_ALLOCED() curr_num_bytes_alloced
#define MAX_NUM_BYTES_ALLOCED() max_num_bytes_alloced

/*
 * MAGIC number based memory tracker. Relies on
 * storing (a) a MAGIC marker and (b) the requested
 * size WITHIN the allocated block of memory. Standard
 * malloc-tracking stuff, really.
 *
 * Overview:
 * (1) ALLOCATION:
 * When asked to allocate a block of 'X' bytes, allocate
 * 'X' + 8 bytes. Then, in the FIRST 4 bytes, write the
 * requested size. In the NEXT 4 bytes, write a special
 * (i.e. MAGIC) number to let our deallocator know that
 * this block of memory was allocated using this technique.
 * Also, keep track of the number of bytes allocated.
 *
 * (2) DEALLOCATION:
 * When given an object to deallocate, we first check
 * the MAGIC number by decrementing the pointer by 
 * 4 bytes and reading the (integer) stored there.
 * After ensuring the pointer was, in fact, allocated
 * by us, we then read the size of the allocated
 * block (again, by decrementing the pointer by 4
 * bytes and reading the integer size). We 
 * use this size argument to decrement # of bytes
 * allocated.
 */
#define PW_MEM_MAGIC 0xdeadbeef

#define PW_ADD_MAGIC(x) ({char *__tmp1 = (char *)(x); *((int *)__tmp1) = PW_MEM_MAGIC; __tmp1 += sizeof(int); __tmp1;})
#define PW_ADD_SIZE(x,s) ({char *__tmp1 = (char *)(x); *((int *)__tmp1) = (s); __tmp1 += sizeof(int); __tmp1;})
#define PW_ADD_STAMP(x,s) PW_ADD_MAGIC(PW_ADD_SIZE((x), (s)))

#define PW_IS_MAGIC(x) ({int *__tmp1 = (int *)((char *)(x) - sizeof(int)); *__tmp1 == PW_MEM_MAGIC;})
#define PW_REMOVE_STAMP(x) ({char *__tmp1 = (char *)(x); __tmp1 -= sizeof(int) * 2; __tmp1;})
#define PW_GET_SIZE(x) (*((int *)(x)))

static __always_inline void *pw_kmalloc(size_t size, gfp_t flags)
{
	size_t act_size = 0;
	void *retVal = NULL;
	/*
	 * No point in allocating if
	 * we were unable to allocate
	 * previously!
	 */
	{
		if(!SHOULD_TRACE()){
			return NULL;
		}
	}
	/*
	 * (1) Allocate requested block.
	 */
	act_size = size + sizeof(int) * 2;
	retVal = kmalloc(act_size, flags);
	if(!retVal){
		/*
		 * Panic if we couldn't allocate
		 * requested memory.
		 */
		printk(KERN_INFO "ERROR: could NOT allocate memory!\n");
		MEM_PANIC();
		return NULL;
	}
	/*
	 * (2) Update memory usage stats.
	 */
	LOCK(pw_kmalloc_lock);
	{
		total_num_bytes_alloced += size;
		curr_num_bytes_alloced += size;
		if(curr_num_bytes_alloced > max_num_bytes_alloced)
			max_num_bytes_alloced = curr_num_bytes_alloced;
	}
	UNLOCK(pw_kmalloc_lock);
	/*
	 * Debugging ONLY.
	 */
	if(false && total_num_bytes_alloced > 3200000){
		MEM_PANIC();
		smp_mb();
		printk(KERN_INFO "DEBUG: Total # bytes = %llu, SET PANIC FLAG!\n", total_num_bytes_alloced);
	}
	/*
	 * (3) And finally, add the 'size'
	 * and 'magic' stamps.
	 */
	return PW_ADD_STAMP(retVal, size);
};

static __always_inline char *pw_kstrdup(const char *str, gfp_t flags)
{
	char *ret = NULL;
	size_t ret_size = strlen(str), str_size = ret_size + 1;

	/*
	 * No point in allocating if
	 * we were unable to allocate
	 * previously!
	 */
	if(!SHOULD_TRACE() || !str){
		return NULL;
	}

	/*
	 * (1) Use 'pw_kmalloc(...)' to allocate a
	 * block of memory for our string.
	 */
	if( (ret = pw_kmalloc(str_size, flags)) == NULL){
		/*
		 * No need to PANIC -- 'pw_kmalloc(...)'
		 * would have done that already.
		 */
		return NULL;
	}

	/*
	 * (2) Copy string contents into
	 * newly allocated block.
	 */
	memcpy(ret, str, ret_size);
	ret[ret_size] = '\0';

	return ret;

};

static void pw_kfree(const void *obj)
{
	void *tmp = NULL;
	size_t size = 0;

	/*
	 * (1) Check if this block was allocated
	 * by us.
	 */
	if(!PW_IS_MAGIC(obj)){
		printk(KERN_INFO "ERROR: %p is NOT a PW_MAGIC ptr!\n", obj);
		return;
	}
	/*
	 * (2) Strip the magic num...
	 */
	tmp = PW_REMOVE_STAMP(obj);
	/*
	 * ...and retrieve size of block.
	 */
	size = PW_GET_SIZE(tmp);
	/*
	 * (3) Update memory usage stats.
	 */
	LOCK(pw_kmalloc_lock);
	{
		curr_num_bytes_alloced -= size;
	}
	UNLOCK(pw_kmalloc_lock);
	/*
	 * And finally, free the block.
	 */
	kfree(tmp);
};

#else // !DO_MEM_DEBUGGING

/*
 * Helper macros to print out
 * mem debugging stats.
 */
#define TOTAL_NUM_BYTES_ALLOCED() (u64)0
#define CURR_NUM_BYTES_ALLOCED() (u64)0
#define MAX_NUM_BYTES_ALLOCED() (u64)0

static __always_inline void *pw_kmalloc(size_t size, int flags)
{
	void *ret = NULL;

	if(SHOULD_TRACE()){
		if(!(ret = kmalloc(size, flags))){
			/*
			 * Panic if we couldn't allocate
			 * requested memory.
			 */
			MEM_PANIC();
		}
	}
	return ret;
};

static __always_inline char *pw_kstrdup(const char *str, int flags)
{
	char *ret = NULL;

	if(SHOULD_TRACE()){
		if(!(ret = kstrdup(str, flags))){
			/*
			 * Panic if we couldn't allocate
			 * requested memory.
			 */
			MEM_PANIC();
		}
	}

	return ret;
};

static __always_inline void pw_kfree(void *mem)
{
	kfree(mem);
};

#endif // DO_MEM_DEBUGGING


#endif // _PW_MEM_H_
