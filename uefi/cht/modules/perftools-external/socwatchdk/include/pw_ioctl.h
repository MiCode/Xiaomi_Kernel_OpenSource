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
 * Description: file containing IOCTL info.
 */

#ifndef _PW_IOCTL_H_
#define _PW_IOCTL_H_ 1

#if PW_KERNEL_MODULE
  #include <linux/ioctl.h>
  #if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
    #include <linux/compat.h>
  #endif // COMPAT && x64
#else
  #include <sys/ioctl.h>
#endif

/* 
 * The APWR-specific IOCTL magic
 * number -- used to ensure IOCTLs
 * are delivered to the correct
 * driver.
 */
// #define APWR_IOCTL_MAGIC_NUM 0xdead
#define APWR_IOCTL_MAGIC_NUM 100

/* 
 * The name of the device file 
 */
// #define DEVICE_FILE_NAME "/dev/pw_driver_char_dev"
#define PW_DEVICE_FILE_NAME "/dev/apwr_driver_char_dev"
#define PW_DEVICE_NAME "apwr_driver_char_dev"

/*
 * Data structs that the IOCTLs will need.
 */
#include "pw_structs.h"
// #include "pw_defines.h"

/*
 * The actual IOCTL commands.
 *
 * From the kernel documentation:
 * "_IOR" ==> Read IOCTL
 * "_IOW" ==> Write IOCTL
 * "_IOWR" ==> Read/Write IOCTL
 *
 * Where "Read" and "Write" are from the user's perspective
 * (similar to the file "read" and "write" calls).
 */
#define PW_IOCTL_CONFIG _IOW(APWR_IOCTL_MAGIC_NUM, 1, struct PWCollector_ioctl_arg *)
#if DO_COUNT_DROPPED_SAMPLES
    #define PW_IOCTL_CMD _IOWR(APWR_IOCTL_MAGIC_NUM, 2, struct PWCollector_ioctl_arg *)
#else
    #define PW_IOCTL_CMD _IOW(APWR_IOCTL_MAGIC_NUM, 2, struct PWCollector_ioctl_arg *)
#endif // DO_COUNT_DROPPED_SAMPLES
#define PW_IOCTL_STATUS _IOR(APWR_IOCTL_MAGIC_NUM, 3, struct PWCollector_ioctl_arg *)
#define PW_IOCTL_SAMPLE _IOR(APWR_IOCTL_MAGIC_NUM, 4, struct PWCollector_ioctl_arg *)
#define PW_IOCTL_CHECK_PLATFORM _IOR(APWR_IOCTL_MAGIC_NUM, 5, struct PWCollector_ioctl_arg *)
#define PW_IOCTL_VERSION _IOR(APWR_IOCTL_MAGIC_NUM, 6, struct PWCollector_version_info *)
#define PW_IOCTL_MICRO_PATCH _IOR(APWR_IOCTL_MAGIC_NUM, 7, struct PWCollector_micro_patch_info *)
#define PW_IOCTL_IRQ_MAPPINGS _IOR(APWR_IOCTL_MAGIC_NUM, 8, struct PWCollector_irq_mapping_block *)
#define PW_IOCTL_PROC_MAPPINGS _IOR(APWR_IOCTL_MAGIC_NUM, 9, struct PWCollector_PROC_mapping_block *)
#define PW_IOCTL_TURBO_THRESHOLD _IOR(APWR_IOCTL_MAGIC_NUM, 10, struct PWCollector_turbo_threshold *)
#define PW_IOCTL_AVAILABLE_FREQUENCIES _IOR(APWR_IOCTL_MAGIC_NUM, 11, struct PWCollector_available_frequencies *)
#define PW_IOCTL_COLLECTION_TIME _IOW(APWR_IOCTL_MAGIC_NUM, 12, unsigned long *)
#define PW_IOCTL_MMAP_SIZE _IOW(APWR_IOCTL_MAGIC_NUM, 13, unsigned long *)
#define PW_IOCTL_BUFFER_SIZE _IOW(APWR_IOCTL_MAGIC_NUM, 14, unsigned long *)
#define PW_IOCTL_DO_D_NC_READ _IOR(APWR_IOCTL_MAGIC_NUM, 15, unsigned long *)
#define PW_IOCTL_FSB_FREQ _IOR(APWR_IOCTL_MAGIC_NUM, 16, unsigned long *)
#define PW_IOCTL_MSR_ADDRS _IOW(APWR_IOCTL_MAGIC_NUM, 17, struct PWCollector_ioctl_arg *)
#define PW_IOCTL_FREQ_RATIOS _IOR(APWR_IOCTL_MAGIC_NUM, 18, unsigned long *)
#define PW_IOCTL_PLATFORM_RES_CONFIG _IOW(APWR_IOCTL_MAGIC_NUM, 19, struct PWCollector_ioctl_arg *)

/*
 * 32b-compatible version of the above
 * IOCTL numbers. Required ONLY for
 * 32b compatibility on 64b systems,
 * and ONLY by the driver.
 */
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
    #define PW_IOCTL_CONFIG32 _IOW(APWR_IOCTL_MAGIC_NUM, 1, compat_uptr_t)
#if DO_COUNT_DROPPED_SAMPLES
        #define PW_IOCTL_CMD32 _IOWR(APWR_IOCTL_MAGIC_NUM, 2, compat_uptr_t)
#else
        #define PW_IOCTL_CMD32 _IOW(APWR_IOCTL_MAGIC_NUM, 2, compat_uptr_t)
#endif // DO_COUNT_DROPPED_SAMPLES
    #define PW_IOCTL_STATUS32 _IOR(APWR_IOCTL_MAGIC_NUM, 3, compat_uptr_t)
    #define PW_IOCTL_SAMPLE32 _IOR(APWR_IOCTL_MAGIC_NUM, 4, compat_uptr_t)
    #define PW_IOCTL_CHECK_PLATFORM32 _IOR(APWR_IOCTL_MAGIC_NUM, 5, compat_uptr_t)
    #define PW_IOCTL_VERSION32 _IOR(APWR_IOCTL_MAGIC_NUM, 6, compat_uptr_t)
    #define PW_IOCTL_MICRO_PATCH32 _IOR(APWR_IOCTL_MAGIC_NUM, 7, compat_uptr_t)
    #define PW_IOCTL_IRQ_MAPPINGS32 _IOR(APWR_IOCTL_MAGIC_NUM, 8, compat_uptr_t)
    #define PW_IOCTL_PROC_MAPPINGS32 _IOR(APWR_IOCTL_MAGIC_NUM, 9, compat_uptr_t)
    #define PW_IOCTL_TURBO_THRESHOLD32 _IOR(APWR_IOCTL_MAGIC_NUM, 10, compat_uptr_t)
    #define PW_IOCTL_AVAILABLE_FREQUENCIES32 _IOR(APWR_IOCTL_MAGIC_NUM, 11, compat_uptr_t)
    #define PW_IOCTL_COLLECTION_TIME32 _IOW(APWR_IOCTL_MAGIC_NUM, 12, compat_uptr_t)
    #define PW_IOCTL_MMAP_SIZE32 _IOW(APWR_IOCTL_MAGIC_NUM, 13, compat_uptr_t)
    #define PW_IOCTL_BUFFER_SIZE32 _IOW(APWR_IOCTL_MAGIC_NUM, 14, compat_uptr_t)
    #define PW_IOCTL_DO_D_NC_READ32 _IOR(APWR_IOCTL_MAGIC_NUM, 15, compat_uptr_t)
    #define PW_IOCTL_FSB_FREQ32 _IOR(APWR_IOCTL_MAGIC_NUM, 16, compat_uptr_t)
    #define PW_IOCTL_MSR_ADDRS32 _IOW(APWR_IOCTL_MAGIC_NUM, 17, compat_uptr_t)
    #define PW_IOCTL_FREQ_RATIOS32 _IOR(APWR_IOCTL_MAGIC_NUM, 18, compat_uptr_t)
    #define PW_IOCTL_PLATFORM_RES_CONFIG32 _IOW(APWR_IOCTL_MAGIC_NUM, 19, compat_uptr_t)
#endif // defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)

#endif // _PW_IOCTL_H_
