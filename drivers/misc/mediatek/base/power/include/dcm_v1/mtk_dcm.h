/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_DCM_H__
#define __MTK_DCM_H__

int mt_dcm_init(void);
void mt_dcm_disable(void);
void mt_dcm_restore(void);

/* unit of frequency is MHz */
extern int sync_dcm_set_cci_freq(unsigned int cci);
extern int sync_dcm_set_mp0_freq(unsigned int mp0);
extern int sync_dcm_set_mp1_freq(unsigned int mp1);
extern int sync_dcm_set_mp2_freq(unsigned int mp2);

#endif /* #ifndef __MTK_DCM_H__ */

