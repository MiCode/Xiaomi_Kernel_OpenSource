/*
    Copyright (C) 2009-2014 Intel Corporation.  All Rights Reserved.
 
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

#ifndef _SNBUNC_CBO_H_
#define _SNBUNC_CBO_H_

/*
 * Local to this architecture: SNB uncore NCU unit 
 * Arch Perf monitoring version 3
 */
// NCU/CBO MSRs
#define CBO_PERF_GLOBAL_CTRL        0x391
#define CBO_PERF_GLOBAL_STATUS      0x392

#define CBO_PERF_PMON_CONFIG        0x396

// To get the correct MSR, need to 
// take a cbo base + corresponding offset
// e.g.: CBO0_PERF_CTR0 = CBO0_BASE_MSR + CBO_PERF_CTR0

// base addr per cbo
#define CBO0_BASE_MSR               0x700
#define CBO1_BASE_MSR               0x710
#define CBO2_BASE_MSR               0x720
#define CBO3_BASE_MSR               0x730
#define CBO4_BASE_MSR               0x740

// offsets
#define CBO_PERF_EVTSEL0            0x0
#define CBO_PERF_EVTSEL1            0x1
#define CBO_PERF_EVTSEL2            0x2
#define CBO_PERF_EVTSEL3            0x3
#define CBO_PERF_UNIT_STAT          0x4
#define CBO_PERF_UNIT_CTRL          0x5
#define CBO_PERF_CTR0               0x6
#define CBO_PERF_CTR1               0x7
#define CBO_PERF_CTR2               0x8
#define CBO_PERF_CTR3               0x9

#define IA32_DEBUG_CTRL             0x1D9

extern DISPATCH_NODE  snbunc_cbo_dispatch;

#endif 
