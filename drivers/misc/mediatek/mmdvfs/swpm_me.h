/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


void init_me_swpm(void);
void set_swpm_me_freq(unsigned int venc_freq, unsigned int vdec_freq,
	unsigned int mdp_freq);
void set_swpm_disp_active(bool is_on);
void set_swpm_disp_work(void);
void set_swpm_venc_active(bool is_on);
void set_swpm_vdec_active(bool is_on);
void set_swpm_mdp_active(bool is_on);
