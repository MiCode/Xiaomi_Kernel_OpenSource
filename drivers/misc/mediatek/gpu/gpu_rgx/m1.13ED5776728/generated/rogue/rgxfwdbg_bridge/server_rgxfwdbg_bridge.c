/*******************************************************************************
@File
@Title          Server bridge for rgxfwdbg
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxfwdbg
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

#include "devicemem_server.h"
#include "rgxfwdbg.h"
#include "pmr.h"
#include "rgxtimecorr.h"

#include "common_rgxfwdbg_bridge.h"

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

/* ***************************************************************************
 * Server-side bridge entry points
 */

static IMG_INT
PVRSRVBridgeRGXFWDebugSetFWLog(IMG_UINT32 ui32DispatchTableEntry,
			       PVRSRV_BRIDGE_IN_RGXFWDEBUGSETFWLOG *
			       psRGXFWDebugSetFWLogIN,
			       PVRSRV_BRIDGE_OUT_RGXFWDEBUGSETFWLOG *
			       psRGXFWDebugSetFWLogOUT,
			       CONNECTION_DATA * psConnection)
{

	psRGXFWDebugSetFWLogOUT->eError =
	    PVRSRVRGXFWDebugSetFWLogKM(psConnection, OSGetDevNode(psConnection),
				       psRGXFWDebugSetFWLogIN->
				       ui32RGXFWLogType);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXFWDebugDumpFreelistPageList(IMG_UINT32 ui32DispatchTableEntry,
					   PVRSRV_BRIDGE_IN_RGXFWDEBUGDUMPFREELISTPAGELIST
					   * psRGXFWDebugDumpFreelistPageListIN,
					   PVRSRV_BRIDGE_OUT_RGXFWDEBUGDUMPFREELISTPAGELIST
					   *
					   psRGXFWDebugDumpFreelistPageListOUT,
					   CONNECTION_DATA * psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psRGXFWDebugDumpFreelistPageListIN);

	psRGXFWDebugDumpFreelistPageListOUT->eError =
	    PVRSRVRGXFWDebugDumpFreelistPageListKM(psConnection,
						   OSGetDevNode(psConnection));

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXFWDebugSetHCSDeadline(IMG_UINT32 ui32DispatchTableEntry,
				     PVRSRV_BRIDGE_IN_RGXFWDEBUGSETHCSDEADLINE *
				     psRGXFWDebugSetHCSDeadlineIN,
				     PVRSRV_BRIDGE_OUT_RGXFWDEBUGSETHCSDEADLINE
				     * psRGXFWDebugSetHCSDeadlineOUT,
				     CONNECTION_DATA * psConnection)
{

	psRGXFWDebugSetHCSDeadlineOUT->eError =
	    PVRSRVRGXFWDebugSetHCSDeadlineKM(psConnection,
					     OSGetDevNode(psConnection),
					     psRGXFWDebugSetHCSDeadlineIN->
					     ui32RGXHCSDeadline);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXFWDebugSetOSidPriority(IMG_UINT32 ui32DispatchTableEntry,
				      PVRSRV_BRIDGE_IN_RGXFWDEBUGSETOSIDPRIORITY
				      * psRGXFWDebugSetOSidPriorityIN,
				      PVRSRV_BRIDGE_OUT_RGXFWDEBUGSETOSIDPRIORITY
				      * psRGXFWDebugSetOSidPriorityOUT,
				      CONNECTION_DATA * psConnection)
{

	psRGXFWDebugSetOSidPriorityOUT->eError =
	    PVRSRVRGXFWDebugSetOSidPriorityKM(psConnection,
					      OSGetDevNode(psConnection),
					      psRGXFWDebugSetOSidPriorityIN->
					      ui32OSid,
					      psRGXFWDebugSetOSidPriorityIN->
					      ui32Priority);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXFWDebugSetOSNewOnlineState(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXFWDEBUGSETOSNEWONLINESTATE
					  * psRGXFWDebugSetOSNewOnlineStateIN,
					  PVRSRV_BRIDGE_OUT_RGXFWDEBUGSETOSNEWONLINESTATE
					  * psRGXFWDebugSetOSNewOnlineStateOUT,
					  CONNECTION_DATA * psConnection)
{

	psRGXFWDebugSetOSNewOnlineStateOUT->eError =
	    PVRSRVRGXFWDebugSetOSNewOnlineStateKM(psConnection,
						  OSGetDevNode(psConnection),
						  psRGXFWDebugSetOSNewOnlineStateIN->
						  ui32OSid,
						  psRGXFWDebugSetOSNewOnlineStateIN->
						  ui32OSNewState);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXFWDebugPHRConfigure(IMG_UINT32 ui32DispatchTableEntry,
				   PVRSRV_BRIDGE_IN_RGXFWDEBUGPHRCONFIGURE *
				   psRGXFWDebugPHRConfigureIN,
				   PVRSRV_BRIDGE_OUT_RGXFWDEBUGPHRCONFIGURE *
				   psRGXFWDebugPHRConfigureOUT,
				   CONNECTION_DATA * psConnection)
{

	psRGXFWDebugPHRConfigureOUT->eError =
	    PVRSRVRGXFWDebugPHRConfigureKM(psConnection,
					   OSGetDevNode(psConnection),
					   psRGXFWDebugPHRConfigureIN->
					   ui32ui32PHRMode);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCurrentTime(IMG_UINT32 ui32DispatchTableEntry,
			   PVRSRV_BRIDGE_IN_RGXCURRENTTIME * psRGXCurrentTimeIN,
			   PVRSRV_BRIDGE_OUT_RGXCURRENTTIME *
			   psRGXCurrentTimeOUT, CONNECTION_DATA * psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psRGXCurrentTimeIN);

	psRGXCurrentTimeOUT->eError =
	    PVRSRVRGXCurrentTime(psConnection, OSGetDevNode(psConnection),
				 &psRGXCurrentTimeOUT->ui64Time);

	return 0;
}

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR InitRGXFWDBGBridge(void);
PVRSRV_ERROR DeinitRGXFWDBGBridge(void);

/*
 * Register all RGXFWDBG functions with services
 */
PVRSRV_ERROR InitRGXFWDBGBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
			      PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGSETFWLOG,
			      PVRSRVBridgeRGXFWDebugSetFWLog, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
			      PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGDUMPFREELISTPAGELIST,
			      PVRSRVBridgeRGXFWDebugDumpFreelistPageList, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
			      PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGSETHCSDEADLINE,
			      PVRSRVBridgeRGXFWDebugSetHCSDeadline, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
			      PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGSETOSIDPRIORITY,
			      PVRSRVBridgeRGXFWDebugSetOSidPriority, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
			      PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGSETOSNEWONLINESTATE,
			      PVRSRVBridgeRGXFWDebugSetOSNewOnlineState, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
			      PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGPHRCONFIGURE,
			      PVRSRVBridgeRGXFWDebugPHRConfigure, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
			      PVRSRV_BRIDGE_RGXFWDBG_RGXCURRENTTIME,
			      PVRSRVBridgeRGXCurrentTime, NULL);

	return PVRSRV_OK;
}

/*
 * Unregister all rgxfwdbg functions with services
 */
PVRSRV_ERROR DeinitRGXFWDBGBridge(void)
{

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
				PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGSETFWLOG);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
				PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGDUMPFREELISTPAGELIST);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
				PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGSETHCSDEADLINE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
				PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGSETOSIDPRIORITY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
				PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGSETOSNEWONLINESTATE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
				PVRSRV_BRIDGE_RGXFWDBG_RGXFWDEBUGPHRCONFIGURE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXFWDBG,
				PVRSRV_BRIDGE_RGXFWDBG_RGXCURRENTTIME);

	return PVRSRV_OK;
}
