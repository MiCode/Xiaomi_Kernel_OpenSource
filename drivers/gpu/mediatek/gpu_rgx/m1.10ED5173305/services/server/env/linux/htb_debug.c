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
#include "htb_debug.h"
#include "kernel_compatibility.h"

// Global data handles for buffer manipulation and processing
typedef struct
{
    PPVR_DEBUGFS_ENTRY_DATA psDumpHostDebugFSEntry;	/* debugFS entry hook */
	IMG_HANDLE hStream;                 /* Stream handle for debugFS use */
} HTB_DBG_INFO;

static HTB_DBG_INFO g_sHTBData;

// Enable for extra debug level
//#define HTB_CHATTY	1

/*****************************************************************************
 * debugFS display routines
 ******************************************************************************/
static int HTBDumpBuffer(DUMPDEBUG_PRINTF_FUNC *, void *, void *);
static void _HBTraceSeqPrintf(void *, const IMG_CHAR *, ...);
static int _DebugHBTraceSeqShow(struct seq_file *, void *);
static void *_DebugHBTraceSeqStart(struct seq_file *, loff_t *);
static void _DebugHBTraceSeqStop(struct seq_file *, void *);
static void *_DebugHBTraceSeqNext(struct seq_file *, void *, loff_t *);

static void _HBTraceSeqPrintf(void *pvDumpDebugFile,
                              const IMG_CHAR *pszFormat, ...)
{
	struct seq_file *psSeqFile = (struct seq_file *)pvDumpDebugFile;
	va_list         ArgList;

	va_start(ArgList, pszFormat);
	seq_printf(psSeqFile, pszFormat, ArgList);
	va_end(ArgList);
}

static int _DebugHBTraceSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	int	retVal;

	PVR_ASSERT(NULL != psSeqFile);

	/* psSeqFile should never be NULL */
	if (psSeqFile == NULL)
	{
		return -1;
	}

	/*
	 * Ensure that we have a valid address to use to dump info from. If NULL we
	 * return a failure code to terminate the seq_read() call. pvData is either
	 * SEQ_START_TOKEN (for the initial call) or an HTB buffer address for
	 * subsequent calls [returned from the NEXT function].
	 */
	if (pvData == NULL)
	{
		return -1;
	}


	retVal = HTBDumpBuffer(_HBTraceSeqPrintf, psSeqFile, pvData);

#ifdef HTB_CHATTY
	PVR_DPF((PVR_DBG_WARNING, "%s: Returning %d", __func__, retVal));
#endif	/* HTB_CHATTY */

	return retVal;
}

typedef struct {
	IMG_PBYTE	pBuf;		/* Raw data buffer from TL stream */
	IMG_UINT32	uiBufLen;	/* Amount of data to process from 'pBuf' */
	IMG_UINT32	uiTotal;	/* Total bytes processed */
	IMG_UINT32	uiMsgLen;	/* Length of HTB message to be processed */
	IMG_PBYTE	pCurr;		/* pointer to current message to be decoded */
	IMG_CHAR    szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];	/* Output string */
} HTB_Sentinel_t;

static IMG_UINT32 idToLogIdx(IMG_UINT32);	/* Forward declaration */

/*
 * HTB_GetNextMessage
 *
 * Get next non-empty message block from the buffer held in pSentinel->pBuf
 * If we exhaust the data buffer we refill it (after releasing the previous
 * message(s) [only one non-NULL message, but PAD messages will get released
 * as we traverse them].
 *
 * Input:
 *	pSentinel		references the already acquired data buffer
 *
 * Output:
 *	pSentinel
 *		-> uiMsglen updated to the size of the non-NULL message
 *
 * Returns:
 *	Address of first non-NULL message in the buffer (if any)
 *	NULL if there is no further data available from the stream and the buffer
 *	contents have been drained.
 */
static IMG_PBYTE HTB_GetNextMessage(HTB_Sentinel_t *);
static IMG_PBYTE HTB_GetNextMessage(HTB_Sentinel_t *pSentinel)
{
	IMG_PBYTE	pNext, pLast, pStart, pData = NULL;
	IMG_PBYTE	pCurrent;		/* Current processing point within buffer */
	PVRSRVTL_PPACKETHDR	ppHdr;	/* Current packet header */
	IMG_UINT32	uiHdrType;		/* Packet header type */
	IMG_UINT32	uiMsgSize;		/* Message size of current packet (bytes) */
	IMG_UINT32	ui32DataSize;
	IMG_UINT32	uiBufLen;
	IMG_BOOL    bUnrecognizedErrorPrinted = IMG_FALSE;
	IMG_UINT32  ui32Data;
	IMG_UINT32  ui32LogIdx;
	PVRSRV_ERROR eError;

	PVR_ASSERT(NULL != pSentinel);

	uiBufLen = pSentinel->uiBufLen;
	/* Convert from byte to uint32 size */
	ui32DataSize = pSentinel->uiBufLen / sizeof (IMG_UINT32);

	pLast = pSentinel->pBuf + pSentinel->uiBufLen;

	pStart = pSentinel->pBuf;

	pNext = pStart;
	pSentinel->uiMsgLen = 0;	// Reset count for this message
	uiMsgSize = 0;				// nothing processed so far
	ui32LogIdx = HTB_SF_LAST;	// Loop terminator condition

	do
	{
		/*
		 * If we've drained the buffer we must RELEASE and ACQUIRE some more.
		 */
		if (pNext >= pLast)
		{
			eError = TLClientReleaseData(DIRECT_BRIDGE_HANDLE, g_sHTBData.hStream);
			PVR_ASSERT(eError == PVRSRV_OK);

			eError = TLClientAcquireData(DIRECT_BRIDGE_HANDLE,
				g_sHTBData.hStream, &pSentinel->pBuf, &pSentinel->uiBufLen);

			if (PVRSRV_OK != eError)
			{
				PVR_DPF((PVR_DBG_WARNING, "%s: %s FAILED '%s'", __func__,
					"TLClientAcquireData", PVRSRVGETERRORSTRING(eError)));
				return NULL;
			}

			// Reset our limits - if we've returned an empty buffer we're done.
			pLast = pSentinel->pBuf + pSentinel->uiBufLen;
			pStart = pSentinel->pBuf;
			pNext = pStart;

			if (pStart == NULL || pLast == NULL)
			{
				return NULL;
			}
		}

		/*
		 * We should have a header followed by data block(s) in the stream.
		 */

		pCurrent = pNext;
		ppHdr = GET_PACKET_HDR(pCurrent);

		if (ppHdr == NULL)
		{
			PVR_DPF((PVR_DBG_ERROR,
	    		"%s: Unexpected NULL packet in Host Trace buffer", __func__));
			pSentinel->uiMsgLen += uiMsgSize;
			return NULL;		// This should never happen
		}

		/*
		 * This should *NEVER* fire. If it does it means we have got some
		 * dubious packet header back from the HTB stream. In this case
		 * the sensible thing is to abort processing and return to
		 * the caller
		 */
		uiHdrType = GET_PACKET_TYPE(ppHdr);

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

			PVR_ASSERT(pNext != NULL);

			uiMsgSize = (IMG_UINT32)((size_t)pNext - (size_t)ppHdr);

			pSentinel->uiMsgLen += uiMsgSize;

			pData = GET_PACKET_DATA_PTR(ppHdr);

			/*
			 * Handle non-DATA packet types. These include PAD fields which
			 * may have data associated and other types. We simply discard
			 * these as they have no decodable information within them.
			 */
			if (uiHdrType != PVRSRVTL_PACKETTYPE_DATA)
			{
				/*
				 * Now release the current non-data packet and proceed to the
				 * next entry (if any).
				 */
				eError = TLClientReleaseDataLess(DIRECT_BRIDGE_HANDLE,
				    g_sHTBData.hStream, uiMsgSize);

#ifdef HTB_CHATTY
				PVR_DPF((PVR_DBG_WARNING, "%s: Packet Type %x Length %u",
					__func__, uiHdrType, uiMsgSize));
#endif

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_WARNING, "%s: %s FAILED - '%s' message"
						" size %u", __func__, "TLClientReleaseDataLess",
						PVRSRVGETERRORSTRING(eError), uiMsgSize));
				}

				eError = TLClientAcquireData(DIRECT_BRIDGE_HANDLE,
					g_sHTBData.hStream, &pSentinel->pBuf, &pSentinel->uiBufLen);

				if (PVRSRV_OK != eError)
				{
					PVR_DPF((PVR_DBG_WARNING, "%s: %s FAILED - %s Giving up",
						__func__, "TLClientAcquireData",
						PVRSRVGETERRORSTRING(eError)));

					return NULL;
				}
				pSentinel->uiMsgLen = 0;
				// Reset our limits - if we've returned an empty buffer we're done.
				pLast = pSentinel->pBuf + pSentinel->uiBufLen;
				pStart = pSentinel->pBuf;
				pNext = pStart;

				if (pStart == NULL || pLast == NULL)
				{
					return NULL;
				}
				continue;
			}
			if (pData == NULL || pData >= pLast)
			{
				continue;
			}
			ui32Data = *(IMG_UINT32 *)pData;
			ui32LogIdx = idToLogIdx(ui32Data);
		}
		else
		{
			PVR_DPF((PVR_DBG_WARNING, "Unexpected Header @%p value %x",
				ppHdr, uiHdrType));

			return NULL;
		}

		/*
		 * Check if the unrecognized ID is valid and therefore, tracebuf
		 * needs updating.
		 */
		if (HTB_SF_LAST == ui32LogIdx && HTB_LOG_VALIDID(ui32Data)
			&& IMG_FALSE == bUnrecognizedErrorPrinted)
		{
			PVR_DPF((PVR_DBG_WARNING,
			    "%s: Unrecognised LOG value '%x' GID %x Params %d ID %x @ '%p'",
				__func__, ui32Data, HTB_SF_GID(ui32Data),
				HTB_SF_PARAMNUM(ui32Data), ui32Data & 0xfff, pData));
			bUnrecognizedErrorPrinted = IMG_FALSE;
		}

	} while (HTB_SF_LAST == ui32LogIdx);

#ifdef HTB_CHATTY
	PVR_DPF((PVR_DBG_WARNING, "%s: Returning data @ %p Log value '%x'",
		__func__, pCurrent, ui32Data));
#endif	/* HTB_CHATTY */

	return pCurrent;
}

/*
 * HTB_GetFirstMessage
 *
 * Called from START to obtain the buffer address of the first message within
 * pSentinel->pBuf. Will ACQUIRE data if the buffer is empty.
 *
 * Input:
 *	pSentinel
 *	puiPosition			Offset within the debugFS file
 *
 * Output:
 *	pSentinel->pCurr	Set to reference the first valid non-NULL message within
 *						the buffer. If no valid message is found set to NULL.
 *	pSentinel
 *		->pBuf		if unset on entry
 *		->uiBufLen	if pBuf unset on entry
 *
 * Side-effects:
 *	HTB TL stream will be updated to bypass any zero-length PAD messages before
 *	the first non-NULL message (if any).
 */
static void HTB_GetFirstMessage(HTB_Sentinel_t *, loff_t *);
static void HTB_GetFirstMessage(HTB_Sentinel_t *pSentinel, loff_t *puiPosition)
{
	PVRSRV_ERROR	eError;

	if (pSentinel == NULL)
		return;

	if (pSentinel->pBuf == NULL)
	{
		/* Acquire data */
		pSentinel->uiMsgLen = 0;

		eError = TLClientAcquireData(DIRECT_BRIDGE_HANDLE,
		    g_sHTBData.hStream, &pSentinel->pBuf, &pSentinel->uiBufLen);

		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s: %s FAILED '%s'",
			    __func__, "TLClientAcquireData", PVRSRVGETERRORSTRING(eError)));

			pSentinel->pBuf = NULL;
			pSentinel->pCurr = NULL;
		}
		else
		{
			/*
			 * If there is no data available we set pSentinel->pCurr to NULL
			 * and return. This is expected behaviour if we've drained the
			 * data and nothing else has yet been produced.
			 */
			if (pSentinel->uiBufLen == 0 || pSentinel->pBuf == NULL)
			{
#ifdef HTB_CHATTY
				PVR_DPF((PVR_DBG_WARNING, "%s: Empty Buffer @ %p", __func__,
					pSentinel->pBuf));
#endif	/* HTB_CHATTY */
				pSentinel->pCurr = NULL;
				return;
			}
		}
	}

	/* Locate next message within buffer. NULL => no more data to process */
	pSentinel->pCurr = HTB_GetNextMessage(pSentinel);
}

/*
 * _DebugHBTraceSeqStart:
 *
 * Returns the address to use for subsequent 'Show', 'Next', 'Stop' file ops.
 * Return SEQ_START_TOKEN for the very first call and allocate a sentinel for
 * use by the 'Show' routine and its helpers.
 * This is stored in the psSeqFile->private hook field.
 *
 * We obtain access to the TLstream associated with the HTB. If this doesn't
 * exist (because no pvrdebug capture trace has been set) we simply return with
 * a NULL value which will stop the seq_file traversal.
 */
static void *_DebugHBTraceSeqStart(struct seq_file *psSeqFile,
                                   loff_t *puiPosition)
{
	HTB_Sentinel_t	*pSentinel = (HTB_Sentinel_t *)psSeqFile->private;
	PVRSRV_ERROR	eError;
	IMG_UINT32		uiTLMode;
	void			*retVal;
	IMG_HANDLE		hStream;

	/* Open the stream in non-blocking mode so that we can determine if there
	 * is no data to consume. Also disable the producer callback (if any) and
	 * the open callback so that we do not generate spurious trace data when
	 * accessing the stream.
	 */
	uiTLMode = PVRSRV_STREAM_FLAG_ACQUIRE_NONBLOCKING|
			   PVRSRV_STREAM_FLAG_DISABLE_PRODUCER_CALLBACK|
			   PVRSRV_STREAM_FLAG_IGNORE_OPEN_CALLBACK;

	eError = TLClientOpenStream(DIRECT_BRIDGE_HANDLE, HTB_STREAM_NAME, uiTLMode,
	    &hStream);

	if (PVRSRV_ERROR_ALREADY_OPEN == eError)
	{
		/* Stream allows only one reader so return error if it's already
		 * opened. */
#ifdef HTB_CHATTY
		PVR_DPF((PVR_DBG_WARNING, "%s: Stream handle %p already exists for %s",
		    __func__, g_sHTBData.hStream, HTB_STREAM_NAME));
#endif
		return ERR_PTR(-EBUSY);
	}
	else if (PVRSRV_OK != eError)
	{
		/*
		 * No stream available so nothing to report
		 */
		return NULL;
	}

	PVR_ASSERT(g_sHTBData.hStream == NULL);
	g_sHTBData.hStream = hStream;

	/*
	 * Ensure we have our debug-specific data store allocated and hooked from
	 * our seq_file private data.
	 * If the allocation fails we can safely return NULL which will stop
	 * further calls from the seq_file routines (NULL return from START or NEXT
	 * means we have no (more) data to process)
	 */
	if (pSentinel == NULL)
	{
		pSentinel = (HTB_Sentinel_t *)OSAllocZMem(sizeof (HTB_Sentinel_t));
		psSeqFile->private = pSentinel;
	}

	/*
	 * Find the first message location within pSentinel->pBuf
	 * => for SEQ_START_TOKEN we must issue our first ACQUIRE, also for the
	 * subsequent re-START calls (if any).
	 */

	HTB_GetFirstMessage(pSentinel, puiPosition);

	if (*puiPosition == 0)
	{
		retVal = SEQ_START_TOKEN;
	}
	else
	{
		if (pSentinel == NULL)
		{
			retVal = NULL;
		}
		else
		{
			retVal = (void *)pSentinel->pCurr;
		}
	}

#ifdef HTB_CHATTY
	PVR_DPF((PVR_DBG_WARNING, "%s: Returning %p, Stream %s @ %p", __func__,
		 retVal, HTB_STREAM_NAME, g_sHTBData.hStream));
#endif	/* HTB_CHATTY */

	return retVal;

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
	IMG_UINT32		uiMsgLen;

	if (NULL == pSentinel)
		return;

	uiMsgLen = pSentinel->uiMsgLen;

#ifdef HTB_CHATTY
	PVR_DPF((PVR_DBG_WARNING, "%s: MsgLen = %d", __func__, uiMsgLen));
#endif	/* HTB_CHATTY */

	/* If we get here the handle should never be NULL because
	 * _DebugHBTraceSeqStart() shouldn't allow that. */
	if (g_sHTBData.hStream != NULL)
	{
		PVRSRV_ERROR eError;

		if (uiMsgLen != 0)
		{
			eError = TLClientReleaseDataLess(DIRECT_BRIDGE_HANDLE,
				g_sHTBData.hStream, uiMsgLen);

			if (PVRSRV_OK != eError)
			{
				PVR_DPF((PVR_DBG_WARNING, "%s: %s FAILED - %s, nBytes %u",
					__func__, "TLClientReleaseDataLess",
					PVRSRVGETERRORSTRING(eError), uiMsgLen));
			}
		}

		eError = TLClientCloseStream(DIRECT_BRIDGE_HANDLE, g_sHTBData.hStream);
		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s() failed (%s) in %s()",
				"TLClientCloseStream", PVRSRVGETERRORSTRING(eError),
				__func__));
		}
		g_sHTBData.hStream = NULL;
	}

	if (pSentinel != NULL)
	{
		psSeqFile->private = NULL;
		OSFreeMem(pSentinel);
	}
}


/*
 * _DebugHBTraceSeqNext:
 *
 * This is where we release any acquired data which has been processed by the
 * SeqShow routine. If we have encountered a seq_file overflow we stop
 * processing and return NULL. Otherwise we release the message that we
 * previously processed and simply update our position pointer to the next
 * valid HTB message (if any)
 */
static void *_DebugHBTraceSeqNext(struct seq_file *psSeqFile,
                                  void *pvData,
                                  loff_t *puiPosition)
{
	loff_t			curPos;
	HTB_Sentinel_t	*pSentinel = (HTB_Sentinel_t *)psSeqFile->private;
	PVRSRV_ERROR	eError;

	PVR_UNREFERENCED_PARAMETER(pvData);

	if (puiPosition)
	{
		curPos = *puiPosition;
		*puiPosition = curPos+1;
	}

	/*
	 * Determine if we've had an overflow on the previous 'Show' call. If so
	 * we leave the previously acquired data in the queue (by releasing 0 bytes)
	 * and return NULL to end this seq_read() iteration.
	 * If we have not overflowed we simply get the next HTB message and use that
	 * for our display purposes
	 */

	if (seq_has_overflowed(psSeqFile))
	{
		(void)TLClientReleaseDataLess(DIRECT_BRIDGE_HANDLE, g_sHTBData.hStream, 0);

#ifdef HTB_CHATTY
		PVR_DPF((PVR_DBG_WARNING, "%s: OVERFLOW - returning NULL", __func__));
#endif	/* HTB_CHATTY */

		return (void *)NULL;
	}
	else
	{
		eError = TLClientReleaseDataLess(DIRECT_BRIDGE_HANDLE, g_sHTBData.hStream,
		    pSentinel->uiMsgLen);

		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s: %s FAILED '%s' @ %p Length %d",
				__func__, "TLClientReleaseDataLess",
				PVRSRVGETERRORSTRING(eError), pSentinel->pCurr,
			    pSentinel->uiMsgLen));
			PVR_DPF((PVR_DBG_WARNING, "%s: Buffer @ %p..%p", __func__,
				pSentinel->pBuf,
				(IMG_PBYTE)(pSentinel->pBuf+pSentinel->uiBufLen)));

		}

		eError = TLClientAcquireData(DIRECT_BRIDGE_HANDLE,
		    g_sHTBData.hStream, &pSentinel->pBuf, &pSentinel->uiBufLen);

		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s: %s FAILED '%s'\nPrev message len %d",
			    __func__, "TLClientAcquireData", PVRSRVGETERRORSTRING(eError),
				pSentinel->uiMsgLen));
			pSentinel->pBuf = NULL;
		}

		pSentinel->uiMsgLen = 0;	// We don't (yet) know the message size
	}

#ifdef HTB_CHATTY
	PVR_DPF((PVR_DBG_WARNING, "%s: Returning %p Msglen %d",
		__func__, pSentinel->pBuf, pSentinel->uiMsgLen));
#endif	/* HTB_CHATTY */

	if (pSentinel->pBuf == NULL || pSentinel->uiBufLen == 0)
	{
		return NULL;
	}

	pSentinel->pCurr = HTB_GetNextMessage(pSentinel);

	return pSentinel->pCurr;
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
static const IMG_UINT32 uiMax_aGroups = ARRAY_SIZE(aGroups) - 1;

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
 * DecodeHTB
 *
 * Decode the data buffer message located at pBuf. This should be a valid
 * HTB message as we are provided with the start of the buffer. If empty there
 * is no message to process. We update the uiMsgLen field with the size of the
 * HTB message that we have processed so that it can be returned to the system
 * on successful logging of the message to the output file.
 *
 *	Input
 *		pSentinel reference to newly read data and pending completion data
 *		          from a previous invocation [handle seq_file buffer overflow]
 *		 -> pBuf         reference to raw data that we are to parse
 *		 -> uiBufLen     total number of bytes of data available
 *		 -> pCurr        start of message to decode
 *
 *		pvDumpDebugFile     output file
 *		pfnDumpDebugPrintf  output generating routine
 *
 * Output
 *		pSentinel
 *		 -> uiMsgLen	length of the decoded message which will be freed to
 *						the system on successful completion of the seq_file
 *						update via _DebugHBTraceSeqNext(),
 * Return Value
 *		0				successful decode
 *		-1				unsuccessful decode
 */
static int
DecodeHTB(HTB_Sentinel_t *pSentinel,
	void *pvDumpDebugFile, DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf)
{
	IMG_UINT32	ui32Data, ui32LogIdx, ui32ArgsCur;
	IMG_CHAR	*pszFmt = NULL;
	IMG_CHAR	aszOneArgFmt[MAX_STRING_SIZE];
	IMG_BOOL	bUnrecognizedErrorPrinted = IMG_FALSE;

	IMG_UINT32	ui32DataSize;
	IMG_UINT32	uiBufLen = pSentinel->uiBufLen;
	size_t	nPrinted;

	IMG_PBYTE	pNext, pLast, pStart, pData = NULL;
	PVRSRVTL_PPACKETHDR	ppHdr;	/* Current packet header */
	IMG_UINT32	uiHdrType;		/* Packet header type */
	IMG_UINT32	uiMsgSize;		/* Message size of current packet (bytes) */
	IMG_BOOL	bPacketsDropped;

	/* Convert from byte to uint32 size */
	ui32DataSize = uiBufLen / sizeof (IMG_UINT32);

	pLast = pSentinel->pBuf + pSentinel->uiBufLen;
	pStart = pSentinel->pCurr;

	pNext = pStart;
	pSentinel->uiMsgLen = 0;	// Reset count for this message
	uiMsgSize = 0;				// nothing processed so far
	ui32LogIdx = HTB_SF_LAST;	// Loop terminator condition

#ifdef HTB_CHATTY
	PVR_DPF((PVR_DBG_WARNING, "%s: Buf @ %p..%p, Length = %d", __func__,
		pStart, pLast, uiBufLen));
#endif	/* HTB_CHATTY */

	/*
	 * We should have a DATA header with the necessary information following
	 */
	ppHdr = GET_PACKET_HDR(pStart);

	if (ppHdr == NULL)
	{
			PVR_DPF((PVR_DBG_ERROR,
	    		"%s: Unexpected NULL packet in Host Trace buffer", __func__));
			return -1;
	}

	uiHdrType = GET_PACKET_TYPE(ppHdr);
	PVR_ASSERT(uiHdrType == PVRSRVTL_PACKETTYPE_DATA);

	pNext = (IMG_PBYTE)GET_NEXT_PACKET_ADDR(ppHdr);

	PVR_ASSERT(pNext != NULL);

	uiMsgSize = (IMG_UINT32)((size_t)pNext - (size_t)ppHdr);

	pSentinel->uiMsgLen += uiMsgSize;

	pData = GET_PACKET_DATA_PTR(ppHdr);

	if (pData == NULL || pData >= pLast)
	{
#ifdef HTB_CHATTY
		PVR_DPF((PVR_DBG_WARNING, "%s: pData = %p, pLast = %p Returning 0",
			__func__, pData, pLast));
#endif	/* HTB_CHATTY */
		return 0;
	}

	ui32Data = *(IMG_UINT32 *)pData;
	ui32LogIdx = idToLogIdx(ui32Data);

	/*
	 * Check if the unrecognized ID is valid and therefore, tracebuf
	 * needs updating.
	 */
	if (HTB_SF_LAST == ui32LogIdx && HTB_LOG_VALIDID(ui32Data)
		&& IMG_FALSE == bUnrecognizedErrorPrinted)
	{
		PVR_DPF((PVR_DBG_WARNING,
		    "%s: Unrecognised LOG value '%x' GID %x Params %d ID %x @ '%p'",
			__func__, ui32Data, HTB_SF_GID(ui32Data),
			HTB_SF_PARAMNUM(ui32Data), ui32Data & 0xfff, pData));
		bUnrecognizedErrorPrinted = IMG_FALSE;

		return 0;
	}

	/* The string format we are going to display */
	/*
	 * The display will show the header (log-ID, group-ID, number of params)
	 * The maximum parameter list length = 15 (only 4bits used to encode)
	 * so we need HEADER + 15 * sizeof (UINT32) and the displayed string
	 * describing the event. We use a buffer in the per-process pSentinel
	 * structure to hold the data.
	 */
	// <MTK ADD
	if (ui32LogIdx >= sizeof(aLogs) / sizeof(HTB_TRACEBUF_LOG)) {
		PVR_DPF((PVR_DBG_WARNING,
			"%s: ui32LogIdx %u is out of range",
			__func__, ui32LogIdx));

		return 0;
	}
	//  MTK ADD>
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

		PVR_DUMPDEBUG_LOG(aszOneArgFmt);
	}

	{
		IMG_UINT32 ui32Timestamp, ui32PID;
		IMG_CHAR	*szBuffer = pSentinel->szBuffer;	// Buffer start
		IMG_CHAR	*pszBuffer = pSentinel->szBuffer;	// Current place in buf
		size_t		uBufSize = sizeof ( pSentinel->szBuffer );
		IMG_UINT32	*pui32Data = (IMG_UINT32 *)pData;
		IMG_UINT32	ui_aGroupIdx;

		// Get PID field from data stream
		pui32Data++;
		ui32PID = *pui32Data;
		// Get Timestamp from data stream
		pui32Data++;
		ui32Timestamp = *pui32Data;
		// Move to start of message contents data
		pui32Data++;

		/*
		 * We need to snprintf the data to a local in-kernel buffer
		 * and then PVR_DUMPDEBUG_LOG() that in one shot
		 */
		ui_aGroupIdx = MIN(HTB_SF_GID(ui32Data), uiMax_aGroups);
 		nPrinted = OSSNPrintf(szBuffer, uBufSize, "%10u:%5u-%s> ",
			ui32Timestamp, ui32PID, aGroups[ui_aGroupIdx]);
		if (nPrinted >= uBufSize)
		{
			PVR_DUMPDEBUG_LOG("Buffer overrun - %ld printed, max space %ld\n",
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
						" max space %ld\n", (long) nPrinted, (long) uBufSize);
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
						" max space %ld\n", (long) nPrinted, (long) uBufSize);
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
						" max space %ld\n", (long) nPrinted, (long) uBufSize);
				}
				pszBuffer = szBuffer + OSStringLength(szBuffer);
				uBufSize -= (pszBuffer - szBuffer);
			}
		}

		uBufSize = (IMG_UINT32)(pszBuffer - szBuffer);

		PVR_DUMPDEBUG_LOG(szBuffer);

		/* Update total bytes processed */
		pSentinel->uiTotal += uBufSize;
	}
	return 0;
}

/*
 * HTBDumpBuffer: Dump the Host Trace Buffer using the TLClient API
 *
 * This routine just parses *one* message from the buffer.
 * The stream will be opened by the Start() routine, closed by the Stop() and
 * updated for data consumed by this routine once we have DebugPrintf'd it.
 * We use the new TLReleaseDataLess() routine which enables us to update the
 * HTB contents with just the amount of data we have successfully processed.
 * If we need to leave the data available we can call this with a 0 count.
 * This will happen in the case of a buffer overflow so that we can reprocess
 * any data which wasn't handled before.
 *
 * In case of overflow or an error we return -1 otherwise 0
 *
 * Input:
 *  pfnDumpDebugPrintf  output routine to display data
 *  pvDumpDebugFile     seq_file handle (from kernel seq_read() call)
 *  pvData              data address to start dumping from
 *                      (set by Start() / Next())
 */
static int HTBDumpBuffer(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                          void *pvDumpDebugFile,
                          void *pvData)
{
	struct seq_file *psSeqFile = (struct seq_file *)pvDumpDebugFile;
	HTB_Sentinel_t  *pSentinel = (HTB_Sentinel_t *)psSeqFile->private;

	PVR_ASSERT(NULL != pvData);

	if (pvData == SEQ_START_TOKEN)
	{
		if (pSentinel->pCurr == NULL)
		{
#ifdef HTB_CHATTY
			PVR_DPF((PVR_DBG_WARNING, "%s: SEQ_START_TOKEN, Empty buffer",
				__func__));
#endif	/* HTB_CHATTY */
			return 0;
		}
		PVR_ASSERT(pSentinel->pCurr != NULL);

		/* Display a Header as we have data to process */
		seq_printf(psSeqFile, "%-10s:%-5s-%s  %s\n",
			"Timestamp", "Proc ID", "Group", "Log Entry");
	}
	else
	{
		if (pvData != NULL)
		{
			PVR_ASSERT(pSentinel->pCurr == pvData);
		}
	}

	return DecodeHTB(pSentinel, pvDumpDebugFile, pfnDumpDebugPrintf);
}


/******************************************************************************
 * External Entry Point routines ...
 ******************************************************************************/
/**************************************************************************/ /*!
 @Function     HTB_CreateFSEntry

 @Description  Create the debugFS entry-point for the host-trace-buffer

 @Returns      eError          internal error code, PVRSRV_OK on success

 */ /**************************************************************************/
PVRSRV_ERROR HTB_CreateFSEntry(void)
{
	PVRSRV_ERROR eError;

	eError = PVRDebugFSCreateEntry("host_trace", NULL,
	                               &gsHTBReadOps,
	                               NULL,
	                               NULL, NULL, NULL,
	                               &g_sHTBData.psDumpHostDebugFSEntry);

	PVR_LOGR_IF_ERROR(eError, "PVRDebugFSCreateEntry");

	return eError;
}


/**************************************************************************/ /*!
 @Function     HTB_DestroyFSEntry

 @Description  Destroy the debugFS entry-point created by earlier
               HTB_CreateFSEntry() call.
*/ /**************************************************************************/
void HTB_DestroyFSEntry(void)
{
	if (g_sHTBData.psDumpHostDebugFSEntry)
	{
		PVRDebugFSRemoveEntry(&g_sHTBData.psDumpHostDebugFSEntry);
		g_sHTBData.psDumpHostDebugFSEntry = NULL;
	}
}

/* EOF */
