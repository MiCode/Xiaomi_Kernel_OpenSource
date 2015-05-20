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


#include <asm/local.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/mm.h> // for "remap_pfn_range"
#include <asm/io.h> // for "virt_to_phys"
#include <asm/uaccess.h> // for "copy_to_user"

#include "pw_structs.h"
#include "pw_output_buffer.h"
#include "pw_defines.h"
#include "pw_mem.h"

/*
 * Global variable definitions.
 */
u64 pw_num_samples_produced = 0, pw_num_samples_dropped = 0;
unsigned long pw_buffer_alloc_size = 0;
int pw_max_num_cpus = -1;
/*
 * The size of the 'buffer' data array in each segment.
 * This is 64kB - 2 * sizeof(u32)
 */
#define PW_SEG_DATA_SIZE 65528
#define PW_SEG_SIZE_BYTES ( PW_SEG_DATA_SIZE + 2 * sizeof(u32) ) /* 64 kB */
#define PW_DATA_BUFFER_SIZE (PW_SEG_SIZE_BYTES)
#define PW_OUTPUT_BUFFER_SIZE (PW_DATA_BUFFER_SIZE * NUM_SEGS_PER_BUFFER)
/*
 * How much space is available in a given segment?
 */
#define SPACE_AVAIL(seg) ( (seg)->is_full ? 0 : (PW_SEG_DATA_SIZE - (seg)->bytes_written) )
#define GET_OUTPUT_BUFFER(cpu) &per_cpu_output_buffers[(cpu)]
/*
 * Convenience macro: iterate over each segment in a per-cpu output buffer.
 */
#define for_each_segment(i) for (i=0; i<NUM_SEGS_PER_BUFFER; ++i)
/*
 * How many buffers are we using?
 */
// #define GET_NUM_OUTPUT_BUFFERS() (pw_max_num_cpus)
#define GET_NUM_OUTPUT_BUFFERS() (pw_max_num_cpus + 1)
/*
 * Convenience macro: iterate over each per-cpu output buffer.
 */
#define for_each_output_buffer(i) for (i=0; i<GET_NUM_OUTPUT_BUFFERS(); ++i)

/*
 * Typedefs and forward declarations.
 */
typedef struct pw_data_buffer pw_data_buffer_t;
typedef struct pw_output_buffer pw_output_buffer_t;

/*
 * Output buffer data structures.
 */
struct pw_data_buffer {
    u32 bytes_written;
    u32 is_full;
    char buffer[1];
};

struct pw_output_buffer {
    pw_data_buffer_t *buffers[NUM_SEGS_PER_BUFFER];
    int buff_index;
    u32 produced_samples;
    u32 dropped_samples;
    int last_seg_read;
    unsigned long free_pages;
    unsigned long mem_alloc_size;
} ____cacheline_aligned_in_smp;

/*
 * Local function declarations.
 */
pw_data_buffer_t *pw_get_next_available_segment_i(pw_output_buffer_t *buffer, int size);

/*
 * Local variable definitions.
 */
/*
 * The alarm queue.
 */
wait_queue_head_t pw_reader_queue;
/*
 * Per-cpu output buffers.
 */
pw_output_buffer_t *per_cpu_output_buffers = NULL;
/*
 * Variables for book keeping.
 */
/*
static DEFINE_PER_CPU(local_t, pw_num_tps) = LOCAL_INIT(0);
static DEFINE_PER_CPU(local_t, pw_num_d_msg) = LOCAL_INIT(0);
*/
volatile unsigned long reader_map = 0;
int pw_last_cpu_read = -1;
s32 pw_last_mask = -1;

/*
 * Function definitions.
 */
pw_data_buffer_t inline *pw_get_next_available_segment_i(pw_output_buffer_t *buffer, int size)
{
    int i=0;
    int buff_index = buffer->buff_index;

    for_each_segment(i) {
        buff_index = CIRCULAR_INC(buff_index, NUM_SEGS_PER_BUFFER_MASK);
        if (SPACE_AVAIL(buffer->buffers[buff_index]) >= size) {
            buffer->buff_index = buff_index;
            return buffer->buffers[buff_index];
        }
    }
    return NULL;
};

static pw_data_buffer_t *get_producer_seg_i(size_t size, int *cpu, u32 *write_index, bool *should_wakeup, bool *did_drop_sample)
{
    pw_data_buffer_t *seg = NULL;
    unsigned long flags = 0;

    local_irq_save(flags);
    // get_cpu();
    {
        pw_output_buffer_t *buffer = GET_OUTPUT_BUFFER(*cpu = CPU());
        int buff_index = buffer->buff_index;
        if (buff_index < 0 || buff_index >= NUM_SEGS_PER_BUFFER) {
            // printk(KERN_INFO "ERROR: cpu = %d, buff_index = %d\n", cpu, buff_index);
            seg = NULL;
            goto prod_seg_done;
        }
        seg = buffer->buffers[buff_index];

        if (unlikely(SPACE_AVAIL(seg) < size)) {
            seg->is_full = 1;
            *should_wakeup = true;
            seg = pw_get_next_available_segment_i(buffer, size);
            // seg = NULL;
            if (seg == NULL) {
                /*
                 * We couldn't find a non-full segment.
                 */
                buffer->dropped_samples++;
                *did_drop_sample = true;
                goto prod_seg_done;
            }
        }
        *write_index = seg->bytes_written;
        seg->bytes_written += size;

        buffer->produced_samples++;
    }
prod_seg_done:
    // put_cpu();
    local_irq_restore(flags);
    return seg;
};

int pw_produce_generic_msg(struct PWCollector_msg *msg, bool allow_wakeup)
{
    int retval = PW_SUCCESS;
    bool should_wakeup = false;
    bool should_print_error = false;
    bool did_drop_sample = false;
    int cpu = -1;
    bool did_switch_buffer = false;
    int size = 0;
    pw_data_buffer_t *seg = NULL;
    char *dst = NULL;
    u32 write_index = 0;

    if (!msg) {
        pw_pr_error("ERROR: CANNOT produce a NULL msg!\n");
        return -PW_ERROR;
    }

    size = msg->data_len + PW_MSG_HEADER_SIZE;

    pw_pr_debug("[%d]: size = %d\n", RAW_CPU(), size);

    seg = get_producer_seg_i(size, &cpu, &write_index, &should_wakeup, &did_drop_sample);

    if (likely(seg)) {
        dst = &seg->buffer[write_index];
        *((PWCollector_msg_t *)dst) = *msg;
        dst += PW_MSG_HEADER_SIZE;
        memcpy(dst, (void *)((unsigned long)msg->p_data), msg->data_len);
    } else {
        pw_pr_warn("WARNING: NULL seg! Msg type = %u\n", msg->data_type);
    }

    if (unlikely(should_wakeup && allow_wakeup && waitqueue_active(&pw_reader_queue))) {
        set_bit(cpu, &reader_map); // we're guaranteed this won't get reordered!
        smp_mb(); // TODO: do we really need this?
        // printk(KERN_INFO "[%d]: has full seg!\n", cpu);
        pw_pr_debug(KERN_INFO "[%d]: has full seg!\n", cpu);
        wake_up_interruptible(&pw_reader_queue);
    }

    if (did_drop_sample) {
        // pw_pr_warn("Dropping sample\n");
    }

    if (should_print_error) {
        // pw_pr_error("ERROR in produce!\n");
    }

    if (did_switch_buffer) {
        // pw_pr_debug("[%d]: switched sub buffers!\n", cpu);
    }

    return retval;
};

int pw_produce_generic_msg_on_cpu(int cpu, struct PWCollector_msg *msg, bool allow_wakeup)
{
    // unsigned long flags = 0;
    const int retval = PW_SUCCESS;
    bool should_wakeup = false;
    bool should_print_error = false;
    bool did_drop_sample = false;
    bool did_switch_buffer = false;
    int size = msg->data_len + PW_MSG_HEADER_SIZE;

    pw_pr_debug("[%d]: cpu = %d, size = %d\n", RAW_CPU(), cpu, size);

    // local_irq_save(flags);
    // get_cpu();
    {
        pw_output_buffer_t *buffer = GET_OUTPUT_BUFFER(cpu);
        int buff_index = buffer->buff_index;
        pw_data_buffer_t *seg = buffer->buffers[buff_index];
        char *dst = NULL;

        if (unlikely(SPACE_AVAIL(seg) < size)) {
            seg->is_full = 1;
            should_wakeup = true;
            seg = pw_get_next_available_segment_i(buffer, size);
            if (seg == NULL) {
                /*
                 * We couldn't find a non-full segment.
                 */
                // retval = -PW_ERROR;
                should_wakeup = true;
                buffer->dropped_samples++;
                did_drop_sample = true;
                goto done;
            }
        }

        dst = &seg->buffer[seg->bytes_written];

        *((PWCollector_msg_t *)dst) = *msg;
        dst += PW_MSG_HEADER_SIZE;
        // pw_pr_debug("Diff = %d\n", (dst - &seg->buffer[seg->bytes_written]));
        memcpy(dst, (void *)((unsigned long)msg->p_data), msg->data_len);

        seg->bytes_written += size;

        buffer->produced_samples++;

        pw_pr_debug(KERN_INFO "OK: [%d] PRODUCED a generic msg!\n", cpu);
    }
done:
    // local_irq_restore(flags);
    // put_cpu();

    if (should_wakeup && allow_wakeup && waitqueue_active(&pw_reader_queue)) {
        set_bit(cpu, &reader_map); // we're guaranteed this won't get reordered!
        smp_mb(); // TODO: do we really need this?
        // printk(KERN_INFO "[%d]: has full seg!\n", cpu);
        pw_pr_debug(KERN_INFO "[%d]: has full seg!\n", cpu);
        wake_up_interruptible(&pw_reader_queue);
    }

    if (did_drop_sample) {
        // pw_pr_warn("Dropping sample\n");
    }

    if (should_print_error) {
        // pw_pr_error("ERROR in produce!\n");
    }

    if (did_switch_buffer) {
        // pw_pr_debug("[%d]: switched sub buffers!\n", cpu);
    }

    return retval;
};

int pw_init_per_cpu_buffers(void)
{
    int cpu = -1;
    unsigned long per_cpu_mem_size = PW_OUTPUT_BUFFER_SIZE;

    // if (pw_max_num_cpus <= 0)
    if (GET_NUM_OUTPUT_BUFFERS() <= 0) {
        // pw_pr_error("ERROR: max # cpus = %d\n", pw_max_num_cpus);
        pw_pr_error("ERROR: max # output buffers= %d\n", GET_NUM_OUTPUT_BUFFERS());
        return -PW_ERROR;
    }

    pw_pr_debug("DEBUG: pw_max_num_cpus = %d, num output buffers = %d\n", pw_max_num_cpus, GET_NUM_OUTPUT_BUFFERS());

    per_cpu_output_buffers = (pw_output_buffer_t *)pw_kmalloc(sizeof(pw_output_buffer_t) * GET_NUM_OUTPUT_BUFFERS(), GFP_KERNEL | __GFP_ZERO);
    if (per_cpu_output_buffers == NULL) {
        pw_pr_error("ERROR allocating space for per-cpu output buffers!\n");
        pw_destroy_per_cpu_buffers();
        return -PW_ERROR;
    }
    // for (cpu=0; cpu<pw_max_num_cpus; ++cpu)
    for_each_output_buffer(cpu) {
        pw_output_buffer_t *buffer = &per_cpu_output_buffers[cpu];
        char *buff = NULL;
        int i=0;
        buffer->mem_alloc_size = per_cpu_mem_size;
        buffer->free_pages = __get_free_pages(GFP_KERNEL | __GFP_ZERO, get_order(per_cpu_mem_size));
        pw_buffer_alloc_size += (1 << get_order(per_cpu_mem_size)) * PAGE_SIZE;
        if (buffer->free_pages == 0) {
            pw_pr_error("ERROR allocating pages for buffer [%d]!\n", cpu);
            pw_destroy_per_cpu_buffers();
            return -PW_ERROR;
        }
        buff = (char *)buffer->free_pages;
        for_each_segment(i) {
            buffer->buffers[i] = (pw_data_buffer_t *)buff;
            buff += PW_DATA_BUFFER_SIZE;
        }
    }

    {
        init_waitqueue_head(&pw_reader_queue);
    }
    return PW_SUCCESS;
};

void pw_destroy_per_cpu_buffers(void)
{
    int cpu = -1;

#if DO_DEBUG_OUTPUT
    if (per_cpu_output_buffers != NULL) {
        /*
         * Testing!
         */
        int cpu = -1, i=0;
        // for_each_possible_cpu(cpu) {
        // for (cpu=0; cpu<pw_max_num_cpus; ++cpu)
        for_each_output_buffer(cpu) {
            pw_pr_debug("CPU: %d: # dropped = %d\n", cpu, per_cpu_output_buffers[cpu].dropped_samples);
            for_each_segment(i) {
                pw_data_buffer_t *buffer = per_cpu_output_buffers[cpu].buffers[i];
                pw_pr_debug("\tBuff [%d]: bytes_written = %u, is_full = %s, buffer = %p\n", i, buffer->bytes_written, GET_BOOL_STRING(buffer->is_full), buffer->buffer);
            }
        }
    }
#endif // DO_DEBUG_OUTPUT
    if (per_cpu_output_buffers != NULL) {
        // for_each_possible_cpu(cpu) {
        // for (cpu=0; cpu<pw_max_num_cpus; ++cpu)
        for_each_output_buffer(cpu) {
            pw_output_buffer_t *buffer = &per_cpu_output_buffers[cpu];
            if (buffer->free_pages != 0) {
                free_pages(buffer->free_pages, get_order(buffer->mem_alloc_size));
                buffer->free_pages = 0;
            }
        }
        pw_kfree(per_cpu_output_buffers);
        per_cpu_output_buffers = NULL;
    }
};

void pw_reset_per_cpu_buffers(void)
{
    int cpu = 0, i = 0;
    // for_each_possible_cpu(cpu) {
    // for (cpu=0; cpu<pw_max_num_cpus; ++cpu)
    for_each_output_buffer(cpu) {
        pw_output_buffer_t *buffer = GET_OUTPUT_BUFFER(cpu);
        buffer->buff_index = buffer->dropped_samples = buffer->produced_samples = 0;
        buffer->last_seg_read = -1;

        for_each_segment(i) {
            memset(buffer->buffers[i], 0, PW_DATA_BUFFER_SIZE);
        }
    }
    pw_last_cpu_read = -1;
    pw_last_mask = -1;
};

int pw_map_per_cpu_buffers(struct vm_area_struct *vma, unsigned long *total_size)
{
    int cpu = -1;
    unsigned long start = vma->vm_start;
    /*
     * We have a number of output buffers. Each (per-cpu) output buffer is one contiguous memory
     * area, but the individual buffers are disjoint from each other. We therefore need to
     * loop over each buffer and map in its memory area.
     */
    // for_each_possible_cpu(cpu) {
    // for (cpu=0; cpu<pw_max_num_cpus; ++cpu)
    for_each_output_buffer(cpu) {
        pw_output_buffer_t *buffer = &per_cpu_output_buffers[cpu];
        unsigned long buff_size = buffer->mem_alloc_size;
        int ret = remap_pfn_range(vma, start, virt_to_phys((void *)buffer->free_pages) >> PAGE_SHIFT, buff_size, vma->vm_page_prot);
        if (ret < 0) {
            return ret;
        }
        *total_size += buff_size;
        start += buff_size;
    }

    return PW_SUCCESS;
};

bool pw_any_seg_full(u32 *val, const bool *is_flush_mode)
{
    int num_visited = 0, i = 0;

    if (!val || !is_flush_mode) {
        pw_pr_error("ERROR: NULL ptrs in pw_any_seg_full!\n");
        return false;
    }

    *val = PW_NO_DATA_AVAIL_MASK;
    pw_pr_debug(KERN_INFO "Checking for full seg: val = %u, flush = %s\n", *val, GET_BOOL_STRING(*is_flush_mode));
    // for_each_online_cpu(num_visited)
    for_each_output_buffer(num_visited) {
        pw_output_buffer_t *buffer = NULL;
        /*
        if (++pw_last_cpu_read >= num_online_cpus()) {
            pw_last_cpu_read = 0;
        }
        */
        if (++pw_last_cpu_read >= GET_NUM_OUTPUT_BUFFERS()) {
            pw_last_cpu_read = 0;
        }
        buffer = GET_OUTPUT_BUFFER(pw_last_cpu_read);
        for_each_segment(i) {
            if (++buffer->last_seg_read >= NUM_SEGS_PER_BUFFER) {
                buffer->last_seg_read = 0;
            }
            if (pw_last_cpu_read == 0) {
                pw_pr_debug(KERN_INFO "Any_seg_Full: cpu = %d, segment = %d, flush-mode = %s, non-empty = %s\n", pw_last_cpu_read, buffer->last_seg_read, GET_BOOL_STRING(*is_flush_mode), GET_BOOL_STRING(buffer->buffers[buffer->last_seg_read]->bytes_written > 0));
            }
            smp_mb();
            if (buffer->buffers[buffer->last_seg_read]->is_full || (*is_flush_mode && buffer->buffers[buffer->last_seg_read]->bytes_written > 0)) {
                *val = (pw_last_cpu_read & 0xffff) << 16 | (buffer->last_seg_read & 0xffff);
                // pw_last_mask = *val;
                return true;
            }
        }
    }
    /*
     * Reaches here only if there's no data to be read.
     */
    if (*is_flush_mode) {
        /*
         * We've drained all buffers and need to tell the userspace application there
         * isn't any data. Unfortunately, we can't just return a 'zero' value for the
         * mask (because that could also indicate that segment # 0 of cpu #0 has data).
         */
        *val = PW_ALL_WRITES_DONE_MASK;
        return true;
    }
    return false;
};

/*
 * Has semantics of 'copy_to_user()' -- returns # of bytes that could NOT be copied
 * (On success ==> returns 0).
 */
unsigned long pw_consume_data(u32 mask, char __user *buffer, size_t bytes_to_read, size_t *bytes_read)
{
    int which_cpu = -1, which_seg = -1;
    unsigned long bytes_not_copied = 0;
    pw_output_buffer_t *buff = NULL;
    pw_data_buffer_t *seg = NULL;

    if (!buffer || !bytes_read) {
        pw_pr_error("ERROR: NULL ptrs in pw_consume_data!\n");
        return -PW_ERROR;
    }

    if (bytes_to_read != PW_DATA_BUFFER_SIZE) {
        pw_pr_error("Error: bytes_to_read = %u, required to be %lu\n", (unsigned)bytes_to_read, (unsigned long)PW_DATA_BUFFER_SIZE);
        return bytes_to_read;
    }
    which_cpu = mask >> 16; which_seg = mask & 0xffff;
    pw_pr_debug(KERN_INFO "CONSUME: cpu = %d, seg = %d\n", which_cpu, which_seg);
    if (which_seg >= NUM_SEGS_PER_BUFFER) {
        pw_pr_error("Error: which_seg (%d) >= NUM_SEGS_PER_BUFFER (%d)\n", which_seg, NUM_SEGS_PER_BUFFER);
        return bytes_to_read;
    }
    /*
     * OK to access unlocked; either the segment is FULL, or no collection
     * is ongoing. In either case, we're GUARANTEED no producer is touching
     * this segment.
     */
    buff = GET_OUTPUT_BUFFER(which_cpu);
    seg = buff->buffers[which_seg];
    bytes_not_copied = copy_to_user(buffer, seg->buffer, seg->bytes_written); // dst,src
    if (likely(bytes_not_copied == 0)) {
      *bytes_read = seg->bytes_written;
    } else {
        pw_pr_warn("Warning: couldn't copy %u bytes\n", bytes_not_copied);
    }
    seg->is_full = seg->bytes_written = 0;
    return bytes_not_copied;
};

unsigned long pw_get_buffer_size(void)
{
    return PW_DATA_BUFFER_SIZE;
};

void pw_count_samples_produced_dropped(void)
{
    int cpu = 0;
    pw_num_samples_produced = pw_num_samples_dropped = 0;
    if (per_cpu_output_buffers == NULL) {
        return;
    }
    // for_each_possible_cpu(cpu) {
    // for (cpu=0; cpu<pw_max_num_cpus; ++cpu)
    for_each_output_buffer(cpu) {
        pw_output_buffer_t *buff = GET_OUTPUT_BUFFER(cpu);
        pw_pr_debug(KERN_INFO "[%d]: # samples = %u\n", cpu, buff->produced_samples);
        pw_num_samples_dropped += buff->dropped_samples;
        pw_num_samples_produced += buff->produced_samples;
    }
};
