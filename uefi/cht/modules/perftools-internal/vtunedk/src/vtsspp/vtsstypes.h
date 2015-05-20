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
#ifndef _VTSSTYPES_H_
#define _VTSSTYPES_H_

#include <linux/types.h>        /* for size_t */

/**
// Data Types and Macros
*/
/* Should be in sync with collectunits1/traceformat/include/traceformat.h */
#define VTSS_WINDOWS_IA32   0
#define VTSS_WINDOWS_IA64   1
#define VTSS_WINDOWS_EM64T  2

#define VTSS_LINUX_IA32     3
#define VTSS_LINUX_IA64     4
#define VTSS_LINUX_EM64T    5
#define VTSS_LINUX_KNC      6

#define VTSS_UNKNOWN_ARCH   0xff

#pragma pack(push, 1)

#define VTSS_FMTCFG_RESERVED 0xF0   /// specified for bits unused in the trace being generated
                                    /// for format indicator bits specify the size of 0

/// defbit[i] specifies the size code for a corresponding syntax element as follows:
/// [76543210]
///  ||
///  |+------- reserved
///  +-------- 0: fixed-length element, bits 0-5 provide the size of a syntax element in bytes
///   
///   +------- 1: variable-length element, bits 0-2 specify the number of bytes at the 
///               beginning of the element which, in their turn, provide the element's size

typedef struct
{
    unsigned char rank;
    unsigned int  and_mask;
    unsigned int  cmp_mask;

    unsigned char defcount;
    unsigned char defbit[0x20]; /// sizeof(flagword)

} fmtcfg_t;

/// format configuration trace record
typedef struct
{
    unsigned int flagword;
    unsigned short size;
    unsigned short type;

} fcf_trace_record_t;

/// hardware configuration trace record
typedef struct
{
    unsigned int flagword;
    unsigned short size;
    unsigned short type;

} hcf_trace_record_t;

/// collector configuration trace record
typedef struct
{
    unsigned int flagword;
    unsigned short size;
    unsigned short type;

    int version;

    short major;
    short minor;
    int   revision;
    long long features;

    unsigned char len;
    char name[0];

} col_trace_record_t;

/// system configuration trace record
typedef struct
{
    unsigned int flagword;
    unsigned short size;
    unsigned short type;

} sys_trace_record_t;

/// software configuration trace record
typedef struct
{
    unsigned int flagword;
    unsigned int vresidx;
    unsigned short size;
    unsigned short type;

} scf_trace_record_t;

typedef struct
{
    unsigned int flagword;
    unsigned int vectored;
    unsigned char vec_no;
    long long tsc;
    long long utc;

} time_marker_record_t;

/// new task trace record
typedef struct
{
    unsigned int flagword;
    unsigned int activity;
    unsigned int cpuidx;
    unsigned int pid;
    unsigned int tid;
    long long cputsc;
    long long realtsc;
    unsigned short size;
    unsigned short type;

} ntk_trace_record_t;

/// dynamically captured module trace record
typedef struct
{
    unsigned int flagword;
    unsigned int pid;
    unsigned int tid;
    long long cputsc;
    long long realtsc;
    unsigned short size;
    unsigned short type;

    size_t start;
    size_t end;
    size_t offset;
    unsigned char bin;
    unsigned short len;

} dlm_trace_record_t;

typedef struct
{
    unsigned int flagword;
    unsigned int pid;
    unsigned int tid;
    long long cputsc;
    long long realtsc;
    unsigned short size;
    unsigned short type;

    unsigned int start;
    unsigned int end;
    unsigned int offset;
    unsigned char bin;
    unsigned short len;

} dlm_trace_record_32_t;

// new active thread trace record
typedef struct
{
    unsigned int flagword;
    unsigned int activity;
    unsigned int residx;
    unsigned int cpuidx;
    unsigned int pid;
    unsigned int tid;
    long long cputsc;
    long long realtsc;

} nth_trace_record_t;

// context swap in trace record
typedef union
{
    struct
    {
        unsigned int flagword;
        unsigned int activity;
        unsigned int residx;
        unsigned int cpuidx;
        long long cputsc;
        long long realtsc;
        long long execaddr;

    } procina;

    struct
    {
        unsigned int flagword;
        unsigned int activity;
        unsigned int residx;
        unsigned int cpuidx;
        long long cputsc;
        long long realtsc;

    } procin;

    struct
    {
        unsigned int flagword;
        unsigned int activity;
        unsigned int cpuidx;
        unsigned int pid;
        unsigned int tid;
        long long cputsc;
        long long realtsc;
        long long execaddr;

    } sysina;

    struct
    {
        unsigned int flagword;
        unsigned int activity;
        unsigned int cpuidx;
        unsigned int pid;
        unsigned int tid;
        long long cputsc;
        long long realtsc;

    } sysin;

} cti_trace_record_t;

// context swap out trace record
typedef union
{
    struct
    {
        unsigned int flagword;
        unsigned int activity;
        unsigned int cpuidx;
        long long cputsc;
        long long realtsc;
        long long execaddr;

    } procout;

    struct
    {
        unsigned int flagword;
        unsigned int activity;
        unsigned int cpuidx;
        long long cputsc;
        long long realtsc;

    } sysout;

} cto_trace_record_t;

// process creation/destruction trace record
typedef struct
{
    unsigned int flagword;
    unsigned int activity;
    unsigned int cpuidx;
    unsigned int pid;
    unsigned int tid;
    long long cputsc;
    long long realtsc;
    unsigned short size;
    unsigned short type;

} prc_trace_record_t;

// thread creation/destruction trace record
typedef struct
{
    unsigned int flagword;
    unsigned int activity;
    unsigned int cpuidx;
    unsigned int pid;
    unsigned int tid;
    long long cputsc;
    long long realtsc;

} thr_trace_record_t;

// module load trace record
typedef struct
{
    unsigned int flagword;
    unsigned int pid;
    unsigned int tid;
    long long cputsc;
    long long realtsc;
    unsigned short size;
    unsigned short type;

    unsigned int start;
    unsigned int end;
    unsigned int offset;
    unsigned char bin;
    unsigned short len;

} mod_trace_record_t;

/// stack trace record
typedef struct
{
    unsigned int flagword;
    unsigned int residx;
    unsigned short size;
    unsigned short type;
    
    unsigned int idx;


} stk_trace_kernel_record_t;

/// stack trace record
typedef struct
{
    unsigned int flagword;
    unsigned int residx;
    unsigned int cpuidx;
    long long cputsc;
    long long execaddr;
    unsigned short size;
    unsigned short type;

    union
    {
        struct
        {
            size_t sp;
            size_t fp;
        };
        struct
        {
            unsigned int sp32;
            unsigned int fp32;
        };
    };

} stk_trace_record_t;

/// large stack trace record
typedef struct
{
    unsigned int flagword;
    unsigned int residx;
    unsigned int cpuidx;
    long long cputsc;
    long long execaddr;
    unsigned int size;
    unsigned short type;

    union
    {
        struct
        {
            size_t sp;
            size_t fp;
        };
        struct
        {
            unsigned int sp32;
            unsigned int fp32;
        };
    };

} lstk_trace_record_t;

/**
// Trace Records generated for events
*/

/// Trigger Point Event record
typedef struct
{
    unsigned int flagword;
    unsigned int vectored;
    unsigned int cpuidx;
    unsigned char muxgroup;
    unsigned char event_no;

} tpe_trace_record_t;

/// Sample Point Event record
typedef struct
{
    unsigned int flagword;
    unsigned int vectored;
    unsigned int activity;
    unsigned int residx;
    unsigned int cpuidx;
    unsigned long long cputsc;
    unsigned char muxgroup;
    unsigned char event_no;

} spe_trace_record_t;

/// General Purpose Event record
typedef struct
{
    unsigned int flagword;
    unsigned int vectored;
    unsigned int residx;
    unsigned int cpuidx;
    unsigned long long cputsc;
    unsigned char muxgroup;
    unsigned char event_no;
} gpe_trace_record_t;

typedef union
{
    /// trigger point event record
    tpe_trace_record_t tperec;
    /// sample point event record
    spe_trace_record_t sperec;
    /// general-purpose event record
    gpe_trace_record_t gperec;
} event_trace_record_t;

typedef struct
{
    unsigned int flagword;
    unsigned int residx;
    unsigned int cpuidx;
    unsigned long long cputsc;
    unsigned short size;
    unsigned short type;
} bts_trace_record_t;

typedef struct
{
    unsigned int flagword;
    unsigned int activity;
    unsigned int residx;
    unsigned int cpuidx;
    long long cputsc;
    unsigned short size;
    unsigned short type;
    long long entry_tsc;
    unsigned int entry_cpu;
    unsigned int fid;

} prb_trace_record_t;

// thread name trace record
typedef struct
{
    prb_trace_record_t probe;
    unsigned char version;
    unsigned short length;

} thname_trace_record_t;

typedef struct
{
    unsigned int flagword;
    unsigned short size;
    unsigned short type;

} debug_info_record_t;

#pragma pack(pop)

#endif /* _VTSSTYPES_H_ */
