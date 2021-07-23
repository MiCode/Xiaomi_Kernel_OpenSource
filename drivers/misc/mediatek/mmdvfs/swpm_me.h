/*
 * Copyright (C) 2020 MediaTek Inc.
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


void init_me_swpm(void);
void set_swpm_me_freq(unsigned int venc_freq, unsigned int vdec_freq,
	unsigned int mdp_freq);
void set_swpm_disp_active(bool is_on);
void set_swpm_disp_work(void);
void set_swpm_venc_active(bool is_on);
void set_swpm_vdec_active(bool is_on);
void set_swpm_mdp_active(bool is_on);
