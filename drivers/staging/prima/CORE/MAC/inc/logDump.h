/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*============================================================================
Copyright (c) 2007 Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Confidential and Proprietary

logDump.h

Provides api's for dump commands.

Author:    Santosh Mandiganal
Date:      04/06/2008
============================================================================*/


#ifndef __LOGDUMP_H__
#define __LOGDUMP_H__

#define MAX_DUMP_CMD            999
#define MAX_DUMP_TABLE_ENTRY    10

typedef char * (*tpFunc)(tpAniSirGlobal, tANI_U32, tANI_U32, tANI_U32, tANI_U32, char *);

typedef struct sDumpFuncEntry  {
    tANI_U32    id;
    char       *description;
    tpFunc      func;
} tDumpFuncEntry;

typedef struct sDumpModuleEntry  {
    tANI_U32    mindumpid;
    tANI_U32    maxdumpid;
    tANI_U32    nItems;
    tDumpFuncEntry     *dumpTable;
} tDumpModuleEntry;

typedef struct sRegList {
    tANI_U32    addr;
    char       *name;
} tLogdRegList;

int log_sprintf(tpAniSirGlobal pMac, char *pBuf, char *fmt, ... );

char *
dump_log_level_set( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p);

char *
dump_cfg_set( tpAniSirGlobal pMac, tANI_U32 arg1,
              tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p);

char *
dump_cfg_get( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2,
              tANI_U32 arg3, tANI_U32 arg4, char *p);

char *
dump_cfg_group_get( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2,
                    tANI_U32 arg3, tANI_U32 arg4, char *p);

void logDumpRegisterTable( tpAniSirGlobal pMac, tDumpFuncEntry *pEntry,
                           tANI_U32   nItems );


void logDumpInit(tpAniSirGlobal pMac);

#endif /* __LOGDUMP_H__ */
