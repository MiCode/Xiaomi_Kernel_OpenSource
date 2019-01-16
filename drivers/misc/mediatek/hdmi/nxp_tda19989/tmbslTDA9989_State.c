/**
 * Copyright (C) 2009 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmbslTDA9989_state.c
 *
 * \version       $Revision: 2 $
 *
*/

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/
#ifndef TMFL_TDA19989
#define TMFL_TDA19989
#endif

#ifndef TMFL_NO_RTOS
#define TMFL_NO_RTOS
#endif

#ifndef TMFL_LINUX_OS_KERNEL_DRIVER
#define TMFL_LINUX_OS_KERNEL_DRIVER
#endif


#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
#include <linux/kernel.h>
#include <linux/module.h>
#endif

#include "tmbslTDA9989_Functions.h"
#include "tmbslTDA9989_local.h"
#include "tmbslTDA9989_State_l.h"


/*============================================================================*/
/*                     TYPES DECLARATIONS                                     */
/*============================================================================*/


/*============================================================================*/
/*                       CONSTANTS DECLARATIONS                               */
/*============================================================================*/

/*============================================================================*/
/*                       DEFINES DECLARATIONS                               */
/*============================================================================*/


/*============================================================================*/
/*                       VARIABLES DECLARATIONS                               */
/*============================================================================*/

/*============================================================================*/
/*                       FUNCTION PROTOTYPES                                  */
/*============================================================================*/

/*============================================================================*/
/* setState                                                                   */
/*============================================================================*/
tmErrorCode_t setState(tmHdmiTxobject_t *pDis, tmbslTDA9989Event_t event) {
	tmbslTDA9989State_t state = pDis->state;
	UInt8 nIgnoredEvents = pDis->nIgnoredEvents;

	switch (state) {
	case ST_UNINITIALIZED:
		switch (event) {
		case EV_INIT:
			state = ST_STANDBY;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_PLUGGEDIN:
			state = ST_AWAIT_EDID;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;

	case ST_STANDBY:
		switch (event) {
		case EV_UNPLUGGED:
			state = ST_DISCONNECTED;
			break;	/*Only when PowerSetState(ON) */
		case EV_PLUGGEDIN:
			state = ST_AWAIT_EDID;
			break;	/*Only when PowerSetState(ON) */
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_SETINOUT:
			state = ST_VIDEO_NO_HDCP;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;

		default:
			nIgnoredEvents++;
			break;
		}
		break;
	case ST_SLEEP:
		switch (event) {
		case EV_UNPLUGGED:
			state = ST_DISCONNECTED;
			break;	/*Only when PowerSetState(ON) */
		case EV_PLUGGEDIN:
			state = ST_AWAIT_EDID;
			break;	/*Only when PowerSetState(ON) */
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;

	case ST_DISCONNECTED:
		switch (event) {
		case EV_PLUGGEDIN:
			state = ST_AWAIT_EDID;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;
	case ST_AWAIT_EDID:
		switch (event) {
		case EV_GETBLOCKDATA:
			state = ST_AWAIT_RX_SENSE;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_UNPLUGGED:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;

	case ST_AWAIT_RX_SENSE:
		switch (event) {
		case EV_SINKON:
			state = ST_SINK_CONNECTED;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_UNPLUGGED:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;

	case ST_SINK_CONNECTED:
		switch (event) {
		case EV_SETINOUT:
			state = ST_VIDEO_NO_HDCP;
			break;
		case EV_SINKOFF:
			state = ST_AWAIT_RX_SENSE;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_UNPLUGGED:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;
	case ST_VIDEO_NO_HDCP:
		switch (event) {
		case EV_OUTDISABLE:
			state = ST_SINK_CONNECTED;
			break;
		case EV_HDCP_RUN:
			state = ST_HDCP_WAIT_RX;
			break;
		case EV_SINKOFF:
			state = ST_AWAIT_RX_SENSE;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_UNPLUGGED:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;
	case ST_HDCP_WAIT_RX:
		switch (event) {
		case EV_HDCP_BKSV_NREPEAT:
			state = ST_HDCP_AUTHENTICATED;
			break;
		case EV_HDCP_BKSV_REPEAT:
			state = ST_HDCP_WAIT_BSTATUS;
			break;
		case EV_HDCP_BKSV_NSECURE:
			state = ST_HDCP_WAIT_RX;
			break;
		case EV_HDCP_T0:
			state = ST_HDCP_WAIT_RX;
			break;
		case EV_HDCP_STOP:
			state = ST_VIDEO_NO_HDCP;
			break;
		case EV_SINKOFF:
			state = ST_AWAIT_RX_SENSE;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_UNPLUGGED:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;
	case ST_HDCP_WAIT_BSTATUS:
		switch (event) {
		case EV_HDCP_BSTATUS_GOOD:
			state = ST_HDCP_WAIT_SHA_1;
			break;
		case EV_HDCP_T0:
			state = ST_HDCP_WAIT_RX;
			break;
		case EV_HDCP_STOP:
			state = ST_VIDEO_NO_HDCP;
			break;
		case EV_SINKOFF:
			state = ST_AWAIT_RX_SENSE;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_UNPLUGGED:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;
	case ST_HDCP_WAIT_SHA_1:
		switch (event) {
		case EV_HDCP_KSV_SECURE:
			state = ST_HDCP_AUTHENTICATED;
			break;
		case EV_HDCP_T0:
			state = ST_HDCP_WAIT_RX;
			break;
		case EV_HDCP_STOP:
			state = ST_VIDEO_NO_HDCP;
			break;
		case EV_SINKOFF:
			state = ST_AWAIT_RX_SENSE;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_UNPLUGGED:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;
	case ST_HDCP_AUTHENTICATED:
		switch (event) {
		case EV_HDCP_T0:
			state = ST_HDCP_WAIT_RX;
			break;
		case EV_HDCP_STOP:
			state = ST_VIDEO_NO_HDCP;
			break;
		case EV_SINKOFF:
			state = ST_AWAIT_RX_SENSE;
			break;
		case EV_DEINIT:
			state = ST_UNINITIALIZED;
			break;
		case EV_STANDBY:
			state = ST_STANDBY;
			break;
		case EV_SLEEP:
			state = ST_SLEEP;
			break;
		case EV_UNPLUGGED:
			state = ST_SLEEP;
			break;
		default:
			nIgnoredEvents++;
			break;
		}
		break;

	case ST_INVALID:
		nIgnoredEvents++;
		break;

	default:
		break;
	}

	pDis->state = state;
	pDis->nIgnoredEvents = nIgnoredEvents;

	if (nIgnoredEvents) {
		/* int cbeDebug = 1; */
	}

	return TM_OK;
}

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
EXPORT_SYMBOL(setState);
#endif

/*============================================================================*/
/*                            END OF FILE                                     */
/*============================================================================*/
