/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_MODOPS_H_
#define _MTK_IMGSYS_MODOPS_H_

#include "mtk_imgsys-module.h"
#include "modules/mtk_imgsys-dip.h"
#include "modules/mtk_imgsys-traw.h"
#include "modules/mtk_imgsys-pqdip.h"
#include "modules/mtk_imgsys-wpe.h"
//#include "modules/mtk_imgsys-me.h"
#include "modules/mtk_imgsys-adl.h"
#include "mtk-ipesys-me.h"
#include "mtk_imgsys-debug.h"

const struct module_ops imgsys_isp7_modules[] = {
	[IMGSYS_MOD_TRAW] = {
		.module_id = IMGSYS_MOD_TRAW,
		.init = imgsys_traw_set_initial_value,
		.set = imgsys_traw_set_initial_value_hw,
		.dump = imgsys_traw_debug_dump,
		.uninit = imgsys_traw_uninit,
	},
	[IMGSYS_MOD_DIP] = {
		.module_id = IMGSYS_MOD_DIP,
		.init = imgsys_dip_set_initial_value,
		.set = imgsys_dip_set_hw_initial_value,
		.dump = imgsys_dip_debug_dump,
		.uninit = imgsys_dip_uninit,
	},
	[IMGSYS_MOD_PQDIP] = {
		.module_id = IMGSYS_MOD_PQDIP,
		.init = imgsys_pqdip_set_initial_value,
		.set = imgsys_pqdip_set_hw_initial_value,
		.dump = imgsys_pqdip_debug_dump,
		.uninit = imgsys_pqdip_uninit,
	},
	[IMGSYS_MOD_ME] = {
		.module_id = IMGSYS_MOD_ME,
		.init = ipesys_me_set_initial_value,
		.set = NULL,
		.dump = ipesys_me_debug_dump,
		.uninit = ipesys_me_uninit,
	},
	[IMGSYS_MOD_WPE] = {
		.module_id = IMGSYS_MOD_WPE,
		.init = imgsys_wpe_set_initial_value,
		.set = imgsys_wpe_set_hw_initial_value,
		.dump = imgsys_wpe_debug_dump,
		.uninit = imgsys_wpe_uninit,
	},
	[IMGSYS_MOD_ADL] = {
		.module_id = IMGSYS_MOD_ADL,
		.init = imgsys_adl_init,
		.set = imgsys_adl_set,
		.dump = imgsys_adl_debug_dump,
		.uninit = imgsys_adl_uninit,
	},
	/*pure sw usage for timeout debug dump*/
	[IMGSYS_MOD_IMGMAIN] = {
		.module_id = IMGSYS_MOD_IMGMAIN,
		.init = imgsys_main_init,
		.set = imgsys_main_set_init,
		.dump = NULL,
		.uninit = imgsys_main_uninit,
	},
};
#define MTK_IMGSYS_MODULE_NUM	ARRAY_SIZE(imgsys_isp7_modules)


#endif /* _MTK_IMGSYS_MODOPS_H_ */

