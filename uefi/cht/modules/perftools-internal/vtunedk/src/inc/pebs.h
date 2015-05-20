/*
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.
 
    This file is part of SEP Development Kit
 
    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.
 
    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 
    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/


#ifndef _PEBS_H_
#define _PEBS_H_

typedef struct PEBS_REC_NODE_S  PEBS_REC_NODE;

struct PEBS_REC_NODE_S {
    U64 r_flags;             // Offset 0x00
    U64 linear_ip;           // Offset 0x08
    U64 rax;                 // Offset 0x10
    U64 rbx;                 // Offset 0x18
    U64 rcx;                 // Offset 0x20
    U64 rdx;                 // Offset 0x28
    U64 rsi;                 // Offset 0x30
    U64 rdi;                 // Offset 0x38
    U64 rbp;                 // Offset 0x40
    U64 rsp;                 // Offset 0x48
    U64 r8;                  // Offset 0x50
    U64 r9;                  // Offset 0x58
    U64 r10;                 // Offset 0x60
    U64 r11;                 // Offset 0x68
    U64 r12;                 // Offset 0x70
    U64 r13;                 // Offset 0x78
    U64 r14;                 // Offset 0x80
    U64 r15;                 // Offset 0x88
};

typedef struct PEBS_REC_EXT_NODE_S  PEBS_REC_EXT_NODE;
typedef        PEBS_REC_EXT_NODE   *PEBS_REC_EXT;
struct PEBS_REC_EXT_NODE_S {
    PEBS_REC_NODE pebs_basic;          // Offset 0x00 to 0x88
    U64           glob_perf_overflow;  // Offset 0x90
    U64           data_linear_address; // Offset 0x98
    U64           data_source;         // Offset 0xA0
    U64           latency;             // Offset 0xA8
};

#define PEBS_REC_EXT_r_flags(x)               (x)->pebs_basic.r_flags
#define PEBS_REC_EXT_linear_ip(x)             (x)->pebs_basic.linear_ip
#define PEBS_REC_EXT_rax(x)                   (x)->pebs_basic.rax
#define PEBS_REC_EXT_rbx(x)                   (x)->pebs_basic.rbx
#define PEBS_REC_EXT_rcx(x)                   (x)->pebs_basic.rcx
#define PEBS_REC_EXT_rdx(x)                   (x)->pebs_basic.rdx
#define PEBS_REC_EXT_rsi(x)                   (x)->pebs_basic.rsi
#define PEBS_REC_EXT_rdi(x)                   (x)->pebs_basic.rdi
#define PEBS_REC_EXT_rbp(x)                   (x)->pebs_basic.rbp
#define PEBS_REC_EXT_rsp(x)                   (x)->pebs_basic.rsp
#define PEBS_REC_EXT_r8(x)                    (x)->pebs_basic.r8
#define PEBS_REC_EXT_r9(x)                    (x)->pebs_basic.r9
#define PEBS_REC_EXT_r10(x)                   (x)->pebs_basic.r10
#define PEBS_REC_EXT_r11(x)                   (x)->pebs_basic.r11
#define PEBS_REC_EXT_r12(x)                   (x)->pebs_basic.r12
#define PEBS_REC_EXT_r13(x)                   (x)->pebs_basic.r13
#define PEBS_REC_EXT_r14(x)                   (x)->pebs_basic.r14
#define PEBS_REC_EXT_r15(x)                   (x)->pebs_basic.r15
#define PEBS_REC_EXT_glob_perf_overflow(x)    (x)->glob_perf_overflow
#define PEBS_REC_EXT_data_linear_address(x)   (x)->data_linear_address
#define PEBS_REC_EXT_data_source(x)           (x)->data_source
#define PEBS_REC_EXT_latency(x)               (x)->latency


typedef struct PEBS_REC_EXT1_NODE_S  PEBS_REC_EXT1_NODE;
typedef        PEBS_REC_EXT1_NODE   *PEBS_REC_EXT1;
struct PEBS_REC_EXT1_NODE_S {
    PEBS_REC_EXT_NODE pebs_ext;
    U64               eventing_ip; //Offset 0xB0
    U64               hle_info;    //Offset 0xB8
};

#define PEBS_REC_EXT1_r_flags(x)               (x)->pebs_ext.pebs_basic.r_flags
#define PEBS_REC_EXT1_linear_ip(x)             (x)->pebs_ext.pebs_basic.linear_ip
#define PEBS_REC_EXT1_rax(x)                   (x)->pebs_ext.pebs_basic.rax
#define PEBS_REC_EXT1_rbx(x)                   (x)->pebs_ext.pebs_basic.rbx
#define PEBS_REC_EXT1_rcx(x)                   (x)->pebs_ext.pebs_basic.rcx
#define PEBS_REC_EXT1_rdx(x)                   (x)->pebs_ext.pebs_basic.rdx
#define PEBS_REC_EXT1_rsi(x)                   (x)->pebs_ext.pebs_basic.rsi
#define PEBS_REC_EXT1_rdi(x)                   (x)->pebs_ext.pebs_basic.rdi
#define PEBS_REC_EXT1_rbp(x)                   (x)->pebs_ext.pebs_basic.rbp
#define PEBS_REC_EXT1_rsp(x)                   (x)->pebs_ext.pebs_basic.rsp
#define PEBS_REC_EXT1_r8(x)                    (x)->pebs_ext.pebs_basic.r8
#define PEBS_REC_EXT1_r9(x)                    (x)->pebs_ext.pebs_basic.r9
#define PEBS_REC_EXT1_r10(x)                   (x)->pebs_ext.pebs_basic.r10
#define PEBS_REC_EXT1_r11(x)                   (x)->pebs_ext.pebs_basic.r11
#define PEBS_REC_EXT1_r12(x)                   (x)->pebs_ext.pebs_basic.r12
#define PEBS_REC_EXT1_r13(x)                   (x)->pebs_ext.pebs_basic.r13
#define PEBS_REC_EXT1_r14(x)                   (x)->pebs_ext.pebs_basic.r14
#define PEBS_REC_EXT1_r15(x)                   (x)->pebs_ext.pebs_basic.r15
#define PEBS_REC_EXT1_glob_perf_overflow(x)    (x)->pebs_ext.glob_perf_overflow
#define PEBS_REC_EXT1_data_linear_address(x)   (x)->pebs_ext.data_linear_address
#define PEBS_REC_EXT1_data_source(x)           (x)->pebs_ext.data_source
#define PEBS_REC_EXT1_latency(x)               (x)->pebs_ext.latency
#define PEBS_REC_EXT1_eventing_ip(x)           (x)->eventing_ip
#define PEBS_REC_EXT1_hle_info(x)              (x)->hle_info


typedef struct PEBS_REC_EXT2_NODE_S  PEBS_REC_EXT2_NODE;
typedef        PEBS_REC_EXT2_NODE   *PEBS_REC_EXT2;
struct PEBS_REC_EXT2_NODE_S {
    PEBS_REC_EXT1_NODE pebs_ext1;
    U64                tsc; //Offset 0xC0
};

#define PEBS_REC_EXT2_r_flags(x)               (x)->pebs_ext1->pebs_ext.pebs_basic.r_flags
#define PEBS_REC_EXT2_linear_ip(x)             (x)->pebs_ext1->pebs_ext.pebs_basic.linear_ip
#define PEBS_REC_EXT2_rax(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.rax
#define PEBS_REC_EXT2_rbx(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.rbx
#define PEBS_REC_EXT2_rcx(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.rcx
#define PEBS_REC_EXT2_rdx(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.rdx
#define PEBS_REC_EXT2_rsi(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.rsi
#define PEBS_REC_EXT2_rdi(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.rdi
#define PEBS_REC_EXT2_rbp(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.rbp
#define PEBS_REC_EXT2_rsp(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.rsp
#define PEBS_REC_EXT2_r8(x)                    (x)->pebs_ext1->pebs_ext.pebs_basic.r8
#define PEBS_REC_EXT2_r9(x)                    (x)->pebs_ext1->pebs_ext.pebs_basic.r9
#define PEBS_REC_EXT2_r10(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.r10
#define PEBS_REC_EXT2_r11(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.r11
#define PEBS_REC_EXT2_r12(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.r12
#define PEBS_REC_EXT2_r13(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.r13
#define PEBS_REC_EXT2_r14(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.r14
#define PEBS_REC_EXT2_r15(x)                   (x)->pebs_ext1->pebs_ext.pebs_basic.r15
#define PEBS_REC_EXT2_glob_perf_overflow(x)    (x)->pebs_ext1->pebs_ext.glob_perf_overflow
#define PEBS_REC_EXT2_data_linear_address(x)   (x)->pebs_ext1->pebs_ext.data_linear_address
#define PEBS_REC_EXT2_data_source(x)           (x)->pebs_ext1->pebs_ext.data_source
#define PEBS_REC_EXT2_latency(x)               (x)->pebs_ext1->pebs_ext.latency
#define PEBS_REC_EXT2_eventing_ip(x)           (x)->pebs_ext1->eventing_ip
#define PEBS_REC_EXT2_hle_info(x)              (x)->pebs_ext1->hle_info
#define PEBS_REC_EXT2_tsc(x)                   (x)->tsc


typedef struct DEAR_INFO_NODE_S   DEAR_INFO_NODE;
typedef        DEAR_INFO_NODE     *DEAR_INFO;

struct DEAR_INFO_NODE_S {
    U64 linear_address;
    U64 data_source;
    U64 latency;
    U64 node_id;
    U64 phys_addr;
};

#define DEAR_INFO_nodeid(x)                   (x)->node_id
#define DEAR_INFO_phys_addr(x)                (x)->phys_addr
#define DEAR_INFO_linear_address(x)           (x)->linear_address
#define DEAR_INFO_data_source(x)              (x)->data_source
#define DEAR_INFO_latency(x)                  (x)->latency

typedef struct DTS_BUFFER_EXT_NODE_S  DTS_BUFFER_EXT_NODE;
typedef        DTS_BUFFER_EXT_NODE   *DTS_BUFFER_EXT;
struct  DTS_BUFFER_EXT_NODE_S {
    U64 base;                   // Offset 0x00
    U64 index;                  // Offset 0x08
    U64 max;                    // Offset 0x10
    U64 threshold;              // Offset 0x18
    U64 pebs_base;              // Offset 0x20
    U64 pebs_index;             // Offset 0x28
    U64 pebs_max;               // Offset 0x30
    U64 pebs_threshold;         // Offset 0x38
    U64 counter_reset0;         // Offset 0x40
    U64 counter_reset1;         // Offset 0x48
    U64 counter_reset2;         // Offset 0x50
    U64 counter_reset3;
};

#define DTS_BUFFER_EXT_base(x)               (x)->base
#define DTS_BUFFER_EXT_index(x)              (x)->index
#define DTS_BUFFER_EXT_max(x)                (x)->max
#define DTS_BUFFER_EXT_threshold(x)          (x)->threshold
#define DTS_BUFFER_EXT_pebs_base(x)          (x)->pebs_base
#define DTS_BUFFER_EXT_pebs_index(x)         (x)->pebs_index
#define DTS_BUFFER_EXT_pebs_max(x)           (x)->pebs_max
#define DTS_BUFFER_EXT_pebs_threshold(x)     (x)->pebs_threshold
#define DTS_BUFFER_EXT_counter_reset0(x)     (x)->counter_reset0
#define DTS_BUFFER_EXT_counter_reset1(x)     (x)->counter_reset1
#define DTS_BUFFER_EXT_counter_reset2(x)     (x)->counter_reset2
#define DTS_BUFFER_EXT_counter_reset3(x)     (x)->counter_reset3

extern VOID
PEBS_Initialize (
    DRV_CONFIG  cfg
);

extern VOID
PEBS_Destroy (
    DRV_CONFIG  cfg
);

extern VOID 
PEBS_Reset_Index (
    S32 this_cpu
);

extern VOID
PEBS_Modify_IP (
    void       *sample,
    DRV_BOOL    is_64bit_addr
);

extern VOID
PEBS_Modify_TSC (
    void       *sample
);

extern VOID
PEBS_Fill_Buffer (
    S8            *buffer,
    EVENT_DESC    evt_desc,
    DRV_BOOL      virt_phys_translation_ena
);

extern U64
PEBS_Overflowed (
    S32  this_cpu,
    U64  overflow_status
);

/*
 *  Dispatch table for virtualized functions.
 *  Used to enable common functionality for different
 *  processor microarchitectures
 */
typedef struct PEBS_DISPATCH_NODE_S  PEBS_DISPATCH_NODE;
typedef        PEBS_DISPATCH_NODE   *PEBS_DISPATCH;
struct PEBS_DISPATCH_NODE_S {
    VOID (*initialize_threshold)(DTS_BUFFER_EXT, U32);
    U64  (*overflow)(S32, U64);
    VOID (*modify_ip)(void*, DRV_BOOL);
    VOID (*modify_tsc)(void*);
};

#endif  
