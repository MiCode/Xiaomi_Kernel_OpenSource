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
#ifndef _VTSSTRACE_H_
#define _VTSSTRACE_H_

/**
//
// VTune Trace File Format for Stack Sampling
//
*/

/// flags to initialize the mandatory flagword member of each trace record
#define UEC_EMPTYREC    0x00000000
#define UEC_OVERFLOW    0x80000000
#define UEC_EXTENDED    0x40000000
#define UEC_VECTORED    0x20000000
#define UEC_SEQMARK     0x10000000
#define UEC_MAGIC       0x08000000

#define UEC_LEAF0       0x00000000
#define UEC_LEAF1       0x02000000
#define UEC_LEAF2       0x04000000
#define UEC_LEAF3       0x06000000

/// Leaf 0 flags (default)
#define UEC_RECSIZE     0x00000001
#define UEC_ACTIVITY    0x00000002
#define UEC_VRESIDX     0x00000004
#define UEC_CPUIDX      0x00000008
#define UEC_USRLVLID    0x00000010
#define UEC_EXTRA       0x00000020  /// may be used as MUX_GROUP for events
#define UEC_CPUTSC      0x00000040
#define UEC_CPUEVENT    0x00000080
#define UEC_REALTSC     0x00000100
#define UEC_CHPSETEV    0x00000200
#define UEC_THREADID    0x00000400
#define UEC_PROCESSID   0x00000800
#define UEC_EXECADDR    0x00001000
#define UEC_REFADDR     0x00002000
#define UEC_EXEPHYSADDR 0x00004000
#define UEC_REFPHYSADDR 0x00008000
#define UEC_PWREVENT    0x00010000
#define UEC_SYSTRACE    0x00020000
#define UEC_LARGETRACE  0x00040000
#define UEC_USERTRACE   0x00080000
#define UEC_CPURECTSC   0x00100000
#define UEC_REALRECTSC  0x00200000
#define UEC_TPADDR      0x00400000
#define UEC_TPIDX       0x00800000
#define UEC_PADDING     0x01000000

/// same specification defining L0 names
#define UECL0_RECSIZE       0x00000001
#define UECL0_ACTIVITY      0x00000002
#define UECL0_VRESIDX       0x00000004
#define UECL0_CPUIDX        0x00000008
#define UECL0_USRLVLID      0x00000010
#define UECL0_EXTRA         0x00000020
#define UECL0_CPUTSC        0x00000040
#define UECL0_CPUEVENT      0x00000080
#define UECL0_REALTSC       0x00000100
#define UECL0_CHPSETEV      0x00000200
#define UECL0_THREADID      0x00000400
#define UECL0_PROCESSID     0x00000800
#define UECL0_EXECADDR      0x00001000
#define UECL0_REFADDR       0x00002000
#define UECL0_EXEPHYSADDR   0x00004000
#define UECL0_REFPHYSADDR   0x00008000
#define UECL0_PWREVENT      0x00010000
#define UECL0_SYSTRACE      0x00020000
#define UECL0_LARGETRACE    0x00040000
#define UECL0_USERTRACE     0x00080000
#define UECL0_CPURECTSC     0x00100000
#define UECL0_REALRECTSC    0x00200000
#define UECL0_TPADDR        0x00400000
#define UECL0_TPIDX         0x00800000
#define UECL0_PADDING       0x01000000

/// Leaf 1 flags
#define UECL1_ACTIVITY      0x00000001
#define UECL1_VRESIDX       0x00000002
#define UECL1_CPUIDX        0x00000004
#define UECL1_USRLVLID      0x00000008
#define UECL1_CPUTSC        0x00000010
#define UECL1_REALTSC       0x00000020
#define UECL1_MUXGROUP      0x00000040
#define UECL1_CPUEVENT      0x00000080
#define UECL1_CHPSETEV      0x00000100
#define UECL1_OSEVENT       0x00000200
#define UECL1_EXECADDR      0x00000400
#define UECL1_REFADDR       0x00000800
#define UECL1_EXEPHYSADDR   0x00001000
#define UECL1_REFPHYSADDR   0x00002000
#define UECL1_TPIDX         0x00004000
#define UECL1_TPADDR        0x00008000
#define UECL1_PWREVENT      0x00010000
#define UECL1_CPURECTSC     0x00020000
#define UECL1_REALRECTSC    0x00040000
#define UECL1_PADDING       0x00080000
#define UECL1_REFID         0x00100000
#define UECL1_UNKNOWN1      0x00200000
#define UECL1_SYSTRACE      0x00400000
#define UECL1_LARGETRACE    0x00800000
#define UECL1_CODETRACE     0x00800000  /// compatibility synonym
#define UECL1_USERTRACE     0x01000000

#define UECL1_EXT_CPUFREQ   0x00000001

/// Leaf 2 flags
#define UECL2_GLOBALTSC     0x00000001  /// recomputed global timestamp for trace indexing purposes
#define UECL2_FILEOFFSET    0x00000002  /// trace file offset for indexing purposes
#define UECL2_LARGETRACE    0x01000000  /// large trace record to contain trace indices

/// UEC Magic Values
#define UEC_MAGICVALUE  0xaddedefa
#define UEC_MAGICUSR    0xdefadefa

/// semantic IDs for activity
#define SEMID_ACTIVITY "bit-activity"
#define SEMID_ACTMARK  "byte-actmark"

/// activity flags
#define UECACT_USERDEFINED    0x80000000
#define UECACT_SWITCHFROM     0x00000000
#define UECACT_SWITCHTO       0x00000001
#define UECACT_SWITCHREALTO   0x00000002
#define UECACT_SAMPLED        0x00000004
#define UECACT_APC            0x00000008
#define UECACT_EXCEPTION      0x00000010
#define UECACT_INTERRUPT      0x00000020
#define UECACT_PROBED         0x00000040
#define UECACT_CODETRACE      0x00000080
#define UECACT_FREQUENCY      0x00000100
#define UECACT_MODULELOAD     0x00000200
#define UECACT_MODULEUNLOAD   0x00000400
#define UECACT_TRIGGERED      0x00000800
#define UECACT_NEWTASK        0x00001000
#define UECACT_OLDTASK        0x00002000
#define UECACT_SYNCHRO        0x00004000
#define UECACT_BTSOVFLW       0x00008000
#define UECACT_NESTED         0x00010000    /// ORed with context switch activities
#define UECACT_CALLBACK       0x00020000

/// new activity mark semantics (LSB is the grouping bit)
#define UECACTMARK_GROUP          0x01  /// XORed with the activities
#define UECACTMARK_SWITCHFROM     0x02
#define UECACTMARK_SWITCHTO       0x04
#define UECACTMARK_SWITCHREALTO   0x06
#define UECACTMARK_SAMPLED        0x08
#define UECACTMARK_APC            0x0a
#define UECACTMARK_EXCEPTION      0x0c
#define UECACTMARK_INTERRUPT      0x0e
#define UECACTMARK_PROBED         0x10
#define UECACTMARK_CODETRACE      0x12
#define UECACTMARK_FREQUENCY      0x14
#define UECACTMARK_MODULELOAD     0x16
#define UECACTMARK_MODULEUNLOAD   0x18
#define UECACTMARK_TRIGGERED      0x1a
#define UECACTMARK_NEWTASK        0x1c
#define UECACTMARK_OLDTASK        0x1e
#define UECACTMARK_SYNCHRO        0x20
#define UECACTMARK_BTSOVFLW       0x22
#define UECACTMARK_CALLBACK       0x23
#define UECACTMARK_NESTED         0x40  /// ORed with context switch activities

/// systrace types
#define UECSYSTRACE_PARTIAL_RECORD   0x8000

#define UECSYSTRACE_PROCESS_NAME   0
#define UECSYSTRACE_STACK_SAMPLE32 1
#define UECSYSTRACE_STACK_SAMPLE64 2
#define UECSYSTRACE_MODULE_MAP32   3
#define UECSYSTRACE_MODULE_MAP64   4
#define UECSYSTRACE_INST_SAMPLE32  5
#define UECSYSTRACE_INST_SAMPLE64  6
#define UECSYSTRACE_STACK_INC32    7
#define UECSYSTRACE_STACK_INC64    8
#define UECSYSTRACE_STACK_EXT32    9
#define UECSYSTRACE_STACK_EXT64    10
#define UECSYSTRACE_STACK_INCEXT32 11
#define UECSYSTRACE_STACK_INCEXT64 12

#define UECSYSTRACE_STACK_CTX32_V0    13    /// full stack preceded with real esp:ebp
#define UECSYSTRACE_STACK_CTX64_V0    14    /// full stack preceded with real rsp:rbp
#define UECSYSTRACE_STACK_CTXINC32_V0 15    /// incremental stack preceded with real esp:ebp
#define UECSYSTRACE_STACK_CTXINC64_V0 16    /// incremental stack preceded with real rsp:rbp

#define UECSYSTRACE_SWCFG  17   /// software configuration record
#define UECSYSTRACE_HWCFG  18   /// hardware configuration record
#define UECSYSTRACE_FMTCFG 19   /// forward compatibility format record

#define UECSYSTRACE_BRANCH_V0 20    /// branch trace record

#define UECSYSTRACE_REGCTX32      21    /// register context for IA32 (PEBS)
#define UECSYSTRACE_REGCTX32E     22    /// register context for EM64T (PEBS)
#define UECSYSTRACE_REGCTX32_VEC  23    /// a vector of register contexts for IA32 (PEBS)
#define UECSYSTRACE_REGCTX32E_VEC 24    /// a vector of register contexts for EM64T (PEBS)

#define UECSYSTRACE_INDEX_LOCATOR 25    /// a record to locate trace indices
#define UECSYSTRACE_INDEX_STREAM  26    /// a stream of trace index records

#define UECSYSTRACE_CLEAR_STACK32 27    /// 32-bit call stack sequence
#define UECSYSTRACE_CLEAR_STACK64 28    /// 64-bit call stack sequence

#define UECSYSTRACE_EXECTX_V0     29    /// execution context for IA32/EM64T (register and memory contents)

#define UECSYSTRACE_LOAD_JIT32   30
#define UECSYSTRACE_UNLOAD_JIT32 31
#define UECSYSTRACE_LOAD_JIT64   32
#define UECSYSTRACE_UNLOAD_JIT64 33

#define UECSYSTRACE_STACK_CTX32_V1    40    /// full stack preceded with esp:bottom (instead of fp)
#define UECSYSTRACE_STACK_CTX64_V1    41    /// full stack preceded with  rsp:bottom (instead of fp)
#define UECSYSTRACE_STACK_CTXINC32_V1 42    /// incremental stack preceded with  esp:bottom (instead of fp)
#define UECSYSTRACE_STACK_CTXINC64_V1 43    /// incremental stack preceded with  rsp:bottom (instead of fp)

#define UECSYSTRACE_COLCFG  44  /// collector configuration record
#define UECSYSTRACE_SYSINFO 45  /// system information record

#define UECSYSTRACE_STACK_CTX32_V2    46    /// full stack without sp and fp values (both equal exectx.sp)
#define UECSYSTRACE_STACK_CTX64_V2    47    /// full stack without sp and fp values (both equal exectx.sp)
#define UECSYSTRACE_STACK_CTXINC32_V2 48    /// incremental stack without sp and fp values (both equal exectx.sp)
#define UECSYSTRACE_STACK_CTXINC64_V2 49    /// incremental stack without sp and fp values (both equal exectx.sp)

#define UECSYSTRACE_STREAM_ZLIB 50          /// a record containing a stream compressed with ZLIB
#define UECSYSTRACE_DEBUG       60          /// a record with debugging info in a human-readable format
#define UECSYSTRACE_IPT         35          /// a record with a raw IPT data stream

/// module types for for systrace(module map)
#define MODTYPE_ELF      0x00   /// default Linux module type
#define MODTYPE_COFF     0x01   /// default Windows module type
#define MODTYPE_BIN      0x02   /// any non-structured executable region
#define MODTYPE_JIT_FLAG 0x80   /// should be ORed with the actial JITted module type

// Pre-defined User Record Types
#define URT_PARTIAL_RECORD   0x8000      /// indicates the next subsequent record
                                         /// should be appended to the current one

#define URT_CALLSTACK_DATA   0x0000      /// a sequence of function IDs / addresses
#define URT_FUNCMODID_MAP    0x0001      /// a map of IDs to functions and modules
#define URT_ALTSTREAM        0x0002      /// wraps an alternative stream of user-level data
                                         /// all records of this type should be concatenated
                                         /// and parsed as a separate stream
#define URT_IMPORTS32        0x0003      /// recorded info on instrumented import functions
#define URT_IMPORTS64        0x0004      /// recorded info on instrumented import functions
#define URT_APIWRAP32_V0     0x0005      /// recorded info on instrumented API functions
#define URT_APIWRAP64_V0     0x0006      /// recorded info on instrumented API functions
#define URT_APIWRAP32_V1     0x0007      /// recorded info on instrumented API functions
#define URT_APIWRAP64_V1     0x0008      /// recorded info on instrumented API functions
#define URT_APIWRAP32_V2     0x0009      /// recorded info on instrumented API functions
#define URT_APIWRAP64_V2     0x000a      /// recorded info on instrumented API functions

#define URT_DEBUG            60          /// a record with debugging info in a human-readable format

// Pre-defined system function IDs
#define FID_THREAD_NAME      0x11e       /// thread name
#define FID_GENERIC_SYSCALL  0x100000    /// system calls tracked by the driver

#endif /* _VTSSTRACE_H_ */
