/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DDP_DUMP_H_
#define _DDP_DUMP_H_

#include "ddp_info.h"
#include "ddp_path.h"

char *ddp_get_fmt_name(enum DISP_MODULE_ENUM module, unsigned int fmt);

void disp_aal_dump_reg(enum DISP_MODULE_ENUM module);
void disp_ccorr_dump_reg(enum DISP_MODULE_ENUM module);
void disp_color_dump_reg(enum DISP_MODULE_ENUM module);
void disp_dither_dump_reg(enum DISP_MODULE_ENUM module);
void disp_dsi_dump_reg(enum DISP_MODULE_ENUM module);
void disp_gamma_dump_reg(enum DISP_MODULE_ENUM module);
void disp_mutex_dump_reg(enum DISP_MODULE_ENUM module);
void disp_ovl_dump_reg(enum DISP_MODULE_ENUM module);
void disp_pwm_dump_reg(enum DISP_MODULE_ENUM module);
void disp_rdma_dump_reg(enum DISP_MODULE_ENUM module);
void disp_wdma_dump_reg(enum DISP_MODULE_ENUM module);
void mipi_tx_config_dump_reg(enum DISP_MODULE_ENUM module);
void mmsys_config_dump_reg(enum DISP_MODULE_ENUM module);

int ddp_dump_analysis(enum DISP_MODULE_ENUM module);
int ddp_dump_reg(enum DISP_MODULE_ENUM module);

#endif /* _DDP_DUMP_H_ */
