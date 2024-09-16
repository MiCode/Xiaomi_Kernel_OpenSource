/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/





/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-PLAT]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/* ALPS and COMBO header files */
#include <mtk_wcn_cmb_stub.h>

/* MTK_WCN_COMBO header files */
#include "wmt_plat.h"
#include "wmt_plat_stub.h"
#include "wmt_exp.h"
#include "wmt_lib.h"
#include "osal.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/




/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static VOID wmt_plat_func_ctrl(UINT32 type, UINT32 on);



/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static void wmt_plat_func_ctrl(unsigned int type, unsigned int on)
{
	if (on)
		mtk_wcn_wmt_func_on((ENUM_WMTDRV_TYPE_T) type);
	else
		mtk_wcn_wmt_func_off((ENUM_WMTDRV_TYPE_T) type);
}

static signed long wmt_plat_thremal_query(void)
{
	return wmt_lib_tm_temp_query();
}

INT32 wmt_plat_stub_init(VOID)
{
	INT32 iRet = -1;
	struct _CMB_STUB_CB_ stub_cb = {0};

	stub_cb.aif_ctrl_cb = wmt_plat_audio_ctrl;
	stub_cb.func_ctrl_cb = wmt_plat_func_ctrl;
	stub_cb.thermal_query_cb = wmt_plat_thremal_query;
	stub_cb.size = sizeof(stub_cb);

	/* register to cmb_stub */
	iRet = mtk_wcn_cmb_stub_reg(&stub_cb);
	return iRet;
}
