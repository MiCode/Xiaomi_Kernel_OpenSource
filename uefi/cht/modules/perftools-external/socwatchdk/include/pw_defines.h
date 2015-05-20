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

#ifndef _PW_DEFINES_H_
#define _PW_DEFINES_H_ 1

#include "pw_version.h"

/* ***************************************************
 * Common to kernel and userspace.
 * ***************************************************
 */
#define PW_SUCCESS 0
#define PW_ERROR 1

/*
 * Helper macro to convert 'u64' to 'unsigned long long' to avoid gcc warnings.
 */
#define TO_ULL(x) (unsigned long long)(x)
/*
 * Convert an arg to 'unsigned long'
 */
#define TO_UL(x) (unsigned long)(x)
/*
 * Helper macro for string representation of a boolean value.
 */
#define GET_BOOL_STRING(b) ( (b) ? "TRUE" : "FALSE" )

/*
 * Circularly increment 'i' MODULO 'l'.
 * ONLY WORKS IF 'l' is (power of 2 - 1) ie.
 * l == (2 ^ x) - 1
 */
#define CIRCULAR_INC(index, mask) ( ( (index) + 1) & (mask) )
#define CIRCULAR_ADD(index, val, mask) ( ( (index) + (val) ) & (mask) )
/*
 * Circularly decrement 'i'.
 */
#define CIRCULAR_DEC(i,m) ({int __tmp1 = (i); if(--__tmp1 < 0) __tmp1 = (m); __tmp1;})

#ifdef __KERNEL__

/* ***************************************************
 * The following is only valid for kernel code.
 * ***************************************************
 */

#define CPU() (raw_smp_processor_id())
#define RAW_CPU() (raw_smp_processor_id())
#define TID() (current->pid)
#define PID() (current->tgid)
#define NAME() (current->comm)
#define PKG(c) ( cpu_data(c).phys_proc_id )
#define IT_REAL_INCR() (current->signal->it_real_incr.tv64)

#define ATOMIC_CAS(ptr, old_val, new_val) ( cmpxchg( (ptr), (old_val), (new_val) ) == (old_val) )

/*
 * Should we allow debug output.
 * Set to: "1" ==> 'OUTPUT' is enabled.
 *         "0" ==> 'OUTPUT' is disabled.
 */
#define DO_DEBUG_OUTPUT 0
/*
 * Control whether to output driver ERROR messages.
 * These are independent of the 'OUTPUT' macro
 * (which controls debug messages).
 * Set to '1' ==> Print driver error messages (to '/var/log/messages')
 *        '0' ==> Do NOT print driver error messages
 */
#define DO_PRINT_DRIVER_ERROR_MESSAGES 1
/*
 * Macros to control output printing.
 */
#if DO_DEBUG_OUTPUT
    #define pw_pr_debug(...) printk(KERN_INFO __VA_ARGS__)
    #define pw_pr_warn(...) printk(KERN_WARNING __VA_ARGS__)
#else
    #define pw_pr_debug(...)
    #define pw_pr_warn(...)
#endif
/*
 * Macro for driver error messages.
 */
#if (DO_PRINT_DRIVER_ERROR_MESSAGES || DO_DEBUG_OUTPUT)
    #define pw_pr_error(...) printk(KERN_ERR __VA_ARGS__)
#else
    #define pw_pr_error(...)
#endif

#else // __KERNEL__

/* ***************************************************
 * The following is valid only for userspace code.
 * ***************************************************
 */
/*
 * Default output file name -- the extensions depend on
 * which program is executing: wuwatch output files have
 * a ".sw1" extension, while wudump output files have a
 * ".txt" extension. The extensions are added in by the
 * respective programs i.e. wuwatch/wudump.
 */
#define DEFAULT_WUWATCH_OUTPUT_FILE_NAME "wuwatch_output"
/*
 * Default wuwatch config file name.
 */
#define DEFAULT_WUWATCH_CONFIG_FILE_NAME "wuwatch_config.txt"
/*
 * Macro to convert a {major.minor.other} version into a
 * single 32-bit unsigned version number.
 * This is useful when comparing versions, for example.
 * Pretty much identical to the 'KERNEL_VERSION(...)' macro.
 */
//#define WUWATCH_VERSION(major, minor, other) ( (2^16) * (major) + (2^8) * (minor) + (other) )
#define COLLECTOR_VERSION(major, minor, other) ( (2^16) * (major) + (2^8) * (minor) + (other) )
/* **************************************
 * Debugging tools.
 * **************************************
 */
extern bool g_do_debugging;
#define db_fprintf(...) do { \
    if (g_do_debugging) { \
        fprintf(__VA_ARGS__); \
    } \
} while(0)
#define db_assert(e, ...) do { \
    if (g_do_debugging && !(e)) { \
	    fprintf(stderr, __VA_ARGS__);	\
	    assert(false);			\
	}					\
} while(0)
#define db_abort(...) do { \
    if (g_do_debugging) { \
        fprintf(stderr, __VA_ARGS__); \
        assert(false); \
    } \
} while(0)
#define db_copy(...) do { \
    if (g_do_debugging) { \
        std::copy(__VA_ARGS__); \
    } \
} while(0)
#define db_perror(...) do { \
    if (g_do_debugging) { \
        perror(__VA_ARGS__); \
    } \
} while(0)

#define LOG_WUWATCH_FUNCTION_ENTER() db_fprintf(stderr, "ENTERING Function \"%s\"\n", __FUNCTION__);
#define LOG_WUWATCH_FUNCTION_EXIT() db_fprintf(stderr, "EXITTING Function \"%s\"\n", __FUNCTION__);

/*
 * Macros corresponding to the kernel versions of 'likely()'
 * and 'unlikely()' -- GCC SPECIFIC ONLY!
 */
#if defined (__linux__)
	#define likely(x) __builtin_expect(!!(x), 1)
	#define unlikely(x) __builtin_expect(!!(x), 0)
#else // windows
	#define likely(x) (!!(x))
	#define unlikely(x) (!!(x))
#endif // linux

#endif // __KERNEL__

#endif // _PW_DEFINES_H_
