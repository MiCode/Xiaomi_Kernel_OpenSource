/***********************************************************************************/
/*  Copyright (c) 2010-2011, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1140 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/
/*
	@file: si_drv_mhl_tx.h
 */

void 	SiiMhlTxDeviceIsr(void);

void    SiiMhlSwitchStatus(int status);

#define	MHL_LOGICAL_DEVICE_MAP		(MHL_DEV_LD_VIDEO | MHL_DEV_LD_GUI)
