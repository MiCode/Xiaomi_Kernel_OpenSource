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
#include "pvr_debugfs.h"
#include "linkage.h"
#include "pvr_uaccess.h"
#include "pvrsrv.h"
#include "lists.h"
#include "osfunc.h"

#include "rgx_options.h"
#include "devicemem.h"

#if defined(SUPPORT_RGX)
#include "rgxdevice.h"
#include "rgxdebug.h"
#include "rgxinit.h"

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
		printk(KERN_ERR "%s (truncated)\n", pszBuf);
	}
	else
	{
		printk(KERN_ERR "%s\n", pszBuf);
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
	loff_t *puiCurrentPosition = va_arg(va, loff_t *);
	loff_t uiPosition = va_arg(va, loff_t);
	loff_t uiCurrentPosition = *puiCurrentPosition;

	(*puiCurrentPosition)++;

	return (uiCurrentPosition == uiPosition) ? psDevNode : NULL;
}

static void *_DebugVersionSeqStart(struct seq_file *psSeqFile,
				   loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	if (*puiPosition == 0)
	{
		return SEQ_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugVersionCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static void _DebugVersionSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_DebugVersionSeqNext(struct seq_file *psSeqFile,
				  void *pvData,
				  loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*puiPosition)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugVersionCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

#define SEQ_PRINT_VERSION_FMTSPEC "%s Version: %u.%u @ %u (%s) build options: 0x%08x %s\n"
#define STR_DEBUG   "debug"
#define STR_RELEASE "release"

static int _DebugVersionSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	if (pvData == SEQ_START_TOKEN)
	{
		if (psPVRSRVData->sDriverInfo.bIsNoMatch)
		{
			const BUILD_INFO *psBuildInfo;

			psBuildInfo = &psPVRSRVData->sDriverInfo.sUMBuildInfo;
			seq_printf(psSeqFile, SEQ_PRINT_VERSION_FMTSPEC,
			           "UM Driver",
				       PVRVERSION_UNPACK_MAJ(psBuildInfo->ui32BuildVersion),
				       PVRVERSION_UNPACK_MIN(psBuildInfo->ui32BuildVersion),
				       psBuildInfo->ui32BuildRevision,
				       (psBuildInfo->ui32BuildType == BUILD_TYPE_DEBUG) ? STR_DEBUG : STR_RELEASE,
				       psBuildInfo->ui32BuildOptions,
					   PVR_BUILD_DIR);

			psBuildInfo = &psPVRSRVData->sDriverInfo.sKMBuildInfo;
			seq_printf(psSeqFile, SEQ_PRINT_VERSION_FMTSPEC,
			           "KM Driver",
				       PVRVERSION_UNPACK_MAJ(psBuildInfo->ui32BuildVersion),
				       PVRVERSION_UNPACK_MIN(psBuildInfo->ui32BuildVersion),
				       psBuildInfo->ui32BuildRevision,
				       (psBuildInfo->ui32BuildType == BUILD_TYPE_DEBUG) ? STR_DEBUG : STR_RELEASE,
				       psBuildInfo->ui32BuildOptions,
					   PVR_BUILD_DIR);
		}
		else
		{
			/*
			 * bIsNoMatch is `false` in one of the following cases:
			 * - UM & KM version parameters actually match.
			 * - A comparison between UM & KM has not been made yet, because no
			 *   client ever connected.
			 *
			 * In both cases, available (KM) version info is the best output we
			 * can provide.
			 */
			seq_printf(psSeqFile, "Driver Version: %s (%s) build options: 0x%08lx %s\n",
			           PVRVERSION_STRING, PVR_BUILD_TYPE, RGX_BUILD_OPTIONS_KM, PVR_BUILD_DIR);
		}
	}
	else if (pvData != NULL)
	{
		PVRSRV_DEVICE_NODE *psDevNode = (PVRSRV_DEVICE_NODE *)pvData;
#if defined(SUPPORT_RGX)
		PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
#endif
		IMG_BOOL bFwVersionInfoPrinted = IMG_FALSE;

		seq_printf(psSeqFile, "\nDevice Name: %s\n", psDevNode->psDevConfig->pszName);

		if (psDevNode->psDevConfig->pszVersion)
		{
			seq_printf(psSeqFile, "Device Version: %s\n", psDevNode->psDevConfig->pszVersion);
		}

		if (psDevNode->pfnDeviceVersionString)
		{
			IMG_CHAR *pszDeviceVersionString;

			if (psDevNode->pfnDeviceVersionString(psDevNode, &pszDeviceVersionString) == PVRSRV_OK)
			{
				seq_printf(psSeqFile, "%s\n", pszDeviceVersionString);

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

					seq_printf(psSeqFile, SEQ_PRINT_VERSION_FMTSPEC,
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
				PVR_DPF((PVR_DBG_ERROR,
						 "%s: Error acquiring CPU virtual address of FWInitMemDesc",
						 __func__));
			}
		}
#endif

		if (!bFwVersionInfoPrinted)
		{
			seq_printf(psSeqFile, "Firmware Version: Info unavailable %s\n",
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

static struct seq_operations gsDebugVersionReadOps =
{
	.start = _DebugVersionSeqStart,
	.stop = _DebugVersionSeqStop,
	.next = _DebugVersionSeqNext,
	.show = _DebugVersionSeqShow,
};

/*************************************************************************/ /*!
 Status DebugFS entry
*/ /**************************************************************************/

static void *_DebugStatusCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode,
										 va_list va)
{
	loff_t *puiCurrentPosition = va_arg(va, loff_t *);
	loff_t uiPosition = va_arg(va, loff_t);
	loff_t uiCurrentPosition = *puiCurrentPosition;

	(*puiCurrentPosition)++;

	return (uiCurrentPosition == uiPosition) ? psDevNode : NULL;
}

static void *_DebugStatusSeqStart(struct seq_file *psSeqFile,
								  loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	if (*puiPosition == 0)
	{
		return SEQ_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugStatusCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static void _DebugStatusSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_DebugStatusSeqNext(struct seq_file *psSeqFile,
								 void *pvData,
								 loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*puiPosition)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugStatusCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static int _DebugStatusSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData == SEQ_START_TOKEN)
	{
		PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;

		if (psPVRSRVData != NULL)
		{
			switch (psPVRSRVData->eServicesState)
			{
				case PVRSRV_SERVICES_STATE_OK:
					seq_printf(psSeqFile, "Driver Status:   OK\n");
					break;
				case PVRSRV_SERVICES_STATE_BAD:
					seq_printf(psSeqFile, "Driver Status:   BAD\n");
					break;
				case PVRSRV_SERVICES_STATE_UNDEFINED:
					seq_printf(psSeqFile, "Driver Status:   UNDEFINED\n");
					break;
				default:
					seq_printf(psSeqFile, "Driver Status:   UNKNOWN (%d)\n", psPVRSRVData->eServicesState);
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

		seq_printf(psSeqFile, "Firmware Status: %s%s\n", pszStatus, pszReason);

		if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
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

				for (ui32DMIndex = 0; ui32DMIndex < psDevInfo->sDevFeatureCfg.ui32MAXDMCount; ui32DMIndex++)
				{
					ui32HWREventCount += psHWRInfoBuf->aui32HwrDmLockedUpCount[ui32DMIndex];
					ui32CRREventCount += psHWRInfoBuf->aui32HwrDmOverranCount[ui32DMIndex];
				}

				seq_printf(psSeqFile, "HWR Event Count: %d\n", ui32HWREventCount);
				seq_printf(psSeqFile, "CRR Event Count: %d\n", ui32CRREventCount);
#if defined(PVRSRV_STALLED_CCB_ACTION)
				/* Write the number of Sync Lockup Recovery (SLR) events... */
				seq_printf(psSeqFile, "SLR Event Count: %d\n", psDevInfo->psRGXFWIfFwOsData->ui32ForcedUpdatesRequested);
#endif
			}

			if (psFwSysData != NULL)
			{
				seq_printf(psSeqFile, "FWF Event Count: %d\n", psFwSysData->ui32FWFaults);
			}

			/* Write the number of APM events... */
			seq_printf(psSeqFile, "APM Event Count: %d\n", psDevInfo->ui32ActivePMReqTotal);

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

					seq_printf(psSeqFile, "GPU Utilisation: %u%%\n", (IMG_UINT32)util);
				}
				else
				{
					seq_printf(psSeqFile, "GPU Utilisation: -\n");
				}
			}
#endif
		}
	}

	return 0;
}

static ssize_t DebugStatusSet(const char __user *pcBuffer,
							  size_t uiCount,
							  loff_t *puiPosition,
							  void *pvData)
{
	IMG_CHAR acDataBuffer[6];

	if (puiPosition == NULL || *puiPosition != 0)
	{
		return -EIO;
	}

	if (uiCount == 0 || uiCount > ARRAY_SIZE(acDataBuffer))
	{
		return -EINVAL;
	}

	if (pvr_copy_from_user(acDataBuffer, pcBuffer, uiCount))
	{
		return -EINVAL;
	}

	if (acDataBuffer[uiCount - 1] != '\n')
	{
		return -EINVAL;
	}

	if (((acDataBuffer[0] == 'k') || ((acDataBuffer[0] == 'K'))) && uiCount == 2)
	{
		PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
		psPVRSRVData->eServicesState = PVRSRV_SERVICES_STATE_BAD;
	}
	else
	{
		return -EINVAL;
	}

	*puiPosition += uiCount;
	return uiCount;
}

static struct seq_operations gsDebugStatusReadOps =
{
	.start = _DebugStatusSeqStart,
	.stop = _DebugStatusSeqStop,
	.next = _DebugStatusSeqNext,
	.show = _DebugStatusSeqShow,
};

/*************************************************************************/ /*!
 Dump Debug DebugFS entry
*/ /**************************************************************************/

static void *_DebugDumpDebugCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode, va_list va)
{
	loff_t *puiCurrentPosition = va_arg(va, loff_t *);
	loff_t uiPosition = va_arg(va, loff_t);
	loff_t uiCurrentPosition = *puiCurrentPosition;

	(*puiCurrentPosition)++;

	return (uiCurrentPosition == uiPosition) ? psDevNode : NULL;
}

static void *_DebugDumpDebugSeqStart(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	if (*puiPosition == 0)
	{
		return SEQ_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugDumpDebugCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static void _DebugDumpDebugSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_DebugDumpDebugSeqNext(struct seq_file *psSeqFile,
									void *pvData,
									loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*puiPosition)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugDumpDebugCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static void _DumpDebugSeqPrintf(void *pvDumpDebugFile,
				const IMG_CHAR *pszFormat, ...)
{
	struct seq_file *psSeqFile = (struct seq_file *)pvDumpDebugFile;
	IMG_CHAR szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	va_list ArgList;

	va_start(ArgList, pszFormat);
	vsnprintf(szBuffer, PVR_MAX_DEBUG_MESSAGE_LEN, pszFormat, ArgList);
	va_end(ArgList);
	seq_printf(psSeqFile, "%s\n", szBuffer);
}

static int _DebugDumpDebugSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL  &&  pvData != SEQ_START_TOKEN)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE *)pvData;

		if (psDeviceNode->pvDevice != NULL)
		{
			PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX,
						_DumpDebugSeqPrintf, psSeqFile);
		}
	}

	return 0;
}

static struct seq_operations gsDumpDebugReadOps =
{
	.start = _DebugDumpDebugSeqStart,
	.stop  = _DebugDumpDebugSeqStop,
	.next  = _DebugDumpDebugSeqNext,
	.show  = _DebugDumpDebugSeqShow,
};

#if defined(SUPPORT_RGX)
/*************************************************************************/ /*!
 Firmware Trace DebugFS entry
*/ /**************************************************************************/
static void *_DebugFWTraceCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode, va_list va)
{
	loff_t *puiCurrentPosition = va_arg(va, loff_t *);
	loff_t uiPosition = va_arg(va, loff_t);
	loff_t uiCurrentPosition = *puiCurrentPosition;

	(*puiCurrentPosition)++;

	return (uiCurrentPosition == uiPosition) ? psDevNode : NULL;
}

static void *_DebugFWTraceSeqStart(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	if (*puiPosition == 0)
	{
		return SEQ_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugFWTraceCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static void _DebugFWTraceSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_DebugFWTraceSeqNext(struct seq_file *psSeqFile,
								  void *pvData,
								  loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*puiPosition)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _DebugFWTraceCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static void _FWTraceSeqPrintf(void *pvDumpDebugFile,
				const IMG_CHAR *pszFormat, ...)
{
	struct seq_file *psSeqFile = (struct seq_file *)pvDumpDebugFile;
	IMG_CHAR szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	va_list ArgList;

	va_start(ArgList, pszFormat);
	vsnprintf(szBuffer, PVR_MAX_DEBUG_MESSAGE_LEN, pszFormat, ArgList);
	va_end(ArgList);
	seq_printf(psSeqFile, "%s\n", szBuffer);
}

static int _DebugFWTraceSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL  &&  pvData != SEQ_START_TOKEN)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE *)pvData;

		if (psDeviceNode->pvDevice != NULL)
		{
			PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

			RGXDumpFirmwareTrace(_FWTraceSeqPrintf, psSeqFile, psDevInfo);
		}
	}

	return 0;
}

static struct seq_operations gsFWTraceReadOps =
{
	.start = _DebugFWTraceSeqStart,
	.stop  = _DebugFWTraceSeqStop,
	.next  = _DebugFWTraceSeqNext,
	.show  = _DebugFWTraceSeqShow,
};

#if defined(SUPPORT_FIRMWARE_GCOV)

static PVRSRV_RGXDEV_INFO *getPsDevInfo(struct seq_file *psSeqFile)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;

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

static void *_FirmwareGcovSeqStart(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = getPsDevInfo(psSeqFile);

	if (psDevInfo != NULL)
	{
		if (psDevInfo->psFirmwareGcovBufferMemDesc != NULL)
		{
			void *pvCpuVirtAddr;
			DevmemAcquireCpuVirtAddr(psDevInfo->psFirmwareGcovBufferMemDesc, &pvCpuVirtAddr);
			return *puiPosition ? NULL : pvCpuVirtAddr;
		}
	}

	return NULL;
}

static void _FirmwareGcovSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = getPsDevInfo(psSeqFile);

	PVR_UNREFERENCED_PARAMETER(pvData);

	if (psDevInfo != NULL)
	{
		if (psDevInfo->psFirmwareGcovBufferMemDesc != NULL)
		{
			DevmemReleaseCpuVirtAddr(psDevInfo->psFirmwareGcovBufferMemDesc);
		}
	}
}

static void *_FirmwareGcovSeqNext(struct seq_file *psSeqFile,
								  void *pvData,
								  loff_t *puiPosition)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);
	PVR_UNREFERENCED_PARAMETER(puiPosition);
	return NULL;
}

static int _FirmwareGcovSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = getPsDevInfo(psSeqFile);

	if (psDevInfo != NULL)
	{
		seq_write(psSeqFile, pvData, psDevInfo->ui32FirmwareGcovSize);
	}
	return 0;
}

static struct seq_operations gsFirmwareGcovReadOps =
{
	.start = _FirmwareGcovSeqStart,
	.stop  = _FirmwareGcovSeqStop,
	.next  = _FirmwareGcovSeqNext,
	.show  = _FirmwareGcovSeqShow,
};

#endif /* defined(SUPPORT_FIRMWARE_GCOV) */

#if defined(SUPPORT_POWMON_COMPONENT)
/*************************************************************************/ /*!
 Power monitoring DebugFS entry
*/ /**************************************************************************/
static void *_PowMonCompare_AnyVaCb(PVRSRV_DEVICE_NODE *psDevNode, va_list va)
{
	loff_t *puiCurrentPosition = va_arg(va, loff_t *);
	loff_t uiPosition = va_arg(va, loff_t);
	loff_t uiCurrentPosition = *puiCurrentPosition;

	(*puiCurrentPosition)++;

	return (uiCurrentPosition == uiPosition) ? psDevNode : NULL;
}

static void *_PowMonTraceSeqStart(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	if (*puiPosition == 0)
	{
		return SEQ_START_TOKEN;
	}

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _PowMonCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static void _PowMonTraceSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *_PowMonTraceSeqNext(struct seq_file *psSeqFile,
								 void *pvData,
								 loff_t *puiPosition)
{
	PVRSRV_DATA *psPVRSRVData = (PVRSRV_DATA *)psSeqFile->private;
	loff_t uiCurrentPosition = 1;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*puiPosition)++;

	return List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
										  _PowMonCompare_AnyVaCb,
										  &uiCurrentPosition,
										  *puiPosition);
}

static void _PowMonTraceSeqPrintf(void *pvDumpDebugFile,
				const IMG_CHAR *pszFormat, ...)
{
	struct seq_file *psSeqFile = (struct seq_file *)pvDumpDebugFile;
	IMG_CHAR szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	va_list ArgList;

	va_start(ArgList, pszFormat);
	vsnprintf(szBuffer, PVR_MAX_DEBUG_MESSAGE_LEN, pszFormat, ArgList);
	va_end(ArgList);
	seq_printf(psSeqFile, "%s\n", szBuffer);
}

static int _PowMonTraceSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL  &&  pvData != SEQ_START_TOKEN)
	{
		PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE *)pvData;

		if (psDeviceNode->pvDevice != NULL)
		{
			PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

			RGXDumpPowerMonitoring(_PowMonTraceSeqPrintf, psSeqFile, psDevInfo);
		}
	}

	return 0;
}

static struct seq_operations gsPowMonReadOps =
{
	.start = _PowMonTraceSeqStart,
	.stop  = _PowMonTraceSeqStop,
	.next  = _PowMonTraceSeqNext,
	.show  = _PowMonTraceSeqShow,
};
#endif /* defined(SUPPORT_POWMON_COMPONENT) */

#endif /* defined(SUPPORT_RGX) */
/*************************************************************************/ /*!
 Debug level DebugFS entry
*/ /**************************************************************************/

#if defined(DEBUG) || defined(PVR_DPF_ADHOC_DEBUG_ON)
static void *DebugLevelSeqStart(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	if (*puiPosition == 0)
	{
		return psSeqFile->private;
	}

	return NULL;
}

static void DebugLevelSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *DebugLevelSeqNext(struct seq_file *psSeqFile,
							   void *pvData,
							   loff_t *puiPosition)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);
	PVR_UNREFERENCED_PARAMETER(puiPosition);

	return NULL;
}

static int DebugLevelSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData != NULL)
	{
		IMG_UINT32 uiDebugLevel = *((IMG_UINT32 *)pvData);

		seq_printf(psSeqFile, "%u\n", uiDebugLevel);

		return 0;
	}

	return -EINVAL;
}

static struct seq_operations gsDebugLevelReadOps =
{
	.start = DebugLevelSeqStart,
	.stop = DebugLevelSeqStop,
	.next = DebugLevelSeqNext,
	.show = DebugLevelSeqShow,
};


static IMG_INT DebugLevelSet(const char __user *pcBuffer,
							 size_t uiCount,
							 loff_t *puiPosition,
							 void *pvData)
{
	IMG_UINT32 *uiDebugLevel = (IMG_UINT32 *)pvData;
	IMG_CHAR acDataBuffer[6];

	if (puiPosition == NULL || *puiPosition != 0)
	{
		return -EIO;
	}

	if (uiCount == 0 || uiCount > ARRAY_SIZE(acDataBuffer))
	{
		return -EINVAL;
	}

	if (pvr_copy_from_user(acDataBuffer, pcBuffer, uiCount))
	{
		return -EINVAL;
	}

	if (acDataBuffer[uiCount - 1] != '\n')
	{
		return -EINVAL;
	}

	if (sscanf(acDataBuffer, "%u", &gPVRDebugLevel) == 0)
	{
		return -EINVAL;
	}

	/* As this is Linux the next line uses a GCC builtin function */
	(*uiDebugLevel) &= (1 << __builtin_ffsl(DBGPRIV_LAST)) - 1;

	*puiPosition += uiCount;
	return uiCount;
}
#endif /* defined(DEBUG) */

static PPVR_DEBUGFS_ENTRY_DATA gpsVersionDebugFSEntry;

static PPVR_DEBUGFS_ENTRY_DATA gpsStatusDebugFSEntry;
static PPVR_DEBUGFS_ENTRY_DATA gpsDumpDebugDebugFSEntry;

#if defined(SUPPORT_RGX)
static PPVR_DEBUGFS_ENTRY_DATA gpsFWTraceDebugFSEntry;
#if defined(SUPPORT_POWMON_COMPONENT)
static PPVR_DEBUGFS_ENTRY_DATA gpsPowMonDebugFSEntry;
#endif
#if defined(SUPPORT_FIRMWARE_GCOV)
static PPVR_DEBUGFS_ENTRY_DATA gpsFirmwareGcovDebugFSEntry;
#endif
#endif

#if defined(DEBUG) || defined(PVR_DPF_ADHOC_DEBUG_ON)
static PPVR_DEBUGFS_ENTRY_DATA gpsDebugLevelDebugFSEntry;
#endif

int PVRDebugCreateDebugFSEntries(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	int iResult;

	PVR_ASSERT(psPVRSRVData != NULL);

	/*
	 * The DebugFS entries are designed to work in a single device system but
	 * this function will be called multiple times in a multi-device system.
	 * Return an error in this case.
	 */
	if (gpsVersionDebugFSEntry)
	{
		return -EEXIST;
	}

#if defined(SUPPORT_RGX) && !defined(NO_HARDWARE)
	if (SORgxGpuUtilStatsRegister(&ghGpuUtilUserDebugFS) != PVRSRV_OK)
	{
		return -ENOMEM;
	}
#endif

	iResult = PVRDebugFSCreateFile("version",
									NULL,
									&gsDebugVersionReadOps,
									NULL,
									NULL,
									psPVRSRVData,
									&gpsVersionDebugFSEntry);
	if (iResult != 0)
	{
		goto PVRDebugCreateDebugFSEntriesErrorExit;
	}

	iResult = PVRDebugFSCreateFile("status",
									NULL,
									&gsDebugStatusReadOps,
									(PVRSRV_ENTRY_WRITE_FUNC *)DebugStatusSet,
									NULL,
									psPVRSRVData,
									&gpsStatusDebugFSEntry);
	if (iResult != 0)
	{
		goto PVRDebugCreateDebugFSEntriesErrorExit;
	}

	iResult = PVRDebugFSCreateFile("debug_dump",
									NULL,
									&gsDumpDebugReadOps,
									NULL,
									NULL,
									psPVRSRVData,
									&gpsDumpDebugDebugFSEntry);
	if (iResult != 0)
	{
		goto PVRDebugCreateDebugFSEntriesErrorExit;
	}

#if defined(SUPPORT_RGX)
	if (! PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		iResult = PVRDebugFSCreateFile("firmware_trace",
										NULL,
										&gsFWTraceReadOps,
										NULL,
										NULL,
										psPVRSRVData,
										&gpsFWTraceDebugFSEntry);
		if (iResult != 0)
		{
			goto PVRDebugCreateDebugFSEntriesErrorExit;
		}

#if defined(SUPPORT_POWMON_COMPONENT)
		iResult = PVRDebugFSCreateFile("power_mon",
										NULL,
										&gsPowMonReadOps,
										NULL,
										NULL,
										psPVRSRVData,
										&gpsPowMonDebugFSEntry);
		if (iResult != 0)
		{
			goto PVRDebugCreateDebugFSEntriesErrorExit;
		}
#endif
	}

#if defined(SUPPORT_FIRMWARE_GCOV)
	{

		iResult = PVRDebugFSCreateFile("firmware_gcov",
										NULL,
										&gsFirmwareGcovReadOps,
										NULL,
										NULL,
										psPVRSRVData,
										&gpsFirmwareGcovDebugFSEntry);

		if (iResult != 0)
		{
			goto PVRDebugCreateDebugFSEntriesErrorExit;
		}
	}
#endif

#endif

#if defined(DEBUG) || defined(PVR_DPF_ADHOC_DEBUG_ON)
	iResult = PVRDebugFSCreateFile("debug_level",
									NULL,
									&gsDebugLevelReadOps,
									(PVRSRV_ENTRY_WRITE_FUNC *)DebugLevelSet,
									NULL,
									&gPVRDebugLevel,
									&gpsDebugLevelDebugFSEntry);
	if (iResult != 0)
	{
		goto PVRDebugCreateDebugFSEntriesErrorExit;
	}
#endif

	return 0;

PVRDebugCreateDebugFSEntriesErrorExit:

	PVRDebugRemoveDebugFSEntries();

	return iResult;
}

void PVRDebugRemoveDebugFSEntries(void)
{
#if defined(SUPPORT_RGX) && !defined(NO_HARDWARE)
	if (ghGpuUtilUserDebugFS != NULL)
	{
		SORgxGpuUtilStatsUnregister(ghGpuUtilUserDebugFS);
		ghGpuUtilUserDebugFS = NULL;
	}
#endif

#if defined(DEBUG) || defined(PVR_DPF_ADHOC_DEBUG_ON)
	if (gpsDebugLevelDebugFSEntry != NULL)
	{
		PVRDebugFSRemoveFile(&gpsDebugLevelDebugFSEntry);
	}
#endif

#if defined(SUPPORT_RGX)
	if (gpsFWTraceDebugFSEntry != NULL)
	{
		PVRDebugFSRemoveFile(&gpsFWTraceDebugFSEntry);
	}

#if defined(SUPPORT_POWMON_COMPONENT)
	if (gpsPowMonDebugFSEntry != NULL)
	{
		PVRDebugFSRemoveFile(&gpsPowMonDebugFSEntry);
	}
#endif

#if defined(SUPPORT_FIRMWARE_GCOV)
	if (gpsFirmwareGcovDebugFSEntry != NULL)
	{
		PVRDebugFSRemoveFile(&gpsFirmwareGcovDebugFSEntry);
	}
#endif

#endif /* defined(SUPPORT_RGX) */

	if (gpsDumpDebugDebugFSEntry != NULL)
	{
		PVRDebugFSRemoveFile(&gpsDumpDebugDebugFSEntry);
	}

	if (gpsStatusDebugFSEntry != NULL)
	{
		PVRDebugFSRemoveFile(&gpsStatusDebugFSEntry);
	}

	if (gpsVersionDebugFSEntry != NULL)
	{
		PVRDebugFSRemoveFile(&gpsVersionDebugFSEntry);
	}
}
