/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DDP_MISC_H_
#define _DDP_MISC_H_

#define SMI_LARB_ULTRA_DIS 0x70
#define SMI_LARB_PREULTRA_DIS 0x74
#define SMI_LARB_FORCE_ULTRA 0x78
#define SMI_LARB_FORCE_PREULTRA 0x7C

void enable_smi_ultra(unsigned int larb, unsigned int value);
void enable_smi_preultra(unsigned int larb, unsigned int value);
void disable_smi_ultra(unsigned int larb, unsigned int value);
void disable_smi_preultra(unsigned int larb, unsigned int value);

void golden_setting_test(void);

void fake_engine(unsigned int idx, unsigned int en,
			unsigned int wr_en, unsigned int rd_en,
			unsigned int latency, unsigned int preultra_cnt,
			unsigned int ultra_cnt);
void dump_fake_engine(void);

void MMPathTracePrimaryOvl2Dsi(void);
void MMPathTracePrimaryOvl2Mem(void);
void MMPathTraceSecondOvl2Mem(void);
#endif
