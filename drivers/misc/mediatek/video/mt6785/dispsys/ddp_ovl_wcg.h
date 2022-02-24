/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DDP_OVL_WCG_H_
#define _DDP_OVL_WCG_H_

#include "ddp_info.h"
#include "graphics-base-v1.0.h"
#include "graphics-base-v1.1.h"

/* platform dependent: L3 of OVL4L for mt6779 E1 */
#define DISP_OVL_CSC_MASK 0x00000020

char *lcm_color_mode_str(enum android_color_mode cm);
bool is_ovl_standard(enum android_dataspace ds);
bool is_ovl_wcg(enum android_dataspace ds);
int ovl_color_manage(enum DISP_MODULE_ENUM module,
		     struct disp_ddp_path_config *pconfig, void *handle);

#endif /* _DDP_OVL_WCG_H_ */
