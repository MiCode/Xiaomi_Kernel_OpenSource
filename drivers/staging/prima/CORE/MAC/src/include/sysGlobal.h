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

#ifndef __SYS_GLOBAL_H__
#define __SYS_GLOBAL_H__

typedef struct sAniSirSys
{
    tANI_U32 abort; /* system is aborting and will be unloaded, only MMH thread is running */

    tANI_U32 gSysFrameCount[4][16];
    tANI_U32 gSysBbtReceived;
    tANI_U32 gSysBbtPostedToLim;
    tANI_U32 gSysBbtPostedToSch;
    tANI_U32 gSysBbtPostedToPmm;
    tANI_U32 gSysBbtPostedToHal;
    tANI_U32 gSysBbtDropped;
    tANI_U32 gSysBbtNonLearnFrameInv;
    tANI_U32 gSysBbtLearnFrameInv;
    tANI_U32 gSysBbtCrcFail;
    tANI_U32 gSysBbtDuplicates;
    tANI_U32 gSysReleaseCount;
    tANI_U32 probeError, probeBadSsid, probeIgnore, probeRespond;

    tANI_U32 gSysEnableLearnMode;
    tANI_U32 gSysEnableScanMode;
    tANI_U32 gSysEnableLinkMonitorMode;
} tAniSirSys, *tpAniSirSys;

#endif
