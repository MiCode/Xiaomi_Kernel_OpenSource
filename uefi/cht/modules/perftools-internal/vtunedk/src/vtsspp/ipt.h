/*COPYRIGHT**
// -------------------------------------------------------------------------
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of the accompanying license
//  agreement or nondisclosure agreement with Intel Corporation and may not
//  be copied or disclosed except in accordance with the terms of that
//  agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
// -------------------------------------------------------------------------
**COPYRIGHT*/

/*
//  File  : ipt.h
//  Author: Stanislav Bratanov
*/

#ifndef _VTSS_IPT_H_
#define _VTSS_IPT_H_

#include "vtss_autoconf.h"
#include "transport.h"

/**
// Data Types and Macros
*/

/*
// IPT macro definitions
*/
#define IPT_CONTROL_MSR  0x570
#define IPT_STATUS_MSR   0x571
#define IPT_OUT_BASE_MSR 0x560
#define IPT_OUT_MASK_MSR 0x561

#define IPT_BUF_SIZE     0x1000

/*
// IPT structures
*/

#pragma pack(push, 1)

typedef struct
{
    /// |63:12 PhysAddr|11:10 rsvd|9:6 size|5|4: stop|3|2: int|1|0: end
    unsigned long long entry[1];

} topa_t;

typedef struct
{
    unsigned int flagword;
    unsigned int residx;
    unsigned int cpuidx;
    long long cputsc;
    unsigned short size;
    unsigned short type;

} ipt_trace_record_t;

#pragma pack(pop)

/**
// Function Declarations
*/

int vtss_ipt_init(void);
void vtss_ipt_fini(void);
int vtss_has_ipt_overflowed(void);
void vtss_enable_ipt(void);
void vtss_disable_ipt(void);
void vtss_dump_ipt(struct vtss_transport_data* trnd, int tidx, int cpu, int is_safe);

#endif /* _VTSS_IPT_H_ */
