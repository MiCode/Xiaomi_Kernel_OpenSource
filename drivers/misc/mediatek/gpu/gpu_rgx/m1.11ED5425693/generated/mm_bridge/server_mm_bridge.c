/*******************************************************************************
@File
@Title          Server bridge for mm
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for mm
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
*******************************************************************************/

#include <linux/uaccess.h>

#include "img_defs.h"

#include "devicemem.h"
#include "devicemem_server.h"
#include "pmr.h"
#include "devicemem_heapcfg.h"
#include "physmem.h"
#include "physmem_tdsecbuf.h"
#include "devicemem_utils.h"

#include "common_mm_bridge.h"

#include "allocmem.h"
#include "pvr_debug.h"
#include "connection_server.h"
#include "pvr_bridge.h"
#if defined(SUPPORT_RGX)
#include "rgx_bridge.h"
#endif
#include "srvcore.h"
#include "handle.h"

#include <linux/slab.h>

static PVRSRV_ERROR ReleasePMRExport(void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(pvData);

	return PVRSRV_OK;
}

/* ***************************************************************************
 * Server-side bridge entry points
 */

static IMG_INT
PVRSRVBridgePMRExportPMR(IMG_UINT32 ui32DispatchTableEntry,
			 PVRSRV_BRIDGE_IN_PMREXPORTPMR * psPMRExportPMRIN,
			 PVRSRV_BRIDGE_OUT_PMREXPORTPMR * psPMRExportPMROUT,
			 CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hPMR = psPMRExportPMRIN->hPMR;
	PMR *psPMRInt = NULL;
	PMR_EXPORT *psPMRExportInt = NULL;
	IMG_HANDLE hPMRExportInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psPMRExportPMROUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psPMRExportPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto PMRExportPMR_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psPMRExportPMROUT->eError =
	    PMRExportPMR(psPMRInt,
			 &psPMRExportInt,
			 &psPMRExportPMROUT->ui64Size,
			 &psPMRExportPMROUT->ui32Log2Contig,
			 &psPMRExportPMROUT->ui64Password);
	/* Exit early if bridged call fails */
	if (unlikely(psPMRExportPMROUT->eError != PVRSRV_OK))
	{
		goto PMRExportPMR_exit;
	}

	/*
	 * For cases where we need a cross process handle we actually allocate two.
	 *
	 * The first one is a connection specific handle and it gets given the real
	 * release function. This handle does *NOT* get returned to the caller. It's
	 * purpose is to release any leaked resources when we either have a bad or
	 * abnormally terminated client. If we didn't do this then the resource
	 * wouldn't be freed until driver unload. If the resource is freed normally,
	 * this handle can be looked up via the cross process handle and then
	 * released accordingly.
	 *
	 * The second one is a cross process handle and it gets given a noop release
	 * function. This handle does get returned to the caller.
	 */

	/* Lock over handle creation. */
	LockHandle(psConnection->psProcessHandleBase->psHandleBase);

	psPMRExportPMROUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psProcessHandleBase->
				      psHandleBase, &hPMRExportInt,
				      (void *)psPMRExportInt,
				      PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) & PMRUnexportPMR);
	if (unlikely(psPMRExportPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);
		goto PMRExportPMR_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);

	/* Lock over handle creation. */
	LockHandle(KERNEL_HANDLE_BASE);
	psPMRExportPMROUT->eError =
	    PVRSRVAllocHandleUnlocked(KERNEL_HANDLE_BASE,
				      &psPMRExportPMROUT->hPMRExport,
				      (void *)psPMRExportInt,
				      PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) & ReleasePMRExport);
	if (unlikely(psPMRExportPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(KERNEL_HANDLE_BASE);
		goto PMRExportPMR_exit;
	}
	/* Release now we have created handles. */
	UnlockHandle(KERNEL_HANDLE_BASE);

 PMRExportPMR_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psPMRExportPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRExportPMROUT->hPMRExport)
		{
			PVRSRV_ERROR eError;

			/* Lock over handle creation cleanup. */
			LockHandle(KERNEL_HANDLE_BASE);

			eError = PVRSRVReleaseHandleUnlocked(KERNEL_HANDLE_BASE,
							     (IMG_HANDLE)
							     psPMRExportPMROUT->
							     hPMRExport,
							     PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
			if (unlikely
			    ((eError != PVRSRV_OK)
			     && (eError != PVRSRV_ERROR_RETRY)))
			{
				PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVBridgePMRExportPMR: %s",
					 PVRSRVGetErrorString(eError)));
			}
			/* Releasing the handle should free/destroy/release the resource.
			 * This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK)
				   || (eError == PVRSRV_ERROR_RETRY));

			/* Release now we have cleaned up creation handles. */
			UnlockHandle(KERNEL_HANDLE_BASE);

		}

		if (hPMRExportInt)
		{
			PVRSRV_ERROR eError;
			/* Lock over handle creation cleanup. */
			LockHandle(psConnection->psProcessHandleBase->
				   psHandleBase);

			eError =
			    PVRSRVReleaseHandleUnlocked(psConnection->
							psProcessHandleBase->
							psHandleBase,
							hPMRExportInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
			if ((eError != PVRSRV_OK)
			    && (eError != PVRSRV_ERROR_RETRY))
			{
				PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVBridgePMRExportPMR: %s",
					 PVRSRVGetErrorString(eError)));
			}
			/* Releasing the handle should free/destroy/release the resource.
			 * This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK)
				   || (eError == PVRSRV_ERROR_RETRY));

			/* Avoid freeing/destroying/releasing the resource a second time below */
			psPMRExportInt = NULL;
			/* Release now we have cleaned up creation handles. */
			UnlockHandle(psConnection->psProcessHandleBase->
				     psHandleBase);
		}

		if (psPMRExportInt)
		{
			LockHandle(KERNEL_HANDLE_BASE);
			PMRUnexportPMR(psPMRExportInt);
			UnlockHandle(KERNEL_HANDLE_BASE);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgePMRUnexportPMR(IMG_UINT32 ui32DispatchTableEntry,
			   PVRSRV_BRIDGE_IN_PMRUNEXPORTPMR * psPMRUnexportPMRIN,
			   PVRSRV_BRIDGE_OUT_PMRUNEXPORTPMR *
			   psPMRUnexportPMROUT, CONNECTION_DATA * psConnection)
{
	PMR_EXPORT *psPMRExportInt = NULL;
	IMG_HANDLE hPMRExportInt = NULL;

	PVR_UNREFERENCED_PARAMETER(psConnection);

	/* Lock over handle destruction. */
	LockHandle(KERNEL_HANDLE_BASE);
	psPMRUnexportPMROUT->eError =
	    PVRSRVLookupHandleUnlocked(KERNEL_HANDLE_BASE,
				       (void **)&psPMRExportInt,
				       (IMG_HANDLE) psPMRUnexportPMRIN->
				       hPMRExport,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT,
				       IMG_FALSE);
	if (unlikely(psPMRUnexportPMROUT->eError != PVRSRV_OK))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgePMRUnexportPMR: %s",
			 PVRSRVGetErrorString(psPMRUnexportPMROUT->eError)));
	}
	PVR_ASSERT(psPMRUnexportPMROUT->eError == PVRSRV_OK);

	/* Release now we have destroyed handles. */
	UnlockHandle(KERNEL_HANDLE_BASE);

	/* Lock over handle destruction. */
	LockHandle(psConnection->psProcessHandleBase->psHandleBase);
	/*
	 * Find the connection specific handle that represents the same data
	 * as the cross process handle as releasing it will actually call the
	 * data's real release function (see the function where the cross
	 * process handle is allocated for more details).
	 */
	psPMRUnexportPMROUT->eError =
	    PVRSRVFindHandleUnlocked(psConnection->psProcessHandleBase->
				     psHandleBase, &hPMRExportInt,
				     psPMRExportInt,
				     PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
	if (unlikely(psPMRUnexportPMROUT->eError != PVRSRV_OK))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgePMRUnexportPMR: %s",
			 PVRSRVGetErrorString(psPMRUnexportPMROUT->eError)));
	}
	PVR_ASSERT(psPMRUnexportPMROUT->eError == PVRSRV_OK);

	psPMRUnexportPMROUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psProcessHandleBase->
					psHandleBase, hPMRExportInt,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
	if (unlikely
	    ((psPMRUnexportPMROUT->eError != PVRSRV_OK)
	     && (psPMRUnexportPMROUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgePMRUnexportPMR: %s",
			 PVRSRVGetErrorString(psPMRUnexportPMROUT->eError)));
	}
	PVR_ASSERT((psPMRUnexportPMROUT->eError == PVRSRV_OK) ||
		   (psPMRUnexportPMROUT->eError == PVRSRV_ERROR_RETRY));
	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);

	/* Lock over handle destruction. */
	LockHandle(KERNEL_HANDLE_BASE);

	psPMRUnexportPMROUT->eError =
	    PVRSRVReleaseHandleUnlocked(KERNEL_HANDLE_BASE,
					(IMG_HANDLE) psPMRUnexportPMRIN->
					hPMRExport,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
	if (unlikely
	    ((psPMRUnexportPMROUT->eError != PVRSRV_OK)
	     && (psPMRUnexportPMROUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgePMRUnexportPMR: %s",
			 PVRSRVGetErrorString(psPMRUnexportPMROUT->eError)));
		UnlockHandle(KERNEL_HANDLE_BASE);
		goto PMRUnexportPMR_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(KERNEL_HANDLE_BASE);

 PMRUnexportPMR_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgePMRGetUID(IMG_UINT32 ui32DispatchTableEntry,
		      PVRSRV_BRIDGE_IN_PMRGETUID * psPMRGetUIDIN,
		      PVRSRV_BRIDGE_OUT_PMRGETUID * psPMRGetUIDOUT,
		      CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hPMR = psPMRGetUIDIN->hPMR;
	PMR *psPMRInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psPMRGetUIDOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psPMRGetUIDOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto PMRGetUID_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psPMRGetUIDOUT->eError = PMRGetUID(psPMRInt, &psPMRGetUIDOUT->ui64UID);

 PMRGetUID_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgePMRMakeLocalImportHandle(IMG_UINT32 ui32DispatchTableEntry,
				     PVRSRV_BRIDGE_IN_PMRMAKELOCALIMPORTHANDLE *
				     psPMRMakeLocalImportHandleIN,
				     PVRSRV_BRIDGE_OUT_PMRMAKELOCALIMPORTHANDLE
				     * psPMRMakeLocalImportHandleOUT,
				     CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hBuffer = psPMRMakeLocalImportHandleIN->hBuffer;
	PMR *psBufferInt = NULL;
	PMR *psExtMemInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psPMRMakeLocalImportHandleOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psBufferInt,
				       hBuffer,
				       PVRSRV_HANDLE_TYPE_PMR_LOCAL_EXPORT_HANDLE,
				       IMG_TRUE);
	if (unlikely(psPMRMakeLocalImportHandleOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto PMRMakeLocalImportHandle_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psPMRMakeLocalImportHandleOUT->eError =
	    PMRMakeLocalImportHandle(psBufferInt, &psExtMemInt);
	/* Exit early if bridged call fails */
	if (unlikely(psPMRMakeLocalImportHandleOUT->eError != PVRSRV_OK))
	{
		goto PMRMakeLocalImportHandle_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psProcessHandleBase->psHandleBase);

	psPMRMakeLocalImportHandleOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psProcessHandleBase->
				      psHandleBase,
				      &psPMRMakeLocalImportHandleOUT->hExtMem,
				      (void *)psExtMemInt,
				      PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      PMRUnmakeLocalImportHandle);
	if (unlikely(psPMRMakeLocalImportHandleOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);
		goto PMRMakeLocalImportHandle_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);

 PMRMakeLocalImportHandle_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psBufferInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hBuffer,
					    PVRSRV_HANDLE_TYPE_PMR_LOCAL_EXPORT_HANDLE);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psPMRMakeLocalImportHandleOUT->eError != PVRSRV_OK)
	{
		if (psExtMemInt)
		{
			LockHandle(KERNEL_HANDLE_BASE);
			PMRUnmakeLocalImportHandle(psExtMemInt);
			UnlockHandle(KERNEL_HANDLE_BASE);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgePMRUnmakeLocalImportHandle(IMG_UINT32 ui32DispatchTableEntry,
				       PVRSRV_BRIDGE_IN_PMRUNMAKELOCALIMPORTHANDLE
				       * psPMRUnmakeLocalImportHandleIN,
				       PVRSRV_BRIDGE_OUT_PMRUNMAKELOCALIMPORTHANDLE
				       * psPMRUnmakeLocalImportHandleOUT,
				       CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psProcessHandleBase->psHandleBase);

	psPMRUnmakeLocalImportHandleOUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psProcessHandleBase->
					psHandleBase,
					(IMG_HANDLE)
					psPMRUnmakeLocalImportHandleIN->hExtMem,
					PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT);
	if (unlikely
	    ((psPMRUnmakeLocalImportHandleOUT->eError != PVRSRV_OK)
	     && (psPMRUnmakeLocalImportHandleOUT->eError !=
		 PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgePMRUnmakeLocalImportHandle: %s",
			 PVRSRVGetErrorString(psPMRUnmakeLocalImportHandleOUT->
					      eError)));
		UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);
		goto PMRUnmakeLocalImportHandle_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);

 PMRUnmakeLocalImportHandle_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgePMRImportPMR(IMG_UINT32 ui32DispatchTableEntry,
			 PVRSRV_BRIDGE_IN_PMRIMPORTPMR * psPMRImportPMRIN,
			 PVRSRV_BRIDGE_OUT_PMRIMPORTPMR * psPMRImportPMROUT,
			 CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hPMRExport = psPMRImportPMRIN->hPMRExport;
	PMR_EXPORT *psPMRExportInt = NULL;
	PMR *psPMRInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(KERNEL_HANDLE_BASE);

	/* Look up the address from the handle */
	psPMRImportPMROUT->eError =
	    PVRSRVLookupHandleUnlocked(KERNEL_HANDLE_BASE,
				       (void **)&psPMRExportInt,
				       hPMRExport,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT,
				       IMG_TRUE);
	if (unlikely(psPMRImportPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(KERNEL_HANDLE_BASE);
		goto PMRImportPMR_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(KERNEL_HANDLE_BASE);

	psPMRImportPMROUT->eError =
	    PhysmemImportPMR(psConnection, OSGetDevData(psConnection),
			     psPMRExportInt,
			     psPMRImportPMRIN->ui64uiPassword,
			     psPMRImportPMRIN->ui64uiSize,
			     psPMRImportPMRIN->ui32uiLog2Contig, &psPMRInt);
	/* Exit early if bridged call fails */
	if (unlikely(psPMRImportPMROUT->eError != PVRSRV_OK))
	{
		goto PMRImportPMR_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psPMRImportPMROUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psPMRImportPMROUT->hPMR,
				      (void *)psPMRInt,
				      PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) & PMRUnrefPMR);
	if (unlikely(psPMRImportPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto PMRImportPMR_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 PMRImportPMR_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(KERNEL_HANDLE_BASE);

	/* Unreference the previously looked up handle */
	if (psPMRExportInt)
	{
		PVRSRVReleaseHandleUnlocked(KERNEL_HANDLE_BASE,
					    hPMRExport,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(KERNEL_HANDLE_BASE);

	if (psPMRImportPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRInt)
		{
			LockHandle(KERNEL_HANDLE_BASE);
			PMRUnrefPMR(psPMRInt);
			UnlockHandle(KERNEL_HANDLE_BASE);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgePMRLocalImportPMR(IMG_UINT32 ui32DispatchTableEntry,
			      PVRSRV_BRIDGE_IN_PMRLOCALIMPORTPMR *
			      psPMRLocalImportPMRIN,
			      PVRSRV_BRIDGE_OUT_PMRLOCALIMPORTPMR *
			      psPMRLocalImportPMROUT,
			      CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hExtHandle = psPMRLocalImportPMRIN->hExtHandle;
	PMR *psExtHandleInt = NULL;
	PMR *psPMRInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psProcessHandleBase->psHandleBase);

	/* Look up the address from the handle */
	psPMRLocalImportPMROUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psProcessHandleBase->
				       psHandleBase, (void **)&psExtHandleInt,
				       hExtHandle,
				       PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT,
				       IMG_TRUE);
	if (unlikely(psPMRLocalImportPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);
		goto PMRLocalImportPMR_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);

	psPMRLocalImportPMROUT->eError =
	    PMRLocalImportPMR(psExtHandleInt,
			      &psPMRInt,
			      &psPMRLocalImportPMROUT->uiSize,
			      &psPMRLocalImportPMROUT->sAlign);
	/* Exit early if bridged call fails */
	if (unlikely(psPMRLocalImportPMROUT->eError != PVRSRV_OK))
	{
		goto PMRLocalImportPMR_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psPMRLocalImportPMROUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psPMRLocalImportPMROUT->hPMR,
				      (void *)psPMRInt,
				      PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) & PMRUnrefPMR);
	if (unlikely(psPMRLocalImportPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto PMRLocalImportPMR_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 PMRLocalImportPMR_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psProcessHandleBase->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psExtHandleInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psProcessHandleBase->
					    psHandleBase, hExtHandle,
					    PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psProcessHandleBase->psHandleBase);

	if (psPMRLocalImportPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRInt)
		{
			LockHandle(KERNEL_HANDLE_BASE);
			PMRUnrefPMR(psPMRInt);
			UnlockHandle(KERNEL_HANDLE_BASE);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgePMRUnrefPMR(IMG_UINT32 ui32DispatchTableEntry,
			PVRSRV_BRIDGE_IN_PMRUNREFPMR * psPMRUnrefPMRIN,
			PVRSRV_BRIDGE_OUT_PMRUNREFPMR * psPMRUnrefPMROUT,
			CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psPMRUnrefPMROUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					(IMG_HANDLE) psPMRUnrefPMRIN->hPMR,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	if (unlikely((psPMRUnrefPMROUT->eError != PVRSRV_OK) &&
		     (psPMRUnrefPMROUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgePMRUnrefPMR: %s",
			 PVRSRVGetErrorString(psPMRUnrefPMROUT->eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto PMRUnrefPMR_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

 PMRUnrefPMR_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgePMRUnrefUnlockPMR(IMG_UINT32 ui32DispatchTableEntry,
			      PVRSRV_BRIDGE_IN_PMRUNREFUNLOCKPMR *
			      psPMRUnrefUnlockPMRIN,
			      PVRSRV_BRIDGE_OUT_PMRUNREFUNLOCKPMR *
			      psPMRUnrefUnlockPMROUT,
			      CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psPMRUnrefUnlockPMROUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					(IMG_HANDLE) psPMRUnrefUnlockPMRIN->
					hPMR, PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	if (unlikely
	    ((psPMRUnrefUnlockPMROUT->eError != PVRSRV_OK)
	     && (psPMRUnrefUnlockPMROUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgePMRUnrefUnlockPMR: %s",
			 PVRSRVGetErrorString(psPMRUnrefUnlockPMROUT->eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto PMRUnrefUnlockPMR_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

 PMRUnrefUnlockPMR_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgePhysmemNewRamBackedPMR(IMG_UINT32 ui32DispatchTableEntry,
				   PVRSRV_BRIDGE_IN_PHYSMEMNEWRAMBACKEDPMR *
				   psPhysmemNewRamBackedPMRIN,
				   PVRSRV_BRIDGE_OUT_PHYSMEMNEWRAMBACKEDPMR *
				   psPhysmemNewRamBackedPMROUT,
				   CONNECTION_DATA * psConnection)
{
	IMG_UINT32 *ui32MappingTableInt = NULL;
	IMG_CHAR *uiAnnotationInt = NULL;
	PMR *psPMRPtrInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks *
	     sizeof(IMG_UINT32)) +
	    (psPhysmemNewRamBackedPMRIN->ui32AnnotationLength *
	     sizeof(IMG_CHAR)) + 0;

	if (unlikely
	    (psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks >
	     PMR_MAX_SUPPORTED_PAGE_COUNT))
	{
		psPhysmemNewRamBackedPMROUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto PhysmemNewRamBackedPMR_exit;
	}

	if (unlikely
	    (psPhysmemNewRamBackedPMRIN->ui32AnnotationLength >
	     DEVMEM_ANNOTATION_MAX_LEN))
	{
		psPhysmemNewRamBackedPMROUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto PhysmemNewRamBackedPMR_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psPhysmemNewRamBackedPMRIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psPhysmemNewRamBackedPMRIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psPhysmemNewRamBackedPMROUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto PhysmemNewRamBackedPMR_exit;
			}
		}
	}

	if (psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks != 0)
	{
		ui32MappingTableInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks * sizeof(IMG_UINT32) >
	    0)
	{
		if (OSCopyFromUser
		    (NULL, ui32MappingTableInt,
		     (const void __user *)psPhysmemNewRamBackedPMRIN->
		     pui32MappingTable,
		     psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psPhysmemNewRamBackedPMROUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto PhysmemNewRamBackedPMR_exit;
		}
	}
	if (psPhysmemNewRamBackedPMRIN->ui32AnnotationLength != 0)
	{
		uiAnnotationInt =
		    (IMG_CHAR *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psPhysmemNewRamBackedPMRIN->ui32AnnotationLength *
		    sizeof(IMG_CHAR);
	}

	/* Copy the data over */
	if (psPhysmemNewRamBackedPMRIN->ui32AnnotationLength *
	    sizeof(IMG_CHAR) > 0)
	{
		if (OSCopyFromUser
		    (NULL, uiAnnotationInt,
		     (const void __user *)psPhysmemNewRamBackedPMRIN->
		     puiAnnotation,
		     psPhysmemNewRamBackedPMRIN->ui32AnnotationLength *
		     sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psPhysmemNewRamBackedPMROUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto PhysmemNewRamBackedPMR_exit;
		}
		((IMG_CHAR *)
		 uiAnnotationInt)[(psPhysmemNewRamBackedPMRIN->
				   ui32AnnotationLength * sizeof(IMG_CHAR)) -
				  1] = '\0';
	}

	psPhysmemNewRamBackedPMROUT->eError =
	    PhysmemNewRamBackedPMR(psConnection, OSGetDevData(psConnection),
				   psPhysmemNewRamBackedPMRIN->uiSize,
				   psPhysmemNewRamBackedPMRIN->uiChunkSize,
				   psPhysmemNewRamBackedPMRIN->
				   ui32NumPhysChunks,
				   psPhysmemNewRamBackedPMRIN->
				   ui32NumVirtChunks, ui32MappingTableInt,
				   psPhysmemNewRamBackedPMRIN->ui32Log2PageSize,
				   psPhysmemNewRamBackedPMRIN->uiFlags,
				   psPhysmemNewRamBackedPMRIN->
				   ui32AnnotationLength, uiAnnotationInt,
				   psPhysmemNewRamBackedPMRIN->ui32PID,
				   &psPMRPtrInt);
	/* Exit early if bridged call fails */
	if (unlikely(psPhysmemNewRamBackedPMROUT->eError != PVRSRV_OK))
	{
		goto PhysmemNewRamBackedPMR_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psPhysmemNewRamBackedPMROUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psPhysmemNewRamBackedPMROUT->hPMRPtr,
				      (void *)psPMRPtrInt,
				      PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) & PMRUnrefPMR);
	if (unlikely(psPhysmemNewRamBackedPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto PhysmemNewRamBackedPMR_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 PhysmemNewRamBackedPMR_exit:

	if (psPhysmemNewRamBackedPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRPtrInt)
		{
			LockHandle(KERNEL_HANDLE_BASE);
			PMRUnrefPMR(psPMRPtrInt);
			UnlockHandle(KERNEL_HANDLE_BASE);
		}
	}

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgePhysmemNewRamBackedLockedPMR(IMG_UINT32 ui32DispatchTableEntry,
					 PVRSRV_BRIDGE_IN_PHYSMEMNEWRAMBACKEDLOCKEDPMR
					 * psPhysmemNewRamBackedLockedPMRIN,
					 PVRSRV_BRIDGE_OUT_PHYSMEMNEWRAMBACKEDLOCKEDPMR
					 * psPhysmemNewRamBackedLockedPMROUT,
					 CONNECTION_DATA * psConnection)
{
	IMG_UINT32 *ui32MappingTableInt = NULL;
	IMG_CHAR *uiAnnotationInt = NULL;
	PMR *psPMRPtrInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks *
	     sizeof(IMG_UINT32)) +
	    (psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength *
	     sizeof(IMG_CHAR)) + 0;

	if (unlikely
	    (psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks >
	     PMR_MAX_SUPPORTED_PAGE_COUNT))
	{
		psPhysmemNewRamBackedLockedPMROUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto PhysmemNewRamBackedLockedPMR_exit;
	}

	if (unlikely
	    (psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength >
	     DEVMEM_ANNOTATION_MAX_LEN))
	{
		psPhysmemNewRamBackedLockedPMROUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto PhysmemNewRamBackedLockedPMR_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psPhysmemNewRamBackedLockedPMRIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psPhysmemNewRamBackedLockedPMRIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psPhysmemNewRamBackedLockedPMROUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto PhysmemNewRamBackedLockedPMR_exit;
			}
		}
	}

	if (psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks != 0)
	{
		ui32MappingTableInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32MappingTableInt,
		     (const void __user *)psPhysmemNewRamBackedLockedPMRIN->
		     pui32MappingTable,
		     psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psPhysmemNewRamBackedLockedPMROUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto PhysmemNewRamBackedLockedPMR_exit;
		}
	}
	if (psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength != 0)
	{
		uiAnnotationInt =
		    (IMG_CHAR *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength *
		    sizeof(IMG_CHAR);
	}

	/* Copy the data over */
	if (psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength *
	    sizeof(IMG_CHAR) > 0)
	{
		if (OSCopyFromUser
		    (NULL, uiAnnotationInt,
		     (const void __user *)psPhysmemNewRamBackedLockedPMRIN->
		     puiAnnotation,
		     psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength *
		     sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psPhysmemNewRamBackedLockedPMROUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto PhysmemNewRamBackedLockedPMR_exit;
		}
		((IMG_CHAR *)
		 uiAnnotationInt)[(psPhysmemNewRamBackedLockedPMRIN->
				   ui32AnnotationLength * sizeof(IMG_CHAR)) -
				  1] = '\0';
	}

	psPhysmemNewRamBackedLockedPMROUT->eError =
	    PhysmemNewRamBackedLockedPMR(psConnection,
					 OSGetDevData(psConnection),
					 psPhysmemNewRamBackedLockedPMRIN->
					 uiSize,
					 psPhysmemNewRamBackedLockedPMRIN->
					 uiChunkSize,
					 psPhysmemNewRamBackedLockedPMRIN->
					 ui32NumPhysChunks,
					 psPhysmemNewRamBackedLockedPMRIN->
					 ui32NumVirtChunks, ui32MappingTableInt,
					 psPhysmemNewRamBackedLockedPMRIN->
					 ui32Log2PageSize,
					 psPhysmemNewRamBackedLockedPMRIN->
					 uiFlags,
					 psPhysmemNewRamBackedLockedPMRIN->
					 ui32AnnotationLength, uiAnnotationInt,
					 psPhysmemNewRamBackedLockedPMRIN->
					 ui32PID, &psPMRPtrInt);
	/* Exit early if bridged call fails */
	if (unlikely(psPhysmemNewRamBackedLockedPMROUT->eError != PVRSRV_OK))
	{
		goto PhysmemNewRamBackedLockedPMR_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psPhysmemNewRamBackedLockedPMROUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psPhysmemNewRamBackedLockedPMROUT->
				      hPMRPtr, (void *)psPMRPtrInt,
				      PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) & PMRUnrefUnlockPMR);
	if (unlikely(psPhysmemNewRamBackedLockedPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto PhysmemNewRamBackedLockedPMR_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 PhysmemNewRamBackedLockedPMR_exit:

	if (psPhysmemNewRamBackedLockedPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRPtrInt)
		{
			LockHandle(KERNEL_HANDLE_BASE);
			PMRUnrefUnlockPMR(psPMRPtrInt);
			UnlockHandle(KERNEL_HANDLE_BASE);
		}
	}

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgePhysmemImportSecBuf(IMG_UINT32 ui32DispatchTableEntry,
				PVRSRV_BRIDGE_IN_PHYSMEMIMPORTSECBUF *
				psPhysmemImportSecBufIN,
				PVRSRV_BRIDGE_OUT_PHYSMEMIMPORTSECBUF *
				psPhysmemImportSecBufOUT,
				CONNECTION_DATA * psConnection)
{
	PMR *psPMRPtrInt = NULL;

	psPhysmemImportSecBufOUT->eError =
	    PhysmemImportSecBuf(psConnection, OSGetDevData(psConnection),
				psPhysmemImportSecBufIN->uiSize,
				psPhysmemImportSecBufIN->ui32Log2Align,
				psPhysmemImportSecBufIN->uiFlags,
				&psPMRPtrInt,
				&psPhysmemImportSecBufOUT->ui64SecBufHandle);
	/* Exit early if bridged call fails */
	if (unlikely(psPhysmemImportSecBufOUT->eError != PVRSRV_OK))
	{
		goto PhysmemImportSecBuf_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psPhysmemImportSecBufOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psPhysmemImportSecBufOUT->hPMRPtr,
				      (void *)psPMRPtrInt,
				      PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) & PMRUnrefPMR);
	if (unlikely(psPhysmemImportSecBufOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto PhysmemImportSecBuf_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 PhysmemImportSecBuf_exit:

	if (psPhysmemImportSecBufOUT->eError != PVRSRV_OK)
	{
		if (psPMRPtrInt)
		{
			LockHandle(KERNEL_HANDLE_BASE);
			PMRUnrefPMR(psPMRPtrInt);
			UnlockHandle(KERNEL_HANDLE_BASE);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntPin(IMG_UINT32 ui32DispatchTableEntry,
			 PVRSRV_BRIDGE_IN_DEVMEMINTPIN * psDevmemIntPinIN,
			 PVRSRV_BRIDGE_OUT_DEVMEMINTPIN * psDevmemIntPinOUT,
			 CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hPMR = psDevmemIntPinIN->hPMR;
	PMR *psPMRInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntPinOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psDevmemIntPinOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntPin_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntPinOUT->eError = DevmemIntPin(psPMRInt);

 DevmemIntPin_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntUnpin(IMG_UINT32 ui32DispatchTableEntry,
			   PVRSRV_BRIDGE_IN_DEVMEMINTUNPIN * psDevmemIntUnpinIN,
			   PVRSRV_BRIDGE_OUT_DEVMEMINTUNPIN *
			   psDevmemIntUnpinOUT, CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hPMR = psDevmemIntUnpinIN->hPMR;
	PMR *psPMRInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntUnpinOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psDevmemIntUnpinOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntUnpin_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntUnpinOUT->eError = DevmemIntUnpin(psPMRInt);

 DevmemIntUnpin_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntPinValidate(IMG_UINT32 ui32DispatchTableEntry,
				 PVRSRV_BRIDGE_IN_DEVMEMINTPINVALIDATE *
				 psDevmemIntPinValidateIN,
				 PVRSRV_BRIDGE_OUT_DEVMEMINTPINVALIDATE *
				 psDevmemIntPinValidateOUT,
				 CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hMapping = psDevmemIntPinValidateIN->hMapping;
	DEVMEMINT_MAPPING *psMappingInt = NULL;
	IMG_HANDLE hPMR = psDevmemIntPinValidateIN->hPMR;
	PMR *psPMRInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntPinValidateOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psMappingInt,
				       hMapping,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING,
				       IMG_TRUE);
	if (unlikely(psDevmemIntPinValidateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntPinValidate_exit;
	}

	/* Look up the address from the handle */
	psDevmemIntPinValidateOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psDevmemIntPinValidateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntPinValidate_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntPinValidateOUT->eError =
	    DevmemIntPinValidate(psMappingInt, psPMRInt);

 DevmemIntPinValidate_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psMappingInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hMapping,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING);
	}

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntUnpinInvalidate(IMG_UINT32 ui32DispatchTableEntry,
				     PVRSRV_BRIDGE_IN_DEVMEMINTUNPININVALIDATE *
				     psDevmemIntUnpinInvalidateIN,
				     PVRSRV_BRIDGE_OUT_DEVMEMINTUNPININVALIDATE
				     * psDevmemIntUnpinInvalidateOUT,
				     CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hMapping = psDevmemIntUnpinInvalidateIN->hMapping;
	DEVMEMINT_MAPPING *psMappingInt = NULL;
	IMG_HANDLE hPMR = psDevmemIntUnpinInvalidateIN->hPMR;
	PMR *psPMRInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntUnpinInvalidateOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psMappingInt,
				       hMapping,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING,
				       IMG_TRUE);
	if (unlikely(psDevmemIntUnpinInvalidateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntUnpinInvalidate_exit;
	}

	/* Look up the address from the handle */
	psDevmemIntUnpinInvalidateOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psDevmemIntUnpinInvalidateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntUnpinInvalidate_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntUnpinInvalidateOUT->eError =
	    DevmemIntUnpinInvalidate(psMappingInt, psPMRInt);

 DevmemIntUnpinInvalidate_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psMappingInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hMapping,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING);
	}

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntCtxCreate(IMG_UINT32 ui32DispatchTableEntry,
			       PVRSRV_BRIDGE_IN_DEVMEMINTCTXCREATE *
			       psDevmemIntCtxCreateIN,
			       PVRSRV_BRIDGE_OUT_DEVMEMINTCTXCREATE *
			       psDevmemIntCtxCreateOUT,
			       CONNECTION_DATA * psConnection)
{
	DEVMEMINT_CTX *psDevMemServerContextInt = NULL;
	IMG_HANDLE hPrivDataInt = NULL;

	psDevmemIntCtxCreateOUT->hDevMemServerContext = NULL;

	psDevmemIntCtxCreateOUT->eError =
	    DevmemIntCtxCreate(psConnection, OSGetDevData(psConnection),
			       psDevmemIntCtxCreateIN->bbKernelMemoryCtx,
			       &psDevMemServerContextInt,
			       &hPrivDataInt,
			       &psDevmemIntCtxCreateOUT->ui32CPUCacheLineSize);
	/* Exit early if bridged call fails */
	if (unlikely(psDevmemIntCtxCreateOUT->eError != PVRSRV_OK))
	{
		goto DevmemIntCtxCreate_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psDevmemIntCtxCreateOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psDevmemIntCtxCreateOUT->
				      hDevMemServerContext,
				      (void *)psDevMemServerContextInt,
				      PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      DevmemIntCtxDestroy);
	if (unlikely(psDevmemIntCtxCreateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntCtxCreate_exit;
	}

	psDevmemIntCtxCreateOUT->eError =
	    PVRSRVAllocSubHandleUnlocked(psConnection->psHandleBase,
					 &psDevmemIntCtxCreateOUT->hPrivData,
					 (void *)hPrivDataInt,
					 PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
					 PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
					 psDevmemIntCtxCreateOUT->
					 hDevMemServerContext);
	if (unlikely(psDevmemIntCtxCreateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntCtxCreate_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 DevmemIntCtxCreate_exit:

	if (psDevmemIntCtxCreateOUT->eError != PVRSRV_OK)
	{
		if (psDevmemIntCtxCreateOUT->hDevMemServerContext)
		{
			PVRSRV_ERROR eError;

			/* Lock over handle creation cleanup. */
			LockHandle(psConnection->psHandleBase);

			eError =
			    PVRSRVReleaseHandleUnlocked(psConnection->
							psHandleBase,
							(IMG_HANDLE)
							psDevmemIntCtxCreateOUT->
							hDevMemServerContext,
							PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
			if (unlikely
			    ((eError != PVRSRV_OK)
			     && (eError != PVRSRV_ERROR_RETRY)))
			{
				PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVBridgeDevmemIntCtxCreate: %s",
					 PVRSRVGetErrorString(eError)));
			}
			/* Releasing the handle should free/destroy/release the resource.
			 * This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK)
				   || (eError == PVRSRV_ERROR_RETRY));

			/* Avoid freeing/destroying/releasing the resource a second time below */
			psDevMemServerContextInt = NULL;
			/* Release now we have cleaned up creation handles. */
			UnlockHandle(psConnection->psHandleBase);

		}

		if (psDevMemServerContextInt)
		{
			DevmemIntCtxDestroy(psDevMemServerContextInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntCtxDestroy(IMG_UINT32 ui32DispatchTableEntry,
				PVRSRV_BRIDGE_IN_DEVMEMINTCTXDESTROY *
				psDevmemIntCtxDestroyIN,
				PVRSRV_BRIDGE_OUT_DEVMEMINTCTXDESTROY *
				psDevmemIntCtxDestroyOUT,
				CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psDevmemIntCtxDestroyOUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					(IMG_HANDLE) psDevmemIntCtxDestroyIN->
					hDevmemServerContext,
					PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
	if (unlikely
	    ((psDevmemIntCtxDestroyOUT->eError != PVRSRV_OK)
	     && (psDevmemIntCtxDestroyOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeDevmemIntCtxDestroy: %s",
			 PVRSRVGetErrorString(psDevmemIntCtxDestroyOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntCtxDestroy_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

 DevmemIntCtxDestroy_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntHeapCreate(IMG_UINT32 ui32DispatchTableEntry,
				PVRSRV_BRIDGE_IN_DEVMEMINTHEAPCREATE *
				psDevmemIntHeapCreateIN,
				PVRSRV_BRIDGE_OUT_DEVMEMINTHEAPCREATE *
				psDevmemIntHeapCreateOUT,
				CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hDevmemCtx = psDevmemIntHeapCreateIN->hDevmemCtx;
	DEVMEMINT_CTX *psDevmemCtxInt = NULL;
	DEVMEMINT_HEAP *psDevmemHeapPtrInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntHeapCreateOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psDevmemCtxInt,
				       hDevmemCtx,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX,
				       IMG_TRUE);
	if (unlikely(psDevmemIntHeapCreateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntHeapCreate_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntHeapCreateOUT->eError =
	    DevmemIntHeapCreate(psDevmemCtxInt,
				psDevmemIntHeapCreateIN->sHeapBaseAddr,
				psDevmemIntHeapCreateIN->uiHeapLength,
				psDevmemIntHeapCreateIN->ui32Log2DataPageSize,
				&psDevmemHeapPtrInt);
	/* Exit early if bridged call fails */
	if (unlikely(psDevmemIntHeapCreateOUT->eError != PVRSRV_OK))
	{
		goto DevmemIntHeapCreate_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psDevmemIntHeapCreateOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psDevmemIntHeapCreateOUT->hDevmemHeapPtr,
				      (void *)psDevmemHeapPtrInt,
				      PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      DevmemIntHeapDestroy);
	if (unlikely(psDevmemIntHeapCreateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntHeapCreate_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 DevmemIntHeapCreate_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psDevmemCtxInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hDevmemCtx,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psDevmemIntHeapCreateOUT->eError != PVRSRV_OK)
	{
		if (psDevmemHeapPtrInt)
		{
			DevmemIntHeapDestroy(psDevmemHeapPtrInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntHeapDestroy(IMG_UINT32 ui32DispatchTableEntry,
				 PVRSRV_BRIDGE_IN_DEVMEMINTHEAPDESTROY *
				 psDevmemIntHeapDestroyIN,
				 PVRSRV_BRIDGE_OUT_DEVMEMINTHEAPDESTROY *
				 psDevmemIntHeapDestroyOUT,
				 CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psDevmemIntHeapDestroyOUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					(IMG_HANDLE) psDevmemIntHeapDestroyIN->
					hDevmemHeap,
					PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP);
	if (unlikely
	    ((psDevmemIntHeapDestroyOUT->eError != PVRSRV_OK)
	     && (psDevmemIntHeapDestroyOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeDevmemIntHeapDestroy: %s",
			 PVRSRVGetErrorString(psDevmemIntHeapDestroyOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntHeapDestroy_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

 DevmemIntHeapDestroy_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntMapPMR(IMG_UINT32 ui32DispatchTableEntry,
			    PVRSRV_BRIDGE_IN_DEVMEMINTMAPPMR *
			    psDevmemIntMapPMRIN,
			    PVRSRV_BRIDGE_OUT_DEVMEMINTMAPPMR *
			    psDevmemIntMapPMROUT,
			    CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hDevmemServerHeap = psDevmemIntMapPMRIN->hDevmemServerHeap;
	DEVMEMINT_HEAP *psDevmemServerHeapInt = NULL;
	IMG_HANDLE hReservation = psDevmemIntMapPMRIN->hReservation;
	DEVMEMINT_RESERVATION *psReservationInt = NULL;
	IMG_HANDLE hPMR = psDevmemIntMapPMRIN->hPMR;
	PMR *psPMRInt = NULL;
	DEVMEMINT_MAPPING *psMappingInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntMapPMROUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psDevmemServerHeapInt,
				       hDevmemServerHeap,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP,
				       IMG_TRUE);
	if (unlikely(psDevmemIntMapPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntMapPMR_exit;
	}

	/* Look up the address from the handle */
	psDevmemIntMapPMROUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psReservationInt,
				       hReservation,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
				       IMG_TRUE);
	if (unlikely(psDevmemIntMapPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntMapPMR_exit;
	}

	/* Look up the address from the handle */
	psDevmemIntMapPMROUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psDevmemIntMapPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntMapPMR_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntMapPMROUT->eError =
	    DevmemIntMapPMR(psDevmemServerHeapInt,
			    psReservationInt,
			    psPMRInt,
			    psDevmemIntMapPMRIN->uiMapFlags, &psMappingInt);
	/* Exit early if bridged call fails */
	if (unlikely(psDevmemIntMapPMROUT->eError != PVRSRV_OK))
	{
		goto DevmemIntMapPMR_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psDevmemIntMapPMROUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psDevmemIntMapPMROUT->hMapping,
				      (void *)psMappingInt,
				      PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) & DevmemIntUnmapPMR);
	if (unlikely(psDevmemIntMapPMROUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntMapPMR_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 DevmemIntMapPMR_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psDevmemServerHeapInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hDevmemServerHeap,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP);
	}

	/* Unreference the previously looked up handle */
	if (psReservationInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hReservation,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
	}

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psDevmemIntMapPMROUT->eError != PVRSRV_OK)
	{
		if (psMappingInt)
		{
			DevmemIntUnmapPMR(psMappingInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntUnmapPMR(IMG_UINT32 ui32DispatchTableEntry,
			      PVRSRV_BRIDGE_IN_DEVMEMINTUNMAPPMR *
			      psDevmemIntUnmapPMRIN,
			      PVRSRV_BRIDGE_OUT_DEVMEMINTUNMAPPMR *
			      psDevmemIntUnmapPMROUT,
			      CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psDevmemIntUnmapPMROUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					(IMG_HANDLE) psDevmemIntUnmapPMRIN->
					hMapping,
					PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING);
	if (unlikely
	    ((psDevmemIntUnmapPMROUT->eError != PVRSRV_OK)
	     && (psDevmemIntUnmapPMROUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeDevmemIntUnmapPMR: %s",
			 PVRSRVGetErrorString(psDevmemIntUnmapPMROUT->eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntUnmapPMR_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

 DevmemIntUnmapPMR_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntReserveRange(IMG_UINT32 ui32DispatchTableEntry,
				  PVRSRV_BRIDGE_IN_DEVMEMINTRESERVERANGE *
				  psDevmemIntReserveRangeIN,
				  PVRSRV_BRIDGE_OUT_DEVMEMINTRESERVERANGE *
				  psDevmemIntReserveRangeOUT,
				  CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hDevmemServerHeap =
	    psDevmemIntReserveRangeIN->hDevmemServerHeap;
	DEVMEMINT_HEAP *psDevmemServerHeapInt = NULL;
	DEVMEMINT_RESERVATION *psReservationInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntReserveRangeOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psDevmemServerHeapInt,
				       hDevmemServerHeap,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP,
				       IMG_TRUE);
	if (unlikely(psDevmemIntReserveRangeOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntReserveRange_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntReserveRangeOUT->eError =
	    DevmemIntReserveRange(psDevmemServerHeapInt,
				  psDevmemIntReserveRangeIN->sAddress,
				  psDevmemIntReserveRangeIN->uiLength,
				  &psReservationInt);
	/* Exit early if bridged call fails */
	if (unlikely(psDevmemIntReserveRangeOUT->eError != PVRSRV_OK))
	{
		goto DevmemIntReserveRange_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psDevmemIntReserveRangeOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psDevmemIntReserveRangeOUT->hReservation,
				      (void *)psReservationInt,
				      PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      DevmemIntUnreserveRange);
	if (unlikely(psDevmemIntReserveRangeOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntReserveRange_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 DevmemIntReserveRange_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psDevmemServerHeapInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hDevmemServerHeap,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psDevmemIntReserveRangeOUT->eError != PVRSRV_OK)
	{
		if (psReservationInt)
		{
			DevmemIntUnreserveRange(psReservationInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntUnreserveRange(IMG_UINT32 ui32DispatchTableEntry,
				    PVRSRV_BRIDGE_IN_DEVMEMINTUNRESERVERANGE *
				    psDevmemIntUnreserveRangeIN,
				    PVRSRV_BRIDGE_OUT_DEVMEMINTUNRESERVERANGE *
				    psDevmemIntUnreserveRangeOUT,
				    CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psDevmemIntUnreserveRangeOUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					(IMG_HANDLE)
					psDevmemIntUnreserveRangeIN->
					hReservation,
					PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
	if (unlikely
	    ((psDevmemIntUnreserveRangeOUT->eError != PVRSRV_OK)
	     && (psDevmemIntUnreserveRangeOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeDevmemIntUnreserveRange: %s",
			 PVRSRVGetErrorString(psDevmemIntUnreserveRangeOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntUnreserveRange_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

 DevmemIntUnreserveRange_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeChangeSparseMem(IMG_UINT32 ui32DispatchTableEntry,
			    PVRSRV_BRIDGE_IN_CHANGESPARSEMEM *
			    psChangeSparseMemIN,
			    PVRSRV_BRIDGE_OUT_CHANGESPARSEMEM *
			    psChangeSparseMemOUT,
			    CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hSrvDevMemHeap = psChangeSparseMemIN->hSrvDevMemHeap;
	DEVMEMINT_HEAP *psSrvDevMemHeapInt = NULL;
	IMG_HANDLE hPMR = psChangeSparseMemIN->hPMR;
	PMR *psPMRInt = NULL;
	IMG_UINT32 *ui32AllocPageIndicesInt = NULL;
	IMG_UINT32 *ui32FreePageIndicesInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psChangeSparseMemIN->ui32AllocPageCount * sizeof(IMG_UINT32)) +
	    (psChangeSparseMemIN->ui32FreePageCount * sizeof(IMG_UINT32)) + 0;

	if (unlikely
	    (psChangeSparseMemIN->ui32AllocPageCount >
	     PMR_MAX_SUPPORTED_PAGE_COUNT))
	{
		psChangeSparseMemOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto ChangeSparseMem_exit;
	}

	if (unlikely
	    (psChangeSparseMemIN->ui32FreePageCount >
	     PMR_MAX_SUPPORTED_PAGE_COUNT))
	{
		psChangeSparseMemOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto ChangeSparseMem_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psChangeSparseMemIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psChangeSparseMemIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psChangeSparseMemOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto ChangeSparseMem_exit;
			}
		}
	}

	if (psChangeSparseMemIN->ui32AllocPageCount != 0)
	{
		ui32AllocPageIndicesInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psChangeSparseMemIN->ui32AllocPageCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psChangeSparseMemIN->ui32AllocPageCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32AllocPageIndicesInt,
		     (const void __user *)psChangeSparseMemIN->
		     pui32AllocPageIndices,
		     psChangeSparseMemIN->ui32AllocPageCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psChangeSparseMemOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto ChangeSparseMem_exit;
		}
	}
	if (psChangeSparseMemIN->ui32FreePageCount != 0)
	{
		ui32FreePageIndicesInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psChangeSparseMemIN->ui32FreePageCount * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psChangeSparseMemIN->ui32FreePageCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32FreePageIndicesInt,
		     (const void __user *)psChangeSparseMemIN->
		     pui32FreePageIndices,
		     psChangeSparseMemIN->ui32FreePageCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psChangeSparseMemOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto ChangeSparseMem_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psChangeSparseMemOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psSrvDevMemHeapInt,
				       hSrvDevMemHeap,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP,
				       IMG_TRUE);
	if (unlikely(psChangeSparseMemOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto ChangeSparseMem_exit;
	}

	/* Look up the address from the handle */
	psChangeSparseMemOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psChangeSparseMemOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto ChangeSparseMem_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psChangeSparseMemOUT->eError =
	    DevmemIntChangeSparse(psSrvDevMemHeapInt,
				  psPMRInt,
				  psChangeSparseMemIN->ui32AllocPageCount,
				  ui32AllocPageIndicesInt,
				  psChangeSparseMemIN->ui32FreePageCount,
				  ui32FreePageIndicesInt,
				  psChangeSparseMemIN->ui32SparseFlags,
				  psChangeSparseMemIN->uiFlags,
				  psChangeSparseMemIN->sDevVAddr,
				  psChangeSparseMemIN->ui64CPUVAddr);

 ChangeSparseMem_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psSrvDevMemHeapInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hSrvDevMemHeap,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP);
	}

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntMapPages(IMG_UINT32 ui32DispatchTableEntry,
			      PVRSRV_BRIDGE_IN_DEVMEMINTMAPPAGES *
			      psDevmemIntMapPagesIN,
			      PVRSRV_BRIDGE_OUT_DEVMEMINTMAPPAGES *
			      psDevmemIntMapPagesOUT,
			      CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hReservation = psDevmemIntMapPagesIN->hReservation;
	DEVMEMINT_RESERVATION *psReservationInt = NULL;
	IMG_HANDLE hPMR = psDevmemIntMapPagesIN->hPMR;
	PMR *psPMRInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntMapPagesOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psReservationInt,
				       hReservation,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
				       IMG_TRUE);
	if (unlikely(psDevmemIntMapPagesOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntMapPages_exit;
	}

	/* Look up the address from the handle */
	psDevmemIntMapPagesOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psDevmemIntMapPagesOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntMapPages_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntMapPagesOUT->eError =
	    DevmemIntMapPages(psReservationInt,
			      psPMRInt,
			      psDevmemIntMapPagesIN->ui32PageCount,
			      psDevmemIntMapPagesIN->ui32PhysicalPgOffset,
			      psDevmemIntMapPagesIN->uiFlags,
			      psDevmemIntMapPagesIN->sDevVAddr);

 DevmemIntMapPages_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psReservationInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hReservation,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
	}

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntUnmapPages(IMG_UINT32 ui32DispatchTableEntry,
				PVRSRV_BRIDGE_IN_DEVMEMINTUNMAPPAGES *
				psDevmemIntUnmapPagesIN,
				PVRSRV_BRIDGE_OUT_DEVMEMINTUNMAPPAGES *
				psDevmemIntUnmapPagesOUT,
				CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hReservation = psDevmemIntUnmapPagesIN->hReservation;
	DEVMEMINT_RESERVATION *psReservationInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntUnmapPagesOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psReservationInt,
				       hReservation,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
				       IMG_TRUE);
	if (unlikely(psDevmemIntUnmapPagesOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntUnmapPages_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntUnmapPagesOUT->eError =
	    DevmemIntUnmapPages(psReservationInt,
				psDevmemIntUnmapPagesIN->sDevVAddr,
				psDevmemIntUnmapPagesIN->ui32PageCount);

 DevmemIntUnmapPages_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psReservationInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hReservation,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIsVDevAddrValid(IMG_UINT32 ui32DispatchTableEntry,
				  PVRSRV_BRIDGE_IN_DEVMEMISVDEVADDRVALID *
				  psDevmemIsVDevAddrValidIN,
				  PVRSRV_BRIDGE_OUT_DEVMEMISVDEVADDRVALID *
				  psDevmemIsVDevAddrValidOUT,
				  CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hDevmemCtx = psDevmemIsVDevAddrValidIN->hDevmemCtx;
	DEVMEMINT_CTX *psDevmemCtxInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIsVDevAddrValidOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psDevmemCtxInt,
				       hDevmemCtx,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX,
				       IMG_TRUE);
	if (unlikely(psDevmemIsVDevAddrValidOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIsVDevAddrValid_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIsVDevAddrValidOUT->eError =
	    DevmemIntIsVDevAddrValid(psConnection, OSGetDevData(psConnection),
				     psDevmemCtxInt,
				     psDevmemIsVDevAddrValidIN->sAddress);

 DevmemIsVDevAddrValid_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psDevmemCtxInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hDevmemCtx,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeHeapCfgHeapConfigCount(IMG_UINT32 ui32DispatchTableEntry,
				   PVRSRV_BRIDGE_IN_HEAPCFGHEAPCONFIGCOUNT *
				   psHeapCfgHeapConfigCountIN,
				   PVRSRV_BRIDGE_OUT_HEAPCFGHEAPCONFIGCOUNT *
				   psHeapCfgHeapConfigCountOUT,
				   CONNECTION_DATA * psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psHeapCfgHeapConfigCountIN);

	psHeapCfgHeapConfigCountOUT->eError =
	    HeapCfgHeapConfigCount(psConnection, OSGetDevData(psConnection),
				   &psHeapCfgHeapConfigCountOUT->
				   ui32NumHeapConfigs);

	return 0;
}

static IMG_INT
PVRSRVBridgeHeapCfgHeapCount(IMG_UINT32 ui32DispatchTableEntry,
			     PVRSRV_BRIDGE_IN_HEAPCFGHEAPCOUNT *
			     psHeapCfgHeapCountIN,
			     PVRSRV_BRIDGE_OUT_HEAPCFGHEAPCOUNT *
			     psHeapCfgHeapCountOUT,
			     CONNECTION_DATA * psConnection)
{

	psHeapCfgHeapCountOUT->eError =
	    HeapCfgHeapCount(psConnection, OSGetDevData(psConnection),
			     psHeapCfgHeapCountIN->ui32HeapConfigIndex,
			     &psHeapCfgHeapCountOUT->ui32NumHeaps);

	return 0;
}

static IMG_INT
PVRSRVBridgeHeapCfgHeapConfigName(IMG_UINT32 ui32DispatchTableEntry,
				  PVRSRV_BRIDGE_IN_HEAPCFGHEAPCONFIGNAME *
				  psHeapCfgHeapConfigNameIN,
				  PVRSRV_BRIDGE_OUT_HEAPCFGHEAPCONFIGNAME *
				  psHeapCfgHeapConfigNameOUT,
				  CONNECTION_DATA * psConnection)
{
	IMG_CHAR *puiHeapConfigNameInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz *
	     sizeof(IMG_CHAR)) + 0;

	if (psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz >
	    DEVMEM_HEAPNAME_MAXLENGTH)
	{
		psHeapCfgHeapConfigNameOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto HeapCfgHeapConfigName_exit;
	}

	psHeapCfgHeapConfigNameOUT->puiHeapConfigName =
	    psHeapCfgHeapConfigNameIN->puiHeapConfigName;

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psHeapCfgHeapConfigNameIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psHeapCfgHeapConfigNameIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psHeapCfgHeapConfigNameOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto HeapCfgHeapConfigName_exit;
			}
		}
	}

	if (psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz != 0)
	{
		puiHeapConfigNameInt =
		    (IMG_CHAR *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz *
		    sizeof(IMG_CHAR);
	}

	psHeapCfgHeapConfigNameOUT->eError =
	    HeapCfgHeapConfigName(psConnection, OSGetDevData(psConnection),
				  psHeapCfgHeapConfigNameIN->
				  ui32HeapConfigIndex,
				  psHeapCfgHeapConfigNameIN->
				  ui32HeapConfigNameBufSz,
				  puiHeapConfigNameInt);

	if ((psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz *
	     sizeof(IMG_CHAR)) > 0)
	{
		if (unlikely
		    (OSCopyToUser
		     (NULL,
		      (void __user *)psHeapCfgHeapConfigNameOUT->
		      puiHeapConfigName, puiHeapConfigNameInt,
		      (psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz *
		       sizeof(IMG_CHAR))) != PVRSRV_OK))
		{
			psHeapCfgHeapConfigNameOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto HeapCfgHeapConfigName_exit;
		}
	}

 HeapCfgHeapConfigName_exit:

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgeHeapCfgHeapDetails(IMG_UINT32 ui32DispatchTableEntry,
			       PVRSRV_BRIDGE_IN_HEAPCFGHEAPDETAILS *
			       psHeapCfgHeapDetailsIN,
			       PVRSRV_BRIDGE_OUT_HEAPCFGHEAPDETAILS *
			       psHeapCfgHeapDetailsOUT,
			       CONNECTION_DATA * psConnection)
{
	IMG_CHAR *puiHeapNameOutInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psHeapCfgHeapDetailsIN->ui32HeapNameBufSz * sizeof(IMG_CHAR)) + 0;

	if (psHeapCfgHeapDetailsIN->ui32HeapNameBufSz >
	    DEVMEM_HEAPNAME_MAXLENGTH)
	{
		psHeapCfgHeapDetailsOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto HeapCfgHeapDetails_exit;
	}

	psHeapCfgHeapDetailsOUT->puiHeapNameOut =
	    psHeapCfgHeapDetailsIN->puiHeapNameOut;

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psHeapCfgHeapDetailsIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psHeapCfgHeapDetailsIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psHeapCfgHeapDetailsOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto HeapCfgHeapDetails_exit;
			}
		}
	}

	if (psHeapCfgHeapDetailsIN->ui32HeapNameBufSz != 0)
	{
		puiHeapNameOutInt =
		    (IMG_CHAR *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psHeapCfgHeapDetailsIN->ui32HeapNameBufSz *
		    sizeof(IMG_CHAR);
	}

	psHeapCfgHeapDetailsOUT->eError =
	    HeapCfgHeapDetails(psConnection, OSGetDevData(psConnection),
			       psHeapCfgHeapDetailsIN->ui32HeapConfigIndex,
			       psHeapCfgHeapDetailsIN->ui32HeapIndex,
			       psHeapCfgHeapDetailsIN->ui32HeapNameBufSz,
			       puiHeapNameOutInt,
			       &psHeapCfgHeapDetailsOUT->sDevVAddrBase,
			       &psHeapCfgHeapDetailsOUT->uiHeapLength,
			       &psHeapCfgHeapDetailsOUT->
			       ui32Log2DataPageSizeOut,
			       &psHeapCfgHeapDetailsOUT->
			       ui32Log2ImportAlignmentOut,
			       &psHeapCfgHeapDetailsOUT->
			       ui32Log2TilingStrideFactorOut);

	if ((psHeapCfgHeapDetailsIN->ui32HeapNameBufSz * sizeof(IMG_CHAR)) > 0)
	{
		if (unlikely
		    (OSCopyToUser
		     (NULL,
		      (void __user *)psHeapCfgHeapDetailsOUT->puiHeapNameOut,
		      puiHeapNameOutInt,
		      (psHeapCfgHeapDetailsIN->ui32HeapNameBufSz *
		       sizeof(IMG_CHAR))) != PVRSRV_OK))
		{
			psHeapCfgHeapDetailsOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto HeapCfgHeapDetails_exit;
		}
	}

 HeapCfgHeapDetails_exit:

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemIntRegisterPFNotifyKM(IMG_UINT32 ui32DispatchTableEntry,
					PVRSRV_BRIDGE_IN_DEVMEMINTREGISTERPFNOTIFYKM
					* psDevmemIntRegisterPFNotifyKMIN,
					PVRSRV_BRIDGE_OUT_DEVMEMINTREGISTERPFNOTIFYKM
					* psDevmemIntRegisterPFNotifyKMOUT,
					CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hDevmemCtx = psDevmemIntRegisterPFNotifyKMIN->hDevmemCtx;
	DEVMEMINT_CTX *psDevmemCtxInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemIntRegisterPFNotifyKMOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psDevmemCtxInt,
				       hDevmemCtx,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX,
				       IMG_TRUE);
	if (unlikely(psDevmemIntRegisterPFNotifyKMOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemIntRegisterPFNotifyKM_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemIntRegisterPFNotifyKMOUT->eError =
	    DevmemIntRegisterPFNotifyKM(psDevmemCtxInt,
					psDevmemIntRegisterPFNotifyKMIN->
					ui32PID,
					psDevmemIntRegisterPFNotifyKMIN->
					bRegister);

 DevmemIntRegisterPFNotifyKM_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psDevmemCtxInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hDevmemCtx,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeGetMaxDevMemSize(IMG_UINT32 ui32DispatchTableEntry,
			     PVRSRV_BRIDGE_IN_GETMAXDEVMEMSIZE *
			     psGetMaxDevMemSizeIN,
			     PVRSRV_BRIDGE_OUT_GETMAXDEVMEMSIZE *
			     psGetMaxDevMemSizeOUT,
			     CONNECTION_DATA * psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psGetMaxDevMemSizeIN);

	psGetMaxDevMemSizeOUT->eError =
	    PVRSRVGetMaxDevMemSizeKM(psConnection, OSGetDevData(psConnection),
				     &psGetMaxDevMemSizeOUT->uiLMASize,
				     &psGetMaxDevMemSizeOUT->uiUMASize);

	return 0;
}

static IMG_INT
PVRSRVBridgeDevmemGetFaultAddress(IMG_UINT32 ui32DispatchTableEntry,
				  PVRSRV_BRIDGE_IN_DEVMEMGETFAULTADDRESS *
				  psDevmemGetFaultAddressIN,
				  PVRSRV_BRIDGE_OUT_DEVMEMGETFAULTADDRESS *
				  psDevmemGetFaultAddressOUT,
				  CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hDevmemCtx = psDevmemGetFaultAddressIN->hDevmemCtx;
	DEVMEMINT_CTX *psDevmemCtxInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psDevmemGetFaultAddressOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psDevmemCtxInt,
				       hDevmemCtx,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX,
				       IMG_TRUE);
	if (unlikely(psDevmemGetFaultAddressOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto DevmemGetFaultAddress_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psDevmemGetFaultAddressOUT->eError =
	    DevmemIntGetFaultAddress(psConnection, OSGetDevData(psConnection),
				     psDevmemCtxInt,
				     &psDevmemGetFaultAddressOUT->
				     sFaultAddress);

 DevmemGetFaultAddress_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psDevmemCtxInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hDevmemCtx,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

static IMG_BOOL bUseLock = IMG_TRUE;

PVRSRV_ERROR InitMMBridge(void);
PVRSRV_ERROR DeinitMMBridge(void);

/*
 * Register all MM functions with services
 */
PVRSRV_ERROR InitMMBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMREXPORTPMR,
			      PVRSRVBridgePMRExportPMR, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRUNEXPORTPMR,
			      PVRSRVBridgePMRUnexportPMR, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRGETUID,
			      PVRSRVBridgePMRGetUID, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_PMRMAKELOCALIMPORTHANDLE,
			      PVRSRVBridgePMRMakeLocalImportHandle, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_PMRUNMAKELOCALIMPORTHANDLE,
			      PVRSRVBridgePMRUnmakeLocalImportHandle, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRIMPORTPMR,
			      PVRSRVBridgePMRImportPMR, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_PMRLOCALIMPORTPMR,
			      PVRSRVBridgePMRLocalImportPMR, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRUNREFPMR,
			      PVRSRVBridgePMRUnrefPMR, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_PMRUNREFUNLOCKPMR,
			      PVRSRVBridgePMRUnrefUnlockPMR, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR,
			      PVRSRVBridgePhysmemNewRamBackedPMR, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDLOCKEDPMR,
			      PVRSRVBridgePhysmemNewRamBackedLockedPMR, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_PHYSMEMIMPORTSECBUF,
			      PVRSRVBridgePhysmemImportSecBuf, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTPIN,
			      PVRSRVBridgeDevmemIntPin, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTUNPIN,
			      PVRSRVBridgeDevmemIntUnpin, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTPINVALIDATE,
			      PVRSRVBridgeDevmemIntPinValidate, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTUNPININVALIDATE,
			      PVRSRVBridgeDevmemIntUnpinInvalidate, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTCTXCREATE,
			      PVRSRVBridgeDevmemIntCtxCreate, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTCTXDESTROY,
			      PVRSRVBridgeDevmemIntCtxDestroy, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTHEAPCREATE,
			      PVRSRVBridgeDevmemIntHeapCreate, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY,
			      PVRSRVBridgeDevmemIntHeapDestroy, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTMAPPMR,
			      PVRSRVBridgeDevmemIntMapPMR, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTUNMAPPMR,
			      PVRSRVBridgeDevmemIntUnmapPMR, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTRESERVERANGE,
			      PVRSRVBridgeDevmemIntReserveRange, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTUNRESERVERANGE,
			      PVRSRVBridgeDevmemIntUnreserveRange, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_CHANGESPARSEMEM,
			      PVRSRVBridgeChangeSparseMem, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTMAPPAGES,
			      PVRSRVBridgeDevmemIntMapPages, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTUNMAPPAGES,
			      PVRSRVBridgeDevmemIntUnmapPages, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMISVDEVADDRVALID,
			      PVRSRVBridgeDevmemIsVDevAddrValid, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_HEAPCFGHEAPCONFIGCOUNT,
			      PVRSRVBridgeHeapCfgHeapConfigCount, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_HEAPCFGHEAPCOUNT,
			      PVRSRVBridgeHeapCfgHeapCount, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_HEAPCFGHEAPCONFIGNAME,
			      PVRSRVBridgeHeapCfgHeapConfigName, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_HEAPCFGHEAPDETAILS,
			      PVRSRVBridgeHeapCfgHeapDetails, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMINTREGISTERPFNOTIFYKM,
			      PVRSRVBridgeDevmemIntRegisterPFNotifyKM, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_GETMAXDEVMEMSIZE,
			      PVRSRVBridgeGetMaxDevMemSize, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM,
			      PVRSRV_BRIDGE_MM_DEVMEMGETFAULTADDRESS,
			      PVRSRVBridgeDevmemGetFaultAddress, NULL,
			      bUseLock);

	return PVRSRV_OK;
}

/*
 * Unregister all mm functions with services
 */
PVRSRV_ERROR DeinitMMBridge(void)
{

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PMREXPORTPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PMRUNEXPORTPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRGETUID);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PMRMAKELOCALIMPORTHANDLE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PMRUNMAKELOCALIMPORTHANDLE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PMRIMPORTPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PMRLOCALIMPORTPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRUNREFPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PMRUNREFUNLOCKPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDLOCKEDPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_PHYSMEMIMPORTSECBUF);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTPIN);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTUNPIN);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTPINVALIDATE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTUNPININVALIDATE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTCTXCREATE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTCTXDESTROY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTHEAPCREATE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTMAPPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTUNMAPPMR);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTRESERVERANGE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTUNRESERVERANGE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_CHANGESPARSEMEM);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTMAPPAGES);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTUNMAPPAGES);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMISVDEVADDRVALID);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_HEAPCFGHEAPCONFIGCOUNT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_HEAPCFGHEAPCOUNT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_HEAPCFGHEAPCONFIGNAME);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_HEAPCFGHEAPDETAILS);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMINTREGISTERPFNOTIFYKM);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_GETMAXDEVMEMSIZE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_MM,
				PVRSRV_BRIDGE_MM_DEVMEMGETFAULTADDRESS);

	return PVRSRV_OK;
}
