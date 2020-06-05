/*************************************************************************/ /*!
@File
@Title          Debug Info framework functions and types.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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

#include "di_server.h"
#include "osdi_impl.h"
#include "pvrsrv_error.h"
#include "dllist.h"
#include "lock.h"
#include "allocmem.h"

#define ROOT_GROUP_NAME PVR_DRM_NAME

/*! Implementation object. */
typedef struct DI_IMPL
{
	const IMG_CHAR *pszName;       /*<! name of the implementation */
	OSDI_IMPL_CB sCb;              /*<! implementation callbacks */
	IMG_BOOL bInitialised;         /*<! set to IMG_TRUE after implementation
	                                    is initialised */

	DLLIST_NODE sListNode;         /*<! node element of the global list of all
	                                    implementations */
} DI_IMPL;

/*! Wrapper object for objects originating from derivative implementations.
 * This wraps both entries and groups native implementation objects. */
typedef struct DI_NATIVE_HANDLE
{
	void *pvHandle;                /*!< opaque handle to the native object */
	DI_IMPL *psDiImpl;             /*!< implementation pvHandle is associated
	                                    with */
	DLLIST_NODE sListNode;         /*!< node element of native handles list */
} DI_NATIVE_HANDLE;

/*! Debug Info entry object.
 *
 * Depending on the implementation this can be represented differently. For
 * example for the DebugFS this translates to a file.
 */
struct DI_ENTRY
{
	const IMG_CHAR *pszName;       /*!< name of the entry */
	DI_ITERATOR_CB sIterCb;        /*!< iterator interface for the entry */
	DLLIST_NODE sNativeHandleList; /*!< list of native handles belonging to this
	                                    entry */
};

/*! Debug Info group object.
 *
 * Depending on the implementation this can be represented differently. For
 * example for the DebugFS this translates to a directory.
 */
struct DI_GROUP
{
	const IMG_CHAR *pszName;         /*!< name of the group */
	const struct DI_GROUP *psParent; /*!< parent groups */
	DLLIST_NODE sNativeHandleList;   /*! list of native handles belonging to
	                                     this group */
};

/* List of all registered implementations. */
static DECLARE_DLLIST(_g_sImpls);

/* Root group for the DI entries and groups. This group is used as a root
 * group for all other groups and entries if during creation a parent groups
 * is not given.
 * This doesn't keep references to any of the children so clients are
 * for now responsible for destroying groups and entries created by them. */
static DI_GROUP _g_sRootGroup = {
	.pszName = ROOT_GROUP_NAME
};

/* Protects access to _g_sImpls and _g_sRootGroup */
static POS_LOCK _g_hLock;

static PVRSRV_ERROR _InitImpl(DI_IMPL *psImpl)
{
	PVRSRV_ERROR eError;
	DI_NATIVE_HANDLE *psNativeGroup;

	eError = psImpl->sCb.pfnInit();
	PVR_LOG_GOTO_IF_ERROR(eError, "psImpl->pfnInit()", return_);

	psNativeGroup = OSAllocMem(sizeof(*psNativeGroup));
	PVR_LOG_GOTO_IF_NOMEM(psNativeGroup, eError, deinit_impl_);

	eError = psImpl->sCb.pfnCreateGroup(_g_sRootGroup.pszName, NULL,
										&psNativeGroup->pvHandle);
	PVR_LOG_GOTO_IF_ERROR(eError, "psImpl->sCb.pfnCreateGroup", free_memory_);

	psNativeGroup->psDiImpl = psImpl;
	dllist_add_to_head(&_g_sRootGroup.sNativeHandleList,
	                   &psNativeGroup->sListNode);

	psImpl->bInitialised = IMG_TRUE;

	return PVRSRV_OK;

free_memory_:
	OSFreeMem(psNativeGroup);
deinit_impl_:
	psImpl->sCb.pfnDeInit();
return_:
	return eError;
}

PVRSRV_ERROR DIInit(void)
{
	PVRSRV_ERROR eError = OSLockCreate(&_g_hLock);
	PVR_LOG_RETURN_IF_ERROR(eError, "OSLockCreate");

	return PVRSRV_OK;
}

PVRSRV_ERROR DIInitImplementations(void)
{
	DLLIST_NODE *psThis, *psNext;

	OSLockAcquire(_g_hLock);

	dllist_init(&_g_sRootGroup.sNativeHandleList);

	/* Loops over all implementations and initialise it. If the initialisation
	 * fails throw it away and continue with the others. */
	dllist_foreach_node(&_g_sImpls, psThis, psNext)
	{
		DI_IMPL *psImpl = IMG_CONTAINER_OF(psThis, DI_IMPL, sListNode);

		PVRSRV_ERROR eError = _InitImpl(psImpl);
		if (eError != PVRSRV_OK)
		{
			/* implementation could not be initialised so remove it from the
			 * list, free the memory and forget about it */

			PVR_DPF((PVR_DBG_ERROR, "%s: could not initialise \"%s\" debug "
			        "info implementation, discarding", __func__,
			        psImpl->pszName));

			dllist_remove_node(&psImpl->sListNode);
			OSFreeMem(psImpl);
		}
	}

	/* For now don't return error because the common DI implementation is not
	 * ready yet and this would break OSs other than Linux and Android. */
#if 0
	if (dllist_is_empty(&_g_sImpls))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: no debug info implementation exists",
		        __func__));

		return PVRSRV_ERROR_INIT_FAILURE;
	}
#endif /* 0 */

	OSLockRelease(_g_hLock);

	return PVRSRV_OK;
}

void DIDeInit(void)
{
	DLLIST_NODE *psThis, *psNext;

	OSLockAcquire(_g_hLock);

	/* Remove all of the native instances of the root group. */
	dllist_foreach_node(&_g_sRootGroup.sNativeHandleList, psThis, psNext)
	{
		DI_NATIVE_HANDLE *psNativeGroup =
		        IMG_CONTAINER_OF(psThis, DI_NATIVE_HANDLE, sListNode);
		DI_IMPL *psImpl = psNativeGroup->psDiImpl;

		psImpl->sCb.pfnDestroyGroup(psNativeGroup->pvHandle);
		dllist_remove_node(&psNativeGroup->sListNode);
		OSFreeMem(psNativeGroup);
	}

	/* Remove all of the implementations. */
	dllist_foreach_node(&_g_sImpls, psThis, psNext)
	{
		DI_IMPL *psDiImpl = IMG_CONTAINER_OF(psThis, DI_IMPL, sListNode);

		if (psDiImpl->bInitialised)
		{
			psDiImpl->sCb.pfnDeInit();
			psDiImpl->bInitialised = IMG_FALSE;
		}

		dllist_remove_node(&psDiImpl->sListNode);
		OSFreeMem(psDiImpl);
	}

	OSLockRelease(_g_hLock);

	/* all resources freed so free the lock itself too */

	OSLockDestroy(_g_hLock);
}

static IMG_BOOL _ValidateIteratorCb(const DI_ITERATOR_CB *psIterCb,
                                    DI_ENTRY_TYPE eType)
{
	IMG_UINT32 uiFlags = 0;

	if (psIterCb == NULL)
	{
		return IMG_FALSE;
	}

	if (eType == DI_ENTRY_TYPE_GENERIC)
	{
		uiFlags |= psIterCb->pfnShow != NULL ? BIT(0) : 0;
		uiFlags |= psIterCb->pfnStart != NULL ? BIT(1) : 0;
		uiFlags |= psIterCb->pfnStop != NULL ? BIT(2) : 0;
		uiFlags |= psIterCb->pfnNext != NULL ? BIT(3) : 0;

		/* either only pfnShow or all callbacks need to be set */
		if (uiFlags != BIT(0) && !BITMASK_HAS(uiFlags, 0x0f))
		{
			return IMG_FALSE;
		}
	}
	else if (eType == DI_ENTRY_TYPE_RANDOM_ACCESS)
	{
		uiFlags |= psIterCb->pfnRead != NULL ? BIT(0) : 0;
		uiFlags |= psIterCb->pfnSeek != NULL ? BIT(1) : 0;

		/* either only pfnRead or all callbacks need to be set */
		if (uiFlags != BIT(0) && !BITMASK_HAS(uiFlags, 0x03))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static PVRSRV_ERROR _CreateNativeEntry(const IMG_CHAR *pszName,
                                       DI_ENTRY *psEntry,
                                       const DI_NATIVE_HANDLE *psNativeParent,
                                       void *pvPriv,
                                       DI_ENTRY_TYPE eType,
                                       DI_NATIVE_HANDLE **ppsNativeEntry)
{
	PVRSRV_ERROR eError;
	DI_IMPL *psImpl = psNativeParent->psDiImpl;

	DI_NATIVE_HANDLE *psNativeEntry = OSAllocMem(sizeof(*psNativeEntry));
	PVR_LOG_GOTO_IF_NOMEM(psNativeEntry, eError, return_);

	eError = psImpl->sCb.pfnCreateEntry(pszName,
	                                    eType,
	                                    &psEntry->sIterCb,
	                                    pvPriv,
	                                    psNativeParent->pvHandle,
	                                    &psNativeEntry->pvHandle);
	PVR_LOG_GOTO_IF_ERROR(eError, "psImpl->sCb.pfnCreateGroup", free_memory_);

	psNativeEntry->psDiImpl = psImpl;

	*ppsNativeEntry = psNativeEntry;

	return PVRSRV_OK;

free_memory_:
	OSFreeMem(psNativeEntry);
return_:
	return eError;
}

PVRSRV_ERROR DICreateEntry(const IMG_CHAR *pszName,
                           const DI_GROUP *psGroup,
                           const DI_ITERATOR_CB *psIterCb,
                           void *pvPriv,
                           DI_ENTRY_TYPE eType,
                           DI_ENTRY **ppsEntry)
{
	PVRSRV_ERROR eError;
	DLLIST_NODE *psThis, *psNext;
	DI_ENTRY *psEntry;

	PVR_LOG_RETURN_IF_INVALID_PARAM(pszName != NULL, "pszName");
	PVR_LOG_RETURN_IF_INVALID_PARAM(_ValidateIteratorCb(psIterCb, eType),
	                                "psIterCb");
	PVR_LOG_RETURN_IF_INVALID_PARAM(ppsEntry != NULL, "psEntry");

	psEntry = OSAllocMem(sizeof(*psEntry));
	PVR_LOG_RETURN_IF_NOMEM(psEntry, "OSAllocMem");

	if (psGroup == NULL)
	{
		psGroup = &_g_sRootGroup;
	}

	psEntry->pszName = pszName;
	psEntry->sIterCb = *psIterCb;
	dllist_init(&psEntry->sNativeHandleList);

	OSLockAcquire(_g_hLock);

	/* Iterate over all of the native handles of parent group to create
	 * the entry for every registered implementation. */
	dllist_foreach_node(&psGroup->sNativeHandleList, psThis, psNext)
	{
		DI_NATIVE_HANDLE *psNativeEntry, *psNativeGroup =
		        IMG_CONTAINER_OF(psThis, DI_NATIVE_HANDLE, sListNode);

		eError = _CreateNativeEntry(pszName, psEntry, psNativeGroup, pvPriv,
		                            eType, &psNativeEntry);
		PVR_GOTO_IF_ERROR(eError, cleanup_);

		dllist_add_to_head(&psEntry->sNativeHandleList,
		                   &psNativeEntry->sListNode);
	}

	OSLockRelease(_g_hLock);

	*ppsEntry = psEntry;

	return PVRSRV_OK;

cleanup_:
	OSLockRelease(_g_hLock);

	/* Something went wrong so if there were any native entries created remove
	 * them from the list, free them and free the DI entry itself. */
	dllist_foreach_node(&psEntry->sNativeHandleList, psThis, psNext)
	{
		DI_NATIVE_HANDLE *psNativeEntry =
		        IMG_CONTAINER_OF(psThis, DI_NATIVE_HANDLE, sListNode);

		dllist_remove_node(&psNativeEntry->sListNode);
		OSFreeMem(psNativeEntry);
	}

	OSFreeMem(psEntry);

	return eError;
}

void DIDestroyEntry(DI_ENTRY *psEntry)
{
	DLLIST_NODE *psThis, *psNext;

	PVR_LOG_RETURN_VOID_IF_FALSE(psEntry != NULL,
	                             "psEntry invalid in DIDestroyEntry()");

	/* Iterate through all of the native entries of the DI entry, remove
	 * them from the list and then destroy them. After that, destroy the
	 * DI entry itself. */
	dllist_foreach_node(&psEntry->sNativeHandleList, psThis, psNext)
	{
		DI_NATIVE_HANDLE *psNative = IMG_CONTAINER_OF(psThis, DI_NATIVE_HANDLE,
		                                              sListNode);

		/* The implementation must ensure that entry is not removed if any
		 * operations are being executed on the entry. If this is the case
		 * the implementation should block until all of them are finished
		 * and prevent any further operations.
		 * This will guarantee proper synchronisation between the DI framework
		 * and underlying implementations and prevent destruction/access
		 * races. */
		psNative->psDiImpl->sCb.pfnDestroyEntry(psNative->pvHandle);
		dllist_remove_node(&psNative->sListNode);
		OSFreeMem(psNative);
	}

	OSFreeMem(psEntry);
}

static PVRSRV_ERROR _CreateNativeGroup(const IMG_CHAR *pszName,
                                       const DI_NATIVE_HANDLE *psNativeParent,
                                       DI_NATIVE_HANDLE **ppsNativeGroup)
{
	PVRSRV_ERROR eError;
	DI_IMPL *psImpl = psNativeParent->psDiImpl;

	DI_NATIVE_HANDLE *psNativeGroup = OSAllocMem(sizeof(*psNativeGroup));
	PVR_LOG_GOTO_IF_NOMEM(psNativeGroup, eError, return_);

	eError = psImpl->sCb.pfnCreateGroup(pszName,
	                                    psNativeParent->pvHandle,
	                                    &psNativeGroup->pvHandle);
	PVR_LOG_GOTO_IF_ERROR(eError, "psImpl->sCb.pfnCreateGroup", free_memory_);

	psNativeGroup->psDiImpl = psImpl;

	*ppsNativeGroup = psNativeGroup;

	return PVRSRV_OK;

free_memory_:
	OSFreeMem(psNativeGroup);
return_:
	return eError;
}

PVRSRV_ERROR DICreateGroup(const IMG_CHAR *pszName,
                           const DI_GROUP *psParent,
                           DI_GROUP **ppsGroup)
{
	PVRSRV_ERROR eError;
	DLLIST_NODE *psThis, *psNext;
	DI_GROUP *psGroup;

	PVR_LOG_RETURN_IF_INVALID_PARAM(pszName != NULL, "pszName");
	PVR_LOG_RETURN_IF_INVALID_PARAM(ppsGroup != NULL, "ppsDiGroup");

	psGroup = OSAllocMem(sizeof(*psGroup));
	PVR_LOG_RETURN_IF_NOMEM(psGroup, "OSAllocMem");

	if (psParent == NULL)
	{
		psParent = &_g_sRootGroup;
	}

	psGroup->pszName = pszName;
	psGroup->psParent = psParent;
	dllist_init(&psGroup->sNativeHandleList);

	OSLockAcquire(_g_hLock);

	/* Iterate over all of the native handles of parent group to create
	 * the group for every registered implementation. */
	dllist_foreach_node(&psParent->sNativeHandleList, psThis, psNext)
	{
		DI_NATIVE_HANDLE *psNativeGroup = NULL, *psNativeParent =
		        IMG_CONTAINER_OF(psThis, DI_NATIVE_HANDLE, sListNode);

		eError = _CreateNativeGroup(pszName, psNativeParent, &psNativeGroup);
		PVR_GOTO_IF_ERROR(eError, cleanup_);

		dllist_add_to_head(&psGroup->sNativeHandleList,
		                   &psNativeGroup->sListNode);
	}

	OSLockRelease(_g_hLock);

	*ppsGroup = psGroup;

	return PVRSRV_OK;

cleanup_:
	OSLockRelease(_g_hLock);

	/* Something went wrong so if there were any native groups created remove
	 * them from the list, free them and free the DI group itself. */
	dllist_foreach_node(&psGroup->sNativeHandleList, psThis, psNext)
	{
		DI_NATIVE_HANDLE *psNativeGroup =
		        IMG_CONTAINER_OF(psThis, DI_NATIVE_HANDLE, sListNode);

		dllist_remove_node(&psNativeGroup->sListNode);
		OSFreeMem(psNativeGroup);
	}

	OSFreeMem(psGroup);

	return eError;
}

void DIDestroyGroup(DI_GROUP *psGroup)
{
	DLLIST_NODE *psThis, *psNext;

	PVR_LOG_RETURN_VOID_IF_FALSE(psGroup != NULL,
	                             "psGroup invalid in DIDestroyGroup()");

	/* Iterate through all of the native groups of the DI group, remove
	 * them from the list and then destroy them. After that destroy the
	 * DI group itself. */
	dllist_foreach_node(&psGroup->sNativeHandleList, psThis, psNext)
	{
		DI_NATIVE_HANDLE *psNative = IMG_CONTAINER_OF(psThis, DI_NATIVE_HANDLE,
		                                              sListNode);

		psNative->psDiImpl->sCb.pfnDestroyEntry(psNative->pvHandle);
		dllist_remove_node(&psNative->sListNode);
		OSFreeMem(psNative);
	}

	OSFreeMem(psGroup);
}

void *DIGetPrivData(const OSDI_IMPL_ENTRY *psEntry)
{
	PVR_ASSERT(psEntry != NULL);

	return psEntry->pvPrivData;
}

void DISetPrivData(OSDI_IMPL_ENTRY *psEntry, void *pvPrivData)
{
	PVR_ASSERT(psEntry != NULL);

	psEntry->pvPrivData = pvPrivData;
}

void DIPrintf(const OSDI_IMPL_ENTRY *psEntry, const IMG_CHAR *pszFmt, ...)
{
	va_list args;

	PVR_ASSERT(psEntry != NULL);
	PVR_ASSERT(psEntry->psCb != NULL);
	PVR_ASSERT(psEntry->psCb->pfnVPrintf != NULL);
	PVR_ASSERT(psEntry->pvNative != NULL);

	va_start(args, pszFmt);
	psEntry->psCb->pfnVPrintf(psEntry->pvNative, pszFmt, args);
	va_end(args);
}

void DIPuts(const OSDI_IMPL_ENTRY *psEntry, const IMG_CHAR *pszStr)
{
	PVR_ASSERT(psEntry != NULL);
	PVR_ASSERT(psEntry->psCb != NULL);
	PVR_ASSERT(psEntry->psCb->pfnPuts != NULL);
	PVR_ASSERT(psEntry->pvNative != NULL);

	psEntry->psCb->pfnPuts(psEntry->pvNative, pszStr);
}

IMG_BOOL DIHasOverflowed(const OSDI_IMPL_ENTRY *psEntry)
{
	PVR_ASSERT(psEntry != NULL);
	PVR_ASSERT(psEntry->psCb != NULL);
	PVR_ASSERT(psEntry->psCb->pfnHasOverflowed != NULL);
	PVR_ASSERT(psEntry->pvNative != NULL);

	return psEntry->psCb->pfnHasOverflowed(psEntry->pvNative);
}

/* ---- OS implementation API ---------------------------------------------- */

static IMG_BOOL _ValidateImplCb(const OSDI_IMPL_CB *psImplCb)
{
	PVR_GOTO_IF_FALSE(psImplCb->pfnInit != NULL, failed_);
	PVR_GOTO_IF_FALSE(psImplCb->pfnDeInit != NULL, failed_);
	PVR_GOTO_IF_FALSE(psImplCb->pfnCreateGroup != NULL, failed_);
	PVR_GOTO_IF_FALSE(psImplCb->pfnDestroyGroup != NULL, failed_);
	PVR_GOTO_IF_FALSE(psImplCb->pfnCreateEntry != NULL, failed_);
	PVR_GOTO_IF_FALSE(psImplCb->pfnDestroyEntry != NULL, failed_);

	return IMG_TRUE;

failed_:
	return IMG_FALSE;
}

PVRSRV_ERROR DIRegisterImplementation(const IMG_CHAR *pszName,
                                      const OSDI_IMPL_CB *psImplCb)
{
	DI_IMPL *psDiImpl;

	PVR_LOG_RETURN_IF_INVALID_PARAM(pszName != NULL, "pszName");
	PVR_LOG_RETURN_IF_INVALID_PARAM(_ValidateImplCb(psImplCb), "psImplCb");

	psDiImpl = OSAllocMem(sizeof(*psDiImpl));
	PVR_LOG_RETURN_IF_NOMEM(psDiImpl, "OSAllocMem");

	psDiImpl->pszName = pszName;
	psDiImpl->sCb = *psImplCb;

	OSLockAcquire(_g_hLock);
	dllist_add_to_tail(&_g_sImpls, &psDiImpl->sListNode);
	OSLockRelease(_g_hLock);

	return PVRSRV_OK;
}
