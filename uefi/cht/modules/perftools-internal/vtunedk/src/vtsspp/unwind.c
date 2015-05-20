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
/**
//
// Stack Unwinding and Related Support Functions
//
*/
#include "vtss_config.h"
#include "unwind.h"

/**
// Stack unwinding functions
*/

/// initialize the stack control object
int vtss_init_stack(stack_control_t * stk)
{
    memset(stk, 0, sizeof(stack_control_t));

    spin_lock_init(&stk->spin_lock);
    stk->init     = vtss_init_stack;
    stk->realloc  = realloc_stack;
    stk->destroy  = destroy_stack;
    stk->clear    = clear_stack;
    stk->unwind   = unwind_stack_fwd;
    stk->validate = validate_stack;
    stk->augment  = augment_stack;
    stk->is_full  = is_full_stack;
    stk->compress = compress_stack;
    stk->setup    = setup_stack;
    stk->data     = data_stack;
    stk->lock     = lock_stack;
    stk->trylock  = trylock_stack;
    stk->unlock   = unlock_stack;
    stk->kernel_callchain_size =VTSS_DYNSIZE_STACKS;
    stk->acc = vtss_user_vm_accessor_init(1, vtss_time_limit);
    return realloc_stack(stk);
}

/// grow the stack map
static int realloc_stack(stack_control_t* stk)
{
    int order;
    unsigned int len = VTSS_DYNSIZE_STACKS;
    char* buf;

    if(stk->buffer)
    {
        len = stk->size;
    }
    if(len > 16*VTSS_DYNSIZE_STACKS)
    {
        TRACE("limit of allocation stack buffer");
        return VTSS_ERR_NOMEMORY;
    }
    order = get_order(len + len);
    if(!(buf = (char*)__get_free_pages((GFP_NOWAIT | __GFP_NORETRY | __GFP_NOWARN), order)))
    {
        return VTSS_ERR_NOMEMORY;
    }
    if(stk->buffer)
    {
        free_pages((unsigned long)stk->buffer, get_order(stk->size));
    }
    stk->buffer = buf;
    stk->size = (PAGE_SIZE << order);
    stk->compressed = stk->buffer + stk->size / 2;
    stk->stkmap_end = stk->stkmap_start = stk->stkmap_common = (stkmap_t*)stk->compressed;
    return 0;
}

/// destroy the stack control object
static void destroy_stack(stack_control_t* stk)
{
    trylock_stack(stk);
    if (stk->acc)
    {
        vtss_user_vm_accessor_fini(stk->acc);
        stk->acc = NULL;
    }
    stk->acc = NULL;
    unlock_stack(stk);
    if(stk->buffer)
    {
        free_pages((unsigned long)stk->buffer, get_order(stk->size));
        stk->buffer = NULL;
        stk->size = 0;
    }
}

/// clear stack map
static void clear_stack(stack_control_t* stk)
{
    stk->compressed = stk->buffer + stk->size / 2;
    stk->stkmap_end = stk->stkmap_start = stk->stkmap_common = (stkmap_t*)stk->compressed;
}

/// build an incremental map of stack, frame, and instruction pointers

///#define SWAP(a, b) (a) += (b); (b) = (a) - (b); (a) = (a) - (b)
///#define SWAP(a, b) {size_t c; c = (a); (a) = (b); (b) = c;}

/// walk from SP up to stack base
static int unwind_stack_fwd(stack_control_t* stk)
{
    stkptr_t value;
    stkptr_t search_sp;
    stkptr_t search_border;

    char* sp = stk->user_sp.chp;
    char* bp = stk->bp.chp;

    int find_changed_region = 1;

    stkmap_t* stkmap_curr;
    stkmap_t* stkmap_end = stk->stkmap_end;
    stkmap_t* stkmap_start = stk->stkmap_start;
    stkmap_t* stkmap_common = stk->stkmap_common;

    int stkmap_size = stk->size / 2;
    int stkmap_commsize = 0;

    int wow64 = stk->wow64;
    int stride = wow64 ? 4 : sizeof(void*);

    size_t tmp = stk->user_sp.szt;
    size_t val_idx = VTSS_STACK_CACHE_SIZE;
    char* vals = stk->value_cache;
    size_t val_size = 0;

    /* check for bad args */

    if(!stk->buffer)
    {
        return VTSS_ERR_NOMEMORY;
    }

    /* do argument corrections */

    /// align sp to machine word size
    if(!wow64)
    {
        sp = (char*)(tmp + ((sizeof(void*) - (tmp & (sizeof(void*) - 1))) & ((!(tmp & (sizeof(void*) - 1))) - 1)));
    }
    else
    {
        sp = (char*)(tmp + ((sizeof(int) - (tmp & (sizeof(int) - 1))) & ((!(tmp & (sizeof(int) - 1))) - 1)));
    }
    /// correct initial stack map pointers
    if((unsigned char*)stkmap_start == stk->compressed)
    {
        stk->compressed = stk->buffer;
        stkmap_common = stkmap_end = stkmap_start = stk->stkmap_start = (stkmap_t*)(stk->buffer + stkmap_size);
    }

    /// clear the border between changed/unchanged stack map regions
    stkmap_common = 0;

    /* check if the stack and stack map intersect */
    search_border.chp = 0;
    if(stkmap_start != stkmap_end)  /// the map is not empty
    {
        if((stkmap_end - 1)->sp.chp >= bp || (stkmap_end - 1)->sp.chp < sp)
        {
            /// the stack map is beyond the actual stack, clear it
            stkmap_common = stkmap_end = stkmap_start;
            /// nothing to find
            find_changed_region = 0;
            /// search the entire stack, the map is emptied
            search_border.chp = bp - stride;
        }
    }
    else    /// empty stack
    {
        /// clear the stack map
        stkmap_common = stkmap_end = stkmap_start;
        /// nothing to find
        find_changed_region = 0;
        /// search the entire stack, the map is emptied
        search_border.chp = bp - stride;
    }

    /* search the stack and the stack map to detect the changed/unchanged regions */
    value.szt = 0;
    if(find_changed_region)
    {
        /// TODO: enable the try-except block in case of numerous 'empty' stack records
        ///__try
        ///{
            for(stkmap_curr = stkmap_start; stkmap_curr < stkmap_end; stkmap_curr++)
            {
                /// check if within the actual stack
                if(stkmap_curr->sp.chp < sp)
                {
                    /// remember the changed border
                    stkmap_common = stkmap_curr;
                    continue;
                }
                /// read in the actual stack contents
                if (stk->acc->read(stk->acc, stkmap_curr->sp.szp, &vals[0], stride) != stride) {
                    TRACE("SP=0x%p: break search, [0x%p - 0x%p], ip=0x%p", stkmap_curr->sp.szp, stk->user_sp.vdp, stk->bp.vdp, stk->user_ip.vdp);
                    /// clear the stack map
                    stkmap_common = stkmap_end = stkmap_start;
                    /// search the entire stack, the map is emptied
                    search_border.chp = bp - stride;
                    goto end_of_search;
                }
                value.szt = 0;
                value.szt = wow64 ? *(u32*)(&vals[0]) : *(u64*)(&vals[0]);
                //value.szt = (size_t)(wow64 ? *stkmap_curr->sp.uip : *stkmap_curr->sp.szp);
                /// check if the current element has changed
                if(stkmap_curr->value.szt != value.szt)
                {
                    /// remember the changed border
                    stkmap_common = stkmap_curr;
                }
                else
                {
                    /// check the stack above the last map element
                    if(stkmap_curr + 1 == stkmap_end)
                    {
                        /* Not required for Linux */
#if 0
                        for(search_sp.chp = stkmap_curr->sp.chp + stride; search_sp.chp < bp; search_sp.chp += stride)
                        {
                            value.szt = 0; // = (size_t)(wow64 ? *search_sp.uip : *search_sp.szp);
                            if (stk->acc->read(stk->acc, search_sp.szp, &value.szt, stride) != stride) {
                                TRACE("SP=0x%p: break search, [0x%p - 0x%p], ip=0x%p", search_sp.szp, stk->sp.vdp, stk->bp.vdp, stk->ip.vdp);
                                /// clear the stack map
                                stkmap_common = stkmap_end = stkmap_start;
                                /// search the entire stack, the map is emptied
                                search_border.chp = bp - stride;
                                goto end_of_search;
                            }

                            if(value.chp < sp || value.chp >= bp)
                            {
                                if (stk->acc->validate(stk->acc, (unsigned long)value.szt))
                                {
                                    /// remember the changed border
                                    stkmap_common = stkmap_curr;
                                    break;
                                }
                            }
                        }
#endif
                    }
                    else
                    {
                        if(value.chp < sp || value.chp >= bp)
                        {
                            /// do extra IP search above the current IP element
                            if(stkmap_curr + 1 < stkmap_end)
                            {
                                search_border.chp = min((stkmap_curr + 1)->sp.chp, stkmap_curr->sp.chp + IP_SEARCH_RANGE * stride);
                            }
                            else
                            {
                                search_border.chp = min(bp, stkmap_curr->sp.chp + IP_SEARCH_RANGE * stride);
                            }
                            //stk->value = value;
                            /// search for IPs from the same module
                            val_size = 0;
                            for(search_sp.chp = stkmap_curr->sp.chp + stride; search_sp.chp < search_border.chp; search_sp.chp += stride, val_idx += stride)
                            {
                                value.szt = 0;
                                if (val_idx >= val_size)
                                {
                                val_idx = 0;
                                val_size = min(PAGE_SIZE-(unsigned long)(search_sp.szt&(~PAGE_MASK)), (unsigned long)(search_border.chp-search_sp.chp+stride));
                                val_size = min((size_t)IP_SEARCH_RANGE * stride, val_size);
                                if (stk->acc->read(stk->acc, search_sp.szp, &vals[val_idx], val_size) != val_size) {
                                    TRACE("SP=0x%p: break search, [0x%p - 0x%p], ip=0x%p", search_sp.szp, stk->user_sp.vdp, stk->bp.vdp, stk->user_ip.vdp);
                                    //printk("failed to read 0!!! SP=0x%p: break search, [0x%p - 0x%p], ip=0x%p\n", search_sp.szp, stk->user_sp.vdp, stk->bp.vdp, stk->user_ip.vdp);
                                    /// clear the stack map
                                    stkmap_common = stkmap_end = stkmap_start;
                                    /// search the entire stack, the map is emptied
                                    search_border.chp = bp - stride;
                                    val_idx = VTSS_STACK_CACHE_SIZE;
                                    val_size = 0;
                                    goto end_of_search;
                                }
                                }
                                value.szt = wow64 ? *(u32*)(&vals[val_idx]) : *(u64*)(&vals[val_idx]);
//                                value.szt = ((stkptr_t*)&vals[val_idx])->szt;
                                //stk->value.szt = value.szt = (size_t)(wow64 ? *search_sp.uip : *search_sp.szp);
                                /// this is a relaxed IP search condition (to increase the performance)
                                if(stk->acc->validate(stk->acc, (unsigned long)value.szt))
                                {
                                    /// remember the changed border
                                    stkmap_common = stkmap_curr;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            if(stkmap_common)   /// found the chnaged/unchanged border
            {
                /// use the map element following the changed one
                stkmap_common++;
            }
            else                /// unchanged stack
            {
                stkmap_common = stkmap_start;
            }
            /// search below the unchanged region
            search_border.chp = stkmap_common == stkmap_end ? bp - stride : stkmap_common->sp.chp - stride;
            /// switch to a new stack map
            {
                void* p = (void*)stkmap_start;
                stkmap_start = (stkmap_t*)stk->compressed;
                stk->compressed = (unsigned char*)p;
            }
        ///}
        ///__except(EXCEPTION_EXECUTE_HANDLER)
        ///{
        ///    /// clear the stack map
        ///    stkmap_common = stkmap_end = stkmap_start;
        ///    /// search the entire stack, the map is emptied
        ///    search_border.chp = bp - stride;
        ///}
    }
end_of_search:

    /// compute the size of the unchanged stack map region
    stkmap_commsize = (int)((stkmap_end - stkmap_common) * sizeof(stkmap_t));
    /// correct the free size of stack map
    stkmap_size -= stkmap_commsize;
    val_idx = VTSS_STACK_CACHE_SIZE;

    /* search the stack for IPs and FPs and update the stack map */
    val_size = 0;
    for(search_sp.chp = sp, stkmap_curr = stkmap_start; search_sp.chp <= search_border.chp; search_sp.chp += stride, val_idx +=stride)
    {
//    static int vtss_debug = 0;
        if((char*)stkmap_curr - (char*)stkmap_start >= stkmap_size)
        {
            /// the map is full
            return VTSS_ERR_NOMEMORY;
        }
        if (val_idx >= val_size)
        {
            /// read a value from the stack
            val_size = min(PAGE_SIZE-(unsigned long)(search_sp.szt&(~PAGE_MASK)), (unsigned long)(search_border.chp-search_sp.chp+stride));
            val_size = min((size_t)VTSS_STACK_CACHE_SIZE, val_size);
//            size_t val_size = min((unsigned long)VTSS_STACK_CACHE_SIZE, (unsigned long)(search_border.chp-search_sp.chp));
            val_idx = 0;
            value.szt = 0;
            if (stk->acc->read(stk->acc, search_sp.szp, &vals[val_idx], val_size) != val_size) {
                TRACE("SP=0x%p: skip page, [0x%p - 0x%p], ip=0x%p", search_sp.szp, stk->user_sp.vdp, stk->bp.vdp, stk->user_ip.vdp);
//                printk("read failed!!! SP=0x%p: skip page, [0x%p - 0x%p], ip=0x%p\n", search_sp.szp, stk->user_sp.vdp, stk->bp.vdp, stk->user_ip.vdp);
#ifdef VTSS_MEM_FAULT_BREAK
                break;
#else
                if ((search_border.szt - (search_sp.szt & PAGE_MASK)) > PAGE_SIZE) {
                    search_sp.chp += PAGE_SIZE;
                    search_sp.szt &= PAGE_MASK;
                    search_sp.chp -= stride;
                    val_idx = VTSS_STACK_CACHE_SIZE;
                    val_size = 0;
                    continue;
                } else {
                    break;
                }
#endif
            }
        }
/*        if (vtss_debug <=10){
            stkptr_t vvv;
            value.szt = wow64 ? *(u32*)(&vals[val_idx]) : *(u64*)(&vals[val_idx]);
            if (stk->acc->read(stk->acc, search_sp.szp, &vvv.szt, stride) != stride) {
                printk("failed to read \n");
            } else {
                printk("sizeof(size_t)=%d, vals[%d].szt=%lx, vals[%d].chp=%p, vals[%d].uip=%p, str = (%#2x %#2x %#2x %#2x %#2x %#2x %#2x %#2x), vvv = [%lx, %p], value = [%lx, %p]\n",
                 (int)sizeof(size_t), (int)val_idx, ((stkptr_t*)&vals[val_idx])->szt, (int)val_idx, ((stkptr_t*)&vals[val_idx])->chp, (int)val_idx,((stkptr_t*)&vals[val_idx])->uip,vals[val_idx],vals[val_idx+1],vals[val_idx+2],vals[val_idx+3],
                 vals[val_idx+4], vals[val_idx+5],vals[val_idx+6],vals[val_idx+7],vvv.szt, vvv.chp, value.szt, value.chp);
            }
            vtss_debug++;
        }*/

            value.szt = wow64 ? *(u32*)(&vals[val_idx]) : *(u64*)(&vals[val_idx]);
            /// validate the value
            if(value.chp < search_sp.chp || value.chp >= bp)
            {
                /// it's not FP, check for IP
                if( !stk->acc->validate(stk->acc, (unsigned long)value.szt) )
                {
                    /// not IP
                    continue;
                }
            }
            else
            {
                /// validate FP
                if(value.szt & (wow64 ? 3 : sizeof(void*) - 1))
                {
                    continue;
                }
            }
            /// it's either FP or IP, store it  
            stkmap_curr->sp = search_sp;
            stkmap_curr->value = value;
            stkmap_curr++;
    }
    if(stkmap_common != stkmap_end)
    {
        /// merge in the unchanged stack map region
        memcpy(stkmap_curr, stkmap_common, stkmap_commsize);
        /// include one common element into the current stack increment
        stkmap_curr++;
        stkmap_commsize -= sizeof(stkmap_t);
    }
    /// store the map parameters
    stk->stkmap_start = stkmap_start;
    stk->stkmap_end = (stkmap_t*)((char*)stkmap_curr + stkmap_commsize);
    stk->stkmap_common = stkmap_curr;
    return 0;
}

/// validate a return address (callback)
static int validate_stack(stack_control_t* stk)
{
    int rc = 0;
    VTSS_PROFILE(vld, rc = stk->acc->validate(stk->acc, (unsigned long)stk->value.szt));
    return rc;
}

/// ensure a full stack map is generated
static void augment_stack(stack_control_t* stk)
{
    stk->stkmap_common = stk->stkmap_end;
}

/// check if the stack map increment is zero
static int is_full_stack(stack_control_t* stk)
{
    return stk->stkmap_common == stk->stkmap_end;
}

/// compress the collected stack map
static int compress_stack(stack_control_t* stk)
{
    size_t* map;
    int count;
    size_t ip;
    size_t sp;
    size_t fp;
    unsigned char* compressed;
    int i, j;
    int prefix;
    size_t value;
    size_t base;
    size_t offset;
    int sign;
    size_t tmp;
    size_t stksize = stk->size / 2;

    compressed = stk->compressed;

    base = stk->bp.szt;
    ip = stk->user_ip.szt;
    sp = stk->user_sp.szt;
    fp = stk->user_fp.szt;

    map = (size_t*)stk->stkmap_start;
    count = (int)(stk->stkmap_common - stk->stkmap_start) * 2;

    /// correct sp
    if(!stk->wow64)
    {
        sp = sp + ((sizeof(void*) - (sp & (sizeof(void*) - 1))) & ((!(sp & (sizeof(void*) - 1))) - 1));
    }
    else
    {
        sp = sp + ((sizeof(int) - (sp & (sizeof(int) - 1))) & ((!(sp & (sizeof(int) - 1))) - 1));
    }

    for(i = 0; i < count; i++)
    {
        /// check the border
        if((size_t)(compressed - stk->compressed) >= stksize)
        {
            return 0;
        }

        /// [[prefix][stack pointer][prefix][value]]...
        ///  [prefix bits: |7: frame(1)/code(0)|6: sign|5:|4:|3-0: bytes per value]
        ///  [prefix bits for sp: |7: (1) - idicates value is encoded in prefix|6-0: scaled sp value]
        ///                           (0) - |6:|5:|4:|3-0: bytes per value]
        ///  [prefix bits for fp: |7: frame(1)/code(0)|6: sign|5: in-prefix (0) - |4:|3-0: bytes per value]
        ///                                                                 (1) - |4-0: scaled fp value]
        ///  [value: difference from previous value of the same type]

        /// compress a stack pointer
        value = map[i] - sp;    /// assert(value >= 0);
        value >>= 2;            /// scale the stack pointer down by 4

        if(value < 0x80)
        {
            prefix = 0x80 | (int)value;
            *compressed++ = (unsigned char)prefix;
        }
        else
        {

            for(j = sizeof(size_t) - 1; j >= 0; j--)
            {
                if(value & (((size_t)0xff) << (j << 3)))
                {
                    break;
                }
            }
            prefix = j + 1;
            *compressed++ = (unsigned char)prefix;

            for(; j >= 0; j--)
            {
                *compressed++ = (unsigned char)(value & 0xff);
                value >>= 8;
            }
        }
        sp = map[i++];

        /// test & compress a value
        value = map[i];

        if(value >= sp && value < base)
        {
            offset = fp;
            fp = value;
            value -= offset;
            tmp = (value & (((size_t)1) << ((sizeof(size_t) << 3) - 1)));
            value = (value >> 2) | tmp | (tmp >> 1);

            if(value < 0x20)
            {
                prefix = 0xa0 | (int)value;
                *compressed++ = (unsigned char)prefix;
                continue;
            }
            else if(value > (size_t)-32)
            {
                prefix = 0xe0 | (int)(value & 0xff);
                *compressed++ = (unsigned char)prefix;
                continue;
            }
            prefix = 0x80;
        }
        else
        {
            offset = ip;
            ip = value;
            prefix = 0;
            value -= offset;
        }
        sign = (value & (((size_t)1) << ((sizeof(size_t) << 3) - 1))) ? 0xff : 0;

        for(j = sizeof(size_t) - 1; j >= 0; j--)
        {
            if(((value >> (j << 3)) & 0xff) != sign)
            {
                break;
            }
        }
        prefix |= sign ? 0x40 : 0;
        prefix |= j + 1;
        *compressed++ = (unsigned char)prefix;

        for(; j >= 0; j--)
        {
            *compressed++ = (unsigned char)(value & 0xff);
            value >>= 8;
        }
    }
    return (int)(compressed - stk->compressed);
}

/// setup current stack parameters
static void setup_stack(stack_control_t* stk, user_vm_accessor_t* acc,
                        void* ip, void* sp, void* bp, void* fp, int wow64)
{
    stk->acc    = acc;
    stk->user_ip.vdp = ip;
    stk->user_sp.vdp = sp;
    stk->bp.vdp = bp;
    stk->user_fp.vdp = fp;
    stk->wow64  = wow64;
}

/// get a pointer to the compressed stack data
static char* data_stack(stack_control_t* stk)
{
    return stk->compressed;
}

static void lock_stack(stack_control_t* stk)
{
    spin_lock(&stk->spin_lock);
}

static int trylock_stack(stack_control_t* stk)
{
    if (!stk->acc) return 0;
    return spin_trylock(&stk->spin_lock);
}

static void unlock_stack(stack_control_t* stk)
{
    spin_unlock(&stk->spin_lock);
}
