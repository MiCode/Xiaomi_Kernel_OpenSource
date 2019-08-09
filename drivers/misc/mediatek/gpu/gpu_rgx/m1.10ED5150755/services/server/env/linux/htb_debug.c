/*************************************************************************/ /*!
@File           htb_debug.c
@Title          Debug Functionality
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Provides kernel side debugFS Functionality.
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
#include "rgxdevice.h"
#include "debugmisc_server.h"
#include "htbserver.h"
#include "htbuffer.h"
#include "htbuffer_types.h"
#include "tlstream.h"
#include "tlclient.h"
#include "pvrsrv_tlcommon.h"
#include "pvr_debugfs.h"
#include "img_types.h"
#include "pvrsrv_error.h"
#include "osfunc.h"
#include "allocmem.h"
#include "pvr_notifier.h"
#include "pvrsrv.h"

/*
 * External interface routines:
 *
 * HTB_CreateFSEntry
 *	Instantiates storage and an entry in debugFS
 *
 * HTB_UpdateFSEntry
 *	Updates storage settings if necessary
 *
 * HTB_DestroyFSEntry
 *	Removes debugFS entry and releases allocated storage
 */
PVRSRV_ERROR HTB_CreateFSEntry(IMG_UINT32, const IMG_CHAR *);
PVRSRV_ERROR HTB_UpdateFSEntry(IMG_UINT32);
void HTB_DestroyFSEntry(void);

/* Flag value to mark Stream handle closure */
#define HTB_STREAM_CLOSED   (IMG_HANDLE)0xfacedead

// Global data handles for buffer manipulation and processing
typedef struct
{
	const IMG_CHAR *szStreamName;       /* Name of Host Trace Buffer */

    PPVR_DEBUGFS_ENTRY_DATA psDumpHostDebugFSEntry;	/* debugFS entry hook */

	IMG_UINT32 ui32BufferSize;          /* Size of allocated pRawBuf */
	IMG_HANDLE hStream;                 /* Stream handle for debugFS use */

	IMG_PBYTE pBuf;                     /* Acquired buffer */
	IMG_UINT32 uiBufLen;                /* Acquired buffer length */
	IMG_PBYTE pCurrent;                 /* Current processing entry in buffer */

	IMG_PBYTE pRawBuf;                  /* Copy-buffer to reduce TL lockout */
	IMG_PBYTE pRawCur;                  /* Current position within pRawBuf */
} HTB_CTRL_INFO;

static HTB_CTRL_INFO g_sCtrl;

/*****************************************************************************
 * debugFS display routines
 ******************************************************************************/
static void HTBDumpBuffer(DUMPDEBUG_PRINTF_FUNC *, void *, void *);
static void _HBTraceSeqPrintf(void *, const IMG_CHAR *, ...);
static int _DebugHBTraceSeqShow(struct seq_file *, void *);
static void *_DebugHBTraceSeqStart(struct seq_file *, loff_t *);
static void _DebugHBTraceSeqStop(struct seq_file *, void *);
static void *_DebugHBTraceSeqNext(struct seq_file *, void *, loff_t *);

static void _HBTraceSeqPrintf(void *pvDumpDebugFile,
                              const IMG_CHAR *pszFormat, ...)
{
	struct seq_file *psSeqFile = (struct seq_file *)pvDumpDebugFile;
	IMG_CHAR        aszBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	va_list         ArgList;

	va_start(ArgList, pszFormat);
	vsnprintf(aszBuffer, PVR_MAX_DEBUG_MESSAGE_LEN, pszFormat, ArgList);
	va_end(ArgList);
	seq_puts(psSeqFile, aszBuffer);
}

static int _DebugHBTraceSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	HTBDumpBuffer(_HBTraceSeqPrintf, psSeqFile, pvData);

	return 0;
}

typedef struct {
	IMG_PBYTE	pBuf;		/* Raw data buffer from TL stream */
	IMG_BOOL	bOverflowed;/* output buffer has overflowed so must retry */
	IMG_BOOL	bHeaderFlag;/* Display a header field if set */
	IMG_UINT32	uiBufLen;	/* Amount of data to process from 'pBuf' */
	IMG_UINT32	uiTotal;	/* Total bytes processed */
	IMG_UINT32	uiRetries;	/* # of attempts to process overflowing buffer */
	IMG_PBYTE	pOvflow;	/* Position to restart scanning on Overflow */
	IMG_UINT32	uiBufOvLen;	/* Amount of data to process on overflow */
} HTB_Sentinel_t;

/*
 * _DebugHBTraceSeqStart:
 *
 * Returns the address to use for subsequent 'Show', 'Next', 'Stop' file ops.
 * Return SEQ_START_TOKEN for the very first call and allocate a sentinel for
 * use by the 'Show' routine and its helpers.
 * This is stored in the psSeqFile->private hook field.
 */
static void *_DebugHBTraceSeqStart(struct seq_file *psSeqFile,
                                   loff_t *puiPosition)
{
	HTB_Sentinel_t	*pSentinel = (HTB_Sentinel_t *)psSeqFile->private;

	if (*puiPosition == 0)
	{
		if (pSentinel == NULL)
		{
			pSentinel = (HTB_Sentinel_t *)OSAllocZMem(sizeof (HTB_Sentinel_t));
			psSeqFile->private = pSentinel;
		}
		return SEQ_START_TOKEN;
	}
	else
	{
		if (pSentinel == NULL)
		{
			pSentinel = (HTB_Sentinel_t *)OSAllocZMem(sizeof (HTB_Sentinel_t));
			psSeqFile->private = pSentinel;
		}
		return (void *)pSentinel;
	}

}

/*
 * _DebugTBTraceSeqStop:
 *
 * Stop processing data collection and release any previously allocated private
 * data structure if we have exhausted the previously filled data buffers.
 */
static void _DebugHBTraceSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	HTB_Sentinel_t	*pSentinel = (HTB_Sentinel_t *)psSeqFile->private;

	if (pSentinel != NULL)
	{
		if (pSentinel->bOverflowed)
		{
			return;
		}
		OSFreeMem(pSentinel);
		psSeqFile->private = NULL;
	}
}


/*
 * _DebugHBTraceSeqNext:
 *
 * Provide the next location to use for debug output. We return NULL to
 * start a new sequence of STOP / START / SHOW / NEXT requests to produce the
 * data. The pSentinel structure handles the necessary buffer sizing so that we
 * do not exceed the initial seq_file provided buffer limit.
 */
static void *_DebugHBTraceSeqNext(struct seq_file *psSeqFile,
                                  void *pvData,
                                  loff_t *puiPosition)
{
	loff_t			curPos;

	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);

	if (puiPosition)
	{
		curPos = *puiPosition;
		*puiPosition = curPos+1;
	}

	return (void *)NULL;		/* We are a one-shot routine */
}

static const struct seq_operations gsHTBReadOps = {
	.start = _DebugHBTraceSeqStart,
	.stop  = _DebugHBTraceSeqStop,
	.next  = _DebugHBTraceSeqNext,
	.show  = _DebugHBTraceSeqShow,
};


/******************************************************************************
 * HTB Dumping routines and definitions
 ******************************************************************************/
#define IS_VALID_FMT_STRING(FMT) (strchr(FMT, '%') != NULL)
#define MAX_STRING_SIZE (128)

typedef enum
{
	TRACEBUF_ARG_TYPE_INT,
	TRACEBUF_ARG_TYPE_ERR,
	TRACEBUF_ARG_TYPE_NONE
} TRACEBUF_ARG_TYPE;

/*
 * Array of all Host Trace log IDs used to convert the tracebuf data
 */
typedef struct _HTB_TRACEBUF_LOG_ {
	HTB_LOG_SFids eSFId;
	IMG_CHAR      *pszName;
	IMG_CHAR      *pszFmt;
	IMG_UINT32    ui32ArgNum;
} HTB_TRACEBUF_LOG;

static const HTB_TRACEBUF_LOG aLogs[] = {
#define X(a, b, c, d, e) {HTB_LOG_CREATESFID(a,b,e), #c, d, e},
	HTB_LOG_SFIDLIST
#undef X
};

static const IMG_CHAR *aGroups[] = {
#define X(A,B) #B,
	HTB_LOG_SFGROUPLIST
#undef X
};

static TRACEBUF_ARG_TYPE ExtractOneArgFmt(IMG_CHAR **, IMG_CHAR *);
/*
 * ExtractOneArgFmt
 *
 * Scan the input 'printf-like' string *ppszFmt and return the next
 * value string to be displayed. If there is no '%' format field in the
 * string we return 'TRACEBUF_ARG_TYPE_NONE' and leave the input string
 * untouched.
 *
 * Input
 *	ppszFmt          reference to format string to be decoded
 *	pszOneArgFmt     single field format from *ppszFmt
 *
 * Returns
 *	TRACEBUF_ARG_TYPE_ERR       unrecognised argument
 *	TRACEBUF_ARG_TYPE_INT       variable is of numeric type
 *	TRACEBUF_ARG_TYPE_NONE      no variable reference in *ppszFmt
 *
 * Side-effect
 *	*ppszFmt is updated to reference the next part of the format string
 *	to be scanned
 */
static TRACEBUF_ARG_TYPE ExtractOneArgFmt(
	IMG_CHAR **ppszFmt,
	IMG_CHAR *pszOneArgFmt)
{
	IMG_CHAR          *pszFmt;
	IMG_CHAR          *psT;
	IMG_UINT32        ui32Count = MAX_STRING_SIZE;
	IMG_UINT32        ui32OneArgSize;
	TRACEBUF_ARG_TYPE eRet = TRACEBUF_ARG_TYPE_ERR;

	if (NULL == ppszFmt)
		return TRACEBUF_ARG_TYPE_ERR;

	pszFmt = *ppszFmt;
	if (NULL == pszFmt)
		return TRACEBUF_ARG_TYPE_ERR;

	/*
	 * Find the first '%'
	 * NOTE: we can be passed a simple string to display which will have no
	 * parameters embedded within it. In this case we simply return
	 * TRACEBUF_ARG_TYPE_NONE and the string contents will be the full pszFmt
	 */
	psT = strchr(pszFmt, '%');
	if (psT == NULL)
	{
		return TRACEBUF_ARG_TYPE_NONE;
	}

	/* Find next conversion identifier after the initial '%' */
	while ((*psT++) && (ui32Count-- > 0))
	{
		switch (*psT)
		{
			case 'd':
			case 'i':
			case 'o':
			case 'u':
			case 'x':
			case 'X':
			{
				eRet = TRACEBUF_ARG_TYPE_INT;
				goto _found_arg;
			}
			case 's':
			{
				eRet = TRACEBUF_ARG_TYPE_ERR;
				goto _found_arg;
			}
		}
	}

	if ((psT == NULL) || (ui32Count == 0)) return TRACEBUF_ARG_TYPE_ERR;

_found_arg:
	ui32OneArgSize = psT - pszFmt + 1;
	OSCachedMemCopy(pszOneArgFmt, pszFmt, ui32OneArgSize);
	pszOneArgFmt[ui32OneArgSize] = '\0';

	*ppszFmt = psT + 1;

	return eRet;
}

static IMG_UINT32 idToLogIdx(IMG_UINT32);
static IMG_UINT32 idToLogIdx(IMG_UINT32 ui32CheckData)
{
	IMG_UINT32	i = 0;
	for (i = 0; aLogs[i].eSFId != HTB_SF_LAST; i++)
	{
		if ( ui32CheckData == aLogs[i].eSFId )
			return i;
	}
	/* Nothing found, return max value */
	return HTB_SF_LAST;
}

/*
 * HandleOverflow
 *
 * For Linux-based kernel debugFS output, we have to take special steps to
 * ensure that we do not flood the associated seq_file output file with our
 * data stream.
 * If we exceed the 'size' of the file the seq_file handler in the kernel code
 * will simply free the buffer and reallocate with twice the size. Unfortunately
 * this will lose the previously filled data.
 * To avoid this, we check for there being sufficient space remaining in the
 * seq_file handle, and if the new data addition would exceed this we flag it
 * as an overflow, and set-up the transfer to continue at the current failure
 * point on the next invocation.
 *
 * Input parameters:
 *		pSentinel       - handle to private data containing offset and buffer
 *		                  location
 *		pvDumpDebugFile - handle to the debugFS instance we are updating
 *		pCurrent        - current byte position in the HTB data stream
 *		ui32Size        - number of bytes to reserve in the output stream
 *		ui32NewTotal    - new transfer count to reset our private handle to
 *
 * Return Value:
 *	IMG_FALSE   - no overflow condition
 *	IMG_TRUE    - overflow would occur - Sentinel has been set to recover from
 *	              current position within the data buffer
 */
static IMG_BOOL HandleOverflow(HTB_Sentinel_t *, void *, IMG_PBYTE, IMG_UINT32, IMG_UINT32);
static IMG_BOOL
HandleOverflow(
	HTB_Sentinel_t *pSentinel,  // Private data handle
	void *pvDumpDebugFile,      // Output file reference handle
	IMG_PBYTE  pCurrent,        // Current position in buffer
	IMG_UINT32 ui32Size,        // Data stream size to reserve
	IMG_UINT32 ui32NewTotal)    // New total to reset to
{
	struct seq_file *psSeqFile = (struct seq_file *)pvDumpDebugFile;

	/*
	 * Make sure we don't overwrite any previous information for an existing
	 * overflow condition.
	 * In this case we update the pSentinel fields when we are performing the
	 * subsequent resubmission.
	 */
	if (pSentinel->bOverflowed)
	{
		return IMG_TRUE;
	}

	if (psSeqFile->count + ui32Size >= psSeqFile->size)
	{
		pSentinel->bOverflowed = IMG_TRUE;

		pSentinel->pOvflow = pCurrent;
		pSentinel->uiBufOvLen = pSentinel->uiBufLen -
		    (IMG_UINT32)(pCurrent - pSentinel->pBuf);
		pSentinel->uiTotal = ui32NewTotal;
	}
	else
		pSentinel->bOverflowed = IMG_FALSE;

	return pSentinel->bOverflowed;
}

/*
 * DecodeHTB
 *
 * Scan the data buffer between [pBuf..pBuf+ui32BufLen)
 * Look for a valid Header within these constraints and decode any identified
 * data records held there-in.
 *
 * If called with a partially written buffer we will have ->bOverflowed set
 * this means we have to reprocess the entire ->pOvflow .. + ->uiBufOvLen range
 * and try to commit that to the new buffer.
 *
 *	Input
 *		pSentinel reference to newly read data and pending completion data
 *		          from a previous invocation [handle seq_file buffer overflow]
 *		 -> pBuf         reference to raw data that we are to parse
 *		 -> uiBufLen     number of bytes of data to parse
 *		 -> bOverflowed  Set if we've overcommitted to the output file
 *		 -> bHeaderFlag  display a header for the data
 *		 -> pOvflow      point at which to start re-processing for Overflow
 *		 -> uiBufOvLen   length of overflow data to process
 *
 *		pvDumpDebugFile     output file
 *		pfnDumpDebugPrintf  output generating routine
 */
static void
DecodeHTB(HTB_Sentinel_t *pSentinel,
	void *pvDumpDebugFile, DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf)
{
	IMG_UINT32	*ui32TraceBuf = (IMG_UINT32 *)pSentinel->pBuf;
	IMG_UINT32	ui32TraceOff = 0;

	IMG_UINT32	ui32Data, ui32LogIdx, ui32ArgsCur;
	IMG_CHAR	*pszFmt = NULL;
	IMG_CHAR	aszOneArgFmt[MAX_STRING_SIZE];
	IMG_BOOL	bUnrecognizedErrorPrinted = IMG_FALSE;
	IMG_CHAR	aszHeader[80];		// Header display string

	IMG_UINT32	ui32DataSize;
	IMG_UINT32	uiBufLen = pSentinel->uiBufLen;
	size_t	nPrinted;
	IMG_UINT32	ui32CurTotal = pSentinel->uiTotal;	// Current total on entry

	IMG_PBYTE	pNext, pLast, pStart, pData = NULL;
	IMG_PBYTE	pCurrent;		/* Current processing point within buffer */
	PVRSRVTL_PPACKETHDR	ppHdr;	/* Current packet header */

	/* Convert from byte to uint32 size */
	ui32DataSize = uiBufLen / sizeof (IMG_UINT32);

	pLast = pSentinel->pBuf + pSentinel->uiBufLen;
	pStart = pSentinel->pBuf;

	/*
	 * Display a meaningful header for the output data
	 */
	if (pSentinel->bHeaderFlag && !pSentinel->bOverflowed)
	{
		nPrinted = OSSNPrintf(aszHeader, sizeof (aszHeader),
			"%-10s:%-5s-%s  %s\n",
			"Timestamp", "Proc ID", "Group", "Log Entry");

		PVR_DUMPDEBUG_LOG(aszHeader);
		OSCachedMemSet(aszHeader, 0, sizeof (aszHeader));
		OSCachedMemSet(aszHeader, '=', nPrinted);
		aszHeader[nPrinted - 1] = '\n';
		PVR_DUMPDEBUG_LOG(aszHeader);
	}

	/*
	 * Need to determine if we are likely to hit buffer overflow before we
	 * start producing data. If we are, we will need to fill the buffer and
	 * then re-fill it on the subsequent call which should have had a new
	 * back-end buffer allocated via the seq_file routines.
	 * Note: we may well have to re-parse the data multiple times if we have
	 * a lot to process. To limit the O(n^2) behaviour of this, we split
	 * the output and attempt to never hit the overflow condition.
	 * Subsequent calls will be marked with bOverflowed and pOvflow refers
	 * to next location within [pBuf .. pBuf+uiBufLen) to scan. uiBufOvLen
	 * is the remaining amount of data to process.
	 */
	if (pSentinel->bOverflowed)
	{
		pSentinel->bOverflowed = IMG_FALSE;		/* Reset and try again */
		pSentinel->uiRetries++;

		uiBufLen = pSentinel->uiBufOvLen;
		ui32TraceBuf = (IMG_UINT32 *)pSentinel->pOvflow;
		ui32DataSize = uiBufLen / sizeof (IMG_UINT32);

		pLast = pSentinel->pOvflow + pSentinel->uiBufOvLen;
		pStart = pSentinel->pOvflow;

		pSentinel->pOvflow = NULL;
		pSentinel->uiBufOvLen = 0;
	}

	pNext = pStart;
	do
	{
		IMG_BOOL	bPacketsDropped;
		IMG_UINT32	uiHdrType;

		/* Seek for next valid header */
		do
		{

			if (pNext >= pLast)
			{
				return;
			}

			/*
			 * We should have a header followed by data block(s) in the stream.
			 * Show the header type and data length.
			 */

			pCurrent = pNext;
			ppHdr = GET_PACKET_HDR(pCurrent);

			if (ppHdr == NULL)
			{
				PVR_DPF((PVR_DBG_ERROR,
			    	"%s: Unexpected NULL packet in Host Trace buffer",
					__func__));
				return;		// This should never happen
			}

			uiHdrType = GET_PACKET_TYPE(ppHdr);

			/*
			 * This should *NEVER* fire. If it does it means we have got some
			 * dubious packet header back from the HTB stream. In this case
			 * the sensible thing is to abort processing and return to
			 * the caller
			 */
			PVR_ASSERT(uiHdrType < PVRSRVTL_PACKETTYPE_LAST &&
				uiHdrType > PVRSRVTL_PACKETTYPE_UNDEF);

			if (uiHdrType < PVRSRVTL_PACKETTYPE_LAST &&
				uiHdrType > PVRSRVTL_PACKETTYPE_UNDEF)
			{
				/*
				 * We have a (potentially) valid data header. We should see if
				 * the associated packet header matches one of our expected
				 * types.
				 */
				pNext = (IMG_PBYTE)GET_NEXT_PACKET_ADDR(ppHdr);
				pData = GET_PACKET_DATA_PTR(ppHdr);

				/*
				 * Handle zero-length data (aka PADDING fields)
				 */
				if (pData == NULL || pData >= pLast)
				{
					return;
				}
				ui32Data = *(IMG_UINT32 *)pData;
				ui32LogIdx = idToLogIdx(ui32Data);

				ui32TraceOff = (IMG_UINT32)(pData - pStart) /
					sizeof (IMG_UINT32);
			}
			else
			{
				PVR_DPF((PVR_DBG_WARNING, "Unexpected Header @%p value %x",
					ppHdr, uiHdrType));

				return;
			}

			/*
			 * Check if the unrecognized ID is valid and therefore, tracebuf
			 * needs updating.
			 */
			if (HTB_SF_LAST == ui32LogIdx && HTB_LOG_VALIDID(ui32Data)
				&& IMG_FALSE == bUnrecognizedErrorPrinted
				)
			{
				PVR_DPF((PVR_DBG_WARNING,
				    "%s: Unrecognised LOG value '%x' GID %x Params %d "
					"ID %x @ '%p'",
					__func__, ui32Data, HTB_SF_GID(ui32Data),
					HTB_SF_PARAMNUM(ui32Data), ui32Data & 0xfff,
					pData));
				bUnrecognizedErrorPrinted = IMG_FALSE;
			}
		} while (HTB_SF_LAST == ui32LogIdx);

		/* The string format we are going to display */
		/*
		 * The display will show the header (log-ID, group-ID, number of params)
		 * The maximum parameter list length = 15 (only 4bits used to encode)
		 * so we need HEADER + 15 * sizeof (UINT32)
		 */
		pszFmt = aLogs[ui32LogIdx].pszFmt;

		/* add the message payload size to the running count */
		ui32ArgsCur = HTB_SF_PARAMNUM(ui32Data);

		/* Determine if we've over-filled the buffer and had to drop packets */
		bPacketsDropped = CHECK_PACKETS_DROPPED(ppHdr);
		if (bPacketsDropped ||
			(uiHdrType == PVRSRVTL_PACKETTYPE_MOST_RECENT_WRITE_FAILED))
		{
			/* Flag this as it is useful to know ... */
			nPrinted = OSSNPrintf(aszOneArgFmt, sizeof (aszOneArgFmt),
"\n<========================== *** PACKETS DROPPED *** ======================>\n");

			if (HandleOverflow(pSentinel, pvDumpDebugFile, pCurrent, nPrinted,
				ui32CurTotal))
			{
				/* We will overflow. Abort this processing early */
				PVR_DPF((PVR_DBG_WARNING,
					"%s: Buffer Overflow %d done PACKETS DROPPED!!!",
					__func__, pSentinel->uiTotal));

				return;
			}
			else
			{
				PVR_DUMPDEBUG_LOG(aszOneArgFmt);
			}
		}

		/*
		 * only process if the entire message is in the buffer.
		 * TBD: This should be a given seeing as we are interrogating the
		 * TL provided data and that *ought* to be in the correct format...
		 */
		{
			IMG_UINT32 ui32Timestamp, ui32PID;
			IMG_CHAR	szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
			IMG_CHAR	*pszBuffer = szBuffer;
			size_t		uBufSize = sizeof ( szBuffer );
			IMG_UINT32	*pui32Data = (IMG_UINT32 *)pData;

			// Get PID field from data stream
			pui32Data++;
			ui32PID = *pui32Data;
			// Get Timestamp from data stream
			pui32Data++;
			ui32Timestamp = *pui32Data;
			// Move to start of message contents data
			pui32Data++;

			ui32TraceOff = (IMG_UINT32)(pui32Data - (IMG_UINT32 *)pStart) /
				 sizeof (IMG_UINT32);
			/*
			 * We need to snprintf the data to a local in-kernel buffer
			 * and then PVR_DUMPDEBUG_LOG() that in one shot
			 */
 			nPrinted = OSSNPrintf(szBuffer, uBufSize, "%10u:%5u-%s> ",
				ui32Timestamp, ui32PID, aGroups[HTB_SF_GID(ui32Data)]);
			if (nPrinted >= uBufSize)
			{
				PVR_DUMPDEBUG_LOG("Buffer overrun - %ld printed, max space %ld",
					(long) nPrinted, (long) uBufSize);
			}

			/* Update where our next 'output' point in the buffer is */
			pszBuffer += OSStringLength(szBuffer);
			uBufSize -= (pszBuffer - szBuffer);

			/*
			 * Print one argument at a time as this simplifies handling variable
			 * number of arguments. Special case handling for no arguments.
			 * This is the case for simple format strings such as
			 * HTB_SF_MAIN_KICK_UNCOUNTED.
			 */
			if (ui32ArgsCur == 0)
			{
				if (pszFmt)
				{
					nPrinted = OSSNPrintf(pszBuffer, uBufSize, pszFmt);
					if (nPrinted >= uBufSize)
					{
						PVR_DUMPDEBUG_LOG("Buffer overrun - %ld printed,"
							" max space %ld", (long) nPrinted, (long) uBufSize);
					}
					pszBuffer = szBuffer + OSStringLength(szBuffer);
					uBufSize -= (pszBuffer - szBuffer);
				}
			}
			else
			{
				while ( IS_VALID_FMT_STRING(pszFmt) && (uBufSize > 0) )
				{
					IMG_UINT32 ui32TmpArg = *pui32Data;
					TRACEBUF_ARG_TYPE eArgType;

					eArgType = ExtractOneArgFmt(&pszFmt, aszOneArgFmt);

					ui32TraceOff++;
					pui32Data++;
					ui32ArgsCur--;

					switch (eArgType)
					{
						case TRACEBUF_ARG_TYPE_INT:
							nPrinted = OSSNPrintf(pszBuffer, uBufSize,
								aszOneArgFmt, ui32TmpArg);
							break;

						case TRACEBUF_ARG_TYPE_NONE:
							nPrinted = OSSNPrintf(pszBuffer, uBufSize, pszFmt);
							break;

						default:
							nPrinted = OSSNPrintf(pszBuffer, uBufSize,
								"Error processing  arguments, type not "
								"recognized (fmt: %s)", aszOneArgFmt);
							break;
					}
					if (nPrinted >= uBufSize)
					{
						PVR_DUMPDEBUG_LOG("Buffer overrun - %ld printed,"
							" max space %ld", (long) nPrinted, (long) uBufSize);
					}
					pszBuffer = szBuffer + OSStringLength(szBuffer);
					uBufSize -= (pszBuffer - szBuffer);
				}
				/* Display any remaining text in pszFmt string */
				if (pszFmt)
				{
					nPrinted = OSSNPrintf(pszBuffer, uBufSize, pszFmt);
					if (nPrinted >= uBufSize)
					{
						PVR_DUMPDEBUG_LOG("Buffer overrun - %ld printed,"
							" max space %ld", (long) nPrinted, (long) uBufSize);
					}
					pszBuffer = szBuffer + OSStringLength(szBuffer);
					uBufSize -= (pszBuffer - szBuffer);
				}
			}

			/*
			 * Check to see if we can fit this buffer into the output file
			 * without overflowing. If we hit the size limit we are going to
			 * end up losing the data and will need to re-process into a larger
			 * buffer. This is extraordinarily expensive in VM terms and an
			 * O(N^2) time increase depending on the size of the data buffer
			 * that we are trying to process.
			 */
			uBufSize = (IMG_UINT32)(pszBuffer - szBuffer);

			g_sCtrl.pCurrent = pCurrent;

			if (HandleOverflow(pSentinel, pvDumpDebugFile, pCurrent, uBufSize,
				ui32CurTotal))
			{
				return;
			}
			PVR_DUMPDEBUG_LOG(szBuffer);

			/* Update total bytes processed */
			pSentinel->uiTotal += uBufSize;

			/*
			 * We may have overflowed the output buffer. If so, we need to
			 * rewrite the data that caused the overflow so as not to lose it
			 * on the next iteration. This is a catastrophic failure as we will
			 * need to reprocess the entire buffer so far.
			 */
			if (HandleOverflow(pSentinel, pvDumpDebugFile, pCurrent, 0, 0))
			{

				/* Reset the overflow references to the start of the data */
				pSentinel->pOvflow = pSentinel->pBuf;
				pSentinel->uiBufOvLen = pSentinel->uiBufLen;
				return;
			}
			else
			{
				ui32CurTotal += uBufSize;
			}
		}

	} while (!pSentinel->bOverflowed && (pNext < pLast));
}

/*
 * HTBAcquireData:
 *
 * Grab data from the TL mechanism and place in the pre-allocated copy-buffer
 * This allows us to release our hold on the data producers before we start
 * processing the byte stream.
 * We have no hook from the seq_file to inform us that we have had a short
 * read, so we treat all subsequent reads as 'bOverflowed' and process them
 * until we need more data.
 */
static PVRSRV_ERROR
HTBAcquireData(HTB_Sentinel_t *pSentinel)
{
	PVRSRV_ERROR eError;

	if (g_sCtrl.pRawCur)
	{
		IMG_PBYTE	pLast = g_sCtrl.pRawBuf;

		pLast += g_sCtrl.uiBufLen;

		/*
		 * If we haven't completed processing our outstanding data continue
		 * from where we were.
		 */
		if (g_sCtrl.pRawCur > g_sCtrl.pRawBuf && g_sCtrl.pCurrent < pLast)
		{
			pSentinel->pOvflow = g_sCtrl.pRawCur;
			pSentinel->uiBufOvLen = (IMG_UINT32)(pLast - g_sCtrl.pCurrent);

			pSentinel->bOverflowed = IMG_TRUE;
			return PVRSRV_OK;
		}

	}
	eError = TLClientAcquireData(DIRECT_BRIDGE_HANDLE,
		g_sCtrl.hStream, &pSentinel->pBuf, &pSentinel->uiBufLen);

	if (PVRSRV_OK == eError)
	{
		OSCachedMemCopy(g_sCtrl.pRawBuf, pSentinel->pBuf, pSentinel->uiBufLen);

		pSentinel->pBuf = g_sCtrl.pRawBuf;

		pSentinel->bOverflowed = IMG_FALSE;

		eError = TLClientReleaseData(DIRECT_BRIDGE_HANDLE, g_sCtrl.hStream);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s() failed (%s) in %s()",
				"TLClientReleaseData", PVRSRVGETERRORSTRING(eError), __func__));
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_WARNING, "%s() failed (%s) in %s()",
			"TLClientAcquireData", PVRSRVGETERRORSTRING(eError), __func__));
	}

	return eError;
}

static void
HTBReleaseData(HTB_Sentinel_t *pSentinel)
{
	static IMG_UINT32	uiVal = 0;

	OSCachedMemSet(g_sCtrl.pRawBuf, uiVal, pSentinel->uiBufLen);

	uiVal++;
	uiVal = uiVal % 256;

	g_sCtrl.pRawCur = NULL;
}

/*
 * HTBDumpBuffer: Dump the Host Trace Buffer using the TLClient API
 */
static void HTBDumpBuffer(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                          void *pvDumpDebugFile,
                          void *pvData)
{
	PVRSRV_ERROR     eError = PVRSRV_OK;
	IMG_UINT32       uiTLMode;				/* Mode flags for TL client */
	struct seq_file *psSeqFile = (struct seq_file *)pvDumpDebugFile;
	HTB_Sentinel_t  *pSentinel = (HTB_Sentinel_t *)psSeqFile->private;
	static IMG_BOOL  bGate = IMG_FALSE;		/* Binary toggle */

	/*
	 * Set the TL stream behaviour to non-blocking and no-callback. If we
	 * don't disable the callback we end up injecting HTFBWSync events on
	 * every stream open call. This seems to be by design as the stream
	 * is created with a 'notify on reader' callback. Perhaps this should be
	 * a flaggable option so that we could just disable this for our KM
	 * access ? TBD...
	 */
	uiTLMode = PVRSRV_STREAM_FLAG_ACQUIRE_NONBLOCKING|
			   PVRSRV_STREAM_FLAG_DISABLE_PRODUCER_CALLBACK;

	/*
	 * We need to handle seq_file buffer exhaustion. For the initial case
	 * we are instantiated with an empty sentinel (SEQ_START_TOKEN)
	 * In this case we need to initiate the data capture and processing.
	 * On subsequent calls we need to continue where we left off
	 * (if BufLen != 0) and only once we've processed all of the data should
	 * we close the stream down.
	 * This *may* be antisocial to other consumers of the HTB stream, but there
	 * should not be any other readers of this at all.
	 */

	/*
	 * Only open the stream if we have no pending interrupted writes from
	 * an earlier buffer exhaustion
	 */
	if (!pSentinel->bOverflowed)
	{
		if (bGate)
		{
			/*
			 * We need to handle the insertion of an FWSYNC message into
			 * the HTB on every stream open/close cycle.
			 * If we always read the HTB on every call (when no overflow was
			 * marked) we would always have data to produce.
			 * This is a 'cat -v' like behaviour. To provide a more expected
			 * 'just give me the data and then EOF'
			 * operating mode we only return data on every other 'non-overflow'
			 * call.
			 */

			bGate = IMG_FALSE;

			/*
			 * Make sure we do not have a dangling reference to the stream.
			 * This can occur if a short-read is made from the debugFS handle
			 * and we do not complete the buffer drain.
			 * In this case we treat this as a deferred overflow and set the
			 * pSentinel values up to continue from where we last produced
			 * any output.
			 */

			if (g_sCtrl.hStream != (IMG_HANDLE)0 &&
				g_sCtrl.hStream != HTB_STREAM_CLOSED)
			{
				bGate = IMG_TRUE;

				pSentinel->pBuf = g_sCtrl.pBuf;
				pSentinel->uiBufLen = g_sCtrl.uiBufLen;

				pSentinel->pOvflow = g_sCtrl.pCurrent;
				pSentinel->uiBufOvLen = g_sCtrl.uiBufLen -
					(IMG_UINT32)(pSentinel->pOvflow - pSentinel->pBuf);

				pSentinel->bOverflowed = IMG_TRUE;
			}
			else
			{
				return;
			}
		}
		else
		{
			/*
			 * Check to see that we haven't got a handle still open.
			 * If we do, we have probably had a short read (e.g.
			 * 'head /sys/kernel/debug/pvr/host_trace' which leaves us with
			 * cached data in our copy-buffer. We continue to use the active
			 * handle until we've drained the data from it.
			 */
			if (g_sCtrl.hStream == (IMG_HANDLE)0 ||
				g_sCtrl.hStream == HTB_STREAM_CLOSED)
			{
				eError = TLClientOpenStream(DIRECT_BRIDGE_HANDLE,
					g_sCtrl.szStreamName, uiTLMode, &g_sCtrl.hStream);

				if (PVRSRV_OK != eError) {
					/*
				 	 * No stream available so nothing to report
				 	 */
					return;
				}

				pSentinel->uiBufLen = 0;
				pSentinel->pBuf = NULL;
				pSentinel->bOverflowed = IMG_FALSE;

			}
			bGate = IMG_TRUE;
		}
	}

	/*
	 * We now have a handle and we should simply read / decode and dump the
	 * data stream (if any) present
	 *
	 * This is a sequence of:
	 *	while (TLClientAcquireData(handle, hStream, &pBuf, &uiBufLen) == OK)
	 *  {
	 *		Decode_Data(pBuf, uiBufLen);
	 *		TLClientReleaseData(handle, hStream);
	 *  }
 	 *
	 * To avoid holding off any other consumers of the stream we coalesce
	 * the Acquire / Release into a single operation (HTBAcquireData) and copy
	 * the data to be processed into a global copy-buffer (referenced by
	 * pRawBuf / pRawCur)
	 * Once we have consumed the entire buffer we get a new data stream from
	 * TL.
	 */

	do
	{
		if (!pSentinel->bOverflowed)
		{
			eError = HTBAcquireData(pSentinel);

			if (eError == PVRSRV_OK)
			{
				if (pSentinel->uiBufLen > 0)
				{
					g_sCtrl.pBuf = pSentinel->pBuf;
					g_sCtrl.uiBufLen = pSentinel->uiBufLen;
					g_sCtrl.pCurrent = g_sCtrl.pBuf;

					pSentinel->bHeaderFlag = (pvData == SEQ_START_TOKEN);
					DecodeHTB(pSentinel, pvDumpDebugFile, pfnDumpDebugPrintf);

					/*
					 * We may have exhausted the buffer stream capacity. If so
					 * we still have valid data which we need to process on the
					 * next iteration of the dumping loop. In this case we
					 * cannot release the data as we are going to use it later.
					 */

					if (!pSentinel->bOverflowed)
					{
						HTBReleaseData(pSentinel);
					}
				}
				else
				{
					/*
					 * We got zero bytes back. As we are in non-blocking access
					 * we have to close and re-open the stream to get any
					 * further data back. There is a limitation in the
					 * AcquireData() / Release() model which requires this.
					 */

					HTBReleaseData(pSentinel);
					g_sCtrl.pBuf = (IMG_PBYTE)NULL;
					g_sCtrl.pCurrent = (IMG_PBYTE)NULL;
					g_sCtrl.uiBufLen = 0;
				}
			}
			else
			{
				PVR_DPF((PVR_DBG_WARNING, "%s() failed (%s) in %s()",
					"HTBAcquireData", PVRSRVGETERRORSTRING(eError),
					__func__));
			}
		}
		else
		{
			/*
			 * Dump out any remaining data and reset the remaining-data
			 * flag so that we start going through more ClientData in the
			 * above 'if' block
			 */
			DecodeHTB(pSentinel, pvDumpDebugFile, pfnDumpDebugPrintf);

			if (!pSentinel->bOverflowed)
			{
				pSentinel->uiRetries = 0;

				HTBReleaseData(pSentinel);

				g_sCtrl.pBuf = (IMG_PBYTE)NULL;
				g_sCtrl.pCurrent = (IMG_PBYTE)NULL;
				g_sCtrl.uiBufLen = 0;
			}
		}
	}
	while ((eError == PVRSRV_OK) && (pSentinel->uiBufLen > 0) &&
		(!pSentinel->bOverflowed));

	/*
	 * Only close the stream if we have no deferred data to process.
	 */
	if (!pSentinel->bOverflowed)
	{
		eError = TLClientCloseStream(DIRECT_BRIDGE_HANDLE, g_sCtrl.hStream);
		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s() failed (%s) in %s()",
				"TLClientCloseStream", PVRSRVGETERRORSTRING(eError),
				__func__));
		}
		g_sCtrl.hStream = HTB_STREAM_CLOSED;
	}
}


/******************************************************************************
 * External Entry Point routines ...
 ******************************************************************************/
/*
 * HTB_CreateFSEntry
 *
 * Create the debugFS entry-point for the host-trace-buffer
 *
 * Input    uiBufSize       Size of host trace buffer configured
 *          pszName         Name associated with Host Trace buffer stream
 *
 * Returns  eError          internal error code, PVRSRV_OK on success
 */
PVRSRV_ERROR HTB_CreateFSEntry(IMG_UINT32 uiBufSize, const IMG_CHAR *pszName)
{
	PVRSRV_ERROR eError;

	eError = PVRDebugFSCreateEntry("host_trace", NULL,
	                               &gsHTBReadOps,
	                               NULL,
	                               NULL, NULL, NULL,
	                               &g_sCtrl.psDumpHostDebugFSEntry);

	PVR_LOGR_IF_ERROR(eError, "PVRDebugFSCreateEntry");

	/*
	 * Allocate space for our copy buffer. We are restricted to using the
	 * worst-case scenario of duplicating the entire configured HTBufferSize
	 * buffer. TL access makes it almost impossible to limit this to a smaller
	 * amount until we have a per-packet access mechanism. Then we can simply
	 * allocate / reallocate a per-stream maxTLpacketSize copy buffer. This
	 * can only be done once the stream is active, so we rely on being updated
	 * when/if the TLStreamCreate() is issued. Until then we pre-allocate
	 * a duplicate buffer.
	 */
	g_sCtrl.pRawBuf = (IMG_PBYTE)OSAllocZMem(uiBufSize);
	if (g_sCtrl.pRawBuf == (IMG_PBYTE)NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;

		/* Free the FS entry points as we're not able to work */
		PVRDebugFSRemoveEntry(&g_sCtrl.psDumpHostDebugFSEntry);
	}
	else
	{
		g_sCtrl.ui32BufferSize = uiBufSize;
		g_sCtrl.szStreamName = pszName;
	}

	return eError;
}

/*
 * HTB_UpdateFSEntry
 *
 * Update the allocated buffer size for the host-trace-buffer
 *
 * Input   uiBufSize       Size of host-trace-buffer
 *
 * Returns eError          internal error code, PVRSRV_OK on success
 */
PVRSRV_ERROR HTB_UpdateFSEntry(IMG_UINT32 uiBufSize)
{
	IMG_UINT32	uiCopySize;

	if (g_sCtrl.ui32BufferSize == uiBufSize)
	{
		return PVRSRV_OK;
	}

	uiCopySize = MIN(uiBufSize, g_sCtrl.ui32BufferSize);
	if (g_sCtrl.pRawBuf)
	{
		IMG_PBYTE	pNew;

		pNew = (IMG_PBYTE)OSAllocZMem(uiBufSize);

		if (!pNew)
		{
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		OSCachedMemCopy(pNew, g_sCtrl.pRawBuf, uiCopySize);
		OSFreeMem(g_sCtrl.pRawBuf);
		g_sCtrl.pRawBuf = pNew;
		g_sCtrl.ui32BufferSize = uiBufSize;
	}

	return PVRSRV_OK;
}

/*
 * HTB_DestroyFSEntry
 *
 * Destroy the debugFS entry-point created by earlier HTB_CreateFSEntry.
 * Release any allocated copy-buffer.
 */
void HTB_DestroyFSEntry(void)
{
	if (g_sCtrl.psDumpHostDebugFSEntry)
	{
		PVRDebugFSRemoveEntry(&g_sCtrl.psDumpHostDebugFSEntry);
		g_sCtrl.psDumpHostDebugFSEntry = NULL;
	}

	if (g_sCtrl.pRawBuf)
	{
		OSFreeMem(g_sCtrl.pRawBuf);
		g_sCtrl.pRawBuf = NULL;
	}
}

/* EOF */
