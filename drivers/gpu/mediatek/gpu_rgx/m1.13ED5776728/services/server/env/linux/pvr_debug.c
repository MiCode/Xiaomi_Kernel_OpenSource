/*************************************************************************/ /*!
@File
@Title          Debug Functionality
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Provides kernel side Debug Functionality.
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/hardirq.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <stdarg.h>

#include "allocmem.h"
#include "pvrversion.h"
#include "img_types.h"
#include "img_defs.h"
#include "servicesext.h"
#include "pvr_debug.h"
#include "srvkm.h"
#include "linkage.h"
#include "pvr_uaccess.h"
#include "pvrsrv.h"
#include "lists.h"
#include "osfunc.h"
#include "mtk_version.h"
#include "di_server.h"

#include "rgx_options.h"

#if defined(SUPPORT_RGX)
#include "rgxdevice.h"
#include "rgxdebug.h"
#include "rgxinit.h"
#include "rgxfwutils.h"
#include "sofunc_rgx.h"
/* Handle used by DebugFS to get GPU utilisation stats */
static IMG_HANDLE ghGpuUtilUserDebugFS;
#endif

#if defined(PVRSRV_NEED_PVR_DPF)

/******** BUFFERED LOG MESSAGES ********/

/* Because we don't want to have to handle CCB wrapping, each buffered
 * message is rounded up to PVRSRV_DEBUG_CCB_MESG_MAX bytes. This means
 * there is the same fixed number of messages that can be stored,
 * regardless of message length.
 */

#if defined(PVRSRV_DEBUG_CCB_MAX)

#define PVRSRV_DEBUG_CCB_MESG_MAX	PVR_MAX_DEBUG_MESSAGE_LEN

#include <linux/syscalls.h>
#include <linux/time.h>

typedef struct
{
	const IMG_CHAR *pszFile;
	IMG_INT iLine;
	IMG_UINT32 ui32TID;
	IMG_UINT32 ui32PID;
	IMG_CHAR pcMesg[PVRSRV_DEBUG_CCB_MESG_MAX];
	struct timeval sTimeVal;
}
PVRSRV_DEBUG_CCB;

static PVRSRV_DEBUG_CCB gsDebugCCB[PVRSRV_DEBUG_CCB_MAX];

static IMG_UINT giOffset;

/* protects access to gsDebugCCB */
static DEFINE_SPINLOCK(gsDebugCCBLock);

static void
AddToBufferCCB(const IMG_CHAR *pszFileName, IMG_UINT32 ui32Line,
			   const IMG_CHAR *szBuffer)
{
	unsigned long uiFlags;

	spin_lock_irqsave(&gsDebugCCBLock, uiFlags);

	gsDebugCCB[giOffset].pszFile = pszFileName;
	gsDebugCCB[giOffset].iLine   = ui32Line;
	gsDebugCCB[giOffset].ui32TID = current->pid;
	gsDebugCCB[giOffset].ui32PID = current->tgid;

	do_gettimeofday(&gsDebugCCB[giOffset].sTimeVal);

	OSStringLCopy(gsDebugCCB[giOffset].pcMesg, szBuffer,
	              PVRSRV_DEBUG_CCB_MESG_MAX);

	giOffset = (giOffset + 1) % PVRSRV_DEBUG_CCB_MAX;

	spin_unlock_irqrestore(&gsDebugCCBLock, uiFlags);
}

void PVRSRVDebugPrintfDumpCCB(void)
{
	int i;
	unsigned long uiFlags;

	spin_lock_irqsave(&gsDebugCCBLock, uiFlags);

	for (i = 0; i < PVRSRV_DEBUG_CCB_MAX; i++)
	{
		PVRSRV_DEBUG_CCB *psDebugCCBEntry =
			&gsDebugCCB[(giOffset + i) % PVRSRV_DEBUG_CCB_MAX];

		/* Early on, we won't have PVRSRV_DEBUG_CCB_MAX messages */
		if (!psDebugCCBEntry->pszFile)
		{
			continue;
		}

		printk(KERN_ERR "%s:%d: (%ld.%ld, tid=%u, pid=%u) %s\n",
			   psDebugCCBEntry->pszFile,
			   psDebugCCBEntry->iLine,
			   (long)psDebugCCBEntry->sTimeVal.tv_sec,
			   (long)psDebugCCBEntry->sTimeVal.tv_usec,
			   psDebugCCBEntry->ui32TID,
			   psDebugCCBEntry->ui32PID,
			   psDebugCCBEntry->pcMesg);

		/* Clear this entry so it doesn't get printed the next time again. */
		psDebugCCBEntry->pszFile = NULL;
	}

	spin_unlock_irqrestore(&gsDebugCCBLock, uiFlags);
}

#else /* defined(PVRSRV_DEBUG_CCB_MAX) */

static INLINE void
AddToBufferCCB(const IMG_CHAR *pszFileName, IMG_UINT32 ui32Line,
			   const IMG_CHAR *szBuffer)
{
	(void)pszFileName;
	(void)szBuffer;
	(void)ui32Line;
}

void PVRSRVDebugPrintfDumpCCB(void)
{
	/* Not available */
}

#endif /* defined(PVRSRV_DEBUG_CCB_MAX) */

#endif /* defined(PVRSRV_NEED_PVR_DPF) */

#if defined(PVRSRV_NEED_PVR_DPF)

#define PVR_MAX_FILEPATH_LEN 256

#if !defined(PVR_TESTING_UTILS)
static
#endif
IMG_UINT32 gPVRDebugLevel =
	(
	 DBGPRIV_FATAL | DBGPRIV_ERROR | DBGPRIV_WARNING

#if defined(PVRSRV_DEBUG_CCB_MAX)
	 | DBGPRIV_BUFFERED
#endif /* defined(PVRSRV_DEBUG_CCB_MAX) */

#if defined(PVR_DPF_ADHOC_DEBUG_ON)
	 | DBGPRIV_DEBUG
#endif /* defined(PVR_DPF_ADHOC_DEBUG_ON) */
	);

module_param(gPVRDebugLevel, uint, 0644);
MODULE_PARM_DESC(gPVRDebugLevel,
				 "Sets the level of debug output (default 0x7)");

#endif /* defined(PVRSRV_NEED_PVR_DPF) || defined(PVRSRV_NEED_PVR_TRACE) */

#define	PVR_MAX_MSG_LEN PVR_MAX_DEBUG_MESSAGE_LEN

/* Message buffer for messages */
static IMG_CHAR gszBuffer[PVR_MAX_MSG_LEN + 1];

/* The lock is used to control access to gszBuffer */
static DEFINE_SPINLOCK(gsDebugLock);

/*
 * Append a string to a buffer using formatted conversion.
 * The function takes a variable number of arguments, pointed
 * to by the var args list.
 */
__printf(3, 0)
static IMG_BOOL VBAppend(IMG_CHAR *pszBuf, IMG_UINT32 ui32BufSiz, const IMG_CHAR *pszFormat, va_list VArgs)
{
	IMG_UINT32 ui32Used;
	IMG_UINT32 ui32Space;
	IMG_INT32 i32Len;

	ui32Used = OSStringLength(pszBuf);
	BUG_ON(ui32Used >= ui32BufSiz);
	ui32Space = ui32BufSiz - ui32Used;

	i32Len = vsnprintf(&pszBuf[ui32Used], ui32Space, pszFormat, VArgs);
	pszBuf[ui32BufSiz - 1] = 0;

	/* Return true if string was truncated */
	return i32Len < 0 || i32Len >= (IMG_INT32)ui32Space;
}

/*************************************************************************/ /*!
@Function       PVRSRVReleasePrintf
@Description    To output an important message to the user in release builds
@Input          pszFormat   The message format string
@Input          ...         Zero or more arguments for use by the format string
*/ /**************************************************************************/
void PVRSRVReleasePrintf(const IMG_CHAR *pszFormat, ...)
{
	va_list vaArgs;
	unsigned long ulLockFlags = 0;
	IMG_CHAR *pszBuf = gszBuffer;
	IMG_UINT32 ui32BufSiz = sizeof(gszBuffer);
	IMG_INT32  result;

	va_start(vaArgs, pszFormat);

	spin_lock_irqsave(&gsDebugLock, ulLockFlags);

	result = snprintf(pszBuf, (ui32BufSiz - 2), "PVR_K:  %u: ", current->pid);
	PVR_ASSERT(result>0);
	ui32BufSiz -= result;

	if (VBAppend(pszBuf, ui32BufSiz, pszFormat, vaArgs))
	{
		printk(KERN_INFO "%s (truncated)\n", pszBuf);
	}
	else
	{
		printk(KERN_INFO "%s\n", pszBuf);
	}

	spin_unlock_irqrestore(&gsDebugLock, ulLockFlags);
	va_end(vaArgs);
}

#if defined(PVRSRV_NEED_PVR_TRACE)

/*************************************************************************/ /*!
@Function       PVRTrace
@Description    To output a debug message to the user
@Input          pszFormat   The message format string
@Input          ...         Zero or more arguments for use by the format string
*/ /**************************************************************************/
void PVRSRVTrace(const IMG_CHAR *pszFormat, ...)
{
	va_list VArgs;
	unsigned long ulLockFlags = 0;
	IMG_CHAR *pszBuf = gszBuffer;
	IMG_UINT32 ui32BufSiz = sizeof(gszBuffer);
	IMG_INT32  result;

	va_start(VArgs, pszFormat);

	spin_lock_irqsave(&gsDebugLock, ulLockFlags);

	result = snprintf(pszBuf, (ui32BufSiz - 2), "PVR: %u: ", current->pid);
	PVR_ASSERT(result>0);
	ui32BufSiz -= result;

	if (VBAppend(pszBuf, ui32BufSiz, pszFormat, VArgs))
	{
		printk(KERN_ERR "PVR_K:(Message Truncated): %s\n", pszBuf);
	}
	else
	{
		printk(KERN_ERR "%s\n", pszBuf);
	}

	spin_unlock_irqrestore(&gsDebugLock, ulLockFlags);

	va_end(VArgs);
}

#endif /* defined(PVRSRV_NEED_PVR_TRACE) */

#if defined(PVRSRV_NEED_PVR_DPF)

/*
 * Append a string to a buffer using formatted conversion.
 * The function takes a variable number of arguments, calling
 * VBAppend to do the actual work.
 */
__printf(3, 4)
static IMG_BOOL BAppend(IMG_CHAR *pszBuf, IMG_UINT32 ui32BufSiz, const IMG_CHAR *pszFormat, ...)
{
	va_list VArgs;
	IMG_BOOL bTrunc;

	va_start (VArgs, pszFormat);

	bTrunc = VBAppend(pszBuf, ui32BufSiz, pszFormat, VArgs);

	va_end (VArgs);

	return bTrunc;
}

/*************************************************************************/ /*!
@Function       PVRSRVDebugPrintf
@Description    To output a debug message to the user
@Input          uDebugLevel The current debug level
@Input          pszFile     The source file generating the message
@Input          uLine       The line of the source file
@Input          pszFormat   The message format string
@Input          ...         Zero or more arguments for use by the format string
*/ /**************************************************************************/
void PVRSRVDebugPrintf(IMG_UINT32 ui32DebugLevel,
			   const IMG_CHAR *pszFullFileName,
			   IMG_UINT32 ui32Line,
			   const IMG_CHAR *pszFormat,
			   ...)
{
	const IMG_CHAR *pszFileName = pszFullFileName;
	IMG_CHAR *pszLeafName;
	va_list vaArgs;
	unsigned long ulLockFlags = 0;
	IMG_CHAR *pszBuf = gszBuffer;
	IMG_UINT32 ui32BufSiz = sizeof(gszBuffer);

	if (!(gPVRDebugLevel & ui32DebugLevel))
	{
		return;
	}

	va_start(vaArgs, pszFormat);

	spin_lock_irqsave(&gsDebugLock, ulLockFlags);

	switch (ui32DebugLevel)
	{
		case DBGPRIV_FATAL:
		{
			OSStringLCopy(pszBuf, "PVR_K:(Fatal): ", ui32BufSiz);
			break;
		}
		case DBGPRIV_ERROR:
		{
			OSStringLCopy(pszBuf, "PVR_K:(Error): ", ui32BufSiz);
			break;
		}
		case DBGPRIV_WARNING:
		{
			OSStringLCopy(pszBuf, "PVR_K:(Warn):  ", ui32BufSiz);
			break;
		}
		case DBGPRIV_MESSAGE:
		{
			OSStringLCopy(pszBuf, "PVR_K:(Mesg):  ", ui32BufSiz);
			break;
		}
		case DBGPRIV_VERBOSE:
		{
			OSStringLCopy(pszBuf, "PVR_K:(Verb):  ", ui32BufSiz);
			break;
		}
		case DBGPRIV_DEBUG:
		{
			OSStringLCopy(pszBuf, "PVR_K:(Debug): ", ui32BufSiz);
			break;
		}
		case DBGPRIV_CALLTRACE:
		case DBGPRIV_ALLOC:
		case DBGPRIV_BUFFERED:
		default:
		{
			OSStringLCopy(pszBuf, "PVR_K: ", ui32BufSiz);
			break;
		}
	}

	if (current->pid == task_tgid_nr(current))
	{
		(void) BAppend(pszBuf, ui32BufSiz, "%5u: ", current->pid);
	}
	else
	{
		(void) BAppend(pszBuf, ui32BufSiz, "%5u-%5u: ", task_tgid_nr(current) /* pid id of group*/, current->pid /* task id */);
	}

	if (VBAppend(pszBuf, ui32BufSiz, pszFormat, vaArgs))
	{
		printk(KERN_ERR "%s (truncated)\n", pszBuf);
	}
	else
	{
		IMG_BOOL bTruncated = IMG_FALSE;

#if !defined(__sh__)
		pszLeafName = (IMG_CHAR *)strrchr (pszFileName, '/');

		if (pszLeafName)
		{
			pszFileName = pszLeafName+1;
		}
#endif /* __sh__ */

#if defined(DEBUG)
		{
			static const IMG_CHAR *lastFile;

			if (lastFile == pszFileName)
			{
				bTruncated = BAppend(pszBuf, ui32BufSiz, " [%u]", ui32Line);
			}
			else
			{
				bTruncated = BAppend(pszBuf, ui32BufSiz, " [%s:%u]", pszFileName, ui32Line);
				lastFile = pszFileName;
			}
		}
#else
		bTruncated = BAppend(pszBuf, ui32BufSiz, " [%u]", ui32Line);
#endif

		if (bTruncated)
		{
			printk(KERN_ERR "%s (truncated)\n", pszBuf);
		}
		else
		{
			if (ui32DebugLevel & DBGPRIV_BUFFERED)
			{
				AddToBufferCCB(pszFileName, ui32Line, pszBuf);
			}
			else
			{
				printk(KERN_ERR "%s\n", pszBuf);
			}
		}
	}

	spin_unlock_irqrestore(&gsDebugLock, ulLockFlags);

	va_end (vaArgs);
}

#endif /* PVRSRV_NEED_PVR_DPF */


/*************************************************************************/ /*!
 Version DebugFS entry
*/ /**************************************************************************/

static void *_DebugVersionCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode,
                                          va_list va)
{
	IMG_UINT64 *pui64CurrentPosition = va_arg(va, IMG_UINT64 *);
	IMG_UINT64 ui64Position = va_arg(va, IMG_UINT64);
	IMG_UINT64 ui64CurrentPosition = *pui64CurrentPosition;

	(*pui64CurrentPosition)++;

	return (ui64CurrentPosition == ui64Position) ? psDevNode : NULL;
}

static void *_VersionStartOp(OSDI_IMPL_ENTRY *psEntry, IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	if (psPVRSRVData == NULL) {
		PVR_DPF((PVR_DBG_ERROR, "psPVRSRVData = NULL"));
		return NULL;
	}

	if (*pui64Pos == 0)
	{
		return DI_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugVersionCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _VersionStopOp(OSDI_IMPL_ENTRY *psEntry, void *pvPriv)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvPriv);
}

static void *_VersionNextOp(OSDI_IMPL_ENTRY *psEntry,void *pvPriv,
                            IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	(*pui64Pos)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
	                                      _DebugVersionCompare_AnyVaCb,
	                                      &uiCurrentPosition,
	                                      *pui64Pos);
}

#define DI_PRINT_VERSION_FMTSPEC "%s Version: %u.%u @ %u (%s) build options: 0x%08x %s\n"
#define STR_DEBUG   "debug"
#define STR_RELEASE "release"

#if !defined(PVR_ARCH_NAME)
#define PVR_ARCH_NAME "Unknown"
#endif

static int _VersionShowOp(OSDI_IMPL_ENTRY *psEntry, void *pvPriv)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);

	if (pvPriv == DI_START_TOKEN)
	{
		if (psPVRSRVData->sDriverInfo.bIsNoMatch)
		{
			const BUILD_INFO *psBuildInfo;

			psBuildInfo = &psPVRSRVData->sDriverInfo.sUMBuildInfo;
			DIPrintf(psEntry, DI_PRINT_VERSION_FMTSPEC,
			         "UM Driver",
			         PVRVERSION_UNPACK_MAJ(psBuildInfo->ui32BuildVersion),
			         PVRVERSION_UNPACK_MIN(psBuildInfo->ui32BuildVersion),
			         psBuildInfo->ui32BuildRevision,
			         (psBuildInfo->ui32BuildType == BUILD_TYPE_DEBUG) ? STR_DEBUG : STR_RELEASE,
			         psBuildInfo->ui32BuildOptions,
			         PVR_BUILD_DIR);

			psBuildInfo = &psPVRSRVData->sDriverInfo.sKMBuildInfo;
			DIPrintf(psEntry, DI_PRINT_VERSION_FMTSPEC,
			         "KM Driver (" PVR_ARCH_NAME ")",
			         PVRVERSION_UNPACK_MAJ(psBuildInfo->ui32BuildVersion),
			         PVRVERSION_UNPACK_MIN(psBuildInfo->ui32BuildVersion),
			         psBuildInfo->ui32BuildRevision,
			         (psBuildInfo->ui32BuildType == BUILD_TYPE_DEBUG) ? STR_DEBUG : STR_RELEASE,
			         psBuildInfo->ui32BuildOptions,
			         PVR_BUILD_DIR);
		}
		else
		{
			/* bIsNoMatch is `false` in one of the following cases:
			 * - UM & KM version parameters actually match.
			 * - A comparison between UM & KM has not been made yet, because no
			 *   client ever connected.
			 *
			 * In both cases, available (KM) version info is the best output we
			 * can provide.
			 */
			DIPrintf(psEntry, "Driver Version: %s (%s) (%s) build options: "
			         "0x%08lx %s\n", PVRVERSION_STRING, PVR_ARCH_NAME,
			         PVR_BUILD_TYPE, RGX_BUILD_OPTIONS_KM, PVR_BUILD_DIR);
			DIPrintf(psEntry, "MTK Version String: %s\n", MTK_DEBUG_VERSION_STR);
		}
	}
	else if (pvPriv != NULL)
	{
		PVRSRV_DEVICE_NODE *psDevNode = (PVRSRV_DEVICE_NODE *) pvPriv;
		PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;
#if defined(SUPPORT_RGX)
		PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
#endif
		IMG_BOOL bFwVersionInfoPrinted = IMG_FALSE;

		DIPrintf(psEntry, "\nDevice Name: %s\n", psDevConfig->pszName);

		if (psDevConfig->pszVersion)
		{
			DIPrintf(psEntry, "Device Version: %s\n",
			          psDevConfig->pszVersion);
		}

		if (psDevNode->pfnDeviceVersionString)
		{
			IMG_CHAR *pszDeviceVersionString;

			if (psDevNode->pfnDeviceVersionString(psDevNode, &pszDeviceVersionString) == PVRSRV_OK)
			{
				DIPrintf(psEntry, "%s\n", pszDeviceVersionString);

				OSFreeMem(pszDeviceVersionString);
			}
		}
#if defined(SUPPORT_RGX)
		/* print device's firmware version info */
		if (psDevInfo->psRGXFWIfOsInitMemDesc != NULL)
		{
			/* psDevInfo->psRGXFWIfOsInitMemDesc should be permanently mapped */
			if (psDevInfo->psRGXFWIfOsInit != NULL)
			{
				if (psDevInfo->psRGXFWIfOsInit->sRGXCompChecks.bUpdated)
				{
					const RGXFWIF_COMPCHECKS *psRGXCompChecks =
					        &psDevInfo->psRGXFWIfOsInit->sRGXCompChecks;

					DIPrintf(psEntry, DI_PRINT_VERSION_FMTSPEC,
					         "Firmware",
					         PVRVERSION_UNPACK_MAJ(psRGXCompChecks->ui32DDKVersion),
					         PVRVERSION_UNPACK_MIN(psRGXCompChecks->ui32DDKVersion),
					         psRGXCompChecks->ui32DDKBuild,
					         ((psRGXCompChecks->ui32BuildOptions & OPTIONS_DEBUG_MASK) ?
					          STR_DEBUG : STR_RELEASE),
					         psRGXCompChecks->ui32BuildOptions,
					         PVR_BUILD_DIR);
					bFwVersionInfoPrinted = IMG_TRUE;
				}
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Error acquiring CPU virtual "
				        "address of FWInitMemDesc", __func__));
			}
		}
#endif

		if (!bFwVersionInfoPrinted)
		{
			DIPrintf(psEntry, "Firmware Version: Info unavailable %s\n",
#if defined(NO_HARDWARE)
			         "on NoHW driver"
#else
			         "(Is INIT complete?)"
#endif
			         );
		}
	}

	return 0;
}

#if defined(SUPPORT_RGX) && defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
/*************************************************************************/ /*!
 Power data DebugFS entry
*/ /**************************************************************************/

static void *_DebugPowerDataCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode,
					  va_list va)
{
	IMG_UINT64 *pui64CurrentPosition = va_arg(va, IMG_UINT64 *);
	IMG_UINT64 ui64Position = va_arg(va, IMG_UINT64;
	IMG_UINT64 ui64CurrentPosition = *pui64CurrentPosition;

	(*puiCurrentPosition)++;

	return (ui64CurrentPosition == ui64Position) ? psDevNode : NULL;
}

static void *_DebugPowerDataDIStart(OSDI_IMPL_ENTRY *psEntry,
									 IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 0;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugPowerDataCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _DebugPowerDataDIStop(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_DebugPowerDataDINext(OSDI_IMPL_ENTRY *psEntry,
									void *pvData,
									IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 0;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*pui64Pos)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugPowerDataCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static PVRSRV_ERROR SendPowerCounterCommand(PVRSRV_DEVICE_NODE* psDeviceNode,
											RGXFWIF_COUNTER_DUMP_REQUEST eRequestType,
											IMG_UINT32 *pui32kCCBCommandSlot)
{
	PVRSRV_ERROR eError;

	RGXFWIF_KCCB_CMD sCounterDumpCmd;

	sCounterDumpCmd.eCmdType = RGXFWIF_KCCB_CMD_COUNTER_DUMP;
	sCounterDumpCmd.uCmdData.sCounterDumpConfigData.eCounterDumpRequest = eRequestType;

	eError = RGXScheduleCommandAndGetKCCBSlot(psDeviceNode->pvDevice,
				RGXFWIF_DM_GP,
				&sCounterDumpCmd,
				0,
				PDUMP_FLAGS_CONTINUOUS,
				pui32kCCBCommandSlot);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "SendPowerCounterCommand: RGXScheduleCommandAndGetKCCBSlot failed. Error:%u", eError));
	}

	return eError;
}

static void *_IsDevNodeNotInitialised(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	return psDeviceNode->eDevState == PVRSRV_DEVICE_STATE_ACTIVE ? NULL : psDeviceNode;
}

static void _SendPowerCounterCommand(PVRSRV_DEVICE_NODE* psDeviceNode,
									 va_list va)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	IMG_UINT32         ui32kCCBCommandSlot;

	OSLockAcquire(psDevInfo->hCounterDumpingLock);

	SendPowerCounterCommand(psDeviceNode, va_arg(va, RGXFWIF_COUNTER_DUMP_REQUEST), &ui32kCCBCommandSlot);

	OSLockRelease(psDevInfo->hCounterDumpingLock);
}

static int _DebugPowerDataDIShow(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	IMG_UINT32 ui32kCCBCommandSlot;
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (pvData != NULL)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE *)pvData;
		PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

		if (psDeviceNode->eDevState != PVRSRV_DEVICE_STATE_ACTIVE)
		{
			PVR_DPF((PVR_DBG_ERROR, "Not all device nodes were initialised when power counter data was requested!"));
			return -EIO;
		}

		OSLockAcquire(psDevInfo->hCounterDumpingLock);

		eError = SendPowerCounterCommand(psDeviceNode, RGXFWIF_PWR_COUNTER_DUMP_SAMPLE, &ui32kCCBCommandSlot);

		if (eError != PVRSRV_OK)
		{
			OSLockRelease(psDevInfo->hCounterDumpingLock);
			return -EIO;
		}

		/* Wait for FW complete completion */
		eError = RGXWaitForKCCBSlotUpdate(psDevInfo, ui32kCCBCommandSlot, PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: RGXWaitForKCCBSlotUpdate failed (%u)",
			         __func__,
			         eError));
			OSLockRelease(psDevInfo->hCounterDumpingLock);
			return -EIO;
		}

		/* Read back the buffer */
		{
			IMG_UINT32* pui32PowerBuffer;
			IMG_UINT32 ui32NumOfRegs, ui32SamplePeriod;
			IMG_UINT32 i, j;

			eError = DevmemAcquireCpuVirtAddr(psDevInfo->psCounterBufferMemDesc, (void**)&pui32PowerBuffer);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,
				         "%s: Failed to acquire buffer memory mapping (%u)",
				         __func__,
				         eError));
				OSLockRelease(psDevInfo->hCounterDumpingLock);
				return -EIO;
			}

			ui32NumOfRegs = *pui32PowerBuffer++;
			ui32SamplePeriod = *pui32PowerBuffer++;

			if (ui32NumOfRegs)
			{
				DIPrintf(psEntry, "Power counter data for device id: %d\n", psDeviceNode->sDevId.i32UMIdentifier);
				DIPrintf(psEntry, "Sample period: 0x%08x\n", ui32SamplePeriod);

				for (i = 0; i < ui32NumOfRegs; i++)
				{
					IMG_UINT32 ui32High, ui32Low;
					IMG_UINT32 ui32RegOffset = *pui32PowerBuffer++;
					IMG_UINT32 ui32NumOfInstances = *pui32PowerBuffer++;

					PVR_ASSERT(ui32NumOfInstances);

					DIPrintf(psEntry, "0x%08x:", ui32RegOffset);

					for (j = 0; j < ui32NumOfInstances; j++)
					{
						ui32Low = *pui32PowerBuffer++;
						ui32High = *pui32PowerBuffer++;

						DIPrintf(psEntry, " 0x%016llx", (IMG_UINT64)ui32Low | (IMG_UINT64)ui32High << 32);
					}

					DIPrintf(psEntry, "\n");
				}
			}

			DevmemReleaseCpuVirtAddr(psDevInfo->psCounterBufferMemDesc);
		}

		OSLockRelease(psDevInfo->hCounterDumpingLock);
	}

	return eError;
}

static IMG_INT64 PowerDataSet(const IMG_CHAR __user *pcBuffer,
                              IMG_UINT64 ui64Count, IMG_UINT64 *pui64Pos,
                              void *pvData)
{
	PVRSRV_DATA* psPVRSRVData = (PVRSRV_DATA*) pvData;
	RGXFWIF_COUNTER_DUMP_REQUEST eRequest;

	PVR_RETURN_IF_FALSE(pcBuffer != NULL, -EIO);
	PVR_RETURN_IF_FALSE(pui64Pos != NULL && *pui64Pos == 0, -EIO);
	PVR_RETURN_IF_FALSE(ui64Count >= 1, -EINVAL);
	PVR_RETURN_IF_FALSE(pcBuffer[ui64Count - 1] == '\0', -EINVAL);

	if (List_PVRSRV_DEVICE_NODE_Any(psPVRSRVData->psDeviceNodeList,
	                                _IsDevNodeNotInitialised))
	{
		PVR_DPF((PVR_DBG_ERROR, "Not all device nodes were initialised when "
		        "power counter data was requested!"));
		return -EIO;
	}

	if (pcBuffer[0] == '1')
	{
		eRequest = RGXFWIF_PWR_COUNTER_DUMP_START;
	}
	else if (pcBuffer[0] == '0')
	{
		eRequest = RGXFWIF_PWR_COUNTER_DUMP_STOP;
	}
	else
	{
		return -EINVAL;
	}

	List_PVRSRV_DEVICE_NODE_ForEach_va(psPVRSRVData->psDeviceNodeList,
									   _SendPowerCounterCommand, eRequest);

	*pui64Pos += ui64Count;
	return ui64Count;
}

#endif /* SUPPORT_RGX && SUPPORT_POWER_SAMPLING_VIA_DEBUGFS*/
/*************************************************************************/ /*!
 Status DebugFS entry
*/ /**************************************************************************/

static void *_DebugStatusCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode,
										 va_list va)
{
	IMG_UINT64 *pui64CurrentPosition = va_arg(va, IMG_UINT64 *);
	IMG_UINT64 ui64Position = va_arg(va, IMG_UINT64);
	IMG_UINT64 ui64CurrentPosition = *pui64CurrentPosition;

	(*pui64CurrentPosition)++;

	return (ui64CurrentPosition == ui64Position) ? psDevNode : NULL;
}

static void *_DebugStatusDIStart(OSDI_IMPL_ENTRY *psEntry, IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	if (*pui64Pos == 0)
	{
		return DI_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugStatusCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _DebugStatusDIStop(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_DebugStatusDINext(OSDI_IMPL_ENTRY *psEntry,
								 void *pvData,
								 IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*pui64Pos)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugStatusCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static int _DebugStatusDIShow(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	if (pvData == DI_START_TOKEN)
	{
		PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);

		if (psPVRSRVData != NULL)
		{
			switch (psPVRSRVData->eServicesState)
			{
				case PVRSRV_SERVICES_STATE_OK:
					DIPrintf(psEntry, "Driver Status:   OK\n");
					break;
				case PVRSRV_SERVICES_STATE_BAD:
					DIPrintf(psEntry, "Driver Status:   BAD\n");
					break;
				case PVRSRV_SERVICES_STATE_UNDEFINED:
					DIPrintf(psEntry, "Driver Status:   UNDEFINED\n");
					break;
				default:
					DIPrintf(psEntry, "Driver Status:   UNKNOWN (%d)\n", psPVRSRVData->eServicesState);
					break;
			}
		}
	}
	else if (pvData != NULL)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE *)pvData;
		IMG_CHAR           *pszStatus = "";
		IMG_CHAR           *pszReason = "";
		PVRSRV_DEVICE_HEALTH_STATUS eHealthStatus;
		PVRSRV_DEVICE_HEALTH_REASON eHealthReason;

		/* Update the health status now if possible... */
		if (psDeviceNode->pfnUpdateHealthStatus)
		{
			psDeviceNode->pfnUpdateHealthStatus(psDeviceNode, IMG_FALSE);
		}
		eHealthStatus = OSAtomicRead(&psDeviceNode->eHealthStatus);
		eHealthReason = OSAtomicRead(&psDeviceNode->eHealthReason);

		switch (eHealthStatus)
		{
			case PVRSRV_DEVICE_HEALTH_STATUS_OK:  pszStatus = "OK";  break;
			case PVRSRV_DEVICE_HEALTH_STATUS_NOT_RESPONDING:  pszStatus = "NOT RESPONDING";  break;
			case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:  pszStatus = "DEAD";  break;
			case PVRSRV_DEVICE_HEALTH_STATUS_FAULT:  pszStatus = "FAULT";  break;
			case PVRSRV_DEVICE_HEALTH_STATUS_UNDEFINED:  pszStatus = "UNDEFINED";  break;
			default:  pszStatus = "UNKNOWN";  break;
		}

		switch (eHealthReason)
		{
			case PVRSRV_DEVICE_HEALTH_REASON_NONE:  pszReason = "";  break;
			case PVRSRV_DEVICE_HEALTH_REASON_ASSERTED:  pszReason = " (Asserted)";  break;
			case PVRSRV_DEVICE_HEALTH_REASON_POLL_FAILING:  pszReason = " (Poll failing)";  break;
			case PVRSRV_DEVICE_HEALTH_REASON_TIMEOUTS:  pszReason = " (Global Event Object timeouts rising)";  break;
			case PVRSRV_DEVICE_HEALTH_REASON_QUEUE_CORRUPT:  pszReason = " (KCCB offset invalid)";  break;
			case PVRSRV_DEVICE_HEALTH_REASON_QUEUE_STALLED:  pszReason = " (KCCB stalled)";  break;
			case PVRSRV_DEVICE_HEALTH_REASON_IDLING:  pszReason = " (Idling)";  break;
			case PVRSRV_DEVICE_HEALTH_REASON_RESTARTING:  pszReason = " (Restarting)";  break;
			case PVRSRV_DEVICE_HEALTH_REASON_MISSING_INTERRUPTS:  pszReason = " (Missing interrupts)";  break;
			default:  pszReason = " (Unknown reason)";  break;
		}

		DIPrintf(psEntry, "Firmware Status: %s%s\n", pszStatus, pszReason);

		if (PVRSRV_VZ_MODE_IS(GUEST))
		{
			/*
			 * Guest drivers do not support the following functionality:
			 *	- Perform actual on-chip fw tracing.
			 *	- Collect actual on-chip GPU utilization stats.
			 *	- Perform actual on-chip GPU power/dvfs management.
			 *	- As a result no more information can be provided.
			 */
			return 0;
		}

		/* Write other useful stats to aid the test cycle... */
		if (psDeviceNode->pvDevice != NULL)
		{
#if defined(SUPPORT_RGX)
			PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
			RGXFWIF_HWRINFOBUF *psHWRInfoBuf = psDevInfo->psRGXFWIfHWRInfoBufCtl;
			RGXFWIF_SYSDATA *psFwSysData = psDevInfo->psRGXFWIfFwSysData;

			/* Calculate the number of HWR events in total across all the DMs... */
			if (psHWRInfoBuf != NULL)
			{
				IMG_UINT32 ui32HWREventCount = 0;
				IMG_UINT32 ui32CRREventCount = 0;
				IMG_UINT32 ui32DMIndex;

				for (ui32DMIndex = 0; ui32DMIndex < RGXFWIF_DM_MAX; ui32DMIndex++)
				{
					ui32HWREventCount += psHWRInfoBuf->aui32HwrDmLockedUpCount[ui32DMIndex];
					ui32CRREventCount += psHWRInfoBuf->aui32HwrDmOverranCount[ui32DMIndex];
				}

				DIPrintf(psEntry, "HWR Event Count: %d\n", ui32HWREventCount);
				DIPrintf(psEntry, "CRR Event Count: %d\n", ui32CRREventCount);
#if defined(PVRSRV_STALLED_CCB_ACTION)
				/* Write the number of Sync Lockup Recovery (SLR) events... */
				DIPrintf(psEntry, "SLR Event Count: %d\n", psDevInfo->psRGXFWIfFwOsData->ui32ForcedUpdatesRequested);
#endif
			}

			if (psFwSysData != NULL)
			{
				DIPrintf(psEntry, "FWF Event Count: %d\n", psFwSysData->ui32FWFaults);
			}

			/* Write the number of APM events... */
			DIPrintf(psEntry, "APM Event Count: %d\n", psDevInfo->ui32ActivePMReqTotal);

			/* Write the current GPU Utilisation values... */
			if (psDevInfo->pfnGetGpuUtilStats &&
				eHealthStatus == PVRSRV_DEVICE_HEALTH_STATUS_OK)
			{
				RGXFWIF_GPU_UTIL_STATS sGpuUtilStats;
				PVRSRV_ERROR eError = PVRSRV_OK;

				eError = psDevInfo->pfnGetGpuUtilStats(psDeviceNode,
													   ghGpuUtilUserDebugFS,
													   &sGpuUtilStats);

				if ((eError == PVRSRV_OK) &&
					((IMG_UINT32)sGpuUtilStats.ui64GpuStatCumulative))
				{
					IMG_UINT64 util;
					IMG_UINT32 rem;

					util = 100 * sGpuUtilStats.ui64GpuStatActive;
					util = OSDivide64(util, (IMG_UINT32)sGpuUtilStats.ui64GpuStatCumulative, &rem);

					DIPrintf(psEntry, "GPU Utilisation: %u%%\n", (IMG_UINT32)util);
				}
				else
				{
					DIPrintf(psEntry, "GPU Utilisation: -\n");
				}
			}
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
			/* Show the detected #LISR, #MISR scheduled calls */
			DIPrintf(psEntry, "RGX #LISR: %llu\n", psDeviceNode->ui64nLISR);
			DIPrintf(psEntry, "RGX #MISR: %llu\n", psDeviceNode->ui64nMISR);
#endif
#endif
		}
	}

	return 0;
}

static IMG_INT64 DebugStatusSet(const IMG_CHAR *pcBuffer, IMG_UINT64 ui64Count,
                                IMG_UINT64 *pui64Pos, void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	PVR_RETURN_IF_FALSE(pcBuffer != NULL, -EIO);
	PVR_RETURN_IF_FALSE(pui64Pos != NULL && *pui64Pos == 0, -EIO);
	PVR_RETURN_IF_FALSE(ui64Count >= 1, -EINVAL);
	PVR_RETURN_IF_FALSE(pcBuffer[0] == 'k' || pcBuffer[0] == 'K', -EINVAL);
	PVR_RETURN_IF_FALSE(pcBuffer[ui64Count - 1] == '\0', -EINVAL);

	psPVRSRVData->eServicesState = PVRSRV_SERVICES_STATE_BAD;

	*pui64Pos += ui64Count;
	return ui64Count;
}

/*************************************************************************/ /*!
 Dump Debug DebugFS entry
*/ /**************************************************************************/

static void *_DebugDumpDebugCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode, va_list va)
{
	IMG_UINT64 *pui64CurrentPosition = va_arg(va, IMG_UINT64 *);
	IMG_UINT64 ui64Position = va_arg(va, IMG_UINT64);
	IMG_UINT64 ui64CurrentPosition = *pui64CurrentPosition;

	(*pui64CurrentPosition)++;

	return (ui64CurrentPosition == ui64Position) ? psDevNode : NULL;
}

static void *_DebugDumpDebugDIStart(OSDI_IMPL_ENTRY *psEntry,
                                    IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	if (*pui64Pos == 0)
	{
		return DI_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugDumpDebugCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _DebugDumpDebugDIStop(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_DebugDumpDebugDINext(OSDI_IMPL_ENTRY *psEntry,
									void *pvData,
									IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*pui64Pos)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugDumpDebugCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _DumpDebugDIPrintf(void *pvDumpDebugFile,
                               const IMG_CHAR *pszFormat, ...)
{
	OSDI_IMPL_ENTRY *psEntry = pvDumpDebugFile;
	IMG_CHAR szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	va_list ArgList;

	va_start(ArgList, pszFormat);
	vsnprintf(szBuffer, PVR_MAX_DEBUG_MESSAGE_LEN, pszFormat, ArgList);
	va_end(ArgList);
	DIPrintf(psEntry, "%s\n", szBuffer);
}

static int _DebugDumpDebugDIShow(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	if (pvData != NULL  &&  pvData != DI_START_TOKEN)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE *)pvData;

		if (psDeviceNode->pvDevice != NULL)
		{
			PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX,
			                   _DumpDebugDIPrintf, psEntry);
		}
	}

	return 0;
}

#if defined(SUPPORT_RGX)
/*************************************************************************/ /*!
 Firmware Trace DebugFS entry
*/ /**************************************************************************/
static void *_DebugFWTraceCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode, va_list va)
{
	IMG_UINT64 *pui64CurrentPosition = va_arg(va, IMG_UINT64 *);
	IMG_UINT64 ui64Position = va_arg(va, IMG_UINT64);
	IMG_UINT64 ui64CurrentPosition = *pui64CurrentPosition;

	(*pui64CurrentPosition)++;

	return (ui64CurrentPosition == ui64Position) ? psDevNode : NULL;
}

static void *_DebugFWTraceDIStart(OSDI_IMPL_ENTRY *psEntry, IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	if (*pui64Pos == 0)
	{
		return DI_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugFWTraceCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _DebugFWTraceDIStop(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_DebugFWTraceDINext(OSDI_IMPL_ENTRY *psEntry,
								  void *pvData,
								  IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*pui64Pos)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugFWTraceCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _FWTraceDIPrintf(void *pvDumpDebugFile,
				const IMG_CHAR *pszFormat, ...)
{
	OSDI_IMPL_ENTRY *psEntry = pvDumpDebugFile;
	IMG_CHAR szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	va_list ArgList;

	va_start(ArgList, pszFormat);
	vsnprintf(szBuffer, PVR_MAX_DEBUG_MESSAGE_LEN, pszFormat, ArgList);
	va_end(ArgList);
	DIPrintf(psEntry, "%s\n", szBuffer);
}

static int _DebugFWTraceDIShow(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	if (pvData != NULL  &&  pvData != DI_START_TOKEN)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE *)pvData;

		if (psDeviceNode->pvDevice != NULL)
		{
			PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

			RGXDumpFirmwareTrace(_FWTraceDIPrintf, psEntry, psDevInfo);
		}
	}

	return 0;
}

#if defined(SUPPORT_FIRMWARE_GCOV)

static PVRSRV_RGXDEV_INFO *getPsDevInfo(OSDI_IMPL_ENTRY *psEntry)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);

	if (psPVRSRVData != NULL)
	{
		if (psPVRSRVData->psDeviceNodeList != NULL)
		{
			PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO*)psPVRSRVData->psDeviceNodeList->pvDevice;
			return psDevInfo;
		}
	}
	return NULL;
}

static void *_FirmwareGcovDIStart(OSDI_IMPL_ENTRY *psEntry, IMG_UINT64 *pui64Pos)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = getPsDevInfo(psEntry);

	if (psDevInfo != NULL)
	{
		if (psDevInfo->psFirmwareGcovBufferMemDesc != NULL)
		{
			void *pvCpuVirtAddr;
			DevmemAcquireCpuVirtAddr(psDevInfo->psFirmwareGcovBufferMemDesc, &pvCpuVirtAddr);
			return *pui64Pos ? NULL : pvCpuVirtAddr;
		}
	}

	return NULL;
}

static void _FirmwareGcovDIStop(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = getPsDevInfo(psEntry);

	PVR_UNREFERENCED_PARAMETER(pvData);

	if (psDevInfo != NULL)
	{
		if (psDevInfo->psFirmwareGcovBufferMemDesc != NULL)
		{
			DevmemReleaseCpuVirtAddr(psDevInfo->psFirmwareGcovBufferMemDesc);
		}
	}
}

static void *_FirmwareGcovDINext(OSDI_IMPL_ENTRY *psEntry,
								  void *pvData,
								  IMG_UINT64 *pui64Pos)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
	PVR_UNREFERENCED_PARAMETER(pui64Pos);
	return NULL;
}

static int _FirmwareGcovDIShow(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = getPsDevInfo(psEntry);

	if (psDevInfo != NULL)
	{
		DIWrite(psEntry, pvData, psDevInfo->ui32FirmwareGcovSize);
	}
	return 0;
}

#endif /* defined(SUPPORT_FIRMWARE_GCOV) */

#if defined(SUPPORT_POWER_VALIDATION_VIA_DEBUGFS)
/*************************************************************************/ /*!
 Power monitoring DebugFS entry
*/ /**************************************************************************/
static void *_PowMonCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode, va_list va)
{
	IMG_UINT64 *pui64CurrentPosition = va_arg(va, IMG_UINT64 *);
	IMG_UINT64 ui64Position = va_arg(va, IMG_UINT64);
	IMG_UINT64 ui64CurrentPosition = *pui64CurrentPosition;

	(*pui64CurrentPosition)++;

	return (ui64CurrentPosition == ui64Position) ? psDevNode : NULL;
}

static void *_PowMonTraceDIStart(OSDI_IMPL_ENTRY *psEntry, IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	if (*pui64Pos == 0)
	{
		return DI_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _PowMonCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _PowMonTraceDIStop(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_PowMonTraceDINext(OSDI_IMPL_ENTRY *psEntry,
								 void *pvData,
								 IMG_UINT64 *pui64Pos)
{
	PVRSRV_DATA *psPVRSRVData = DIGetPrivData(psEntry);
	IMG_UINT64 uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*pui64Pos)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _PowMonCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *pui64Pos);
}

static void _PowMonTraceDIPrintf(void *pvDumpDebugFile,
				const IMG_CHAR *pszFormat, ...)
{
	OSDI_IMPL_ENTRY *psEntry = pvDumpDebugFile;
	IMG_CHAR szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	va_list ArgList;

	va_start(ArgList, pszFormat);
	vsnprintf(szBuffer, PVR_MAX_DEBUG_MESSAGE_LEN, pszFormat, ArgList);
	va_end(ArgList);
	DIPrintf(psEntry, "%s\n", szBuffer);
}

static int _PowMonTraceDIShow(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	if (pvData != NULL  &&  pvData != DI_START_TOKEN)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE *)pvData;

		if (psDeviceNode->pvDevice != NULL)
		{
			PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

			RGXDumpPowerMonitoring(_PowMonTraceDIPrintf, psEntry, psDevInfo);
		}
	}

	return 0;
}
#endif /* defined(SUPPORT_POWER_VALIDATION_VIA_DEBUGFS) */

#if defined(SUPPORT_VALIDATION)
/*************************************************************************/ /*!
 RGX Registers Dump DebugFS entry
*/ /**************************************************************************/
static IMG_INT64 _RgxRegsSeek(IMG_UINT64 ui64Offset, void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = pvData;
	PVRSRV_RGXDEV_INFO *psDevInfo;

	PVR_LOG_RETURN_IF_FALSE(psPVRSRVData != NULL, "psPVRSRVData is NULL", -1);

	psDevInfo = psPVRSRVData->psDeviceNodeList->pvDevice;

	PVR_LOG_RETURN_IF_FALSE(ui64Offset <= (psDevInfo->ui32RegSize - 4),
	                        "register offset is too big", -1);

	return ui64Offset;
}

static IMG_INT64 _RgxRegsRead(IMG_CHAR *pcBuffer, IMG_UINT64 ui64Count,
                              IMG_UINT64 *pui64Pos, void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = pvData;
	IMG_UINT64 uiRegVal = 0x00;
	PVRSRV_RGXDEV_INFO *psDevInfo;
	void __iomem *pvRegs;

	PVR_LOG_RETURN_IF_FALSE(psPVRSRVData != NULL,
	                        "psPVRSRVData is NULL", -ENXIO);
	PVR_LOG_RETURN_IF_FALSE(ui64Count == 4 || ui64Count == 8,
	                        "wrong RGX register size", -EIO);
	PVR_LOG_RETURN_IF_FALSE(!(*pui64Pos & (ui64Count - 1)),
	                        "register read offset isn't aligned", -EINVAL);

	psDevInfo = psPVRSRVData->psDeviceNodeList->pvDevice;
	pvRegs = psDevInfo->pvRegsBaseKM;

	uiRegVal = ui64Count == 4 ?
	        OSReadHWReg32(pvRegs, *pui64Pos) : OSReadHWReg64(pvRegs, *pui64Pos);

	OSCachedMemCopy(pcBuffer, &uiRegVal, ui64Count);

	return ui64Count;
}

static IMG_INT64 _RgxRegsWrite(const IMG_CHAR *pcBuffer, IMG_UINT64 ui64Count,
                               IMG_UINT64 *pui64Pos, void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = pvData;
	PVRSRV_RGXDEV_INFO *psDevInfo;

	/* ignore the '\0' character */
	ui64Count -= 1;

	PVR_LOG_RETURN_IF_FALSE(psPVRSRVData != NULL,
	                        "psPVRSRVData == NULL", -ENXIO);
	PVR_LOG_RETURN_IF_FALSE(ui64Count == 4 || ui64Count == 8,
	                        "wrong RGX register size", -EIO);
	PVR_LOG_RETURN_IF_FALSE(!(*pui64Pos & (ui64Count - 1)),
	                        "register read offset isn't aligned", -EINVAL);

	psDevInfo = psPVRSRVData->psDeviceNodeList->pvDevice;

	if (ui64Count == 4)
	{
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, *pui64Pos,
		               *((IMG_UINT32 *) (void *) pcBuffer));
	}
	else
	{
		OSWriteHWReg64(psDevInfo->pvRegsBaseKM, *pui64Pos,
		               *((IMG_UINT64 *) (void *) pcBuffer));
	}

	return ui64Count;
}
#endif /* defined(SUPPORT_VALIDATION) && !defined(NO_HARDWARE) */

#endif /* defined(SUPPORT_RGX) */
/*************************************************************************/ /*!
 Debug level DebugFS entry
*/ /**************************************************************************/

#if defined(DEBUG) || defined(PVR_DPF_ADHOC_DEBUG_ON)
static void *DebugLevelDIStart(OSDI_IMPL_ENTRY *psEntry, IMG_UINT64 *pui64Pos)
{
	if (*pui64Pos == 0)
	{
		return DIGetPrivData(psEntry);
	}

	return NULL;
}

static void DebugLevelDIStop(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *DebugLevelDINext(OSDI_IMPL_ENTRY *psEntry,
							   void *pvData,
							   IMG_UINT64 *pui64Pos)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
	PVR_UNREFERENCED_PARAMETER(pui64Pos);

	return NULL;
}

static int DebugLevelDIShow(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	if (pvData != NULL)
	{
		IMG_UINT32 uiDebugLevel = *((IMG_UINT32 *)pvData);

		DIPrintf(psEntry, "%u\n", uiDebugLevel);

		return 0;
	}

	return -EINVAL;
}

static IMG_INT64 DebugLevelSet(const IMG_CHAR *pcBuffer, IMG_UINT64 ui64Count,
                               IMG_UINT64 *pui64Pos, void *pvData)
{
	IMG_UINT32 *uiDebugLevel = pvData;
	const IMG_UINT uiMaxBufferSize = 6;

	PVR_RETURN_IF_FALSE(pcBuffer != NULL, -EIO);
	PVR_RETURN_IF_FALSE(pui64Pos != NULL && *pui64Pos == 0, -EIO);
	PVR_RETURN_IF_FALSE(ui64Count > 0 && ui64Count < uiMaxBufferSize, -EINVAL);
	PVR_RETURN_IF_FALSE(pcBuffer[ui64Count - 1] == '\0', -EINVAL);

	if (sscanf(pcBuffer, "%u", &gPVRDebugLevel) == 0)
	{
		return -EINVAL;
	}

	/* As this is Linux the next line uses a GCC builtin function */
	(*uiDebugLevel) &= (1 << __builtin_ffsl(DBGPRIV_LAST)) - 1;

	*pui64Pos += ui64Count;
	return ui64Count;
}
#endif /* defined(DEBUG) */

static DI_ENTRY *gpsVersionDIEntry;

static DI_ENTRY *gpsStatusDIEntry;
static DI_ENTRY *gpsDumpDebugDIEntry;

#if defined(SUPPORT_RGX)
static DI_ENTRY *gpsFWTraceDIEntry;
#if defined(SUPPORT_POWER_VALIDATION_VIA_DEBUGFS)
static DI_ENTRY *gpsPowMonDIEntry;
#endif
#if defined(SUPPORT_FIRMWARE_GCOV)
static DI_ENTRY *gpsFirmwareGcovDIEntry;
#endif
#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
static DI_ENTRY *gpsPowerDataDIEntry;
#endif
#if defined(SUPPORT_VALIDATION)
static DI_ENTRY *gpsRGXRegsDIEntry;
#endif
#endif

#if defined(DEBUG) || defined(PVR_DPF_ADHOC_DEBUG_ON)
static DI_ENTRY *gpsDebugLevelDIEntry;
#endif

int PVRDebugCreateDIEntries(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR eError;

	PVR_ASSERT(psPVRSRVData != NULL);

	/*
	 * The DebugFS entries are designed to work in a single device system but
	 * this function will be called multiple times in a multi-device system.
	 * Return an error in this case.
	 */
	if (gpsVersionDIEntry)
	{
		return -EEXIST;
	}

#if defined(SUPPORT_RGX) && !defined(NO_HARDWARE)
	if (SORgxGpuUtilStatsRegister(&ghGpuUtilUserDebugFS) != PVRSRV_OK)
	{
		return -ENOMEM;
	}
#endif

	{
		DI_ITERATOR_CB sIterator = {
			.pfnStart = _VersionStartOp,
			.pfnStop = _VersionStopOp,
			.pfnNext = _VersionNextOp,
			.pfnShow = _VersionShowOp
		};

		eError = DICreateEntry("version", NULL, &sIterator, psPVRSRVData,
		                       DI_ENTRY_TYPE_GENERIC, &gpsVersionDIEntry);
		PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
	}

	{
		DI_ITERATOR_CB sIterator = {
			.pfnStart = _DebugStatusDIStart,
			.pfnStop = _DebugStatusDIStop,
			.pfnNext = _DebugStatusDINext,
			.pfnShow = _DebugStatusDIShow,
			.pfnWrite = DebugStatusSet,
			//'K' expected + Null terminator
			.ui32WriteLenMax = ((1U)+1U)
		};
		eError = DICreateEntry("status", NULL, &sIterator, psPVRSRVData,
		                       DI_ENTRY_TYPE_GENERIC, &gpsStatusDIEntry);
		PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
	}

	{
		DI_ITERATOR_CB sIterator = {
			.pfnStart = _DebugDumpDebugDIStart,
			.pfnStop = _DebugDumpDebugDIStop,
			.pfnNext = _DebugDumpDebugDINext,
			.pfnShow = _DebugDumpDebugDIShow
		};
		eError = DICreateEntry("debug_dump", NULL, &sIterator, psPVRSRVData,
		                       DI_ENTRY_TYPE_GENERIC, &gpsDumpDebugDIEntry);
		PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
	}

#if defined(SUPPORT_RGX)
	if (! PVRSRV_VZ_MODE_IS(GUEST))
	{
		{
			DI_ITERATOR_CB sIterator = {
				.pfnStart = _DebugFWTraceDIStart,
				.pfnStop = _DebugFWTraceDIStop,
				.pfnNext = _DebugFWTraceDINext,
				.pfnShow = _DebugFWTraceDIShow
			};
			eError = DICreateEntry("firmware_trace", NULL, &sIterator,
			                       psPVRSRVData, DI_ENTRY_TYPE_GENERIC,
			                       &gpsFWTraceDIEntry);
			PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
		}

#if defined(SUPPORT_POWER_VALIDATION_VIA_DEBUGFS)
		{
			DI_ITERATOR_CB sIterator = {
				.pfnStart = _PowMonTraceDIStart,
				.pfnStop = _PowMonTraceDIStop,
				.pfnNext = _PowMonTraceDINext,
				.pfnShow = _PowMonTraceDIShow
			};
			eError = DICreateEntry("power_mon", NULL, &sIterator, psPVRSRVData,
			                       DI_ENTRY_TYPE_GENERIC, &gpsPowMonDIEntry);
			PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
		}
#endif
	}

#if defined(SUPPORT_FIRMWARE_GCOV)
	{
		DI_ITERATOR_CB sIterator = {
			.pfnStart = _FirmwareGcovDIStart,
			.pfnStop = _FirmwareGcovDIStop,
			.pfnNext = _FirmwareGcovDINext,
			.pfnShow = _FirmwareGcovDIShow
		};

		eError = DICreateEntry("firmware_gcov", NULL, &sIterator, psPVRSRVData,
		                       DI_ENTRY_TYPE_GENERIC, &gpsFirmwareGcovDIEntry);
		PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
	}
#endif

#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
	{
		DI_ITERATOR_CB sIterator = {
			.pfnStart = _DebugPowerDataDIStart,
			.pfnStop = _DebugPowerDataDIStop,
			.pfnNext = _DebugPowerDataDINext,
			.pfnShow = _DebugPowerDataDIShow,
			.pfnWrite = PowerDataSet,
			//Expects '0' or '1' plus Null terminator
			.ui32WriteLenMax = ((1U)+1U)
		};
		eError = DICreateEntry("power_data", NULL, &sIterator, psPVRSRVData,
		                       DI_ENTRY_TYPE_GENERIC, &gpsPowerDataDIEntry);
		PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
	}
#endif

#if defined(SUPPORT_VALIDATION)
	{
		DI_ITERATOR_CB sIterator = {
			.pfnSeek = _RgxRegsSeek,
			.pfnRead = _RgxRegsRead,
			.pfnWrite = _RgxRegsWrite,
			//Max size of input binary data is 4 bytes (UINT32) or 8 bytes (UINT64)
			.ui32WriteLenMax = ((8U)+1U)
		};
		eError = DICreateEntry("rgxregs", NULL, &sIterator, psPVRSRVData,
		                       DI_ENTRY_TYPE_RANDOM_ACCESS, &gpsRGXRegsDIEntry);

		PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
	}
#endif

#endif

#if defined(DEBUG) || defined(PVR_DPF_ADHOC_DEBUG_ON)
	{
		DI_ITERATOR_CB sIterator = {
			.pfnStart = DebugLevelDIStart,
			.pfnStop = DebugLevelDIStop,
			.pfnNext = DebugLevelDINext,
			.pfnShow = DebugLevelDIShow,
			.pfnWrite = DebugLevelSet,
			//Max value of 255(3 char) + Null Term
			.ui32WriteLenMax = ((3U)+1U)
		};
		eError = DICreateEntry("debug_level", NULL, &sIterator, &gPVRDebugLevel,
		                       DI_ENTRY_TYPE_GENERIC, &gpsDebugLevelDIEntry);
		PVR_GOTO_IF_ERROR(eError, PVRDebugCreateDIEntriesErrorExit);
	}
#endif

	return 0;

PVRDebugCreateDIEntriesErrorExit:
	PVRDebugRemoveDIEntries();

	return -EFAULT;
}

void PVRDebugRemoveDIEntries(void)
{
#if defined(SUPPORT_RGX) && !defined(NO_HARDWARE)
	if (ghGpuUtilUserDebugFS != NULL)
	{
		SORgxGpuUtilStatsUnregister(ghGpuUtilUserDebugFS);
		ghGpuUtilUserDebugFS = NULL;
	}
#endif

#if defined(DEBUG) || defined(PVR_DPF_ADHOC_DEBUG_ON)
	if (gpsDebugLevelDIEntry != NULL)
	{
		DIDestroyEntry(gpsDebugLevelDIEntry);
	}
#endif

#if defined(SUPPORT_RGX)
	if (gpsFWTraceDIEntry != NULL)
	{
		DIDestroyEntry(gpsFWTraceDIEntry);
	}

#if defined(SUPPORT_POWER_VALIDATION_VIA_DEBUGFS)
	if (gpsPowMonDIEntry != NULL)
	{
		DIDestroyEntry(gpsPowMonDIEntry);
	}
#endif

#if defined(SUPPORT_FIRMWARE_GCOV)
	if (gpsFirmwareGcovDIEntry != NULL)
	{
		DIDestroyEntry(gpsFirmwareGcovDIEntry);
	}
#endif

#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
	if (gpsPowerDataDIEntry != NULL)
	{
		DIDestroyEntry(gpsPowerDataDIEntry);
	}
#endif

#if defined(SUPPORT_VALIDATION)
	if (gpsRGXRegsDIEntry != NULL)
	{
		DIDestroyEntry(gpsRGXRegsDIEntry);
	}
#endif
#endif /* defined(SUPPORT_RGX) */

	if (gpsDumpDebugDIEntry != NULL)
	{
		DIDestroyEntry(gpsDumpDebugDIEntry);
	}

	if (gpsStatusDIEntry != NULL)
	{
		DIDestroyEntry(gpsStatusDIEntry);
	}

	if (gpsVersionDIEntry != NULL)
	{
		DIDestroyEntry(gpsVersionDIEntry);
	}
}
