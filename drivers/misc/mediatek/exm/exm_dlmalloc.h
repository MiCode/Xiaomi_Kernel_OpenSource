/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#ifndef _EXM_DLMALLOC_H_
#define _EXM_DLMALLOC_H_

/* Configure dlmalloc. */
/* #define DLMALLOC_EXPORT static*/
#define ONLY_MSPACES 1
#define USE_LOCKS 2
#define ABORT  dump_stack()
#define HAVE_MMAP 0
#define HAVE_MREMAP 0
#define MALLOC_FAILURE_ACTION  ENOMEM
#define NO_MALLINFO 1
#define NO_MALLOC_STATS 1
#define MORECORE_CANNOT_TRIM 1

#define LACKS_UNISTD_H
#define LACKS_FCNTL_H
#define LACKS_SYS_PARAM_H
#define LACKS_SYS_MMAN_H
#define LACKS_STRINGS_H
#define LACKS_STRING_H
#define LACKS_SYS_TYPES_H
#define LACKS_ERRNO_H
#define LACKS_STDLIB_H
#define LACKS_SCHED_H
#define LACKS_TIME_H

#define KERNEL_EXTMEM_MSPACE

extern void dump_stack(void) __cold;

#endif /* _EXM_DLMALLOC_H_ */

