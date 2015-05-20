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

#ifndef _PW_OUTPUT_BUFFER_H_
#define _PW_OUTPUT_BUFFER_H_ 1
/*
 * Special mask for the case where all buffers have been flushed.
 */
// #define PW_ALL_WRITES_DONE_MASK 0xffffffff
#define PW_ALL_WRITES_DONE_MASK ((u32)-1)
/*
 * Special mask for the case where no data is available to be read.
 */
#define PW_NO_DATA_AVAIL_MASK ((u32)-2)

/*
 * Forward declarations.
 */
typedef struct PWC_tps_msg PWC_tps_msg_t;
struct PWCollector_msg;
struct c_msg;

/*
 * Common data structures.
 */
#pragma pack(push)
#pragma pack(2)
/*
 * Everything EXCEPT the 'c_msg_t' field MUST match
 * the fields in 'PWCollector_msg_t' EXACTLY!!!
 */
struct PWC_tps_msg {
    u64 tsc;
    u16 data_len;
    u8 cpuidx; // should really be a u16!!!
    u8 data_type;
    struct c_msg data;
};
#pragma pack(pop)

/*
 * Variable declarations.
 */
extern u64 pw_num_samples_produced, pw_num_samples_dropped;
extern unsigned long pw_buffer_alloc_size;
extern wait_queue_head_t pw_reader_queue;
extern int pw_max_num_cpus;

/*
 * Public API.
 */
int pw_init_per_cpu_buffers(void);
void pw_destroy_per_cpu_buffers(void);
void pw_reset_per_cpu_buffers(void);
int pw_map_per_cpu_buffers(struct vm_area_struct *vma, unsigned long *total_size);

void pw_count_samples_produced_dropped(void);

int pw_produce_generic_msg(struct PWCollector_msg *, bool);
int pw_produce_generic_msg_on_cpu(int cpu, struct PWCollector_msg *, bool);

bool pw_any_seg_full(u32 *val, const bool *is_flush_mode);
unsigned long pw_consume_data(u32 mask, char __user *buffer, size_t bytes_to_read, size_t *bytes_read);

unsigned long pw_get_buffer_size(void);

void pw_wait_once(void);
void pw_wakeup(void);

/*
 * Debugging ONLY!!!
 */
void pw_dump_pages(const char *msg);
#endif // _PW_OUTPUT_BUFFER_H_
