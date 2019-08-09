/**********************************
 * RGX Sync helper functions
 **********************************/
#include "rgxdevice.h"
#include "sync_server.h"
#include "rgxdebug.h"
#include "rgx_fwif_km.h"

typedef struct _RGX_SYNC_DATA_
{
	PRGXFWIF_UFO_ADDR *pauiClientUpdateUFOAddress;
	IMG_UINT32 *paui32ClientUpdateValue;
	IMG_UINT32 ui32ClientUpdateValueCount;
	IMG_UINT32 ui32ClientUpdateCount;

	PRGXFWIF_UFO_ADDR *pauiClientPRUpdateUFOAddress;
	IMG_UINT32 *paui32ClientPRUpdateValue;
	IMG_UINT32 ui32ClientPRUpdateValueCount;
	IMG_UINT32 ui32ClientPRUpdateCount;
} RGX_SYNC_DATA;

//#define TA3D_CHECKPOINT_DEBUG

#if 0 //defined(TA3D_CHECKPOINT_DEBUG)
void _DebugSyncValues(IMG_UINT32 *pui32UpdateValues,
					  IMG_UINT32 ui32Count);

void _DebugSyncCheckpoints(PSYNC_CHECKPOINT *apsSyncCheckpoints,
						   IMG_UINT32 ui32Count);
#endif

PVRSRV_ERROR RGXSyncAppendTimelineUpdate(IMG_UINT32 ui32FenceTimelineUpdateValue,
										 SYNC_ADDR_LIST	*psSyncList,
										 SYNC_ADDR_LIST	*psPRSyncList,	/* FIXME -- is this required? */
										 PVRSRV_CLIENT_SYNC_PRIM *psFenceTimelineUpdateSync,
										 RGX_SYNC_DATA *psSyncData,
										 IMG_BOOL bKick3D);
