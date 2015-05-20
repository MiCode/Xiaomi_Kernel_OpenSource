/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2007-2014 Intel Corporation. All rights reserved.

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

  Copyright(c) 2007-2014 Intel Corporation. All rights reserved.
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

#ifndef _LWPMUDRV_IOCTL_H_
#define _LWPMUDRV_IOCTL_H_

#if defined(__cplusplus)
extern "C" {
#endif


//SEP Driver Operation defines
//
#define DRV_OPERATION_START                          1
#define DRV_OPERATION_STOP                           2
#define DRV_OPERATION_INIT_PMU                       3
#define DRV_OPERATION_GET_NORMALIZED_TSC             4
#define DRV_OPERATION_TSC_SKEW_INFO                  5
#define DRV_OPERATION_PAUSE                          6
#define DRV_OPERATION_RESUME                         7
#define DRV_OPERATION_TERMINATE                      8
#define DRV_OPERATION_RESERVE                        9
#define DRV_OPERATION_VERSION                        10
#define DRV_OPERATION_SWITCH_GROUP                   11
#define DRV_OPERATION_GET_DRIVER_STATE               12
#define DRV_OPERATION_INIT_UNCORE                    13
#define DRV_OPERATION_EM_GROUPS_UNCORE               14
#define DRV_OPERATION_EM_CONFIG_NEXT_UNCORE          15
#define DRV_OPERATION_READ_UNCORE_DATA               16
#define DRV_OPERATION_STOP_MEM                       17
#define DRV_OPERATION_CREATE_MEM                     18
#define DRV_OPERATION_READ_MEM                       19
#define DRV_OPERATION_CHECK_STATUS                   20
#define DRV_OPERATION_TIMER_TRIGGER_READ             21

// IOCTL_SETUP
//

#if defined(DRV_OS_WINDOWS)

//
// NtDeviceIoControlFile IoControlCode values for this device.
//
// Warning:  Remember that the low two bits of the code specify how the
//           buffers are passed to the driver!
//
// 16 bit device type. 12 bit function codes
#define LWPMUDRV_IOCTL_DEVICE_TYPE  0xA000   // values 0-32768 reserved for Microsoft
#define LWPMUDRV_IOCTL_FUNCTION     0x0A00   // values 0-2047  reserved for Microsoft

//
// Basic CTL CODE macro to reduce typographical errors
// Use for FILE_READ_ACCESS
//
#define LWPMUDRV_CTL_READ_CODE(x)    CTL_CODE(LWPMUDRV_IOCTL_DEVICE_TYPE,  \
                                              LWPMUDRV_IOCTL_FUNCTION+(x), \
                                              METHOD_BUFFERED,             \
                                              FILE_READ_ACCESS)

#define LWPMUDRV_IOCTL_START                        LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_START)
#define LWPMUDRV_IOCTL_STOP                         LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_STOP)
#define LWPMUDRV_IOCTL_INIT_PMU                     LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_INIT_PMU)
#define LWPMUDRV_IOCTL_GET_NORMALIZED_TSC           LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_GET_NORMALIZED_TSC)
#define LWPMUDRV_IOCTL_TSC_SKEW_INFO                LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_TSC_SKEW_INFO)
#define LWPMUDRV_IOCTL_PAUSE                        LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_PAUSE)
#define LWPMUDRV_IOCTL_RESUME                       LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_RESUME)
#define LWPMUDRV_IOCTL_TERMINATE                    LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_TERMINATE)
#define LWPMUDRV_IOCTL_RESERVE                      LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_RESERVE)
#define LWPMUDRV_IOCTL_VERSION                      LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_VERSION)
#define LWPMUDRV_IOCTL_SWITCH_GROUP                 LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_SWITCH_GROUP)
#define LWPMUDRV_IOCTL_GET_DRIVER_STATE             LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_GET_DRIVER_STATE)
#define LWPMUDRV_IOCTL_INIT_UNCORE                  LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_INIT_UNCORE)
#define LWPMUDRV_IOCTL_EM_GROUPS_UNCORE             LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_EM_GROUPS_UNCORE)
#define LWPMUDRV_IOCTL_EM_CONFIG_NEXT_UNCORE        LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_EM_CONFIG_NEXT_UNCORE)
#define LWPMUDRV_IOCTL_READ_UNCORE_DATA             LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_READ_UNCORE_DATA)
#define LWPMUDRV_IOCTL_STOP_MEM                     LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_STOP_MEM)
#define LWPMUDRV_IOCTL_CREATE_MEM                   LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_CREATE_MEM)
#define LWPMUDRV_IOCTL_READ_MEM                     LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_READ_MEM)
#define LWPMUDRV_IOCTL_CHECK_STATUS                 LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_CHECK_STATUS)
#define LWPMUDRV_IOCTL_TIMER_TRIGGER_READ           LWPMUDRV_CTL_READ_CODE(DRV_OPERATION_TIMER_TRIGGER_READ)

#elif defined(DRV_OS_LINUX) || defined(DRV_OS_SOLARIS) || defined (DRV_OS_ANDROID)
// IOCTL_ARGS
typedef struct IOCTL_ARGS_NODE_S  IOCTL_ARGS_NODE;
typedef        IOCTL_ARGS_NODE   *IOCTL_ARGS;
struct IOCTL_ARGS_NODE_S {
    U64    r_len;
    U64    w_len;
    char  *r_buf;
    char  *w_buf;
};

// COMPAT IOCTL_ARGS
#if defined (CONFIG_COMPAT) && defined(DRV_EM64T)
typedef struct IOCTL_COMPAT_ARGS_NODE_S  IOCTL_COMPAT_ARGS_NODE;
typedef        IOCTL_COMPAT_ARGS_NODE   *IOCTL_COMPAT_ARGS;
struct IOCTL_COMPAT_ARGS_NODE_S {
    U64            r_len;
    U64            w_len;
    compat_uptr_t  r_buf;
    compat_uptr_t  w_buf;
};
#endif

#define LWPMU_IOC_MAGIC   99

// IOCTL_SETUP
//
#define LWPMUDRV_IOCTL_START                  _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_START)
#define LWPMUDRV_IOCTL_STOP                   _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_STOP)
#define LWPMUDRV_IOCTL_INIT_PMU               _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_INIT_PMU, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_GET_NORMALIZED_TSC     _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_GET_NORMALIZED_TSC, int)
#define LWPMUDRV_IOCTL_TSC_SKEW_INFO          _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_TSC_SKEW_INFO, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_PAUSE                  _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_PAUSE)
#define LWPMUDRV_IOCTL_RESUME                 _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_RESUME)
#define LWPMUDRV_IOCTL_TERMINATE              _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_TERMINATE)
#define LWPMUDRV_IOCTL_RESERVE                _IOR(LWPMU_IOC_MAGIC, DRV_OPERATION_RESERVE, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_VERSION                _IOR(LWPMU_IOC_MAGIC, DRV_OPERATION_VERSION, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_SWITCH_GROUP           _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_SWITCH_GROUP)
#define LWPMUDRV_IOCTL_GET_DRIVER_STATE       _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_GET_DRIVER_STATE, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_INIT_UNCORE            _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_INIT_UNCORE, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_EM_GROUPS_UNCORE       _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_EM_GROUPS_UNCORE, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_EM_CONFIG_NEXT_UNCORE  _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_EM_CONFIG_NEXT_UNCORE, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_READ_UNCORE_DATA       _IOR(LWPMU_IOC_MAGIC, DRV_OPERATION_READ_UNCORE_DATA, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_STOP_MEM               _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_STOP_MEM)
#define LWPMUDRV_IOCTL_CREATE_MEM             _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_CREATE_MEM, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_READ_MEM               _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_READ_MEM, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_CHECK_STATUS           _IOR(LWPMU_IOC_MAGIC, DRV_OPERATION_CHECK_STATUS, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_TIMER_TRIGGER_READ     _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_TIMER_TRIGGER_READ)

#elif defined(DRV_OS_FREEBSD)

// IOCTL_ARGS
typedef struct IOCTL_ARGS_NODE_S  IOCTL_ARGS_NODE;
typedef        IOCTL_ARGS_NODE   *IOCTL_ARGS;
struct IOCTL_ARGS_NODE_S {
    U64    r_len;
    char  *r_buf;
    U64    w_len;
    char  *w_buf;
};

// IOCTL_SETUP
//
#define LWPMU_IOC_MAGIC   99

/* FreeBSD is very strict about IOR/IOW/IOWR specifications on IOCTLs.
 * Since these IOCTLs all pass down the real read/write buffer lengths
 *  and addresses inside of an IOCTL_ARGS_NODE data structure, we
 *  need to specify all of these as _IOW so that the kernel will
 *  view it as userspace passing the data to the driver, rather than
 *  the reverse.  There are also some cases where Linux is passing
 *  a smaller type than IOCTL_ARGS_NODE, even though its really
 *  passing an IOCTL_ARGS_NODE.  These needed to be fixed for FreeBSD.
 */
#define LWPMUDRV_IOCTL_START                  _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_START)
#define LWPMUDRV_IOCTL_STOP                   _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_STOP)
#define LWPMUDRV_IOCTL_INIT_PMU               _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_INIT_PMU)
#define LWPMUDRV_IOCTL_GET_NORMALIZED_TSC     _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_GET_NORMALIZED_TSC, IOCTL_ARGS_NODE)
#define LWPMUDRV_IOCTL_TSC_SKEW_INFO          _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_TSC_SKEW_INFO, IOCTL_ARGS_NODE)
#define LWPMUDRV_IOCTL_PAUSE                  _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_PAUSE)
#define LWPMUDRV_IOCTL_RESUME                 _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_RESUME)
#define LWPMUDRV_IOCTL_TERMINATE              _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_TERMINATE)
#define LWPMUDRV_IOCTL_RESERVE                _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_RESERVE, IOCTL_ARGS_NODE)
#define LWPMUDRV_IOCTL_VERSION                _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_VERSION, IOCTL_ARGS_NODE)
#define LWPMUDRV_IOCTL_SWITCH_GROUP           _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_SWITCH_GROUP)
#define LWPMUDRV_IOCTL_GET_DRIVER_STATE       _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_GET_DRIVER_STATE, IOCTL_ARGS_NODE)
#define LWPMUDRV_IOCTL_INIT_UNCORE            _IOW (LWPMU_IOC_MAGIC, DRV_OPERATION_INIT_UNCORE, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_EM_GROUPS_UNCORE       _IOW (LWPMU_IOC_MAGIC, DRV_OPERATION_EM_GROUPS_UNCORE, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_EM_CONFIG_NEXT_UNCORE  _IOW (LWPMU_IOC_MAGIC, DRV_OPERATION_EM_CONFIG_NEXT_UNCORE, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_READ_UNCORE_DATA       _IOR(LWPMU_IOC_MAGIC, DRV_OPERATION_READ_UNCORE_DATA, IOCTL_ARGS)
#define LWPMUDRV_IOCTL_STOP_MEM               _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_STOP_MEM)
#define LWPMUDRV_IOCTL_CREATE_MEM             _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_CREATE_MEM, IOCTL_ARGS_NODE)
#define LWPMUDRV_IOCTL_READ_MEM               _IOW(LWPMU_IOC_MAGIC, DRV_OPERATION_READ_MEM, IOCTL_ARGS_NODE)
#define LWPMUDRV_IOCTL_CHECK_STATUS           _IOR(LWPMU_IOC_MAGIC, DRV_OPERATION_CHECK_STATUS, IOCTL_ARGS_NODE)
#define LWPMUDRV_IOCTL_TIMER_TRIGGER_READ     _IO (LWPMU_IOC_MAGIC, DRV_OPERATION_TIMER_TRIGGER_READ)

#elif defined(DRV_OS_MAC)

// IOCTL_ARGS
typedef struct IOCTL_ARGS_NODE_S  IOCTL_ARGS_NODE;
typedef        IOCTL_ARGS_NODE   *IOCTL_ARGS;
struct IOCTL_ARGS_NODE_S {
	U64    r_len;
	char  *r_buf;
	U64    w_len;
	char  *w_buf;
	U32	  command;
};

typedef struct CPU_ARGS_NODE_S  CPU_ARGS_NODE;
typedef        CPU_ARGS_NODE   *CPU_ARGS;
struct CPU_ARGS_NODE_S {
	U64    r_len;
	char  *r_buf;
	U32	  command;
	U32	  CPU_ID;
	U32	  BUCKET_ID;
};

// IOCTL_SETUP
//
#define LWPMU_IOC_MAGIC    99
#define OS_SUCCESS         0
#define OS_STATUS          int
#define OS_ILLEGAL_IOCTL  -ENOTTY
#define OS_NO_MEM         -ENOMEM
#define OS_FAULT          -EFAULT

// Task file Opcodes.
// keeping the definitions as IOCTL but in MAC OSX
// these are really OpCodes consumed by Execute command.
#define LWPMUDRV_IOCTL_START                  DRV_OPERATION_START
#define LWPMUDRV_IOCTL_STOP                   DRV_OPERATION_STOP
#define LWPMUDRV_IOCTL_INIT_PMU               DRV_OPERATION_INIT_PMU
#define LWPMUDRV_IOCTL_GET_NORMALIZED_TSC     DRV_OPERATION_GET_NORMALIZED_TSC
#define LWPMUDRV_IOCTL_TSC_SKEW_INFO          DRV_OPERATION_TSC_SKEW_INFO
#define LWPMUDRV_IOCTL_PAUSE                  DRV_OPERATION_PAUSE
#define LWPMUDRV_IOCTL_RESUME                 DRV_OPERATION_RESUME
#define LWPMUDRV_IOCTL_TERMINATE              DRV_OPERATION_TERMINATE
#define LWPMUDRV_IOCTL_RESERVE                DRV_OPERATION_RESERVE
#define LWPMUDRV_IOCTL_VERSION                DRV_OPERATION_VERSION
#define LWPMUDRV_IOCTL_SWITCH_GROUP           DRV_OPERATION_SWITCH_GROUP
#define LWPMUDRV_IOCTL_GET_DRIVER_STATE       DRV_OPERATION_GET_DRIVER_STATE
#define LWPMUDRV_IOCTL_INIT_UNCORE            DRV_OPERATION_INIT_UNCORE
#define LWPMUDRV_IOCTL_EM_GROUPS_UNCORE       DRV_OPERATION_EM_GROUPS_UNCORE
#define LWPMUDRV_IOCTL_EM_CONFIG_NEXT_UNCORE  DRV_OPERATION_EM_CONFIG_NEXT_UNCORE
#define LWPMUDRV_IOCTL_READ_UNCORE_DATA       DRV_OPERATION_READ_UNCORE_DATA
#define LWPMUDRV_IOCTL_STOP_MEM               DRV_OPERATION_STOP_MEM
#define LWPMUDRV_IOCTL_CREATE_MEM             DRV_OPERATION_CREATE_MEM
#define LWPMUDRV_IOCTL_READ_MEM               DRV_OPERATION_READ_MEM
#define LWPMUDRV_IOCTL_CHECK_STATUS           DRV_OPERATION_CHECK_STATUS
#define LWPMUDRV_IOCTL_TIMER_TRIGGER_READ     DRV_OPERATION_TIMER_TRIGGER_READ

// This is only for MAC OSX
#define LWPMUDRV_IOCTL_SET_OSX_VERSION        998
#define LWPMUDRV_IOCTL_PROVIDE_FUNCTION_PTRS  999

#else
#error "unknown OS in lwpmudrv_ioctl.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif

