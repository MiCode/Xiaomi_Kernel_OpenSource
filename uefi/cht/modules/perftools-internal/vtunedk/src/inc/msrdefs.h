/*
    Copyright (C) 2011-2014 Intel Corporation.  All Rights Reserved.
 
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


#ifndef _MSRDEFS_H_
#define _MSRDEFS_H_

/*
 * Arch Perf monitoring version 3
 */
#define IA32_PMC0                   0x0C1
#define IA32_PMC1                   0x0C2
#define IA32_PMC2                   0x0C3
#define IA32_PMC3                   0x0C4
#define IA32_PMC4                   0x0C5
#define IA32_PMC5                   0x0C6
#define IA32_PMC6                   0x0C7
#define IA32_PMC7                   0x0C8
#define IA32_FULL_PMC0              0x4C1
#define IA32_FULL_PMC1              0x4C2
#define IA32_PERFEVTSEL0            0x186
#define IA32_PERFEVTSEL1            0x187
#define IA32_FIXED_CTR0             0x309
#define IA32_FIXED_CTR1             0x30A
#define IA32_FIXED_CTR2             0x30B
#define IA32_PERF_CAPABILITIES      0x345
#define IA32_FIXED_CTRL             0x38D
#define IA32_PERF_GLOBAL_STATUS     0x38E
#define IA32_PERF_GLOBAL_CTRL       0x38F
#define IA32_PERF_GLOBAL_OVF_CTRL   0x390
#define IA32_PEBS_ENABLE            0x3F1
#define IA32_MISC_ENABLE            0x1A0
#define IA32_DS_AREA                0x600
#define IA32_DEBUG_CTRL             0x1D9
#undef  IA32_LBR_FILTER_SELECT
#define IA32_LBR_FILTER_SELECT      0x1c8
#define IA32_PEBS_FRONTEND          0x3F7

#define COMPOUND_CTR_CTL            0x306
#define COMPOUND_PERF_CTR           0x307
#define COMPOUND_CTR_OVF_BIT        0x800
#define COMPOUND_CTR_OVF_SHIFT      12

#endif
