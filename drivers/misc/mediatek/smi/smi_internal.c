/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/io.h>
/* Define SMI_INTERNAL_CCF_SUPPORT when CCF needs to be enabled */
#if !defined(CONFIG_MTK_CLKMGR)
#define SMI_INTERNAL_CCF_SUPPORT
#endif

#if defined(SMI_INTERNAL_CCF_SUPPORT)
#include <linux/clk.h>
/* for ccf clk CB */
#if defined(SMI_D1)
#include "clk-mt6735-pg.h"
#elif defined(SMI_J)
#include "clk-mt6755-pg.h"
#endif
/* notify clk is enabled/disabled for m4u*/
#include "m4u.h"
#else
#include <mach/mt_clkmgr.h>
#endif				/* defined(SMI_INTERNAL_CCF_SUPPORT) */

#include "smi_configuration.h"
#include "smi_common.h"

int smi_larb_clock_is_on(unsigned int larb_index)
{
	int result = 0;

#if defined(SMI_INTERNAL_CCF_SUPPORT)
	result = 1;
#elif !defined(CONFIG_MTK_FPGA) && !defined(CONFIG_FPGA_EARLY_PORTING)
	switch (larb_index) {
	case 0:
		result = clock_is_on(MT_CG_DISP0_SMI_LARB0);
		break;
	case 1:
#if defined(SMI_R)
		result = clock_is_on(MT_CG_LARB1_SMI_CKPDN);
#else
		result = clock_is_on(MT_CG_VDEC1_LARB);
#endif
		break;
	case 2:
#if !defined(SMI_R)
		result = clock_is_on(MT_CG_IMAGE_LARB2_SMI);
#endif
		break;
	case 3:
#if defined(SMI_D1)
		result = clock_is_on(MT_CG_VENC_LARB);
#elif defined(SMI_D3)
		result = clock_is_on(MT_CG_VENC_VENC);
#endif
		break;
	default:
		result = 0;
		break;
	}
#endif				/* !defined (CONFIG_MTK_FPGA) && !defined (CONFIG_FPGA_EARLY_PORTING) */
	return result;
}

