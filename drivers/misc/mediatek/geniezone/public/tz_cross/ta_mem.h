/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


/** Commands for TA memory **/

#ifndef __TRUSTZONE_TA_MEM__
#define __TRUSTZONE_TA_MEM__

#define TZ_TA_MEM_UUID "4477588a-8476-11e2-ad15-e41f1390d676"

/* Command for Secure Memory Management */
#define TZCMD_MEM_SHAREDMEM_REG 0
#define TZCMD_MEM_SHAREDMEM_UNREG 1
#define TZCMD_MEM_SECUREMEM_ALLOC 2
#define TZCMD_MEM_SECUREMEM_REF 3
#define TZCMD_MEM_SECUREMEM_UNREF 4
#define TZCMD_MEM_SECURECM_ALLOC 5
#define TZCMD_MEM_SECURECM_REF 6
#define TZCMD_MEM_SECURECM_UNREF 7
#define TZCMD_MEM_SECURECM_RELEASE 8
#define TZCMD_MEM_SECURECM_APPEND 9
#define TZCMD_MEM_SECURECM_READ 10
#define TZCMD_MEM_SECURECM_WRITE 11
#define TZCMD_MEM_SECURECM_RSIZE 12
#define TZCMD_MEM_TOTAL_SIZE 13
#define TZCMD_MEM_SECUREMEM_ZALLOC 14
#define TZCMD_MEM_SECURECM_ZALLOC 15
#define TZCMD_MEM_RELEASE_SECURECM 16
#define TZCMD_MEM_SECUREMEM_ALLOC_WITH_TAG 17
#define TZCMD_MEM_SECURECM_ALLOC_WITH_TAG 18
#define TZCMD_MEM_SECUREMEM_ZALLOC_WITH_TAG 19
#define TZCMD_MEM_SECURECM_ZALLOC_WITH_TAG 20
#define TZCMD_MEM_SHAREDMEM_REG_WITH_TAG 21
#define TZCMD_MEM_USAGE_SECURECM 22
#define TZCMD_MEM_APPEND_MULTI_CHUNKMEM 23
#define TZCMD_MEM_RELEASE_CHUNKMEM 24
#define TZCMD_MEM_SECUREMULTICHUNKMEM_ALLOC 25
#define TZCMD_MEM_SECUREMULTICHUNKMEM_ZALLOC 26
#define TZCMD_MEM_SECUREMULTICHUNKMEM_REF 27
#define TZCMD_MEM_SECUREMULTICHUNKMEM_UNREF 28
#define TZCMD_MEM_Query_IONHandle 29

#define TZCMD_MEM_APPEND_MULTI_CHUNKMEM_ION 31
#define TZCMD_MEM_RELEASE_CHUNKMEM_ION 32
#define TZCMD_MEM_CONFIG_CHUNKMEM_INFO_ION 34
#define TZCMD_MEM_Query_SECUREMEM_INFO 35

/* data structure for parameter passing */
struct shm_buffer_s {
	unsigned long long buffer;
	unsigned long long size;
};

#endif /* __TRUSTZONE_TA_MEM__ */
