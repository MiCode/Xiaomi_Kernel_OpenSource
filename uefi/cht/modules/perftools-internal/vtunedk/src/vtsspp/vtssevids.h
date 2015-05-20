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
#ifndef _VTSSEVIDS_H_
#define _VTSSEVIDS_H_

#ifdef __cplusplus
extern "C"
{
#endif

/**
// Event ID specified through an external configuration file
*/
#define VTSS_EVID_EXTFLAG 0x80000000
#define VTSS_EVID_FIXED   0x40000000
#define VTSS_EVID_UNCORE  0x20000000

/**
// Event ID format: 32-bit integer:
//   16 most significant bits encode event type,
//   16 least significant bits encode event subtype
// Event type indicates a functional unit within a processor, 
//   chipset, or operating system whose parameters are to be measured.
// Event subtype enables differentiation between different events pertaining
//   to the same functional unit
*/
#define VTSS_EVTYPE_CPUCORE     1
#define VTSS_EVTYPE_CPUFE       2
#define VTSS_EVTYPE_CPUEXEC     3
#define VTSS_EVTYPE_CPURETR     4
#define VTSS_EVTYPE_CPUCACHE    5
#define VTSS_EVTYPE_CPUBUS      6
#define VTSS_EVTYPE_CPUPWR      7
#define VTSS_EVTYPE_CPUIOQ      8
#define VTSS_EVTYPE_FIXED       9

#define VTSS_EVID_FIXED_INSTRUCTIONS_RETIRED (((VTSS_EVTYPE_FIXED << 16) + 0) | VTSS_EVID_FIXED)
#define VTSS_EVID_FIXED_NONHALTED_CLOCKTICKS (((VTSS_EVTYPE_FIXED << 16) + 1) | VTSS_EVID_FIXED)
#define VTSS_EVID_FIXED_NONHALTED_REFTICKS   (((VTSS_EVTYPE_FIXED << 16) + 2) | VTSS_EVID_FIXED)

#define VTSS_EVID_NONHALTED_CLOCKTICKS ((VTSS_EVTYPE_CPUCORE << 16) + 0)
#define VTSS_EVID_INSTRUCTIONS_RETIRED ((VTSS_EVTYPE_CPURETR << 16) + 0)

#define VTSS_EVID_LLCACHE_REFS   ((VTSS_EVTYPE_CPUCACHE << 16) + 0)
#define VTSS_EVID_LLCACHE_MISSES ((VTSS_EVTYPE_CPUCACHE << 16) + 1)
#define VTSS_EVID_L1CACHE_MISSES ((VTSS_EVTYPE_CPUCACHE << 16) + 2)
#define VTSS_EVID_MEMLOAD_L2MISS ((VTSS_EVTYPE_CPUCACHE << 16) + 3)

#define VTSS_EVID_BRANCHES_RETIRED ((VTSS_EVTYPE_CPURETR << 16) + 1)
#define VTSS_EVID_BRANCHES_MISPRED ((VTSS_EVTYPE_CPURETR << 16) + 2)

#define VTSS_EVID_BDR   ((VTSS_EVTYPE_CPUBUS << 16) + 0)
#define VTSS_EVID_BHITM ((VTSS_EVTYPE_CPUBUS << 16) + 1)

#define VTSS_EVID_RESSTALL_ANY ((VTSS_EVTYPE_CPUFE << 16) + 0)
#define VTSS_EVID_RSDISP_NONE  ((VTSS_EVTYPE_CPUFE << 16) + 1)

/**
// Event Modifier format: 32-bit integer, wherein two least significant bits encode 
//   the privilege level to collect events for, as follows:
//   00 - Use default event settings, no privilege level specified
//   01 - User events
//   10 - Supervisor events
//   11 - Any privilege level
*/
#define VTSS_EVMOD_DEFAULT  0x00000
#define VTSS_EVMOD_USER     0x10000
#define VTSS_EVMOD_SYSTEM   0x20000
#define VTSS_EVMOD_ALL      0x30000
#define VTSS_EVMOD_PEBS     0x80000 // event should be counted via PEBS mechanism
#define VTSS_EVMOD_CNT1    0x100000 // counter 1 used (P6 and Core families)
#define VTSS_EVMOD_CNT3    0x300000 // counter 0-3 used (Core i7 family)

#pragma pack(push, 1)

    typedef struct
    {
        unsigned int evsel:8;   /// Event selection
        unsigned int umask:8;   /// Unit mask (event modifier)

        unsigned int mode:2;    /// OS/USER mode events
        unsigned int edge:1;    /// Edge detection
        unsigned int pebs:1;    /// PEBS enable
        unsigned int cnto:3;    /// Counter offset (added to the default counter MSR index)
        unsigned int invt:1;    /// Invert comparison results

        unsigned int cmask:8;   /// Counter mask (threshold)

    } event_modifier_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif
